//-----------------------------------------------------------------------------
// The text-based browser window, used to view the structure of the model by
// groups and for other similar purposes.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

//-----------------------------------------------------------------------------
// A navigation bar that always appears at the top of the window, with a
// link to bring us back home.
//-----------------------------------------------------------------------------
void TextWindow::ScreenHome(int link, uint32_t v) {
    SS.TW.GoToScreen(Screen::LIST_OF_GROUPS);
}
void TextWindow::ShowHeader(bool withNav) {
    ClearScreen();

    const char *header;
    std::string desc;
    if(SS.GW.LockedInWorkplane()) {
        header = "in plane: ";
        desc = SK.GetEntity(SS.GW.ActiveWorkplane())->DescriptionString();
    } else {
        header = "drawing / constraining in 3d";
        desc = "";
    }

    // Navigation buttons
    if(withNav) {
        Printf(false, " %Fl%Lh%fhome%E   %Ft%s%E%s",
            (&TextWindow::ScreenHome), header, desc.c_str());
    } else {
        Printf(false, "        %Ft%s%E%s", header, desc.c_str());
    }

    // Leave space for the icons that are painted here.
    Printf(false, "");
    Printf(false, "");
}

//-----------------------------------------------------------------------------
// The screen that shows a list of every group in the sketch, with options
// to hide or show them, and to view them in detail. This is our home page.
//-----------------------------------------------------------------------------
void TextWindow::ScreenSelectGroup(int link, uint32_t v) {
    GraphicsWindow::MenuEdit(Command::UNSELECT_ALL);
    SS.TW.GoToScreen(Screen::GROUP_INFO);
    SS.TW.shown.group.v = v;
}
void TextWindow::ScreenToggleGroupShown(int link, uint32_t v) {
    hGroup hg = { v };
    Group *g = SK.GetGroup(hg);
    g->visible = !(g->visible);
    // If a group was just shown, then it might not have been generated
    // previously, so regenerate.
    SS.GenerateAll();
}
void TextWindow::ScreenShowGroupsSpecial(int link, uint32_t v) {
    bool state = link == 's';
    for(hGroup hg : SK.groupOrder) {
        Group *g = SK.GetGroup(hg);

        g->visible = state;
    }
    SS.GW.persistentDirty = true;
}
void TextWindow::ScreenActivateGroup(int link, uint32_t v) {
    SS.GW.activeGroup.v = v;
    SK.GetGroup(SS.GW.activeGroup)->Activate();
    SS.GW.ClearSuper();
}
void TextWindow::ReportHowGroupSolved(hGroup hg) {
    SS.GW.ClearSuper();
    SS.TW.GoToScreen(Screen::GROUP_SOLVE_INFO);
    SS.TW.shown.group = hg;
    SS.ScheduleShowTW();
}
void TextWindow::ScreenHowGroupSolved(int link, uint32_t v) {
    if(SS.GW.activeGroup.v != v) {
        ScreenActivateGroup(link, v);
    }
    SS.TW.GoToScreen(Screen::GROUP_SOLVE_INFO);
    SS.TW.shown.group.v = v;
}
void TextWindow::ScreenShowConfiguration(int link, uint32_t v) {
    SS.TW.GoToScreen(Screen::CONFIGURATION);
}
void TextWindow::ScreenShowEditView(int link, uint32_t v) {
    SS.TW.GoToScreen(Screen::EDIT_VIEW);
}
void TextWindow::ScreenGoToWebsite(int link, uint32_t v) {
    Platform::OpenInBrowser("http://solvespace.com/txtlink");
}
void TextWindow::ShowListOfGroups() {
    const char *radioTrue  = " " RADIO_TRUE  " ",
               *radioFalse = " " RADIO_FALSE " ",
               *checkTrue  = " " CHECK_TRUE  " ",
               *checkFalse = " " CHECK_FALSE " ";

    Printf(true, "%Ft active");
    Printf(false, "%Ft    shown dof group-name%E");
    bool afterActive = false;
    bool backgroundParity = false;
    for(hGroup hg : SK.groupOrder) {
        Group *g = SK.GetGroup(hg);

        std::string s = g->DescriptionString();
        bool active = (g->h == SS.GW.activeGroup);
        bool shown = g->visible;
        bool ok = g->IsSolvedOkay();
        bool warn = (g->type == Group::Type::DRAWING_WORKPLANE &&
                     g->polyError.how != PolyError::GOOD) ||
                    ((g->type == Group::Type::EXTRUDE ||
                      g->type == Group::Type::LATHE) &&
                     SK.GetGroup(g->opA)->polyError.how != PolyError::GOOD);
        int dof = g->solved.dof;
        char sdof[16] = "ok ";
        if(ok && dof > 0) {
            if(dof > 999) {
              strcpy(sdof, "###");
            } else {
              sprintf(sdof, "%-3d", dof);
            }
        }
        bool ref = (g->h == Group::HGROUP_REFERENCES);
        Printf(false,
               "%Bp%Fd "
               "%Ft%s%Fb%D%f%Ll%s%E "
               "%Fb%s%D%f%Ll%s%E  "
               "%Fp%D%f%s%Ll%s%E "
               "%Fl%Ll%D%f%s",
               // Alternate between light and dark backgrounds, for readability
               backgroundParity ? 'd' : 'a',
               // Link that activates the group
               ref ? "   " : "",
               g->h.v, (&TextWindow::ScreenActivateGroup),
               ref ? "" : (active ? radioTrue : radioFalse),
               // Link that hides or shows the group
               afterActive ? " - " : "",
               g->h.v, (&TextWindow::ScreenToggleGroupShown),
               afterActive ? "" : (shown ? checkTrue : checkFalse),
               // Link to the errors, if a problem occurred while solving
               ok ? (warn ? 'm' : (dof > 0 ? 'i' : 's')) : 'x',
               g->h.v, (&TextWindow::ScreenHowGroupSolved),
               ok ? ((warn && SS.checkClosedContour) ? "err" : sdof) : "",
               ok ? "" : "ERR",
               // Link to a screen that gives more details on the group
               g->h.v, (&TextWindow::ScreenSelectGroup), s.c_str());

        if(active) afterActive = true;
        backgroundParity = !backgroundParity;
    }

    Printf(true,  "  %Fl%Ls%fshow all%E / %Fl%Lh%fhide all%E",
        &(TextWindow::ScreenShowGroupsSpecial),
        &(TextWindow::ScreenShowGroupsSpecial));
    Printf(true,  "  %Fl%Ls%fline styles%E /"
                   " %Fl%Ls%fview%E /"
                   " %Fl%Ls%fconfiguration%E",
        &(TextWindow::ScreenShowListOfStyles),
        &(TextWindow::ScreenShowEditView),
        &(TextWindow::ScreenShowConfiguration));
}


