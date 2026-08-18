#ifndef TT_H
#define TT_H

#include <cstdint>
#include "move.h"

enum TTFlag
{
    TT_EXACT,
    TT_LOWERBOUND,
    TT_UPPERBOUND
};

void ttInit(size_t sizeMB);
void ttStore(uint64_t hash, int depth, int score, int flag, const Move& bestMove);
void ttClear();
bool ttProbe(uint64_t hash, int depth, int alpha, int beta, int& outScore, Move& outMove);
Move ttGetBestMove(uint64_t hash);   // for move ordering even on partial/non-usable hits

#endif