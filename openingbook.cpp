#include "openingbook.h"
#include "movegen.h"

#include <unordered_map>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <sstream>

struct BookMove
{
    Move move;
    int weight;
};

static std::unordered_map<uint64_t, std::vector<BookMove>> openingBook;
static bool bookInitialized = false;

static std::mt19937 rng(
    static_cast<unsigned int>(
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count()
    )
);


// --------------------------------------------------------
// Convert UCI square to square index
// --------------------------------------------------------

static int squareFromString(const std::string& s)
{
    int file = s[0] - 'a';
    int rank = s[1] - '1';

    return rank * 8 + file;
}


// --------------------------------------------------------
// Find a legal move matching a UCI move string
// --------------------------------------------------------

static Move findMove(Board& board, MoveGenerator& generator,
                     const std::string& moveString)
{
    int from = squareFromString(moveString.substr(0, 2));
    int to   = squareFromString(moveString.substr(2, 2));

    int promotion = NONE;

    if (moveString.length() == 5)
    {
        switch (moveString[4])
        {
            case 'q': promotion = QUEEN;  break;
            case 'r': promotion = ROOK;   break;
            case 'b': promotion = BISHOP; break;
            case 'n': promotion = KNIGHT; break;
        }
    }

    std::vector<Move> moves = generator.generateLegalMoves(board);

    for (const Move& move : moves)
    {
        if (move.from == from &&
            move.to == to &&
            move.promotion == promotion)
        {
            return move;
        }
    }

    Move invalid{-1, -1, NONE, NONE, NONE, FLAG_NONE};
    return invalid;
}


// --------------------------------------------------------
// Add one opening line to the book
// --------------------------------------------------------

static void addOpeningLine(const std::string& line, int weight)
{
    Board board;
    MoveGenerator generator;

    std::istringstream stream(line);
    std::string moveString;

    while (stream >> moveString)
    {
        Move move = findMove(board, generator, moveString);

        if (move.from == -1)
        {
            // Invalid opening line.
            // Ignore the rest of this line.
            return;
        }

        uint64_t hash = board.getZobristHash();

        openingBook[hash].push_back({move, weight});

        UndoInfo undo =
            board.makeMove(move.from, move.to, move.promotion);

        (void)undo;
    }
}


// --------------------------------------------------------
// Build Trishul's opening repertoire
// --------------------------------------------------------

static void initializeOpeningBook()
{
    if (bookInitialized)
        return;

    bookInitialized = true;

    /*
        ====================================================
        RÉTI
        ====================================================

        Highest overall weighting.
        Trishul still gets its beloved Nf3, but doesn't
        blindly repeat the same continuation.
    */

    addOpeningLine(
        "g1f3 g8f6 g2g3 d7d5 f1g2",
        35
    );

    addOpeningLine(
        "g1f3 d7d5 c2c4",
        35
    );

    addOpeningLine(
        "g1f3 g8f6 c2c4 e7e6 g2g3",
        35
    );

    addOpeningLine(
        "g1f3 c7c5 c2c4",
        35
    );

    addOpeningLine(
        "g1f3 d7d5 g2g3",
        30
    );

    addOpeningLine(
        "g1f3 g8f6 g2g3",
        30
    );


    /*
        ====================================================
        ENGLISH
        ====================================================
    */

    addOpeningLine(
        "c2c4 e7e5 b1c3 g8f6 g2g3",
        25
    );

    addOpeningLine(
        "c2c4 g8f6 b1c3 e7e6 g2g3",
        25
    );

    addOpeningLine(
        "c2c4 c7c5 b1c3",
        20
    );


    /*
        ====================================================
        1.d4
        ====================================================
    */

    addOpeningLine(
        "d2d4 g8f6 c2c4 e7e6 b1c3",
        20
    );

    addOpeningLine(
        "d2d4 g8f6 c2c4 g7g6 b1c3",
        20
    );

    addOpeningLine(
        "d2d4 d7d5 c2c4 e7e6 b1c3",
        20
    );


    /*
        ====================================================
        1.e4
        ====================================================
    */

    addOpeningLine(
        "e2e4 c7c5 g1f3 d7d6 d2d4",
        25
    );

    addOpeningLine(
        "e2e4 c7c5 g1f3 b8c6 d2d4",
        25
    );

    addOpeningLine(
        "e2e4 e7e5 g1f3 b8c6 f1b5",
        20
    );

    addOpeningLine(
        "e2e4 e7e5 g1f3 g8f6",
        20
    );

    addOpeningLine(
        "e2e4 e7e6 d2d4 d7d5",
        20
    );


    /*
        ====================================================
        BLACK VS 1.e4
        ====================================================

        Active responses. No "sit there and wait politely"
        nonsense.
    */

    addOpeningLine(
        "e2e4 c7c5",
        35
    );

    addOpeningLine(
        "e2e4 e7e5",
        25
    );

    addOpeningLine(
        "e2e4 e7e6",
        20
    );


    /*
        ====================================================
        BLACK VS 1.d4
        ====================================================
    */

    addOpeningLine(
        "d2d4 g8f6",
        30
    );

    addOpeningLine(
        "d2d4 e7e6",
        25
    );

    addOpeningLine(
        "d2d4 d7d5",
        20
    );

    addOpeningLine(
        "d2d4 g8f6 c2c4 e7e6",
        30
    );

    addOpeningLine(
        "d2d4 g8f6 c2c4 g7g6",
        25
    );


    /*
        ====================================================
        BLACK VS 1.Nf3
        ====================================================
    */

    addOpeningLine(
        "g1f3 g8f6",
        30
    );

    addOpeningLine(
        "g1f3 d7d5",
        25
    );

    addOpeningLine(
        "g1f3 c7c5",
        20
    );


    /*
        ====================================================
        BLACK VS 1.c4
        ====================================================
    */

    addOpeningLine(
        "c2c4 e7e5",
        30
    );

    addOpeningLine(
        "c2c4 g8f6",
        30
    );

    addOpeningLine(
        "c2c4 c7c5",
        20
    );


    /*
        ====================================================
        Additional dynamic branches
        ====================================================
    */

    addOpeningLine(
        "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4",
        15
    );

    addOpeningLine(
        "e2e4 c7c5 g1f3 b8c6 d2d4 c5d4",
        15
    );

    addOpeningLine(
        "g1f3 g8f6 c2c4 e7e6 g2g3 d7d5",
        25
    );

    addOpeningLine(
        "g1f3 d7d5 c2c4 e7e6",
        25
    );
}


// --------------------------------------------------------
// Get a book move for the current position
// --------------------------------------------------------

Move getBookMove(const Board& board)
{
    initializeOpeningBook();

    Move none{-1, -1, NONE, NONE, NONE, FLAG_NONE};

    uint64_t hash = board.getZobristHash();

    auto it = openingBook.find(hash);

    if (it == openingBook.end())
        return none;

    const std::vector<BookMove>& candidates = it->second;

    if (candidates.empty())
        return none;

    int totalWeight = 0;

    for (const BookMove& candidate : candidates)
        totalWeight += candidate.weight;

    std::uniform_int_distribution<int> distribution(1, totalWeight);

    int choice = distribution(rng);

    for (const BookMove& candidate : candidates)
    {
        choice -= candidate.weight;

        if (choice <= 0)
            return candidate.move;
    }

    return candidates.back().move;
}