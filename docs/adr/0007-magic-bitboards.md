# 0007 — Magic bitboards: searched offline, checked in, and kept honest by the code they replace

## Context

Sliding-piece attacks had been a loop/bit-shift ray walk since Milestone 1. That
was a deliberate staging decision, recorded when it was made
([REFERENCE.md §3.3](../REFERENCE.md#33-move-generation-movegen), 2026-07-26):
ship the obviously-correct implementation, get perft clean, put a benchmark
baseline under it, and only then replace it — so the replacement could be quoted
as a measured delta instead of an assumption.

Both preconditions had been met since Milestone 1. The baseline was recorded in
[results.md](../results.md#movegen-baseline) and had not moved outside its noise
band since. What was still missing was evidence that the ray walker was where
the time actually went, rather than merely where it was known to be
theoretically slow.

The sampling profiler added in Milestone 7 supplied it. Aggregated by self time
over `./dahlia --profile depth 16` from the start position — 1,688 samples at
500 Hz:

| Frame | Samples | Share |
|---|---:|---:|
| `make_move` | 249 | 14.8% |
| `sliding_attacks` | 226 | 13.4% |
| `select_next_move` | 212 | 12.6% |
| `score_moves` | 157 | 9.3% |
| `evaluate` | 149 | 8.8% |
| `unmake_move` | 132 | 7.8% |
| ... | | |
| `is_attacked` | 78 | 4.6% |

Second by a hair on this run and first on the longer one that prompted the
change (15.3%), which is close enough to call it a tie for the largest self-time
entry in the engine — and it is the only one of the top six with a known O(1)
replacement. `is_attacked`, four rows further down, is two more slider queries
in all but name, so the ray walker's true share is nearer 18%.

## Decision

Replace the ray walker with **fancy magic bitboards**: a per-square mask, a
per-square multiplier, and a slice of one shared attack table per piece.
Mechanics and the reasoning behind the hash are in
[movegen.md](../movegen.md#magic-bitboards).

Three parts of that are decisions rather than mechanics.

**The multipliers are searched offline and checked in as constants.** A magic
number cannot be derived; it has to be found by trial. That leaves two places to
find it — at startup, every run, or once, in a tool. Startup search is simpler to
maintain and was rejected anyway: it makes process startup take a variable
amount of time for no benefit, and it puts 128 values that determine the
correctness of every sliding move somewhere no diff will ever show them.
`tools/magicgen` finds them, is seeded so the table is reproducible from the
command line in the file's own header comment, and verifies every number against
the ray walker before printing it.

**The numbers are Dahlia's own.** Published magic tables are freely available
and would have worked identically. Writing the search is a few hours and is most
of the educational content of the technique — the sparse-candidate draw and the
`popcount((mask * magic) >> 56) < 6` pre-filter are what separate a search that
finishes in 0.13 s from one that appears to hang — and
[REFERENCE.md §1.1](../REFERENCE.md#1-project-vision)'s tie-break rule points at
the more instructive option when performance is equal. It is equal here: any
valid magic gives the same table.

**The ray walker stays in the binary.** Not as a fallback, and not on any search
path — as the oracle. `init_magics()` fills all 107,648 table entries by calling
it, and `test_magics.cpp` re-derives all 107,648 through the magic lookup and
requires them to match. The alternative is a magic table checked against
hand-written expectations, which is a test written by the same person who would
have written the bug.

## Alternatives considered

**Plain (fixed-shift) magics — one 4,096-entry block per square for rooks.**
Simpler indexing, and 4 MiB of table instead of 841 KiB. Rejected on cache
grounds: the whole point is to make a lookup cheap, and five times the footprint
in the hottest data structure in movegen is the wrong direction. The per-square
slice costs one extra pointer load that the CPU predicts trivially.

**PEXT (`_pext_u64`, BMI2)** instead of multiply-and-shift. It is the better
instruction where it exists — one op, no magic numbers, smaller tables. It also
runs at roughly 1/20th speed on pre-Zen 3 AMD, and does not exist at all on
arm64, which is both the machine this was developed on and one of the two
binaries every release ships. That makes it a runtime-dispatched *addition* to
magics rather than a replacement for them, which is exactly how
[REFERENCE.md §3.3](../REFERENCE.md#33-move-generation-movegen) already staged it.
Still not committed.

**Searching for better-than-minimal magics** — multipliers that pack a square
into fewer bits than `popcount(mask)`, shrinking the table further. Real, and
some published tables do it. Rejected as effort spent on the cost this change
was *not* bottlenecked by: 841 KiB is already the small end, and the search time
to find them is orders of magnitude larger.

**Deleting the ray walker.** It is dead code by the usual definition — nothing
in a search calls it. Keeping it costs a few hundred bytes of text and makes the
strongest correctness argument available for the change. Dead code that is the
test oracle is not dead.

## Consequences

Sliding-attack lookup is 10 to 16 times cheaper, and the whole engine reaches
the same depth 5–30% faster on all four benchmark positions with **identical
node counts everywhere** — the change cannot alter what the search concludes, and the
golden table in `test_search_nodes.cpp` needed no edit. Full numbers in
[results.md](../results.md#magic-bitboards-milestone-7).

- **Startup costs 0.75 ms it did not before**, filling the tables. Recorded as
  `BM_InitAttackTables` rather than left as an unmeasured assumption.
- **841 KiB of new hot data.** The macrobenchmark is the check on that, and it
  is what makes "every position got faster" a claim about the engine rather than
  about a microbenchmark.
- **`rook_attacks`/`bishop_attacks`/`queen_attacks` moved into the header.** They
  are three instructions; leaving them behind a call would have thrown away much
  of the win. `movegen/attacks.h` now includes `movegen/magics.h`, which is a new
  header dependency inside one module and does not touch the module graph.
- **`tools/` exists for the first time**, as
  [REFERENCE.md §1.1](../REFERENCE.md#11-directory-structure) always specified.
  It builds by default (`DAHLIA_BUILD_TOOLS`) so that a tool nobody has run in
  six months still compiles.
- **The spread across the four positions is wider than the change** — −5.3% on
  the tactical position against −29.5% on the endgame. Reported rather than
  averaged away; the tactical position is the one in the set whose profile is
  dominated by ordering and evaluation rather than by generation, and it is a
  useful reminder that a 16× win on one function is not a 16× win on anything.
