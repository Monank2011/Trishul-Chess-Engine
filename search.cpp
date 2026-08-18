#include "search.h"
#include "movegen.h"
#include "eval.h"
#include "attacks.h"
#include <climits>
#include <iostream>
#include "tt.h"
#include <algorithm>
#include "openingbook.h"
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>

const int MAX_QUIESCENCE_DEPTH = 32;

Move killerMoves[MAX_THREADS][MAX_PLY][2];

void initKillerMoves()
{
    // Legacy single-thread init
    for (int ply = 0; ply < MAX_PLY; ply++)
    {
        killerMoves[0][ply][0] =
            Move{-1, -1, NONE, NONE, NONE, FLAG_NONE};

        killerMoves[0][ply][1] =
            Move{-1, -1, NONE, NONE, NONE, FLAG_NONE};
    }
}

void initKillerMovesThreaded(int numThreads)
{
    for (int threadId = 0; threadId < numThreads; threadId++)
    {
        for (int ply = 0; ply < MAX_PLY; ply++)
        {
            killerMoves[threadId][ply][0] =
                Move{-1, -1, NONE, NONE, NONE, FLAG_NONE};

            killerMoves[threadId][ply][1] =
                Move{-1, -1, NONE, NONE, NONE, FLAG_NONE};
        }
    }
}


// [threadId][color][from][to]
int historyTable[MAX_THREADS][2][64][64];

void initHistoryTable()
{
    // Legacy single-thread init
    for (int color = 0; color < 2; color++)
        for (int from = 0; from < 64; from++)
            for (int to = 0; to < 64; to++)
                historyTable[0][color][from][to] = 0;
}

void initHistoryTableThreaded(int numThreads)
{
    for (int threadId = 0; threadId < numThreads; threadId++)
    {
        for (int color = 0; color < 2; color++)
            for (int from = 0; from < 64; from++)
                for (int to = 0; to < 64; to++)
                    historyTable[threadId][color][from][to] = 0;
    }
}

// ------------------------------------------------------------
// Time control
// ------------------------------------------------------------

using Clock = std::chrono::steady_clock;

std::atomic<bool> stopSearch{false};

bool timeControlEnabled = false;

Clock::time_point searchDeadline;

std::atomic<uint64_t> nodeCounter{0};


// Returns true if the search should stop right now.
// Cheap on almost every call - only actually queries the
// clock once every 2048 nodes. Safe to call from multiple
// threads: stopSearch/nodeCounter are atomic.
bool checkTime()
{
    if (!timeControlEnabled)
        return false;

    if (stopSearch.load(std::memory_order_relaxed))
        return true;

    uint64_t n = nodeCounter.fetch_add(1, std::memory_order_relaxed);

    if ((n & 2047ULL) == 0)
    {
        if (Clock::now() >= searchDeadline)
            stopSearch.store(true, std::memory_order_relaxed);
    }

    return stopSearch.load(std::memory_order_relaxed);
}


// ------------------------------------------------------------
// Shared search-timing setup, called ONCE (not per-thread)
// before any search threads are spawned.
// ------------------------------------------------------------

void beginSearchTiming(long long timeLimitMs)
{
    stopSearch.store(false, std::memory_order_relaxed);
    nodeCounter.store(0, std::memory_order_relaxed);

    if (timeLimitMs >= 0)
    {
        timeControlEnabled = true;

        searchDeadline =
            Clock::now() +
            std::chrono::milliseconds(timeLimitMs);
    }
    else
    {
        timeControlEnabled = false;
    }
}


void requestSearchStop()
{
    stopSearch.store(true, std::memory_order_relaxed);
}


// ------------------------------------------------------------
// Piece values
// ------------------------------------------------------------

int getPieceValue(int pieceType)
{
    switch (pieceType)
    {
        case PAWN:   return 100;
        case KNIGHT: return 320;
        case BISHOP: return 330;
        case ROOK:   return 500;
        case QUEEN:  return 900;
        case KING:   return 10000;
        default:     return 0;
    }
}


// ------------------------------------------------------------
// Move ordering
// ------------------------------------------------------------

int scoreMoveForOrdering(const Move& move, int color, int threadId = 0)
{
    if (move.captured != NONE)
    {
        // Big flat bonus so captures always sort above
        // history-scored quiet moves, no matter how "hot"
        // a quiet move's history score has gotten.
        return 1000000
             + getPieceValue(move.captured) * 10
             - getPieceValue(move.piece);
    }

    return historyTable[threadId][color][move.from][move.to];
}


