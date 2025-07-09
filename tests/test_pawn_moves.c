#include <stdio.h>
#include "../src/board.h"
#include "../src/move_list.h"
#include "../src/movegen.h"
#include "../src/move.h"

int main() {
    Board board;
    init_board(&board);

    board.side_to_move = 0; // White to move
    board.squares[8] = wP; // 2 moves
    board.squares[48] = wP;
    board.squares[17] = bP;
    board.squares[23] = bP;
    board.squares[37] = wP;
    board.squares[38] = bP;
    board.en_passant_square = 46;
    board.squares[57] = bQ;
    

    MoveList list;
    generate_all_moves(&board, &list);

    printf("Generated %d pawn moves:\n", list.count);
    for (int i = 0; i < list.count; i++) {
        print_move(list.moves[i]);
        printf("\n");
    }

    return 0;
}
