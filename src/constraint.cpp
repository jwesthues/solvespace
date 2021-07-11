//-----------------------------------------------------------------------------
// Implementation of the Constraint menu, to create new constraints in
// the sketch.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

std::string Constraint::DescriptionString() const {
    std::string s;
    switch(type) {
        case Type::POINTS_COINCIDENT:   s = C_("constr-name", "pts-coincident"); break;
        case Type::PT_PT_DISTANCE:      s = C_("constr-name", "pt-pt-distance"); break;
        case Type::PT_LINE_DISTANCE:    s = C_("constr-name", "pt-line-distance"); break;
        case Type::PT_PLANE_DISTANCE:   s = C_("constr-name", "pt-plane-distance"); break;
        case Type::PT_FACE_DISTANCE:    s = C_("constr-name", "pt-face-distance"); break;
        case Type::PROJ_PT_DISTANCE:    s = C_("constr-name", "proj-pt-pt-distance"); break;
        case Type::PT_IN_PLANE:         s = C_("constr-name", "pt-in-plane"); break;
        case Type::PT_ON_LINE:          s = C_("constr-name", "pt-on-line"); break;
        case Type::PT_ON_FACE:          s = C_("constr-name", "pt-on-face"); break;
        case Type::EQUAL_LENGTH_LINES:  s = C_("constr-name", "eq-length"); break;
        case Type::EQ_LEN_PT_LINE_D:    s = C_("constr-name", "eq-length-and-pt-ln-dist"); break;
        case Type::EQ_PT_LN_DISTANCES:  s = C_("constr-name", "eq-pt-line-distances"); break;
        case Type::LENGTH_RATIO:        s = C_("constr-name", "length-ratio"); break;
        case Type::ARC_ARC_LEN_RATIO:   s = C_("constr-name", "arc-arc-length-ratio"); break;
        case Type::ARC_LINE_LEN_RATIO:  s = C_("constr-name", "arc-line-length-ratio"); break;
        case Type::LENGTH_DIFFERENCE:   s = C_("constr-name", "length-difference"); break;
        case Type::ARC_ARC_DIFFERENCE:  s = C_("constr-name", "arc-arc-len-difference"); break;
        case Type::ARC_LINE_DIFFERENCE: s = C_("constr-name", "arc-line-len-difference"); break;
        case Type::SYMMETRIC:           s = C_("constr-name", "symmetric"); break;
        case Type::SYMMETRIC_HORIZ:     s = C_("constr-name", "symmetric-h"); break;
        case Type::SYMMETRIC_VERT:      s = C_("constr-name", "symmetric-v"); break;
        case Type::SYMMETRIC_LINE:      s = C_("constr-name", "symmetric-line"); break;
        case Type::AT_MIDPOINT:         s = C_("constr-name", "at-midpoint"); break;
        case Type::HORIZONTAL:          s = C_("constr-name", "horizontal"); break;
        case Type::VERTICAL:            s = C_("constr-name", "vertical"); break;
        case Type::DIAMETER:            s = C_("constr-name", "diameter"); break;
        case Type::PT_ON_CIRCLE:        s = C_("constr-name", "pt-on-circle"); break;
        case Type::SAME_ORIENTATION:    s = C_("constr-name", "same-orientation"); break;
        case Type::ANGLE:               s = C_("constr-name", "angle"); break;
        case Type::PARALLEL:            s = C_("constr-name", "parallel"); break;
        case Type::ARC_LINE_TANGENT:    s = C_("constr-name", "arc-line-tangent"); break;
        case Type::CUBIC_LINE_TANGENT:  s = C_("constr-name", "cubic-line-tangent"); break;
        case Type::CURVE_CURVE_TANGENT: s = C_("constr-name", "curve-curve-tangent"); break;
        case Type::PERPENDICULAR:       s = C_("constr-name", "perpendicular"); break;
        case Type::EQUAL_RADIUS:        s = C_("constr-name", "eq-radius"); break;
        case Type::EQUAL_ANGLE:         s = C_("constr-name", "eq-angle"); break;
        case Type::EQUAL_LINE_ARC_LEN:  s = C_("constr-name", "eq-line-len-arc-len"); break;
        case Type::WHERE_DRAGGED:       s = C_("constr-name", "lock-where-dragged"); break;
        case Type::COMMENT:             s = C_("constr-name", "comment"); break;
        default:                        s = "???"; break;
    }

    return ssprintf("c%03x-%s", h.v, s.c_str());
}

#ifndef LIBRARY

