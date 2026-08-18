#include "zobrist.h"
#include <random>

uint64_t zobristPieceKeys[2][6][64];
uint64_t zobristBlackToMove;
uint64_t zobristCastlingKeys[16];
uint64_t zobristEnPassantKeys[8];

void initZobrist()
{
    std::mt19937_64 rng(0xC0FFEE123456789ULL);  // fixed seed for reproducibility
    std::uniform_int_distribution<uint64_t> dist;

    for (int color = 0; color < 2; color++)
        for (int piece = 0; piece < 6; piece++)
            for (int square = 0; square < 64; square++)
                zobristPieceKeys[color][piece][square] = dist(rng);

    zobristBlackToMove = dist(rng);

    for (int i = 0; i < 16; i++)
        zobristCastlingKeys[i] = dist(rng);

    for (int i = 0; i < 8; i++)
        zobristEnPassantKeys[i] = dist(rng);
}