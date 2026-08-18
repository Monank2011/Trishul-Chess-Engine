#include "board.h"
#include <iostream>
#include <sstream>

Board::Board()
{
    reset();
}

void Board::recomputeZobristHash()
{
    zobristHash = 0;

    uint64_t whiteBBs[6] =
    {
        whitePawns, whiteKnights, whiteBishops,
        whiteRooks, whiteQueens, whiteKing
    };

    uint64_t blackBBs[6] =
    {
        blackPawns, blackKnights, blackBishops,
        blackRooks, blackQueens, blackKing
    };

    for (int piece = 0; piece < 6; piece++)
    {
        uint64_t bb = whiteBBs[piece];

        while (bb)
        {
            int sq = __builtin_ctzll(bb);
            zobristHash ^= zobristPieceKeys[0][piece][sq];
            bb &= bb - 1;
        }

        bb = blackBBs[piece];

        while (bb)
        {
            int sq = __builtin_ctzll(bb);
            zobristHash ^= zobristPieceKeys[1][piece][sq];
            bb &= bb - 1;
        }
    }

    if (!whiteToMove)
        zobristHash ^= zobristBlackToMove;

    int castlingIndex =
        (whiteCanCastleKingside  ? 1 : 0) |
        (whiteCanCastleQueenside ? 2 : 0) |
        (blackCanCastleKingside  ? 4 : 0) |
        (blackCanCastleQueenside ? 8 : 0);

    zobristHash ^= zobristCastlingKeys[castlingIndex];

    if (enPassantSquare != -1)
        zobristHash ^= zobristEnPassantKeys[enPassantSquare % 8];
}

void Board::reset()
{
    whitePawns   = 0x000000000000FF00;
    whiteKnights = 0x0000000000000042;
    whiteBishops = 0x0000000000000024;
    whiteRooks   = 0x0000000000000081;
    whiteQueens  = 0x0000000000000008;
    whiteKing    = 0x0000000000000010;

    blackPawns   = 0x00FF000000000000;
    blackKnights = 0x4200000000000000;
    blackBishops = 0x2400000000000000;
    blackRooks   = 0x8100000000000000;
    blackQueens  = 0x0800000000000000;
    blackKing    = 0x1000000000000000;

    whitePieces = whitePawns | whiteKnights | whiteBishops |
                  whiteRooks | whiteQueens | whiteKing;

    blackPieces = blackPawns | blackKnights | blackBishops |
                  blackRooks | blackQueens | blackKing;

    allPieces = whitePieces | blackPieces;

    whiteToMove = true;

    enPassantSquare = -1;

    whiteCanCastleKingside = true;
    whiteCanCastleQueenside = true;
    blackCanCastleKingside = true;
    blackCanCastleQueenside = true;

    halfmoveClock = 0;
    positionHistory.clear();

    recomputeZobristHash();

    positionHistory.push_back(zobristHash);
}

void Board::loadFEN(const std::string& fen)
{
    whitePawns = whiteKnights = whiteBishops = 0;
    whiteRooks = whiteQueens  = whiteKing    = 0;

    blackPawns = blackKnights = blackBishops = 0;
    blackRooks = blackQueens  = blackKing    = 0;

    std::istringstream iss(fen);

    std::string boardPart;
    std::string activeColor;
    std::string castlingPart;
    std::string epPart;

    int halfmove = 0;
    int fullmove = 1;

    iss >> boardPart >> activeColor >> castlingPart >> epPart;

    if (!(iss >> halfmove))
        halfmove = 0;

    if (!(iss >> fullmove))
        fullmove = 1;


    // ----------------------------------------------------
    // Piece placement
    // ----------------------------------------------------

    int rank = 7;   // FEN's first row is rank 8
    int file = 0;

    for (char c : boardPart)
    {
        if (c == '/')
        {
            rank--;
            file = 0;
        }
        else if (c >= '0' && c <= '9')
        {
            file += (c - '0');
        }
        else
        {
            int square = rank * 8 + file;
            uint64_t bit = 1ULL << square;

            switch (c)
            {
                case 'P': whitePawns   |= bit; break;
                case 'N': whiteKnights |= bit; break;
                case 'B': whiteBishops |= bit; break;
                case 'R': whiteRooks   |= bit; break;
                case 'Q': whiteQueens  |= bit; break;
                case 'K': whiteKing    |= bit; break;

                case 'p': blackPawns   |= bit; break;
                case 'n': blackKnights |= bit; break;
                case 'b': blackBishops |= bit; break;
                case 'r': blackRooks   |= bit; break;
                case 'q': blackQueens  |= bit; break;
                case 'k': blackKing    |= bit; break;
            }

            file++;
        }
    }


    // ----------------------------------------------------
    // Side to move
    // ----------------------------------------------------

    whiteToMove = (activeColor == "w");


    // ----------------------------------------------------
    // Castling rights
    // ----------------------------------------------------

    whiteCanCastleKingside  = castlingPart.find('K') != std::string::npos;
    whiteCanCastleQueenside = castlingPart.find('Q') != std::string::npos;
    blackCanCastleKingside  = castlingPart.find('k') != std::string::npos;
    blackCanCastleQueenside = castlingPart.find('q') != std::string::npos;


    // ----------------------------------------------------
    // En passant square
    // ----------------------------------------------------

    if (epPart == "-" || epPart.empty())
    {
        enPassantSquare = -1;
    }
    else
    {
        int epFile = epPart[0] - 'a';
        int epRank = epPart[1] - '1';

        enPassantSquare = epRank * 8 + epFile;
    }


    // ----------------------------------------------------
    // Halfmove clock
    // ----------------------------------------------------

    halfmoveClock = halfmove;


    // ----------------------------------------------------
    // Finalize
    // ----------------------------------------------------

    updateOccupancy();

    positionHistory.clear();

    recomputeZobristHash();

    positionHistory.push_back(zobristHash);
}

