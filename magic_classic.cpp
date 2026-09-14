#include "magic_classic.h"
#include <cstdint>
#include <vector>
#include <random>
#include <iostream>

// ============================================================
// Global attack tables
// ============================================================

uint64_t rookAttackTable[64][4096];
uint64_t bishopAttackTable[64][512];

// ============================================================
// Internal magic numbers and shifts (static, not exposed)
// ============================================================

static uint64_t rookMagics[64];
static uint64_t bishopMagics[64];
static int rookShifts[64];
static int bishopShifts[64];

static uint64_t rookMasks[64];
static uint64_t bishopMasks[64];

// ============================================================
// Slow attack generation (used only during init)
// ============================================================

static uint64_t maskRookRelevant(int square)
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

static uint64_t maskBishopRelevant(int square)
{
    uint64_t attacks = 0ULL;
    int rank = square / 8;
    int file = square % 8;

    // Up-right
    for (int r = rank + 1, f = file + 1; r <= 6 && f <= 6; r++, f++)
        attacks |= 1ULL << (r * 8 + f);
    // Up-left
    for (int r = rank + 1, f = file - 1; r <= 6 && f >= 1; r++, f--)
        attacks |= 1ULL << (r * 8 + f);
    // Down-right
    for (int r = rank - 1, f = file + 1; r >= 1 && f <= 6; r--, f++)
        attacks |= 1ULL << (r * 8 + f);
    // Down-left
    for (int r = rank - 1, f = file - 1; r >= 1 && f >= 1; r--, f--)
        attacks |= 1ULL << (r * 8 + f);

    return attacks;
}

static uint64_t rookAttacks(int square, uint64_t occupancy)
{
    uint64_t attacks = 0ULL;
    int rank = square / 8;
    int file = square % 8;

    // Up
    for (int r = rank + 1; r <= 7; r++) {
        int target = r * 8 + file;
        attacks |= 1ULL << target;
        if (occupancy & (1ULL << target)) break;
    }
    // Down
    for (int r = rank - 1; r >= 0; r--) {
        int target = r * 8 + file;
        attacks |= 1ULL << target;
        if (occupancy & (1ULL << target)) break;
    }
    // Right
    for (int f = file + 1; f <= 7; f++) {
        int target = rank * 8 + f;
        attacks |= 1ULL << target;
        if (occupancy & (1ULL << target)) break;
    }
    // Left
    for (int f = file - 1; f >= 0; f--) {
        int target = rank * 8 + f;
        attacks |= 1ULL << target;
        if (occupancy & (1ULL << target)) break;
    }

    return attacks;
}

static uint64_t bishopAttacks(int square, uint64_t occupancy)
{
    uint64_t attacks = 0ULL;
    int rank = square / 8;
    int file = square % 8;

    // Up-right
    for (int r = rank + 1, f = file + 1; r <= 7 && f <= 7; r++, f++) {
        int target = r * 8 + f;
        attacks |= 1ULL << target;
        if (occupancy & (1ULL << target)) break;
    }
    // Up-left
    for (int r = rank + 1, f = file - 1; r <= 7 && f >= 0; r++, f--) {
        int target = r * 8 + f;
        attacks |= 1ULL << target;
        if (occupancy & (1ULL << target)) break;
    }
    // Down-right
    for (int r = rank - 1, f = file + 1; r >= 0 && f <= 7; r--, f++) {
        int target = r * 8 + f;
        attacks |= 1ULL << target;
        if (occupancy & (1ULL << target)) break;
    }
    // Down-left
    for (int r = rank - 1, f = file - 1; r >= 0 && f >= 0; r--, f--) {
        int target = r * 8 + f;
        attacks |= 1ULL << target;
        if (occupancy & (1ULL << target)) break;
    }

    return attacks;
}

// ============================================================
// Helper: generate occupancy from index
// ============================================================

