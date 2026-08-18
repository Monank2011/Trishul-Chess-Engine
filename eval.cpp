#include "eval.h"
#include "bitboard.h"
#include "attacks.h"
#include "magic.h"

// Non-linear king danger scaling: index = number of attacking pieces (capped at 7)
// Each additional attacker matters more than the last - real danger compounds
const int KING_ATTACK_SCALE[8] = { 0, 0, 15, 40, 80, 130, 190, 260 };

const int BISHOP_PAIR_BONUS = 40;
const int OPEN_FILE_ROOK_BONUS       = 20;
const int SEMI_OPEN_FILE_ROOK_BONUS  = 10;
const int OPEN_FILE_NEAR_KING_BONUS  = 15;   // extra if that open file also faces enemy king

// Bonus per attacked square (mobility) - modest per-square value so it doesn't overwhelm material
const int MOBILITY_WEIGHT = 4;

// Bonus per own-piece attack landing in the enemy king's "zone" (3x3 area around the king)
const int KING_ATTACK_WEIGHT = 8;

const int PAWN_VALUE   = 100;
const int KNIGHT_VALUE = 320;
const int BISHOP_VALUE = 330;
const int ROOK_VALUE   = 500;
const int QUEEN_VALUE  = 900;

// Tables written rank 8 (top) to rank 1 (bottom), a-file to h-file left to right
// (i.e. how you'd naturally read a board diagram)

const int pawnTable[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
     5,  5, 10, 25, 25, 10,  5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5, -5,-10,  0,  0,-10, -5,  5,
     5, 10, 10,-20,-20, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};

const int knightTable[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};

const int bishopTable[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};

const int rookTable[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     0,  0,  0,  5,  5,  0,  0,  0
};