//-----------------------------------------------------------------------------
// The screen that shows information about a specific group, and allows the
// user to edit various things about it.
//-----------------------------------------------------------------------------
void TextWindow::ScreenHoverGroupWorkplane(int link, uint32_t v) {
    SS.GW.hover.Clear();
    hGroup hg = { v };
    SS.GW.hover.entity = hg.entity(0);
    SS.GW.hover.emphasized = true;
}
void TextWindow::ScreenHoverEntity(int link, uint32_t v) {
    SS.GW.hover.Clear();
    hEntity he = { v };
    SS.GW.hover.entity = he;
    SS.GW.hover.emphasized = true;
}
void TextWindow::ScreenHoverRequest(int link, uint32_t v) {
    SS.GW.hover.Clear();
    hRequest hr = { v };
    SS.GW.hover.entity = hr.entity(0);
    SS.GW.hover.emphasized = true;
}
void TextWindow::ScreenHoverConstraint(int link, uint32_t v) {
    if(!SS.GW.showConstraints) return;

    hConstraint hc = { v };
    SS.GW.hover.Clear();
    SS.GW.hover.constraint = hc;
    SS.GW.hover.emphasized = true;
}
void TextWindow::ScreenSelectEntity(int link, uint32_t v) {
    SS.GW.ClearSelection();
    GraphicsWindow::Selection sel = {};
    hEntity he = { v };
    sel.entity = he;
    SS.GW.selection.Add(&sel);
}
void TextWindow::ScreenSelectRequest(int link, uint32_t v) {
    SS.GW.ClearSelection();
    GraphicsWindow::Selection sel = {};
    hRequest hr = { v };
    sel.entity = hr.entity(0);
    SS.GW.selection.Add(&sel);
}
void TextWindow::ScreenSelectConstraint(int link, uint32_t v) {
    SS.GW.ClearSelection();
    GraphicsWindow::Selection sel = {};
    sel.constraint.v = v;
    SS.GW.selection.Add(&sel);
}