//-----------------------------------------------------------------------------
// Delete all constraints with the specified type, entityA, ptA. We use this
// when auto-removing constraints that would become redundant.
//-----------------------------------------------------------------------------
void Constraint::DeleteAllConstraintsFor(Constraint::Type type, hEntity entityA, hEntity ptA)
{
    SK.constraint.ClearTags();
    for(auto &constraint : SK.constraint) {
        ConstraintBase *ct = &constraint;
        if(ct->type != type) continue;

        if(ct->entityA != entityA) continue;
        if(ct->ptA != ptA) continue;
        ct->tag = 1;
    }
    SK.constraint.RemoveTagged();
    // And no need to do anything special, since nothing
    // ever depends on a constraint. But do clear the
    // hover, in case the just-deleted constraint was
    // hovered.
    SS.GW.hover.Clear();
}

hConstraint Constraint::AddConstraint(Constraint *c, bool rememberForUndo) {
    if(rememberForUndo) SS.UndoRemember();

    hConstraint hc = SK.constraint.AddAndAssignId(c);
    SK.GetConstraint(hc)->Generate(&SK.param);

    SS.MarkGroupDirty(c->group);
    SK.GetGroup(c->group)->dofCheckOk = false;
    return c->h;
}

hConstraint Constraint::Constrain(Constraint::Type type, hEntity ptA, hEntity ptB,
                                  hEntity entityA, hEntity entityB,
                                  bool other, bool other2)
{
    Constraint c = {};
    c.group = SS.GW.activeGroup;
    c.workplane = SS.GW.ActiveWorkplane();
    c.type = type;
    c.ptA = ptA;
    c.ptB = ptB;
    c.entityA = entityA;
    c.entityB = entityB;
    c.other = other;
    c.other2 = other2;
    return AddConstraint(&c, /*rememberForUndo=*/false);
}

hConstraint Constraint::TryConstrain(Constraint::Type type, hEntity ptA, hEntity ptB,
                                     hEntity entityA, hEntity entityB,
                                     bool other, bool other2) {
    int rankBefore, rankAfter;
    SolveResult howBefore = SS.TestRankForGroup(SS.GW.activeGroup, &rankBefore);
    hConstraint hc = Constrain(type, ptA, ptB, entityA, entityB, other, other2);
    SolveResult howAfter = SS.TestRankForGroup(SS.GW.activeGroup, &rankAfter);
    // There are two cases where the constraint is clearly redundant:
    //   * If the group wasn't overconstrained and now it is;
    //   * If the group was overconstrained, and adding the constraint doesn't change rank at all.
    if((howBefore == SolveResult::OKAY && howAfter == SolveResult::REDUNDANT_OKAY) ||
       (howBefore == SolveResult::REDUNDANT_OKAY && howAfter == SolveResult::REDUNDANT_OKAY &&
            rankBefore == rankAfter)) {
        SK.constraint.RemoveById(hc);
        hc = {};
    }
    return hc;
}

hConstraint Constraint::ConstrainCoincident(hEntity ptA, hEntity ptB) {
    return Constrain(Type::POINTS_COINCIDENT, ptA, ptB,
        Entity::NO_ENTITY, Entity::NO_ENTITY, /*other=*/false, /*other2=*/false);
}

bool Constraint::ConstrainArcLineTangent(Constraint *c, Entity *line, Entity *arc) {
    Vector l0 = SK.GetEntity(line->point[0])->PointGetNum(),
           l1 = SK.GetEntity(line->point[1])->PointGetNum();
    Vector a1 = SK.GetEntity(arc->point[1])->PointGetNum(),
           a2 = SK.GetEntity(arc->point[2])->PointGetNum();
    if(l0.Equals(a1) || l1.Equals(a1)) {
        c->other = false;
    } else if(l0.Equals(a2) || l1.Equals(a2)) {
        c->other = true;
    } else {
        Error(_("The tangent arc and line segment must share an "
                "endpoint. Constrain them with Constrain -> "
                "On Point before constraining tangent."));
        return false;
    }
    return true;
}

bool Constraint::ConstrainCubicLineTangent(Constraint *c, Entity *line, Entity *cubic) {
    Vector l0 = SK.GetEntity(line->point[0])->PointGetNum(),
           l1 = SK.GetEntity(line->point[1])->PointGetNum();
    Vector as = cubic->CubicGetStartNum(),
           af = cubic->CubicGetFinishNum();

    if(l0.Equals(as) || l1.Equals(as)) {
        c->other = false;
    } else if(l0.Equals(af) || l1.Equals(af)) {
        c->other = true;
    } else {
        Error(_("The tangent cubic and line segment must share an "
                "endpoint. Constrain them with Constrain -> "
                "On Point before constraining tangent."));
        return false;
    }
    return true;
}

