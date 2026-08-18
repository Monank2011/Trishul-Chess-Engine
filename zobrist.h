#ifndef ZOBRIST_H
#define ZOBRIST_H

#include <cstdint>

extern uint64_t zobristPieceKeys[2][6][64];  // [color][pieceType][square]
extern uint64_t zobristBlackToMove;
extern uint64_t zobristCastlingKeys[16];      // all combinations of 4 castling rights
extern uint64_t zobristEnPassantKeys[8];      // one per file

void initZobrist();

#endif