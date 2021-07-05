//-----------------------------------------------------------------------------
// Anything relating to mouse, keyboard, or 6-DOF mouse input.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

void GraphicsWindow::UpdateDraggedPoint(hEntity hp, double mx, double my) {
    Entity *p = SK.GetEntity(hp);
    Vector pos = p->PointGetNum();
    UpdateDraggedNum(&pos, mx, my);
    p->PointForceTo(pos);

    SS.ScheduleShowTW();
}

void GraphicsWindow::UpdateDraggedNum(Vector *pos, double mx, double my) {
    *pos = pos->Plus(projRight.ScaledBy((mx - orig.mouse.x)/scale));
    *pos = pos->Plus(projUp.ScaledBy((my - orig.mouse.y)/scale));
}

void GraphicsWindow::AddPointToDraggedList(hEntity hp) {
    Entity *p = SK.GetEntity(hp);
    // If an entity and its points are both selected, then its points could
    // end up in the list twice. This would be bad, because it would move
    // twice as far as the mouse pointer...
    List<hEntity> *lhe = &(pending.points);
    for(hEntity *hee = lhe->First(); hee; hee = lhe->NextAfter(hee)) {
        if(*hee == hp) {
            // Exact same point.
            return;
        }
        Entity *pe = SK.GetEntity(*hee);
        if(pe->type == p->type &&
           pe->type != Entity::Type::POINT_IN_2D &&
           pe->type != Entity::Type::POINT_IN_3D &&
           pe->group == p->group)
        {
            // Transform-type point, from the same group. So it handles the
            // same unknowns.
            return;
        }
    }
    pending.points.Add(&hp);
}

void GraphicsWindow::StartDraggingByEntity(hEntity he) {
    Entity *e = SK.GetEntity(he);
    if(e->IsPoint()) {
        AddPointToDraggedList(e->h);
    } else if(e->type == Entity::Type::LINE_SEGMENT ||
              e->type == Entity::Type::ARC_OF_CIRCLE ||
              e->type == Entity::Type::CUBIC ||
              e->type == Entity::Type::CUBIC_PERIODIC ||
              e->type == Entity::Type::CIRCLE ||
              e->type == Entity::Type::TTF_TEXT ||
              e->type == Entity::Type::IMAGE)
    {
        int pts;
        EntReqTable::GetEntityInfo(e->type, e->extraPoints,
            NULL, &pts, NULL, NULL);
        for(int i = 0; i < pts; i++) {
            AddPointToDraggedList(e->point[i]);
        }
    }
}

void GraphicsWindow::StartDraggingBySelection() {
    List<Selection> *ls = &(selection);
    for(Selection *s = ls->First(); s; s = ls->NextAfter(s)) {
        if(!s->entity.v) continue;

        StartDraggingByEntity(s->entity);
    }
    // The user might select a point, and then click it again to start
    // dragging; but the point just got unselected by that click. So drag
    // the hovered item too, and they'll always have it.
    if(hover.entity.v) {
        hEntity dragEntity = ChooseFromHoverToDrag().entity;
        if(dragEntity != Entity::NO_ENTITY) {
            StartDraggingByEntity(dragEntity);
        }
    }
}

