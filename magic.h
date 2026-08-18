#ifndef MAGIC_H
#define MAGIC_H

#include <cstdint>

extern uint64_t rookMasks[64];
extern uint64_t bishopMasks[64];

extern int rookRelevantBits[64];
extern int bishopRelevantBits[64];

extern uint64_t rookAttackTable[64][4096];
extern uint64_t bishopAttackTable[64][512];

uint64_t setOccupancy(
    int index,
    int bits,
    uint64_t attackMask);

uint64_t maskRookRelevant(int square);
uint64_t maskBishopRelevant(int square);

uint64_t rookAttacks(
    int square,
    uint64_t occupancy);

uint64_t bishopAttacks(
    int square,
    uint64_t occupancy);

uint64_t getRookAttacks(
    int square,
    uint64_t occupancy);

uint64_t getBishopAttacks(
    int square,
    uint64_t occupancy);

uint64_t getQueenAttacks(
    int square,
    uint64_t occupancy);

void initMagicBitboards();

#endif