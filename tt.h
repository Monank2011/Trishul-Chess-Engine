#ifndef TT_H
#define TT_H

#include <cstdint>
#include <cstddef>
#include "move.h"

enum TTFlag
{
    TT_EXACT,
    TT_LOWERBOUND,
    TT_UPPERBOUND
};

void ttInit(size_t sizeMB);
void ttStore(uint64_t hash, int depth, int score, int flag, const Move& bestMove, int staticEval);
void ttClear();

bool ttProbe(
    uint64_t hash,
    int depth,
    int alpha,
    int beta,
    int ply,
    int& outScore,
    Move& outMove,
    int& outStaticEval
);

Move ttGetBestMove(uint64_t hash);

#endif