void GraphicsWindow::MouseMoved(double x, double y, bool leftDown,
            bool middleDown, bool rightDown, bool shiftDown, bool ctrlDown)
{
    if(window->IsEditorVisible()) return;
    if(context.active) return;

    SS.extraLine.draw = false;

    if(!orig.mouseDown) {
        // If someone drags the mouse into our window with the left button
        // already depressed, then we don't have our starting point; so
        // don't try.
        leftDown = false;
    }

    if(rightDown) {
        middleDown = true;
        shiftDown = !shiftDown;
    }

    if(SS.showToolbar) {
        if(ToolbarMouseMoved((int)x, (int)y)) {
            hover.Clear();
            return;
        }
    }

    if(!leftDown && (pending.operation == Pending::DRAGGING_POINTS ||
                     pending.operation == Pending::DRAGGING_MARQUEE))
    {
        ClearPending();
        Invalidate();
    }

    Point2d mp = Point2d::From(x, y);
    currentMousePosition = mp;

    if(rightDown && orig.mouse.DistanceTo(mp) < 5 && !orig.startedMoving) {
        // Avoid accidentally panning (or rotating if shift is down) if the
        // user wants a context menu.
        return;
    }
    orig.startedMoving = true;

    // If the middle button is down, then mouse movement is used to pan and
    // rotate our view. This wins over everything else.
    if(middleDown) {
        hover.Clear();

        double dx = (x - orig.mouse.x) / scale;
        double dy = (y - orig.mouse.y) / scale;

        if(!(shiftDown || ctrlDown)) {
            double sign = SS.cameraNav ? -1.0 : 1.0;
            double s = 0.3*(PI/180)*scale*sign; // degrees per pixel
            if(SS.turntableNav) {               // lock the Z to vertical
                projRight = orig.projRight.RotatedAbout(Vector::From(0, 0, 1), -s * dx);
                projUp    = orig.projUp.RotatedAbout(
                    Vector::From(orig.projRight.x, orig.projRight.y, orig.projRight.y), s * dy);
            } else {
                projRight = orig.projRight.RotatedAbout(orig.projUp, -s * dx);
                projUp    = orig.projUp.RotatedAbout(orig.projRight, s * dy);
            }

            NormalizeProjectionVectors();
        } else if(ctrlDown) {
            double theta = atan2(orig.mouse.y, orig.mouse.x);
            theta -= atan2(y, x);
            SS.extraLine.draw = true;
            SS.extraLine.ptA = UnProjectPoint(Point2d::From(0, 0));
            SS.extraLine.ptB = UnProjectPoint(mp);

            Vector normal = orig.projRight.Cross(orig.projUp);
            projRight = orig.projRight.RotatedAbout(normal, theta);
            projUp = orig.projUp.RotatedAbout(normal, theta);

            NormalizeProjectionVectors();
        } else {
            offset.x = orig.offset.x + dx*projRight.x + dy*projUp.x;
            offset.y = orig.offset.y + dx*projRight.y + dy*projUp.y;
            offset.z = orig.offset.z + dx*projRight.z + dy*projUp.z;
        }

        orig.projRight = projRight;
        orig.projUp = projUp;
        orig.offset = offset;
        orig.mouse.x = x;
        orig.mouse.y = y;

        if(SS.TW.shown.screen == TextWindow::Screen::EDIT_VIEW) {
            if(havePainted) {
                SS.ScheduleShowTW();
            }
        }
        Invalidate();
        havePainted = false;
        return;
    }

    if(pending.operation == Pending::NONE) {
        double dm = orig.mouse.DistanceTo(mp);
        // If we're currently not doing anything, then see if we should
        // start dragging something.
        if(leftDown && dm > 3) {
            Entity *e = NULL;
            hEntity dragEntity = ChooseFromHoverToDrag().entity;
            if(dragEntity.v) e = SK.GetEntity(dragEntity);
            if(e && e->type != Entity::Type::WORKPLANE) {
                if(!hoverWasSelectedOnMousedown) {
                    // The user clicked an unselected entity, which
                    // means they're dragging just the hovered thing,
                    // not the full selection. So clear all the selection
                    // except that entity.
                    ClearSelection();
                    MakeSelected(dragEntity);
                }
                if(e->type == Entity::Type::CIRCLE && selection.n <= 1) {
                    // Drag the radius.
                    pending.circle = dragEntity;
                    pending.operation = Pending::DRAGGING_RADIUS;
                } else if(e->IsNormal()) {
                    pending.normal = dragEntity;
                    pending.operation = Pending::DRAGGING_NORMAL;
                } else {
                    StartDraggingBySelection();
                    hover.Clear();
                    pending.operation = Pending::DRAGGING_POINTS;
                }
            } else if(hover.constraint.v &&
                            SK.GetConstraint(hover.constraint)->HasLabel())
            {
                ClearSelection();
                pending.constraint = hover.constraint;
                pending.operation = Pending::DRAGGING_CONSTRAINT;
            }
            if(pending.operation != Pending::NONE) {
                // We just started a drag, so remember for the undo before
                // the drag changes anything.
                SS.UndoRemember();
            } else {
                if(!hover.constraint.v) {
                    // That's just marquee selection, which should not cause
                    // an undo remember.
                    if(dm > 10) {
                        if(hover.entity.v) {
                            // Avoid accidentally selecting workplanes when
                            // starting drags.
                            MakeUnselected(hover.entity, /*coincidentPointTrick=*/false);
                            hover.Clear();
                        }
                        pending.operation = Pending::DRAGGING_MARQUEE;
                        orig.marqueePoint =
                            UnProjectPoint(orig.mouseOnButtonDown);
                    }
                }
            }
        } else {
            // Otherwise, just hit test and give up; but don't hit test
            // if the mouse is down, because then the user could hover
            // a point, mouse down (thus selecting it), and drag, in an
            // effort to drag the point, but instead hover a different
            // entity before we move far enough to start the drag.
            if(!leftDown) {
                // Hit testing can potentially take a lot of time.
                // If we haven't painted since last time we highlighted
                // something, don't hit test again, since this just causes
                // a lag.
                if(!havePainted) return;
                HitTestMakeSelection(mp);
            }
        }
        return;
    }

    // If the user has started an operation from the menu, but not
    // completed it, then just do the selection.
    if(pending.operation == Pending::COMMAND) {
        HitTestMakeSelection(mp);
        return;
    }

    if(pending.operation == Pending::DRAGGING_POINTS && ctrlDown) {
        SS.extraLine.ptA = UnProjectPoint(orig.mouseOnButtonDown);
        SS.extraLine.ptB = UnProjectPoint(mp);
        SS.extraLine.draw = true;
    }

    // We're currently dragging something; so do that. But if we haven't
    // painted since the last time we solved, do nothing, because there's
    // no sense solving a frame and not displaying it.
    if(!havePainted) {
        return;
    }

    havePainted = false;
    switch(pending.operation) {
        case Pending::DRAGGING_CONSTRAINT: {
            Constraint *c = SK.constraint.FindById(pending.constraint);
            UpdateDraggedNum(&(c->disp.offset), x, y);
            orig.mouse = mp;
            Invalidate();
            return;
        }

        case Pending::DRAGGING_NEW_LINE_POINT:
            if(!ctrlDown) {
                SS.GW.pending.hasSuggestion =
                    SS.GW.SuggestLineConstraint(SS.GW.pending.request, &SS.GW.pending.suggestion);
            } else {
                SS.GW.pending.hasSuggestion = false;
            }
            // fallthrough
        case Pending::DRAGGING_NEW_POINT:
            UpdateDraggedPoint(pending.point, x, y);
            HitTestMakeSelection(mp);
            SS.MarkGroupDirtyByEntity(pending.point);
            orig.mouse = mp;
            break;

        case Pending::DRAGGING_POINTS:
            if(shiftDown || ctrlDown) {
                // Edit the rotation associated with a POINT_N_ROT_TRANS,
                // either within (ctrlDown) or out of (shiftDown) the plane
                // of the screen. So first get the rotation to apply, in qt.
                Quaternion qt;
                if(ctrlDown) {
                    double d = mp.DistanceTo(orig.mouseOnButtonDown);
                    if(d < 25) {
                        // Don't start dragging the position about the normal
                        // until we're a little ways out, to get a reasonable
                        // reference pos
                        qt = Quaternion::IDENTITY;
                    } else {
                        double theta = atan2(orig.mouse.y-orig.mouseOnButtonDown.y,
                                             orig.mouse.x-orig.mouseOnButtonDown.x);
                        theta -= atan2(y-orig.mouseOnButtonDown.y,
                                       x-orig.mouseOnButtonDown.x);

                        Vector gn = projRight.Cross(projUp);
                        qt = Quaternion::From(gn, -theta);
                    }
                } else {
                    double dx = -(x - orig.mouse.x);
                    double dy = -(y - orig.mouse.y);
                    double s = 0.3*(PI/180); // degrees per pixel
                    qt = Quaternion::From(projUp,   -s*dx).Times(
                         Quaternion::From(projRight, s*dy));
                }

                // Now apply this rotation to the points being dragged.
                List<hEntity> *lhe = &(pending.points);
                for(hEntity *he = lhe->First(); he; he = lhe->NextAfter(he)) {
                    Entity *e = SK.GetEntity(*he);
                    if(e->type != Entity::Type::POINT_N_ROT_TRANS) {
                        if(ctrlDown) {
                            Vector p = e->PointGetNum();
                            p = p.Minus(SS.extraLine.ptA);
                            p = qt.Rotate(p);
                            p = p.Plus(SS.extraLine.ptA);
                            e->PointForceTo(p);
                        } else {
                            UpdateDraggedPoint(*he, x, y);
                        }
                    } else {
                        Quaternion q = e->PointGetQuaternion();
                        Vector     p = e->PointGetNum();
                        q = qt.Times(q);
                        e->PointForceQuaternionTo(q);
                        // Let's rotate about the selected point; so fix up the
                        // translation so that that point didn't move.
                        e->PointForceTo(p);
                    }
                    SS.MarkGroupDirtyByEntity(e->h);
                }
            } else {
                List<hEntity> *lhe = &(pending.points);
                for(hEntity *he = lhe->First(); he; he = lhe->NextAfter(he)) {
                    UpdateDraggedPoint(*he, x, y);
                    SS.MarkGroupDirtyByEntity(*he);
                }
            }
            orig.mouse = mp;
            break;

        case Pending::DRAGGING_NEW_CUBIC_POINT: {
            UpdateDraggedPoint(pending.point, x, y);
            HitTestMakeSelection(mp);

            hRequest hr = pending.point.request();
            if(pending.point == hr.entity(4)) {
                // The very first segment; dragging final point drags both
                // tangent points.
                Vector p0 = SK.GetEntity(hr.entity(1))->PointGetNum(),
                       p3 = SK.GetEntity(hr.entity(4))->PointGetNum(),
                       p1 = p0.ScaledBy(2.0/3).Plus(p3.ScaledBy(1.0/3)),
                       p2 = p0.ScaledBy(1.0/3).Plus(p3.ScaledBy(2.0/3));
                SK.GetEntity(hr.entity(1+1))->PointForceTo(p1);
                SK.GetEntity(hr.entity(1+2))->PointForceTo(p2);
            } else {
                // A subsequent segment; dragging point drags only final
                // tangent point.
                int i = SK.GetEntity(hr.entity(0))->extraPoints;
                Vector pn   = SK.GetEntity(hr.entity(4+i))->PointGetNum(),
                       pnm2 = SK.GetEntity(hr.entity(2+i))->PointGetNum(),
                       pnm1 = (pn.Plus(pnm2)).ScaledBy(0.5);
                SK.GetEntity(hr.entity(3+i))->PointForceTo(pnm1);
            }

            orig.mouse = mp;
            SS.MarkGroupDirtyByEntity(pending.point);
            break;
        }
        case Pending::DRAGGING_NEW_ARC_POINT: {
            UpdateDraggedPoint(pending.point, x, y);
            HitTestMakeSelection(mp);

            hRequest hr = pending.point.request();
            Vector ona = SK.GetEntity(hr.entity(2))->PointGetNum();
            Vector onb = SK.GetEntity(hr.entity(3))->PointGetNum();
            Vector center = (ona.Plus(onb)).ScaledBy(0.5);

            SK.GetEntity(hr.entity(1))->PointForceTo(center);

            orig.mouse = mp;
            SS.MarkGroupDirtyByEntity(pending.point);
            break;
        }
        case Pending::DRAGGING_NEW_RADIUS:
        case Pending::DRAGGING_RADIUS: {
            Entity *circle = SK.GetEntity(pending.circle);
            Vector center = SK.GetEntity(circle->point[0])->PointGetNum();
            Point2d c2 = ProjectPoint(center);
            double r = c2.DistanceTo(mp)/scale;
            SK.GetEntity(circle->distance)->DistanceForceTo(r);

            SS.MarkGroupDirtyByEntity(pending.circle);
            SS.ScheduleShowTW();
            break;
        }

        case Pending::DRAGGING_NORMAL: {
            Entity *normal = SK.GetEntity(pending.normal);
            Vector p = SK.GetEntity(normal->point[0])->PointGetNum();
            Point2d p2 = ProjectPoint(p);

            Quaternion q = normal->NormalGetNum();
            Vector u = q.RotationU(), v = q.RotationV();

            if(ctrlDown) {
                double theta = atan2(orig.mouse.y-p2.y, orig.mouse.x-p2.x);
                theta -= atan2(y-p2.y, x-p2.x);

                Vector normal = projRight.Cross(projUp);
                u = u.RotatedAbout(normal, -theta);
                v = v.RotatedAbout(normal, -theta);
            } else {
                double dx = -(x - orig.mouse.x);
                double dy = -(y - orig.mouse.y);
                double s = 0.3*(PI/180); // degrees per pixel
                u = u.RotatedAbout(projUp, -s*dx);
                u = u.RotatedAbout(projRight, s*dy);
                v = v.RotatedAbout(projUp, -s*dx);
                v = v.RotatedAbout(projRight, s*dy);
            }
            orig.mouse = mp;
            normal->NormalForceTo(Quaternion::From(u, v));

            SS.MarkGroupDirtyByEntity(pending.normal);
            break;
        }

        case Pending::DRAGGING_MARQUEE:
            orig.mouse = mp;
            Invalidate();
            return;

        case Pending::NONE:
        case Pending::COMMAND:
            ssassert(false, "Unexpected pending operation");
    }
}

