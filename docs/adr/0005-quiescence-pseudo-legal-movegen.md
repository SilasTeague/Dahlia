# 0005 — Quiescence generates pseudo-legal moves and proves legality on `make_move`

## Context

Quiescence search (REFERENCE.md 3.8, Milestone 4) runs at every leaf of the main
search, so it accounts for the majority of nodes the engine visits. It searches
only captures and promotions; every quiet move it is handed is discarded.

The obvious implementation calls the movegen entry point the rest of the search
already uses:

```cpp
MoveList moves;
generate_legal_moves(moves, pos);
```

That is what the first working version did, and it was correct. It was also
slow in a way that is invisible from the call site. `generate_legal_moves`
establishes legality the way REFERENCE.md 3.3 specifies — generate
pseudo-legal, then make/unmake each move and reject the ones that leave the
mover's own king attacked:

```cpp
for (int i = 0; i < pseudo.count; i++) {
    StateInfo undo;
    make_move(position, pseudo.moves[i], undo);
    if (!is_in_check(position, us)) move_list.push(pseudo.moves[i]);
    unmake_move(position, pseudo.moves[i], undo);
}
```

For the main search that cost is fine: every legal move produced is a move that
gets searched. For quiescence it is not, because quiescence throws away roughly
nine moves in ten. On a Kiwipete-shaped position that meant a full make/unmake
pair on about 35 quiet moves in order to search about 4 captures — legality
work performed entirely on moves that were then dropped.

The cost showed up in the benchmark history, not in any test. Node counts were
unaffected (an unsearched move is not a node), the whole suite stayed green,
and the only visible symptom was throughput:

| Kiwipete, depth 5 | Wall time | Nodes/sec |
|---|---:|---:|
| `generate_legal_moves` in quiescence | 65.67 ms | 2,455,625 |
| `generate_pseudo_legal_moves` + legality on make | 29.62 ms | 5,445,052 |

This is exactly the failure mode REFERENCE.md 3.11 says the benchmark framework
exists to catch: a change that passes perft and every unit test and quietly
makes the engine half as fast.

## Decision

Quiescence generates **pseudo-legal** moves and folds the legality check into
the `make_move` it already has to perform in order to recurse:

```cpp
make_move(pos, m, undo);
if (is_in_check(pos, us)) {  // the move was only pseudo-legal
    unmake_move(pos, m, undo);
    continue;
}
score = -quiescence(pos, -beta, -alpha, ply + 1, state);
```

Legality is still enforced — no illegal move is ever searched or scored. What
changes is *when* it is established: after the move is made, for the small
number of moves quiescence actually wants, instead of before, for all of them.
The check costs one attack query rather than a second make/unmake pair.

Two consequences follow, and both are accepted deliberately:

- **The capture filter can stop early instead of scanning to the end.** Move
  ordering keeps captures and promotions in score bands strictly above every
  quiet move, so the first quiet move to surface from the selection sort proves
  there are no captures left. `test_ordering.cpp` pins that band separation,
  which is what makes the `break` safe rather than merely usually-correct.
- **Quiescence can no longer detect mate at the horizon.** Knowing a position
  has no legal moves requires the full legal move list — precisely the cost
  being avoided. A checkmate appearing exactly at a quiescence leaf is scored
  as material instead. The main search still detects every mate at depth 1 or
  deeper, so this is a narrow inaccuracy, and buying it back would mean
  reinstating the cost this decision exists to remove.

## Alternatives considered

**Keep `generate_legal_moves` and accept the cost.** Rejected on the measured
numbers: a 2× throughput loss for a convenience at one call site, in the
hottest function in the engine.

**Add a dedicated `generate_captures` to `movegen/`.** This is the
better long-term answer and is REFERENCE.md 3.8's named future extension
(staged move generation). It is not this change, because it is a movegen
change with its own perft-adjacent correctness surface — a capture generator
that misses en passant is a bug that quiescence would silently absorb — and it
should land with its own tests and its own before/after. The decision here
recovers most of the available win without touching movegen at all, which
leaves staged generation still worth doing and still measurable against a
now-honest baseline.

**Filter to captures before the legality loop, inside `generate_legal_moves`.**
Rejected as the wrong shape: it would mean a movegen function taking a
"captures only" flag whose only caller is the search, pushing a search concern
into a module that REFERENCE.md's dependency direction says should not know
search exists.

## Consequences

- Quiescence is roughly 2× cheaper per node on capture-dense positions; the
  Milestone 4 middlegame benchmark reports 5.4M nodes/sec rather than 2.5M.
- `search/` now depends on `generate_pseudo_legal_moves` as well as
  `generate_legal_moves`. Both were already public in `movegen/`; the module
  graph is unchanged.
- The legality check is now written out at the quiescence call site rather than
  being someone else's invariant. That is a real maintenance cost — a future
  edit that adds a second recursion point in quiescence must repeat it — and it
  is the reason a dedicated capture generator remains the preferred end state.
- Node counts moved very slightly (Kiwipete depth 5: 161,268 → 161,277) as a
  side effect of dropping horizon mate detection, not of the legality change
  itself. The golden table in `test_search_nodes.cpp` was updated in the same
  change.
