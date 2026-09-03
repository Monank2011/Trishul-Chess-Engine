#include "uci.h"
#include "board.h"
#include "movegen.h"
#include "search.h"
#include "move.h"
#include "threads.h"
#include "tt.h"
#include "openingbook.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <atomic>

// ------------------------------------------------------------
// UCI state
// ------------------------------------------------------------
static Board board;
static MoveGenerator generator;

static int numThreads = 6;
static bool useNNUE = false;

// Book-only mode:
// true  = engine uses opening book only, search skipped
// false = normal search
static bool useOpeningBook = true;

static std::unique_ptr<ThreadPool> pool;
static std::thread searchResultThread;
static std::atomic<bool> searching{false};

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static bool parseBool(const std::string& value)
{
    return value == "true" || value == "1" || value == "on";
}

static std::string squareToString(int square)
{
    int file = square % 8;
    int rank = square / 8;

    std::string s;
    s += static_cast<char>('a' + file);
    s += static_cast<char>('1' + rank);

    return s;
}

static std::string uciMoveToString(const Move& move)
{
    if (move.from < 0 || move.to < 0)
        return "0000";

    std::string s =
        squareToString(move.from) +
        squareToString(move.to);

    if (move.promotion != NONE)
    {
        switch (move.promotion)
        {
            case QUEEN:
                s += "q";
                break;

            case ROOK:
                s += "r";
                break;

            case BISHOP:
                s += "b";
                break;

            case KNIGHT:
                s += "n";
                break;

            default:
                break;
        }
    }

    return s;
}

static Move parseMoveString(
    Board& board,
    MoveGenerator& generator,
    const std::string& moveStr
)
{
    Move invalid{-1, -1, NONE, NONE, NONE, FLAG_NONE};

    if (moveStr.length() < 4)
        return invalid;

    int file1 = moveStr[0] - 'a';
    int rank1 = moveStr[1] - '1';
    int file2 = moveStr[2] - 'a';
    int rank2 = moveStr[3] - '1';

    if (file1 < 0 || file1 > 7 || rank1 < 0 || rank1 > 7)
        return invalid;

    if (file2 < 0 || file2 > 7 || rank2 < 0 || rank2 > 7)
        return invalid;

    int from = rank1 * 8 + file1;
    int to = rank2 * 8 + file2;

    int promotion = NONE;

    if (moveStr.length() == 5)
    {
        switch (moveStr[4])
        {
            case 'q':
                promotion = QUEEN;
                break;

            case 'r':
                promotion = ROOK;
                break;

            case 'b':
                promotion = BISHOP;
                break;

            case 'n':
                promotion = KNIGHT;
                break;

            default:
                return invalid;
        }
    }

    std::vector<Move> legalMoves =
        generator.generateLegalMoves(board);

    for (const Move& m : legalMoves)
    {
        if (
            m.from == from &&
            m.to == to &&
            m.promotion == promotion
        )
        {
            return m;
        }
    }

    return invalid;
}

// ------------------------------------------------------------
// Search control
// ------------------------------------------------------------
static void stopCurrentSearch()
{
    requestSearchStop();

    if (searchResultThread.joinable())
        searchResultThread.join();

    pool.reset();
    searching.store(false);
}

static void startSearchAsync(
    const Board& searchBoard,
    int depth,
    long long timeLimitMs
)
{
    stopCurrentSearch();

    searching.store(true);

    Board localBoard = searchBoard;

    pool = std::make_unique<ThreadPool>(numThreads);
    pool->startSearch(localBoard, depth, timeLimitMs);

    ThreadPool* rawPool = pool.get();

    searchResultThread = std::thread(
        [rawPool]()
        {
            Move best = rawPool->getBestMove();

            std::cout
                << "bestmove "
                << uciMoveToString(best)
                << std::endl;

            searching.store(false);
        }
    );
}

