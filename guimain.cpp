#include "board.h"
#include "attacks.h"
#include "magic.h"
#include "movegen.h"
#include "search.h"
#include "uci.h"
#include "zobrist.h"
#include "tt.h"
#include "move.h"

#include <iostream>
#include <cstdint>
#include <string>
#include <vector>

int main()
{
    initKnightAttacks();
    initMagicBitboards();
    initKingAttacks();
    initPawnAttacks();
    initZobrist();

    ttInit(64);
    initKillerMoves();
    initHistoryTable();

    uciLoop();
    return 0;
}