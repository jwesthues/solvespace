//-----------------------------------------------------------------------------
// Routines to generate our watertight brep shells from the operations
// and entities specified by the user in each group; templated to work either
// on an SShell of ratpoly surfaces or on an SMesh of triangles.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "iteration.h"

void Group::AssembleLoops(bool *allClosed,
                          bool *allCoplanar,
                          bool *allNonZeroLen)
{
    SBezierList sbl = {};

    int i;
    for(auto &e : SK.entity) {
        if(e.group != h)
            continue;
        if(e.construction)
            continue;
        if(e.forceHidden)
            continue;

        e.GenerateBezierCurves(&sbl);
    }

    SBezier *sb;
    *allNonZeroLen = true;
    for(sb = sbl.l.First(); sb; sb = sbl.l.NextAfter(sb)) {
        for(i = 1; i <= sb->deg; i++) {
            if(!(sb->ctrl[i]).Equals(sb->ctrl[0])) {
                break;
            }
        }
        if(i > sb->deg) {
            // This is a zero-length edge.
            *allNonZeroLen = false;
            polyError.errorPointAt = sb->ctrl[0];
            goto done;
        }
    }

    // Try to assemble all these Beziers into loops. The closed loops go into
    // bezierLoops, with the outer loops grouped with their holes. The
    // leftovers, if any, go in bezierOpens.
    bezierLoops.FindOuterFacesFrom(&sbl, &polyLoops, NULL,
                                   SS.ChordTolMm(),
                                   allClosed, &(polyError.notClosedAt),
                                   allCoplanar, &(polyError.errorPointAt),
                                   &bezierOpens);
    done:
    sbl.Clear();
}

void Group::GenerateLoops() {
    polyLoops.Clear();
    bezierLoops.Clear();
    bezierOpens.Clear();

    if(type == Type::DRAWING_3D || type == Type::DRAWING_WORKPLANE ||
       type == Type::ROTATE || type == Type::TRANSLATE || type == Type::LINKED)
    {
        bool allClosed = false, allCoplanar = false, allNonZeroLen = false;
        AssembleLoops(&allClosed, &allCoplanar, &allNonZeroLen);
        if(!allNonZeroLen) {
            polyError.how = PolyError::ZERO_LEN_EDGE;
        } else if(!allCoplanar) {
            polyError.how = PolyError::NOT_COPLANAR;
        } else if(!allClosed) {
            polyError.how = PolyError::NOT_CLOSED;
        } else {
            polyError.how = PolyError::GOOD;
            // The self-intersecting check is kind of slow, so don't run it
            // unless requested.
            if(SS.checkClosedContour) {
                if(polyLoops.SelfIntersecting(&(polyError.errorPointAt))) {
                    polyError.how = PolyError::SELF_INTERSECTING;
                }
            }
        }
    }
}

void SShell::RemapFaces(Group *g, int remap) {
    SSurface *ss;
    for(ss = surface.First(); ss; ss = surface.NextAfter(ss)){
        hEntity face = { ss->face };
        if(face == Entity::NO_ENTITY) continue;

        face = g->Remap(face, remap);
        ss->face = face.v;
    }
}

void SMesh::RemapFaces(Group *g, int remap) {
    STriangle *tr;
    for(tr = l.First(); tr; tr = l.NextAfter(tr)) {
        hEntity face = { tr->meta.face };
        if(face == Entity::NO_ENTITY) continue;

        face = g->Remap(face, remap);
        tr->meta.face = face.v;
    }
}