// ------------------------------------------------------------
// Main UCI loop
// ------------------------------------------------------------
void uciLoop()
{
    std::string line;

    while (std::getline(std::cin, line))
    {
        std::istringstream iss(line);
        std::string token;

        if (!(iss >> token))
            continue;

        // ----------------------------------------------------
        // uci
        // ----------------------------------------------------
        if (token == "uci")
        {
            std::cout << "id name Trishul\n";
            std::cout << "id author Monank Gohil\n";

            std::cout
                << "option name Hash type spin default 64 min 1 max 4096\n";

            std::cout
                << "option name Threads type spin default 6 min 1 max 256\n";

            std::cout
                << "option name UseBook type check default true\n";

            std::cout
                << "option name UseNNUE type check default false\n";

            std::cout
                << "option name Clear Hash type button\n";

            std::cout << "uciok" << std::endl;
        }

        // ----------------------------------------------------
        // isready
        // ----------------------------------------------------
        else if (token == "isready")
        {
            std::cout << "readyok" << std::endl;
        }

        // ----------------------------------------------------
        // ucinewgame
        // ----------------------------------------------------
        else if (token == "ucinewgame")
        {
            stopCurrentSearch();
            board.reset();
            ttClear();
        }

        // ----------------------------------------------------
        // setoption
        // ----------------------------------------------------
        else if (token == "setoption")
        {
            std::vector<std::string> tokens;
            std::string t;

            while (iss >> t)
                tokens.push_back(t);

            std::string name;
            std::string value;

            bool inName = false;
            bool inValue = false;

            for (const std::string& part : tokens)
            {
                if (part == "name")
                {
                    inName = true;
                    inValue = false;
                    continue;
                }

                if (part == "value")
                {
                    inName = false;
                    inValue = true;
                    continue;
                }

                if (inName)
                {
                    if (!name.empty())
                        name += " ";

                    name += part;
                }
                else if (inValue)
                {
                    if (!value.empty())
                        value += " ";

                    value += part;
                }
            }

            if (name == "Threads")
            {
                stopCurrentSearch();

                try
                {
                    numThreads = std::stoi(value);
                }
                catch (...)
                {
                    numThreads = 1;
                }

                if (numThreads < 1)
                    numThreads = 1;

                if (numThreads > MAX_THREADS)
                    numThreads = MAX_THREADS;
            }
            else if (name == "Hash")
            {
                stopCurrentSearch();

                int hashMB = 64;

                try
                {
                    hashMB = std::stoi(value);
                }
                catch (...)
                {
                    hashMB = 64;
                }

                if (hashMB < 1)
                    hashMB = 1;

                if (hashMB > 4096)
                    hashMB = 4096;

                ttInit(hashMB);
            }
            else if (name == "UseBook")
            {
                useOpeningBook = parseBool(value);
                setOpeningBookEnabled(useOpeningBook);
            }
            else if (name == "UseNNUE")
            {
                useNNUE = parseBool(value);
            }
            else if (name == "Clear Hash")
            {
                stopCurrentSearch();
                ttClear();
            }
        }

        // ----------------------------------------------------
        // position
        // ----------------------------------------------------
        else if (token == "position")
        {
            stopCurrentSearch();

            std::string sub;
            iss >> sub;

            if (sub == "startpos")
            {
                board.reset();

                std::string maybeMoves;

                if (iss >> maybeMoves && maybeMoves == "moves")
                {
                    std::string moveStr;

                    while (iss >> moveStr)
                    {
                        Move m = parseMoveString(
                            board,
                            generator,
                            moveStr
                        );

                        if (m.from == -1)
                            break;

                        board.makeMove(
                            m.from,
                            m.to,
                            m.promotion
                        );
                    }
                }
            }
            else if (sub == "fen")
            {
                std::vector<std::string> fenTokens;
                std::string tok;
                bool movesFound = false;

                while (iss >> tok)
                {
                    if (tok == "moves")
                    {
                        movesFound = true;
                        break;
                    }

                    fenTokens.push_back(tok);
                }

                std::string fen;

                for (size_t i = 0; i < fenTokens.size(); i++)
                {
                    if (i > 0)
                        fen += " ";

                    fen += fenTokens[i];
                }

                board.loadFEN(fen);

                if (movesFound)
                {
                    std::string moveStr;

                    while (iss >> moveStr)
                    {
                        Move m = parseMoveString(
                            board,
                            generator,
                            moveStr
                        );

                        if (m.from == -1)
                            break;

                        board.makeMove(
                            m.from,
                            m.to,
                            m.promotion
                        );
                    }
                }
            }
        }

        // ----------------------------------------------------
        // go
        // ----------------------------------------------------
        else if (token == "go")
        {
            int depth = 64;
            bool depthGiven = false;

            long long movetime = -1;
            long long wtime = -1;
            long long btime = -1;
            long long winc = 0;
            long long binc = 0;

            int movestogo = -1;
            bool infinite = false;

            std::string sub;

            while (iss >> sub)
            {
                if (sub == "depth")
                {
                    iss >> depth;
                    depthGiven = true;
                }
                else if (sub == "movetime")
                {
                    iss >> movetime;
                }
                else if (sub == "wtime")
                {
                    iss >> wtime;
                }
                else if (sub == "btime")
                {
                    iss >> btime;
                }
                else if (sub == "winc")
                {
                    iss >> winc;
                }
                else if (sub == "binc")
                {
                    iss >> binc;
                }
                else if (sub == "movestogo")
                {
                    iss >> movestogo;
                }
                else if (sub == "infinite")
                {
                    infinite = true;
                }
                else if (sub == "ponder")
                {
                    // Ponder support can be added later.
                }
            }

            if (depth < 1)
                depth = 1;

            // ------------------------------------------------------------
            // Opening book:
            // If position is in book, return book move immediately.
            // Search and NNUE are skipped only for book positions.
            // ------------------------------------------------------------
            if (useOpeningBook)
            {
                Move bookMove = getBookMove(board);

                if (bookMove.from != -1)
                {
                    // Safety: validate book move against current legal moves.
                    std::vector<Move> legalMoves =
                        generator.generateLegalMoves(board);

                    Move validatedBookMove{-1, -1, NONE, NONE, NONE, FLAG_NONE};

                    for (const Move& m : legalMoves)
                    {
                        if (
                            m.from == bookMove.from &&
                            m.to == bookMove.to &&
                            m.promotion == bookMove.promotion
                        )
                        {
                            validatedBookMove = m;
                            break;
                        }
                    }

                    if (validatedBookMove.from != -1)
                    {
                        stopCurrentSearch();

                        std::cout
                            << "info string Opening book move"
                            << std::endl;

                        std::cout
                            << "bestmove "
                            << uciMoveToString(validatedBookMove)
                            << std::endl;

                        continue;
                    }
                }
            }

            // ------------------------------------------------
            // Time management
            // ------------------------------------------------
            long long timeLimitMs = -1;

            if (movetime >= 0)
            {
                timeLimitMs = movetime - 50;

                if (timeLimitMs < 10)
                    timeLimitMs = 10;
            }
            else if (wtime >= 0 && btime >= 0)
            {
                long long myTime =
                    board.isWhiteToMove() ? wtime : btime;

                long long myInc =
                    board.isWhiteToMove() ? winc : binc;

                int assumedMovesLeft =
                    (movestogo > 0) ? movestogo : 30;

                timeLimitMs =
                    (myTime / assumedMovesLeft) + (myInc / 2);

                long long maxAllowed = myTime / 2;

                if (timeLimitMs > maxAllowed)
                    timeLimitMs = maxAllowed;

                timeLimitMs -= 50;

                if (timeLimitMs < 10)
                    timeLimitMs = 10;
            }

            if (infinite)
            {
                depth = 64;
                timeLimitMs = -1;
            }
            else if (!depthGiven && timeLimitMs < 0)
            {
                depth = 64;
            }

            startSearchAsync(board, depth, timeLimitMs);
        }

        // ----------------------------------------------------
        // stop
        // ----------------------------------------------------
        else if (token == "stop")
        {
            requestSearchStop();
        }

        // ----------------------------------------------------
        // ponderhit
        // ----------------------------------------------------
        else if (token == "ponderhit")
        {
            // Ponder support can be added later.
        }

        // ----------------------------------------------------
        // quit
        // ----------------------------------------------------
        else if (token == "quit")
        {
            stopCurrentSearch();
            break;
        }
    }

    stopCurrentSearch();
}