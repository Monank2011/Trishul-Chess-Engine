#include "board.h"
#include "attacks.h"
#include "magic.h"
#include "movegen.h"
#include "search.h"
#include "uci.h"
#include "zobrist.h"
#include "tt.h"
#include "move.h"

#include <iostream>
#include <cstdint>
#include <string>
#include <vector>

uint64_t perft(Board& board, MoveGenerator& generator, int depth)
{
    if (depth == 0)
        return 1ULL;

    std::vector<Move> moves = generator.generateLegalMoves(board);

    uint64_t nodes = 0;

    for (const Move& move : moves)
    {
        UndoInfo undo = board.makeMove(
            move.from,
            move.to,
            move.promotion
        );

        nodes += perft(board, generator, depth - 1);

        board.unmakeMove(move.from, move.to, undo);
    }

    return nodes;
}


// ------------------------------------------------------------
// Convert a square number to UCI notation
// ------------------------------------------------------------

std::string squareToStringMain(int square)
{
    int file = square % 8;
    int rank = square / 8;

    std::string result;

    result += char('a' + file);
    result += char('1' + rank);

    return result;
}


// ------------------------------------------------------------
// Convert UCI square notation to square number
// ------------------------------------------------------------

int squareFromStringMain(const std::string& s)
{
    if (s.length() < 2)
        return -1;

    int file = s[0] - 'a';
    int rank = s[1] - '1';

    if (file < 0 || file > 7 || rank < 0 || rank > 7)
        return -1;

    return rank * 8 + file;
}


// ------------------------------------------------------------
// Convert a Move to UCI notation
// ------------------------------------------------------------

std::string moveToStringMain(const Move& move)
{
    if (move.from < 0 || move.to < 0)
        return "0000";

    std::string result;

    result += squareToStringMain(move.from);
    result += squareToStringMain(move.to);

    if (move.promotion != NONE)
    {
        switch (move.promotion)
        {
            case QUEEN:
                result += 'q';
                break;

            case ROOK:
                result += 'r';
                break;

            case BISHOP:
                result += 'b';
                break;

            case KNIGHT:
                result += 'n';
                break;
        }
    }

    return result;
}


// ------------------------------------------------------------
// Parse a user's move
// ------------------------------------------------------------

Move parsePlayerMove(
    Board& board,
    MoveGenerator& generator,
    const std::string& input
)
{
    Move invalid{
        -1,
        -1,
        NONE,
        NONE,
        NONE,
        FLAG_NONE
    };

    if (input.length() < 4)
        return invalid;

    int from = squareFromStringMain(input.substr(0, 2));
    int to   = squareFromStringMain(input.substr(2, 2));

    if (from == -1 || to == -1)
        return invalid;

    int promotion = NONE;

    if (input.length() >= 5)
    {
        switch (input[4])
        {
            case 'q':
            case 'Q':
                promotion = QUEEN;
                break;

            case 'r':
            case 'R':
                promotion = ROOK;
                break;

            case 'b':
            case 'B':
                promotion = BISHOP;
                break;

            case 'n':
            case 'N':
                promotion = KNIGHT;
                break;

            default:
                return invalid;
        }
    }

    std::vector<Move> legalMoves =
        generator.generateLegalMoves(board);

    for (const Move& move : legalMoves)
    {
        if (move.from == from &&
            move.to == to &&
            move.promotion == promotion)
        {
            return move;
        }
    }

    return invalid;
}


// ------------------------------------------------------------
// Play against Trishul
// ------------------------------------------------------------

void playAgainstTrishul(int depth)
{
    Board board;
    MoveGenerator generator;

    std::cout << "\n=====================================\n";
    std::cout << "        PLAY AGAINST TRISHUL\n";
    std::cout << "=====================================\n";

    std::cout << "Choose your side:\n";
    std::cout << "1. White\n";
    std::cout << "2. Black\n";
    std::cout << "> ";

    int side;
    std::cin >> side;

    bool playerIsWhite = (side == 1);

    board.reset();

    while (true)
    {
        board.printBoard();

        std::vector<Move> legalMoves =
            generator.generateLegalMoves(board);

        if (legalMoves.empty())
        {
            bool sideToMoveIsWhite = board.isWhiteToMove();

            uint64_t king =
                sideToMoveIsWhite
                    ? board.getWhiteKing()
                    : board.getBlackKing();

            bool inCheck = false;

            if (king)
            {
                int kingSquare = __builtin_ctzll(king);

                inCheck = isSquareAttacked(
                    board,
                    kingSquare,
                    !sideToMoveIsWhite
                );
            }

            if (inCheck)
            {
                if (sideToMoveIsWhite == playerIsWhite)
                    std::cout << "\nCheckmate! Trishul wins.\n";
                else
                    std::cout << "\nCheckmate! You win!\n";
            }
            else
            {
                std::cout << "\nStalemate! Draw.\n";
            }

            break;
        }

        bool playerTurn =
            board.isWhiteToMove() == playerIsWhite;

        if (playerTurn)
        {
            std::cout << "\nYour move ";

            std::string input;
            std::cin >> input;

            if (input == "quit")
                break;

            Move playerMove =
                parsePlayerMove(
                    board,
                    generator,
                    input
                );

            if (playerMove.from == -1)
            {
                std::cout << "Illegal move. Try again.\n";
                continue;
            }

            board.makeMove(
                playerMove.from,
                playerMove.to,
                playerMove.promotion
            );
        }
        else
        {
            std::cout << "\nTrishul is thinking at depth "
                      << depth << "...\n";

            Move bestMove =
                findBestMove(board, depth);

            std::cout << "Trishul plays: "
                      << moveToStringMain(bestMove)
                      << "\n";

            board.makeMove(
                bestMove.from,
                bestMove.to,
                bestMove.promotion
            );
        }
    }
}


