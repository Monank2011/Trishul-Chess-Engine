#include "eval.h"
#include "bitboard.h"
#include "attacks.h"
#include "magic.h"

// ============================================================
// Tapered evaluation
// ============================================================
//
// Every term has a midgame (mg) and endgame (eg) value.
// The final score is:
//
//     (mg * phase + eg * (24 - phase)) / 24
//
// where phase counts remaining pieces. Start position = 24,
// bare kings = 0. Phase clamps at 24 in case of multiple
// queens from promotion.

// ============================================================
// Phase weights
// ============================================================

static const int PHASE_KNIGHT = 1;
static const int PHASE_BISHOP = 1;
static const int PHASE_ROOK   = 2;
static const int PHASE_QUEEN  = 4;
static const int TOTAL_PHASE  = 24;

// ============================================================
// Material (mg, eg) — PeSTO-derived
// ============================================================

static const int MATERIAL_MG[6] = {  82, 337, 365, 477, 1025, 0 };
static const int MATERIAL_EG[6] = {  94, 281, 297, 512,  936, 0 };

// ============================================================
// PSTs — MIDGAME
// Rank 8 at index 0 (a8), rank 1 at index 63 (h1).
// mirrorForTable() flips for white so the physical board
// orientation is correct for each side.
// ============================================================

static const int PAWN_MG[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     98, 134,  61,  95,  68, 126,  34, -11,
     -6,   7,  26,  31,  65,  56,  25, -20,
    -14,  13,   6,  21,  23,  12,  17, -23,
    -27,  -2,  -5,  12,  17,   6,  10, -25,
    -26,  -4,  -4, -10,   3,   3,  33, -12,
    -35,  -1, -20, -23, -15,  24,  38, -22,
      0,   0,   0,   0,   0,   0,   0,   0
};

static const int KNIGHT_MG[64] = {
   -167, -89, -34, -49,  61, -97, -15, -107,
    -73, -41,  72,  36,  23,  62,   7,  -17,
    -47,  60,  37,  65,  84, 129,  73,   44,
     -9,  17,  19,  53,  37,  69,  18,   22,
    -13,   4,  16,  13,  28,  19,  21,   -8,
    -23,  -9,  12,  10,  19,  17,  25,  -16,
    -29, -53, -12,  -3,  -1,  18, -14,  -19,
   -105, -21, -58, -33, -17, -28, -19,  -23
};

static const int BISHOP_MG[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21
};

static const int ROOK_MG[64] = {
     32,  42,  32,  51,  63,   9,  31,  43,
     27,  32,  58,  62,  80,  67,  26,  44,
     -5,  19,  26,  36,  17,  45,  61,  16,
    -24, -11,   7,  26,  24,  35,  -8, -20,
    -36, -26, -12,  -1,   9,  -7,   6, -23,
    -45, -25, -16, -17,   3,   0,  -5, -33,
    -44, -16, -20,  -9,  -1,  11,  -6, -71,
    -19, -13,   1,  17,  16,   7, -37, -26
};

static const int QUEEN_MG[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50
};

static const int KING_MG[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14
};

// ============================================================
// PSTs — ENDGAME
// ============================================================

static const int PAWN_EG[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0
};

static const int KNIGHT_EG[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64
};

static const int BISHOP_EG[64] = {
    -14, -21, -11,  -8,  -7,  -9, -17, -24,
     -8,  -4,   7, -12,  -3, -13,  -4, -14,
      2,  -8,   0,  -1,  -2,   6,   0,   4,
     -3,   9,  12,   9,  14,  10,   3,   2,
     -6,   3,  13,  19,   7,  10,  -3,  -9,
    -12,  -3,   8,  10,  13,   3,  -7, -15,
    -14, -18,  -7,  -1,   4,  -9, -15, -27,
    -23,  -9, -23,  -5,  -9, -16,  -5, -17
};

