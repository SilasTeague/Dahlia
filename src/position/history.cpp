#include "position/history.h"

#include <algorithm>

bool is_repetition_draw(const PositionHistory& history, const Position& pos, int ply) {
	// Nothing before the last pawn move or capture can repeat this position.
	int scan = std::min(static_cast<int>(pos.halfmove_clock), history.size());

	// Entries at or after this index were pushed by the search, earlier ones were played.
	int root_index = history.size() - ply;

	int played_repeats = 0;
	// Every other entry only: the rest have the other side to move.
	for (int i = history.size() - 2; i >= history.size() - scan; i -= 2) {
		if (history.keys[static_cast<size_t>(i)] != pos.zobrist_key) continue;
		if (i >= root_index) return true;
		if (++played_repeats == 2) return true;
	}
	return false;
}
