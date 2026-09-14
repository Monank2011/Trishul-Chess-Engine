#ifndef SEE_H
#define SEE_H

#include "board.h"
#include "move.h"

// Static Exchange Evaluation.
// Returns the material outcome of the full sequence of
// captures on the target square, starting with `move`.
// Positive = good for the side making the move.
int see(const Board& board, const Move& move);

// Convenience: returns true if SEE >= threshold.
bool seeGE(const Board& board, const Move& move, int threshold);

#endif