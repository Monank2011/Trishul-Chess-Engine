#include "see.h"
#include "attacks.h"
#include "magic.h"

#include <algorithm>

// Defined in search.cpp
extern int getPieceValue(int pieceType);


// ------------------------------------------------------------
// Get a bitboard of one piece type for one side.
// ------------------------------------------------------------
static uint64_t getPieceBitboard(
    const Board& board,
    bool white,
    int pieceType
)
{
    switch (pieceType)
    {
        case PAWN:   return white ? board.getWhitePawns()   : board.getBlackPawns();
        case KNIGHT: return white ? board.getWhiteKnights() : board.getBlackKnights();
        case BISHOP: return white ? board.getWhiteBishops() : board.getBlackBishops();
        case ROOK:   return white ? board.getWhiteRooks()   : board.getBlackRooks();
        case QUEEN:  return white ? board.getWhiteQueens()  : board.getBlackQueens();
        case KING:   return white ? board.getWhiteKing()    : board.getBlackKing();
    }

    return 0ULL;
}


// ------------------------------------------------------------
// All pieces of one side that attack `square`, given the
// current occupancy (so x-rays through removed pieces are
// handled naturally).
// ------------------------------------------------------------
static uint64_t getAttackers(
    const Board& board,
    int square,
    uint64_t occupancy,
    bool byWhite
)
{
    uint64_t attackers = 0ULL;

    // Pawns - reverse lookup: a white pawn attacks `square`
    // iff that pawn sits on a square a black pawn on `square`
    // would attack.
    uint64_t pawns = byWhite
        ? board.getWhitePawns()
        : board.getBlackPawns();

    uint64_t pawnAttackers = byWhite
        ? blackPawnAttacks[square]
        : whitePawnAttacks[square];

    attackers |= pawns & pawnAttackers & occupancy;

    // Knights
    uint64_t knights = byWhite
        ? board.getWhiteKnights()
        : board.getBlackKnights();

    attackers |= knights & knightAttacks[square] & occupancy;

    // Bishops + Queens (diagonal)
    uint64_t bishopsQueens = byWhite
        ? (board.getWhiteBishops() | board.getWhiteQueens())
        : (board.getBlackBishops() | board.getBlackQueens());

    attackers |= bishopsQueens & getBishopAttacks(square, occupancy) & occupancy;

    // Rooks + Queens (straight)
    uint64_t rooksQueens = byWhite
        ? (board.getWhiteRooks() | board.getWhiteQueens())
        : (board.getBlackRooks() | board.getBlackQueens());

    attackers |= rooksQueens & getRookAttacks(square, occupancy) & occupancy;

    // King
    uint64_t king = byWhite
        ? board.getWhiteKing()
        : board.getBlackKing();

    attackers |= king & kingAttacks[square] & occupancy;

    return attackers;
}


// ------------------------------------------------------------
// SEE
// ------------------------------------------------------------
int see(const Board& board, const Move& move)
{
    int from = move.from;
    int to   = move.to;

    int swapList[32];
    int d = 0;

    // Initial gain: value of the captured piece.
    swapList[0] = (move.captured != NONE)
        ? getPieceValue(move.captured)
        : 0;

    // Promotion gain.
    if (move.promotion != NONE)
    {
        swapList[0] +=
            getPieceValue(move.promotion) - getPieceValue(PAWN);
    }

    // Remove the moving piece from the board.
    uint64_t occupancy = board.getAllPieces();
    occupancy &= ~(1ULL << from);

    // En passant: the captured pawn isn't on the target square.
    if (move.flags == FLAG_EN_PASSANT)
    {
        int capturedPawnSq = board.isWhiteToMove()
            ? to - 8
            : to + 8;

        occupancy &= ~(1ULL << capturedPawnSq);
    }

    bool sideToMove = !board.isWhiteToMove();

    while (true)
    {
        uint64_t attackers = getAttackers(
            board,
            to,
            occupancy,
            sideToMove
        );

        if (!attackers)
            break;

        // Least valuable attacker first.
        int attackerSquare = -1;
        int attackerPiece  = NONE;

        for (int pt = PAWN; pt <= KING; pt++)
        {
            uint64_t pieceBB = getPieceBitboard(board, sideToMove, pt);
            uint64_t possible = attackers & pieceBB;

            if (possible)
            {
                attackerSquare = __builtin_ctzll(possible);
                attackerPiece  = pt;
                break;
            }
        }

        if (attackerSquare == -1)
            break;

        d++;

        swapList[d] =
            getPieceValue(attackerPiece) - swapList[d - 1];

        // Neither side wants to continue.
        if (std::max(-swapList[d - 1], swapList[d]) < 0)
            break;

        // Piece is now on `to`, so remove it from its old square.
        occupancy &= ~(1ULL << attackerSquare);

        // If a king "captured", the resulting position is
        // illegal - stop here.
        if (attackerPiece == KING)
            break;

        sideToMove = !sideToMove;
    }

    // Back up the swap list (minimax).
    while (d > 0)
    {
        d--;
        swapList[d] =
            -std::max(-swapList[d], swapList[d + 1]);
    }

    return swapList[0];
}


bool seeGE(const Board& board, const Move& move, int threshold)
{
    return see(board, move) >= threshold;
}