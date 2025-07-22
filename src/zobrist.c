#include "zobrist.h"

// Create a random 64 bit integer from a seed n
uint64_t random64(uint64_t n) {
    const uint64_t z = 0x9FB21C651E98DF25;

    n ^= ((n << 49) | (n >> 15)) ^ ((n << 24) | (n >> 40));
    n *= z;
    n ^= n >> 28;
    n *= z;
    n ^= n >> 28;

    return n;
}

void init_zobrist() {
    int seed = 1;
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 64; j++) {
            psq[i][j] = random64(seed++);
        }
    }

    for (int i = 0; i < 8; i++) {
        enpassant[i] = random64(seed++);
    }

    for (int i = 0; i < 16; i++) {
        castling[i] = random64(seed++);
    }

    side = random64(seed);
}

uint64_t compute_hash(const Board* board) {
    uint64_t hash = 0;

    for (int j = 0; j < 64; j++) {
        if (board->squares[j] != EMPTY) {
            hash ^= psq[board->squares[j]][j];
        } 
    }

    if (board->en_passant_square != -1) {
        hash ^= enpassant[board->en_passant_square % 8];
    }

    hash ^= castling[board->castling_rights];

    if (board->side_to_move == 1) {
        hash ^= side;
    }

    return hash;
}