void TextWindow::ScreenChangeGroupOption(int link, uint32_t v) {
    SS.UndoRemember();
    Group *g = SK.GetGroup(SS.TW.shown.group);

    switch(link) {
        case 's': g->subtype = Group::Subtype::ONE_SIDED; break;
        case 'S': g->subtype = Group::Subtype::TWO_SIDED; break;

        case 'k': g->skipFirst = true; break;
        case 'K': g->skipFirst = false; break;

        case 'c':
            if(g->type == Group::Type::EXTRUDE) {
                // When an extrude group is first created, it's positioned for a union
                // extrusion. If no constraints were added, flip it when we switch between
                // union/assemble and difference/intersection modes to avoid manual work doing the same.
                if(g->meshCombine != (Group::CombineAs)v && g->GetNumConstraints() == 0) {
                    // I apologise for this if statement
                    if((((Group::CombineAs::DIFFERENCE == g->meshCombine) ||
                         (Group::CombineAs::INTERSECTION == g->meshCombine)) &&
                        (Group::CombineAs::DIFFERENCE != (Group::CombineAs)v) &&
                        (Group::CombineAs::INTERSECTION != (Group::CombineAs)v)) ||
                       ((Group::CombineAs::DIFFERENCE != g->meshCombine) &&
                        (Group::CombineAs::INTERSECTION != g->meshCombine) &&
                        ((Group::CombineAs::DIFFERENCE == (Group::CombineAs)v) ||
                         (Group::CombineAs::INTERSECTION == (Group::CombineAs)v)))) {
                        g->ExtrusionForceVectorTo(g->ExtrusionGetVector().Negated());
                    }
                }
            }
            g->meshCombine = (Group::CombineAs)v;
            break;

        case 'P': g->suppress = !(g->suppress); break;

        case 'r': g->relaxConstraints = !(g->relaxConstraints); break;

        case 'e': g->allowRedundant = !(g->allowRedundant); break;

        case 'v': g->visible = !(g->visible); break;

        case 'd': g->allDimsReference = !(g->allDimsReference); break;

        case 'f': g->forceToMesh = !(g->forceToMesh); break;

        case 'x':
            g->explode = !(g->explode);
            if(g->explode && g->explodeDistance < 1e-6) {
                g->explodeDistance = 1.0;
            }
            break;
    }

    SS.MarkGroupDirty(g->h);
    SS.GW.ClearSuper();
}

