#include "tt.h"
#include <vector>
#include <atomic>
#include <cstdint>

// ============================================================
// Lockless transposition table - repaired
// ============================================================

struct TTSlot
{
    std::atomic<uint64_t> key{0};
    std::atomic<uint64_t> data{0};
};

static std::vector<TTSlot> table;
static size_t tableSize = 0;

static const int MATE_THRESHOLD = 900000;

static const int64_t SCORE_OFFSET = 1 << 21;
static const int SCORE_MIN = -2097152;
static const int SCORE_MAX = 2097151;

static const uint64_t OCCUPIED_BIT = 1ULL << 46;

// ------------------------------------------------------------
// Mate score adjustment
// ------------------------------------------------------------
static int adjustMateScoreForProbe(int score, int ply)
{
    if (score > MATE_THRESHOLD)
        return score - ply;

    if (score < -MATE_THRESHOLD)
        return score + ply;

    return score;
}

// ------------------------------------------------------------
// Packing
// ------------------------------------------------------------
static uint64_t packEntry(
    int depth,
    int score,
    int flag,
    const Move& move
)
{
    if (depth < 0)
        depth = 0;

    if (depth > 127)
        depth = 127;

    if (score < SCORE_MIN)
        score = SCORE_MIN;

    if (score > SCORE_MAX)
        score = SCORE_MAX;

    int from = move.from;
    int to = move.to;

    if (from < 0 || from > 63)
        from = 0;

    if (to < 0 || to > 63)
        to = 0;

    uint64_t promoField =
        (move.promotion == NONE)
            ? 0ULL
            : (uint64_t)(move.promotion + 1);

    uint64_t data = 0;

    data |= ((uint64_t)(from & 0x3F));
    data |= ((uint64_t)(to & 0x3F)) << 6;
    data |= (promoField & 0x7ULL) << 12;
    data |= ((uint64_t)(depth & 0x7F)) << 15;
    data |= ((uint64_t)(flag & 0x3)) << 22;

    uint64_t scoreUnsigned =
        (uint64_t)((int64_t)score + SCORE_OFFSET);

    data |= (scoreUnsigned & 0x3FFFFFULL) << 24;
    data |= OCCUPIED_BIT;

    return data;
}

static void unpackEntry(
    uint64_t data,
    int& depth,
    int& score,
    int& flag,
    Move& move
)
{
    move.from = (int)(data & 0x3F);
    move.to = (int)((data >> 6) & 0x3F);

    uint64_t promoField = (data >> 12) & 0x7;

    move.promotion =
        (promoField == 0)
            ? NONE
            : (int)(promoField - 1);

    move.piece = NONE;
    move.captured = NONE;
    move.flags = FLAG_NONE;

    depth = (int)((data >> 15) & 0x7F);
    flag = (int)((data >> 22) & 0x3);

    uint64_t scoreUnsigned = (data >> 24) & 0x3FFFFFULL;
    score = (int)((int64_t)scoreUnsigned - SCORE_OFFSET);
}

static inline size_t ttIndex(uint64_t hash)
{
    return (size_t)(hash & (tableSize - 1));
}

// ------------------------------------------------------------
// Init
// ------------------------------------------------------------
void ttInit(size_t sizeMB)
{
    size_t bytes = sizeMB * 1024ULL * 1024ULL;
    size_t maxEntries = bytes / sizeof(TTSlot);

    if (maxEntries == 0)
        maxEntries = 1;

    // Power of two table size for fast masking.
    size_t pow = 1;
    while (pow <= maxEntries / 2)
        pow <<= 1;

    tableSize = pow;

    table = std::vector<TTSlot>(tableSize);
}

// ------------------------------------------------------------
// Clear
// ------------------------------------------------------------
void ttClear()
{
    if (tableSize == 0)
        return;

    for (size_t i = 0; i < tableSize; i++)
    {
        table[i].data.store(0, std::memory_order_relaxed);
        table[i].key.store(0, std::memory_order_relaxed);
    }
}

// ------------------------------------------------------------
// Store
// ------------------------------------------------------------
void ttStore(
    uint64_t hash,
    int depth,
    int score,
    int flag,
    const Move& bestMove
)
{
    if (tableSize == 0)
        return;

    size_t index = ttIndex(hash);

    uint64_t oldData =
        table[index].data.load(std::memory_order_relaxed);

    uint64_t oldKey =
        table[index].key.load(std::memory_order_relaxed);

    bool oldOccupied = (oldData & OCCUPIED_BIT) != 0;

    // Depth-preferred replacement for same position.
    if (oldOccupied && (oldKey ^ oldData) == hash)
    {
        int oldDepth = (int)((oldData >> 15) & 0x7F);

        if (oldDepth > depth)
            return;
    }

    uint64_t data = packEntry(depth, score, flag, bestMove);
    uint64_t key = hash ^ data;

    table[index].data.store(data, std::memory_order_relaxed);
    table[index].key.store(key, std::memory_order_relaxed);
}

// ------------------------------------------------------------
// Probe
// ------------------------------------------------------------
bool ttProbe(
    uint64_t hash,
    int depth,
    int alpha,
    int beta,
    int ply,
    int& outScore,
    Move& outMove
)
{
    if (tableSize == 0)
        return false;

    size_t index = ttIndex(hash);

    uint64_t data =
        table[index].data.load(std::memory_order_relaxed);

    uint64_t key =
        table[index].key.load(std::memory_order_relaxed);

    if (!(data & OCCUPIED_BIT))
        return false;

    if ((key ^ data) != hash)
        return false;

    int entryDepth;
    int entryScore;
    int entryFlag;
    Move entryMove;

    unpackEntry(
        data,
        entryDepth,
        entryScore,
        entryFlag,
        entryMove
    );

    outMove = entryMove;

    if (entryDepth < depth)
        return false;

    int adjustedScore =
        adjustMateScoreForProbe(entryScore, ply);

    if (entryFlag == TT_EXACT)
    {
        outScore = adjustedScore;
        return true;
    }

    if (entryFlag == TT_LOWERBOUND && adjustedScore >= beta)
    {
        outScore = adjustedScore;
        return true;
    }

    if (entryFlag == TT_UPPERBOUND && adjustedScore <= alpha)
    {
        outScore = adjustedScore;
        return true;
    }

    return false;
}

// ------------------------------------------------------------
// Best move only
// ------------------------------------------------------------
Move ttGetBestMove(uint64_t hash)
{
    Move invalid{-1, -1, NONE, NONE, NONE, FLAG_NONE};

    if (tableSize == 0)
        return invalid;

    size_t index = ttIndex(hash);

    uint64_t data =
        table[index].data.load(std::memory_order_relaxed);

    uint64_t key =
        table[index].key.load(std::memory_order_relaxed);

    if (!(data & OCCUPIED_BIT))
        return invalid;

    if ((key ^ data) != hash)
        return invalid;

    int depth;
    int score;
    int flag;
    Move move;

    unpackEntry(
        data,
        depth,
        score,
        flag,
        move
    );

    return move;
}