// Offline search for the magic multipliers checked into src/movegen/magics.cpp.
//
//   cmake --build --preset release --target dahlia_magicgen
//   ./build/release/tools/magicgen/dahlia_magicgen --seed 0
//
// Prints the two array literals on stdout and a per-square attempt count on
// stderr. The search is randomized, but the generator is seeded, so a given
// seed reproduces a given table exactly -- which is what makes the checked-in
// constants auditable rather than merely asserted. See docs/movegen.md.

#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "core/types.h"
#include "movegen/attacks.h"
#include "movegen/magics.h"

namespace {

// SplitMix64: a seeded generator with no bad seeds, so `--seed 0` is a valid
// experiment rather than a degenerate one.
struct SplitMix64 {
	uint64_t state;

	uint64_t next() {
		uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
		return z ^ (z >> 31);
	}

	// Candidate magics are drawn sparse -- roughly one bit in eight set. A dense
	// multiplier sends too many partial products into the top bits, which is
	// where the index is read from, and collides almost immediately.
	uint64_t sparse() { return next() & next() & next(); }
};

struct SquareResult {
	uint64_t magic;
	long long attempts;
};

SquareResult find_magic(int square, bool rook, SplitMix64& rng) {
	const Bitboard mask = rook ? rook_relevant_mask(square) : bishop_relevant_mask(square);
	const int bits = std::popcount(mask);
	const unsigned shift = static_cast<unsigned>(64 - bits);
	const int size = 1 << bits;

	// Every subset of the mask, paired with the attack set it must produce.
	std::vector<Bitboard> occupancies(static_cast<size_t>(size));
	std::vector<Bitboard> references(static_cast<size_t>(size));
	Bitboard occupied = 0;
	for (int i = 0; i < size; i++) {
		occupancies[static_cast<size_t>(i)] = occupied;
		references[static_cast<size_t>(i)] = rook ? ray_rook_attacks(static_cast<Square>(square), occupied)
		                                          : ray_bishop_attacks(static_cast<Square>(square), occupied);
		occupied = (occupied - mask) & mask;
	}

	// `epoch` avoids clearing a 4,096-entry table per candidate: a slot counts as
	// filled only if it was stamped with the current attempt number.
	std::vector<Bitboard> table(static_cast<size_t>(size));
	std::vector<long long> epoch(static_cast<size_t>(size), 0);

	for (long long attempt = 1;; attempt++) {
		const uint64_t magic = rng.sparse();

		// A magic must carry at least a few mask bits up into the index. This
		// costs one multiply and rejects the large majority of candidates before
		// the fill loop touches memory.
		if (std::popcount((mask * magic) >> 56) < 6) continue;

		bool ok = true;
		for (int i = 0; i < size; i++) {
			const size_t index = static_cast<size_t>((occupancies[static_cast<size_t>(i)] * magic) >> shift);
			if (epoch[index] != attempt) {
				epoch[index] = attempt;
				table[index] = references[static_cast<size_t>(i)];
			} else if (table[index] != references[static_cast<size_t>(i)]) {
				// Two occupancies that need different attack sets landed on one
				// slot. Collisions are only allowed when they agree.
				ok = false;
				break;
			}
		}

		if (ok) return {magic, attempt};
	}
}

// Re-derives every attack set through the magic that was just found, so the tool
// never emits a number it has not itself verified.
bool verify(int square, bool rook, uint64_t magic) {
	const Bitboard mask = rook ? rook_relevant_mask(square) : bishop_relevant_mask(square);
	const unsigned shift = static_cast<unsigned>(64 - std::popcount(mask));
	std::vector<Bitboard> table(static_cast<size_t>(1) << std::popcount(mask));
	std::vector<bool> filled(table.size(), false);

	Bitboard occupied = 0;
	do {
		const Bitboard expected = rook ? ray_rook_attacks(static_cast<Square>(square), occupied)
		                               : ray_bishop_attacks(static_cast<Square>(square), occupied);
		const size_t index = static_cast<size_t>(((occupied & mask) * magic) >> shift);
		if (index >= table.size()) return false;
		if (filled[index] && table[index] != expected) return false;
		filled[index] = true;
		table[index] = expected;
		occupied = (occupied - mask) & mask;
	} while (occupied);

	return true;
}

void emit(const char* name, const uint64_t magics[64]) {
	std::printf("const Bitboard %s[64] = {\n", name);
	for (int square = 0; square < 64; square++) {
		std::printf("\t0x%016llXULL,\n", static_cast<unsigned long long>(magics[square]));
	}
	std::printf("};\n");
}

}  // namespace

int main(int argc, char** argv) {
	uint64_t seed = 0;
	for (int i = 1; i < argc; i++) {
		if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
			seed = std::strtoull(argv[++i], nullptr, 10);
		} else {
			std::fprintf(stderr, "usage: %s [--seed N]\n", argv[0]);
			return 2;
		}
	}

	uint64_t rook[64];
	uint64_t bishop[64];
	long long total_attempts = 0;

	// One generator for the whole run, advanced square by square: the emitted
	// table is a function of the seed alone.
	SplitMix64 rng{seed};

	for (int square = 0; square < 64; square++) {
		const SquareResult r = find_magic(square, /*rook=*/true, rng);
		rook[square] = r.magic;
		total_attempts += r.attempts;
		if (!verify(square, true, r.magic)) {
			std::fprintf(stderr, "rook square %d: emitted magic failed verification\n", square);
			return 1;
		}
		std::fprintf(stderr, "rook   %2d  %2d bits  %8lld attempts\n", square,
		             std::popcount(rook_relevant_mask(square)), r.attempts);
	}

	for (int square = 0; square < 64; square++) {
		const SquareResult r = find_magic(square, /*rook=*/false, rng);
		bishop[square] = r.magic;
		total_attempts += r.attempts;
		if (!verify(square, false, r.magic)) {
			std::fprintf(stderr, "bishop square %d: emitted magic failed verification\n", square);
			return 1;
		}
		std::fprintf(stderr, "bishop %2d  %2d bits  %8lld attempts\n", square,
		             std::popcount(bishop_relevant_mask(square)), r.attempts);
	}

	std::fprintf(stderr, "\nseed %llu, %lld candidates tried across 128 squares\n",
	             static_cast<unsigned long long>(seed), total_attempts);

	emit("rook_magic_numbers", rook);
	std::printf("\n");
	emit("bishop_magic_numbers", bishop);
	return 0;
}
