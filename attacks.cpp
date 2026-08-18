#include "attacks.h"
#include "magic.h"
#include "board.h"

uint64_t knightAttacks[64];

uint64_t maskKnightAttacks(int square)
{
    uint64_t attacks = 0ULL;

    int rank = square / 8;
    int file = square % 8;

    int dr[8] = {-2, -2, -1, -1, 1, 1, 2, 2};
    int df[8] = {-1, 1, -2, 2, -2, 2, -1, 1};

    for (int i = 0; i < 8; i++)
    {
        int r = rank + dr[i];
        int f = file + df[i];

        if (r >= 0 && r < 8 && f >= 0 && f < 8)
        {
            attacks |= 1ULL << (r * 8 + f);
        }
    }

    return attacks;
}

void initKnightAttacks()
{
    for (int square = 0; square < 64; square++)
    {
        knightAttacks[square] = maskKnightAttacks(square);
    }
}

uint64_t kingAttacks[64];

uint64_t whitePawnAttacks[64];
uint64_t blackPawnAttacks[64];

void initKingAttacks()
{
    for (int square = 0; square < 64; square++)
    {
        kingAttacks[square] = 0ULL;

        int rank = square / 8;
        int file = square % 8;

        for (int dr = -1; dr <= 1; dr++)
        {
            for (int df = -1; df <= 1; df++)
            {
                if (dr == 0 && df == 0)
                    continue;

                int r = rank + dr;
                int f = file + df;

                if (r >= 0 && r < 8 && f >= 0 && f < 8)
                {
                    kingAttacks[square] |= 1ULL << (r * 8 + f);
                }
            }
        }
    }
}

void initPawnAttacks()
{
    for (int square = 0; square < 64; square++)
    {
        whitePawnAttacks[square] = 0ULL;
        blackPawnAttacks[square] = 0ULL;

        int rank = square / 8;
        int file = square % 8;

        // White pawn attacks upward
        if (rank < 7)
        {
            if (file > 0)
                whitePawnAttacks[square] |= 1ULL << ((rank + 1) * 8 + file - 1);

            if (file < 7)
                whitePawnAttacks[square] |= 1ULL << ((rank + 1) * 8 + file + 1);
        }

        // Black pawn attacks downward
        if (rank > 0)
        {
            if (file > 0)
                blackPawnAttacks[square] |= 1ULL << ((rank - 1) * 8 + file - 1);

            if (file < 7)
                blackPawnAttacks[square] |= 1ULL << ((rank - 1) * 8 + file + 1);
        }
    }
}

bool isSquareAttacked(const Board& board, int square, bool byWhite)
{
    uint64_t occupancy = board.getAllPieces();

    // Knight attacks
    uint64_t knights = byWhite ? board.getWhiteKnights() : board.getBlackKnights();
    if (knightAttacks[square] & knights)
        return true;

    // King attacks
    uint64_t king = byWhite ? board.getWhiteKing() : board.getBlackKing();
    if (kingAttacks[square] & king)
        return true;

    // Pawn attacks
    // If checking attacks BY white, we look at black's pawn-attack table from `square`
    // (i.e. "would a black pawn on `square` be attacked by a white pawn?")
    uint64_t pawns = byWhite ? board.getWhitePawns() : board.getBlackPawns();
    uint64_t pawnAttackersFromSquare = byWhite ? blackPawnAttacks[square] : whitePawnAttacks[square];
    if (pawnAttackersFromSquare & pawns)
        return true;

    // Bishop / Queen (diagonal sliders)
    uint64_t bishopsQueens = byWhite
        ? (board.getWhiteBishops() | board.getWhiteQueens())
        : (board.getBlackBishops() | board.getBlackQueens());
    if (getBishopAttacks(square, occupancy) & bishopsQueens)
        return true;

    // Rook / Queen (straight sliders)
    uint64_t rooksQueens = byWhite
        ? (board.getWhiteRooks() | board.getWhiteQueens())
        : (board.getBlackRooks() | board.getBlackQueens());
    if (getRookAttacks(square, occupancy) & rooksQueens)
        return true;

    return false;
}