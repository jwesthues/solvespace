#include "solvespace.h"

//-----------------------------------------------------------------------------
// Replace a point-coincident constraint on oldpt with that same constraint
// on newpt. Useful when splitting or tangent arcing.
//-----------------------------------------------------------------------------
void GraphicsWindow::ReplacePointInConstraints(hEntity oldpt, hEntity newpt) {
    int i;
    for(i = 0; i < SK.constraint.n; i++) {
        Constraint *c = &(SK.constraint.elem[i]);

        if(c->type == Constraint::POINTS_COINCIDENT) {
            if(c->ptA.v == oldpt.v) c->ptA = newpt;
            if(c->ptB.v == oldpt.v) c->ptB = newpt;
        }
    }
}

//-----------------------------------------------------------------------------
// A single point must be selected when this function is called. We find two
// non-construction line segments that join at this point, and create a
// tangent arc joining them.
//-----------------------------------------------------------------------------
void GraphicsWindow::MakeTangentArc(void) {
    if(!LockedInWorkplane()) {
        Error("Must be sketching in workplane to create tangent "
              "arc.");
        return;
    }

    // Find two line segments that join at the specified point,
    // and blend them with a tangent arc. First, find the
    // requests that generate the line segments.
    Vector pshared = SK.GetEntity(gs.point[0])->PointGetNum();
    ClearSelection();

    int i, c = 0;
    Entity *line[2];
    Request *lineReq[2];
    bool point1[2];
    for(i = 0; i < SK.request.n; i++) {
        Request *r = &(SK.request.elem[i]);
        if(r->group.v != activeGroup.v) continue;
        if(r->type != Request::LINE_SEGMENT) continue;
        if(r->construction) continue;

        Entity *e = SK.GetEntity(r->h.entity(0));
        Vector p0 = SK.GetEntity(e->point[0])->PointGetNum(),
               p1 = SK.GetEntity(e->point[1])->PointGetNum();
        
        if(p0.Equals(pshared) || p1.Equals(pshared)) {
            if(c < 2) {
                line[c] = e;
                lineReq[c] = r;
                point1[c] = (p1.Equals(pshared));
            }
            c++;
        }
    }
    if(c != 2) {
        Error("To create a tangent arc, select a point where "
              "two non-construction line segments join.");
        return;
    }

    SS.UndoRemember();

    Entity *wrkpl = SK.GetEntity(ActiveWorkplane());
    Vector wn = wrkpl->Normal()->NormalN();

    hEntity hshared = (line[0])->point[point1[0] ? 1 : 0],
            hother0 = (line[0])->point[point1[0] ? 0 : 1],
            hother1 = (line[1])->point[point1[1] ? 0 : 1];

    Vector pother0 = SK.GetEntity(hother0)->PointGetNum(),
           pother1 = SK.GetEntity(hother1)->PointGetNum();

    Vector v0shared = pshared.Minus(pother0),
           v1shared = pshared.Minus(pother1);

    hEntity srcline0 = (line[0])->h,
            srcline1 = (line[1])->h;

    (lineReq[0])->construction = true;
    (lineReq[1])->construction = true;

    // And thereafter we mustn't touch the entity or req ptrs,
    // because the new requests/entities we add might force a
    // realloc.
    memset(line, 0, sizeof(line));
    memset(lineReq, 0, sizeof(lineReq));

    // The sign of vv determines whether shortest distance is
    // clockwise or anti-clockwise.
    Vector v = (wn.Cross(v0shared)).WithMagnitude(1);
    double vv = v1shared.Dot(v);

    double dot = (v0shared.WithMagnitude(1)).Dot(
                  v1shared.WithMagnitude(1));
    double theta = acos(dot);
    double r = 200/scale;
    // Set the radius so that no more than one third of the 
    // line segment disappears.
    r = min(r, v0shared.Magnitude()*tan(theta/2)/3);
    r = min(r, v1shared.Magnitude()*tan(theta/2)/3);
    double el = r/tan(theta/2);

    hRequest rln0 = AddRequest(Request::LINE_SEGMENT, false),
             rln1 = AddRequest(Request::LINE_SEGMENT, false);
    hRequest rarc = AddRequest(Request::ARC_OF_CIRCLE, false);

    Entity *ln0 = SK.GetEntity(rln0.entity(0)),
           *ln1 = SK.GetEntity(rln1.entity(0));
    Entity *arc = SK.GetEntity(rarc.entity(0));

    SK.GetEntity(ln0->point[0])->PointForceTo(pother0);
    Constraint::ConstrainCoincident(ln0->point[0], hother0);

    SK.GetEntity(ln1->point[0])->PointForceTo(pother1);
    Constraint::ConstrainCoincident(ln1->point[0], hother1);

    Vector arc0 = pshared.Minus(v0shared.WithMagnitude(el));
    Vector arc1 = pshared.Minus(v1shared.WithMagnitude(el));

    SK.GetEntity(ln0->point[1])->PointForceTo(arc0);
    SK.GetEntity(ln1->point[1])->PointForceTo(arc1);

    Constraint::Constrain(Constraint::PT_ON_LINE,
        ln0->point[1], Entity::NO_ENTITY, srcline0);
    Constraint::Constrain(Constraint::PT_ON_LINE,
        ln1->point[1], Entity::NO_ENTITY, srcline1);

    Vector center = arc0;
    int a, b;
    if(vv < 0) {
        a = 1; b = 2;
        center = center.Minus(v0shared.Cross(wn).WithMagnitude(r));
    } else {
        a = 2; b = 1;
        center = center.Plus(v0shared.Cross(wn).WithMagnitude(r));
    }

    SK.GetEntity(arc->point[0])->PointForceTo(center);
    SK.GetEntity(arc->point[a])->PointForceTo(arc0);
    SK.GetEntity(arc->point[b])->PointForceTo(arc1);

    Constraint::ConstrainCoincident(arc->point[a], ln0->point[1]);
    Constraint::ConstrainCoincident(arc->point[b], ln1->point[1]);

    Constraint::Constrain(Constraint::ARC_LINE_TANGENT,
        Entity::NO_ENTITY, Entity::NO_ENTITY,
        arc->h, ln0->h, (a==2));
    Constraint::Constrain(Constraint::ARC_LINE_TANGENT,
        Entity::NO_ENTITY, Entity::NO_ENTITY,
        arc->h, ln1->h, (b==2));

    SS.later.generateAll = true;
}