// ------------------------------------------------------------
// Analyze a position
// ------------------------------------------------------------

void analyzePosition(int depth)
{
    Board board;
    MoveGenerator generator;

    board.reset();

    std::cout << "\n=====================================\n";
    std::cout << "         TRISHUL POSITION TEST\n";
    std::cout << "=====================================\n";

    board.printBoard();

    std::cout << "\nEnter moves from the starting position.\n";
    std::cout << "Example: e2e4 e7e5 g1f3\n";
    std::cout << "Type 'done' when finished.\n";
    std::cout << "> ";

    std::string input;

    while (std::cin >> input)
    {
        if (input == "done")
            break;

        Move move =
            parsePlayerMove(
                board,
                generator,
                input
            );

        if (move.from == -1)
        {
            std::cout << "Illegal move: "
                      << input << "\n";
            continue;
        }

        board.makeMove(
            move.from,
            move.to,
            move.promotion
        );
    }

    std::cout << "\nPosition to analyze:\n";
    board.printBoard();

    std::cout << "\nSearching at depth "
              << depth << "...\n";

    Move bestMove =
        findBestMove(board, depth);

    std::cout << "\nBest move: "
              << moveToStringMain(bestMove)
              << "\n";
}


// ------------------------------------------------------------
// Main interactive menu
// ------------------------------------------------------------

int main()
{
    // --------------------------------------------------------
    // Engine initialization
    // --------------------------------------------------------

    initKnightAttacks();
    initMagicBitboards();
    initKingAttacks();
    initPawnAttacks();
    initZobrist();

    ttInit(64);

    initKillerMoves();
    initHistoryTable();

    // --------------------------------------------------------
    // Persistent search depth
    // --------------------------------------------------------

    int searchDepth;

    std::cout << "\n=====================================\n";
    std::cout << "          TRISHUL CHESS ENGINE\n";
    std::cout << "=====================================\n";

    std::cout << "\nEnter search depth: ";
    std::cin >> searchDepth;

    if (searchDepth < 1)
        searchDepth = 1;

    while (true)
    {
        std::cout << "\n\n";
        std::cout << "=====================================\n";
        std::cout << "              TRISHUL\n";
        std::cout << "=====================================\n";

        std::cout << "Current search depth: "
                  << searchDepth << "\n\n";

        std::cout << "1. Play against Trishul\n";
        std::cout << "2. Analyze a position\n";
        std::cout << "3. UCI mode\n";
        std::cout << "4. Change search depth\n";
        std::cout << "5. Perft test\n";
        std::cout << "6. Exit\n";

        std::cout << "\nChoose option: ";

        int option;
        std::cin >> option;

        if (option == 1)
        {
            playAgainstTrishul(searchDepth);
        }
        else if (option == 2)
        {
            analyzePosition(searchDepth);
        }
        else if (option == 3)
        {
            std::cout << "\nEntering UCI mode.\n";
            std::cout << "Type 'quit' to return.\n\n";

            uciLoop();

            std::cout << "\nReturned from UCI mode.\n";
        }
        else if (option == 4)
        {
            std::cout << "\nCurrent depth: "
                      << searchDepth << "\n";

            std::cout << "Enter new search depth: ";

            std::cin >> searchDepth;

            if (searchDepth < 1)
                searchDepth = 1;

            std::cout << "Search depth changed to "
                      << searchDepth << ".\n";
        }
        else if (option == 5)
        {
            int perftDepth;

            std::cout << "\nEnter perft depth: ";
            std::cin >> perftDepth;

            Board board;
            MoveGenerator generator;

            std::cout << "Running perft depth "
                      << perftDepth << "...\n";

            uint64_t nodes =
                perft(board, generator, perftDepth);

            std::cout << "Nodes: "
                      << nodes << "\n";
        }
        else if (option == 6)
        {
            std::cout << "\nTrishul shutting down.\n";
            break;
        }
        else
        {
            std::cout << "\nInvalid option.\n";
        }
    }

    return 0;
}