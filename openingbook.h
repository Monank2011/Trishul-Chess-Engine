#ifndef OPENINGBOOK_H
#define OPENINGBOOK_H

#include "board.h"
#include "move.h"
#include <string>

// Look up a weighted-random book move for the current
// position. Returns an invalid move (from == -1) if the
// position is not in the book or the book is disabled.
Move getBookMove(const Board& board);

// Enable/disable book usage entirely.
void setOpeningBookEnabled(bool enabled);
bool isOpeningBookEnabled();

// Point the engine at a book file. Loads immediately.
// Returns true on success. If loading fails, the engine
// falls back to the built-in repertoire.
bool setBookFile(const std::string& path);

// Get the currently-configured book file path (empty if
// the built-in fallback is in use).
std::string getBookFile();

#endif