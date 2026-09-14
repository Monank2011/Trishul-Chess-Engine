#include "openingbook.h"
#include "movegen.h"

#include <unordered_map>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>

static bool openingBookEnabled = true;
static std::string currentBookFile = "";

void setOpeningBookEnabled(bool enabled)
{
    openingBookEnabled = enabled;
}

bool isOpeningBookEnabled()
{
    return openingBookEnabled;
}

std::string getBookFile()
{
    return currentBookFile;
}


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
    if (s.length() < 2)
        return -1;

    int file = s[0] - 'a';
    int rank = s[1] - '1';

    if (file < 0 || file > 7 || rank < 0 || rank > 7)
        return -1;

    return rank * 8 + file;
}


// --------------------------------------------------------
// Find a legal move matching a UCI move string
// --------------------------------------------------------

static Move findMove(
    Board& board,
    MoveGenerator& generator,
    const std::string& moveString
)
{
    if (moveString.length() < 4)
        return Move{-1, -1, NONE, NONE, NONE, FLAG_NONE};

    int from = squareFromString(moveString.substr(0, 2));
    int to   = squareFromString(moveString.substr(2, 2));

    if (from == -1 || to == -1)
        return Move{-1, -1, NONE, NONE, NONE, FLAG_NONE};

    int promotion = NONE;

    if (moveString.length() == 5)
    {
        switch (moveString[4])
        {
            case 'q': promotion = QUEEN;  break;
            case 'r': promotion = ROOK;   break;
            case 'b': promotion = BISHOP; break;
            case 'n': promotion = KNIGHT; break;
            default: return Move{-1, -1, NONE, NONE, NONE, FLAG_NONE};
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

    return Move{-1, -1, NONE, NONE, NONE, FLAG_NONE};
}


// --------------------------------------------------------
// Add one line of UCI moves to the book
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
            return;   // bad line, ignore the rest

        uint64_t hash = board.getZobristHash();

        openingBook[hash].push_back({move, weight});

        board.makeMove(move.from, move.to, move.promotion);
    }
}


// --------------------------------------------------------
// Parse "weight move1 move2 ..." from one line
// --------------------------------------------------------

static bool parseBookLine(const std::string& raw, int& weight, std::string& moves)
{
    // Strip comments
    std::string line = raw;
    size_t hashPos = line.find('#');
    if (hashPos != std::string::npos)
        line = line.substr(0, hashPos);

    // Trim
    while (!line.empty() && std::isspace((unsigned char)line.front()))
        line.erase(line.begin());
    while (!line.empty() && std::isspace((unsigned char)line.back()))
        line.pop_back();

    if (line.empty())
        return false;

    std::istringstream iss(line);

    if (!(iss >> weight))
        return false;

    if (weight <= 0)
        return false;

    std::string tok;
    std::ostringstream moveStream;
    bool first = true;

    while (iss >> tok)
    {
        if (!first)
            moveStream << " ";
        moveStream << tok;
        first = false;
    }

    moves = moveStream.str();

    return !moves.empty();
}


// --------------------------------------------------------
// Load a book file
// --------------------------------------------------------

bool setBookFile(const std::string& path)
{
    std::ifstream file(path);

    if (!file.is_open())
        return false;

    openingBook.clear();
    bookInitialized = true;

    std::string line;
    int lineNumber = 0;
    int loadedLines = 0;

    while (std::getline(file, line))
    {
        lineNumber++;

        int weight = 0;
        std::string moves;

        if (!parseBookLine(line, weight, moves))
            continue;

        addOpeningLine(moves, weight);
        loadedLines++;
    }

    if (loadedLines == 0)
    {
        // File was empty / all garbage. Fall back.
        openingBook.clear();
        bookInitialized = false;
        currentBookFile = "";
        return false;
    }

    currentBookFile = path;
    return true;
}


// --------------------------------------------------------
// Built-in fallback book
// --------------------------------------------------------

static void initializeFallbackBook()
{
    if (bookInitialized)
        return;

    bookInitialized = true;

    addOpeningLine("g1f3 g8f6 g2g3 d7d5 f1g2", 35);
    addOpeningLine("g1f3 d7d5 c2c4",             35);
    addOpeningLine("g1f3 g8f6 c2c4 e7e6 g2g3",   35);
    addOpeningLine("g1f3 c7c5 c2c4",             35);
    addOpeningLine("g1f3 d7d5 g2g3",             30);
    addOpeningLine("g1f3 g8f6 g2g3",             30);

    addOpeningLine("c2c4 e7e5 b1c3 g8f6 g2g3",   25);
    addOpeningLine("c2c4 g8f6 b1c3 e7e6 g2g3",   25);
    addOpeningLine("c2c4 c7c5 b1c3",             20);

    addOpeningLine("d2d4 g8f6 c2c4 e7e6 b1c3",   20);
    addOpeningLine("d2d4 g8f6 c2c4 g7g6 b1c3",   20);
    addOpeningLine("d2d4 d7d5 c2c4 e7e6 b1c3",   20);

    addOpeningLine("e2e4 c7c5 g1f3 d7d6 d2d4",   25);
    addOpeningLine("e2e4 c7c5 g1f3 b8c6 d2d4",   25);
    addOpeningLine("e2e4 e7e5 g1f3 b8c6 f1b5",   20);
    addOpeningLine("e2e4 e7e5 g1f3 g8f6",        20);
    addOpeningLine("e2e4 e7e6 d2d4 d7d5",        20);

    addOpeningLine("e2e4 c7c5",                  35);
    addOpeningLine("e2e4 e7e5",                  25);
    addOpeningLine("e2e4 e7e6",                  20);

    addOpeningLine("d2d4 g8f6",                  30);
    addOpeningLine("d2d4 e7e6",                  25);
    addOpeningLine("d2d4 d7d5",                  20);

    addOpeningLine("g1f3 g8f6",                  30);
    addOpeningLine("g1f3 d7d5",                  25);
    addOpeningLine("g1f3 c7c5",                  20);

    addOpeningLine("c2c4 e7e5",                  30);
    addOpeningLine("c2c4 g8f6",                  30);
    addOpeningLine("c2c4 c7c5",                  20);
}


// --------------------------------------------------------
// Book lookup
// --------------------------------------------------------

Move getBookMove(const Board& board)
{
    Move none{-1, -1, NONE, NONE, NONE, FLAG_NONE};

    if (!openingBookEnabled)
        return none;

    if (!bookInitialized)
        initializeFallbackBook();

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

    if (totalWeight <= 0)
        return candidates.front().move;

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