void orderMoves(std::vector<Move>& moves, int color, int threadId = 0)
{
    std::sort(
        moves.begin(),
        moves.end(),
        [color](const Move& a, const Move& b)
        {
            return scoreMoveForOrdering(a, color)
                 > scoreMoveForOrdering(b, color);
        }
    );
}


// ------------------------------------------------------------
// Quiescence Search
// ------------------------------------------------------------

int quiescence(
    Board& board,
    MoveGenerator& generator,
    int alpha,
    int beta,
    int qDepth = 0,
    int threadId = 0
)
{
    if (checkTime())
        return 0;

    // Safety net - delta pruning should make this essentially
    // unreachable, but this guarantees quiescence can never
    // run away on some pathological line we haven't thought of.
    if (qDepth >= MAX_QUIESCENCE_DEPTH)
        return evaluate(board);

    int standPat = evaluate(board);

    if (standPat >= beta)
        return beta;

    if (standPat > alpha)
        alpha = standPat;

    std::vector<Move> captures =
        generator.generateLegalCaptures(board);

    orderMoves(captures, board.isWhiteToMove() ? 0 : 1, threadId);

    // Big margin so we never wrongly prune a capture that
    // could matter - this only cuts moves that are truly
    // hopeless even in the most generous case.
    const int DELTA_MARGIN = 200;

    for (const Move& move : captures)
    {
        // ------------------------------------------------
        // Delta pruning
        // ------------------------------------------------
        //
        // Even if this capture wins the maximum possible
        // material (the captured piece, plus a queen's
        // worth if it's a promoting capture), it still
        // can't reach alpha - so it's hopeless. Skip it
        // without recursing.

        if (move.captured != NONE)
        {
            int bestCase =
                standPat +
                getPieceValue(move.captured) +
                DELTA_MARGIN;

            if (move.promotion != NONE)
            {
                bestCase += getPieceValue(QUEEN) - getPieceValue(PAWN);
            }

            if (bestCase <= alpha)
            {
                continue;
            }
        }


        UndoInfo undo =
            board.makeMove(
                move.from,
                move.to,
                move.promotion
            );

        int score =
            -quiescence(
                board,
                generator,
                -beta,
                -alpha,
                qDepth + 1,
                threadId
            );

        board.unmakeMove(
            move.from,
            move.to,
            undo
        );

        if (score >= beta)
            return beta;

        if (score > alpha)
            alpha = score;
    }

    return alpha;
}


// ------------------------------------------------------------
// Negamax
// ------------------------------------------------------------