void Board::printBoard()
{
    std::cout << "\n";

    for (int rank = 7; rank >= 0; rank--)
    {
        std::cout << rank + 1 << " ";

        for (int file = 0; file < 8; file++)
        {
            int square = rank * 8 + file;
            uint64_t bit = 1ULL << square;

            char piece = '.';

            if (whitePawns & bit)   piece = 'P';
            if (whiteKnights & bit) piece = 'N';
            if (whiteBishops & bit) piece = 'B';
            if (whiteRooks & bit)   piece = 'R';
            if (whiteQueens & bit)  piece = 'Q';
            if (whiteKing & bit)    piece = 'K';

            if (blackPawns & bit)   piece = 'p';
            if (blackKnights & bit) piece = 'n';
            if (blackBishops & bit) piece = 'b';
            if (blackRooks & bit)   piece = 'r';
            if (blackQueens & bit)  piece = 'q';
            if (blackKing & bit)    piece = 'k';

            std::cout << piece << " ";
        }

        std::cout << "\n";
    }

    std::cout << "\n  a b c d e f g h\n";
}

void Board::updateOccupancy()
{
    whitePieces = whitePawns | whiteKnights | whiteBishops |
                  whiteRooks | whiteQueens | whiteKing;

    blackPieces = blackPawns | blackKnights | blackBishops |
                  blackRooks | blackQueens | blackKing;

    allPieces = whitePieces | blackPieces;
}


// ========================================================
// NULL MOVE
// ========================================================

void Board::makeNullMove()
{
    nullMoveEnPassantSquare = enPassantSquare;
    nullMoveHash = zobristHash;

    if (enPassantSquare != -1)
        zobristHash ^= zobristEnPassantKeys[enPassantSquare % 8];

    enPassantSquare = -1;

    whiteToMove = !whiteToMove;

    zobristHash ^= zobristBlackToMove;
}

void Board::unmakeNullMove()
{
    whiteToMove = !whiteToMove;

    enPassantSquare = nullMoveEnPassantSquare;

    zobristHash = nullMoveHash;
}