static const int ROOK_EG[64] = {
     13,  10,  18,  15,  12,  12,   8,   5,
     11,  13,  13,  11,  -3,   3,   8,   3,
      7,   7,   7,   5,   4,  -3,  -5,  -3,
      4,   3,  13,   1,   2,   1,  -1,   2,
      3,   5,   8,   4,  -5,  -6,  -8, -11,
     -4,   0,  -5,  -1,  -7, -12,  -8, -16,
     -6,  -6,   0,   2,  -9,  -9, -11,  -3,
     -9,   2,   3,  -1,  -5, -13,   4, -20
};

static const int QUEEN_EG[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41
};

static const int KING_EG[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43
};

// ============================================================
// Tapered tuning constants
// ============================================================

static const int MOBILITY_MG = 4;
static const int MOBILITY_EG = 4;

static const int BISHOP_PAIR_MG = 40;
static const int BISHOP_PAIR_EG = 60;

static const int ROOK_OPEN_FILE_MG = 25;
static const int ROOK_OPEN_FILE_EG = 15;

static const int ROOK_SEMI_OPEN_MG = 12;
static const int ROOK_SEMI_OPEN_EG = 8;

static const int ROOK_OPEN_KING_MG = 15;
static const int ROOK_OPEN_KING_EG = 0;

// King-attack danger scale — MIDGAME ONLY. In the endgame,
// the (24 - phase) weighting naturally fades the term out,
// which is what we want: king safety stops mattering once
// pieces come off.
static const int KING_ATTACK_SCALE[8] = { 0, 0, 15, 40, 80, 130, 190, 260 };

// ============================================================
// Piece-square indexing
// ============================================================

static int mirrorForTable(int square, bool white)
{
    int rank = square / 8;
    int file = square % 8;

    if (white)
        return (7 - rank) * 8 + file;
    else
        return rank * 8 + file;
}

struct TaperScore { int mg; int eg; };

static TaperScore pstScore(
    uint64_t bb,
    const int mgTable[64],
    const int egTable[64],
    bool white
)
{
    TaperScore s;
    s.mg = 0;
    s.eg = 0;

    while (bb)
    {
        int sq = __builtin_ctzll(bb);
        int idx = mirrorForTable(sq, white);

        s.mg += mgTable[idx];
        s.eg += egTable[idx];

        bb &= bb - 1;
    }

    return s;
}

static uint64_t kingZone(int kingSquare)
{
    return kingAttacks[kingSquare] | (1ULL << kingSquare);
}

// ============================================================
// Mobility + king-attack term (fully tapered)
// ============================================================

