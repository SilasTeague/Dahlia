#include "move_list.h"

void add_move(MoveList* list, Move move) {
    if (list->count < MAX_MOVES) {
        list->moves[list->count++] = move; // Add move at the next index and increment count
    }
}