void TextWindow::ScreenColor(int link, uint32_t v) {
    SS.UndoRemember();

    Group *g = SK.GetGroup(SS.TW.shown.group);
    SS.TW.ShowEditControlWithColorPicker(3, g->color);
    SS.TW.edit.meaning = Edit::GROUP_COLOR;
}
void TextWindow::ScreenOpacity(int link, uint32_t v) {
    Group *g = SK.GetGroup(SS.TW.shown.group);

    SS.TW.ShowEditControl(11, ssprintf("%.2f", g->color.alphaF()));
    SS.TW.edit.meaning = Edit::GROUP_OPACITY;
    SS.TW.edit.group = g->h;
}
void TextWindow::ScreenChangeExprA(int link, uint32_t v) {
    Group *g = SK.GetGroup(SS.TW.shown.group);

    SS.TW.ShowEditControl(10, ssprintf("%d", (int)g->valA));
    SS.TW.edit.meaning = Edit::TIMES_REPEATED;
    SS.TW.edit.group.v = v;
}
void TextWindow::ScreenChangeGroupName(int link, uint32_t v) {
    Group *g = SK.GetGroup(SS.TW.shown.group);
    SS.TW.ShowEditControl(12, g->DescriptionString().substr(5));
    SS.TW.edit.meaning = Edit::GROUP_NAME;
    SS.TW.edit.group.v = v;
}
void TextWindow::ScreenChangeGroupScale(int link, uint32_t v) {
    Group *g = SK.GetGroup(SS.TW.shown.group);

    SS.TW.ShowEditControl(13, ssprintf("%.3f", g->scale));
    SS.TW.edit.meaning = Edit::GROUP_SCALE;
    SS.TW.edit.group.v = v;
}
void TextWindow::ScreenChangeGroupExplodeDistance(int link, uint32_t v) {
    Group *g = SK.GetGroup(SS.TW.shown.group);

    SS.TW.ShowEditControl(27, ssprintf("%.3f", g->explodeDistance));
    SS.TW.edit.meaning = Edit::GROUP_EXPLODE_DISTANCE;
    SS.TW.edit.group.v = v;
}
void TextWindow::ScreenDeleteGroup(int link, uint32_t v) {
    SS.UndoRemember();

    hGroup hg = SS.TW.shown.group;
    if(hg == SS.GW.activeGroup) {
        SS.GW.activeGroup = SK.GetGroup(SS.GW.activeGroup)->PreviousGroup()->h;
    }

    // Reset the text window, since we're displaying information about
    // the group that's about to get deleted.
    SS.TW.ClearSuper();

    // This is a major change, so let's re-solve everything.
    SK.group.RemoveById(hg);
    SS.GenerateAll(SolveSpaceUI::Generate::ALL);

    // Reset the graphics window. This will also recreate the default
    // group if it was removed.
    SS.GW.ClearSuper();
}
void TextWindow::ShowGroupInfo() {
    Group *g = SK.GetGroup(shown.group);
    const char *s = "???";

    if(shown.group == Group::HGROUP_REFERENCES) {
        Printf(true, "%FtGROUP  %E%s", g->DescriptionString().c_str());
        goto list_items;
    } else {
        Printf(true, "%FtGROUP  %E%s [%Fl%Ll%D%frename%E/%Fl%Ll%D%fdel%E]",
            g->DescriptionString().c_str(),
            g->h.v, &TextWindow::ScreenChangeGroupName,
            g->h.v, &TextWindow::ScreenDeleteGroup);
    }

    if(g->type == Group::Type::LATHE) {
        Printf(true, " %Ftlathe plane sketch");
    } else if(g->type == Group::Type::EXTRUDE || g->type == Group::Type::ROTATE ||
              g->type == Group::Type::TRANSLATE || g->type == Group::Type::REVOLVE ||
              g->type == Group::Type::HELIX) {
        if(g->type == Group::Type::EXTRUDE) {
            s = "extrude plane sketch";
        } else if(g->type == Group::Type::TRANSLATE) {
            s = "translate original sketch";
        } else if(g->type == Group::Type::HELIX) {
            s = "create helical extrusion";
        } else if(g->type == Group::Type::ROTATE) {
            s = "rotate original sketch";
        } else if(g->type == Group::Type::REVOLVE) {
            s = "revolve original sketch";
        }
        Printf(true, " %Ft%s%E", s);

        bool one = (g->subtype == Group::Subtype::ONE_SIDED);
        Printf(false,
            "%Ba   %f%Ls%Fd%s one-sided%E  "
                  "%f%LS%Fd%s two-sided%E",
            &TextWindow::ScreenChangeGroupOption,
            one ? RADIO_TRUE : RADIO_FALSE,
            &TextWindow::ScreenChangeGroupOption,
            !one ? RADIO_TRUE : RADIO_FALSE);

        if(g->type == Group::Type::ROTATE || g->type == Group::Type::TRANSLATE) {
            if(g->subtype == Group::Subtype::ONE_SIDED) {
                bool skip = g->skipFirst;
                Printf(false,
                   "%Bd   %Ftstart  %f%LK%Fd%s with original%E  "
                         "%f%Lk%Fd%s with copy #1%E",
                    &ScreenChangeGroupOption,
                    !skip ? RADIO_TRUE : RADIO_FALSE,
                    &ScreenChangeGroupOption,
                    skip ? RADIO_TRUE : RADIO_FALSE);
            }

            int times = (int)(g->valA);
            Printf(false, "%Bp   %Ftrepeat%E %d time%s %Fl%Ll%D%f[change]%E",
                (g->subtype == Group::Subtype::ONE_SIDED) ? 'a' : 'd',
                times, times == 1 ? "" : "s",
                g->h.v, &TextWindow::ScreenChangeExprA);
        }
    } else if(g->type == Group::Type::LINKED) {
        Printf(true, " %Ftlink geometry from file%E");
        Platform::Path relativePath = g->linkFile.RelativeTo(SS.saveFile.Parent());
        if(relativePath.IsEmpty()) {
            Printf(false, "%Ba   '%s'", g->linkFile.raw.c_str());
        } else {
            Printf(false, "%Ba   '%s'", relativePath.raw.c_str());
        }
        Printf(false, "%Bd   %Ftscaled by%E %# %Fl%Ll%f%D[change]%E",
            g->scale,
            &TextWindow::ScreenChangeGroupScale, g->h.v);
    } else if(g->type == Group::Type::DRAWING_3D) {
        Printf(true, " %Ftsketch in 3d%E");
    } else if(g->type == Group::Type::DRAWING_WORKPLANE) {
        Printf(true, " %Ftsketch in new workplane%E");
    } else {
        Printf(true, "???");
    }
    Printf(false, "");

    if(g->type == Group::Type::EXTRUDE || g->type == Group::Type::LATHE ||
       g->type == Group::Type::REVOLVE || g->type == Group::Type::LINKED ||
       g->type == Group::Type::HELIX) {
        bool un   = (g->meshCombine == Group::CombineAs::UNION);
        bool diff = (g->meshCombine == Group::CombineAs::DIFFERENCE);
        bool intr = (g->meshCombine == Group::CombineAs::INTERSECTION);
        bool asy  = (g->meshCombine == Group::CombineAs::ASSEMBLE);

        Printf(false, " %Ftsolid model as");
        Printf(false, "%Ba   %f%D%Lc%Fd%s union%E  "
                             "%f%D%Lc%Fd%s assemble%E  ",
            &TextWindow::ScreenChangeGroupOption,
            Group::CombineAs::UNION,
            un ? RADIO_TRUE : RADIO_FALSE,
            &TextWindow::ScreenChangeGroupOption,
            Group::CombineAs::ASSEMBLE,
            (asy ? RADIO_TRUE : RADIO_FALSE));
        Printf(false, "%Ba   %f%D%Lc%Fd%s difference%E  "
                             "%f%D%Lc%Fd%s intersection%E  ",
            &TextWindow::ScreenChangeGroupOption,
            Group::CombineAs::DIFFERENCE,
            diff ? RADIO_TRUE : RADIO_FALSE,
            &TextWindow::ScreenChangeGroupOption,
            Group::CombineAs::INTERSECTION,
            intr ? RADIO_TRUE : RADIO_FALSE);

        if(g->type == Group::Type::EXTRUDE || g->type == Group::Type::LATHE ||
           g->type == Group::Type::REVOLVE || g->type == Group::Type::HELIX) {
            Printf(false,
                "%Bd   %Ftcolor   %E%Bz  %Bd (%@, %@, %@) %f%D%Lf%Fl[change]%E",
                &g->color,
                g->color.redF(), g->color.greenF(), g->color.blueF(),
                ScreenColor, top[rows-1] + 2);
            Printf(false, "%Bd   %Ftopacity%E %@ %f%Lf%Fl[change]%E",
                g->color.alphaF(),
                &TextWindow::ScreenOpacity);
        }

        if(g->type == Group::Type::EXTRUDE || g->type == Group::Type::LATHE ||
           g->type == Group::Type::REVOLVE || g->type == Group::Type::LINKED ||
           g->type == Group::Type::HELIX) {
            Printf(false, "   %Fd%f%LP%s  suppress this group's solid model",
                &TextWindow::ScreenChangeGroupOption,
                g->suppress ? CHECK_TRUE : CHECK_FALSE);
        }

        Printf(false, "");
    }

    Printf(false, " %f%Lv%Fd%s  show entities from this group",
        &TextWindow::ScreenChangeGroupOption,
        g->visible ? CHECK_TRUE : CHECK_FALSE);

    if(!g->IsForcedToMeshBySource()) {
        Printf(false, " %f%Lf%Fd%s  force NURBS surfaces to triangle mesh",
            &TextWindow::ScreenChangeGroupOption,
            g->forceToMesh ? CHECK_TRUE : CHECK_FALSE);
    } else {
        Printf(false, " (model already forced to triangle mesh)");
    }

    Printf(true, " %f%Lr%Fd%s  relax constraints and dimensions",
        &TextWindow::ScreenChangeGroupOption,
        g->relaxConstraints ? CHECK_TRUE : CHECK_FALSE);

    Printf(false, " %f%Le%Fd%s  allow redundant constraints",
        &TextWindow::ScreenChangeGroupOption,
        g->allowRedundant ? CHECK_TRUE : CHECK_FALSE);

    Printf(false, " %f%Ld%Fd%s  treat all dimensions as reference",
        &TextWindow::ScreenChangeGroupOption,
        g->allDimsReference ? CHECK_TRUE : CHECK_FALSE);

    if(g->type == Group::Type::DRAWING_WORKPLANE) {
        Printf(false, " %f%Lx%Fd%s  explode sketch when active",
            &TextWindow::ScreenChangeGroupOption,
            g->explode ? CHECK_TRUE : CHECK_FALSE);
        if(g->explode) {
            Printf(false, "      separate requests by %# %Fl%Ll%f%D[change]%E",
            g->explodeDistance,
            &TextWindow::ScreenChangeGroupExplodeDistance, g->h.v);
        }
    }

    if(g->booleanFailed) {
        Printf(false, "");
        Printf(false, "The Boolean operation failed. It may be ");
        Printf(false, "possible to fix the problem by choosing ");
        Printf(false, "'force NURBS surfaces to triangle mesh'.");
    }

list_items:
    Printf(false, "");
    Printf(false, "%Ft requests in group");

    int a = 0;
    for(auto &r : SK.request) {

        if(r.group == shown.group) {
            std::string s = r.DescriptionString();
            Printf(false, "%Bp   %Fl%Ll%D%f%h%s%E",
                   (a & 1) ? 'd' : 'a',
                   r.h.v,
                   (&TextWindow::ScreenSelectRequest),
                   &(TextWindow::ScreenHoverRequest),
                   s.c_str());
            a++;
        }
    }
    if(a == 0) Printf(false, "%Ba   (none)");

    a = 0;
    Printf(false, "");
    Printf(false, "%Ft constraints in group (%d DOF)", g->solved.dof);
    for(auto &c : SK.constraint) {

        if(c.group == shown.group) {
            std::string s = c.DescriptionString();
            Printf(false, "%Bp   %Fl%Ll%D%f%h%s%E %s",
                   (a & 1) ? 'd' : 'a',
                   c.h.v,
                   (&TextWindow::ScreenSelectConstraint),
                   (&TextWindow::ScreenHoverConstraint),
                   s.c_str(),
                   c.reference ? "(ref)" : "");
            a++;
        }
    }
    if(a == 0) Printf(false, "%Ba   (none)");
}