void GraphicsWindow::ClearPending(bool scheduleShowTW) {
    pending.points.Clear();
    pending.requests.Clear();
    pending = {};
    if(scheduleShowTW) {
        SS.ScheduleShowTW();
    }
}

bool GraphicsWindow::IsFromPending(hRequest r) {
    for(auto &req : pending.requests) {
        if(req == r) return true;
    }
    return false;
}

void GraphicsWindow::AddToPending(hRequest r) {
    pending.requests.Add(&r);
}

void GraphicsWindow::ReplacePending(hRequest before, hRequest after) {
    for(auto &req : pending.requests) {
        if(req == before) {
            req = after;
        }
    }
}

void GraphicsWindow::MouseMiddleOrRightDown(double x, double y) {
    if(window->IsEditorVisible()) return;

    orig.offset = offset;
    orig.projUp = projUp;
    orig.projRight = projRight;
    orig.mouse.x = x;
    orig.mouse.y = y;
    orig.startedMoving = false;
}

void GraphicsWindow::MouseRightUp(double x, double y) {
    SS.extraLine.draw = false;
    Invalidate();

    // Don't show a context menu if the user is right-clicking the toolbar,
    // or if they are finishing a pan.
    if(ToolbarMouseMoved((int)x, (int)y)) return;
    if(orig.startedMoving) return;

    if(context.active) return;

    if(pending.operation == Pending::DRAGGING_NEW_LINE_POINT && pending.hasSuggestion) {
        Constraint::TryConstrain(SS.GW.pending.suggestion,
            Entity::NO_ENTITY, Entity::NO_ENTITY, pending.request.entity(0));
    }

    if(pending.operation == Pending::DRAGGING_NEW_LINE_POINT ||
       pending.operation == Pending::DRAGGING_NEW_CUBIC_POINT ||
       pending.operation == Pending::DRAGGING_NEW_ARC_POINT ||
       pending.operation == Pending::DRAGGING_NEW_RADIUS ||
       pending.operation == Pending::DRAGGING_NEW_POINT
       )
    {
        // Special case; use a right click to stop drawing lines, since
        // a left click would draw another one. This is quicker and more
        // intuitive than hitting escape. Likewise for other entities
        // for consistency.
        ClearPending();
        return;
    }

    // The current mouse location
    Vector v = offset.ScaledBy(-1);
    v = v.Plus(projRight.ScaledBy(x/scale));
    v = v.Plus(projUp.ScaledBy(y/scale));

    Platform::MenuRef menu = Platform::CreateMenu();
    context.active = true;

    if(!hover.IsEmpty()) {
        MakeSelected(&hover);
        SS.ScheduleShowTW();
    }
    GroupSelection();

    bool itemsSelected = (gs.n > 0 || gs.constraints > 0);
    if(itemsSelected) {
        if(gs.stylables > 0) {
            Platform::MenuRef styleMenu = menu->AddSubMenu(_("Assign to Style"));

            bool empty = true;
            for(const Style &s : SK.style) {
                if(s.h.v < Style::FIRST_CUSTOM) continue;

                uint32_t v = s.h.v;

                styleMenu->AddItem(s.DescriptionString(), [v]() {
                    Style::AssignSelectionToStyle(v);
                });
                empty = false;
            }

            if(!empty) styleMenu->AddSeparator();

            styleMenu->AddItem(_("No Style"), []() {
                Style::AssignSelectionToStyle(0);
            });
            styleMenu->AddItem(_("Newly Created Custom Style..."), [this]() {
                uint32_t vs = Style::CreateCustomStyle();
                Style::AssignSelectionToStyle(vs);
                ForceTextWindowShown();
            });
        }
        if(gs.n + gs.constraints == 1) {
            menu->AddItem(_("Group Info"), [this]() {
                hGroup hg;
                if(gs.entities == 1) {
                    hg = SK.GetEntity(gs.entity[0])->group;
                } else if(gs.points == 1) {
                    hg = SK.GetEntity(gs.point[0])->group;
                } else if(gs.constraints == 1) {
                    hg = SK.GetConstraint(gs.constraint[0])->group;
                } else {
                    return;
                }
                ClearSelection();

                SS.TW.GoToScreen(TextWindow::Screen::GROUP_INFO);
                SS.TW.shown.group = hg;
                SS.ScheduleShowTW();
                ForceTextWindowShown();
            });
        }
        if(gs.n + gs.constraints == 1 && gs.stylables == 1) {
            menu->AddItem(_("Style Info"), [this]() {
                hStyle hs;
                if(gs.entities == 1) {
                    hs = Style::ForEntity(gs.entity[0]);
                } else if(gs.points == 1) {
                    hs = Style::ForEntity(gs.point[0]);
                } else if(gs.constraints == 1) {
                    hs = SK.GetConstraint(gs.constraint[0])->GetStyle();
                } else {
                    return;
                }
                ClearSelection();

                SS.TW.GoToScreen(TextWindow::Screen::STYLE_INFO);
                SS.TW.shown.style = hs;
                SS.ScheduleShowTW();
                ForceTextWindowShown();
            });
        }
        if(gs.withEndpoints > 0) {
            menu->AddItem(_("Select Edge Chain"),
                []() { MenuEdit(Command::SELECT_CHAIN); });
        }
        if(gs.constraints == 1 && gs.n == 0) {
            Constraint *c = SK.GetConstraint(gs.constraint[0]);
            if(c->HasLabel() && c->type != Constraint::Type::COMMENT) {
                menu->AddItem(_("Toggle Reference Dimension"),
                    []() { Constraint::MenuConstrain(Command::REFERENCE); });
            }
            if(c->type == Constraint::Type::ANGLE ||
                c->type == Constraint::Type::EQUAL_ANGLE)
            {
                menu->AddItem(_("Other Supplementary Angle"),
                    []() { Constraint::MenuConstrain(Command::OTHER_ANGLE); });
            }
        }
        if(gs.constraintLabels > 0 || gs.points > 0) {
            menu->AddItem(_("Snap to Grid"),
                []() { MenuEdit(Command::SNAP_TO_GRID); });
        }

        if(gs.points == 1 && gs.point[0].isFromRequest()) {
            Request *r = SK.GetRequest(gs.point[0].request());
            int index = r->IndexOfPoint(gs.point[0]);
            if((r->type == Request::Type::CUBIC && (index > 1 && index < r->extraPoints + 2)) ||
                    r->type == Request::Type::CUBIC_PERIODIC) {
                menu->AddItem(_("Remove Spline Point"), [this, r]() {
                    int index = r->IndexOfPoint(gs.point[0]);
                    ssassert(r->extraPoints != 0,
                             "Expected a bezier with interior control points");

                    SS.UndoRemember();
                    Entity *e = SK.GetEntity(r->h.entity(0));

                    // First, fix point-coincident constraints involving this point.
                    // Then, remove all other constraints, since they would otherwise
                    // jump to an adjacent one and mess up the bezier after generation.
                    FixConstraintsForPointBeingDeleted(e->point[index]);
                    RemoveConstraintsForPointBeingDeleted(e->point[index]);

                    for(int i = index; i < MAX_POINTS_IN_ENTITY - 1; i++) {
                        if(e->point[i + 1].v == 0) break;
                        Entity *p0 = SK.GetEntity(e->point[i]);
                        Entity *p1 = SK.GetEntity(e->point[i + 1]);
                        ReplacePointInConstraints(p1->h, p0->h);
                        p0->PointForceTo(p1->PointGetNum());
                    }
                    r->extraPoints--;
                    SS.MarkGroupDirtyByEntity(gs.point[0]);
                    ClearSelection();
                });
            }
        }
        if(gs.entities == 1 && gs.entity[0].isFromRequest()) {
            Request *r = SK.GetRequest(gs.entity[0].request());
            if(r->type == Request::Type::CUBIC || r->type == Request::Type::CUBIC_PERIODIC) {
                Entity *e = SK.GetEntity(gs.entity[0]);
                int addAfterPoint = e->GetPositionOfPoint(GetCamera(), Point2d::From(x, y));
                ssassert(addAfterPoint != -1, "Expected a nearest bezier point to be located");
                // Skip derivative point.
                if(r->type == Request::Type::CUBIC) addAfterPoint++;
                menu->AddItem(_("Add Spline Point"), [this, r, addAfterPoint, v]() {
                    int pointCount = r->extraPoints +
                                     ((r->type == Request::Type::CUBIC_PERIODIC) ? 3 : 4);
                    if(pointCount >= MAX_POINTS_IN_ENTITY) {
                        Error(_("Cannot add spline point: maximum number of points reached."));
                        return;
                    }

                    SS.UndoRemember();
                    r->extraPoints++;
                    SS.MarkGroupDirtyByEntity(gs.entity[0]);
                    SS.GenerateAll(SolveSpaceUI::Generate::REGEN);

                    Entity *e = SK.GetEntity(r->h.entity(0));
                    for(int i = MAX_POINTS_IN_ENTITY; i > addAfterPoint + 1; i--) {
                        Entity *p0 = SK.entity.FindByIdNoOops(e->point[i]);
                        if(p0 == NULL) continue;
                        Entity *p1 = SK.GetEntity(e->point[i - 1]);
                        ReplacePointInConstraints(p1->h, p0->h);
                        p0->PointForceTo(p1->PointGetNum());
                    }
                    Entity *p = SK.GetEntity(e->point[addAfterPoint + 1]);
                    p->PointForceTo(v);
                    SS.MarkGroupDirtyByEntity(gs.entity[0]);
                    ClearSelection();
                });
            }
        }
        if(gs.entities == gs.n) {
            menu->AddItem(_("Toggle Construction"),
                []() { MenuRequest(Command::CONSTRUCTION); });
        }

        if(gs.points == 1) {
            Entity *p = SK.GetEntity(gs.point[0]);
            Constraint *c = nullptr;
            IdList<Constraint,hConstraint> *lc = &(SK.constraint);
            for(Constraint &ci : *lc) {
                if(ci.type != Constraint::Type::POINTS_COINCIDENT) continue;
                if(ci.ptA == p->h || ci.ptB == p->h) {
                    c = &ci;
                    break;
                }
            }
            if(c) {
                menu->AddItem(_("Delete Point-Coincident Constraint"), [this, p]() {
                    if(!p->IsPoint()) return;

                    SS.UndoRemember();
                    SK.constraint.ClearTags();
                    for(Constraint &c : SK.constraint) {
                        if(c.type != Constraint::Type::POINTS_COINCIDENT) continue;
                        if(c.ptA == p->h || c.ptB == p->h) {
                            c.tag = 1;
                        }
                    }
                    SK.constraint.RemoveTagged();
                    ClearSelection();
                });
            }
        }
        menu->AddSeparator();
        if(LockedInWorkplane()) {
            menu->AddItem(_("Cut"),
                []() { MenuClipboard(Command::CUT); });
            menu->AddItem(_("Copy"),
                []() { MenuClipboard(Command::COPY); });
        }
    } else {
        menu->AddItem(_("Select All"),
            []() { MenuEdit(Command::SELECT_ALL); });
    }

    if((!SS.clipboard.r.IsEmpty() || !SS.clipboard.c.IsEmpty()) && LockedInWorkplane()) {
        menu->AddItem(_("Paste"),
            []() { MenuClipboard(Command::PASTE); });
        menu->AddItem(_("Paste Transformed..."),
            []() { MenuClipboard(Command::PASTE_TRANSFORM); });
    }

    if(itemsSelected) {
        menu->AddItem(_("Delete"),
            []() { MenuClipboard(Command::DELETE); });
        menu->AddSeparator();
        menu->AddItem(_("Unselect All"),
            []() { MenuEdit(Command::UNSELECT_ALL); });
    }
    // If only one item is selected, then it must be the one that we just
    // selected from the hovered item; in which case unselect all and hovered
    // are equivalent.
    if(!hover.IsEmpty() && selection.n > 1) {
        menu->AddItem(_("Unselect Hovered"), [this] {
            if(!hover.IsEmpty()) {
                MakeUnselected(&hover, /*coincidentPointTrick=*/true);
            }
        });
    }

    if(itemsSelected) {
        menu->AddSeparator();
        menu->AddItem(_("Zoom to Fit"),
            []() { MenuView(Command::ZOOM_TO_FIT); });
    }

    menu->PopUp();

    context.active = false;
    SS.ScheduleShowTW();
}

