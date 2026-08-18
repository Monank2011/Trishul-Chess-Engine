#include "tt.h"
#include <vector>
#include <atomic>

// ============================================================
// Lockless transposition table (XOR-verified entries)
// ============================================================
//
// Multiple search threads read and write the same TT slots
// with no locking, for speed. The danger of that is a "torn"
// entry: thread A is halfway through writing a slot when
// thread B reads it, and B ends up with a Frankenstein mix of
// A's old data and A's new data - a hash that matches but a
// score/move that doesn't actually belong to that position.
// That's exactly what caused the eval=99999999 / phantom-
// promotion corruption before this rewrite.
//
// The fix (the same technique Stockfish uses): instead of
// storing the real hash, we store hash ^ data. On read, we
// recompute (storedKey ^ storedData) and check it equals the
// hash we're looking for. If the two words were torn apart by
// a concurrent write, this check will almost certainly fail
// (torn data means storedKey ^ storedData no longer equals
// any real hash), and we simply treat it as a miss instead of
// trusting corrupted data. No lock ever taken, and a torn read
// is self-detected rather than silently accepted.
//
// This only works because each slot's real payload (depth,
// score, flag, move) is packed into a single 64-bit word that
// std::atomic<uint64_t> can load/store atomically on its own -
// see packEntry()/unpackEntry() below for the bit layout.

struct TTSlot
{
    std::atomic<uint64_t> key{0};
    std::atomic<uint64_t> data{0};
};

static std::vector<TTSlot> table;
static size_t tableSize = 0;


// ------------------------------------------------------------
// Packing: depth, score, flag, and a move all fit into one
// 64-bit word.
//
// Layout (from bit 0):
//   bits  0- 5 (6 bits)  : move.from       (0-63)
//   bits  6-11 (6 bits)  : move.to         (0-63)
//   bits 12-14 (3 bits)  : move.promotion  (0 = NONE, else piece+1)
//   bits 15-21 (7 bits)  : depth           (0-127)
//   bits 22-23 (2 bits)  : flag            (TT_EXACT/LOWER/UPPER)
//   bits 24-45 (22 bits) : score, offset so it's always positive
//   bit  46    (1 bit)   : occupied marker
// ------------------------------------------------------------

static const int64_t SCORE_OFFSET = 1 << 21;

static uint64_t packEntry(
    int depth,
    int score,
    int flag,
    const Move& move
)
{
    uint64_t data = 0;

    uint64_t promoField =
        (move.promotion == NONE)
            ? 0ULL
            : (uint64_t)(move.promotion + 1);

    data |= ((uint64_t)(move.from & 0x3F));
    data |= ((uint64_t)(move.to   & 0x3F)) << 6;
    data |= (promoField & 0x7ULL) << 12;
    data |= ((uint64_t)(depth & 0x7F)) << 15;
    data |= ((uint64_t)(flag  & 0x3))  << 22;

    uint64_t scoreUnsigned =
        (uint64_t)((int64_t)score + SCORE_OFFSET);

    data |= (scoreUnsigned & 0x3FFFFFULL) << 24;

    data |= (1ULL << 46); // occupied marker

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
    move.to   = (int)((data >> 6) & 0x3F);

    uint64_t promoField = (data >> 12) & 0x7;

    move.promotion =
        (promoField == 0)
            ? NONE
            : (int)(promoField - 1);

    // Not stored in the TT - not needed for move ordering,
    // and the search re-derives these when it actually makes
    // the move on the board.
    move.piece    = NONE;
    move.captured = NONE;
    move.flags    = FLAG_NONE;

    depth = (int)((data >> 15) & 0x7F);
    flag  = (int)((data >> 22) & 0x3);

    uint64_t scoreUnsigned = (data >> 24) & 0x3FFFFFULL;

    score = (int)((int64_t)scoreUnsigned - SCORE_OFFSET);
}