//-----------------------------------------------------------------------------
// The screen that's displayed when the sketch fails to solve. A report of
// what failed, and (if the problem is a singular Jacobian) a list of
// constraints that could be removed to fix it.
//-----------------------------------------------------------------------------
void TextWindow::ScreenAllowRedundant(int link, uint32_t v) {
    SS.UndoRemember();

    Group *g = SK.GetGroup(SS.TW.shown.group);
    g->allowRedundant = true;
    SS.MarkGroupDirty(SS.TW.shown.group);

    SS.TW.shown.screen = Screen::GROUP_INFO;
    SS.TW.Show();
}
void TextWindow::ShowGroupSolveInfo() {
    Group *g = SK.GetGroup(shown.group);
    if(g->IsSolvedOkay()) {
        // Go back to the default group info screen
        shown.screen = Screen::GROUP_INFO;
        Show();
        return;
    }

    Printf(true, "%FtGROUP   %E%s", g->DescriptionString().c_str());
    switch(g->solved.how) {
        case SolveResult::DIDNT_CONVERGE:
            Printf(true, "%FxSOLVE FAILED!%Fd unsolvable constraints");
            Printf(true, "the following constraints are incompatible");
            break;

        case SolveResult::REDUNDANT_DIDNT_CONVERGE:
            Printf(true, "%FxSOLVE FAILED!%Fd unsolvable constraints");
            Printf(true, "the following constraints are unsatisfied");
            break;

        case SolveResult::REDUNDANT_OKAY:
            Printf(true, "%FxSOLVE FAILED!%Fd redundant constraints");
            Printf(true, "remove any one of these to fix it");
            break;

        case SolveResult::TOO_MANY_UNKNOWNS:
            Printf(true, "Too many unknowns in a single group!");
            return;

        default: ssassert(false, "Unexpected solve result");
    }

    for(int i = 0; i < g->solved.remove.n; i++) {
        hConstraint hc = g->solved.remove[i];
        Constraint *c = SK.constraint.FindByIdNoOops(hc);
        if(!c) continue;

        Printf(false, "%Bp   %Fl%Ll%D%f%h%s%E",
            (i & 1) ? 'd' : 'a',
            c->h.v, (&TextWindow::ScreenSelectConstraint),
            (&TextWindow::ScreenHoverConstraint),
            c->DescriptionString().c_str());
    }

    if(g->solved.timeout) {
        Printf(true,  "%FxSome items in list have been ommitted%Fd");
        Printf(false,  "%Fxbecause the operation timed out.%Fd");
    }

    Printf(true,  "It may be possible to fix the problem ");
    Printf(false, "by selecting Edit -> Undo.");

    if(g->solved.how == SolveResult::REDUNDANT_OKAY) {
        Printf(true,  "It is possible to suppress this error ");
        Printf(false, "by %Fl%f%Llallowing redundant constraints%E in ",
                      &TextWindow::ScreenAllowRedundant);
        Printf(false, "this group.");
    }
}