template<class T>
void Group::GenerateForStepAndRepeat(T *steps, T *outs, Group::CombineAs forWhat) {
    T workA, workB;
    workA = {};
    workB = {};
    T *soFar = &workA, *scratch = &workB;

    int n = (int)valA, a0 = 0;
    if(subtype == Subtype::ONE_SIDED && skipFirst) {
        a0++; n++;
    }
    int a;
    for(a = a0; a < n; a++) {
        int ap = a*2 - (subtype == Subtype::ONE_SIDED ? 0 : (n-1));
        int remap = (a == (n - 1)) ? REMAP_LAST : a;

        T transd = {};
        if(type == Type::TRANSLATE) {
            Vector trans = Vector::From(h.param(0), h.param(1), h.param(2));
            trans = trans.ScaledBy(ap);
            transd.MakeFromTransformationOf(steps,
                trans, Quaternion::IDENTITY, 1.0);
        } else {
            Vector trans = Vector::From(h.param(0), h.param(1), h.param(2));
            double theta = ap * SK.GetParam(h.param(3))->val;
            double c = cos(theta), s = sin(theta);
            Vector axis = Vector::From(h.param(4), h.param(5), h.param(6));
            Quaternion q = Quaternion::From(c, s*axis.x, s*axis.y, s*axis.z);
            // Rotation is centered at t; so A(x - t) + t = Ax + (t - At)
            transd.MakeFromTransformationOf(steps,
                trans.Minus(q.Rotate(trans)), q, 1.0);
        }

        // We need to rewrite any plane face entities to the transformed ones.
        transd.RemapFaces(this, remap);

        // And tack this transformed copy on to the return.
        if(soFar->IsEmpty()) {
            scratch->MakeFromCopyOf(&transd);
        } else if (forWhat == CombineAs::ASSEMBLE) {
            scratch->MakeFromAssemblyOf(soFar, &transd);
        } else {
            scratch->MakeFromUnionOf(soFar, &transd);
        }

        swap(scratch, soFar);
        scratch->Clear();
        transd.Clear();
    }

    outs->Clear();
    *outs = *soFar;
}

template<class T>
void Group::GenerateForBoolean(T *prevs, T *thiss, T *outs, Group::CombineAs how) {
    // If this group contributes no new mesh, then our running mesh is the
    // same as last time, no combining required. Likewise if we have a mesh
    // but it's suppressed.
    if(thiss->IsEmpty() || suppress) {
        outs->MakeFromCopyOf(prevs);
        return;
    }

    // So our group's shell appears in thisShell. Combine this with the
    // previous group's shell, using the requested operation.
    if(how == CombineAs::UNION) {
        outs->MakeFromUnionOf(prevs, thiss);
    } else if(how == CombineAs::DIFFERENCE) {
        outs->MakeFromDifferenceOf(prevs, thiss);
    } else {
        outs->MakeFromAssemblyOf(prevs, thiss);
    }
}

