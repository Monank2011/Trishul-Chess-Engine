#ifndef MAGIC_CLASSIC_H
#define MAGIC_CLASSIC_H

#include <cstdint>

extern uint64_t rookAttackTable[64][4096];
extern uint64_t bishopAttackTable[64][512];

void initMagicBitboards();

uint64_t getRookAttacks(int square, uint64_t occupancy);
uint64_t getBishopAttacks(int square, uint64_t occupancy);
uint64_t getQueenAttacks(int square, uint64_t occupancy);

#endif