hEntity GraphicsWindow::SplitLine(hEntity he, Vector pinter) {
    // Save the original endpoints, since we're about to delete this entity.
    Entity *e01 = SK.GetEntity(he);
    hEntity hep0 = e01->point[0], hep1 = e01->point[1];
    Vector p0 = SK.GetEntity(hep0)->PointGetNum(),
           p1 = SK.GetEntity(hep1)->PointGetNum();

    SS.UndoRemember();

    // Add the two line segments this one gets split into.
    hRequest r0i = AddRequest(Request::LINE_SEGMENT, false),
             ri1 = AddRequest(Request::LINE_SEGMENT, false);
    // Don't get entities till after adding, realloc issues

    Entity *e0i = SK.GetEntity(r0i.entity(0)),
           *ei1 = SK.GetEntity(ri1.entity(0));

    SK.GetEntity(e0i->point[0])->PointForceTo(p0);
    SK.GetEntity(e0i->point[1])->PointForceTo(pinter);
    SK.GetEntity(ei1->point[0])->PointForceTo(pinter);
    SK.GetEntity(ei1->point[1])->PointForceTo(p1);

    ReplacePointInConstraints(hep0, e0i->point[0]);
    ReplacePointInConstraints(hep1, ei1->point[1]);
    Constraint::ConstrainCoincident(e0i->point[1], ei1->point[0]);
    return e0i->point[1];
}

