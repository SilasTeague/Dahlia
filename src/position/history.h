#pragma once

#include <cstdint>
#include <vector>

#include "position/position.h"

// Zobrist keys of the positions along the line leading to the position
// currently on the board, oldest first and *excluding* that position itself.
// A Position carries no past, so it cannot answer "has this occurred
// before?" on its own -- and most of the positions that matter for a
// threefold claim were reached before the search root, by moves the search
// never makes. The UCI layer fills this in from `position ... moves` and the
// search pushes/pops its own moves on top (REFERENCE.md 3.4/3.8).
//
// Kept outside Position deliberately: make_move/unmake_move run inside
// generate_legal_moves' legality filter and perft, neither of which cares
// about repetition, so maintaining the line there would tax the hottest path
// in the engine for nothing.
struct PositionHistory {
	std::vector<uint64_t> keys;

	void push(uint64_t key) { keys.push_back(key); }
	void pop() { keys.pop_back(); }
	void clear() { keys.clear(); }
	int size() const { return static_cast<int>(keys.size()); }
};

// True if `pos` -- the position reached at the end of `history` -- should be
// scored as a draw by repetition. `ply` is the distance from the search root,
// i.e. how many of the trailing entries in `history` the search itself pushed.
//
// Two rules, as is standard: a position that repeats one already seen *inside
// the search tree* counts immediately, while a position seen only in the moves
// actually played needs two earlier occurrences, so that the one on the board
// is a real threefold.
//
// The first rule is the approximation every engine makes. A single repetition
// inside the tree is only a twofold, but reaching it means the side to move
// can repeat the same cycle again for the threefold, so scoring it as a draw
// costs almost nothing in accuracy -- and it stops the search from walking the
// same cycle over and over, which is the whole point.
bool is_repetition_draw(const PositionHistory& history, const Position& pos, int ply);
