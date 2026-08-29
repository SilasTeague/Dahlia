# Move generation

How Dahlia turns a position into a list of moves, and — since Milestone 7 — how
it answers "what does this rook attack?" in four instructions instead of a loop.

Specification: [REFERENCE.md §3.3](REFERENCE.md#33-move-generation-movegen). The
measured before/after is in [results.md](results.md#magic-bitboards-milestone-7);
why the swap was staged the way it was, in
[ADR 0007](adr/0007-magic-bitboards.md).

---

## The shape of the module

`movegen/` produces **pseudo-legal** moves and leaves king safety to the
legality filter around `make_move`
([architecture.md](architecture.md#structural-properties)). It knows nothing
about search or evaluation.

Two kinds of piece, two entirely different problems:

- **Leapers** — knight, king, pawn — attack a fixed set of squares that no
  blocker can change. Their attack sets are computed once into
  `knight_attacks[64]`, `king_attacks[64]` and `pawn_attacks[2][64]`, and a
  lookup is one load.
- **Sliders** — bishop, rook, queen — attack along rays that stop at the first
  occupied square. Their attack set is a function of *two* inputs, the square
  and the whole board, so there is nothing to precompute in the obvious way.

Everything below is about the second problem.

## The ray walker, and why it was there first

The original implementation walks each of the four rays a square at a time,
adds each square it passes, and stops after the first occupied one:

```cpp
while (true) {
    s += offset;
    if (s < 0 || s > 63) break;
    ...                         // file-wrap guard
    Bitboard bit = 1ULL << s;
    attacks |= bit;
    if (occupied & bit) break;
}
```

It is obviously correct, which is the entire reason it shipped first
([REFERENCE.md §1.7](REFERENCE.md#17-benchmarking-philosophy)). It is also ten
to sixteen times the cost of the lookup that replaced it, and it took 13.4% of
self-time samples in a profiled search — a tie with `make_move` for the largest
single entry in the engine, before counting the further 4.6% inside
`is_attacked` that is two more slider queries by another name.

It is still in the binary, as `ray_rook_attacks` / `ray_bishop_attacks`. Nothing
on a search path calls it. It survives because it is the oracle: the magic
tables are *built* from it, and every magic lookup is *tested* against it.

## Magic bitboards

The technique is a perfect hash. Four observations get from the ray walk to a
single array read.

**1. Only the squares on the rays matter.** A rook on d4 does not care what sits
on b7. The relevant occupancy is `occupied & mask`, where `mask` is the union of
the piece's four rays.

**2. The last square of each ray does not matter either.** A blocker on h4 stops
a d4 rook's eastward ray at h4 — and so does an empty h4, because the ray ends
there regardless. Nothing is ever *behind* the edge square, so its occupancy
cannot change the answer. Dropping it is what takes a rook from 14 relevant bits
to 12, and the table from 16,384 entries per square to 4,096.

That is `rook_relevant_mask()` / `bishop_relevant_mask()` in
[`magics.h`](../src/movegen/magics.h), and it is why a rook's mask never
includes a corner and a bishop's mask never touches an edge at all.

**3. Those bits can be gathered into a small index by multiplying.** This is the
trick the technique is named for. A 64-bit multiply is 64 shifted copies of the
multiplicand added together; choose the multiplier well and the scattered mask
bits land, overlapped but distinguishably, in the top `n` bits of the product:

```cpp
unsigned index(Bitboard occupied) const {
    return ((occupied & mask) * magic) >> shift;   // shift = 64 - popcount(mask)
}
```

There is no closed form for a multiplier that works. They are found by trying
random candidates until one works, which is what
[`tools/magicgen`](../tools/magicgen/magicgen.cpp) does.

**4. Collisions are allowed when they agree.** The hash does not have to be
injective on occupancies, only on *answers*. Two occupancies that produce the
same attack set may share a slot, and on a rook square most of them do — that is
why 12 bits suffice for 4,096 distinct blocker configurations that yield far
fewer distinct attack sets. The search in `magicgen` rejects a candidate only
when two occupancies collide and *disagree*.

The result is a lookup with no branches and no loop:

```cpp
inline Bitboard rook_attacks(Square square, Bitboard occupied) {
    const Magic& m = rook_magics[square];
    return m.attacks[m.index(occupied)];
}
```

It is inline in the header deliberately. The body is a mask, a multiply, a shift
and a load; a function call would cost more than the work it wraps.

### Fancy magics: one table, not sixty-four

Each square needs a different number of index bits — 12 for a rook in a corner,
10 in the middle, 5 to 9 for bishops. Giving every square the worst case would
mean a flat `Bitboard[64][4096]`, or 4 MiB, almost all of it unused.

Instead each square gets a slice of one shared array sized to its own mask, and
its `Magic` holds a pointer into it. That is the "fancy magic" layout, and it is
what makes the tables affordable:

| Table | Entries | Size |
|---|---:|---:|
| Rook | 102,400 | 800 KiB |
| Bishop | 5,248 | 41 KiB |

The sizes are *derived* from the masks at compile time rather than written down,
so a change to a mask cannot silently overflow a table.

### Where the numbers come from

The 128 multipliers are checked into
[`magics.cpp`](../src/movegen/magics.cpp) as constants. They are not searched at
startup: the search takes a fraction of a second, but a startup cost that varies
run to run is not one an engine should carry, and a constant that has been
reviewed in a diff is worth more than one that is regenerated behind your back.

They are also not copied from another engine. `tools/magicgen` finds them, and
because it is seeded, anyone can check that the checked-in table is the one the
tool produces:

```bash
cmake --build --preset release --target dahlia_magicgen
./build/release/tools/magicgen/dahlia_magicgen --seed 0
```

Seed 0 tried 9.96 million candidates across the 128 squares and finished in
0.13 s. The generator verifies each multiplier against the ray walker before
emitting it, so it cannot print a number it has not itself checked.

Two details in the search are worth naming, because both are the difference
between "runs in a moment" and "runs for an afternoon":

- **Candidates are drawn sparse** — three random words ANDed together, leaving
  roughly one bit in eight set. A dense multiplier pushes too many partial
  products into the top bits and collides almost immediately.
- **A one-multiply filter runs before the fill loop.** If
  `popcount((mask * magic) >> 56) < 6`, the candidate is not carrying enough of
  the mask into the index to be worth testing, and it is rejected without
  touching memory. The overwhelming majority of candidates die here.

### What it costs

Magic bitboards move work from every lookup to a one-time table fill. The fill
is 107,648 ray walks and takes **0.75 ms**, once per process, inside
`init_attack_tables()`. Against a search that runs for seconds, that is not a
cost worth optimizing; it is recorded because a cost that is not measured is a
cost that is being ignored.

The other cost is cache. 841 KiB of attack tables is larger than the L2 on many
machines, and unlike the ray walker — which touched almost no memory — a slider
lookup is now a load that can miss. That is the trade the whole-engine
benchmark exists to catch, and it caught nothing:
[every position got faster](results.md#magic-bitboards-milestone-7).

## How it is verified

The magic tables are checked against the implementation they replaced, not
against hand-written expectations.

`tests/unit/test_magics.cpp` walks **every occupancy either lookup can ever be
handed** — all 102,400 rook and 5,248 bishop mask subsets — and requires the
magic answer to equal the ray-walk answer. It also asserts the count, so a mask
that quietly lost a bit fails as a wrong number of checks rather than as 102,400
silently-easier ones.

Three narrower cases sit alongside it, because the exhaustive test only ever
passes *subsets of the mask* and real callers pass whole boards:

- a dense board with bits set on the edges the mask drops, on every square,
- the empty and full boards, the two extremes for the ray walk,
- the mask/shift invariants themselves — no corner in a rook mask, no edge in a
  bishop mask, never the square itself, and `shift == 64 - popcount(mask)`.

Above that, perft is unchanged and unchangeable: sliding attacks feed every
legal move, so a single wrong entry anywhere in 107,648 would move a perft
count. The full suite, including start-position depth 6 and Kiwipete depth 5
(317 million nodes), matches the reference values exactly.

## What is deliberately not here

- **PEXT.** `_pext_u64` (BMI2) replaces the multiply-and-shift with a single
  instruction and removes the need for magic numbers at all. It is faster on
  Intel and much slower on pre-Zen 3 AMD, so it belongs behind a runtime CPU
  check rather than in the code unconditionally — and Dahlia's shipped binaries
  include an arm64 one, where the instruction does not exist. Tracked in
  [roadmap.md](roadmap.md#explicitly-out-of-scope), not committed.
- **A dedicated capture generator.** Quiescence still filters a full pseudo-legal
  list. That is a movegen change with its own correctness surface and its own
  before/after ([ADR 0005](adr/0005-quiescence-pseudo-legal-movegen.md)).
- **Legal move generation up front.** Unchanged and not planned; see
  [REFERENCE.md §3.3](REFERENCE.md#33-move-generation-movegen).
