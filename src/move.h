#ifndef MOVE_H
#define MOVE_H

typedef struct {
    int from;
    int to;
    int promotion;
} Move;

typedef enum {
    NO_PROMOTION = 0,
    PROMOTE_Q,
    PROMOTE_R,
    PROMOTE_B,
    PROMOTE_N
} PromotionType;

void print_move(const Move);

#endif