UndoInfo Board::makeMove(int from, int to, int promotion)
{
    uint64_t fromBit = 1ULL << from;
    uint64_t toBit   = 1ULL << to;

    uint64_t hashBefore = zobristHash;
    bool moverIsWhite = whiteToMove;

    bool movingPieceIsWhitePawn = (whitePawns & fromBit) != 0;
    bool movingPieceIsBlackPawn = (blackPawns & fromBit) != 0;
    bool isPawnMove = movingPieceIsWhitePawn || movingPieceIsBlackPawn;

    int movedPieceType = NONE;

    if (whitePawns & fromBit || blackPawns & fromBit)
        movedPieceType = PAWN;
    else if (whiteKnights & fromBit || blackKnights & fromBit)
        movedPieceType = KNIGHT;
    else if (whiteBishops & fromBit || blackBishops & fromBit)
        movedPieceType = BISHOP;
    else if (whiteRooks & fromBit || blackRooks & fromBit)
        movedPieceType = ROOK;
    else if (whiteQueens & fromBit || blackQueens & fromBit)
        movedPieceType = QUEEN;
    else if (whiteKing & fromBit || blackKing & fromBit)
        movedPieceType = KING;

    zobristHash ^=
        zobristPieceKeys[moverIsWhite ? 0 : 1][movedPieceType][from];

    bool isCastleKingside  = false;
    bool isCastleQueenside = false;

    if (movedPieceType == KING)
    {
        if (from == 4 && to == 6)
            isCastleKingside = true;
        else if (from == 4 && to == 2)
            isCastleQueenside = true;
        else if (from == 60 && to == 62)
            isCastleKingside = true;
        else if (from == 60 && to == 58)
            isCastleQueenside = true;
    }

    int capturedPieceType = NONE;

    if (whitePawns & toBit || blackPawns & toBit)
        capturedPieceType = PAWN;
    else if (whiteKnights & toBit || blackKnights & toBit)
        capturedPieceType = KNIGHT;
    else if (whiteBishops & toBit || blackBishops & toBit)
        capturedPieceType = BISHOP;
    else if (whiteRooks & toBit || blackRooks & toBit)
        capturedPieceType = ROOK;
    else if (whiteQueens & toBit || blackQueens & toBit)
        capturedPieceType = QUEEN;
    else if (whiteKing & toBit || blackKing & toBit)
        capturedPieceType = KING;

    bool whiteCanCastleKingsideBefore  = whiteCanCastleKingside;
    bool whiteCanCastleQueensideBefore = whiteCanCastleQueenside;
    bool blackCanCastleKingsideBefore  = blackCanCastleKingside;
    bool blackCanCastleQueensideBefore = blackCanCastleQueenside;

    int enPassantSquareBefore = enPassantSquare;

    if (from == 4)
        whiteCanCastleKingside = whiteCanCastleQueenside = false;

    if (from == 60)
        blackCanCastleKingside = blackCanCastleQueenside = false;

    if (from == 0 || to == 0)
        whiteCanCastleQueenside = false;

    if (from == 7 || to == 7)
        whiteCanCastleKingside = false;

    if (from == 56 || to == 56)
        blackCanCastleQueenside = false;

    if (from == 63 || to == 63)
        blackCanCastleKingside = false;

    int oldCastlingIndex =
        (whiteCanCastleKingsideBefore  ? 1 : 0) |
        (whiteCanCastleQueensideBefore ? 2 : 0) |
        (blackCanCastleKingsideBefore  ? 4 : 0) |
        (blackCanCastleQueensideBefore ? 8 : 0);

    int newCastlingIndex =
        (whiteCanCastleKingside  ? 1 : 0) |
        (whiteCanCastleQueenside ? 2 : 0) |
        (blackCanCastleKingside  ? 4 : 0) |
        (blackCanCastleQueenside ? 8 : 0);

    if (oldCastlingIndex != newCastlingIndex)
    {
        zobristHash ^= zobristCastlingKeys[oldCastlingIndex];
        zobristHash ^= zobristCastlingKeys[newCastlingIndex];
    }

    bool isEnPassantCapture = false;

    if (isPawnMove &&
        to == enPassantSquare &&
        !(allPieces & toBit) &&
        (from % 8) != (to % 8))
    {
        isEnPassantCapture = true;
        capturedPieceType = PAWN;
    }

    int halfmoveClockBefore = halfmoveClock;

    if (isPawnMove || capturedPieceType != NONE)
        halfmoveClock = 0;
    else
        halfmoveClock++;

    if (capturedPieceType != NONE)
    {
        int capturedColor = moverIsWhite ? 1 : 0;

        int captureSquare =
            isEnPassantCapture
                ? (moverIsWhite ? to - 8 : to + 8)
                : to;

        zobristHash ^=
            zobristPieceKeys[capturedColor]
                            [capturedPieceType]
                            [captureSquare];
    }

    whitePawns   &= ~toBit;
    whiteKnights &= ~toBit;
    whiteBishops &= ~toBit;
    whiteRooks   &= ~toBit;
    whiteQueens  &= ~toBit;
    whiteKing    &= ~toBit;

    blackPawns   &= ~toBit;
    blackKnights &= ~toBit;
    blackBishops &= ~toBit;
    blackRooks   &= ~toBit;
    blackQueens  &= ~toBit;
    blackKing    &= ~toBit;

    if (isEnPassantCapture)
    {
        if (movingPieceIsWhitePawn)
        {
            int capturedPawnSquare = to - 8;
            blackPawns &= ~(1ULL << capturedPawnSquare);
        }
        else if (movingPieceIsBlackPawn)
        {
            int capturedPawnSquare = to + 8;
            whitePawns &= ~(1ULL << capturedPawnSquare);
        }
    }

    int moveDistance = to - from;
    int newEnPassantSquare = -1;

    if (isPawnMove && moveDistance == 16)
        newEnPassantSquare = from + 8;
    else if (isPawnMove && moveDistance == -16)
        newEnPassantSquare = from - 8;

    if (enPassantSquareBefore != -1)
        zobristHash ^=
            zobristEnPassantKeys[enPassantSquareBefore % 8];

    if (newEnPassantSquare != -1)
        zobristHash ^=
            zobristEnPassantKeys[newEnPassantSquare % 8];

    if (whitePawns & fromBit)
    {
        whitePawns &= ~fromBit;
        whitePawns |= toBit;
    }
    else if (whiteKnights & fromBit)
    {
        whiteKnights &= ~fromBit;
        whiteKnights |= toBit;
    }
    else if (whiteBishops & fromBit)
    {
        whiteBishops &= ~fromBit;
        whiteBishops |= toBit;
    }
    else if (whiteRooks & fromBit)
    {
        whiteRooks &= ~fromBit;
        whiteRooks |= toBit;
    }
    else if (whiteQueens & fromBit)
    {
        whiteQueens &= ~fromBit;
        whiteQueens |= toBit;
    }
    else if (whiteKing & fromBit)
    {
        whiteKing &= ~fromBit;
        whiteKing |= toBit;

        if (isCastleKingside)
        {
            whiteRooks &= ~(1ULL << 7);
            whiteRooks |= (1ULL << 5);

            zobristHash ^= zobristPieceKeys[0][ROOK][7];
            zobristHash ^= zobristPieceKeys[0][ROOK][5];
        }
        else if (isCastleQueenside)
        {
            whiteRooks &= ~(1ULL << 0);
            whiteRooks |= (1ULL << 3);

            zobristHash ^= zobristPieceKeys[0][ROOK][0];
            zobristHash ^= zobristPieceKeys[0][ROOK][3];
        }
    }
    else if (blackPawns & fromBit)
    {
        blackPawns &= ~fromBit;
        blackPawns |= toBit;
    }
    else if (blackKnights & fromBit)
    {
        blackKnights &= ~fromBit;
        blackKnights |= toBit;
    }
    else if (blackBishops & fromBit)
    {
        blackBishops &= ~fromBit;
        blackBishops |= toBit;
    }
    else if (blackRooks & fromBit)
    {
        blackRooks &= ~fromBit;
        blackRooks |= toBit;
    }
    else if (blackQueens & fromBit)
    {
        blackQueens &= ~fromBit;
        blackQueens |= toBit;
    }
    else if (blackKing & fromBit)
    {
        blackKing &= ~fromBit;
        blackKing |= toBit;

        if (isCastleKingside)
        {
            blackRooks &= ~(1ULL << 63);
            blackRooks |= (1ULL << 61);

            zobristHash ^= zobristPieceKeys[1][ROOK][63];
            zobristHash ^= zobristPieceKeys[1][ROOK][61];
        }
        else if (isCastleQueenside)
        {
            blackRooks &= ~(1ULL << 56);
            blackRooks |= (1ULL << 59);

            zobristHash ^= zobristPieceKeys[1][ROOK][56];
            zobristHash ^= zobristPieceKeys[1][ROOK][59];
        }
    }

    if (promotion != NONE)
    {
        if (movingPieceIsWhitePawn)
        {
            whitePawns &= ~toBit;

            switch (promotion)
            {
                case KNIGHT: whiteKnights |= toBit; break;
                case BISHOP: whiteBishops |= toBit; break;
                case ROOK:   whiteRooks   |= toBit; break;
                case QUEEN:  whiteQueens  |= toBit; break;
            }
        }
        else if (movingPieceIsBlackPawn)
        {
            blackPawns &= ~toBit;

            switch (promotion)
            {
                case KNIGHT: blackKnights |= toBit; break;
                case BISHOP: blackBishops |= toBit; break;
                case ROOK:   blackRooks   |= toBit; break;
                case QUEEN:  blackQueens  |= toBit; break;
            }
        }
    }

    int destPieceType =
        (promotion != NONE)
            ? promotion
            : movedPieceType;

    zobristHash ^=
        zobristPieceKeys[moverIsWhite ? 0 : 1]
                        [destPieceType]
                        [to];

    enPassantSquare = newEnPassantSquare;

    updateOccupancy();

    whiteToMove = !whiteToMove;

    zobristHash ^= zobristBlackToMove;

    positionHistory.push_back(zobristHash);

    UndoInfo undo;

    undo.capturedPieceType             = capturedPieceType;
    undo.movedPieceType                = movedPieceType;
    undo.wasEnPassant                  = isEnPassantCapture;
    undo.enPassantSquareBefore         = enPassantSquareBefore;
    undo.promotionApplied              = promotion;

    undo.whiteCanCastleKingsideBefore  =
        whiteCanCastleKingsideBefore;

    undo.whiteCanCastleQueensideBefore =
        whiteCanCastleQueensideBefore;

    undo.blackCanCastleKingsideBefore =
        blackCanCastleKingsideBefore;

    undo.blackCanCastleQueensideBefore =
        blackCanCastleQueensideBefore;

    undo.hashBefore = hashBefore;
    undo.halfmoveClockBefore = halfmoveClockBefore;

    return undo;
}

