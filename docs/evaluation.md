# Evaluation

`eval::evaluate` returns a centipawn score from the side-to-move's perspective:
positive means the side to move is better. It is material plus tapered
piece-square tables, and nothing else.

Specification: [REFERENCE.md §3.6](REFERENCE.md#36-evaluation-eval).
Measured cost and strength effect: [results.md](results.md#piece-square-tables-and-tapered-eval-milestone-5).

---

## Tapering

Two scores are accumulated per position — one against the midgame tables, one
against the endgame tables — and interpolated by a **game phase**:

```
score = (midgame · phase + endgame · (24 − phase)) / 24
```

The phase is counted in the non-pawn material still on the board (knight and
bishop 1 each, rook 2, queen 4; 24 is a full complement, 0 is bare kings and
pawns). Counting material rather than move number means the phase tracks what
the two tables actually disagree about — whether there is anything left to
attack the king with — and it needs no history, so a position pasted in from a
FEN is scored exactly as one reached by playing into it. Promotions can put more
material on the board than the game started with, so the phase is clamped;
otherwise the blend would extrapolate past the midgame table rather than
interpolating between the two.

**Interpolating rather than selecting a table by a phase threshold is the
point.** A threshold makes the score jump the moment a queen comes off, and the
search will happily play a bad trade to land on the favourable side of a
discontinuity it can see.

The clearest illustration is the king. The same e1 square the midgame table pays
+8 to sit on is worth −28 in the endgame table, and the centre it pays −46 to
enter is worth +27 — because once the queens are off the king stops being a
target and starts being a piece.

## The tables

The values are **PeSTO's** (Ronald Friederich's rofChade tables, published on
the Chess Programming Wiki), used as published rather than hand-picked.

That is a deliberate choice worth stating plainly: piece-square values are
either fitted to game data or guessed, and a tuner of Dahlia's own
(`tools/tuner`, Texel-style) is a committed post-Milestone-7 stretch goal, not
something Milestone 5 could produce. Borrowing a published, tuned set buys the
strength that milestone was after without pretending eyeballed numbers are tuned
ones. The tuner, when it lands, replaces these values and gets measured against
them.

What each table encodes, briefly:

| Piece | Midgame | Endgame |
|---|---|---|
| Pawn | advancing, and centre over wing | passed-pawn pressure without a passed-pawn detector — a pawn one square from promoting is +178 here against +98 in the midgame |
| Knight | the sharpest table of the six: −167 in the corner against +65 in the centre is the 4-squares-vs-8-squares reach ratio priced in centipawns | still centralized, but half the spread — fewer pieces left to attack from the outpost |
| Bishop | long diagonals over short, fianchetto over the back rank | nearly flat and slightly positive centrally — an open board is a bishop's board wherever it stands |
| Rook | seventh rank and central files, penalty for h1 behind an uncastled king | flat; a rook's endgame value is in activity the table cannot see, so it declines to guess |
| Queen | nearly flat by design — a queen is worth what it is worth almost anywhere; mostly discourages developing it early | actively rewards centralizing, with nothing left to harass it |
| King | castled and behind its own pawns: g1 (+24) and c1 (+12) against d1 (−54) is the whole of this engine's king safety | the sign flip described above |

## Orientation

Every table is written the way a board is printed — a8 first, h1 last — so the
row nearest the top of the block is the rank furthest from White. White's own
squares are numbered the other way round in this engine (a1 = 0), so:

- a **White** piece reads the table at `square ^ 56`
- a **Black** piece reads it at `square`

Getting this backwards is the classic PST bug (REFERENCE.md §3.6's "PST tables
not flipped correctly for Black"), and no material-only test catches it, because
every one of them compares a position to itself. `test_eval.cpp` asserts the
*result* rather than the indexing: mirroring the position and swapping colours
must produce an equal score.

Equal, not negated — the evaluation is side-to-move-relative, so mirroring
swaps who is to move as well, and the two sign flips cancel.

## piece_value

`eval::piece_value` returns the **larger** of a piece's midgame and endgame
value, deliberately. Its only caller is the search's delta pruning, which asks
"could winning this piece plausibly reach alpha?" and must never answer no when
the answer is yes — a rook is worth 477 in the midgame and 512 in the endgame,
and the pruning margin has to hold at both ends. `KING` is 0: it is never
captured and never counted.

## What is missing

- **No mobility, pawn structure, or king safety** beyond "a castled king scores
  better than a central one". The three Win At Chess positions still recorded as
  failures all need one of those terms, and searching them deeper does not help
  — evaluation, not search, is the binding constraint on strength.
- **No incremental update.** `evaluate()` walks every piece on the board at
  every node rather than maintaining a running score through
  `make_move`/`unmake_move`. That costs about a third of the engine's time
  (34.2 ns per call on a full board, against roughly 110 ns per node). The
  incremental version is a known, measurable win with the benchmark for it
  already in place (`BM_Evaluate_*`).
- **The phase is recomputed per call too**, matching the rest of the evaluation.
  Making one of the two incremental would pay the bookkeeping in the hottest
  function in the engine and still leave the full board walk in place, so the
  two move together or not at all.