hEntity GraphicsWindow::SplitCircle(hEntity he, Vector pinter) {
    SS.UndoRemember();

    Entity *circle = SK.GetEntity(he);
    if(circle->type == Entity::CIRCLE) {
        // Start with an unbroken circle, split it into a 360 degree arc.
        Vector center = SK.GetEntity(circle->point[0])->PointGetNum();

        circle = NULL; // shortly invalid!
        hRequest hr = AddRequest(Request::ARC_OF_CIRCLE, false);

        Entity *arc = SK.GetEntity(hr.entity(0));

        SK.GetEntity(arc->point[0])->PointForceTo(center);
        SK.GetEntity(arc->point[1])->PointForceTo(pinter);
        SK.GetEntity(arc->point[2])->PointForceTo(pinter);

        Constraint::ConstrainCoincident(arc->point[1], arc->point[2]);
        return arc->point[1];
    } else {
        // Start with an arc, break it in to two arcs
        hEntity hc = circle->point[0],
                hs = circle->point[1],
                hf = circle->point[2];
        Vector center = SK.GetEntity(hc)->PointGetNum(),
               start  = SK.GetEntity(hs)->PointGetNum(),
               finish = SK.GetEntity(hf)->PointGetNum();

        circle = NULL; // shortly invalid!
        hRequest hr0 = AddRequest(Request::ARC_OF_CIRCLE, false),
                 hr1 = AddRequest(Request::ARC_OF_CIRCLE, false);

        Entity *arc0 = SK.GetEntity(hr0.entity(0)),
               *arc1 = SK.GetEntity(hr1.entity(0));

        SK.GetEntity(arc0->point[0])->PointForceTo(center);
        SK.GetEntity(arc0->point[1])->PointForceTo(start);
        SK.GetEntity(arc0->point[2])->PointForceTo(pinter);

        SK.GetEntity(arc1->point[0])->PointForceTo(center);
        SK.GetEntity(arc1->point[1])->PointForceTo(pinter);
        SK.GetEntity(arc1->point[2])->PointForceTo(finish);

        ReplacePointInConstraints(hs, arc0->point[1]);
        ReplacePointInConstraints(hf, arc1->point[2]);
        Constraint::ConstrainCoincident(arc0->point[2], arc1->point[1]);
        return arc0->point[2];
    }
}

hEntity GraphicsWindow::SplitCubic(hEntity he, Vector pinter) {
    // Save the original endpoints, since we're about to delete this entity.
    Entity *e01 = SK.GetEntity(he);
    hEntity hep0 = e01->point[0],
            hep1 = e01->point[1],
            hep2 = e01->point[2],
            hep3 = e01->point[3];
    Vector p0 = SK.GetEntity(hep0)->PointGetNum(),
           p1 = SK.GetEntity(hep1)->PointGetNum(),
           p2 = SK.GetEntity(hep2)->PointGetNum(),
           p3 = SK.GetEntity(hep3)->PointGetNum();

    SS.UndoRemember();

    SBezier b0i, bi1, b01 = SBezier::From(p0, p1, p2, p3);
    double t;
    b01.ClosestPointTo(pinter, &t, true);
    b01.SplitAt(t, &b0i, &bi1);

    // Add the two line segments this one gets split into.
    hRequest r0i = AddRequest(Request::CUBIC, false),
             ri1 = AddRequest(Request::CUBIC, false);
    // Don't get entities till after adding, realloc issues

    Entity *e0i = SK.GetEntity(r0i.entity(0)),
           *ei1 = SK.GetEntity(ri1.entity(0));

    SK.GetEntity(e0i->point[0])->PointForceTo(b0i.ctrl[0]);
    SK.GetEntity(e0i->point[1])->PointForceTo(b0i.ctrl[1]);
    SK.GetEntity(e0i->point[2])->PointForceTo(b0i.ctrl[2]);
    SK.GetEntity(e0i->point[3])->PointForceTo(b0i.ctrl[3]);

    SK.GetEntity(ei1->point[0])->PointForceTo(bi1.ctrl[0]);
    SK.GetEntity(ei1->point[1])->PointForceTo(bi1.ctrl[1]);
    SK.GetEntity(ei1->point[2])->PointForceTo(bi1.ctrl[2]);
    SK.GetEntity(ei1->point[3])->PointForceTo(bi1.ctrl[3]);

    ReplacePointInConstraints(hep0, e0i->point[0]);
    ReplacePointInConstraints(hep1, ei1->point[3]);
    Constraint::ConstrainCoincident(e0i->point[3], ei1->point[0]);
    return e0i->point[3];
}