void Group::GenerateShellAndMesh() {
    bool prevBooleanFailed = booleanFailed;
    booleanFailed = false;

    Group *srcg = this;

    thisShell.Clear();
    thisMesh.Clear();
    runningShell.Clear();
    runningMesh.Clear();

    // Don't attempt a lathe or extrusion unless the source section is good:
    // planar and not self-intersecting.
    bool haveSrc = true;
    if(type == Type::EXTRUDE || type == Type::LATHE || type == Type::REVOLVE) {
        Group *src = SK.GetGroup(opA);
        if(src->polyError.how != PolyError::GOOD) {
            haveSrc = false;
        }
    }

    if(type == Type::TRANSLATE || type == Type::ROTATE) {
        // A step and repeat gets merged against the group's previous group,
        // not our own previous group.
        srcg = SK.GetGroup(opA);

        if(!srcg->suppress) {
            if(!IsForcedToMesh()) {
                GenerateForStepAndRepeat<SShell>(&(srcg->thisShell), &thisShell, srcg->meshCombine);
            } else {
                SMesh prevm = {};
                prevm.MakeFromCopyOf(&srcg->thisMesh);
                srcg->thisShell.TriangulateInto(&prevm);
                GenerateForStepAndRepeat<SMesh> (&prevm, &thisMesh, srcg->meshCombine);
            }
        }
    } else if(type == Type::EXTRUDE && haveSrc) {
        Group *src = SK.GetGroup(opA);
        Vector translate = Vector::From(h.param(0), h.param(1), h.param(2));

        Vector tbot, ttop;
        if(subtype == Subtype::ONE_SIDED) {
            tbot = Vector::From(0, 0, 0); ttop = translate.ScaledBy(2);
        } else {
            tbot = translate.ScaledBy(-1); ttop = translate.ScaledBy(1);
        }

        SBezierLoopSetSet *sblss = &(src->bezierLoops);
        SBezierLoopSet *sbls;
        for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
            int is = thisShell.surface.n;
            // Extrude this outer contour (plus its inner contours, if present)
            thisShell.MakeFromExtrusionOf(sbls, tbot, ttop, color);

            // And for any plane faces, annotate the model with the entity for
            // that face, so that the user can select them with the mouse.
            Vector onOrig = sbls->point;
            for(auto ss : indices(thisShell.surface, is)) {
                hEntity face = Entity::NO_ENTITY;

                Vector p = ss->PointAt(0, 0),
                       n = ss->NormalAt(0, 0).WithMagnitude(1);
                double d = n.Dot(p);

                if(ss.GetIndex() <= (is + 1)) {
                    // These are the top and bottom of the shell.
                    if(fabs((onOrig.Plus(ttop)).Dot(n) - d) < LENGTH_EPS) {
                        face = Remap(Entity::NO_ENTITY, REMAP_TOP);
                        ss->face = face.v;
                    }
                    if(fabs((onOrig.Plus(tbot)).Dot(n) - d) < LENGTH_EPS) {
                        face = Remap(Entity::NO_ENTITY, REMAP_BOTTOM);
                        ss->face = face.v;
                    }
                    continue;
                }

                // So these are the sides
                if(ss->degm != 1 || ss->degn != 1) continue;

                Entity *e;
                for(e = SK.entity.First(); e; e = SK.entity.NextAfter(e)) {
                    if(e->group != opA) continue;
                    if(e->type != Entity::Type::LINE_SEGMENT) continue;

                    Vector a = SK.GetEntity(e->point[0])->PointGetNum(),
                           b = SK.GetEntity(e->point[1])->PointGetNum();
                    a = a.Plus(ttop);
                    b = b.Plus(ttop);
                    // Could get taken backwards, so check all cases.
                    if((a.Equals(ss->ctrl[0][0]) && b.Equals(ss->ctrl[1][0])) ||
                       (b.Equals(ss->ctrl[0][0]) && a.Equals(ss->ctrl[1][0])) ||
                       (a.Equals(ss->ctrl[0][1]) && b.Equals(ss->ctrl[1][1])) ||
                       (b.Equals(ss->ctrl[0][1]) && a.Equals(ss->ctrl[1][1])))
                    {
                        face = Remap(e->h, REMAP_LINE_TO_FACE);
                        ss->face = face.v;
                        break;
                    }
                }
            }
        }
    } else if(type == Type::LATHE && haveSrc) {
        Group *src = SK.GetGroup(opA);

        Vector pt   = SK.GetEntity(predef.origin)->PointGetNum(),
               axis = SK.GetEntity(predef.entityB)->VectorGetNum();
        axis = axis.WithMagnitude(1);

        SBezierLoopSetSet *sblss = &(src->bezierLoops);
        SBezierLoopSet *sbls;
        for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
            thisShell.MakeFromRevolutionOf(sbls, pt, axis, color, this);
        }
    } else if(type == Type::REVOLVE && haveSrc) {
        Group *src    = SK.GetGroup(opA);
        double anglef = SK.GetParam(h.param(3))->val * 4; // why the 4 is needed?
        double dists = 0, distf = 0;
        double angles = 0.0;
        if(subtype != Subtype::ONE_SIDED) {
            anglef *= 0.5;
            angles = -anglef;
        }
        Vector pt   = SK.GetEntity(predef.origin)->PointGetNum(),
               axis = SK.GetEntity(predef.entityB)->VectorGetNum();
        axis        = axis.WithMagnitude(1);

        SBezierLoopSetSet *sblss = &(src->bezierLoops);
        SBezierLoopSet *sbls;
        for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
            if(fabs(anglef - angles) < 2 * PI) {
                thisShell.MakeFromHelicalRevolutionOf(sbls, pt, axis, color, this,
                                                      angles, anglef, dists, distf);
            } else {
                thisShell.MakeFromRevolutionOf(sbls, pt, axis, color, this);
            }
        }
    } else if(type == Type::HELIX && haveSrc) {
        Group *src    = SK.GetGroup(opA);
        double anglef = SK.GetParam(h.param(3))->val * 4; // why the 4 is needed?
        double dists = 0, distf = 0;
        double angles = 0.0;
        distf = SK.GetParam(h.param(7))->val * 2; // dist is applied twice
        if(subtype != Subtype::ONE_SIDED) {
            anglef *= 0.5;
            angles = -anglef;
            distf *= 0.5;
            dists = -distf;
        }
        Vector pt   = SK.GetEntity(predef.origin)->PointGetNum(),
               axis = SK.GetEntity(predef.entityB)->VectorGetNum();
        axis        = axis.WithMagnitude(1);

        SBezierLoopSetSet *sblss = &(src->bezierLoops);
        SBezierLoopSet *sbls;
        for(sbls = sblss->l.First(); sbls; sbls = sblss->l.NextAfter(sbls)) {
            thisShell.MakeFromHelicalRevolutionOf(sbls, pt, axis, color, this,
                                                  angles, anglef, dists, distf);
        }
    } else if(type == Type::LINKED) {
        // The imported shell or mesh are copied over, with the appropriate
        // transformation applied. We also must remap the face entities.
        Vector offset = {
            SK.GetParam(h.param(0))->val,
            SK.GetParam(h.param(1))->val,
            SK.GetParam(h.param(2))->val };
        Quaternion q = {
            SK.GetParam(h.param(3))->val,
            SK.GetParam(h.param(4))->val,
            SK.GetParam(h.param(5))->val,
            SK.GetParam(h.param(6))->val };

        thisMesh.MakeFromTransformationOf(&impMesh, offset, q, scale);
        thisMesh.RemapFaces(this, 0);

        thisShell.MakeFromTransformationOf(&impShell, offset, q, scale);
        thisShell.RemapFaces(this, 0);
    }

    if(srcg->meshCombine != CombineAs::ASSEMBLE) {
        thisShell.MergeCoincidentSurfaces();
    }

    // So now we've got the mesh or shell for this group. Combine it with
    // the previous group's mesh or shell with the requested Boolean, and
    // we're done.

    Group *prevg = srcg->RunningMeshGroup();

    if(!IsForcedToMesh()) {
        SShell *prevs = &(prevg->runningShell);
        GenerateForBoolean<SShell>(prevs, &thisShell, &runningShell,
            srcg->meshCombine);

        if(srcg->meshCombine != CombineAs::ASSEMBLE) {
            runningShell.MergeCoincidentSurfaces();
        }

        // If the Boolean failed, then we should note that in the text screen
        // for this group.
        booleanFailed = runningShell.booleanFailed;
        if(booleanFailed != prevBooleanFailed) {
            SS.ScheduleShowTW();
        }
    } else {
        SMesh prevm, thism;
        prevm = {};
        thism = {};

        prevm.MakeFromCopyOf(&(prevg->runningMesh));
        prevg->runningShell.TriangulateInto(&prevm);

        thism.MakeFromCopyOf(&thisMesh);
        thisShell.TriangulateInto(&thism);

        SMesh outm = {};
        GenerateForBoolean<SMesh>(&prevm, &thism, &outm, srcg->meshCombine);

        // Remove degenerate triangles; if we don't, they'll get split in SnapToMesh
        // in every generated group, resulting in polynomial increase in triangle count,
        // and corresponding slowdown.
        outm.RemoveDegenerateTriangles();

        if(srcg->meshCombine != CombineAs::ASSEMBLE) {
            // And make sure that the output mesh is vertex-to-vertex.
            SKdNode *root = SKdNode::From(&outm);
            root->SnapToMesh(&outm);
            root->MakeMeshInto(&runningMesh);
        } else {
            runningMesh.MakeFromCopyOf(&outm);
        }

        outm.Clear();
        thism.Clear();
        prevm.Clear();
    }

    displayDirty = true;
}

