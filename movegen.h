#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "board.h"
#include "move.h"
#include <vector>

class MoveGenerator
{
public:
    std::vector<Move> generateMoves(Board& board);
    std::vector<Move> generateLegalMoves(Board& board);
    std::vector<Move> generateLegalCaptures(Board& board);
};

#endif