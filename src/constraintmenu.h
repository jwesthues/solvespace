//
// Created by benjamin on 10/3/19.
//

#ifndef SOLVESPACE_CONSTRAINTMENU_H
#define SOLVESPACE_CONSTRAINTMENU_H

#include "solvespace.h"
#include "ui.h"

bool TestSelectionComplete(Command command, GraphicsWindow::GroupSelections toTest);
bool TestSelectionPossiblyValid(Command command, GraphicsWindow::GroupSelections toTest);
bool ApplyCommandToSelection(Command command, const GraphicsWindow::GroupSelections selection);
bool doCommandSelectionTest(Command command);

#endif //SOLVESPACE_CONSTRAINTMENU_H
