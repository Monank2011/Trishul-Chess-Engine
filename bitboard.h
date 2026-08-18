// bitboard.h
#pragma once
#include <cstdint>
#include <string>

using Bitboard = uint64_t;

enum Square : int {
    a1, b1, c1, d1, e1, f1, g1, h1,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a8, b8, c8, d8, e8, f8, g8, h8,
    NO_SQUARE
};

// Set/clear/check a bit
inline void setBit(Bitboard& bb, int square)   { bb |= (1ULL << square); }
inline void clearBit(Bitboard& bb, int square) { bb &= ~(1ULL << square); }
inline bool getBit(Bitboard bb, int square)    { return (bb >> square) & 1ULL; }

// Count set bits (population count)
inline int popCount(Bitboard bb) {
    return __builtin_popcountll(bb); // GCC/Clang intrinsic
}

// Get index of least significant set bit, then clear it
inline int popLSB(Bitboard& bb) {
    int sq = __builtin_ctzll(bb); // count trailing zeros
    bb &= bb - 1;                  // clear LSB
    return sq;
}

// Print a bitboard as an 8x8 grid (for debugging)
void printBitboard(Bitboard bb);