static uint64_t generateOccupancy(int index, int bits, uint64_t mask)
{
    uint64_t occupancy = 0ULL;
    for (int i = 0; i < bits; i++) {
        int square = __builtin_ctzll(mask);
        mask &= mask - 1;
        if (index & (1 << i))
            occupancy |= (1ULL << square);
    }
    return occupancy;
}

// ============================================================
// Initialize classic magic bitboards
// ============================================================

void initMagicBitboards()
{
    // Fixed seed for reproducibility
    std::mt19937_64 rng(0x123456789ABCDEFULL);

    for (int square = 0; square < 64; square++)
    {
        // ----------------------------------------------------
        // ROOKS
        // ----------------------------------------------------
        rookMasks[square] = maskRookRelevant(square);
        int bits = __builtin_popcountll(rookMasks[square]);
        int numOccupancies = 1 << bits;

        std::vector<uint64_t> occupancies(numOccupancies);
        std::vector<uint64_t> attacks(numOccupancies);

        for (int i = 0; i < numOccupancies; i++) {
            occupancies[i] = generateOccupancy(i, bits, rookMasks[square]);
            attacks[i] = rookAttacks(square, occupancies[i]);
        }

        bool found = false;
        while (!found) {
            uint64_t magic = rng() & rng() & rng();

            uint64_t tempTable[4096] = {0};
            bool used[4096] = {false};
            bool collision = false;

            for (int i = 0; i < numOccupancies; i++) {
                uint64_t index = (occupancies[i] * magic) >> (64 - bits);

                if (used[index] && tempTable[index] != attacks[i]) {
                    collision = true;
                    break;
                }
                used[index] = true;
                tempTable[index] = attacks[i];
            }

            if (!collision) {
                rookMagics[square] = magic;
                rookShifts[square] = 64 - bits;

                for (int i = 0; i < numOccupancies; i++) {
                    uint64_t index = (occupancies[i] * magic) >> (64 - bits);
                    rookAttackTable[square][index] = attacks[i];
                }
                found = true;
            }
        }

        // ----------------------------------------------------
        // BISHOPS
        // ----------------------------------------------------
        bishopMasks[square] = maskBishopRelevant(square);
        bits = __builtin_popcountll(bishopMasks[square]);
        numOccupancies = 1 << bits;

        occupancies.resize(numOccupancies);
        attacks.resize(numOccupancies);

        for (int i = 0; i < numOccupancies; i++) {
            occupancies[i] = generateOccupancy(i, bits, bishopMasks[square]);
            attacks[i] = bishopAttacks(square, occupancies[i]);
        }

        found = false;
        while (!found) {
            uint64_t magic = rng() & rng() & rng();

            uint64_t tempTable[512] = {0};
            bool used[512] = {false};
            bool collision = false;

            for (int i = 0; i < numOccupancies; i++) {
                uint64_t index = (occupancies[i] * magic) >> (64 - bits);

                if (used[index] && tempTable[index] != attacks[i]) {
                    collision = true;
                    break;
                }
                used[index] = true;
                tempTable[index] = attacks[i];
            }

            if (!collision) {
                bishopMagics[square] = magic;
                bishopShifts[square] = 64 - bits;

                for (int i = 0; i < numOccupancies; i++) {
                    uint64_t index = (occupancies[i] * magic) >> (64 - bits);
                    bishopAttackTable[square][index] = attacks[i];
                }
                found = true;
            }
        }
    }
}

// ============================================================
// Fast attack lookups
// ============================================================

uint64_t getRookAttacks(int square, uint64_t occupancy)
{
    occupancy &= rookMasks[square];
    uint64_t index = (occupancy * rookMagics[square]) >> rookShifts[square];
    return rookAttackTable[square][index];
}

uint64_t getBishopAttacks(int square, uint64_t occupancy)
{
    occupancy &= bishopMasks[square];
    uint64_t index = (occupancy * bishopMagics[square]) >> bishopShifts[square];
    return bishopAttackTable[square][index];
}

uint64_t getQueenAttacks(int square, uint64_t occupancy)
{
    return getRookAttacks(square, occupancy) | getBishopAttacks(square, occupancy);
}