//-----------------------------------------------------------------------------
// When we're stepping a dimension. User specifies the finish value, and
// how many steps to take in between current and finish, re-solving each
// time.
//-----------------------------------------------------------------------------
void TextWindow::ScreenStepDimFinish(int link, uint32_t v) {
    SS.TW.edit.meaning = Edit::STEP_DIM_FINISH;
    std::string edit_value;
    if(SS.TW.stepDim.isDistance) {
        edit_value = SS.MmToString(SS.TW.stepDim.finish);
    } else {
        edit_value = ssprintf("%.3f", SS.TW.stepDim.finish);
    }
    SS.TW.ShowEditControl(12, edit_value);
}
void TextWindow::ScreenStepDimSteps(int link, uint32_t v) {
    SS.TW.edit.meaning = Edit::STEP_DIM_STEPS;
    SS.TW.ShowEditControl(12, ssprintf("%d", SS.TW.stepDim.steps));
}
void TextWindow::ScreenStepDimGo(int link, uint32_t v) {
    hConstraint hc = SS.TW.shown.constraint;
    Constraint *c = SK.constraint.FindByIdNoOops(hc);
    if(c) {
        SS.UndoRemember();

        double start = c->valA, finish = SS.TW.stepDim.finish;
        SS.TW.stepDim.time = GetMilliseconds();
        SS.TW.stepDim.step = 1;

        if(!SS.TW.stepDim.timer) {
            SS.TW.stepDim.timer = Platform::CreateTimer();
        }
        SS.TW.stepDim.timer->onTimeout = [=] {
            if(SS.TW.stepDim.step <= SS.TW.stepDim.steps) {
                c->valA = start + ((finish - start)*SS.TW.stepDim.step)/SS.TW.stepDim.steps;
                SS.MarkGroupDirty(c->group);
                SS.GenerateAll();
                if(!SS.ActiveGroupsOkay()) {
                    // Failed to solve, so quit
                    return;
                }
                SS.TW.stepDim.step++;

                const int64_t STEP_MILLIS = 50;
                int64_t time = GetMilliseconds();
                if(time - SS.TW.stepDim.time < STEP_MILLIS) {
                    SS.TW.stepDim.timer->RunAfterNextFrame();
                } else {
                    SS.TW.stepDim.timer->RunAfter((unsigned)(time - SS.TW.stepDim.time - STEP_MILLIS));
                }
                SS.TW.stepDim.time = time;
            } else {
                SS.TW.GoToScreen(Screen::LIST_OF_GROUPS);
                SS.ScheduleShowTW();
            }
            SS.GW.Invalidate();
        };
        SS.TW.stepDim.timer->RunAfterNextFrame();
    }
}
void TextWindow::ShowStepDimension() {
    Constraint *c = SK.constraint.FindByIdNoOops(shown.constraint);
    if(!c) {
        shown.screen = Screen::LIST_OF_GROUPS;
        Show();
        return;
    }

    Printf(true, "%FtSTEP DIMENSION%E %s", c->DescriptionString().c_str());

    if(stepDim.isDistance) {
        Printf(true,  "%Ba   %Ftstart%E    %s", SS.MmToString(c->valA).c_str());
        Printf(false, "%Bd   %Ftfinish%E   %s %Fl%Ll%f[change]%E",
            SS.MmToString(stepDim.finish).c_str(), &ScreenStepDimFinish);
    } else {
        Printf(true,  "%Ba   %Ftstart%E    %@", c->valA);
        Printf(false, "%Bd   %Ftfinish%E   %@ %Fl%Ll%f[change]%E",
            stepDim.finish, &ScreenStepDimFinish);
    }
    Printf(false, "%Ba   %Ftsteps%E    %d %Fl%Ll%f%D[change]%E",
        stepDim.steps, &ScreenStepDimSteps);

    Printf(true, " %Fl%Ll%fstep dimension now%E", &ScreenStepDimGo);

    Printf(true, "(or %Fl%Ll%fcancel operation%E)", &ScreenHome);
}