hRequest GraphicsWindow::AddRequest(Request::Type type) {
    return AddRequest(type, /*rememberForUndo=*/true);
}
hRequest GraphicsWindow::AddRequest(Request::Type type, bool rememberForUndo) {
    if(rememberForUndo) SS.UndoRemember();

    Request r = {};
    r.group = activeGroup;
    Group *g = SK.GetGroup(activeGroup);
    if(g->type == Group::Type::DRAWING_3D || g->type == Group::Type::DRAWING_WORKPLANE) {
        r.construction = false;
    } else {
        r.construction = true;
    }
    r.workplane = ActiveWorkplane();
    r.type = type;
    SK.request.AddAndAssignId(&r);

    // We must regenerate the parameters, so that the code that tries to
    // place this request's entities where the mouse is can do so. But
    // we mustn't try to solve until reasonable values have been supplied
    // for these new parameters, or else we'll get a numerical blowup.
    r.Generate(&SK.entity, &SK.param);
    SS.MarkGroupDirty(r.group);
    return r.h;
}

Vector GraphicsWindow::SnapToEntityByScreenPoint(Point2d pp, hEntity he) {
    Entity *e = SK.GetEntity(he);
    if(e->IsPoint()) return e->PointGetNum();
    SEdgeList *edges = e->GetOrGenerateEdges();

    double minD = -1.0f;
    double k;
    const SEdge *edge = NULL;
    for(const auto &e : edges->l) {
        Point2d p0 = ProjectPoint(e.a);
        Point2d p1 = ProjectPoint(e.b);
        Point2d dir = p1.Minus(p0);
        double d = pp.DistanceToLine(p0, dir, /*asSegment=*/true);
        if(minD > 0.0 && d > minD) continue;
        minD = d;
        k = pp.Minus(p0).Dot(dir) / dir.Dot(dir);
        edge = &e;
    }
    if(edge == NULL) return UnProjectPoint(pp);
    return edge->a.Plus(edge->b.Minus(edge->a).ScaledBy(k));
}

