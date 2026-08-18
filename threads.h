#ifndef THREADS_H
#define THREADS_H

#include <vector>
#include <thread>
#include <mutex>
#include "board.h"
#include "move.h"

const int MAX_THREADS = 256;

class ThreadPool
{
public:
    ThreadPool(int numThreads);
    ~ThreadPool();

    // Kicks off search across all threads. Blocking calls
    // (opening book check, shared timing setup) happen here,
    // once, before any thread is spawned.
    void startSearch(Board& board, int maxDepth, long long timeLimitMs);

    // Waits for every thread to finish, then returns thread 0's
    // result (the "official" search result). Safe to call once
    // per startSearch().
    Move getBestMove();

private:
    int numThreads;
    std::vector<std::thread> workers;

    std::mutex resultMutex;
    Move bestMove;
    bool haveResult;

    bool usedBookMove;
};

#endif