int negamax(
    Board& board,
    MoveGenerator& generator,
    int depth,
    int alpha,
    int beta,
    int ply,
    bool allowNullMove,
    int threadId = 0
)
{
    // --------------------------------------------------------
    // Time check
    // --------------------------------------------------------

    if (checkTime())
        return 0;


    // --------------------------------------------------------
    // Leaf node
    // --------------------------------------------------------

    if (depth == 0)
        return quiescence(
            board,
            generator,
            alpha,
            beta,
            0,
            threadId
        );


    // --------------------------------------------------------
    // Draw detection: 50-move rule and repetition
    // --------------------------------------------------------

    if (board.getHalfmoveClock() >= 100 ||
        board.isRepetition())
    {
        return 0;
    }


    // --------------------------------------------------------
    // Check detection
    // --------------------------------------------------------

    bool inCheck = isSquareAttacked(
        board,
        __builtin_ctzll(
            board.isWhiteToMove()
                ? board.getWhiteKing()
                : board.getBlackKing()
        ),
        !board.isWhiteToMove()
    );

    // --------------------------------------------------------
    // Static evaluation
    // --------------------------------------------------------

    int staticEval = evaluate(board);

// --------------------------------------------------------
// Razoring
// --------------------------------------------------------
//
// At shallow depths, if the static evaluation is far
// below alpha, the position is unlikely to improve enough
// through a normal search.
//
// We therefore jump directly to quiescence. If even the
// tactical position cannot reach alpha, there is no need
// to search the full move tree.
//
// Conservative first implementation:
//   - depth <= 2
//   - never when in check
//   - static evaluation must be substantially below alpha
//

if (!inCheck &&
    depth <= 2)
{
    int razorMargin =
        250 + 150 * depth;

    if (staticEval + razorMargin <= alpha)
    {
        int razorScore =
            quiescence(
                board,
                generator,
                alpha,
                beta,
                0,
                threadId
            );

        if (razorScore <= alpha)
            return razorScore;
    }
}

    // --------------------------------------------------------
    // Null Move Pruning
    // --------------------------------------------------------

    if (allowNullMove &&
        depth >= 3 &&
        !inCheck)
    {
        int reduction = 2 + depth / 4;

        board.makeNullMove();

        int nullScore =
            -negamax(
                board,
                generator,
                depth - 1 - reduction,
                -beta,
                -beta + 1,
                ply + 1,
                false,
                threadId
            );

        board.unmakeNullMove();

        if (nullScore >= beta)
            return beta;
    }


    // --------------------------------------------------------
    // Transposition Table
    // --------------------------------------------------------

    uint64_t hash = board.getZobristHash();

    int ttScore;

    Move ttMove;

    ttMove.from = -1;

    if (ttProbe(
            hash,
            depth,
            alpha,
            beta,
            ttScore,
            ttMove))
    {
        return ttScore;
    }

    // --------------------------------------------------------
    // Generate legal moves
    // --------------------------------------------------------

    std::vector<Move> moves =
        generator.generateLegalMoves(board);


    // --------------------------------------------------------
    // Checkmate / stalemate
    // --------------------------------------------------------

    if (moves.empty())
    {
        if (inCheck)
        {
            return -1000000 + (10 - depth);
        }
        else
        {
            return 0;
        }
    }


    // --------------------------------------------------------
    // Move ordering
    // --------------------------------------------------------

    orderMoves(moves, board.isWhiteToMove() ? 0 : 1, threadId);


    // Killer moves
    if (ply < MAX_PLY)
    {
        for (int i = 1; i >= 0; i--)
        {
            if (killerMoves[threadId][ply][i].from == -1)
                continue;

            for (size_t j = 0; j < moves.size(); j++)
            {
                if (
                    moves[j].from ==
                        killerMoves[threadId][ply][i].from &&

                    moves[j].to ==
                        killerMoves[threadId][ply][i].to &&

                    moves[j].promotion ==
                        killerMoves[threadId][ply][i].promotion
                )
                {
                    std::swap(moves[0], moves[j]);
                    break;
                }
            }
        }
    }


    // TT move
    if (ttMove.from != -1)
    {
        for (size_t i = 0; i < moves.size(); i++)
        {
            if (
                moves[i].from == ttMove.from &&
                moves[i].to == ttMove.to &&
                moves[i].promotion == ttMove.promotion
            )
            {
                std::swap(moves[0], moves[i]);
                break;
            }
        }
    }


    // --------------------------------------------------------
    // Search moves
    // --------------------------------------------------------

    int bestScore = INT_MIN;

    Move bestMoveAtThisNode = moves[0];

    int originalAlpha = alpha;


    for (size_t moveIndex = 0;
         moveIndex < moves.size();
         moveIndex++)
    {
        const Move& move = moves[moveIndex];


        // ----------------------------------------------------
        // Determine whether this move is quiet
        // ----------------------------------------------------

        bool isQuiet =
            move.captured == NONE &&
            move.promotion == NONE &&
            move.flags == FLAG_NONE;


        // ----------------------------------------------------
        // Determine whether this is a killer move
        // ----------------------------------------------------

        bool isKiller = false;

        if (ply < MAX_PLY)
        {
            bool killer0 =
                killerMoves[threadId][ply][0].from == move.from &&
                killerMoves[threadId][ply][0].to == move.to &&
                killerMoves[threadId][ply][0].promotion == move.promotion;

            bool killer1 =
                killerMoves[threadId][ply][1].from == move.from &&
                killerMoves[threadId][ply][1].to == move.to &&
                killerMoves[threadId][ply][1].promotion == move.promotion;

            isKiller = killer0 || killer1;
        }


        // ----------------------------------------------------
        // Futility Pruning
        // ----------------------------------------------------
        //
        // Only prune later quiet moves at shallow depths.
        //
        // The first move is never pruned because it is the
        // principal candidate produced by move ordering.
        //
        // Killer moves are protected because they have already
        // proven useful as tactical quiet moves.
        //
        // Checking moves are also protected. Since a quiet move
        // can still give check, we make the move temporarily,
        // test the opponent king, and immediately unmake it
        // when the move is futile.
        //
        // This is intentionally conservative:
        //
        //   staticEval + margin <= alpha
        //
        // means even giving this move a reasonable positional
        // improvement is unlikely to raise the score above
        // alpha.
        //

        bool futilityCandidate =
            depth <= 3 &&
            moveIndex > 0 &&
            isQuiet &&
            !isKiller &&
            !inCheck;

        if (futilityCandidate)
        {
            int futilityMargin =
                100 + 120 * depth;

            if (staticEval + futilityMargin <= alpha)
            {
                UndoInfo futilityUndo =
                    board.makeMove(
                        move.from,
                        move.to,
                        move.promotion
                    );

                bool givesCheck =
                    isSquareAttacked(
                        board,
                        __builtin_ctzll(
                            board.isWhiteToMove()
                                ? board.getWhiteKing()
                                : board.getBlackKing()
                        ),
                        !board.isWhiteToMove()
                    );

                board.unmakeMove(
                    move.from,
                    move.to,
                    futilityUndo
                );

                if (!givesCheck)
                {
                    continue;
                }
            }
        }


        // ----------------------------------------------------
        // Make move
        // ----------------------------------------------------

        UndoInfo undo =
            board.makeMove(
                move.from,
                move.to,
                move.promotion
            );


        int score;


        // ----------------------------------------------------
        // PVS + Late Move Reduction
        // ----------------------------------------------------
        //
        // Move 0 gets a full-width search.
        //
        // Later moves are first searched with a null window.
        // Quiet late moves can receive LMR.
        //
        // If the reduced search beats alpha, it is verified
        // at full depth.
        //
        // If the scout search beats alpha but does not reach
        // beta, a full PVS re-search obtains the exact score.
        //

        if (moveIndex == 0)
        {
            // --------------------------------------------
            // First move - full window, full depth
            // --------------------------------------------

            score =
                -negamax(
                    board,
                    generator,
                    depth - 1,
                    -beta,
                    -alpha,
                    ply + 1,
                    true,
                    threadId
                );
        }
        else
        {
            // --------------------------------------------
            // Late Move Reduction
            // --------------------------------------------

            bool doLMR =
                depth >= 3 &&
                moveIndex >= 3 &&
                isQuiet &&
                !isKiller;

            int reduction = 0;

            if (doLMR)
            {
                reduction = 1;

                if (depth >= 6 &&
                    moveIndex >= 6)
                {
                    reduction = 2;
                }

                if (depth >= 10 &&
                    moveIndex >= 10)
                {
                    reduction = 3;
                }
            }

            int searchDepth =
                depth - 1 - reduction;

            if (searchDepth < 0)
                searchDepth = 0;


            // --------------------------------------------
            // Scout search
            // --------------------------------------------

            score =
                -negamax(
                    board,
                    generator,
                    searchDepth,
                    -alpha - 1,
                    -alpha,
                    ply + 1,
                    true,
                    threadId
                );


            // --------------------------------------------
            // Reduced move beat alpha:
            // re-verify at full depth.
            // --------------------------------------------

            if (reduction > 0 && score > alpha)
            {
                score =
                    -negamax(
                        board,
                        generator,
                        depth - 1,
                        -alpha - 1,
                        -alpha,
                        ply + 1,
                        true,
                        threadId
                    );
            }


            // --------------------------------------------
            // Scout search beat alpha:
            // full PVS re-search.
            // --------------------------------------------

            if (score > alpha && score < beta)
            {
                score =
                    -negamax(
                        board,
                        generator,
                        depth - 1,
                        -beta,
                        -alpha,
                        ply + 1,
                        true,
                        threadId
                    );
            }
        }


        // ----------------------------------------------------
        // Unmake move
        // ----------------------------------------------------

        board.unmakeMove(
            move.from,
            move.to,
            undo
        );


        // ----------------------------------------------------
        // Best score / move
        // ----------------------------------------------------

        if (score > bestScore)
        {
            bestScore = score;
            bestMoveAtThisNode = move;
        }


        // ----------------------------------------------------
        // Alpha update
        // ----------------------------------------------------

        if (score > alpha)
            alpha = score;


        // ----------------------------------------------------
        // Beta cutoff
        // ----------------------------------------------------

        if (alpha >= beta)
        {
            // Quiet move that caused a beta cutoff
            // becomes a killer move + gets a history bonus.

            if (
                move.captured == NONE &&
                move.promotion == NONE &&
                move.flags == FLAG_NONE
            )
            {
                if (ply < MAX_PLY)
                {
                    if (
                        !(killerMoves[threadId][ply][0].from == move.from &&
                          killerMoves[threadId][ply][0].to == move.to &&
                          killerMoves[threadId][ply][0].promotion == move.promotion)
                    )
                    {
                        killerMoves[threadId][ply][1] =
                            killerMoves[threadId][ply][0];

                        killerMoves[threadId][ply][0] =
                            move;
                    }
                }

                // History heuristic update
                int color = board.isWhiteToMove() ? 0 : 1;

                int& entry =
                    historyTable[threadId][color][move.from][move.to];

                entry += depth * depth;

                // Prevent overflow / runaway values
                if (entry > 1000000)
                {
                    entry /= 2;
                }
            }

            break;
        }
    }


    // --------------------------------------------------------
    // TT bound type
    // --------------------------------------------------------

    int flag;

    if (bestScore <= originalAlpha)
    {
        flag = TT_UPPERBOUND;
    }
    else if (bestScore >= beta)
    {
        flag = TT_LOWERBOUND;
    }
    else
    {
        flag = TT_EXACT;
    }


    // --------------------------------------------------------
    // Store in TT
    // --------------------------------------------------------

    ttStore(
        hash,
        depth,
        bestScore,
        flag,
        bestMoveAtThisNode
    );


    return bestScore;
}