//-----------------------------------------------------------------------------
// When we're creating tangent arcs (as requests, not as some parametric
// thing). User gets to specify the radius, and whether the old untrimmed
// curves are kept or deleted.
//-----------------------------------------------------------------------------
void TextWindow::ScreenChangeTangentArc(int link, uint32_t v) {
    switch(link) {
        case 'r': {
            SS.TW.edit.meaning = Edit::TANGENT_ARC_RADIUS;
            SS.TW.ShowEditControl(3, SS.MmToString(SS.tangentArcRadius));
            break;
        }

        case 'a': SS.tangentArcManual = !SS.tangentArcManual; break;
        case 'm': SS.tangentArcModify = !SS.tangentArcModify; break;
    }
}
void TextWindow::ShowTangentArc() {
    Printf(true, "%FtTANGENT ARC PARAMETERS%E");

    Printf(true,  "%Ft radius of created arc%E");
    if(SS.tangentArcManual) {
        Printf(false, "%Ba   %s %Fl%Lr%f[change]%E",
            SS.MmToString(SS.tangentArcRadius).c_str(),
            &(TextWindow::ScreenChangeTangentArc));
    } else {
        Printf(false, "%Ba   automatic");
    }

    Printf(false, "");
    Printf(false, "  %Fd%f%La%s  choose radius automatically%E",
        &ScreenChangeTangentArc,
        !SS.tangentArcManual ? CHECK_TRUE : CHECK_FALSE);
    Printf(false, "  %Fd%f%Lm%s  modify original entities%E",
        &ScreenChangeTangentArc,
        SS.tangentArcModify ? CHECK_TRUE : CHECK_FALSE);

    Printf(false, "");
    Printf(false, "To create a tangent arc at a point,");
    Printf(false, "select that point and then choose");
    Printf(false, "Sketch -> Tangent Arc at Point.");
    Printf(true, "(or %Fl%Ll%fback to home screen%E)", &ScreenHome);
}

