#include "magic.h"
#include <cstdint>
#include <immintrin.h>   // for _pext_u64

// ============================================================
// PEXT bitboard data
// ============================================================

uint64_t rookMasks[64];
uint64_t bishopMasks[64];

int rookRelevantBits[64];
int bishopRelevantBits[64];

uint64_t rookAttackTable[64][4096];
uint64_t bishopAttackTable[64][512];

// ============================================================
// Set occupancy
// Creates one occupancy configuration from an index
// ============================================================

uint64_t setOccupancy(
    int index,
    int bits,
    uint64_t attackMask)
{
    uint64_t occupancy = 0ULL;

    for (int count = 0; count < bits; count++)
    {
        int square = __builtin_ctzll(attackMask);

        attackMask &= attackMask - 1;

        if (index & (1 << count))
            occupancy |= 1ULL << square;
    }

    return occupancy;
}

// ============================================================
// Rook relevant occupancy mask
// ============================================================

uint64_t maskRookRelevant(int square)
{
    uint64_t attacks = 0ULL;

    int rank = square / 8;
    int file = square % 8;

    // Up
    for (int r = rank + 1; r <= 6; r++)
        attacks |= 1ULL << (r * 8 + file);

    // Down
    for (int r = rank - 1; r >= 1; r--)
        attacks |= 1ULL << (r * 8 + file);

    // Right
    for (int f = file + 1; f <= 6; f++)
        attacks |= 1ULL << (rank * 8 + f);

    // Left
    for (int f = file - 1; f >= 1; f--)
        attacks |= 1ULL << (rank * 8 + f);

    return attacks;
}

// ============================================================
// Bishop relevant occupancy mask
// ============================================================

uint64_t maskBishopRelevant(int square)
{
    uint64_t attacks = 0ULL;

    int rank = square / 8;
    int file = square % 8;

    // Up-right
    for (int r = rank + 1, f = file + 1;
         r <= 6 && f <= 6;
         r++, f++)
    {
        attacks |= 1ULL << (r * 8 + f);
    }

    // Up-left
    for (int r = rank + 1, f = file - 1;
         r <= 6 && f >= 1;
         r++, f--)
    {
        attacks |= 1ULL << (r * 8 + f);
    }

    // Down-right
    for (int r = rank - 1, f = file + 1;
         r >= 1 && f <= 6;
         r--, f++)
    {
        attacks |= 1ULL << (r * 8 + f);
    }

    // Down-left
    for (int r = rank - 1, f = file - 1;
         r >= 1 && f >= 1;
         r--, f--)
    {
        attacks |= 1ULL << (r * 8 + f);
    }

    return attacks;
}

// ============================================================
// Rook attacks with blockers
// ============================================================

uint64_t rookAttacks(int square, uint64_t occupancy)
{
    uint64_t attacks = 0ULL;

    int rank = square / 8;
    int file = square % 8;

    // Up
    for (int r = rank + 1; r <= 7; r++)
    {
        int target = r * 8 + file;

        attacks |= 1ULL << target;

        if (occupancy & (1ULL << target))
            break;
    }

    // Down
    for (int r = rank - 1; r >= 0; r--)
    {
        int target = r * 8 + file;

        attacks |= 1ULL << target;

        if (occupancy & (1ULL << target))
            break;
    }

    // Right
    for (int f = file + 1; f <= 7; f++)
    {
        int target = rank * 8 + f;

        attacks |= 1ULL << target;

        if (occupancy & (1ULL << target))
            break;
    }

    // Left
    for (int f = file - 1; f >= 0; f--)
    {
        int target = rank * 8 + f;

        attacks |= 1ULL << target;

        if (occupancy & (1ULL << target))
            break;
    }

    return attacks;
}

// ============================================================
// Bishop attacks with blockers
// ============================================================

uint64_t bishopAttacks(int square, uint64_t occupancy)
{
    uint64_t attacks = 0ULL;

    int rank = square / 8;
    int file = square % 8;

    // Up-right
    for (int r = rank + 1, f = file + 1;
         r <= 7 && f <= 7;
         r++, f++)
    {
        int target = r * 8 + f;

        attacks |= 1ULL << target;

        if (occupancy & (1ULL << target))
            break;
    }

    // Up-left
    for (int r = rank + 1, f = file - 1;
         r <= 7 && f >= 0;
         r++, f--)
    {
        int target = r * 8 + f;

        attacks |= 1ULL << target;

        if (occupancy & (1ULL << target))
            break;
    }

    // Down-right
    for (int r = rank - 1, f = file + 1;
         r >= 0 && f <= 7;
         r--, f++)
    {
        int target = r * 8 + f;

        attacks |= 1ULL << target;

        if (occupancy & (1ULL << target))
            break;
    }

    // Down-left
    for (int r = rank - 1, f = file - 1;
         r >= 0 && f >= 0;
         r--, f--)
    {
        int target = r * 8 + f;

        attacks |= 1ULL << target;

        if (occupancy & (1ULL << target))
            break;
    }

    return attacks;
}

// ============================================================
// Initialize PEXT bitboards
// ============================================================

void initMagicBitboards()
{
    for (int square = 0;
         square < 64;
         square++)
    {
        // Masks
        rookMasks[square] =
            maskRookRelevant(square);

        bishopMasks[square] =
            maskBishopRelevant(square);

        // Relevant bits
        rookRelevantBits[square] =
            __builtin_popcountll(
                rookMasks[square]);

        bishopRelevantBits[square] =
            __builtin_popcountll(
                bishopMasks[square]);

        // No magic number search needed with PEXT -
        // we index the table directly using the hardware
        // instruction, so we just enumerate every
        // occupancy subset and store its attack set.

        int rookOccupancyCount =
            1 << rookRelevantBits[square];

        for (int index = 0;
             index < rookOccupancyCount;
             index++)
        {
            uint64_t occupancy =
                setOccupancy(
                    index,
                    rookRelevantBits[square],
                    rookMasks[square]);

            uint64_t pextIndex =
                _pext_u64(
                    occupancy,
                    rookMasks[square]);

            rookAttackTable[square][pextIndex] =
                rookAttacks(square, occupancy);
        }

        int bishopOccupancyCount =
            1 << bishopRelevantBits[square];

        for (int index = 0;
             index < bishopOccupancyCount;
             index++)
        {
            uint64_t occupancy =
                setOccupancy(
                    index,
                    bishopRelevantBits[square],
                    bishopMasks[square]);

            uint64_t pextIndex =
                _pext_u64(
                    occupancy,
                    bishopMasks[square]);

            bishopAttackTable[square][pextIndex] =
                bishopAttacks(square, occupancy);
        }
    }
}

uint64_t getRookAttacks(int square, uint64_t occupancy)
{
    occupancy &= rookMasks[square];

    uint64_t pextIndex =
        _pext_u64(occupancy, rookMasks[square]);

    return rookAttackTable[square][pextIndex];
}

uint64_t getBishopAttacks(int square, uint64_t occupancy)
{
    occupancy &= bishopMasks[square];

    uint64_t pextIndex =
        _pext_u64(occupancy, bishopMasks[square]);

    return bishopAttackTable[square][pextIndex];
}

uint64_t getQueenAttacks(int square, uint64_t occupancy)
{
    return getRookAttacks(square, occupancy) | getBishopAttacks(square, occupancy);
}