// ------------------------------------------------------------
// Iterative Deepening / Find Best Move
// ------------------------------------------------------------

// ------------------------------------------------------------
// PV extraction and reporting
// ------------------------------------------------------------

static std::string pvSquareToString(int square)
{
    char file = 'a' + (square % 8);
    char rank = '1' + (square / 8);

    std::string s;
    s += file;
    s += rank;
    return s;
}

static std::string pvMoveToString(const Move& m)
{
    std::string s = pvSquareToString(m.from) + pvSquareToString(m.to);

    switch (m.promotion)
    {
        case QUEEN:  s += "q"; break;
        case ROOK:   s += "r"; break;
        case BISHOP: s += "b"; break;
        case KNIGHT: s += "n"; break;
        default: break;
    }

    return s;
}


// Walks the TT from the given position, following each node's
// recorded best move, to reconstruct the line the search
// actually trusts - not just the root move, but the sequence
// that justifies its score. Board is taken by value so this
// never disturbs the caller's real position. Every move is
// re-validated against the actual legal move list before being
// trusted - a TT entry could in principle belong to a different
// position that happens to share a hash (astronomically rare,
// but cheap to guard against here).
static std::vector<Move> extractPV(
    Board board,
    MoveGenerator& generator,
    int maxLength
)
{
    std::vector<Move> pv;

    for (int i = 0; i < maxLength && i < 64; i++)
    {
        uint64_t hash = board.getZobristHash();

        Move candidate = ttGetBestMove(hash);

        if (candidate.from == -1)
            break;

        std::vector<Move> legalMoves =
            generator.generateLegalMoves(board);

        bool found = false;
        Move actualMove;

        for (const Move& m : legalMoves)
        {
            if (
                m.from == candidate.from &&
                m.to == candidate.to &&
                m.promotion == candidate.promotion
            )
            {
                actualMove = m;
                found = true;
                break;
            }
        }

        if (!found)
            break;

        pv.push_back(actualMove);

        board.makeMove(
            actualMove.from,
            actualMove.to,
            actualMove.promotion
        );
    }

    return pv;
}