static TaperScore mobilityAndKingSafety(const Board& board, bool white)
{
    TaperScore result;
    result.mg = 0;
    result.eg = 0;

    uint64_t occupancy = board.getAllPieces();
    uint64_t ownPieces = white ? board.getWhitePieces() : board.getBlackPieces();

    int enemyKingSquare = __builtin_ctzll(
        white ? board.getBlackKing() : board.getWhiteKing()
    );
    uint64_t enemyKingZone = kingZone(enemyKingSquare);

    int kingAttackerCount = 0;

    // ---------------- Knights ----------------
    {
        uint64_t knights = white ? board.getWhiteKnights() : board.getBlackKnights();

        while (knights)
        {
            int sq = __builtin_ctzll(knights);
            uint64_t attacks = knightAttacks[sq] & ~ownPieces;

            int mob = popCount(attacks);
            result.mg += mob * MOBILITY_MG;
            result.eg += mob * MOBILITY_EG;

            if (attacks & enemyKingZone) kingAttackerCount++;

            knights &= knights - 1;
        }
    }

    // ---------------- Bishops ----------------
    {
        uint64_t bishops = white ? board.getWhiteBishops() : board.getBlackBishops();
        int bishopCount = popCount(bishops);

        while (bishops)
        {
            int sq = __builtin_ctzll(bishops);
            uint64_t attacks = getBishopAttacks(sq, occupancy) & ~ownPieces;

            int mob = popCount(attacks);
            result.mg += mob * MOBILITY_MG;
            result.eg += mob * MOBILITY_EG;

            if (attacks & enemyKingZone) kingAttackerCount++;

            bishops &= bishops - 1;
        }

        if (bishopCount >= 2)
        {
            result.mg += BISHOP_PAIR_MG;
            result.eg += BISHOP_PAIR_EG;
        }
    }

    // ---------------- Rooks ----------------
    {
        uint64_t rooks     = white ? board.getWhiteRooks() : board.getBlackRooks();
        uint64_t ownPawns   = white ? board.getWhitePawns() : board.getBlackPawns();
        uint64_t enemyPawns = white ? board.getBlackPawns() : board.getWhitePawns();
        int enemyKingFile = enemyKingSquare % 8;

        while (rooks)
        {
            int sq = __builtin_ctzll(rooks);
            uint64_t attacks = getRookAttacks(sq, occupancy) & ~ownPieces;

            int mob = popCount(attacks);
            result.mg += mob * MOBILITY_MG;
            result.eg += mob * MOBILITY_EG;

            if (attacks & enemyKingZone) kingAttackerCount++;

            int file = sq % 8;
            uint64_t fileMask = 0x0101010101010101ULL << file;

            bool ownPawnOnFile   = (fileMask & ownPawns) != 0;
            bool enemyPawnOnFile = (fileMask & enemyPawns) != 0;

            if (!ownPawnOnFile && !enemyPawnOnFile)
            {
                result.mg += ROOK_OPEN_FILE_MG;
                result.eg += ROOK_OPEN_FILE_EG;

                if (file == enemyKingFile)
                {
                    result.mg += ROOK_OPEN_KING_MG;
                    result.eg += ROOK_OPEN_KING_EG;
                }
            }
            else if (!ownPawnOnFile)
            {
                result.mg += ROOK_SEMI_OPEN_MG;
                result.eg += ROOK_SEMI_OPEN_EG;

                if (file == enemyKingFile)
                {
                    result.mg += ROOK_OPEN_KING_MG;
                    result.eg += ROOK_OPEN_KING_EG;
                }
            }

            rooks &= rooks - 1;
        }
    }

    // ---------------- Queens ----------------
    {
        uint64_t queens = white ? board.getWhiteQueens() : board.getBlackQueens();

        while (queens)
        {
            int sq = __builtin_ctzll(queens);
            uint64_t attacks = getQueenAttacks(sq, occupancy) & ~ownPieces;

            int mob = popCount(attacks);
            result.mg += mob * MOBILITY_MG;
            result.eg += mob * MOBILITY_EG;

            if (attacks & enemyKingZone) kingAttackerCount++;

            queens &= queens - 1;
        }
    }

    // King-danger bonus: MIDGAME ONLY.
    if (kingAttackerCount > 7)
        kingAttackerCount = 7;

    result.mg += KING_ATTACK_SCALE[kingAttackerCount];

    return result;
}

// ============================================================
// Phase
// ============================================================

static int computePhase(const Board& board)
{
    int phase = 0;

    phase += popCount(board.getWhiteKnights() | board.getBlackKnights()) * PHASE_KNIGHT;
    phase += popCount(board.getWhiteBishops() | board.getBlackBishops()) * PHASE_BISHOP;
    phase += popCount(board.getWhiteRooks()   | board.getBlackRooks())   * PHASE_ROOK;
    phase += popCount(board.getWhiteQueens()  | board.getBlackQueens())  * PHASE_QUEEN;

    if (phase > TOTAL_PHASE)
        phase = TOTAL_PHASE;

    return phase;
}

// ============================================================
// Main evaluation
// ============================================================

