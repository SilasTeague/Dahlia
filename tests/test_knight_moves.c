#include <stdio.h>
#include "../src/board.h"
#include "../src/move_list.h"
#include "../src/movegen.h"
#include "../src/move.h"

int main() {
    Board board;
    init_board(&board);

    board.side_to_move = 0; // White to move
    board.squares[27] = wN; // 8 moves
    board.squares[0] = bN; // No moves
    board.squares[7] = wN; // 2 moves
    

    MoveList list;
    generate_all_moves(&board, &list);

    printf("Generated %d knight moves:\n", list.count);
    for (int i = 0; i < list.count; i++) {
        print_move(list.moves[i]);
        printf("\n");
    }

    return 0;
}
