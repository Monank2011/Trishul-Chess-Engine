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
    NONE = -1
};

enum MoveFlags
{
    FLAG_NONE             = 0,
    FLAG_EN_PASSANT       = 1 << 0, // 1
    FLAG_CASTLE_KINGSIDE  = 1 << 1, // 2
    FLAG_CASTLE_QUEENSIDE = 1 << 2  // 4
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