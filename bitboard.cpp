// bitboard.cpp
#include "bitboard.h"
#include <iostream>

void printBitboard(Bitboard bb) {
    for (int rank = 7; rank >= 0; rank--) {
        std::cout << (rank + 1) << "  ";
        for (int file = 0; file < 8; file++) {
            int square = rank * 8 + file;
            std::cout << (getBit(bb, square) ? "1 " : ". ");
        }
        std::cout << "\n";
    }
    std::cout << "\n   a b c d e f g h\n\n";
}