void Group::GenerateDisplayItems() {
    // This is potentially slow (since we've got to triangulate a shell, or
    // to find the emphasized edges for a mesh), so we will run it only
    // if its inputs have changed.
    if(displayDirty) {
        Group *pg = RunningMeshGroup();
        if(pg && thisMesh.IsEmpty() && thisShell.IsEmpty()) {
            // We don't contribute any new solid model in this group, so our
            // display items are identical to the previous group's; which means
            // that we can just display those, and stop ourselves from
            // recalculating for those every time we get a change in this group.
            //
            // Note that this can end up recursing multiple times (if multiple
            // groups that contribute no solid model exist in sequence), but
            // that's okay.
            pg->GenerateDisplayItems();

            displayMesh.Clear();
            displayMesh.MakeFromCopyOf(&(pg->displayMesh));

            displayOutlines.Clear();
            if(SS.GW.showEdges || SS.GW.showOutlines) {
                displayOutlines.MakeFromCopyOf(&pg->displayOutlines);
            }
        } else {
            // We do contribute new solid model, so we have to triangulate the
            // shell, and edge-find the mesh.
            displayMesh.Clear();
            runningShell.TriangulateInto(&displayMesh);
            STriangle *t;
            for(t = runningMesh.l.First(); t; t = runningMesh.l.NextAfter(t)) {
                STriangle trn = *t;
                Vector n = trn.Normal();
                trn.an = n;
                trn.bn = n;
                trn.cn = n;
                displayMesh.AddTriangle(&trn);
            }

            displayOutlines.Clear();

            if(SS.GW.showEdges || SS.GW.showOutlines) {
                SOutlineList rawOutlines = {};
                if(!runningMesh.l.IsEmpty()) {
                    // Triangle mesh only; no shell or emphasized edges.
                    runningMesh.MakeOutlinesInto(&rawOutlines, EdgeKind::EMPHASIZED);
                } else {
                    displayMesh.MakeOutlinesInto(&rawOutlines, EdgeKind::SHARP);
                }

                PolylineBuilder builder;
                builder.MakeFromOutlines(rawOutlines);
                builder.GenerateOutlines(&displayOutlines);
                rawOutlines.Clear();
            }
        }

        // If we render this mesh, we need to know whether it's transparent,
        // and we'll want all transparent triangles last, to make the depth test
        // work correctly.
        displayMesh.PrecomputeTransparency();

        // Recalculate mass center if needed
        if(SS.centerOfMass.draw && SS.centerOfMass.dirty && h == SS.GW.activeGroup) {
            SS.UpdateCenterOfMass();
        }
        displayDirty = false;
    }
}