bool GraphicsWindow::ConstrainPointByHovered(hEntity pt, const Point2d *projected) {
    if(!hover.entity.v) return false;

    Entity *point = SK.GetEntity(pt);
    Entity *e = SK.GetEntity(hover.entity);
    if(e->IsPoint()) {
        point->PointForceTo(e->PointGetNum());
        Constraint::ConstrainCoincident(e->h, pt);
        return true;
    }
    if(e->IsCircle()) {
        if(projected != NULL) {
            Vector snapPos = SnapToEntityByScreenPoint(*projected, e->h);
            point->PointForceTo(snapPos);
        }
        Constraint::Constrain(Constraint::Type::PT_ON_CIRCLE,
            pt, Entity::NO_ENTITY, e->h);
        return true;
    }
    if(e->type == Entity::Type::LINE_SEGMENT) {
        if(projected != NULL) {
            Vector snapPos = SnapToEntityByScreenPoint(*projected, e->h);
            point->PointForceTo(snapPos);
        }
        Constraint::Constrain(Constraint::Type::PT_ON_LINE,
            pt, Entity::NO_ENTITY, e->h);
        return true;
    }

    return false;
}

bool GraphicsWindow::MouseEvent(Platform::MouseEvent event) {
    using Platform::MouseEvent;
    double width, height;
    window->GetContentSize(&width, &height);

    event.x = event.x - width / 2;
    event.y = height / 2 - event.y;

    switch(event.type) {
        case MouseEvent::Type::MOTION:
            this->MouseMoved(event.x, event.y,
                             event.button == MouseEvent::Button::LEFT,
                             event.button == MouseEvent::Button::MIDDLE,
                             event.button == MouseEvent::Button::RIGHT,
                             event.shiftDown,
                             event.controlDown);
            break;

        case MouseEvent::Type::PRESS:
            if(event.button == MouseEvent::Button::LEFT) {
                this->MouseLeftDown(event.x, event.y, event.shiftDown, event.controlDown);
            } else if(event.button == MouseEvent::Button::MIDDLE ||
                      event.button == MouseEvent::Button::RIGHT) {
                this->MouseMiddleOrRightDown(event.x, event.y);
            }
            break;

        case MouseEvent::Type::DBL_PRESS:
            if(event.button == MouseEvent::Button::LEFT) {
                this->MouseLeftDoubleClick(event.x, event.y);
            }
            break;

        case MouseEvent::Type::RELEASE:
            if(event.button == MouseEvent::Button::LEFT) {
                this->MouseLeftUp(event.x, event.y, event.shiftDown, event.controlDown);
            } else if(event.button == MouseEvent::Button::RIGHT) {
                this->MouseRightUp(event.x, event.y);
            }
            break;

        case MouseEvent::Type::SCROLL_VERT:
            this->MouseScroll(event.x, event.y, event.shiftDown ? event.scrollDelta / 10 : event.scrollDelta);
            break;

        case MouseEvent::Type::LEAVE:
            this->MouseLeave();
            break;
    }

    return true;
}

