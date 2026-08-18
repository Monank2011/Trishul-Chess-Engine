#include "movegen.h"
#include "attacks.h"
#include "magic.h"

// Helper: find what piece type (if any) occupies a square for a given side
static int getPieceTypeAt(Board& board, bool white, int square)
{
    uint64_t bit = 1ULL << square;

    if (white)
    {
        if (board.getWhitePawns()   & bit) return PAWN;
        if (board.getWhiteKnights() & bit) return KNIGHT;
        if (board.getWhiteBishops() & bit) return BISHOP;
        if (board.getWhiteRooks()   & bit) return ROOK;
        if (board.getWhiteQueens()  & bit) return QUEEN;
        if (board.getWhiteKing()    & bit) return KING;
    }
    else
    {
        if (board.getBlackPawns()   & bit) return PAWN;
        if (board.getBlackKnights() & bit) return KNIGHT;
        if (board.getBlackBishops() & bit) return BISHOP;
        if (board.getBlackRooks()   & bit) return ROOK;
        if (board.getBlackQueens()  & bit) return QUEEN;
        if (board.getBlackKing()    & bit) return KING;
    }

    return NONE;
}

std::vector<Move> MoveGenerator::generateMoves(Board& board)
{
    std::vector<Move> moves;

    bool white = board.isWhiteToMove();

    uint64_t pawns = white
                     ? board.getWhitePawns()
                     : board.getBlackPawns();

    uint64_t ownPieces = white
                         ? board.getWhitePieces()
                         : board.getBlackPieces();

    uint64_t allPieces = board.getAllPieces();

    int epSquare = board.getEnPassantSquare();   // 🆕

    // --------------------------------------------------------
    // PAWN MOVES
    // --------------------------------------------------------

    while (pawns)
    {
        int from = __builtin_ctzll(pawns);

        int promoRank = white ? 7 : 0;

        if (white)
        {
            // One square forward
            int to = from + 8;

            if (to < 64 && !(allPieces & (1ULL << to)))
            {
                if (to / 8 == promoRank)
                {
                    for (int promo : {KNIGHT, BISHOP, ROOK, QUEEN})
                        moves.push_back({from, to, PAWN, NONE, promo, FLAG_NONE});
                }
                else
                {
                    moves.push_back({from, to, PAWN, NONE, NONE, FLAG_NONE});

                    if (from >= 8 && from <= 15)
                    {
                        int doubleTo = from + 16;

                        if (!(allPieces & (1ULL << doubleTo)))
                        {
                            moves.push_back({from, doubleTo, PAWN, NONE, NONE, FLAG_NONE});
                        }
                    }
                }
            }

            // Capture left
            if (from % 8 != 0)
            {
                int capture = from + 7;

                if (capture < 64 && (board.getBlackPieces() & (1ULL << capture)))
                {
                    int capturedPiece = getPieceTypeAt(board, false, capture);

                    if (capture / 8 == promoRank)
                    {
                        for (int promo : {KNIGHT, BISHOP, ROOK, QUEEN})
                            moves.push_back({from, capture, PAWN, capturedPiece, promo, FLAG_NONE});
                    }
                    else
                    {
                        moves.push_back({from, capture, PAWN, capturedPiece, NONE, FLAG_NONE});
                    }
                }
                else if (capture == epSquare)   // 🆕 en passant capture left
                {
                    moves.push_back({from, capture, PAWN, PAWN, NONE, FLAG_EN_PASSANT});
                }
            }

            // Capture right
            if (from % 8 != 7)
            {
                int capture = from + 9;

                if (capture < 64 && (board.getBlackPieces() & (1ULL << capture)))
                {
                    int capturedPiece = getPieceTypeAt(board, false, capture);

                    if (capture / 8 == promoRank)
                    {
                        for (int promo : {KNIGHT, BISHOP, ROOK, QUEEN})
                            moves.push_back({from, capture, PAWN, capturedPiece, promo, FLAG_NONE});
                    }
                    else
                    {
                        moves.push_back({from, capture, PAWN, capturedPiece, NONE, FLAG_NONE});
                    }
                }
                else if (capture == epSquare)   // 🆕 en passant capture right
                {
                    moves.push_back({from, capture, PAWN, PAWN, NONE, FLAG_EN_PASSANT});
                }
            }
        }
        else
        {
            // One square forward
            int to = from - 8;

            if (to >= 0 && !(allPieces & (1ULL << to)))
            {
                if (to / 8 == promoRank)
                {
                    for (int promo : {KNIGHT, BISHOP, ROOK, QUEEN})
                        moves.push_back({from, to, PAWN, NONE, promo, FLAG_NONE});
                }
                else
                {
                    moves.push_back({from, to, PAWN, NONE, NONE, FLAG_NONE});

                    if (from >= 48 && from <= 55)
                    {
                        int doubleTo = from - 16;

                        if (!(allPieces & (1ULL << doubleTo)))
                        {
                            moves.push_back({from, doubleTo, PAWN, NONE, NONE, FLAG_NONE});
                        }
                    }
                }
            }

            // Capture left
            if (from % 8 != 0)
            {
                int capture = from - 9;

                if (capture >= 0 && (board.getWhitePieces() & (1ULL << capture)))
                {
                    int capturedPiece = getPieceTypeAt(board, true, capture);

                    if (capture / 8 == promoRank)
                    {
                        for (int promo : {KNIGHT, BISHOP, ROOK, QUEEN})
                            moves.push_back({from, capture, PAWN, capturedPiece, promo, FLAG_NONE});
                    }
                    else
                    {
                        moves.push_back({from, capture, PAWN, capturedPiece, NONE, FLAG_NONE});
                    }
                }
                else if (capture == epSquare)   // 🆕 en passant capture left
                {
                    moves.push_back({from, capture, PAWN, PAWN, NONE, FLAG_EN_PASSANT});
                }
            }

            // Capture right
            if (from % 8 != 7)
            {
                int capture = from - 7;

                if (capture >= 0 && (board.getWhitePieces() & (1ULL << capture)))
                {
                    int capturedPiece = getPieceTypeAt(board, true, capture);

                    if (capture / 8 == promoRank)
                    {
                        for (int promo : {KNIGHT, BISHOP, ROOK, QUEEN})
                            moves.push_back({from, capture, PAWN, capturedPiece, promo, FLAG_NONE});
                    }
                    else
                    {
                        moves.push_back({from, capture, PAWN, capturedPiece, NONE, FLAG_NONE});
                    }
                }
                else if (capture == epSquare)   // 🆕 en passant capture right
                {
                    moves.push_back({from, capture, PAWN, PAWN, NONE, FLAG_EN_PASSANT});
                }
            }
        }

        pawns &= pawns - 1;
    }

    // --------------------------------------------------------
    // KNIGHT MOVES
    // --------------------------------------------------------

    uint64_t knights = white ? board.getWhiteKnights() : board.getBlackKnights();

    while (knights)
    {
        int from = __builtin_ctzll(knights);

        uint64_t attacks = knightAttacks[from] & ~ownPieces;

        while (attacks)
        {
            int to = __builtin_ctzll(attacks);
            int capturedPiece = getPieceTypeAt(board, !white, to);

            moves.push_back({from, to, KNIGHT, capturedPiece, NONE, FLAG_NONE});

            attacks &= attacks - 1;
        }

        knights &= knights - 1;
    }

    // --------------------------------------------------------
    // KING MOVES
    // --------------------------------------------------------

    uint64_t king = white ? board.getWhiteKing() : board.getBlackKing();

    while (king)
    {
        int from = __builtin_ctzll(king);

        uint64_t attacks = kingAttacks[from] & ~ownPieces;

        while (attacks)
        {
            int to = __builtin_ctzll(attacks);
            int capturedPiece = getPieceTypeAt(board, !white, to);

            moves.push_back({from, to, KING, capturedPiece, NONE, FLAG_NONE});

            attacks &= attacks - 1;
        }

        king &= king - 1;
    }

    // --------------------------------------------------------
    // CASTLING MOVES
    // --------------------------------------------------------

    if (white)
    {
        // Kingside: e1 -> g1
        if (board.getWhiteCanCastleKingside())
        {
            bool squaresEmpty = !(allPieces & (1ULL << 5)) &&   // f1
                                 !(allPieces & (1ULL << 6));     // g1

            if (squaresEmpty)
            {
                bool pathSafe = !isSquareAttacked(board, 4, false) &&  // e1
                                !isSquareAttacked(board, 5, false) &&  // f1
                                !isSquareAttacked(board, 6, false);    // g1

                if (pathSafe)
                {
                    moves.push_back({4, 6, KING, NONE, NONE, FLAG_CASTLE_KINGSIDE});
                }
            }
        }

        // Queenside: e1 -> c1
        if (board.getWhiteCanCastleQueenside())
        {
            bool squaresEmpty = !(allPieces & (1ULL << 1)) &&   // b1
                                 !(allPieces & (1ULL << 2)) &&   // c1
                                 !(allPieces & (1ULL << 3));     // d1

            if (squaresEmpty)
            {
                bool pathSafe = !isSquareAttacked(board, 4, false) &&  // e1
                                !isSquareAttacked(board, 3, false) &&  // d1
                                !isSquareAttacked(board, 2, false);    // c1

                if (pathSafe)
                {
                    moves.push_back({4, 2, KING, NONE, NONE, FLAG_CASTLE_QUEENSIDE});
                }
            }
        }
    }
    else
    {
        // Kingside: e8 -> g8
        if (board.getBlackCanCastleKingside())
        {
            bool squaresEmpty = !(allPieces & (1ULL << 61)) &&  // f8
                                 !(allPieces & (1ULL << 62));    // g8

            if (squaresEmpty)
            {
                bool pathSafe = !isSquareAttacked(board, 60, true) &&  // e8
                                !isSquareAttacked(board, 61, true) &&  // f8
                                !isSquareAttacked(board, 62, true);    // g8

                if (pathSafe)
                {
                    moves.push_back({60, 62, KING, NONE, NONE, FLAG_CASTLE_KINGSIDE});
                }
            }
        }

        // Queenside: e8 -> c8
        if (board.getBlackCanCastleQueenside())
        {
            bool squaresEmpty = !(allPieces & (1ULL << 57)) &&  // b8
                                 !(allPieces & (1ULL << 58)) &&  // c8
                                 !(allPieces & (1ULL << 59));    // d8

            if (squaresEmpty)
            {
                bool pathSafe = !isSquareAttacked(board, 60, true) &&  // e8
                                !isSquareAttacked(board, 59, true) &&  // d8
                                !isSquareAttacked(board, 58, true);    // c8

                if (pathSafe)
                {
                    moves.push_back({60, 58, KING, NONE, NONE, FLAG_CASTLE_QUEENSIDE});
                }
            }
        }
    }

    // --------------------------------------------------------
    // BISHOP MOVES
    // --------------------------------------------------------

    uint64_t bishops = white ? board.getWhiteBishops() : board.getBlackBishops();

    while (bishops)
    {
        int from = __builtin_ctzll(bishops);

        uint64_t attacks = getBishopAttacks(from, allPieces) & ~ownPieces;

        while (attacks)
        {
            int to = __builtin_ctzll(attacks);
            int capturedPiece = getPieceTypeAt(board, !white, to);

            moves.push_back({from, to, BISHOP, capturedPiece, NONE, FLAG_NONE});

            attacks &= attacks - 1;
        }

        bishops &= bishops - 1;
    }

    // --------------------------------------------------------
    // ROOK MOVES
    // --------------------------------------------------------

    uint64_t rooks = white ? board.getWhiteRooks() : board.getBlackRooks();

    while (rooks)
    {
        int from = __builtin_ctzll(rooks);

        uint64_t attacks = getRookAttacks(from, allPieces) & ~ownPieces;

        while (attacks)
        {
            int to = __builtin_ctzll(attacks);
            int capturedPiece = getPieceTypeAt(board, !white, to);

            moves.push_back({from, to, ROOK, capturedPiece, NONE, FLAG_NONE});

            attacks &= attacks - 1;
        }

        rooks &= rooks - 1;
    }

    // --------------------------------------------------------
    // QUEEN MOVES
    // --------------------------------------------------------

    uint64_t queens = white ? board.getWhiteQueens() : board.getBlackQueens();

    while (queens)
    {
        int from = __builtin_ctzll(queens);

        uint64_t attacks = getQueenAttacks(from, allPieces) & ~ownPieces;

        while (attacks)
        {
            int to = __builtin_ctzll(attacks);
            int capturedPiece = getPieceTypeAt(board, !white, to);

            moves.push_back({from, to, QUEEN, capturedPiece, NONE, FLAG_NONE});

            attacks &= attacks - 1;
        }

        queens &= queens - 1;
    }

    return moves;
}

std::vector<Move> MoveGenerator::generateLegalMoves(Board& board)
{
    std::vector<Move> pseudoMoves = generateMoves(board);
    std::vector<Move> legalMoves;

    bool sideToMove = board.isWhiteToMove();

    for (const Move& move : pseudoMoves)
    {
        UndoInfo undo = board.makeMove(move.from, move.to, move.promotion);

        uint64_t kingBitboard = sideToMove
           ? board.getWhiteKing()
           : board.getBlackKing();
 
        int kingSquare = __builtin_ctzll(kingBitboard);

        bool kingInCheck = isSquareAttacked(board, kingSquare, !sideToMove);

        board.unmakeMove(move.from, move.to, undo);

        if (!kingInCheck)
        {
            legalMoves.push_back(move);
        }
    }

    return legalMoves;
}

std::vector<Move> MoveGenerator::generateLegalCaptures(Board& board)
{
    std::vector<Move> legalMoves = generateLegalMoves(board);
    std::vector<Move> captures;

    for (const Move& move : legalMoves)
    {
        if (move.captured != NONE)
        {
            captures.push_back(move);
        }
    }

    return captures;
}