// ------------------------------------------------------------
// Single-thread root search.
//
// Takes Board BY VALUE - each thread must have its own
// independent copy to make/unmake moves on, since Board is
// mutated heavily during search. All threads share the TT
// (lockless, XOR-verified) plus killerMoves/historyTable,
// which are indexed by threadId so each thread has its own
// slice and there's no data race on those either.
//
// Time control (stopSearch/searchDeadline) is shared/global -
// set up ONCE by the caller via beginSearchTiming() before any
// threads are spawned, not per-thread here.
// ------------------------------------------------------------

Move searchRootThread(
    Board board,
    int maxDepth,
    int threadId
)
{
    MoveGenerator generator;

    // --------------------------------------------------------
    // Best move tracking
    // --------------------------------------------------------

    Move bestMoveOverall;

    bool haveMove = false;


    Move previousBestMove;

    previousBestMove.from = -1;
    previousBestMove.to = -1;
    previousBestMove.promotion = NONE;


    // --------------------------------------------------------
    // Iterative Deepening
    // --------------------------------------------------------

    for (int depth = 1;
         depth <= maxDepth;
         depth++)
    {
        std::vector<Move> moves =
            generator.generateLegalMoves(board);


        if (moves.empty())
            break;


        orderMoves(
            moves,
            board.isWhiteToMove() ? 0 : 1,
            threadId
            );


        // ----------------------------------------------------
        // Principal variation move from previous iteration
        // ----------------------------------------------------

        if (previousBestMove.from != -1)
        {
            for (size_t i = 0;
                 i < moves.size();
                 i++)
            {
                if (
                    moves[i].from ==
                        previousBestMove.from &&

                    moves[i].to ==
                        previousBestMove.to &&

                    moves[i].promotion ==
                        previousBestMove.promotion
                )
                {
                    std::swap(moves[0], moves[i]);
                    break;
                }
            }
        }


        Move bestMoveThisDepth = moves[0];

        int bestScore = INT_MIN;


        int alpha = INT_MIN + 1;
        int beta = INT_MAX;


        // ----------------------------------------------------
        // Root move search (PVS)
        // ----------------------------------------------------

        bool iterationAborted = false;

        for (size_t moveIndex = 0;
             moveIndex < moves.size();
             moveIndex++)
        {
            if (checkTime())
            {
                iterationAborted = true;
                break;
            }

            const Move& move = moves[moveIndex];

            UndoInfo undo =
                board.makeMove(
                    move.from,
                    move.to,
                    move.promotion
                );


            int score;

            if (moveIndex == 0)
            {
                score =
                    -negamax(
                        board,
                        generator,
                        depth - 1,
                        -beta,
                        -alpha,
                        0,
                        true,
                        threadId
                    );
            }
            else
            {
                score =
                    -negamax(
                        board,
                        generator,
                        depth - 1,
                        -alpha - 1,
                        -alpha,
                        0,
                        true,
                        threadId
                    );

                if (score > alpha && score < beta)
                {
                    score =
                        -negamax(
                            board,
                            generator,
                            depth - 1,
                            -beta,
                            -alpha,
                            0,
                            true,
                            threadId
                        );
                }
            }


            board.unmakeMove(
                move.from,
                move.to,
                undo
            );


            if (score > bestScore)
            {
                bestScore = score;
                bestMoveThisDepth = move;
            }


            if (bestScore > alpha)
                alpha = bestScore;

            if (checkTime())
            {
                iterationAborted = true;
                break;
            }
        }


        // ----------------------------------------------------
        // Discard incomplete iteration
        // ----------------------------------------------------

        if (iterationAborted)
            break;


        // ----------------------------------------------------
        // Save iteration result
        // ----------------------------------------------------

        bestMoveOverall = bestMoveThisDepth;

        haveMove = true;

        previousBestMove = bestMoveThisDepth;


        if (threadId == 0)
        {
            std::vector<Move> pv =
                extractPV(board, generator, depth);

            std::cout
                << "info depth "
                << depth
                << " score cp "
                << bestScore
                << " pv";

            if (pv.empty())
            {
                // Fallback - PV extraction can come up empty if
                // the TT entry got evicted between finishing the
                // search and reading it back (rare, harmless).
                // Still show the move we actually picked.
                std::cout << " " << pvMoveToString(bestMoveThisDepth);
            }
            else
            {
                for (const Move& m : pv)
                {
                    std::cout << " " << pvMoveToString(m);
                }
            }

            std::cout << "\n";
        }


        // ----------------------------------------------------
        // Soft time check
        // ----------------------------------------------------

        if (timeControlEnabled &&
            Clock::now() >= searchDeadline)
        {
            break;
        }
    }


    return bestMoveOverall;
}


// ------------------------------------------------------------
// findBestMove - single-threaded convenience wrapper.
//
// This is what main.cpp / simple callers use. It does exactly
// what the engine did before multithreading existed: one
// thread (threadId 0), no ThreadPool involved. For actual
// multi-threaded search, use ThreadPool directly (see
// threads.h) - it calls searchRootThread() itself across
// several threads.
// ------------------------------------------------------------

Move findBestMove(
    Board& board,
    int maxDepth,
    long long timeLimitMs
)
{
    beginSearchTiming(timeLimitMs);

    initKillerMoves();
    initHistoryTable();


    // --------------------------------------------------------
    // Opening Book
    // --------------------------------------------------------

    Move bookMove = getBookMove(board);

    if (bookMove.from != -1)
    {
        std::cout
            << "Opening book move: "
            << bookMove.from
            << " -> "
            << bookMove.to
            << "\n";

        return bookMove;
    }


    return searchRootThread(board, maxDepth, 0);
}