void GraphicsWindow::MouseLeftDown(double mx, double my, bool shiftDown, bool ctrlDown) {
    orig.mouseDown = true;

    if(window->IsEditorVisible()) {
        orig.mouse = Point2d::From(mx, my);
        orig.mouseOnButtonDown = orig.mouse;
        window->HideEditor();
        return;
    }
    SS.TW.HideEditControl();

    if(SS.showToolbar) {
        if(ToolbarMouseDown((int)mx, (int)my)) return;
    }

    // This will be clobbered by MouseMoved below.
    bool hasConstraintSuggestion = pending.hasSuggestion;
    Constraint::Type constraintSuggestion = pending.suggestion;

    // Make sure the hover is up to date.
    MouseMoved(mx, my, /*leftDown=*/false, /*middleDown=*/false, /*rightDown=*/false,
        /*shiftDown=*/false, /*ctrlDown=*/false);
    orig.mouse.x = mx;
    orig.mouse.y = my;
    orig.mouseOnButtonDown = orig.mouse;
    Point2d mouse = Point2d::From(mx, my);

    // The current mouse location
    Vector v = offset.ScaledBy(-1);
    v = v.Plus(projRight.ScaledBy(mx/scale));
    v = v.Plus(projUp.ScaledBy(my/scale));

    hRequest hr = {};
    hConstraint hc = {};
    switch(pending.operation) {
        case Pending::COMMAND:
            switch(pending.command) {
                case Command::DATUM_POINT:
                    hr = AddRequest(Request::Type::DATUM_POINT);
                    SK.GetEntity(hr.entity(0))->PointForceTo(v);
                    ConstrainPointByHovered(hr.entity(0), &mouse);

                    ClearSuper();
                    break;

                case Command::LINE_SEGMENT:
                case Command::CONSTR_SEGMENT:
                    hr = AddRequest(Request::Type::LINE_SEGMENT);
                    SK.GetRequest(hr)->construction = (pending.command == Command::CONSTR_SEGMENT);
                    SK.GetEntity(hr.entity(1))->PointForceTo(v);
                    ConstrainPointByHovered(hr.entity(1), &mouse);

                    ClearSuper();
                    AddToPending(hr);

                    pending.operation = Pending::DRAGGING_NEW_LINE_POINT;
                    pending.request = hr;
                    pending.point = hr.entity(2);
                    pending.description = _("click next point of line, or press Esc");
                    SK.GetEntity(pending.point)->PointForceTo(v);
                    break;

                case Command::RECTANGLE: {
                    if(!SS.GW.LockedInWorkplane()) {
                        Error(_("Can't draw rectangle in 3d; first, activate a workplane "
                                "with Sketch -> In Workplane."));
                        ClearSuper();
                        break;
                    }
                    hRequest lns[4];
                    int i;
                    SS.UndoRemember();
                    for(i = 0; i < 4; i++) {
                        lns[i] = AddRequest(Request::Type::LINE_SEGMENT, /*rememberForUndo=*/false);
                        AddToPending(lns[i]);
                    }
                    for(i = 0; i < 4; i++) {
                        Constraint::ConstrainCoincident(
                            lns[i].entity(1), lns[(i+1)%4].entity(2));
                        SK.GetEntity(lns[i].entity(1))->PointForceTo(v);
                        SK.GetEntity(lns[i].entity(2))->PointForceTo(v);
                    }
                    for(i = 0; i < 4; i++) {
                        Constraint::Constrain(
                            (i % 2) ? Constraint::Type::HORIZONTAL : Constraint::Type::VERTICAL,
                            Entity::NO_ENTITY, Entity::NO_ENTITY,
                            lns[i].entity(0));
                    }
                    if(ConstrainPointByHovered(lns[2].entity(1), &mouse)) {
                        Vector pos = SK.GetEntity(lns[2].entity(1))->PointGetNum();
                        for(i = 0; i < 4; i++) {
                            SK.GetEntity(lns[i].entity(1))->PointForceTo(pos);
                            SK.GetEntity(lns[i].entity(2))->PointForceTo(pos);
                        }
                    }

                    pending.operation = Pending::DRAGGING_NEW_POINT;
                    pending.point = lns[1].entity(2);
                    pending.description = _("click to place other corner of rectangle");
                    hr = lns[0];
                    break;
                }
                case Command::CIRCLE:
                    hr = AddRequest(Request::Type::CIRCLE);
                    // Centered where we clicked
                    SK.GetEntity(hr.entity(1))->PointForceTo(v);
                    // Normal to the screen
                    SK.GetEntity(hr.entity(32))->NormalForceTo(
                        Quaternion::From(SS.GW.projRight, SS.GW.projUp));
                    // Initial radius zero
                    SK.GetEntity(hr.entity(64))->DistanceForceTo(0);

                    ConstrainPointByHovered(hr.entity(1), &mouse);

                    ClearSuper();
                    AddToPending(hr);

                    pending.operation = Pending::DRAGGING_NEW_RADIUS;
                    pending.circle = hr.entity(0);
                    pending.description = _("click to set radius");
                    break;

                case Command::ARC: {
                    if(!SS.GW.LockedInWorkplane()) {
                        Error(_("Can't draw arc in 3d; first, activate a workplane "
                                "with Sketch -> In Workplane."));
                        ClearPending();
                        break;
                    }
                    hr = AddRequest(Request::Type::ARC_OF_CIRCLE);
                    // This fudge factor stops us from immediately failing to solve
                    // because of the arc's implicit (equal radius) tangent.
                    Vector adj = SS.GW.projRight.WithMagnitude(2/SS.GW.scale);
                    SK.GetEntity(hr.entity(1))->PointForceTo(v.Minus(adj));
                    SK.GetEntity(hr.entity(2))->PointForceTo(v);
                    SK.GetEntity(hr.entity(3))->PointForceTo(v);
                    ConstrainPointByHovered(hr.entity(2), &mouse);

                    ClearSuper();
                    AddToPending(hr);

                    pending.operation = Pending::DRAGGING_NEW_ARC_POINT;
                    pending.point = hr.entity(3);
                    pending.description = _("click to place point");
                    break;
                }
                case Command::CUBIC:
                    hr = AddRequest(Request::Type::CUBIC);
                    SK.GetEntity(hr.entity(1))->PointForceTo(v);
                    SK.GetEntity(hr.entity(2))->PointForceTo(v);
                    SK.GetEntity(hr.entity(3))->PointForceTo(v);
                    SK.GetEntity(hr.entity(4))->PointForceTo(v);
                    ConstrainPointByHovered(hr.entity(1), &mouse);

                    ClearSuper();
                    AddToPending(hr);

                    pending.operation = Pending::DRAGGING_NEW_CUBIC_POINT;
                    pending.point = hr.entity(4);
                    pending.description = _("click next point of cubic, or press Esc");
                    break;

                case Command::WORKPLANE:
                    if(LockedInWorkplane()) {
                        Error(_("Sketching in a workplane already; sketch in 3d before "
                                "creating new workplane."));
                        ClearSuper();
                        break;
                    }
                    hr = AddRequest(Request::Type::WORKPLANE);
                    SK.GetEntity(hr.entity(1))->PointForceTo(v);
                    SK.GetEntity(hr.entity(32))->NormalForceTo(
                        Quaternion::From(SS.GW.projRight, SS.GW.projUp));
                    ConstrainPointByHovered(hr.entity(1), &mouse);

                    ClearSuper();
                    break;

                case Command::TTF_TEXT: {
                    if(!SS.GW.LockedInWorkplane()) {
                        Error(_("Can't draw text in 3d; first, activate a workplane "
                                "with Sketch -> In Workplane."));
                        ClearSuper();
                        break;
                    }
                    hr = AddRequest(Request::Type::TTF_TEXT);
                    AddToPending(hr);
                    Request *r = SK.GetRequest(hr);
                    r->str = "Abc";
                    r->font = Platform::embeddedFont;

                    for(int i = 1; i <= 4; i++) {
                        SK.GetEntity(hr.entity(i))->PointForceTo(v);
                    }

                    pending.operation = Pending::DRAGGING_NEW_POINT;
                    pending.point = hr.entity(3);
                    pending.description = _("click to place bottom right of text");
                    break;
                }

                case Command::IMAGE: {
                    if(!SS.GW.LockedInWorkplane()) {
                        Error(_("Can't draw image in 3d; first, activate a workplane "
                                "with Sketch -> In Workplane."));
                        ClearSuper();
                        break;
                    }
                    hr = AddRequest(Request::Type::IMAGE);
                    AddToPending(hr);
                    Request *r = SK.GetRequest(hr);
                    r->file = pending.filename;

                    for(int i = 1; i <= 4; i++) {
                        SK.GetEntity(hr.entity(i))->PointForceTo(v);
                    }

                    pending.operation = Pending::DRAGGING_NEW_POINT;
                    pending.point = hr.entity(3);
                    pending.description = "click to place bottom right of image";
                    break;
                }

                case Command::COMMENT: {
                    ClearSuper();
                    Constraint c = {};
                    c.group       = SS.GW.activeGroup;
                    c.workplane   = SS.GW.ActiveWorkplane();
                    c.type        = Constraint::Type::COMMENT;
                    c.disp.offset = v;
                    c.comment     = _("NEW COMMENT -- DOUBLE-CLICK TO EDIT");
                    hc = Constraint::AddConstraint(&c);
                    break;
                }
                default: ssassert(false, "Unexpected pending menu id");
            }
            break;

        case Pending::DRAGGING_RADIUS:
            ClearPending();
            break;

        case Pending::DRAGGING_NEW_POINT:
        case Pending::DRAGGING_NEW_ARC_POINT:
            ConstrainPointByHovered(pending.point, &mouse);
            ClearPending();
            break;

        case Pending::DRAGGING_NEW_CUBIC_POINT: {
            hRequest hr = pending.point.request();
            Request *r = SK.GetRequest(hr);

            if(hover.entity == hr.entity(1) && r->extraPoints >= 2) {
                // They want the endpoints coincident, which means a periodic
                // spline instead.
                r->type = Request::Type::CUBIC_PERIODIC;
                // Remove the off-curve control points, which are no longer
                // needed here; so move [2,ep+1] down, skipping first pt.
                int i;
                for(i = 2; i <= r->extraPoints+1; i++) {
                    SK.GetEntity(hr.entity((i-1)+1))->PointForceTo(
                        SK.GetEntity(hr.entity(i+1))->PointGetNum());
                }
                // and move ep+3 down by two, skipping both
                SK.GetEntity(hr.entity((r->extraPoints+1)+1))->PointForceTo(
                  SK.GetEntity(hr.entity((r->extraPoints+3)+1))->PointGetNum());
                r->extraPoints -= 2;
                // And we're done.
                SS.MarkGroupDirty(r->group);
                ClearPending();
                break;
            }

            if(ConstrainPointByHovered(pending.point, &mouse)) {
                ClearPending();
                break;
            }

            Entity e;
            if(r->extraPoints >= (int)arraylen(e.point) - 4) {
                ClearPending();
                break;
            }

            (SK.GetRequest(hr)->extraPoints)++;
            SS.GenerateAll(SolveSpaceUI::Generate::REGEN);

            int ep = r->extraPoints;
            Vector last = SK.GetEntity(hr.entity(3+ep))->PointGetNum();

            SK.GetEntity(hr.entity(2+ep))->PointForceTo(last);
            SK.GetEntity(hr.entity(3+ep))->PointForceTo(v);
            SK.GetEntity(hr.entity(4+ep))->PointForceTo(v);
            pending.point = hr.entity(4+ep);
            break;
        }

        case Pending::DRAGGING_NEW_LINE_POINT: {
            if(hover.entity.v) {
                Entity *e = SK.GetEntity(hover.entity);
                if(e->IsPoint()) {
                    hRequest hrl = pending.point.request();
                    Entity *sp = SK.GetEntity(hrl.entity(1));
                    if(( e->PointGetNum()).Equals(
                       (sp->PointGetNum())))
                    {
                        // If we constrained by the hovered point, then we
                        // would create a zero-length line segment. That's
                        // not good, so just stop drawing.
                        ClearPending();
                        break;
                    }
                }
            }

            bool doneDragging = ConstrainPointByHovered(pending.point, &mouse);

            // Constrain the line segment horizontal or vertical if close enough
            if(hasConstraintSuggestion) {
                Constraint::TryConstrain(constraintSuggestion,
                    Entity::NO_ENTITY, Entity::NO_ENTITY, pending.request.entity(0));
            }

            if(doneDragging) {
                ClearPending();
                break;
            }

            // Create a new line segment, so that we continue drawing.
            hRequest hr = AddRequest(Request::Type::LINE_SEGMENT);
            ReplacePending(pending.request, hr);
            SK.GetRequest(hr)->construction = SK.GetRequest(pending.request)->construction;
            // Displace the second point of the new line segment slightly,
            // to avoid creating zero-length edge warnings.
            SK.GetEntity(hr.entity(2))->PointForceTo(
                v.Plus(projRight.ScaledBy(0.5/scale)));

            // Constrain the line segments to share an endpoint
            Constraint::ConstrainCoincident(pending.point, hr.entity(1));
            Vector pendingPos = SK.GetEntity(pending.point)->PointGetNum();
            SK.GetEntity(hr.entity(1))->PointForceTo(pendingPos);

            // And drag an endpoint of the new line segment
            pending.operation = Pending::DRAGGING_NEW_LINE_POINT;
            pending.request = hr;
            pending.point = hr.entity(2);
            pending.description = _("click next point of line, or press Esc");

            break;
        }

        case Pending::NONE:
        default:
            ClearPending();
            if(!hover.IsEmpty()) {
                if(!ctrlDown) {
                    hoverWasSelectedOnMousedown = IsSelected(&hover);
                    MakeSelected(&hover);
                } else {
                    MakeUnselected(&hover, /*coincidentPointTrick=*/true);
                }
            }
            break;
    }

    // Activate group with newly created request/constraint
    Group *g = NULL;
    if(hr.v != 0) {
        g = SK.GetGroup(SK.GetRequest(hr)->group);
    }
    if(hc.v != 0) {
        g = SK.GetGroup(SK.GetConstraint(hc)->group);
    }
    if(g != NULL) {
        g->visible = true;
    }

    SS.ScheduleShowTW();
    Invalidate();
}