void Board::unmakeMove(int from, int to, const UndoInfo& undo)
{
    whiteToMove = !whiteToMove;

    bool moverWasWhite = whiteToMove;

    uint64_t fromBit = 1ULL << from;
    uint64_t toBit   = 1ULL << to;

    uint64_t* moverBitboard = nullptr;

    if (moverWasWhite)
    {
        switch (undo.movedPieceType)
        {
            case PAWN:   moverBitboard = &whitePawns;   break;
            case KNIGHT: moverBitboard = &whiteKnights; break;
            case BISHOP: moverBitboard = &whiteBishops; break;
            case ROOK:   moverBitboard = &whiteRooks;   break;
            case QUEEN:  moverBitboard = &whiteQueens;  break;
            case KING:   moverBitboard = &whiteKing;    break;
        }
    }
    else
    {
        switch (undo.movedPieceType)
        {
            case PAWN:   moverBitboard = &blackPawns;   break;
            case KNIGHT: moverBitboard = &blackKnights; break;
            case BISHOP: moverBitboard = &blackBishops; break;
            case ROOK:   moverBitboard = &blackRooks;   break;
            case QUEEN:  moverBitboard = &blackQueens;  break;
            case KING:   moverBitboard = &blackKing;    break;
        }
    }

    *moverBitboard &= ~toBit;
    *moverBitboard |= fromBit;

    if (undo.promotionApplied != NONE)
    {
        uint64_t* promotedBitboard = nullptr;

        if (moverWasWhite)
        {
            switch (undo.promotionApplied)
            {
                case KNIGHT: promotedBitboard = &whiteKnights; break;
                case BISHOP: promotedBitboard = &whiteBishops; break;
                case ROOK:   promotedBitboard = &whiteRooks;   break;
                case QUEEN:  promotedBitboard = &whiteQueens;  break;
            }
        }
        else
        {
            switch (undo.promotionApplied)
            {
                case KNIGHT: promotedBitboard = &blackKnights; break;
                case BISHOP: promotedBitboard = &blackBishops; break;
                case ROOK:   promotedBitboard = &blackRooks;   break;
                case QUEEN:  promotedBitboard = &blackQueens;  break;
            }
        }

        *promotedBitboard &= ~toBit;
    }

    if (undo.movedPieceType == KING)
    {
        if (from == 4 && to == 6)
        {
            whiteRooks &= ~(1ULL << 5);
            whiteRooks |= (1ULL << 7);
        }
        else if (from == 4 && to == 2)
        {
            whiteRooks &= ~(1ULL << 3);
            whiteRooks |= (1ULL << 0);
        }
        else if (from == 60 && to == 62)
        {
            blackRooks &= ~(1ULL << 61);
            blackRooks |= (1ULL << 63);
        }
        else if (from == 60 && to == 58)
        {
            blackRooks &= ~(1ULL << 59);
            blackRooks |= (1ULL << 56);
        }
    }

    if (undo.capturedPieceType != NONE)
    {
        uint64_t* capturedBitboard = nullptr;
        int restoreSquare = to;

        if (undo.wasEnPassant)
        {
            restoreSquare =
                moverWasWhite
                    ? (to - 8)
                    : (to + 8);

            capturedBitboard =
                moverWasWhite
                    ? &blackPawns
                    : &whitePawns;
        }
        else
        {
            bool capturedWasWhite = !moverWasWhite;

            if (capturedWasWhite)
            {
                switch (undo.capturedPieceType)
                {
                    case PAWN:   capturedBitboard = &whitePawns;   break;
                    case KNIGHT: capturedBitboard = &whiteKnights; break;
                    case BISHOP: capturedBitboard = &whiteBishops; break;
                    case ROOK:   capturedBitboard = &whiteRooks;   break;
                    case QUEEN:  capturedBitboard = &whiteQueens;  break;
                    case KING:   capturedBitboard = &whiteKing;    break;
                }
            }
            else
            {
                switch (undo.capturedPieceType)
                {
                    case PAWN:   capturedBitboard = &blackPawns;   break;
                    case KNIGHT: capturedBitboard = &blackKnights; break;
                    case BISHOP: capturedBitboard = &blackBishops; break;
                    case ROOK:   capturedBitboard = &blackRooks;   break;
                    case QUEEN:  capturedBitboard = &blackQueens;  break;
                    case KING:   capturedBitboard = &blackKing;    break;
                }
            }
        }

        *capturedBitboard |= (1ULL << restoreSquare);
    }

    enPassantSquare = undo.enPassantSquareBefore;

    whiteCanCastleKingside =
        undo.whiteCanCastleKingsideBefore;

    whiteCanCastleQueenside =
        undo.whiteCanCastleQueensideBefore;

    blackCanCastleKingside =
        undo.blackCanCastleKingsideBefore;

    blackCanCastleQueenside =
        undo.blackCanCastleQueensideBefore;

    updateOccupancy();

    zobristHash = undo.hashBefore;

    halfmoveClock = undo.halfmoveClockBefore;

    positionHistory.pop_back();
}


