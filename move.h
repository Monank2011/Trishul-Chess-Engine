#ifndef MOVE_H
#define MOVE_H

enum PieceType
{
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    NONE = -1   // used for "no capture"
};

// Move flags (bitmask-style, expand later for castling etc.)
enum MoveFlags
{
    FLAG_NONE      = 0,
    FLAG_EN_PASSANT = 1,
    FLAG_CASTLE_KINGSIDE  = 2,
    FLAG_CASTLE_QUEENSIDE = 3
};

struct Move
{
    int from;
    int to;
    int piece;
    int captured;
    int promotion;
    int flags;
};

#endif