Group *Group::PreviousGroup() const {
    Group *prev = nullptr;
    for(auto const &gh : SK.groupOrder) {
        Group *g = SK.GetGroup(gh);
        if(g->h == h) {
            return prev;
        }
        prev = g;
    }
    return nullptr;
}

Group *Group::RunningMeshGroup() const {
    if(type == Type::TRANSLATE || type == Type::ROTATE) {
        return SK.GetGroup(opA)->RunningMeshGroup();
    } else {
        return PreviousGroup();
    }
}

bool Group::IsMeshGroup() {
    switch(type) {
        case Group::Type::EXTRUDE:
        case Group::Type::LATHE:
        case Group::Type::REVOLVE:
        case Group::Type::HELIX:
        case Group::Type::ROTATE:
        case Group::Type::TRANSLATE:
            return true;

        default:
            return false;
    }
}

void Group::DrawMesh(DrawMeshAs how, Canvas *canvas) {
    if(!(SS.GW.showShaded ||
         SS.GW.drawOccludedAs != GraphicsWindow::DrawOccludedAs::VISIBLE)) return;

    switch(how) {
        case DrawMeshAs::DEFAULT: {
            // Force the shade color to something dim to not distract from
            // the sketch.
            Canvas::Fill fillFront = {};
            if(!SS.GW.showShaded) {
                fillFront.layer = Canvas::Layer::DEPTH_ONLY;
            }
            if(type == Type::DRAWING_3D || type == Type::DRAWING_WORKPLANE) {
                fillFront.color = Style::Color(Style::DIM_SOLID);
            }
            Canvas::hFill hcfFront = canvas->GetFill(fillFront);

            // The back faces are drawn in red; should never seem them, since we
            // draw closed shells, so that's a debugging aid.
            Canvas::hFill hcfBack = {};
            if(SS.drawBackFaces && !displayMesh.isTransparent) {
                Canvas::Fill fillBack = {};
                fillBack.layer = fillFront.layer;
                fillBack.color = RgbaColor::FromFloat(1.0f, 0.1f, 0.1f);
                hcfBack = canvas->GetFill(fillBack);
            } else {
                hcfBack = hcfFront;
            }

            // Draw the shaded solid into the depth buffer for hidden line removal,
            // and if we're actually going to display it, to the color buffer too.
            canvas->DrawMesh(displayMesh, hcfFront, hcfBack);

            // Draw mesh edges, for debugging.
            if(SS.GW.showMesh) {
                Canvas::Stroke strokeTriangle = {};
                strokeTriangle.zIndex = 1;
                strokeTriangle.color  = RgbaColor::FromFloat(0.0f, 1.0f, 0.0f);
                strokeTriangle.width  = 1;
                strokeTriangle.unit   = Canvas::Unit::PX;
                Canvas::hStroke hcsTriangle = canvas->GetStroke(strokeTriangle);
                SEdgeList edges = {};
                for(const STriangle &t : displayMesh.l) {
                    edges.AddEdge(t.a, t.b);
                    edges.AddEdge(t.b, t.c);
                    edges.AddEdge(t.c, t.a);
                }
                canvas->DrawEdges(edges, hcsTriangle);
                edges.Clear();
            }
            break;
        }

        case DrawMeshAs::HOVERED: {
            Canvas::Fill fill = {};
            fill.color   = Style::Color(Style::HOVERED);
            fill.pattern = Canvas::FillPattern::CHECKERED_A;
            fill.zIndex  = 2;
            Canvas::hFill hcf = canvas->GetFill(fill);

            std::vector<uint32_t> faces;
            hEntity he = SS.GW.hover.entity;
            if(he.v != 0 && SK.GetEntity(he)->IsFace()) {
                faces.push_back(he.v);
            }
            canvas->DrawFaces(displayMesh, faces, hcf);
            break;
        }

        case DrawMeshAs::SELECTED: {
            Canvas::Fill fill = {};
            fill.color   = Style::Color(Style::SELECTED);
            fill.pattern = Canvas::FillPattern::CHECKERED_B;
            fill.zIndex  = 1;
            Canvas::hFill hcf = canvas->GetFill(fill);

            std::vector<uint32_t> faces;
            SS.GW.GroupSelection();
            auto const &gs = SS.GW.gs;
            if(gs.faces > 0) faces.push_back(gs.face[0].v);
            if(gs.faces > 1) faces.push_back(gs.face[1].v);
            canvas->DrawFaces(displayMesh, faces, hcf);
            break;
        }
    }
}