bool Constraint::ConstrainCurveCurveTangent(Constraint *c, Entity *eA, Entity *eB) {
    Vector as = eA->EndpointStart(),
           af = eA->EndpointFinish(),
           bs = eB->EndpointStart(),
           bf = eB->EndpointFinish();
    if(as.Equals(bs)) {
        c->other = false;
        c->other2 = false;
    } else if(as.Equals(bf)) {
        c->other = false;
        c->other2 = true;
    } else if(af.Equals(bs)) {
        c->other = true;
        c->other2 = false;
    } else if(af.Equals(bf)) {
        c->other = true;
        c->other2 = true;
    } else {
        Error(_("The curves must share an endpoint. Constrain them "
                "with Constrain -> On Point before constraining "
                "tangent."));
        return false;
    }
    return true;
}

void Constraint::MenuConstrain(Command id) {
    Constraint c = {};
    c.group = SS.GW.activeGroup;
    c.workplane = SS.GW.ActiveWorkplane();

    SS.GW.GroupSelection();
    auto const &gs = SS.GW.gs;
    switch(id) {
        case Command::DISTANCE_DIA:
        case Command::REF_DISTANCE: {
            if(gs.points == 2 && gs.n == 2) {
                c.type = Type::PT_PT_DISTANCE;
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];
            } else if(gs.lineSegments == 1 && gs.n == 1) {
                c.type = Type::PT_PT_DISTANCE;
                Entity *e = SK.GetEntity(gs.entity[0]);
                c.ptA = e->point[0];
                c.ptB = e->point[1];
            } else if(gs.vectors == 1 && gs.points == 2 && gs.n == 3) {
                c.type = Type::PROJ_PT_DISTANCE;
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];
                c.entityA = gs.vector[0];
            } else if(gs.workplanes == 1 && gs.points == 1 && gs.n == 2) {
                c.type = Type::PT_PLANE_DISTANCE;
                c.ptA = gs.point[0];
                c.entityA = gs.entity[0];
            } else if(gs.lineSegments == 1 && gs.points == 1 && gs.n == 2) {
                c.type = Type::PT_LINE_DISTANCE;
                c.ptA = gs.point[0];
                c.entityA = gs.entity[0];
            } else if(gs.faces == 1 && gs.points == 1 && gs.n == 2) {
                c.type = Type::PT_FACE_DISTANCE;
                c.ptA = gs.point[0];
                c.entityA = gs.face[0];
            } else if(gs.circlesOrArcs == 1 && gs.n == 1) {
                c.type = Type::DIAMETER;
                c.entityA = gs.entity[0];
            } else {
                Error(_("Bad selection for distance / diameter constraint. This "
                        "constraint can apply to:\n\n"
                        "    * two points (distance between points)\n"
                        "    * a line segment (length)\n"
                        "    * two points and a line segment or normal (projected distance)\n"
                        "    * a workplane and a point (minimum distance)\n"
                        "    * a line segment and a point (minimum distance)\n"
                        "    * a plane face and a point (minimum distance)\n"
                        "    * a circle or an arc (diameter)\n"));
                return;
            }
            if(c.type == Type::PT_PT_DISTANCE || c.type == Type::PROJ_PT_DISTANCE) {
                Vector n = SS.GW.projRight.Cross(SS.GW.projUp);
                Vector a = SK.GetEntity(c.ptA)->PointGetNum();
                Vector b = SK.GetEntity(c.ptB)->PointGetNum();
                c.disp.offset = n.Cross(a.Minus(b));
                c.disp.offset = (c.disp.offset).WithMagnitude(50/SS.GW.scale);
            } else {
                c.disp.offset = Vector::From(0, 0, 0);
            }

            if(id == Command::REF_DISTANCE) {
                c.reference = true;
            }

            c.valA = 0;
            c.ModifyToSatisfy();
            AddConstraint(&c);
            break;
        }

        case Command::ON_ENTITY: {
            bool dontAddConstraint = false;
            if(gs.points % 2 == 0) {
                dontAddConstraint = true;
                for (int i = 0; i < gs.points / 2; i++) {
                    Constraint c = {};
                    c.group = SS.GW.activeGroup;
                    c.workplane = SS.GW.ActiveWorkplane();
                    c.type = Type::POINTS_COINCIDENT;
                    int doubleI = i * 2;
                    c.ptA = gs.point[doubleI];
                    c.ptB = gs.point[doubleI + 1];
                    AddConstraint(&c, i == 0);
                }
            } else if(gs.points == 1 && gs.workplanes == 1 && gs.n == 2) {
                c.type = Type::PT_IN_PLANE;
                c.ptA = gs.point[0];
                c.entityA = gs.entity[0];
            } else if(gs.points == 1 && gs.lineSegments == 1 && gs.n == 2) {
                c.type = Type::PT_ON_LINE;
                c.ptA = gs.point[0];
                c.entityA = gs.entity[0];
            } else if(gs.points == 1 && gs.circlesOrArcs == 1 && gs.n == 2) {
                c.type = Type::PT_ON_CIRCLE;
                c.ptA = gs.point[0];
                c.entityA = gs.entity[0];
            } else if(gs.points == 1 && gs.faces == 1 && gs.n == 2) {
                c.type = Type::PT_ON_FACE;
                c.ptA = gs.point[0];
                c.entityA = gs.face[0];
            } else {
                Error(_("Bad selection for on point / curve / plane constraint. "
                        "This constraint can apply to:\n\n"
                        "    * two points (points coincident)\n"
                        "    * a point and a workplane (point in plane)\n"
                        "    * a point and a line segment (point on line)\n"
                        "    * a point and a circle or arc (point on curve)\n"
                        "    * a point and a plane face (point on face)\n"));
                return;
            }
            if (!dontAddConstraint) {
                AddConstraint(&c);
            }
            break;
        }
        case Command::EQUAL: {
            bool dontAddConstraint = false;
            if(gs.n == gs.lineSegments) {
                dontAddConstraint = true;
                for (int i = 0; i < gs.lineSegments - 1; i++) {
                    Constraint c = {};
                    c.group = SS.GW.activeGroup;
                    c.workplane = SS.GW.ActiveWorkplane();
                    c.type = Type::EQUAL_LENGTH_LINES;
                    c.entityA = gs.entity[i];
                    c.entityB = gs.entity[i + 1];
                    AddConstraint(&c, i == 0);
                }
            } else if(gs.lineSegments == 2 && gs.points == 2 && gs.n == 4) {
                c.type = Type::EQ_PT_LN_DISTANCES;
                c.entityA = gs.entity[0];
                c.ptA = gs.point[0];
                c.entityB = gs.entity[1];
                c.ptB = gs.point[1];
            } else if(gs.lineSegments == 1 && gs.points == 2 && gs.n == 3) {
                // The same line segment for the distances, but different
                // points.
                c.type = Type::EQ_PT_LN_DISTANCES;
                c.entityA = gs.entity[0];
                c.ptA = gs.point[0];
                c.entityB = gs.entity[0];
                c.ptB = gs.point[1];
            } else if(gs.lineSegments == 2 && gs.points == 1 && gs.n == 3) {
                c.type = Type::EQ_LEN_PT_LINE_D;
                c.entityA = gs.entity[0];
                c.entityB = gs.entity[1];
                c.ptA = gs.point[0];
            } else if(gs.vectors == 4 && gs.n == 4) {
                c.type = Type::EQUAL_ANGLE;
                c.entityA = gs.vector[0];
                c.entityB = gs.vector[1];
                c.entityC = gs.vector[2];
                c.entityD = gs.vector[3];
            } else if(gs.vectors == 3 && gs.n == 3) {
                c.type = Type::EQUAL_ANGLE;
                c.entityA = gs.vector[0];
                c.entityB = gs.vector[1];
                c.entityC = gs.vector[1];
                c.entityD = gs.vector[2];
            } else if(gs.n == gs.circlesOrArcs) {
                dontAddConstraint = true;
                for (int i = 0; i < gs.circlesOrArcs - 1; i++) {
                    Constraint c = {};
                    c.group = SS.GW.activeGroup;
                    c.workplane = SS.GW.ActiveWorkplane();
                    c.type = Type::EQUAL_RADIUS;
                    c.entityA = gs.entity[i];
                    c.entityB = gs.entity[i + 1];
                    AddConstraint(&c);
                }
            } else if(gs.arcs == 1 && gs.lineSegments == 1 && gs.n == 2) {
                c.type = Type::EQUAL_LINE_ARC_LEN;
                if(SK.GetEntity(gs.entity[0])->type == Entity::Type::ARC_OF_CIRCLE) {
                    c.entityA = gs.entity[1];
                    c.entityB = gs.entity[0];
                } else {
                    c.entityA = gs.entity[0];
                    c.entityB = gs.entity[1];
                }
            } else {
                Error(_("Bad selection for equal length / radius constraint. "
                        "This constraint can apply to:\n\n"
                        "    * two line segments (equal length)\n"
                        "    * two line segments and two points "
                                "(equal point-line distances)\n"
                        "    * a line segment and two points "
                                "(equal point-line distances)\n"
                        "    * a line segment, and a point and line segment "
                                "(point-line distance equals length)\n"
                        "    * four line segments or normals "
                                "(equal angle between A,B and C,D)\n"
                        "    * three line segments or normals "
                                "(equal angle between A,B and B,C)\n"
                        "    * two circles or arcs (equal radius)\n"
                        "    * a line segment and an arc "
                                "(line segment length equals arc length)\n"));
                return;
            }
            if(c.type == Type::EQUAL_ANGLE) {
                // Infer the nearest supplementary angle from the sketch.
                Vector a1 = SK.GetEntity(c.entityA)->VectorGetNum(),
                       b1 = SK.GetEntity(c.entityB)->VectorGetNum(),
                       a2 = SK.GetEntity(c.entityC)->VectorGetNum(),
                       b2 = SK.GetEntity(c.entityD)->VectorGetNum();
                double d1 = a1.Dot(b1), d2 = a2.Dot(b2);

                if(d1*d2 < 0) {
                    c.other = true;
                }
            }
            if (!dontAddConstraint) {
                AddConstraint(&c);
            }
            break;
        }
        case Command::RATIO:
            if(gs.lineSegments == 2 && gs.n == 2) {
                c.type = Type::LENGTH_RATIO;
                c.entityA = gs.entity[0];
                c.entityB = gs.entity[1];
            }
            else if(gs.arcs == 2 && gs.n == 2) {
                c.type = Type::ARC_ARC_LEN_RATIO;
                c.entityA = gs.entity[0];
                c.entityB = gs.entity[1];
            }
            else if(gs.lineSegments == 1 && gs.arcs == 1 && gs.n == 2) {
                c.type = Type::ARC_LINE_LEN_RATIO;
                if(SK.GetEntity(gs.entity[0])->type == Entity::Type::ARC_OF_CIRCLE) {
                    c.entityA = gs.entity[1];
                    c.entityB = gs.entity[0];
                } else {
                    c.entityA = gs.entity[0];
                    c.entityB = gs.entity[1];
                }
            } else {
                Error(_("Bad selection for length ratio constraint. This "
                        "constraint can apply to:\n\n"
                        "    * two line segments\n"
                        "    * two arcs\n"
                        "    * one arc and one line segment\n"));
                return;
            }

            c.valA = 0;
            c.ModifyToSatisfy();
            AddConstraint(&c);
            break;

        case Command::DIFFERENCE:
            if(gs.lineSegments == 2 && gs.n == 2) {
                c.type = Type::LENGTH_DIFFERENCE;
                c.entityA = gs.entity[0];
                c.entityB = gs.entity[1];
            }
            else if(gs.arcs == 2 && gs.n == 2) {
                c.type = Type::ARC_ARC_DIFFERENCE;
                c.entityA = gs.entity[0];
                c.entityB = gs.entity[1];
            }
            else if(gs.lineSegments == 1 && gs.arcs == 1 && gs.n == 2) {
                c.type = Type::ARC_LINE_DIFFERENCE;
                if(SK.GetEntity(gs.entity[0])->type == Entity::Type::ARC_OF_CIRCLE) {
                    c.entityA = gs.entity[1];
                    c.entityB = gs.entity[0];
                } else {
                    c.entityA = gs.entity[0];
                    c.entityB = gs.entity[1];
                }
            } else {
                Error(_("Bad selection for length difference constraint. This "
                        "constraint can apply to:\n\n"
                        "    * two line segments\n"
                        "    * two arcs\n"
                        "    * one arc and one line segment\n"));
                return;
            }

            c.valA = 0;
            c.ModifyToSatisfy();
            AddConstraint(&c);
            break;

        case Command::AT_MIDPOINT:
            if(gs.lineSegments == 1 && gs.points == 1 && gs.n == 2) {
                c.type = Type::AT_MIDPOINT;
                c.entityA = gs.entity[0];
                c.ptA = gs.point[0];

                // If a point is at-midpoint, then no reason to also constrain
                // it on-line; so auto-remove that.  Handle as one undo group.
                SS.UndoRemember();
                DeleteAllConstraintsFor(Type::PT_ON_LINE, c.entityA, c.ptA);
                AddConstraint(&c, /*rememberForUndo=*/false);
                break;
            } else if(gs.lineSegments == 1 && gs.workplanes == 1 && gs.n == 2) {
                c.type = Type::AT_MIDPOINT;
                int i = SK.GetEntity(gs.entity[0])->IsWorkplane() ? 1 : 0;
                c.entityA = gs.entity[i];
                c.entityB = gs.entity[1-i];
            } else {
                Error(_("Bad selection for at midpoint constraint. This "
                        "constraint can apply to:\n\n"
                        "    * a line segment and a point "
                              "(point at midpoint)\n"
                        "    * a line segment and a workplane "
                              "(line's midpoint on plane)\n"));
                return;
            }
            AddConstraint(&c);
            break;

        case Command::SYMMETRIC:
            if(gs.points == 2 &&
                                ((gs.workplanes == 1 && gs.n == 3) ||
                                 (gs.n == 2)))
            {
                if(gs.entities > 0)
                    c.entityA = gs.entity[0];
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];
                c.type = Type::SYMMETRIC;
            } else if(gs.lineSegments == 1 &&
                                ((gs.workplanes == 1 && gs.n == 2) ||
                                 (gs.n == 1)))
            {
                Entity *line;
                if(SK.GetEntity(gs.entity[0])->IsWorkplane()) {
                    line = SK.GetEntity(gs.entity[1]);
                    c.entityA = gs.entity[0];
                } else {
                    line = SK.GetEntity(gs.entity[0]);
                }
                c.ptA = line->point[0];
                c.ptB = line->point[1];
                c.type = Type::SYMMETRIC;
            } else if(SS.GW.LockedInWorkplane()
                        && gs.lineSegments == 2 && gs.n == 2)
            {
                Entity *l0 = SK.GetEntity(gs.entity[0]),
                       *l1 = SK.GetEntity(gs.entity[1]);

                if((l1->group != SS.GW.activeGroup) ||
                   (l1->construction && !(l0->construction)))
                {
                    swap(l0, l1);
                }
                c.ptA = l1->point[0];
                c.ptB = l1->point[1];
                c.entityA = l0->h;
                c.type = Type::SYMMETRIC_LINE;
            } else if(SS.GW.LockedInWorkplane()
                        && gs.lineSegments == 1 && gs.points == 2 && gs.n == 3)
            {
                c.ptA = gs.point[0];
                c.ptB = gs.point[1];
                c.entityA = gs.entity[0];
                c.type = Type::SYMMETRIC_LINE;
            } else {
                Error(_("Bad selection for symmetric constraint. This constraint "
                        "can apply to:\n\n"
                        "    * two points or a line segment "
                            "(symmetric about workplane's coordinate axis)\n"
                        "    * line segment, and two points or a line segment "
                            "(symmetric about line segment)\n"
                        "    * workplane, and two points or a line segment "
                            "(symmetric about workplane)\n"));
                return;
            }
            // We may remove constraints so remember manually
            if(c.entityA == Entity::NO_ENTITY) {
                // Horizontal / vertical symmetry, implicit symmetry plane
                // normal to the workplane
                if(c.workplane == Entity::FREE_IN_3D) {
                    Error(_("A workplane must be active when constraining "
                            "symmetric without an explicit symmetry plane."));
                    return;
                }
                Vector pa = SK.GetEntity(c.ptA)->PointGetNum();
                Vector pb = SK.GetEntity(c.ptB)->PointGetNum();
                Vector dp = pa.Minus(pb);
                EntityBase *norm = SK.GetEntity(c.workplane)->Normal();;
                Vector u = norm->NormalU(), v = norm->NormalV();
                if(fabs(dp.Dot(u)) > fabs(dp.Dot(v))) {
                    c.type = Type::SYMMETRIC_HORIZ;
                } else {
                    c.type = Type::SYMMETRIC_VERT;
                }
                if(gs.lineSegments == 1) {
                    // If this line segment is already constrained horiz or
                    // vert, then auto-remove that redundant constraint.
                    // Handle as one undo group.
                    SS.UndoRemember();
                    DeleteAllConstraintsFor(Type::HORIZONTAL, (gs.entity[0]),
                        Entity::NO_ENTITY);
                    DeleteAllConstraintsFor(Type::VERTICAL, (gs.entity[0]),
                        Entity::NO_ENTITY);
                    AddConstraint(&c, /*rememberForUndo=*/false);
                    break;
                }
            }
            AddConstraint(&c);
            break;

        case Command::VERTICAL:
        case Command::HORIZONTAL: {
            if(c.workplane == Entity::FREE_IN_3D) {
                Error(_("Activate a workplane (with Sketch -> In Workplane) before "
                        "applying a horizontal or vertical constraint."));
                return;
            }
            if(gs.lineSegments == gs.n) {
                for (int i = 0; i < gs.lineSegments; i++) {
                    Constraint c = {};
                    c.group = SS.GW.activeGroup;
                    c.workplane = SS.GW.ActiveWorkplane();
                    c.type = id == Command::HORIZONTAL ? Type::HORIZONTAL : Type::VERTICAL;
                    c.entityA = gs.entity[i];
                    DeleteAllConstraintsFor(c.type, gs.entity[i], Entity::NO_ENTITY);
                    AddConstraint(&c, i == 0);
                }
            } else if(gs.points % 2 == 0 && gs.n == gs.points) {
                for (int i = 0; i < gs.points / 2; i++) {
                    Constraint c = {};
                    c.group = SS.GW.activeGroup;
                    c.workplane = SS.GW.ActiveWorkplane();
                    c.type = id == Command::HORIZONTAL ? Type::HORIZONTAL : Type::VERTICAL;
                    int doubleI = i * 2;
                    c.ptA = gs.point[doubleI];
                    c.ptB = gs.point[doubleI + 1];
                    AddConstraint(&c, i == 0);
                }
            } else {
                Error(_("Bad selection for horizontal / vertical constraint. "
                        "This constraint can apply to:\n\n"
                        "    * two points\n"
                        "    * a line segment\n"));
                return;
            }
            break;
        }

        case Command::ORIENTED_SAME: {
            if(gs.anyNormals == 2 && gs.n == 2) {
                c.type = Type::SAME_ORIENTATION;
                c.entityA = gs.anyNormal[0];
                c.entityB = gs.anyNormal[1];
            } else {
                Error(_("Bad selection for same orientation constraint. This "
                        "constraint can apply to:\n\n"
                        "    * two normals\n"));
                return;
            }
            SS.UndoRemember();

            Entity *nfree = SK.GetEntity(c.entityA);
            Entity *nref  = SK.GetEntity(c.entityB);
            if(nref->group == SS.GW.activeGroup) {
                swap(nref, nfree);
            }
            if(nfree->group == SS.GW.activeGroup && nref->group != SS.GW.activeGroup) {
                // nfree is free, and nref is locked (since it came from a
                // previous group); so let's force nfree aligned to nref,
                // and make convergence easy
                Vector ru = nref ->NormalU(), rv = nref ->NormalV();
                Vector fu = nfree->NormalU(), fv = nfree->NormalV();

                if(fabs(fu.Dot(ru)) < fabs(fu.Dot(rv))) {
                    // There might be an odd*90 degree rotation about the
                    // normal vector; allow that, since the numerical
                    // constraint does
                    swap(ru, rv);
                }
                fu = fu.Dot(ru) > 0 ? ru : ru.ScaledBy(-1);
                fv = fv.Dot(rv) > 0 ? rv : rv.ScaledBy(-1);

                nfree->NormalForceTo(Quaternion::From(fu, fv));
            }
            AddConstraint(&c, /*rememberForUndo=*/false);
            break;
        }

        case Command::OTHER_ANGLE:
            if(gs.constraints == 1 && gs.n == 0) {
                Constraint *c = SK.GetConstraint(gs.constraint[0]);
                if(c->type == Type::ANGLE) {
                    SS.UndoRemember();
                    c->other = !(c->other);
                    c->ModifyToSatisfy();
                    break;
                }
                if(c->type == Type::EQUAL_ANGLE) {
                    SS.UndoRemember();
                    c->other = !(c->other);
                    SS.MarkGroupDirty(c->group);
                    break;
                }
            }
            Error(_("Must select an angle constraint."));
            return;

        case Command::REFERENCE:
            if(gs.constraints == 1 && gs.n == 0) {
                Constraint *c = SK.GetConstraint(gs.constraint[0]);
                if(c->HasLabel() && c->type != Type::COMMENT) {
                    SS.UndoRemember();
                    (c->reference) = !(c->reference);
                    SS.MarkGroupDirty(c->group, /*onlyThis=*/true);
                    break;
                }
            }
            Error(_("Must select a constraint with associated label."));
            return;

        case Command::ANGLE:
        case Command::REF_ANGLE: {
            if(gs.vectors == 2 && gs.n == 2) {
                c.type = Type::ANGLE;
                c.entityA = gs.vector[0];
                c.entityB = gs.vector[1];
                c.valA = 0;
            } else {
                Error(_("Bad selection for angle constraint. This constraint "
                        "can apply to:\n\n"
                        "    * two line segments\n"
                        "    * a line segment and a normal\n"
                        "    * two normals\n"));
                return;
            }

            Entity *ea = SK.GetEntity(c.entityA),
                   *eb = SK.GetEntity(c.entityB);
            if(ea->type == Entity::Type::LINE_SEGMENT &&
               eb->type == Entity::Type::LINE_SEGMENT)
            {
                Vector a0 = SK.GetEntity(ea->point[0])->PointGetNum(),
                       a1 = SK.GetEntity(ea->point[1])->PointGetNum(),
                       b0 = SK.GetEntity(eb->point[0])->PointGetNum(),
                       b1 = SK.GetEntity(eb->point[1])->PointGetNum();
                if(a0.Equals(b0) || a1.Equals(b1)) {
                    // okay, vectors should be drawn in same sense
                } else if(a0.Equals(b1) || a1.Equals(b0)) {
                    // vectors are in opposite sense
                    c.other = true;
                } else {
                    // no shared point; not clear which intersection to draw
                }
            }

            if(id == Command::REF_ANGLE) {
                c.reference = true;
            }

            c.ModifyToSatisfy();
            AddConstraint(&c);
            break;
        }

        case Command::PARALLEL:
            if(gs.vectors == 2 && gs.n == 2) {
                c.type = Type::PARALLEL;
                c.entityA = gs.vector[0];
                c.entityB = gs.vector[1];
            } else if(gs.lineSegments == 1 && gs.arcs == 1 && gs.n == 2) {
                Entity *line = SK.GetEntity(gs.entity[0]),
                       *arc  = SK.GetEntity(gs.entity[1]);
                if(line->type == Entity::Type::ARC_OF_CIRCLE) {
                    swap(line, arc);
                }
                if(!ConstrainArcLineTangent(&c, line, arc)) {
                    return;
                }
                c.type = Type::ARC_LINE_TANGENT;
                c.entityA = arc->h;
                c.entityB = line->h;
            } else if(gs.lineSegments == 1 && gs.cubics == 1 && gs.n == 2) {
                Entity *line  = SK.GetEntity(gs.entity[0]),
                       *cubic = SK.GetEntity(gs.entity[1]);
                if(line->type == Entity::Type::CUBIC) {
                    swap(line, cubic);
                }
                if(!ConstrainCubicLineTangent(&c, line, cubic)) {
                    return;
                }
                c.type = Type::CUBIC_LINE_TANGENT;
                c.entityA = cubic->h;
                c.entityB = line->h;
            } else if(gs.cubics + gs.arcs == 2 && gs.n == 2) {
                if(!SS.GW.LockedInWorkplane()) {
                    Error(_("Curve-curve tangency must apply in workplane."));
                    return;
                }
                Entity *eA = SK.GetEntity(gs.entity[0]),
                       *eB = SK.GetEntity(gs.entity[1]);
                if(!ConstrainCurveCurveTangent(&c, eA, eB)) {
                    return;
                }
                c.type = Type::CURVE_CURVE_TANGENT;
                c.entityA = eA->h;
                c.entityB = eB->h;
            } else {
                Error(_("Bad selection for parallel / tangent constraint. This "
                        "constraint can apply to:\n\n"
                        "    * two line segments (parallel)\n"
                        "    * a line segment and a normal (parallel)\n"
                        "    * two normals (parallel)\n"
                        "    * two line segments, arcs, or beziers, that share "
                              "an endpoint (tangent)\n"));
                return;
            }
            AddConstraint(&c);
            break;

        case Command::PERPENDICULAR:
            if(gs.vectors == 2 && gs.n == 2) {
                c.type = Type::PERPENDICULAR;
                c.entityA = gs.vector[0];
                c.entityB = gs.vector[1];
            } else {
                Error(_("Bad selection for perpendicular constraint. This "
                        "constraint can apply to:\n\n"
                        "    * two line segments\n"
                        "    * a line segment and a normal\n"
                        "    * two normals\n"));
                return;
            }
            AddConstraint(&c);
            break;

        case Command::WHERE_DRAGGED:
            if(gs.points == 1 && gs.n == 1) {
                c.type = Type::WHERE_DRAGGED;
                c.ptA = gs.point[0];
            } else {
                Error(_("Bad selection for lock point where dragged constraint. "
                        "This constraint can apply to:\n\n"
                        "    * a point\n"));
                return;
            }
            AddConstraint(&c);
            break;

        case Command::COMMENT:
            if(gs.points == 1 && gs.n == 1) {
                c.type = Type::COMMENT;
                c.ptA = gs.point[0];
                c.group       = SS.GW.activeGroup;
                c.workplane   = SS.GW.ActiveWorkplane();
                c.comment     = _("NEW COMMENT -- DOUBLE-CLICK TO EDIT");
                AddConstraint(&c);
            } else {
                SS.GW.pending.operation = GraphicsWindow::Pending::COMMAND;
                SS.GW.pending.command = Command::COMMENT;
                SS.GW.pending.description = _("click center of comment text");
                SS.ScheduleShowTW();
            }
            break;

        default: ssassert(false, "Unexpected menu ID");
    }

    for(const Constraint &cc : SK.constraint) {
        if(c.h != cc.h && c.Equals(cc)) {
            // Oops, we already have this exact constraint. Remove the one we just added.
            SK.constraint.RemoveById(c.h);
            SS.GW.ClearSelection();
            // And now select the old one, to give feedback.
            SS.GW.MakeSelected(cc.h);
            return;
        }
    }

    if(SK.constraint.FindByIdNoOops(c.h)) {
        Constraint *constraint = SK.GetConstraint(c.h);
        if(SS.TestRankForGroup(c.group) == SolveResult::REDUNDANT_OKAY &&
                !SK.GetGroup(SS.GW.activeGroup)->allowRedundant &&
                constraint->HasLabel()) {
            constraint->reference = true;
        }
    }

    if((id == Command::DISTANCE_DIA || id == Command::ANGLE ||
        id == Command::RATIO || id == Command::DIFFERENCE) &&
       SS.immediatelyEditDimension) {
        SS.GW.EditConstraint(c.h);
    }

    SS.GW.ClearSelection();
}

#endif /* ! LIBRARY */
