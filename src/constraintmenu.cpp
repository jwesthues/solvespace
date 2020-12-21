//
// Created by benjamin on 10/3/19.
//

#include "solvespace.h"

#include <vector>
#include "ui.h"
#include "constraintmenu.h"


struct ConstraintConditions {
    Command command;
    GraphicsWindow::GroupSelections valid_selection;  // TODO: make this some kind of iterable
    // TODO: add the applying logic in here
};
// static initialization should zero anything that we don't declare.
static ConstraintConditions Conditions[] = {
    {Command::SELECT_ON, {.points = 2, .n = 2}}
};


std::vector<GraphicsWindow::GroupSelections> getConditions(Command command) {

    std::vector<GraphicsWindow::GroupSelections> conditions;
    for(const auto condition: Conditions){
        if(condition.command == command) {
            conditions.push_back(condition.valid_selection);
        }
    }

    return conditions;
}

bool selectCountEqual(const GraphicsWindow::GroupSelections a, const GraphicsWindow::GroupSelections b) {
    return a.points == b.points &&
            a.entities == b.entities &&
            a.workplanes == b.workplanes &&
            a.faces == b.faces &&
            a.lineSegments == b.lineSegments &&
            a.circlesOrArcs == b.circlesOrArcs &&
            a.arcs == b.arcs &&
            a.cubics == b.cubics &&
            a.periodicCubics == b.periodicCubics &&
            a.anyNormals == b.anyNormals &&
            a.vectors == b.vectors &&
            a.constraints == b.constraints &&
            a.stylables == b.stylables &&
            a.constraintLabels == b.constraintLabels &&
            a.withEndpoints == b.withEndpoints &&
            a.n == b.n;
}

bool selectCountGreater(const GraphicsWindow::GroupSelections a, const GraphicsWindow::GroupSelections b) {
    return a.points >= b.points &&
           a.entities >= b.entities &&
           a.workplanes >= b.workplanes &&
           a.faces >= b.faces &&
           a.lineSegments >= b.lineSegments &&
           a.circlesOrArcs >= b.circlesOrArcs &&
           a.arcs >= b.arcs &&
           a.cubics >= b.cubics &&
           a.periodicCubics >= b.periodicCubics &&
           a.anyNormals >= b.anyNormals &&
           a.vectors >= b.vectors &&
           a.constraints >= b.constraints &&
           a.stylables >= b.stylables &&
           a.constraintLabels >= b.constraintLabels &&
           a.withEndpoints >= b.withEndpoints &&
           a.n >= b.n;
}

/**
 * A selection for a command is complete if the selection has the exact number of components that
 * the command might desire.
 */
bool TestSelectionComplete(Command command, const GraphicsWindow::GroupSelections toTest) {
    // get the command's target selections
    auto conditions = getConditions(command);
    // if any of the target selections have all the same counts as the selection to test, return true
    for(auto condition: conditions) {
        if (selectCountEqual(condition, toTest)){
            return true;
        }
    }
    
    return false;
}

/**
 * A selection for a command can possibly be valid if the selection has less items selected than
 * a command might desire.
 */
bool TestSelectionPossiblyValid(Command command, const GraphicsWindow::GroupSelections toTest) {
    // get the command's target selections
    auto conditions = getConditions(command);
    // if any of the target selections have more counts than the selection to test, return true
    for(auto condition: conditions) {
        if (selectCountGreater(condition, toTest)){
            return true;
        }
    }

    return false;
}

// TODO: Make this unique for each command/condition
bool ApplyCommandToSelection(Command command, const GraphicsWindow::GroupSelections selection) {

    Constraint c = {};
    c.group = SS.GW.activeGroup;
    c.workplane = SS.GW.ActiveWorkplane();
    bool constraintValid = true;
    if(selection.points == 2 && selection.n == 2) {
        c.type = SolveSpace::Constraint::Type::POINTS_COINCIDENT;
        c.ptA = selection.point[0];
        c.ptB = selection.point[1];
    } else {
        // set to false in else so if there are multiple, you only set it in a single place and
        // don't have to copy it.
        constraintValid = false;
    }

    if(constraintValid) {
        SolveSpace::Constraint::AddConstraint(&c);
    }
    return constraintValid;
}

bool doCommandSelectionTest(Command command) {
    SS.GW.GroupSelection();
    auto const &gs = SS.GW.gs;

    // do we still have the chance to fulfill a valid selection?
    bool valid = TestSelectionPossiblyValid(command, gs);
    // do we fulfill any of the possible ones?
    bool fulfills = TestSelectionComplete(command, gs);
    bool success = false;
    if(fulfills) {
        // if we do, then does it meet the extra requirements?
        success = ApplyCommandToSelection(command, gs);
        if(!success) {
            // TODO: should this be a real error, or should we continue?
            printf("Could not apply constraint to selection\n");
        }
    }
    if(!valid) {
        // TODO display real error here - look it up from a table?
        Error("Ya done goofed");
    }
    // If we are finished handling the selection (whether due to success or failure), clear
    // the current command and selection.
    if(!valid || success){
        SS.GW.selectionCommand = Command::NONE;
        SS.GW.ClearSelection();
    }
}