hEntity GraphicsWindow::SplitEntity(hEntity he, Vector pinter) {
    Entity *e = SK.GetEntity(he);
    int entityType = e->type;

    hEntity ret;
    if(e->IsCircle()) {
        ret = SplitCircle(he, pinter);
    } else if(e->type == Entity::LINE_SEGMENT) {
        ret = SplitLine(he, pinter);
    } else if(e->type == Entity::CUBIC) {
        ret = SplitCubic(he, pinter);
    } else {
        Error("Couldn't split this entity; lines, circles, or cubics only.");
        return Entity::NO_ENTITY;
    }

    // Finally, delete the request that generated the original entity.
    int reqType;
    switch(entityType) {
        case Entity::CIRCLE:        reqType = Request::CIRCLE; break;
        case Entity::ARC_OF_CIRCLE: reqType = Request::ARC_OF_CIRCLE; break;
        case Entity::LINE_SEGMENT:  reqType = Request::LINE_SEGMENT; break;
        case Entity::CUBIC:         reqType = Request::CUBIC; break;
        default: oops();
    }
    int i;
    SK.request.ClearTags();
    for(i = 0; i < SK.request.n; i++) {
        Request *r = &(SK.request.elem[i]);
        if(r->group.v != activeGroup.v) continue;
        if(r->type != reqType) continue;
    
        // If the user wants to keep the old entities around, they can just
        // mark them construction first.
        if(he.v == r->h.entity(0).v && !r->construction) {
            r->tag = 1;
            break;
        }
    }
    DeleteTaggedRequests();

    return ret;
}

void GraphicsWindow::SplitLinesOrCurves(void) {
    if(!LockedInWorkplane()) {
        Error("Must be sketching in workplane to split.");
        return;
    }

    GroupSelection();
    if(!(gs.n == 2 && (gs.lineSegments + gs.circlesOrArcs + gs.cubics) == 2)) {
        Error("Select two entities that intersect each other (e.g. two lines "
              "or two circles or a circle and a line).");
        return;
    }

    hEntity ha = gs.entity[0],
            hb = gs.entity[1];
    Entity *ea = SK.GetEntity(ha),
           *eb = SK.GetEntity(hb);
   
    // Compute the possibly-rational Bezier curves for each of these entities
    SBezierList sbla, sblb;
    ZERO(&sbla);
    ZERO(&sblb);
    ea->GenerateBezierCurves(&sbla);
    eb->GenerateBezierCurves(&sblb);
    // and then compute the points where they intersect, based on those curves.
    SPointList inters;
    ZERO(&inters);
    sbla.AllIntersectionsWith(&sblb, &inters);

    // If there's multiple points, then just take the first one.
    if(inters.l.n > 0) {
        Vector pi = inters.l.elem[0].p;
        hEntity hia = SplitEntity(ha, pi),
                hib = SplitEntity(hb, pi);
        // SplitEntity adds the coincident constraints to join the split halves
        // of each original entity; and then we add the constraint to join
        // the two entities together at the split point.
        if(hia.v && hib.v) {
            Constraint::ConstrainCoincident(hia, hib);
        }
    } else {
        Error("Can't split; no intersection found.");
    }

    // All done, clean up and regenerate.
    inters.Clear();
    sbla.Clear();
    sblb.Clear();
    ClearSelection();
    SS.later.generateAll = true;
}

