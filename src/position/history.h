#pragma once

#include <cstdint>
#include <vector>

#include "position/position.h"

// Zobrist keys of the line leading to the position on the board, oldest first
// and excluding it. Kept outside Position so make_move -- which also runs inside
// the legality filter and perft -- pays no bookkeeping for it.
struct PositionHistory {
	std::vector<uint64_t> keys;

	void push(uint64_t key) { keys.push_back(key); }
	void pop() { keys.pop_back(); }
	void clear() { keys.clear(); }
	int size() const { return static_cast<int>(keys.size()); }
};

// True if `pos`, the position at the end of `history`, is a draw by repetition.
// `ply` is how many trailing entries the search itself pushed: a repeat inside
// the tree counts once, one only in the played game needs two (docs/search.md).
bool is_repetition_draw(const PositionHistory& history, const Position& pos, int ply);