const int queenTable[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

const int kingTable[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
     20, 20,  0,  0,  0,  0, 20, 20,
     20, 30, 10,  0,  0, 10, 30, 20
};

// Converts a LERF square (a1=0) into an index into the tables above
// (which are written top-row-first, i.e. rank 8 first)
int mirrorForTable(int square, bool white)
{
    int rank = square / 8;
    int file = square % 8;

    if (white)
    {
        // White reads the table "upside down" relative to LERF rank order
        int tableRank = 7 - rank;
        return tableRank * 8 + file;
    }
    else
    {
        // Black uses the table as-is from its own perspective (mirrored ranks)
        return rank * 8 + file;
    }
}

int pieceSquareScore(uint64_t bitboard, const int table[64], bool white)
{
    int score = 0;
    uint64_t bb = bitboard;

    while (bb)
    {
        int square = __builtin_ctzll(bb);
        score += table[mirrorForTable(square, white)];
        bb &= bb - 1;
    }

    return score;
}

uint64_t kingZone(int kingSquare)
{
    // Reuse the king attack table - it's exactly the "surrounding squares" pattern we want,
    // plus the king's own square
    return kingAttacks[kingSquare] | (1ULL << kingSquare);
}

int mobilityAndKingSafety(const Board& board, bool white)
{
    uint64_t occupancy = board.getAllPieces();
    uint64_t ownPieces = white ? board.getWhitePieces() : board.getBlackPieces();

    int enemyKingSquare = __builtin_ctzll(white ? board.getBlackKing() : board.getWhiteKing());
    uint64_t enemyKingZone = kingZone(enemyKingSquare);

    int score = 0;
    int kingAttackerCount = 0;   // 🆕 how many distinct pieces attack the zone

    // Knights
    uint64_t knights = white ? board.getWhiteKnights() : board.getBlackKnights();
    while (knights)
    {
        int sq = __builtin_ctzll(knights);
        uint64_t attacks = knightAttacks[sq] & ~ownPieces;
        score += popCount(attacks) * MOBILITY_WEIGHT;
        if (attacks & enemyKingZone) kingAttackerCount++;
        knights &= knights - 1;
    }

    // Bishops
    uint64_t bishops = white ? board.getWhiteBishops() : board.getBlackBishops();
    int bishopCount = popCount(bishops);   // 🆕 for bishop pair bonus
    while (bishops)
    {
        int sq = __builtin_ctzll(bishops);
        uint64_t attacks = getBishopAttacks(sq, occupancy) & ~ownPieces;
        score += popCount(attacks) * MOBILITY_WEIGHT;
        if (attacks & enemyKingZone) kingAttackerCount++;
        bishops &= bishops - 1;
    }
    if (bishopCount >= 2) score += BISHOP_PAIR_BONUS;   // 🆕

    // Rooks
    uint64_t rooks = white ? board.getWhiteRooks() : board.getBlackRooks();
    uint64_t ownPawns   = white ? board.getWhitePawns() : board.getBlackPawns();
    uint64_t enemyPawns = white ? board.getBlackPawns() : board.getWhitePawns();
    int enemyKingFile = enemyKingSquare % 8;

    while (rooks)
    {
        int sq = __builtin_ctzll(rooks);
        uint64_t attacks = getRookAttacks(sq, occupancy) & ~ownPieces;
        score += popCount(attacks) * MOBILITY_WEIGHT;
        if (attacks & enemyKingZone) kingAttackerCount++;

        // 🆕 Open/semi-open file bonus
        int file = sq % 8;
        uint64_t fileMask = 0x0101010101010101ULL << file;

        bool ownPawnOnFile   = fileMask & ownPawns;
        bool enemyPawnOnFile = fileMask & enemyPawns;

        if (!ownPawnOnFile && !enemyPawnOnFile)
        {
            score += OPEN_FILE_ROOK_BONUS;
            if (file == enemyKingFile) score += OPEN_FILE_NEAR_KING_BONUS;
        }
        else if (!ownPawnOnFile)
        {
            score += SEMI_OPEN_FILE_ROOK_BONUS;
            if (file == enemyKingFile) score += OPEN_FILE_NEAR_KING_BONUS;
        }

        rooks &= rooks - 1;
    }

    // Queens
    uint64_t queens = white ? board.getWhiteQueens() : board.getBlackQueens();
    while (queens)
    {
        int sq = __builtin_ctzll(queens);
        uint64_t attacks = getQueenAttacks(sq, occupancy) & ~ownPieces;
        score += popCount(attacks) * MOBILITY_WEIGHT;
        if (attacks & enemyKingZone) kingAttackerCount++;
        queens &= queens - 1;
    }

    // 🆕 Apply non-linear king danger bonus based on total distinct attackers
    if (kingAttackerCount > 7) kingAttackerCount = 7;
    score += KING_ATTACK_SCALE[kingAttackerCount];

    return score;
}

int evaluate(const Board& board)
{
    int whiteScore = 0;
    int blackScore = 0;

    // Material
    whiteScore += popCount(board.getWhitePawns())   * PAWN_VALUE;
    whiteScore += popCount(board.getWhiteKnights()) * KNIGHT_VALUE;
    whiteScore += popCount(board.getWhiteBishops()) * BISHOP_VALUE;
    whiteScore += popCount(board.getWhiteRooks())   * ROOK_VALUE;
    whiteScore += popCount(board.getWhiteQueens())  * QUEEN_VALUE;

    blackScore += popCount(board.getBlackPawns())   * PAWN_VALUE;
    blackScore += popCount(board.getBlackKnights()) * KNIGHT_VALUE;
    blackScore += popCount(board.getBlackBishops()) * BISHOP_VALUE;
    blackScore += popCount(board.getBlackRooks())   * ROOK_VALUE;
    blackScore += popCount(board.getBlackQueens())  * QUEEN_VALUE;

    // Piece-square bonuses
    whiteScore += pieceSquareScore(board.getWhitePawns(),   pawnTable,   true);
    whiteScore += pieceSquareScore(board.getWhiteKnights(), knightTable, true);
    whiteScore += pieceSquareScore(board.getWhiteBishops(), bishopTable, true);
    whiteScore += pieceSquareScore(board.getWhiteRooks(),   rookTable,   true);
    whiteScore += pieceSquareScore(board.getWhiteQueens(),  queenTable,  true);
    whiteScore += pieceSquareScore(board.getWhiteKing(),    kingTable,   true);

    blackScore += pieceSquareScore(board.getBlackPawns(),   pawnTable,   false);
    blackScore += pieceSquareScore(board.getBlackKnights(), knightTable, false);
    blackScore += pieceSquareScore(board.getBlackBishops(), bishopTable, false);
    blackScore += pieceSquareScore(board.getBlackRooks(),   rookTable,   false);
    blackScore += pieceSquareScore(board.getBlackQueens(),  queenTable,  false);
    blackScore += pieceSquareScore(board.getBlackKing(),    kingTable,   false);

    whiteScore += mobilityAndKingSafety(board, true);
    blackScore += mobilityAndKingSafety(board, false);

    int score = whiteScore - blackScore;

    return board.isWhiteToMove() ? score : -score;
}