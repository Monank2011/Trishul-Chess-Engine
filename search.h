#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "move.h"
#include "threads.h"

constexpr int MAX_PLY = 128;

// Single-threaded convenience wrapper (used by main.cpp / simple callers).
Move findBestMove(Board& board, int depth, long long timeLimitMs = -1);

// Per-thread search worker - used by ThreadPool for real multithreaded
// search. Board is taken by value on purpose (see search.cpp).
Move searchRootThread(Board board, int maxDepth, int threadId);

// Shared time-control setup/control, called by ThreadPool once before
// spawning threads (not per-thread).
void beginSearchTiming(long long timeLimitMs);
void requestSearchStop();

void initKillerMoves();
void initHistoryTable();

// Thread-aware killer moves and history tables
void initKillerMovesThreaded(int numThreads);
void initHistoryTableThreaded(int numThreads);

extern Move killerMoves[MAX_THREADS][MAX_PLY][2];
extern int historyTable[MAX_THREADS][2][64][64];

#endif