void Group::Draw(Canvas *canvas) {
    // Everything here gets drawn whether or not the group is hidden; we
    // can control this stuff independently, with show/hide solids, edges,
    // mesh, etc.

    GenerateDisplayItems();
    DrawMesh(DrawMeshAs::DEFAULT, canvas);

    if(SS.GW.showEdges) {
        Canvas::Stroke strokeEdge = Style::Stroke(Style::SOLID_EDGE);
        strokeEdge.zIndex = 1;
        Canvas::hStroke hcsEdge = canvas->GetStroke(strokeEdge);

        canvas->DrawOutlines(displayOutlines, hcsEdge,
                             SS.GW.showOutlines
                             ? Canvas::DrawOutlinesAs::EMPHASIZED_WITHOUT_CONTOUR
                             : Canvas::DrawOutlinesAs::EMPHASIZED_AND_CONTOUR);

        if(SS.GW.drawOccludedAs != GraphicsWindow::DrawOccludedAs::INVISIBLE) {
            Canvas::Stroke strokeHidden = Style::Stroke(Style::HIDDEN_EDGE);
            if(SS.GW.drawOccludedAs == GraphicsWindow::DrawOccludedAs::VISIBLE) {
                strokeHidden.stipplePattern = StipplePattern::CONTINUOUS;
            }
            strokeHidden.layer  = Canvas::Layer::OCCLUDED;
            Canvas::hStroke hcsHidden = canvas->GetStroke(strokeHidden);

            canvas->DrawOutlines(displayOutlines, hcsHidden,
                                 Canvas::DrawOutlinesAs::EMPHASIZED_AND_CONTOUR);
        }
    }

    if(SS.GW.showOutlines) {
        Canvas::Stroke strokeOutline = Style::Stroke(Style::OUTLINE);
        strokeOutline.zIndex = 1;
        Canvas::hStroke hcsOutline = canvas->GetStroke(strokeOutline);

        canvas->DrawOutlines(displayOutlines, hcsOutline,
                             Canvas::DrawOutlinesAs::CONTOUR_ONLY);
    }
}

