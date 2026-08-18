#ifndef BOARD_H
#define BOARD_H

#include <cstdint>
#include "move.h"
#include "zobrist.h"
#include <string>
#include <vector>

struct UndoInfo
{
    int  capturedPieceType;
    int  movedPieceType;
    bool wasEnPassant;
    int  enPassantSquareBefore;
    int  promotionApplied;

    bool whiteCanCastleKingsideBefore;
    bool whiteCanCastleQueensideBefore;
    bool blackCanCastleKingsideBefore;
    bool blackCanCastleQueensideBefore;

    uint64_t hashBefore;   // 🆕 full hash before this move, for instant restore on unmake
    int      halfmoveClockBefore;
};

class Board {
public:
    Board();

    void reset();
    void loadFEN(const std::string& fen);
    void printBoard();
    UndoInfo makeMove(int from, int to, int promotion = NONE);
    void unmakeMove(int from, int to, const UndoInfo& undo);
    void makeNullMove();
    void unmakeNullMove();

    uint64_t getWhitePawns() const;
    uint64_t getWhiteKnights() const;
    uint64_t getWhiteBishops() const;
    uint64_t getWhiteRooks() const;
    uint64_t getWhiteQueens() const;
    uint64_t getWhiteKing() const;

    uint64_t getBlackPawns() const;
    uint64_t getBlackKnights() const;
    uint64_t getBlackBishops() const;
    uint64_t getBlackRooks() const;
    uint64_t getBlackQueens() const;
    uint64_t getBlackKing() const;

    uint64_t getWhitePieces() const;
    uint64_t getBlackPieces() const;
    uint64_t getAllPieces() const;

    bool isWhiteToMove() const;

    int getEnPassantSquare() const;

    bool getWhiteCanCastleKingside() const;
    bool getWhiteCanCastleQueenside() const;
    bool getBlackCanCastleKingside() const;
    bool getBlackCanCastleQueenside() const;

    uint64_t getZobristHash() const;
    int getHalfmoveClock() const;
    bool isRepetition() const;

private:

    uint64_t whitePawns;
    uint64_t whiteKnights;
    uint64_t whiteBishops;
    uint64_t whiteRooks;
    uint64_t whiteQueens;
    uint64_t whiteKing;

    uint64_t blackPawns;
    uint64_t blackKnights;
    uint64_t blackBishops;
    uint64_t blackRooks;
    uint64_t blackQueens;
    uint64_t blackKing;

    uint64_t whitePieces;
    uint64_t blackPieces;
    uint64_t allPieces;

    bool whiteToMove;

    int enPassantSquare;

    bool whiteCanCastleKingside;
    bool whiteCanCastleQueenside;
    bool blackCanCastleKingside;
    bool blackCanCastleQueenside;

    uint64_t zobristHash;
    int nullMoveEnPassantSquare;
    uint64_t nullMoveHash;

    int halfmoveClock;
    std::vector<uint64_t> positionHistory;

    void updateOccupancy();
    void recomputeZobristHash();
};

#endif