void ttInit(size_t sizeMB)
{
    size_t bytes = sizeMB * 1024ULL * 1024ULL;

    tableSize = bytes / sizeof(TTSlot);

    if (tableSize == 0)
        tableSize = 1;

    // std::vector<TTSlot> can't be resized via assign() since
    // std::atomic isn't copyable - construct a fresh one of
    // the right size instead (each TTSlot default-constructs
    // to key=0, data=0, which is a safe "empty" state).
    table = std::vector<TTSlot>(tableSize);
}

void ttClear()
{
    for (size_t i = 0; i < tableSize; i++)
    {
        table[i].data.store(0, std::memory_order_relaxed);
        table[i].key.store(0, std::memory_order_relaxed);
    }
}


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

    size_t index = hash % tableSize;

    // ----------------------------------------------------
    // Depth-preferred replacement
    // ----------------------------------------------------
    //
    // With multiple threads writing the same table, a
    // shallow write can otherwise clobber a deep, expensive
    // result from another thread. Only overwrite an entry
    // for the SAME position if the new search was at least
    // as deep as the old one. A different position landing
    // in the same slot (a hash collision) is always replaced,
    // since keeping stale unrelated data has no value.

    uint64_t oldData = table[index].data.load(std::memory_order_relaxed);
    uint64_t oldKey  = table[index].key.load(std::memory_order_relaxed);

    if ((oldKey ^ oldData) == hash)
    {
        int oldDepth = (int)((oldData >> 15) & 0x7F);

        if (oldDepth > depth)
            return; // keep the existing deeper entry
    }

    uint64_t data = packEntry(depth, score, flag, bestMove);
    uint64_t key  = hash ^ data;

    // Write data, THEN key. If a concurrent reader loads data
    // (new) and key (old) - or data (old) and key (new) - the
    // XOR check on read will not match the real hash, and the
    // read is correctly rejected as torn. The only way a torn
    // read passes the check is if it happens to reconstruct
    // exactly the right hash by coincidence, which is not
    // realistically possible with 64-bit values.
    table[index].data.store(data, std::memory_order_relaxed);
    table[index].key.store(key,   std::memory_order_relaxed);
}


bool ttProbe(
    uint64_t hash,
    int depth,
    int alpha,
    int beta,
    int& outScore,
    Move& outMove
)
{
    if (tableSize == 0)
        return false;

    size_t index = hash % tableSize;

    uint64_t data = table[index].data.load(std::memory_order_relaxed);
    uint64_t key  = table[index].key.load(std::memory_order_relaxed);

    if ((key ^ data) != hash)
        return false; // empty slot, different position, or torn read

    int entryDepth, entryScore, entryFlag;
    Move entryMove;

    unpackEntry(data, entryDepth, entryScore, entryFlag, entryMove);

    outMove = entryMove; // useful for move ordering even if too shallow to use the score

    if (entryDepth < depth)
        return false; // not searched deep enough to trust the score

    if (entryFlag == TT_EXACT)
    {
        outScore = entryScore;
        return true;
    }
    else if (entryFlag == TT_LOWERBOUND && entryScore >= beta)
    {
        outScore = entryScore;
        return true;
    }
    else if (entryFlag == TT_UPPERBOUND && entryScore <= alpha)
    {
        outScore = entryScore;
        return true;
    }

    return false;
}


Move ttGetBestMove(uint64_t hash)
{
    if (tableSize == 0)
        return Move{-1, -1, NONE, NONE, NONE, FLAG_NONE};

    size_t index = hash % tableSize;

    uint64_t data = table[index].data.load(std::memory_order_relaxed);
    uint64_t key  = table[index].key.load(std::memory_order_relaxed);

    if ((key ^ data) != hash)
        return Move{-1, -1, NONE, NONE, NONE, FLAG_NONE};

    int depth, score, flag;
    Move move;

    unpackEntry(data, depth, score, flag, move);

    return move;
}