void Group::DrawPolyError(Canvas *canvas) {
    const Camera &camera = canvas->GetCamera();

    Canvas::Stroke strokeUnclosed = Style::Stroke(Style::DRAW_ERROR);
    strokeUnclosed.color = strokeUnclosed.color.WithAlpha(50);
    Canvas::hStroke hcsUnclosed = canvas->GetStroke(strokeUnclosed);

    Canvas::Stroke strokeError = Style::Stroke(Style::DRAW_ERROR);
    strokeError.layer = Canvas::Layer::FRONT;
    strokeError.width = 1.0f;
    Canvas::hStroke hcsError = canvas->GetStroke(strokeError);

    double textHeight = Style::DefaultTextHeight() / camera.scale;

    // And finally show the polygons too, and any errors if it's not possible
    // to assemble the lines into closed polygons.
    if(polyError.how == PolyError::NOT_CLOSED) {
        // Report this error only in sketch-in-workplane groups; otherwise
        // it's just a nuisance.
        if(type == Type::DRAWING_WORKPLANE) {
            canvas->DrawVectorText(_("not closed contour, or not all same style!"),
                                   textHeight,
                                   polyError.notClosedAt.b, camera.projRight, camera.projUp,
                                   hcsError);
            canvas->DrawLine(polyError.notClosedAt.a, polyError.notClosedAt.b, hcsUnclosed);
        }
    } else if(polyError.how == PolyError::NOT_COPLANAR ||
              polyError.how == PolyError::SELF_INTERSECTING ||
              polyError.how == PolyError::ZERO_LEN_EDGE) {
        // These errors occur at points, not lines
        if(type == Type::DRAWING_WORKPLANE) {
            const char *msg;
            if(polyError.how == PolyError::NOT_COPLANAR) {
                msg = _("points not all coplanar!");
            } else if(polyError.how == PolyError::SELF_INTERSECTING) {
                msg = _("contour is self-intersecting!");
            } else {
                msg = _("zero-length edge!");
            }
            canvas->DrawVectorText(msg, textHeight,
                                   polyError.errorPointAt, camera.projRight, camera.projUp,
                                   hcsError);
        }
    } else {
        // The contours will get filled in DrawFilledPaths.
    }
}

void Group::DrawFilledPaths(Canvas *canvas) {
    for(const SBezierLoopSet &sbls : bezierLoops.l) {
        if(sbls.l.IsEmpty() || sbls.l[0].l.IsEmpty())
            continue;

        // In an assembled loop, all the styles should be the same; so doesn't
        // matter which one we grab.
        const SBezier *sb = &(sbls.l[0].l[0]);
        Style *s = Style::Get({ (uint32_t)sb->auxA });

        Canvas::Fill fill = {};
        fill.zIndex = 1;
        if(s->filled) {
            // This is a filled loop, where the user specified a fill color.
            fill.color = s->fillColor;
        } else if(h == SS.GW.activeGroup && SS.checkClosedContour &&
                    polyError.how == PolyError::GOOD) {
            // If this is the active group, and we are supposed to check
            // for closed contours, and we do indeed have a closed and
            // non-intersecting contour, then fill it dimly.
            fill.color = Style::Color(Style::CONTOUR_FILL).WithAlpha(127);
        } else continue;
        Canvas::hFill hcf = canvas->GetFill(fill);

        SPolygon sp = {};
        sbls.MakePwlInto(&sp);
        canvas->DrawPolygon(sp, hcf);
        sp.Clear();
    }
}

void Group::DrawContourAreaLabels(Canvas *canvas) {
    const Camera &camera = canvas->GetCamera();
    Vector gr = camera.projRight.ScaledBy(1 / camera.scale);
    Vector gu = camera.projUp.ScaledBy(1 / camera.scale);

    for(SBezierLoopSet &sbls : bezierLoops.l) {
        if(sbls.l.IsEmpty() || sbls.l[0].l.IsEmpty())
            continue;

        Vector min = sbls.l[0].l[0].ctrl[0];
        Vector max = min;
        Vector zero = Vector::From(0.0, 0.0, 0.0);
        sbls.GetBoundingProjd(Vector::From(1.0, 0.0, 0.0), zero, &min.x, &max.x);
        sbls.GetBoundingProjd(Vector::From(0.0, 1.0, 0.0), zero, &min.y, &max.y);
        sbls.GetBoundingProjd(Vector::From(0.0, 0.0, 1.0), zero, &min.z, &max.z);

        Vector mid = min.Plus(max).ScaledBy(0.5);

        hStyle hs = { Style::CONSTRAINT };
        Canvas::Stroke stroke = Style::Stroke(hs);
        stroke.layer = Canvas::Layer::FRONT;

        double scale = SS.MmPerUnit();
        std::string label = ssprintf("%.3f %s²",
                                     fabs(sbls.SignedArea() / (scale * scale)),
                                     SS.UnitName());

        double fontHeight = Style::TextHeight(hs);
        double textWidth  = VectorFont::Builtin()->GetWidth(fontHeight, label),
               textHeight = VectorFont::Builtin()->GetCapHeight(fontHeight);
        Vector pos = mid.Minus(gr.ScaledBy(textWidth / 2.0))
                        .Minus(gu.ScaledBy(textHeight / 2.0));
        canvas->DrawVectorText(label, fontHeight, pos, gr, gu, canvas->GetStroke(stroke));
    }
}