void GraphicsWindow::MouseLeftUp(double mx, double my, bool shiftDown, bool ctrlDown) {
    orig.mouseDown = false;

    switch(pending.operation) {
        case Pending::DRAGGING_POINTS:
        case Pending::DRAGGING_CONSTRAINT:
        case Pending::DRAGGING_NORMAL:
        case Pending::DRAGGING_RADIUS:
            if(!hoverWasSelectedOnMousedown) {
                // And then clear the selection again, since they
                // probably didn't want that selected if they just
                // were dragging it.
                ClearSelection();
            }
            hoverWasSelectedOnMousedown = false;
            SS.extraLine.draw = false;
            ClearPending();
            Invalidate();
            break;

        case Pending::DRAGGING_MARQUEE:
            SelectByMarquee();
            ClearPending();
            Invalidate();
            break;

        case Pending::NONE:
            if(hover.IsEmpty() && !ctrlDown) {
                ClearSelection();
            }
            break;

        default:
            break;  // do nothing
    }
}

void GraphicsWindow::EditConstraint(hConstraint constraint) {
    constraintBeingEdited = constraint;
    ClearSuper();

    Constraint *c = SK.GetConstraint(constraintBeingEdited);
    if(!c->HasLabel()) {
        // Not meaningful to edit a constraint without a dimension
        return;
    }
    if(c->reference) {
        // Not meaningful to edit a reference dimension
        return;
    }

    Vector p3 = c->GetLabelPos(GetCamera());
    Point2d p2 = ProjectPoint(p3);

    std::string editValue;
    std::string editPlaceholder;
    switch(c->type) {
        case Constraint::Type::COMMENT:
            editValue = c->comment;
            editPlaceholder = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
            break;

        default: {
            double value = fabs(c->valA);

            // If displayed as radius, also edit as radius.
            if(c->type == Constraint::Type::DIAMETER && c->other)
                value /= 2;

            // Try showing value with default number of digits after decimal first.
            if(c->type == Constraint::Type::LENGTH_RATIO || c->type == Constraint::Type::ARC_ARC_LEN_RATIO || c->type == Constraint::Type::ARC_LINE_LEN_RATIO) {
                editValue = ssprintf("%.3f", value);
            } else if(c->type == Constraint::Type::ANGLE) {
                editValue = SS.DegreeToString(value);
            } else {
                editValue = SS.MmToString(value);
                value /= SS.MmPerUnit();
            }
            // If that's not enough to represent it exactly, show the value with as many
            // digits after decimal as required, up to 10.
            int digits = 0;
            while(fabs(std::stod(editValue) - value) > 1e-10) {
                editValue = ssprintf("%.*f", digits, value);
                digits++;
            }
            editPlaceholder = "10.000000";
            break;
        }
    }

    double width, height;
    window->GetContentSize(&width, &height);
    hStyle hs = c->disp.style;
    if(hs.v == 0) hs.v = Style::CONSTRAINT;
    double capHeight = Style::TextHeight(hs);
    double fontHeight = VectorFont::Builtin()->GetHeight(capHeight);
    double editMinWidth = VectorFont::Builtin()->GetWidth(capHeight, editPlaceholder);
    window->ShowEditor(p2.x + width / 2, height / 2 - p2.y,
                        fontHeight, editMinWidth,
                        /*isMonospace=*/false, editValue);
}