int evaluate(const Board& board)
{
    if (board.getWhiteKing() == 0 || board.getBlackKing() == 0)
        return 0;

    int phase = computePhase(board);

    int mgWhite = 0, egWhite = 0;
    int mgBlack = 0, egBlack = 0;

    // ---------------- Material ----------------
    mgWhite += popCount(board.getWhitePawns())   * MATERIAL_MG[PAWN];
    mgWhite += popCount(board.getWhiteKnights()) * MATERIAL_MG[KNIGHT];
    mgWhite += popCount(board.getWhiteBishops()) * MATERIAL_MG[BISHOP];
    mgWhite += popCount(board.getWhiteRooks())   * MATERIAL_MG[ROOK];
    mgWhite += popCount(board.getWhiteQueens())  * MATERIAL_MG[QUEEN];

    egWhite += popCount(board.getWhitePawns())   * MATERIAL_EG[PAWN];
    egWhite += popCount(board.getWhiteKnights()) * MATERIAL_EG[KNIGHT];
    egWhite += popCount(board.getWhiteBishops()) * MATERIAL_EG[BISHOP];
    egWhite += popCount(board.getWhiteRooks())   * MATERIAL_EG[ROOK];
    egWhite += popCount(board.getWhiteQueens())  * MATERIAL_EG[QUEEN];

    mgBlack += popCount(board.getBlackPawns())   * MATERIAL_MG[PAWN];
    mgBlack += popCount(board.getBlackKnights()) * MATERIAL_MG[KNIGHT];
    mgBlack += popCount(board.getBlackBishops()) * MATERIAL_MG[BISHOP];
    mgBlack += popCount(board.getBlackRooks())   * MATERIAL_MG[ROOK];
    mgBlack += popCount(board.getBlackQueens())  * MATERIAL_MG[QUEEN];

    egBlack += popCount(board.getBlackPawns())   * MATERIAL_EG[PAWN];
    egBlack += popCount(board.getBlackKnights()) * MATERIAL_EG[KNIGHT];
    egBlack += popCount(board.getBlackBishops()) * MATERIAL_EG[BISHOP];
    egBlack += popCount(board.getBlackRooks())   * MATERIAL_EG[ROOK];
    egBlack += popCount(board.getBlackQueens())  * MATERIAL_EG[QUEEN];

    // ---------------- Piece-square tables ----------------
    TaperScore s;

    s = pstScore(board.getWhitePawns(),   PAWN_MG,   PAWN_EG,   true);
    mgWhite += s.mg; egWhite += s.eg;

    s = pstScore(board.getWhiteKnights(), KNIGHT_MG, KNIGHT_EG, true);
    mgWhite += s.mg; egWhite += s.eg;

    s = pstScore(board.getWhiteBishops(), BISHOP_MG, BISHOP_EG, true);
    mgWhite += s.mg; egWhite += s.eg;

    s = pstScore(board.getWhiteRooks(),   ROOK_MG,   ROOK_EG,   true);
    mgWhite += s.mg; egWhite += s.eg;

    s = pstScore(board.getWhiteQueens(),  QUEEN_MG,  QUEEN_EG,  true);
    mgWhite += s.mg; egWhite += s.eg;

    s = pstScore(board.getWhiteKing(),    KING_MG,   KING_EG,   true);
    mgWhite += s.mg; egWhite += s.eg;

    s = pstScore(board.getBlackPawns(),   PAWN_MG,   PAWN_EG,   false);
    mgBlack += s.mg; egBlack += s.eg;

    s = pstScore(board.getBlackKnights(), KNIGHT_MG, KNIGHT_EG, false);
    mgBlack += s.mg; egBlack += s.eg;

    s = pstScore(board.getBlackBishops(), BISHOP_MG, BISHOP_EG, false);
    mgBlack += s.mg; egBlack += s.eg;

    s = pstScore(board.getBlackRooks(),   ROOK_MG,   ROOK_EG,   false);
    mgBlack += s.mg; egBlack += s.eg;

    s = pstScore(board.getBlackQueens(),  QUEEN_MG,  QUEEN_EG,  false);
    mgBlack += s.mg; egBlack += s.eg;

    s = pstScore(board.getBlackKing(),    KING_MG,   KING_EG,   false);
    mgBlack += s.mg; egBlack += s.eg;

    // ---------------- Mobility + king safety ----------------
    TaperScore mw = mobilityAndKingSafety(board, true);
    TaperScore mb = mobilityAndKingSafety(board, false);

    mgWhite += mw.mg; egWhite += mw.eg;
    mgBlack += mb.mg; egBlack += mb.eg;

    // ---------------- Blend ----------------
    int mg = mgWhite - mgBlack;
    int eg = egWhite - egBlack;

    int score = (mg * phase + eg * (TOTAL_PHASE - phase)) / TOTAL_PHASE;

    return board.isWhiteToMove() ? score : -score;
}