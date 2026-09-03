#ifndef OPENINGBOOK_H
#define OPENINGBOOK_H

#include "board.h"
#include "move.h"

Move getBookMove(const Board& board);

void setOpeningBookEnabled(bool enabled);
bool isOpeningBookEnabled();

#endif