// ========================================================
// GETTERS
// ========================================================

uint64_t Board::getWhitePawns() const
{
    return whitePawns;
}

uint64_t Board::getWhiteKnights() const
{
    return whiteKnights;
}

uint64_t Board::getWhiteBishops() const
{
    return whiteBishops;
}

uint64_t Board::getWhiteRooks() const
{
    return whiteRooks;
}

uint64_t Board::getWhiteQueens() const
{
    return whiteQueens;
}

uint64_t Board::getWhiteKing() const
{
    return whiteKing;
}

uint64_t Board::getBlackPawns() const
{
    return blackPawns;
}

uint64_t Board::getBlackKnights() const
{
    return blackKnights;
}

uint64_t Board::getBlackBishops() const
{
    return blackBishops;
}

uint64_t Board::getBlackRooks() const
{
    return blackRooks;
}

uint64_t Board::getBlackQueens() const
{
    return blackQueens;
}

uint64_t Board::getBlackKing() const
{
    return blackKing;
}

uint64_t Board::getWhitePieces() const
{
    return whitePieces;
}

uint64_t Board::getBlackPieces() const
{
    return blackPieces;
}

uint64_t Board::getAllPieces() const
{
    return allPieces;
}

bool Board::isWhiteToMove() const
{
    return whiteToMove;
}

int Board::getEnPassantSquare() const
{
    return enPassantSquare;
}

bool Board::getWhiteCanCastleKingside() const
{
    return whiteCanCastleKingside;
}

bool Board::getWhiteCanCastleQueenside() const
{
    return whiteCanCastleQueenside;
}

bool Board::getBlackCanCastleKingside() const
{
    return blackCanCastleKingside;
}

bool Board::getBlackCanCastleQueenside() const
{
    return blackCanCastleQueenside;
}

uint64_t Board::getZobristHash() const
{
    return zobristHash;
}

int Board::getHalfmoveClock() const
{
    return halfmoveClock;
}

bool Board::isRepetition() const
{
    if (positionHistory.empty())
        return false;

    uint64_t currentHash = positionHistory.back();

    int size = (int)positionHistory.size();

    int checked = 0;

    // A repetition can only involve positions since the
    // last irreversible move (pawn move / capture), which
    // is exactly what halfmoveClock bounds.
    for (int i = size - 2;
         i >= 0 && checked < halfmoveClock;
         i--, checked++)
    {
        if (positionHistory[i] == currentHash)
            return true;
    }

    return false;
}