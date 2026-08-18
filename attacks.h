#ifndef ATTACKS_H
#define ATTACKS_H
#include "board.h"

#include <cstdint>

extern uint64_t knightAttacks[64];

extern uint64_t kingAttacks[64];

extern uint64_t whitePawnAttacks[64];
extern uint64_t blackPawnAttacks[64];

void initKnightAttacks();
void initKingAttacks();
void initPawnAttacks();

uint64_t maskKnightAttacks(int square);

bool isSquareAttacked(const Board& board, int square, bool byWhite);

#endif