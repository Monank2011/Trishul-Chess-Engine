#include "uci.h"
#include "board.h"
#include "movegen.h"
#include "search.h"
#include "move.h"
#include "threads.h"
#include "tt.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// --------------------------------------------------------
// UCI Options
// --------------------------------------------------------

int numThreads = 6;  // Default to 6 threads

// --------------------------------------------------------
// Square <-> string conversion
// --------------------------------------------------------

int squareFromString(const std::string& s)
{
    int file = s[0] - 'a';
    int rank = s[1] - '1';
    return rank * 8 + file;
}

std::string squareToString(int square)
{
    int file = square % 8;
    int rank = square / 8;

    std::string s;
    s += ('a' + file);
    s += ('1' + rank);
    return s;
}

// --------------------------------------------------------
// Parse a UCI move string (e.g. "e2e4", "e7e8q") into a legal Move
// --------------------------------------------------------

Move parseMoveString(Board& board, MoveGenerator& generator, const std::string& moveStr)
{
    int from = squareFromString(moveStr.substr(0, 2));
    int to   = squareFromString(moveStr.substr(2, 2));

    int promotion = NONE;
    if (moveStr.length() == 5)
    {
        char promoChar = moveStr[4];
        switch (promoChar)
        {
            case 'q': promotion = QUEEN;  break;
            case 'r': promotion = ROOK;   break;
            case 'b': promotion = BISHOP; break;
            case 'n': promotion = KNIGHT; break;
        }
    }

    std::vector<Move> legalMoves = generator.generateLegalMoves(board);

    for (const Move& m : legalMoves)
    {
        if (m.from == from && m.to == to && m.promotion == promotion)
        {
            return m;
        }
    }

    // Fallback (shouldn't happen if GUI sends legal moves) - return a dummy move
    Move invalid;
    invalid.from = from;
    invalid.to = to;
    invalid.piece = NONE;
    invalid.captured = NONE;
    invalid.promotion = promotion;
    invalid.flags = FLAG_NONE;
    return invalid;
}

// --------------------------------------------------------
// Main UCI loop
// --------------------------------------------------------

void uciLoop()
{
    Board board;
    MoveGenerator generator;

    std::string line;

    while (std::getline(std::cin, line))
    {
        std::istringstream iss(line);
        std::string token;
        iss >> token;

        if (token == "uci")
        {
            std::cout << "id name Trishul\n";
            std::cout << "id author Monank Gohil\n";
            std::cout << "option name Hash type spin default 64 min 1 max 4096\n";
            std::cout << "option name Threads type spin default 6 min 1 max 256\n";
            std::cout << "uciok\n";
        }
        else if (token == "isready")
        {
            std::cout << "readyok\n";
        }
                else if (token == "ucinewgame")
        {
            board.reset();
            ttClear();
        }
        else if (token == "setoption")
        {
            std::string name, value;
            std::string sub;
            
            while (iss >> sub)
            {
                if (sub == "name")
                {
                    iss >> name;
                }
                else if (sub == "value")
                {
                    iss >> value;
                    
                    if (name == "Threads")
                    {
                        numThreads = std::stoi(value);
                        if (numThreads < 1)
                            numThreads = 1;
                        if (numThreads > 256)
                            numThreads = 256;
                    }
                    else if (name == "Hash")
                    {
                        int hashMB = std::stoi(value);

                        if (hashMB < 1)
                            hashMB = 1;
                        if (hashMB > 4096)
                            hashMB = 4096;

                        ttInit(hashMB);
                    }
                }
            }
        }
        else if (token == "position")
        {
            std::string sub;
            iss >> sub;

            if (sub == "startpos")
            {
                board.reset();

                std::string movesToken;
                iss >> movesToken; // consume "moves" if present

                std::string moveStr;
                while (iss >> moveStr)
                {
                    Move m = parseMoveString(board, generator, moveStr);
                    board.makeMove(m.from, m.to, m.promotion);
                }
            }
            else if (sub == "fen")
            {
                std::string fenFields[6];

                for (int i = 0; i < 6; i++)
                    iss >> fenFields[i];

                std::string fen =
                    fenFields[0] + " " + fenFields[1] + " " +
                    fenFields[2] + " " + fenFields[3] + " " +
                    fenFields[4] + " " + fenFields[5];

                board.loadFEN(fen);

                std::string movesToken;
                iss >> movesToken; // consume "moves" if present

                std::string moveStr;
                while (iss >> moveStr)
                {
                    Move m = parseMoveString(board, generator, moveStr);
                    board.makeMove(m.from, m.to, m.promotion);
                }
            }
        }
        else if (token == "go")
        {
            int depth = 64; // effectively "as deep as time allows"

            bool depthGiven = false;

            long long movetime = -1;
            long long wtime = -1;
            long long btime = -1;
            long long winc = 0;
            long long binc = 0;
            int movestogo = -1;

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
            }

            long long timeLimitMs = -1;

            if (movetime >= 0)
            {
                timeLimitMs = movetime - 50; // small safety buffer

                if (timeLimitMs < 10)
                    timeLimitMs = 10;
            }
            else if (wtime >= 0 && btime >= 0)
            {
                long long myTime  = board.isWhiteToMove() ? wtime : btime;
                long long myInc   = board.isWhiteToMove() ? winc  : binc;

                int assumedMovesLeft =
                    (movestogo > 0) ? movestogo : 30;

                timeLimitMs = (myTime / assumedMovesLeft) + (myInc / 2);

                // Never spend more than half of what's left,
                // no matter what the formula above says.
                long long maxAllowed = myTime / 2;

                if (timeLimitMs > maxAllowed)
                    timeLimitMs = maxAllowed;

                timeLimitMs -= 50; // communication/safety buffer

                if (timeLimitMs < 10)
                    timeLimitMs = 10;
            }
            // else: no time info at all - fall back to fixed depth
            // (timeLimitMs stays -1, meaning "unlimited")

            if (depthGiven && timeLimitMs < 0)
            {
                // "go depth N" with no clock info - keep the
                // old fixed-depth behavior exactly as before.
            }
            else if (!depthGiven)
            {
                depth = 64; // let the clock be the real limit
            }

            ThreadPool pool(numThreads);

            pool.startSearch(board, depth, timeLimitMs);

            Move best = pool.getBestMove();

            std::cout << "bestmove " << squareToString(best.from) << squareToString(best.to);

            if (best.promotion != NONE)
            {
                switch (best.promotion)
                {
                    case QUEEN:  std::cout << "q"; break;
                    case ROOK:   std::cout << "r"; break;
                    case BISHOP: std::cout << "b"; break;
                    case KNIGHT: std::cout << "n"; break;
                }
            }

            std::cout << "\n";
        }
        else if (token == "quit")
        {
            break;
        }
    }
}