void GraphicsWindow::MouseLeftDoubleClick(double mx, double my) {
    if(window->IsEditorVisible()) return;
    SS.TW.HideEditControl();

    if(hover.constraint.v) {
        EditConstraint(hover.constraint);
    }
}

void GraphicsWindow::EditControlDone(const std::string &s) {
    window->HideEditor();
    window->Invalidate();

    Constraint *c = SK.GetConstraint(constraintBeingEdited);

    if(c->type == Constraint::Type::COMMENT) {
        SS.UndoRemember();
        c->comment = s;
        return;
    }

    if(Expr *e = Expr::From(s, true)) {
        SS.UndoRemember();

        switch(c->type) {
            case Constraint::Type::PROJ_PT_DISTANCE:
            case Constraint::Type::PT_LINE_DISTANCE:
            case Constraint::Type::PT_FACE_DISTANCE:
            case Constraint::Type::PT_PLANE_DISTANCE:
            case Constraint::Type::LENGTH_DIFFERENCE:
            case Constraint::Type::ARC_ARC_DIFFERENCE:
            case Constraint::Type::ARC_LINE_DIFFERENCE: {
                // The sign is not displayed to the user, but this is a signed
                // distance internally. To flip the sign, the user enters a
                // negative distance.
                bool wasNeg = (c->valA < 0);
                if(wasNeg) {
                    c->valA = -SS.ExprToMm(e);
                } else {
                    c->valA = SS.ExprToMm(e);
                }
                break;
            }
            case Constraint::Type::ANGLE:
            case Constraint::Type::LENGTH_RATIO:
            case Constraint::Type::ARC_ARC_LEN_RATIO:
            case Constraint::Type::ARC_LINE_LEN_RATIO:
                // These don't get the units conversion for distance, and
                // they're always positive
                c->valA = fabs(e->Eval());
                break;

            case Constraint::Type::DIAMETER:
                c->valA = fabs(SS.ExprToMm(e));

                // If displayed and edited as radius, convert back
                // to diameter
                if(c->other)
                    c->valA *= 2;
                break;

            default:
                // These are always positive, and they get the units conversion.
                c->valA = fabs(SS.ExprToMm(e));
                break;
        }
        SS.MarkGroupDirty(c->group);
    }
}

void GraphicsWindow::MouseScroll(double x, double y, double delta) {
    double offsetRight = offset.Dot(projRight);
    double offsetUp = offset.Dot(projUp);

    double righti = x/scale - offsetRight;
    double upi = y/scale - offsetUp;

    // The default zoom factor is 1.2x for one scroll wheel click (delta==1).
    // To support smooth scrolling where scroll wheel events come in increments
    // smaller (or larger) than 1 we do:
    //     scale *= exp(ln(1.2) * delta);
    // to ensure that the same total scroll delta always results in the same
    // total zoom irrespective of in how many increments the zoom was applied.
    // For example if we scroll a total delta of a+b in two events vs. one then
    //     scale * e^a * e^b == scale * e^(a+b)
    // while
    //     scale * a * b != scale * (a+b)
    // So this constant is ln(1.2) = 0.1823216 to make the default zoom 1.2x
    scale *= exp(0.1823216 * delta);

    double rightf = x/scale - offsetRight;
    double upf = y/scale - offsetUp;

    offset = offset.Plus(projRight.ScaledBy(rightf - righti));
    offset = offset.Plus(projUp.ScaledBy(upf - upi));

    if(SS.TW.shown.screen == TextWindow::Screen::EDIT_VIEW) {
        if(havePainted) {
            SS.ScheduleShowTW();
        }
    }
    havePainted = false;
    Invalidate();
}

void GraphicsWindow::MouseLeave() {
    // Un-hover everything when the mouse leaves our window, unless there's
    // currently a context menu shown.
    if(!context.active) {
        hover.Clear();
        toolbarHovered = Command::NONE;
        Invalidate();
    }
    SS.extraLine.draw = false;
}

void GraphicsWindow::SixDofEvent(Platform::SixDofEvent event) {
    if(event.type == Platform::SixDofEvent::Type::RELEASE) {
        ZoomToFit(/*includingInvisibles=*/false, /*useSelection=*/true);
        Invalidate();
        return;
    }

    if(!havePainted) return;
    Vector out = projRight.Cross(projUp);

    // rotation vector is axis of rotation, and its magnitude is angle
    Vector aa = Vector::From(event.rotationX, event.rotationY, event.rotationZ);
    // but it's given with respect to screen projection frame
    aa = aa.ScaleOutOfCsys(projRight, projUp, out);
    double aam = aa.Magnitude();
    if(aam > 0.0) aa = aa.WithMagnitude(1);

    // This can either transform our view, or transform a linked part.
    GroupSelection();
    Entity *e = NULL;
    Group *g = NULL;
    if(gs.points == 1   && gs.n == 1) e = SK.GetEntity(gs.point [0]);
    if(gs.entities == 1 && gs.n == 1) e = SK.GetEntity(gs.entity[0]);
    if(e) g = SK.GetGroup(e->group);
    if(g && g->type == Group::Type::LINKED && !event.shiftDown) {
        // Apply the transformation to a linked part. Gain down the Z
        // axis, since it's hard to see what you're doing on that one since
        // it's normal to the screen.
        Vector t = projRight.ScaledBy(event.translationX/scale).Plus(
                   projUp   .ScaledBy(event.translationY/scale).Plus(
                   out      .ScaledBy(0.1*event.translationZ/scale)));
        Quaternion q = Quaternion::From(aa, aam);

        // If we go five seconds without SpaceNavigator input, or if we've
        // switched groups, then consider that a new action and save an undo
        // point.
        int64_t now = GetMilliseconds();
        if(now - last6DofTime > 5000 ||
           last6DofGroup != g->h)
        {
            SS.UndoRemember();
        }

        g->TransformImportedBy(t, q);

        last6DofTime = now;
        last6DofGroup = g->h;
        SS.MarkGroupDirty(g->h);
    } else {
        // Apply the transformation to the view of the everything. The
        // x and y components are translation; but z component is scale,
        // not translation, or else it would do nothing in a parallel
        // projection
        offset = offset.Plus(projRight.ScaledBy(event.translationX/scale));
        offset = offset.Plus(projUp.ScaledBy(event.translationY/scale));
        scale *= exp(0.001*event.translationZ);

        if(aam > 0.0) {
            projRight = projRight.RotatedAbout(aa, -aam);
            projUp    = projUp.   RotatedAbout(aa, -aam);
            NormalizeProjectionVectors();
        }
    }

    havePainted = false;
    Invalidate();
}
