#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/move.h"

// Transposition table (REFERENCE.md 3.7): caches search results by Zobrist
// key. Mate-distance score adjustment is the caller's job, not this module's.
namespace search {

enum class Bound : uint8_t { Exact, LowerBound, UpperBound };

struct TTEntry {
	uint64_t key = 0;
	Move best_move{NULL_SQUARE, NULL_SQUARE};
	int16_t score = 0;
	int8_t depth = -1;
	Bound bound = Bound::Exact;
	uint8_t generation = 0;
};

class TranspositionTable {
 public:
	explicit TranspositionTable(size_t megabytes = 16);

	// Reallocates and discards all entries; rounds down to a power-of-two entry count.
	void resize(size_t megabytes);

	void clear();

	// Bumps the replacement generation so prior-search entries become freely replaceable.
	void new_search();

	// Depth-preferred: a same-generation entry only survives if searched at least as deep.
	void store(uint64_t key, Move best_move, int16_t score, int depth, Bound bound);

	// Verified probe -- only true on an exact key match, never an index collision.
	bool probe(uint64_t key, TTEntry& out) const;

	// UCI `info hashfull`, estimated from a fixed-size sample.
	int hashfull_permille() const;

 private:
	std::vector<TTEntry> entries_;
	size_t mask_ = 0;
	uint8_t generation_ = 0;
};

}  // namespace search