//-----------------------------------------------------------------------------
// The edit control is visible, and the user just pressed enter.
//-----------------------------------------------------------------------------
void TextWindow::EditControlDone(std::string s) {
    edit.showAgain = false;

    switch(edit.meaning) {
        case Edit::TIMES_REPEATED:
            if(Expr *e = Expr::From(s, /*popUpError=*/true)) {
                SS.UndoRemember();

                double ev = e->Eval();
                if((int)ev < 1) {
                    Error(_("Can't repeat fewer than 1 time."));
                    break;
                }
                if((int)ev > 999) {
                    Error(_("Can't repeat more than 999 times."));
                    break;
                }

                Group *g = SK.GetGroup(edit.group);
                g->valA = ev;

                if(g->type == Group::Type::ROTATE) {
                    // If the group does not contain any constraints, then
                    // set the numerical guess to space the copies uniformly
                    // over one rotation. Don't touch the guess if we're
                    // already constrained, because that would break
                    // convergence.
                    if(g->GetNumConstraints() == 0) {
                        double copies = (g->skipFirst) ? (ev + 1) : ev;
                        SK.GetParam(g->h.param(3))->val = PI/(2*copies);
                    }
                }

                SS.MarkGroupDirty(g->h);
            }
            break;

        case Edit::GROUP_NAME:
            if(s.empty()) {
                Error(_("Group name cannot be empty"));
            } else {
                SS.UndoRemember();

                Group *g = SK.GetGroup(edit.group);
                g->name = s;
            }
            break;

        case Edit::GROUP_SCALE:
            if(Expr *e = Expr::From(s, /*popUpError=*/true)) {
                double ev = e->Eval();
                if(fabs(ev) < 1e-6) {
                    Error(_("Scale cannot be zero."));
                } else {
                    Group *g = SK.GetGroup(edit.group);
                    g->scale = ev;
                    SS.MarkGroupDirty(g->h);
                }
            }
            break;

        case Edit::GROUP_COLOR: {
            Vector rgb;
            if(sscanf(s.c_str(), "%lf, %lf, %lf", &rgb.x, &rgb.y, &rgb.z)==3) {
                rgb = rgb.ClampWithin(0, 1);

                Group *g = SK.group.FindByIdNoOops(SS.TW.shown.group);
                if(!g) break;
                g->color = RgbaColor::FromFloat((float)rgb.x, (float)rgb.y, (float)rgb.z,
                                                g->color.alphaF());

                SS.MarkGroupDirty(g->h);
                SS.GW.ClearSuper();
            } else {
                Error(_("Bad format: specify color as r, g, b"));
            }
            break;
        }
        case Edit::GROUP_OPACITY:
            if(Expr *e = Expr::From(s, /*popUpError=*/true)) {
                double alpha = e->Eval();
                if(alpha < 0 || alpha > 1) {
                    Error(_("Opacity must be between zero and one."));
                } else {
                    Group *g = SK.GetGroup(edit.group);
                    g->color.alpha = (int)(255.1f * alpha);
                    SS.MarkGroupDirty(g->h);
                    SS.GW.ClearSuper();
                }
            }
            break;

        case Edit::GROUP_EXPLODE_DISTANCE:
            if(Expr *e = Expr::From(s, /*popUpError=*/true)) {
                double ev = e->Eval();
                if(ev < 1e-6) {
                    Error(_("Explode separation must be above zero."));
                } else {
                    Group *g = SK.GetGroup(edit.group);
                    g->explodeDistance = ev;
                    SS.MarkGroupDirty(g->h);
                }
            }
            break;

        case Edit::TTF_TEXT:
            SS.UndoRemember();
            if(Request *r = SK.request.FindByIdNoOops(edit.request)) {
                r->str = s;
                SS.MarkGroupDirty(r->group);
            }
            break;

        case Edit::STEP_DIM_FINISH:
            if(Expr *e = Expr::From(s, /*popUpError=*/true)) {
                if(stepDim.isDistance) {
                    stepDim.finish = SS.ExprToMm(e);
                } else {
                    stepDim.finish = e->Eval();
                }
            }
            break;

        case Edit::STEP_DIM_STEPS:
            stepDim.steps = min(300, max(1, atoi(s.c_str())));
            break;

        case Edit::TANGENT_ARC_RADIUS:
            if(Expr *e = Expr::From(s, /*popUpError=*/true)) {
                if(e->Eval() < LENGTH_EPS) {
                    Error(_("Radius cannot be zero or negative."));
                    break;
                }
                SS.tangentArcRadius = SS.ExprToMm(e);
            }
            break;

        default: {
            int cnt = 0;
            if(EditControlDoneForStyles(s))         cnt++;
            if(EditControlDoneForConfiguration(s))  cnt++;
            if(EditControlDoneForPaste(s))          cnt++;
            if(EditControlDoneForView(s))           cnt++;
            ssassert(cnt == 1, "Expected exactly one parameter to be edited");
            break;
        }
    }
    SS.GW.Invalidate();
    SS.ScheduleShowTW();

    if(!edit.showAgain) {
        HideEditControl();
        edit.meaning = Edit::NOTHING;
    }
}

