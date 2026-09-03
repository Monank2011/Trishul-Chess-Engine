// trishul_shim.cpp
#include "magic.h"
#include "attacks.h"

extern "C" {

    void shim_init() {
        initMagicBitboards();
        initKnightAttacks();
        initKingAttacks();
        initPawnAttacks();
    }

    unsigned long long shim_rook_attacks(int square, unsigned long long occupancy) {
        return getRookAttacks(square, occupancy);
    }

    unsigned long long shim_bishop_attacks(int square, unsigned long long occupancy) {
        return getBishopAttacks(square, occupancy);
    }

    unsigned long long shim_queen_attacks(int square, unsigned long long occupancy) {
        return getQueenAttacks(square, occupancy);
    }

    unsigned long long shim_knight_attacks(int square) {
        return knightAttacks[square];
    }

    unsigned long long shim_king_attacks(int square) {
        return kingAttacks[square];
    }

    unsigned long long shim_pawn_attacks(int square, int is_white) {
        return is_white ? whitePawnAttacks[square] : blackPawnAttacks[square];
    }

}