#include "threads.h"
#include "search.h"
#include "movegen.h"
#include "openingbook.h"

#include <iostream>


ThreadPool::ThreadPool(int numThreads)
    : numThreads(numThreads),
      haveResult(false),
      usedBookMove(false)
{
    if (this->numThreads < 1)
        this->numThreads = 1;

    if (this->numThreads > MAX_THREADS)
        this->numThreads = MAX_THREADS;
}


ThreadPool::~ThreadPool()
{
    // Safety net - if getBestMove() was never called, make
    // sure we don't destroy the pool while threads are still
    // running.
    for (std::thread& worker : workers)
    {
        if (worker.joinable())
            worker.join();
    }
}


void ThreadPool::startSearch(
    Board& board,
    int maxDepth,
    long long timeLimitMs
)
{
    haveResult = false;
    usedBookMove = false;
    workers.clear();


    // --------------------------------------------------------
    // Opening book - checked once, before any threads spawn.
    // If we have a book move, there's no reason to search at
    // all.
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

        std::lock_guard<std::mutex> lock(resultMutex);

        bestMove = bookMove;
        haveResult = true;
        usedBookMove = true;

        return;
    }


    // --------------------------------------------------------
    // Shared setup - done ONCE, before any thread starts,
    // since this state (stop flag, deadline, killer/history
    // tables) is shared across every worker thread.
    // --------------------------------------------------------

    beginSearchTiming(timeLimitMs);

    initKillerMovesThreaded(numThreads);
    initHistoryTableThreaded(numThreads);


    // --------------------------------------------------------
    // Spawn workers. Each gets its own COPY of the board
    // (searchRootThread takes Board by value) so threads never
    // step on each other's move-making. Thread 0's result is
    // the one we report; the rest exist purely to warm the
    // shared TT faster than a single thread could alone.
    // --------------------------------------------------------

    for (int i = 0; i < numThreads; i++)
    {
        workers.emplace_back(
            [this, board, maxDepth, i]() mutable
            {
                Move result = searchRootThread(board, maxDepth, i);

                if (i == 0)
                {
                    std::lock_guard<std::mutex> lock(resultMutex);

                    bestMove = result;
                    haveResult = true;
                }
            }
        );
    }
}


Move ThreadPool::getBestMove()
{
    if (usedBookMove)
    {
        std::lock_guard<std::mutex> lock(resultMutex);
        return bestMove;
    }


    // Wait for every thread to finish. In fixed-depth mode
    // this means all threads reach maxDepth; in timed mode
    // they all stop at (roughly) the same shared deadline via
    // checkTime().
    for (std::thread& worker : workers)
    {
        if (worker.joinable())
            worker.join();
    }

    workers.clear();


    std::lock_guard<std::mutex> lock(resultMutex);

    if (!haveResult)
    {
        // Shouldn't normally happen (thread 0 always completes
        // at least depth 1 before any time cutoff can trigger),
        // but return a clearly-invalid move rather than garbage
        // if it ever does.
        return Move{-1, -1, NONE, NONE, NONE, FLAG_NONE};
    }

    return bestMove;
}