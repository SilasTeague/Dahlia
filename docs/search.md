# Search

Everything in `src/search/`: how the tree is walked, what is pruned, what is
ordered, and which techniques are allowed to be wrong.

Measured results for each technique are in [results.md](results.md). The
constants and the sweeps behind them are in
[ADR 0006](adr/0006-aspiration-lmr-constants.md). The specification is
[REFERENCE.md §3.7–3.9](REFERENCE.md#38-search-search).

---

## Shape of the search

`search::think` iteratively deepens from depth 1 until the depth limit or the
time budget runs out, returning the best move from the last fully-completed
depth. Each iteration runs through `search_root`, which wraps the aspiration
window around `negamax`.

`negamax` is fail-soft alpha-beta with PVS. Fail-soft matters downstream: the
re-search conditions and the aspiration widening both read *how far* past a
bound a score got, which a fail-hard search does not report.

Scores are `int16_t` centipawns from the side-to-move's perspective.
`kMateScore` is 32000 and a score counts as a mate score once it is within
`kMaxPly` of it — far enough that no ordinary material or positional score could
reach it. Mate scores are stored root-relative in the transposition table and
read back node-relative, which is the mate-distance adjustment
[REFERENCE.md §3.8](REFERENCE.md#38-search-search) names as a common off-by-one
bug class.

A draw scores exactly 0 — neither side gains by steering into one. A contempt
factor (preferring a fight over a draw against weaker opposition) is a later,
separately measurable change.

## Stopping

`stop_requested` is an `std::atomic<bool>` owned by the caller, so a UCI `stop`
arriving on the reader thread reaches a search already in flight. It is loaded
unconditionally once per node; that load is cheap, and it only *gates* the
time check, which runs on a node-count modulus (every 2048 nodes). Checking the
clock every node is too slow, checking it too rarely blows the time control.

An aborted iteration is discarded in favour of the last completed depth, except
at depth 1 — alpha-beta updates the root's best move as soon as any move
improves on −∞, so even an aborted first iteration returns something legal.

## Draw by repetition

Scored inside the search rather than filtered afterwards, and checked *before*
the transposition table probe: the table can hold a perfectly real score for
that key from a line where it was not a repetition, and along this line the game
is over. The root is exempt, since it still has to return a move.

Two rules, as is standard. A position that repeats one already seen *inside the
search tree* counts on its first occurrence; a position seen only in the moves
actually played needs two earlier occurrences, so the one on the board is a real
threefold. The first rule is the approximation every engine makes — reaching a
twofold inside the tree means the side to move could repeat the cycle again, so
scoring it as a draw costs almost nothing in accuracy and stops the search
re-walking cycles.

`is_repetition_draw` scans back at most `halfmove_clock` plies (nothing before
the last pawn move or capture can recur) and only every other entry (the rest
have the other side to move, which the Zobrist key already distinguishes).

Not yet scored: the fifty-move rule and insufficient material.

## Transposition table

A power-of-two-sized table indexed by Zobrist key, storing key, best move,
score, depth, bound type, and generation. Probes are verified against the full
key, so an index collision is a miss rather than a wrong answer. Replacement is
depth-preferred within a generation; `new_search` bumps the generation so
entries from a previous search become freely replaceable.

The table serves two purposes that are worth keeping distinct:

- **A move to try first.** Ordering information, not a claim about a score.
- **A score cutoff.** An entry searched at least as deep as the current node can
  answer the node outright (exact bound) or narrow its window (lower/upper
  bound).

Only the second is gated by `SearchTuning::transposition_cutoffs`. Switching it
off leaves the table filling, ordering, and costing what it costs — it just
stops answering questions on the search's behalf. That distinction is what makes
"is this technique exact?" answerable at all; see below.

## Move ordering

Alpha-beta's node count is dominated by how early the best move is tried:
search the best move first at every node and the tree approaches its theoretical
minimum; search it last and alpha-beta degenerates toward plain minimax. Nothing
in `ordering.h` changes what the search concludes — only how fast it gets there.

Moves are scored once per node into **numerically disjoint bands**:

| Band | Score base | Contents |
|---|---|---|
| TT move | `1 << 30` | the best move a previous, possibly shallower search found |
| Captures and promotions | `1 << 20` | ranked by MVV-LVA; promotions by the piece promoted to |
| Killers | `1 << 18` | this ply's two killer moves |
| Quiets | history score, clamped below `1 << 18` | butterfly history |

Band separation is a load-bearing invariant, not a tidiness preference.
Quiescence relies on "the first quiet move to surface means no captures remain"
to stop scanning its list early, and `test_ordering.cpp` pins it. It also keeps
each band independently tunable — changing how captures score against each other
cannot accidentally push one above the TT move.

Scores are drawn out by **selection sort, one move at a time**, rather than
sorting the whole list up front. A well-ordered node cuts off on its first or
second move, and every comparison spent ranking the moves after it is wasted.
The cost is O(n) per move taken instead of O(n log n) once, which only loses at
nodes that search nearly all their moves — exactly the nodes where ordering has
already failed and the sort is not the expensive part.

### MVV-LVA

Most Valuable Victim / Least Valuable Attacker: a capture's score rises with
what it takes and falls with what it risks. PxQ is searched before QxP, because
if the queen is defended PxQ still wins material outright while QxP loses it —
so PxQ is far likelier to cause a cutoff. The victim's rank is multiplied by
more than any attacker rank can reach, so every capture of a queen sorts above
every capture of a rook regardless of what is capturing.

This is a static *ordering*, not a static evaluation: it says nothing about
whether the victim is defended. Ordering a losing QxP alongside a winning one
costs nodes, never correctness — quiescence still scores the exchange correctly
once it searches it. SEE (static exchange evaluation) is the usual fix and is
deliberately out of scope; it is a separately measurable change.

En passant is a capture whose victim is not on the destination square, so a bare
`board[m.to]` check misses it. Both `is_capture` and `mvv_lva_score` handle it
explicitly.

### Killers

Two killer moves per ply: quiet moves that produced a beta cutoff at this
distance from the root in a *sibling* subtree. The heuristic is that positions
at the same ply share threats — if ...Qxh2 refuted one move at ply 4, it very
often refutes the next one too, even though the two nodes are otherwise
unrelated and share no transposition.

Two slots rather than one: a single slot is overwritten by every cutoff and so
remembers only the most recent sibling, throwing away a killer that was working
across the whole node. Two is conventional; the measured gain from a third is
small enough that engines differ on whether to bother. `store` demotes rather
than duplicates, so a move already in the first slot does not consume both.

Only quiet moves are stored — a capture that causes a cutoff is already ranked
above every quiet by MVV-LVA, so recording it would only take a killer slot a
quiet move could use.

Killers are **per-search**: they are claims about the shape of *this* tree, and
the position has moved on by the next `go`.

### History

Butterfly history indexed `[side][from][to]`: how often a quiet move has caused
a beta cutoff anywhere in the search so far. Where killers are a per-ply memory,
history is a global one — "this move keeps being good in this game", regardless
of depth or ply.

The bonus is **depth squared**. A cutoff found at depth 9 is evidence from a
much larger subtree than one at depth 2, and weighting them equally lets the
shallow nodes — far more numerous — drown out the deep ones.

When any entry saturates, the whole side's table is **halved rather than
clamped**, preserving the relative order that is the only thing the ordering
reads. Without that, a long game's early favourites pin themselves at the
ceiling and the table stops distinguishing anything — the "stale saturation"
pitfall REFERENCE.md §3.8 names.

The table is **caller-owned and cleared on `ucinewgame`**, not per search: its
value comes from accumulating across the moves of one game. Carried into an
unrelated position it is stale bias, not a head start.

## Quiescence

A fixed-depth search evaluates whatever position it lands on, so QxP at the last
ply scores as "won a pawn" even when the obvious recapture is waiting one ply
deeper. That is the horizon effect, and on a material-only evaluation it was the
dominant source of blunders. Quiescence fixes it by stopping only at positions
where no capture is pending: at the leaf, it searches captures and promotions
and nothing else.

**Stand-pat.** The side to move is never *obliged* to capture, so the static
score of the position as it stands is a lower bound on what the node is worth.
Without it the search would be forced to play out every available capture,
including the ones that just lose material.

**Delta pruning.** If even winning the captured piece outright, plus a
two-pawn margin for the positional value this evaluation cannot see, would not
reach alpha, the capture cannot change the node's score and is skipped. Two
pawns is loose enough that no ordinary tactic is pruned and tight enough to cut
the long tails of pointless captures in lost positions. Promotions are exempt —
their gain is not the captured piece. Gated by
`SearchTuning::delta_pruning`.

**Termination is structural, not depth-limited.** Every move searched here
either removes a piece from the board or promotes a pawn. Both are strictly
monotone and bounded — 30 capturable pieces, 16 pawns — so no line can run past
roughly 46 plies even in principle, and the `kMaxPly` guard bounds it
regardless. There is no depth counter because there does not need to be one;
this is the property the termination tests pin.

**Moves are generated pseudo-legally**, and legality is proved by the
`make_move` the recursion performs anyway. Calling `generate_legal_moves`
instead — which make/unmakes every move to filter it — meant paying full
legality checks on roughly 35 quiet moves in order to search 4 captures, and
halved engine throughput. See
[ADR 0005](adr/0005-quiescence-pseudo-legal-movegen.md).

**Deliberately not done here:** check evasions (a node in check gets the same
stand-pat treatment as any other, which is unsound in the strict sense but
standard for a first implementation), mate detection at the horizon, and TT
probes. All three are separately measurable changes. Mate detection in
particular would need the full legal move list, which is exactly the cost the
pseudo-legal path exists to avoid — and `negamax` still detects every mate at
depth 1 or deeper, so only a mate appearing precisely at a quiescence leaf is
missed.

## Principal Variation Search

The first move at each node gets a full `[alpha, beta]` window; every move after
it is searched against the null window `[alpha, alpha+1]`, which asks only "is
this better than what we already have?" and cannot answer with anything but yes
or no. A window one point wide fails high or low almost immediately, cutting off
most of the subtree.

It is the right question because move ordering makes the first move the best one
84–96% of the time across the four benchmark positions at depth 7 — a
precondition that was *measured before the change was made*, not assumed. The
remaining few percent pay for it: a scout that fails high proves only that the
move beats alpha, not by how much, so the search repeats with the real window.
Re-searches then run at 0.01–0.46% of scouts.

The re-search condition is `alpha < score < beta`, not just `score > alpha`.
That is what stops the cost compounding: inside a node that is itself being
scouted, beta is already `alpha + 1`, so a fail-high there is the answer rather
than a question to ask again.

PVS is applied at every node with no depth threshold. A threshold was tried and
rejected — see [results.md](results.md#principal-variation-search-milestone-5).

**PVS is exact but not score-identical in the presence of the TT.** A
null-window search stores a *bound* where a full window stored an exact score,
so a later probe may have to search where it used to return. Verified by
rebuilding both versions with the table's score cutoffs disabled: the scores
then match exactly at every depth and position tested.

## Null-move pruning

Hand the opponent a free move, search the result `R = 2` plies shallower, and if
the position is *still* above beta, the real move list is not worth generating.
Nearly every position obliges, because passing is worse than any legal move —
nearly.

Applied from depth 3 up. Below that the null search would run at depth ≤ 0, a
bare quiescence, which answers a different question than the search it stands in
for. `R = 2` is the conventional starting value: deep enough that the null
search costs a fraction of the real one, shallow enough that it still sees the
threats that would refute the assumption.

Four guards:

- **Never two in a row.** `allow_null` is false inside the subtree of a null
  move already made, so the search can never answer "what if I pass?" with
  "well, what if we both pass?" — two passes return the same position two plies
  shallower, which proves nothing and costs a subtree.
- **Never in check.** Passing there would leave a capturable king, and the whole
  subtree would be scoring an impossible position.
- **Never when beta is already a mate score.** Near a forced mate, "I'm winning
  even if I do nothing" is a statement about beta, not about the position, and a
  shallow search that fails to find the mate would prune the line that does.
- **`has_non_pawn_material` — the zugzwang guard.** The assumption "passing is
  worse than any real move" is false exactly where a side has nothing but pawns,
  which cannot move backwards. Removing it and changing nothing else makes the
  search score the won K+P benchmark endgame at +219 instead of +962 at depth 17
  — a 700-centipawn misjudgement of a won game, pinned by `test_nullmove.cpp`.

When the null search returns a mate score, `beta` is returned instead: it found
a mate for a side that was handed a free move, which says nothing about whether
the mate exists in the real game, and propagating it would put a mate score into
the table for a position that is not mating.

**Known and accepted:** the pruning runs before the move list is generated, so a
*stalemate* is treated as a position where the side to move could happily pass —
precisely what a stalemated side would want to do. The zugzwang guard covers the
common case (a stalemated side is usually down to a bare king), and detecting
the rest would mean generating the move list, which is the cost this heuristic
exists to avoid.

The null move is pushed onto the repetition history like a real move: the child
counts entries back from the end of the line to tell the search's own moves from
the game's, and a null move that skipped the push would shift that count by one
for everything below it.

## Aspiration windows

Alpha-beta's cost falls sharply as the window narrows. Iterative deepening hands
the search a free estimate of where the answer will land — the score at depth
*d−1* is almost always within a few centipawns of the score at depth *d*,
because one extra ply rarely overturns a position's assessment. So rather than
searching depth *d* with the maximal window and paying for its width, search it
with a narrow window centred on the previous score, and pay a re-search only
when the guess was wrong.

- **Half-width 25 centipawns.** A quarter of a pawn either side: wide enough
  that ordinary positional drift between plies stays inside it, narrow enough
  that the window does real work. Both ends of that trade were measured — see
  [results.md](results.md#aspiration-windows-milestone-6).
- **From depth 4 up.** Below it the iteration is too cheap for a narrower window
  to save anything measurable, and the score is still moving too much for the
  previous one to be a usable guess.
- **Skipped entirely on a mate score.** Mate scores are enormous and jump by
  whole plies rather than centipawns, so a window of a few centipawns around one
  is guaranteed to fail, and each failure costs a full re-search of the depth.

**Widening is one-sided.** A fail-low says the answer is below alpha and tells
us nothing new about beta, so only the failing bound moves; the re-search still
runs with a narrow window on the other side, which is the part worth keeping.
The new bound is taken from the *returned score* rather than from the old bound
— what fail-soft buys here — so the next window is placed around the real answer
instead of guessing outward from a bound already known to be wrong.

**Widening is geometric** (`delta += delta / 2`). A score that has genuinely
moved a long way — a piece hanging that the last ply could not see — would
otherwise be chased outward in fixed steps, paying a re-search for each one.
Growing by half each time bounds the re-searches at a handful even when the
first guess is badly wrong.

The loop also terminates when both bounds are already maximal. That is
unreachable in practice — fail-soft cannot return a score outside ±`kMateScore`
— but it is the loop's termination guarantee, and an infinite loop there would
hang the engine until the clock cut it off.

**Aspiration is exact.** A score that escapes the window is never accepted, only
re-searched wider, so a failed guess costs nodes and never accuracy. Like PVS,
it is nonetheless not *score-identical* in the presence of the TT, and for the
same reason.

**Deliberately not done:** `info` output on a fail-high/fail-low. A GUI would
display the intermediate bound (Stockfish sends `lowerbound`/`upperbound`),
which is a presentation feature rather than a search one — and this engine's
`info` lines already omit `pv` and `seldepth`.

## Late move reductions

Every other technique here is exact. **LMR is the first one allowed to be
wrong**, and the trade is why it is worth it: search the moves the ordering
already ranked as unpromising to a *shallower* depth, and spend what that saves
on depth for everything else.

The bet is on the move ordering, not on the moves. By the time the fourth quiet
move at a node is reached, the TT move, every capture, both killers and the
highest-history quiets have been searched and none beat alpha. Such a move is
very unlikely to be best — and if the ordering is right, all the search needs
from it is confirmation that it isn't, which a shallow search provides cheaply.

**Reduction is `0.75 + ln(depth) · ln(move_index) / 2.25`**, truncated to an
integer and precomputed into a 64 × 256 table on first use. Both logarithms
matter, and multiplying them is what makes the shape right: deep searches can
afford to give up more plies because they have more to give, late moves deserve
to lose more because ordering is more confident about them, and a move that is
both gets reduced hardest. A flat reduction is either too timid at depth 20 or
reckless at depth 4.

Applied from depth 3 up (at depth 2 a reduced search runs at depth 0, a bare
quiescence), after the first 3 moves at a node (which covers the ordering's
high-confidence band — the TT move plus, typically, the best captures or a
killer), and clamped so a reduced search never falls below depth 1.

Four classes of move are excluded, each because the ordering's "late means
unpromising" claim does not cover it:

- **Captures and promotions.** They are ranked by what they win, not by how good
  they turned out to be. A late capture is late because MVV-LVA thinks it wins
  the least material, which says nothing about whether it refutes the position —
  and with no SEE in the ordering, "late capture" does not even mean "losing
  capture".
- **Moves that give check.** They force the reply, so the subtree below them is
  narrow and cheap already; reducing saves little and risks missing a forcing
  line, which is the expensive kind of mistake.
- **Every move at a node already in check.** All of them are forced evasions, so
  there is no "late, therefore unpromising" ordering claim to lean on.

**Three searches of increasing cost.** A reduced move is asked three questions,
and only moves that survive the cheap ones reach the expensive one:

1. The reduced scout — "does this beat alpha?", for a move the ordering says
   shouldn't.
2. If it beats alpha, the reduction is the suspect, so the same null-window
   question is asked again at full depth. Nothing from the reduced search is
   kept.
3. If it still beats alpha and the node has a real window, PVS's own re-search
   establishes by how much.

Steps 2 and 3 are separate on purpose: a reduced move that beats alpha usually
stops beating it at full depth, and the null window is enough to prove that.

**The failure mode is one-sided.** LMR can miss a good move the reduced search
never noticed. It can never *promote* a bad one, because beating alpha always
triggers the full-depth re-search. That asymmetry — not score equality — is what
`test_research.cpp` pins.

**Four conventional extra guards were implemented, measured, and rejected**: a
zugzwang guard mirroring null-move's, exempting killer moves, reducing less at
PV nodes, and searching four moves at full depth instead of three. Only the last
was rejected as harmful; the other three were rejected as *unresolvable by node
counts*. See [ADR 0006](adr/0006-aspiration-lmr-constants.md).

## SearchTuning and "safe mode"

`SearchTuning` is a struct of switches, all defaulting to on, for the parts of
the search that are allowed to be wrong. It is deliberately not part of
`SearchLimits`: those are the GUI's instructions, while these are the engine's
own settings, and no UCI command reaches them. `tests/unit/test_research.cpp` is
the only caller that changes any of them; nothing in `src/` does.

| Switch | What it gates |
|---|---|
| `aspiration_windows` | narrow initial window at the root |
| `late_move_reductions` | reduced-depth search of late quiet moves |
| `null_move_pruning` | the free-move probe |
| `delta_pruning` | quiescence discarding hopeless captures |
| `transposition_cutoffs` | the table answering a node, as opposed to ordering it |

`SearchTuning::exact()` switches all of them off. What is left is alpha-beta
with PVS, quiescence and move ordering, none of which may change a score. It is
slow by design — a reference to compare against, not a way to play.

Two of these need explaining. Null-move and delta pruning are switchable because
**both read the current window** — null-move searches against beta, delta
pruning discards captures that cannot reach alpha — so both prune differently
when a window narrows, and both can prune something real. They are the reason a
narrower window changes this engine's scores even with the table's cutoffs
disabled, and therefore the reason the question "does the aspiration window
change the answer?" cannot be asked without holding them still.

`transposition_cutoffs` is there because **"exact" is not the same claim as
"returns the same score", and the difference is entirely the table.** A
narrow-window search stores *bounds* where a full-window search would have
stored exact scores; both are correct, but a later probe can only cut off on the
second kind, so the engine reads more effective depth out of a table filled by
wide windows than by narrow ones. Every narrowing technique in this engine
therefore changes results through the table while changing nothing about its own
arithmetic.

Milestone 5 established that for PVS by rebuilding the engine twice by hand and
comparing. These switches make the same verification a test that runs in CI.

## Time management

`search::time_budget_ms` converts UCI `go` parameters into a single-move budget:

```
budget = min(time_left / divisor + increment / 2, time_left / 2) - move_overhead
```

- `divisor` is 20 by default. `movestogo` only *tightens* it: with fewer than 20
  moves before the next time control, spending a twentieth per move would leave
  time unspent at the control and risk flagging near it.
- The `time_left / 2` cap stops one move spending the whole clock.
- `movetime` bypasses the formula but still pays the overhead reserve.
- A bare `go` with no time control gets a fixed 200 ms anytime budget rather than
  searching forever.
- `go depth` and `go infinite` get a 24-hour cap, which is not a deadline but a
  way to keep the comparison well-formed until `stop` arrives.

**The `Move Overhead` reserve is load-bearing.** Everything after the search
returns — serializing the output, the write and flush, transport to the GUI —
happens on the GUI's clock, and a GUI with a zero time margin (cutechess-cli's
default `timemargin`) scores an on-the-deadline reply as a forfeit. The reserve
is subtracted from every GUI-imposed budget including `movetime`, and never
yields less than 1 ms: an overhead larger than the budget means the engine is in
desperate time trouble and must still return something.

Deferred: the soft/hard limit split and instability-based extension (spending
longer when the best move keeps changing between iterations), both of which need
a move-quality signal this engine does not yet track.
