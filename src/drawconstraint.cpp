//-----------------------------------------------------------------------------
// Given a constraint, draw a graphical and user-selectable representation
// of that constraint on-screen. We can either draw with gl, or compute the
// distance from a point (the location of the mouse pointer) to the lines
// that we would have drawn, for selection.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

std::string Constraint::Label() const {
    std::string result;
    if(type == Type::ANGLE) {
        result = SS.DegreeToString(valA) + "°";
    } else if(type == Type::LENGTH_RATIO) {
        result = ssprintf("%.3f:1", valA);
    } else if(type == Type::COMMENT) {
        result = comment;
    } else if(type == Type::DIAMETER) {
        if(!other) {
            result = "⌀" + SS.MmToStringSI(valA);
        } else {
            result = "R" + SS.MmToStringSI(valA / 2);
        }
    } else {
        // valA has units of distance
        result = SS.MmToStringSI(fabs(valA));
    }
    if(reference) {
        result += " REF";
    }
    return result;
}

void Constraint::DoLine(Canvas *canvas, Canvas::hStroke hcs, Vector a, Vector b) {
    const Camera &camera = canvas->GetCamera();

    a = camera.AlignToPixelGrid(a);
    b = camera.AlignToPixelGrid(b);
    canvas->DrawLine(a, b, hcs);
}

void Constraint::DoStippledLine(Canvas *canvas, Canvas::hStroke hcs, Vector a, Vector b) {
    Canvas::Stroke strokeStippled = *canvas->strokes.FindById(hcs);
    strokeStippled.stipplePattern = StipplePattern::SHORT_DASH;
    strokeStippled.stippleScale   = 4.0;
    Canvas::hStroke hcsStippled = canvas->GetStroke(strokeStippled);
    DoLine(canvas, hcsStippled, a, b);
}

void Constraint::DoLabel(Canvas *canvas, Canvas::hStroke hcs,
                         Vector ref, Vector *labelPos, Vector gr, Vector gu) {
    const Camera &camera = canvas->GetCamera();

    std::string s = Label();
    double textHeight = Style::TextHeight(GetStyle()) / camera.scale;
    double swidth  = VectorFont::Builtin()->GetWidth(textHeight, s),
           sheight = VectorFont::Builtin()->GetCapHeight(textHeight);

    // By default, the reference is from the center; but the style could
    // specify otherwise if one is present, and it could also specify a
    // rotation.
    if(type == Type::COMMENT && disp.style.v) {
        Style *st = Style::Get(disp.style);
        // rotation first
        double rads = st->textAngle*PI/180;
        double c = cos(rads), s = sin(rads);
        Vector pr = gr, pu = gu;
        gr = pr.ScaledBy( c).Plus(pu.ScaledBy(s));
        gu = pr.ScaledBy(-s).Plus(pu.ScaledBy(c));
        // then origin
        uint32_t o = (uint32_t)st->textOrigin;
        if(o & (uint32_t)Style::TextOrigin::LEFT) ref = ref.Plus(gr.WithMagnitude(swidth/2));
        if(o & (uint32_t)Style::TextOrigin::RIGHT) ref = ref.Minus(gr.WithMagnitude(swidth/2));
        if(o & (uint32_t)Style::TextOrigin::BOT) ref = ref.Plus(gu.WithMagnitude(sheight/2));
        if(o & (uint32_t)Style::TextOrigin::TOP) ref = ref.Minus(gu.WithMagnitude(sheight/2));
    }

    Vector o = ref.Minus(gr.WithMagnitude(swidth/2)).Minus(
                         gu.WithMagnitude(sheight/2));
    canvas->DrawVectorText(s, textHeight, o, gr.WithMagnitude(1), gu.WithMagnitude(1), hcs);
    if(labelPos) *labelPos = o;
}

void Constraint::DoProjectedPoint(Canvas *canvas, Canvas::hStroke hcs,
                                  Vector *r, Vector n, Vector o) {
    double d = r->DistanceToPlane(n, o);
    Vector p = r->Minus(n.ScaledBy(d));
    DoStippledLine(canvas, hcs, p, *r);
    *r = p;
}

void Constraint::DoProjectedPoint(Canvas *canvas, Canvas::hStroke hcs, Vector *r) {
    Vector p = r->ProjectInto(workplane);
    DoStippledLine(canvas, hcs, p, *r);
    *r = p;
}

//-----------------------------------------------------------------------------
// There is a rectangular box, aligned to our display axes (projRight, projUp)
// centered at ref. This is where a dimension label will be drawn. We want to
// draw a line from A to B. If that line would intersect the label box, then
// trim the line to leave a gap for it, and return zero. If not, then extend
// the line to almost meet the box, and return either positive or negative,
// depending whether that extension was from A or from B.
//-----------------------------------------------------------------------------
int Constraint::DoLineTrimmedAgainstBox(Canvas *canvas, Canvas::hStroke hcs,
                                        Vector ref, Vector a, Vector b, bool extend) {
    const Camera &camera = canvas->GetCamera();
    double th      = Style::TextHeight(GetStyle()) / camera.scale;
    double pixels  = 1.0 / camera.scale;
    double swidth  = VectorFont::Builtin()->GetWidth(th, Label()) + 8 * pixels,
           sheight = VectorFont::Builtin()->GetCapHeight(th) + 8 * pixels;
    Vector gu = camera.projUp.WithMagnitude(1),
           gr = camera.projRight.WithMagnitude(1);
    return DoLineTrimmedAgainstBox(canvas, hcs, ref, a, b, extend, gr, gu, swidth, sheight);
}

int Constraint::DoLineTrimmedAgainstBox(Canvas *canvas, Canvas::hStroke hcs,
                                        Vector ref, Vector a, Vector b, bool extend,
                                        Vector gr, Vector gu, double swidth, double sheight) {
    struct {
        Vector n;
        double d;
    } planes[4];
    // reference pos is the center of box occupied by text; build a rectangle
    // around that, aligned to axes gr and gu, from four planes will all four
    // normals pointing inward
    planes[0].n = gu.ScaledBy(-1); planes[0].d = -(gu.Dot(ref) + sheight/2);
    planes[1].n = gu;              planes[1].d =   gu.Dot(ref) - sheight/2;
    planes[2].n = gr;              planes[2].d =   gr.Dot(ref) - swidth/2;
    planes[3].n = gr.ScaledBy(-1); planes[3].d = -(gr.Dot(ref) + swidth/2);

    double tmin = VERY_POSITIVE, tmax = VERY_NEGATIVE;
    Vector dl = b.Minus(a);

    for(int i = 0; i < 4; i++) {
        bool parallel;
        Vector p = Vector::AtIntersectionOfPlaneAndLine(
                                planes[i].n, planes[i].d,
                                a, b, &parallel);
        if(parallel) continue;

        int j;
        for(j = 0; j < 4; j++) {
            double d = (planes[j].n).Dot(p) - planes[j].d;
            if(d < -LENGTH_EPS) break;
        }
        if(j < 4) continue;

        double t = (p.Minus(a)).DivProjected(dl);
        tmin = min(t, tmin);
        tmax = max(t, tmax);
    }

    // Both in range; so there's pieces of the line on both sides of the label box.
    if(tmin >= 0.0 && tmin <= 1.0 && tmax >= 0.0 && tmax <= 1.0) {
        DoLine(canvas, hcs, a, a.Plus(dl.ScaledBy(tmin)));
        DoLine(canvas, hcs, a.Plus(dl.ScaledBy(tmax)), b);
        return 0;
    }

    // Only one intersection in range; so the box is right on top of the endpoint
    if(tmin >= 0.0 && tmin <= 1.0) {
        DoLine(canvas, hcs, a, a.Plus(dl.ScaledBy(tmin)));
        return 0;
    }

    // Likewise.
    if(tmax >= 0.0 && tmax <= 1.0) {
        DoLine(canvas, hcs, a.Plus(dl.ScaledBy(tmax)), b);
        return 0;
    }

    // The line does not intersect the label; so the line should get
    // extended to just barely meet the label.
    // 0 means the label lies within the line, negative means it's outside
    // and closer to b, positive means outside and closer to a.
    if(tmax < 0.0) {
        if(extend) a = a.Plus(dl.ScaledBy(tmax));
        DoLine(canvas, hcs, a, b);
        return 1;
    }

    if(tmin > 1.0) {
        if(extend) b = a.Plus(dl.ScaledBy(tmin));
        DoLine(canvas, hcs, a, b);
        return -1;
    }

    // This will happen if the entire line lies within the box.
    return 0;
}

void Constraint::DoArrow(Canvas *canvas, Canvas::hStroke hcs,
                         Vector p, Vector dir, Vector n, double width, double angle, double da) {
    dir = dir.WithMagnitude(width / cos(angle));
    dir = dir.RotatedAbout(n, da);
    DoLine(canvas, hcs, p, p.Plus(dir.RotatedAbout(n,  angle)));
    DoLine(canvas, hcs, p, p.Plus(dir.RotatedAbout(n, -angle)));
}

//-----------------------------------------------------------------------------
// Draw a line with arrows on both ends, and possibly a gap in the middle for
// the dimension. We will use these for most length dimensions. The length
// being dimensioned is from A to B; but those points get extended perpendicular
// to the line AB, until the line between the extensions crosses ref (the
// center of the label).
//-----------------------------------------------------------------------------
void Constraint::DoLineWithArrows(Canvas *canvas, Canvas::hStroke hcs,
                                  Vector ref, Vector a, Vector b,
                                  bool onlyOneExt)
{
    const Camera &camera = canvas->GetCamera();
    double pixels = 1.0 / camera.scale;

    Vector ab   = a.Minus(b);
    Vector ar   = a.Minus(ref);
    // Normal to a plane containing the line and the label origin.
    Vector n    = ab.Cross(ar);
    // Within that plane, and normal to the line AB; so that's our extension
    // line.
    Vector out  = ab.Cross(n).WithMagnitude(1);
    out = out.ScaledBy(-out.Dot(ar));

    Vector ae = a.Plus(out), be = b.Plus(out);

    // Extension lines extend 10 pixels beyond where the arrows get
    // drawn (which is at the same offset perpendicular from AB as the
    // label).
    DoLine(canvas, hcs, a, ae.Plus(out.WithMagnitude(10*pixels)));
    if(!onlyOneExt) {
        DoLine(canvas, hcs, b, be.Plus(out.WithMagnitude(10*pixels)));
    }

    int within = DoLineTrimmedAgainstBox(canvas, hcs, ref, ae, be);

    // Arrow heads are 13 pixels long, with an 18 degree half-angle.
    double theta = 18*PI/180;
    Vector arrow = (be.Minus(ae)).WithMagnitude(13*pixels);

    if(within != 0) {
        arrow = arrow.ScaledBy(-1);
        Vector seg = (be.Minus(ae)).WithMagnitude(18*pixels);
        if(within < 0) DoLine(canvas, hcs, ae, ae.Minus(seg));
        if(within > 0) DoLine(canvas, hcs, be, be.Plus(seg));
    }

    DoArrow(canvas, hcs, ae, arrow, n, 13.0 * pixels, theta, 0.0);
    DoArrow(canvas, hcs, be, arrow.Negated(), n, 13.0 * pixels, theta, 0.0);
}

void Constraint::DoEqualLenTicks(Canvas *canvas, Canvas::hStroke hcs,
                                 Vector a, Vector b, Vector gn, Vector *refp) {
    const Camera &camera = canvas->GetCamera();

    Vector m = (a.ScaledBy(1.0/3)).Plus(b.ScaledBy(2.0/3));
    if(refp) *refp = m;
    Vector ab = a.Minus(b);
    Vector n = (gn.Cross(ab)).WithMagnitude(10/camera.scale);

    DoLine(canvas, hcs, m.Minus(n), m.Plus(n));
}

void Constraint::DoEqualRadiusTicks(Canvas *canvas, Canvas::hStroke hcs,
                                    hEntity he, Vector *refp) {
    const Camera &camera = canvas->GetCamera();
    Entity *circ = SK.GetEntity(he);

    Vector center = SK.GetEntity(circ->point[0])->PointGetNum();
    double r = circ->CircleGetRadiusNum();
    Quaternion q = circ->Normal()->NormalGetNum();
    Vector u = q.RotationU(), v = q.RotationV();

    double theta;
    if(circ->type == Entity::Type::CIRCLE) {
        theta = PI/2;
    } else if(circ->type == Entity::Type::ARC_OF_CIRCLE) {
        double thetaa, thetab, dtheta;
        circ->ArcGetAngles(&thetaa, &thetab, &dtheta);
        theta = thetaa + dtheta/2;
    } else ssassert(false, "Unexpected entity type");

    Vector d = u.ScaledBy(cos(theta)).Plus(v.ScaledBy(sin(theta)));
    d = d.ScaledBy(r);
    Vector p = center.Plus(d);
    if(refp) *refp = p;
    Vector tick = d.WithMagnitude(10/camera.scale);
    DoLine(canvas, hcs, p.Plus(tick), p.Minus(tick));
}

void Constraint::DoArcForAngle(Canvas *canvas, Canvas::hStroke hcs,
                               Vector a0, Vector da, Vector b0, Vector db,
                               Vector offset, Vector *ref, bool trim)
{
    const Camera &camera = canvas->GetCamera();
    double pixels = 1.0 / camera.scale;
    Vector gr = camera.projRight.ScaledBy(1.0);
    Vector gu = camera.projUp.ScaledBy(1.0);

    if(workplane != Entity::FREE_IN_3D) {
        a0 = a0.ProjectInto(workplane);
        b0 = b0.ProjectInto(workplane);
        da = da.ProjectVectorInto(workplane);
        db = db.ProjectVectorInto(workplane);
    }

    Vector a1 = a0.Plus(da);
    Vector b1 = b0.Plus(db);

    bool skew;
    Vector pi = Vector::AtIntersectionOfLines(a0, a0.Plus(da),
                                              b0, b0.Plus(db), &skew);

    if(!skew) {
        *ref = pi.Plus(offset);
        // We draw in a coordinate system centered at the intersection point.
        // One basis vector is da, and the other is normal to da and in
        // the plane that contains our lines (so normal to its normal).
        da = da.WithMagnitude(1);
        db = db.WithMagnitude(1);

        Vector norm = da.Cross(db);

        Vector dna = norm.Cross(da).WithMagnitude(1.0);
        Vector dnb = norm.Cross(db).WithMagnitude(1.0);

        // da and db magnitudes are 1.0
        double thetaf = acos(da.Dot(db));

        // Calculate median
        Vector m = da.ScaledBy(cos(thetaf/2)).Plus(
                   dna.ScaledBy(sin(thetaf/2)));
        Vector rm = (*ref).Minus(pi);

        // Test which side we have to place an arc
        if(m.Dot(rm) < 0) {
            da = da.ScaledBy(-1); dna = dna.ScaledBy(-1);
            db = db.ScaledBy(-1); dnb = dnb.ScaledBy(-1);
        }

        double rda = rm.Dot(da), rdna = rm.Dot(dna);

        // Introduce minimal arc radius in pixels
        double r = max(sqrt(rda*rda + rdna*rdna), 15.0 * pixels);

        double th = Style::TextHeight(GetStyle()) / camera.scale;
        double swidth   = VectorFont::Builtin()->GetWidth(th, Label()) + 8*pixels,
               sheight  = VectorFont::Builtin()->GetCapHeight(th) + 6*pixels;
        double textR = sqrt(swidth * swidth + sheight * sheight) / 2.0;
        *ref = pi.Plus(rm.WithMagnitude(std::max(rm.Magnitude(), 15 * pixels + textR)));

        // Arrow points
        Vector apa = da. ScaledBy(r).Plus(pi);
        Vector apb = da. ScaledBy(r*cos(thetaf)).Plus(
                     dna.ScaledBy(r*sin(thetaf))).Plus(pi);

        double arrowW = 13 * pixels;
        double arrowA = 18.0 * PI / 180.0;
        bool arrowVisible = apb.Minus(apa).Magnitude() > 2.5 * arrowW;
        // Arrow reversing indicator
        bool arrowRev = false;

        // The minimal extension length in angular representation
        double extAngle = 18 * pixels / r;

        // Arc additional angle
        double addAngle = 0.0;
        // Arc start angle
        double startAngle = 0.0;

        // Arc extension to db.
        // We have just enlarge angle value.
        if(HasLabel() && rm.Dot(dnb) > 0.0) {
            // rm direction projected to plane with u = da, v = dna
            Vector rmp = da.ScaledBy(rda).Plus(dna.ScaledBy(rdna)).WithMagnitude(1.0);
            // rmp and db magnitudes are 1.0
            addAngle = std::max(acos(rmp.Dot(db)), extAngle);

            if(arrowVisible) {
                startAngle = -extAngle;
                addAngle += extAngle;
                arrowRev = true;
            }
        }

        // Arc extension to da.
        // We are enlarge angle value and rewrite basis to align along rm projection.
        if(HasLabel() && rm.Dot(dna) < 0.0) {
            // rm direction projected to plane with u = da, v = dna
            Vector rmp = da.ScaledBy(rda).Plus(dna.ScaledBy(rdna)).WithMagnitude(1.0);
            // rmp and da magnitudes are 1.0
            startAngle = -std::max(acos(rmp.Dot(da)), extAngle);
            addAngle = -startAngle;

            if(arrowVisible) {
                addAngle += extAngle;
                arrowRev = true;
            }
        }

        Vector prev;
        int n = 30;
        for(int i = 0; i <= n; i++) {
            double theta = startAngle + (i*(thetaf + addAngle))/n;
            Vector p =  da.ScaledBy(r*cos(theta)).Plus(
                       dna.ScaledBy(r*sin(theta))).Plus(pi);
            if(i > 0) {
                if(trim) {
                    DoLineTrimmedAgainstBox(canvas, hcs, *ref, prev, p,
                                            /*extend=*/false, gr, gu, swidth, sheight + 2*pixels);
                } else {
                    DoLine(canvas, hcs, prev, p);
                }
            }
            prev = p;
        }

        DoLineExtend(canvas, hcs, a0, a1, apa, 5.0 * pixels);
        DoLineExtend(canvas, hcs, b0, b1, apb, 5.0 * pixels);

        // Draw arrows only when we have enough space.
        if(arrowVisible) {
            double angleCorr = arrowW / (2.0 * r);
            if(arrowRev) {
                dna = dna.ScaledBy(-1.0);
                angleCorr = -angleCorr;
            }
            DoArrow(canvas, hcs, apa, dna, norm, arrowW, arrowA, angleCorr);
            DoArrow(canvas, hcs, apb, dna, norm, arrowW, arrowA, thetaf + PI - angleCorr);
        }
    } else {
        // The lines are skew; no wonderful way to illustrate that.

        *ref = a0.Plus(b0);
        *ref = (*ref).ScaledBy(0.5).Plus(disp.offset);
        gu = gu.WithMagnitude(1);
        double textHeight = Style::TextHeight(GetStyle()) / camera.scale;
        Vector trans =
            (*ref).Plus(gu.ScaledBy(-1.5*VectorFont::Builtin()->GetCapHeight(textHeight)));
        canvas->DrawVectorText("angle between skew lines", textHeight,
                               trans, gr.WithMagnitude(1), gu.WithMagnitude(1),
                               hcs);
    }
}

bool Constraint::IsVisible() const {
    if(SS.GW.showConstraints == GraphicsWindow::ShowConstraintMode::SCM_NOSHOW ) return false;
    bool isDim = false;
    switch (type){
    case ConstraintBase::Type::ANGLE:
    case ConstraintBase::Type::DIAMETER:
    case ConstraintBase::Type::PT_PT_DISTANCE:
    case ConstraintBase::Type::PT_FACE_DISTANCE:
    case ConstraintBase::Type::PT_LINE_DISTANCE:
    case ConstraintBase::Type::PT_PLANE_DISTANCE:
        isDim = true;
        break;
    default:;
    }

    if(SS.GW.showConstraints == GraphicsWindow::ShowConstraintMode::SCM_SHOW_ALL || isDim ) {
        Group *g = SK.GetGroup(group);
        // If the group is hidden, then the constraints are hidden and not
        // able to be selected.
        if(!(g->visible)) return false;
        // And likewise if the group is not the active group; except for comments
        // with an assigned style.
        if(g->h != SS.GW.activeGroup && !(type == Type::COMMENT && disp.style.v)) {
            return false;
        }
        if(disp.style.v) {
            Style *s = Style::Get(disp.style);
            if(!s->visible) return false;
        }
        return true;
    }
    return false;
}

bool Constraint::DoLineExtend(Canvas *canvas, Canvas::hStroke hcs,
                              Vector p0, Vector p1, Vector pt, double salient) {
    Vector dir = p1.Minus(p0);
    double k = dir.Dot(pt.Minus(p0)) / dir.Dot(dir);
    Vector ptOnLine = p0.Plus(dir.ScaledBy(k));

    // Draw projection line.
    DoLine(canvas, hcs, pt, ptOnLine);

    // Calculate salient direction.
    Vector sd = dir.WithMagnitude(1.0).ScaledBy(salient);

    Vector from;
    Vector to;

    if(k < 0.0) {
        from = p0;
        to = ptOnLine.Minus(sd);
    } else if(k > 1.0) {
        from = p1;
        to = ptOnLine.Plus(sd);
    } else {
        return false;
    }

    // Draw extension line.
    DoLine(canvas, hcs, from, to);
    return true;
}

void Constraint::DoLayout(DrawAs how, Canvas *canvas,
                          Vector *labelPos, std::vector<Vector> *refs) {
    if(!(how == DrawAs::HOVERED || how == DrawAs::SELECTED) &&
       !IsVisible()) return;

    // Unit vectors that describe our current view of the scene. One pixel
    // long, not one actual unit.
    const Camera &camera = canvas->GetCamera();
    Vector gr = camera.projRight.ScaledBy(1/camera.scale);
    Vector gu = camera.projUp.ScaledBy(1/camera.scale);
    Vector gn = (gr.Cross(gu)).WithMagnitude(1/camera.scale);

    double textHeight = Style::TextHeight(GetStyle()) / camera.scale;

    RgbaColor color = {};
    switch(how) {
        case DrawAs::DEFAULT:  color = Style::Color(GetStyle()); break;
        case DrawAs::HOVERED:  color = Style::Color(Style::HOVERED);    break;
        case DrawAs::SELECTED: color = Style::Color(Style::SELECTED);   break;
    }
    Canvas::Stroke stroke = Style::Stroke(GetStyle());
    stroke.layer    = Canvas::Layer::FRONT;
    stroke.color    = color;
    stroke.zIndex   = 4;
    Canvas::hStroke hcs = canvas->GetStroke(stroke);

    Canvas::Fill fill = {};
    fill.layer      = Canvas::Layer::FRONT;
    fill.color      = color;
    fill.zIndex     = stroke.zIndex;
    Canvas::hFill hcf = canvas->GetFill(fill);

    switch(type) {
        case Type::PT_PT_DISTANCE: {
            Vector ap = SK.GetEntity(ptA)->PointGetNum();
            Vector bp = SK.GetEntity(ptB)->PointGetNum();

            if(workplane != Entity::FREE_IN_3D) {
                DoProjectedPoint(canvas, hcs, &ap);
                DoProjectedPoint(canvas, hcs, &bp);
            }

            Vector ref = ((ap.Plus(bp)).ScaledBy(0.5)).Plus(disp.offset);
            if(refs) refs->push_back(ref);

            DoLineWithArrows(canvas, hcs, ref, ap, bp, /*onlyOneExt=*/false);
            DoLabel(canvas, hcs, ref, labelPos, gr, gu);
            return;
        }

        case Type::PROJ_PT_DISTANCE: {
            Vector ap = SK.GetEntity(ptA)->PointGetNum(),
                   bp = SK.GetEntity(ptB)->PointGetNum(),
                   dp = (bp.Minus(ap)),
                   pp = SK.GetEntity(entityA)->VectorGetNum();

            Vector ref = ((ap.Plus(bp)).ScaledBy(0.5)).Plus(disp.offset);
            if(refs) refs->push_back(ref);

            pp = pp.WithMagnitude(1);
            double d = dp.Dot(pp);
            Vector bpp = ap.Plus(pp.ScaledBy(d));
            DoStippledLine(canvas, hcs, ap, bpp);
            DoStippledLine(canvas, hcs, bp, bpp);

            DoLineWithArrows(canvas, hcs, ref, ap, bpp, /*onlyOneExt=*/false);
            DoLabel(canvas, hcs, ref, labelPos, gr, gu);
            return;
        }

        case Type::PT_FACE_DISTANCE:
        case Type::PT_PLANE_DISTANCE: {
            Vector pt = SK.GetEntity(ptA)->PointGetNum();
            Entity *enta = SK.GetEntity(entityA);
            Vector n, p;
            if(type == Type::PT_PLANE_DISTANCE) {
                n = enta->Normal()->NormalN();
                p = enta->WorkplaneGetOffset();
            } else {
                n = enta->FaceGetNormalNum();
                p = enta->FaceGetPointNum();
            }

            double d = (p.Minus(pt)).Dot(n);
            Vector closest = pt.Plus(n.WithMagnitude(d));

            Vector ref = ((closest.Plus(pt)).ScaledBy(0.5)).Plus(disp.offset);
            if(refs) refs->push_back(ref);

            if(!pt.Equals(closest)) {
                DoLineWithArrows(canvas, hcs, ref, pt, closest, /*onlyOneExt=*/true);
            }

            DoLabel(canvas, hcs, ref, labelPos, gr, gu);
            return;
        }

        case Type::PT_LINE_DISTANCE: {
            Vector pt = SK.GetEntity(ptA)->PointGetNum();
            Entity *line = SK.GetEntity(entityA);
            Vector lA = SK.GetEntity(line->point[0])->PointGetNum();
            Vector lB = SK.GetEntity(line->point[1])->PointGetNum();
            Vector dl = lB.Minus(lA);

            if(workplane != Entity::FREE_IN_3D) {
                lA = lA.ProjectInto(workplane);
                lB = lB.ProjectInto(workplane);
                DoProjectedPoint(canvas, hcs, &pt);
            }

            // Find the closest point on the line
            Vector closest = pt.ClosestPointOnLine(lA, dl);

            Vector ref = ((closest.Plus(pt)).ScaledBy(0.5)).Plus(disp.offset);
            if(refs) refs->push_back(ref);
            DoLabel(canvas, hcs, ref, labelPos, gr, gu);

            if(!pt.Equals(closest)) {
                DoLineWithArrows(canvas, hcs, ref, pt, closest, /*onlyOneExt=*/true);

                // Draw projected point
                Vector a    = pt;
                Vector b    = closest;
                Vector ab   = a.Minus(b);
                Vector ar   = a.Minus(ref);
                Vector n    = ab.Cross(ar);
                Vector out  = ab.Cross(n).WithMagnitude(1);
                out = out.ScaledBy(-out.Dot(ar));
                Vector be   = b.Plus(out);
                Vector np   = lA.Minus(pt).Cross(lB.Minus(pt)).WithMagnitude(1.0);
                DoProjectedPoint(canvas, hcs, &be, np, pt);

                // Extensions to line
                double pixels = 1.0 / camera.scale;
                Vector refClosest = ref.ClosestPointOnLine(lA, dl);
                double ddl = dl.Dot(dl);
                if(fabs(ddl) > LENGTH_EPS * LENGTH_EPS) {
                    double t = refClosest.Minus(lA).Dot(dl) / ddl;
                    if(t < 0.0) {
                        DoLine(canvas, hcs, refClosest.Minus(dl.WithMagnitude(10.0 * pixels)), lA);
                    } else if(t > 1.0) {
                        DoLine(canvas, hcs, refClosest.Plus(dl.WithMagnitude(10.0 * pixels)), lB);
                    }
                }
            }

            if(workplane != Entity::FREE_IN_3D) {
                // Draw the projection marker from the closest point on the
                // projected line to the projected point on the real line.
                Vector lAB = (lA.Minus(lB));
                double t = (lA.Minus(closest)).DivProjected(lAB);

                Vector lA = SK.GetEntity(line->point[0])->PointGetNum();
                Vector lB = SK.GetEntity(line->point[1])->PointGetNum();

                Vector c2 = (lA.ScaledBy(1-t)).Plus(lB.ScaledBy(t));
                DoProjectedPoint(canvas, hcs, &c2);
            }
            return;
        }

        case Type::DIAMETER: {
            Entity *circle = SK.GetEntity(entityA);
            Vector center = SK.GetEntity(circle->point[0])->PointGetNum();
            Quaternion q = SK.GetEntity(circle->normal)->NormalGetNum();
            Vector n = q.RotationN().WithMagnitude(1);
            double r = circle->CircleGetRadiusNum();

            Vector ref = center.Plus(disp.offset);
            // Force the label into the same plane as the circle.
            ref = ref.Minus(n.ScaledBy(n.Dot(ref) - n.Dot(center)));
            if(refs) refs->push_back(ref);

            Vector mark = ref.Minus(center);
            mark = mark.WithMagnitude(mark.Magnitude()-r);
            DoLineTrimmedAgainstBox(canvas, hcs, ref, ref, ref.Minus(mark));

            Vector topLeft;
            DoLabel(canvas, hcs, ref, &topLeft, gr, gu);
            if(labelPos) *labelPos = topLeft;
            return;
        }

        case Type::POINTS_COINCIDENT: {
            if(how == DrawAs::DEFAULT) {
                // Let's adjust the color of this constraint to have the same
                // rough luma as the point color, so that the constraint does not
                // stand out in an ugly way.
                RgbaColor cd = Style::Color(Style::DATUM),
                          cc = Style::Color(Style::CONSTRAINT);
                // convert from 8-bit color to a vector
                Vector vd = Vector::From(cd.redF(), cd.greenF(), cd.blueF()),
                       vc = Vector::From(cc.redF(), cc.greenF(), cc.blueF());
                // and scale the constraint color to have the same magnitude as
                // the datum color, maybe a bit dimmer
                vc = vc.WithMagnitude(vd.Magnitude()*0.9);
                // and set the color to that.
                fill.color = RGBf(vc.x, vc.y, vc.z);
                hcf = canvas->GetFill(fill);
            }

            for(int a = 0; a < 2; a++) {
                Vector r = camera.projRight.ScaledBy((a+1)/camera.scale);
                Vector d = camera.projUp.ScaledBy((2-a)/camera.scale);
                for(int i = 0; i < 2; i++) {
                    Vector p = SK.GetEntity(i == 0 ? ptA : ptB)-> PointGetNum();
                    if(refs) refs->push_back(p);
                    canvas->DrawQuad(p.Plus (r).Plus (d),
                                     p.Plus (r).Minus(d),
                                     p.Minus(r).Minus(d),
                                     p.Minus(r).Plus (d),
                                     hcf);
                }

            }
            return;
        }

        case Type::PT_ON_CIRCLE:
        case Type::PT_ON_LINE:
        case Type::PT_ON_FACE:
        case Type::PT_IN_PLANE: {
            double s = 8/camera.scale;
            Vector p = SK.GetEntity(ptA)->PointGetNum();
            if(refs) refs->push_back(p);
            Vector r, d;
            if(type == Type::PT_ON_FACE) {
                Vector n = SK.GetEntity(entityA)->FaceGetNormalNum();
                r = n.Normal(0);
                d = n.Normal(1);
            } else if(type == Type::PT_IN_PLANE) {
                EntityBase *n = SK.GetEntity(entityA)->Normal();
                r = n->NormalU();
                d = n->NormalV();
            } else {
                r = gr;
                d = gu;
                s *= (6.0/8); // draw these a little smaller
            }
            r = r.WithMagnitude(s); d = d.WithMagnitude(s);
            DoLine(canvas, hcs, p.Plus (r).Plus (d), p.Plus (r).Minus(d));
            DoLine(canvas, hcs, p.Plus (r).Minus(d), p.Minus(r).Minus(d));
            DoLine(canvas, hcs, p.Minus(r).Minus(d), p.Minus(r).Plus (d));
            DoLine(canvas, hcs, p.Minus(r).Plus (d), p.Plus (r).Plus (d));
            return;
        }

        case Type::WHERE_DRAGGED: {
            Vector p = SK.GetEntity(ptA)->PointGetNum();
            if(refs) refs->push_back(p);
            Vector u = p.Plus(gu.WithMagnitude(8/camera.scale)).Plus(
                              gr.WithMagnitude(8/camera.scale)),
                   uu = u.Minus(gu.WithMagnitude(5/camera.scale)),
                   ur = u.Minus(gr.WithMagnitude(5/camera.scale));
            // Draw four little crop marks, uniformly spaced (by ninety
            // degree rotations) around the point.
            int i;
            for(i = 0; i < 4; i++) {
                DoLine(canvas, hcs, u, uu);
                DoLine(canvas, hcs, u, ur);
                u = u.RotatedAbout(p, gn, PI/2);
                ur = ur.RotatedAbout(p, gn, PI/2);
                uu = uu.RotatedAbout(p, gn, PI/2);
            }
            return;
        }

        case Type::SAME_ORIENTATION: {
            for(int i = 0; i < 2; i++) {
                Entity *e = SK.GetEntity(i == 0 ? entityA : entityB);
                Quaternion q = e->NormalGetNum();
                Vector n = q.RotationN().WithMagnitude(25/camera.scale);
                Vector u = q.RotationU().WithMagnitude(6/camera.scale);
                Vector p = SK.GetEntity(e->point[0])->PointGetNum();
                p = p.Plus(n.WithMagnitude(10/camera.scale));
                if(refs) refs->push_back(p);

                DoLine(canvas, hcs, p.Plus(u), p.Minus(u).Plus(n));
                DoLine(canvas, hcs, p.Minus(u), p.Plus(u).Plus(n));
            }
            return;
        }

        case Type::EQUAL_ANGLE: {
            Vector ref;
            Entity *a = SK.GetEntity(entityA);
            Entity *b = SK.GetEntity(entityB);
            Entity *c = SK.GetEntity(entityC);
            Entity *d = SK.GetEntity(entityD);

            Vector a0 = a->VectorGetStartPoint();
            Vector b0 = b->VectorGetStartPoint();
            Vector c0 = c->VectorGetStartPoint();
            Vector d0 = d->VectorGetStartPoint();
            Vector da = a->VectorGetNum();
            Vector db = b->VectorGetNum();
            Vector dc = c->VectorGetNum();
            Vector dd = d->VectorGetNum();

            if(other) {
                a0 = a0.Plus(da);
                da = da.ScaledBy(-1);
            }

            DoArcForAngle(canvas, hcs, a0, da, b0, db,
                da.WithMagnitude(40/camera.scale), &ref, /*trim=*/false);
            if(refs) refs->push_back(ref);
            DoArcForAngle(canvas, hcs, c0, dc, d0, dd,
                dc.WithMagnitude(40/camera.scale), &ref, /*trim=*/false);
            if(refs) refs->push_back(ref);

            return;
        }

        case Type::ANGLE: {
            Entity *a = SK.GetEntity(entityA);
            Entity *b = SK.GetEntity(entityB);

            Vector a0 = a->VectorGetStartPoint();
            Vector b0 = b->VectorGetStartPoint();
            Vector da = a->VectorGetNum();
            Vector db = b->VectorGetNum();
            if(other) {
                a0 = a0.Plus(da);
                da = da.ScaledBy(-1);
            }

            Vector ref;
            DoArcForAngle(canvas, hcs, a0, da, b0, db, disp.offset, &ref, /*trim=*/true);
            DoLabel(canvas, hcs, ref, labelPos, gr, gu);
            if(refs) refs->push_back(ref);
            return;
        }

        case Type::PERPENDICULAR: {
            Vector u = Vector::From(0, 0, 0), v = Vector::From(0, 0, 0);
            Vector rn, ru;
            if(workplane == Entity::FREE_IN_3D) {
                rn = gn;
                ru = gu;
            } else {
                EntityBase *normal = SK.GetEntity(workplane)->Normal();
                rn = normal->NormalN();
                ru = normal->NormalV(); // ru meaning r_up, not u/v
            }

            for(int i = 0; i < 2; i++) {
                Entity *e = SK.GetEntity(i == 0 ? entityA : entityB);

                if(i == 0) {
                    // Calculate orientation of perpendicular sign only
                    // once, so that it's the same both times it's drawn
                    u = e->VectorGetNum();
                    u = u.WithMagnitude(16/camera.scale);
                    v = (rn.Cross(u)).WithMagnitude(16/camera.scale);
                    // a bit of bias to stop it from flickering between the
                    // two possibilities
                    if(fabs(u.Dot(ru)) < fabs(v.Dot(ru)) + LENGTH_EPS) {
                        swap(u, v);
                    }
                    if(u.Dot(ru) < 0) u = u.ScaledBy(-1);
                }

                Vector p = e->VectorGetRefPoint();
                Vector s = p.Plus(u).Plus(v);
                DoLine(canvas, hcs, s, s.Plus(v));
                Vector m = s.Plus(v.ScaledBy(0.5));
                DoLine(canvas, hcs, m, m.Plus(u));
                if(refs) refs->push_back(m);
            }
            return;
        }

        case Type::CURVE_CURVE_TANGENT:
        case Type::CUBIC_LINE_TANGENT:
        case Type::ARC_LINE_TANGENT: {
            Vector textAt, u, v;

            if(type == Type::ARC_LINE_TANGENT) {
                Entity *arc = SK.GetEntity(entityA);
                Entity *norm = SK.GetEntity(arc->normal);
                Vector c = SK.GetEntity(arc->point[0])->PointGetNum();
                Vector p =
                    SK.GetEntity(arc->point[other ? 2 : 1])->PointGetNum();
                Vector r = p.Minus(c);
                textAt = p.Plus(r.WithMagnitude(14/camera.scale));
                u = norm->NormalU();
                v = norm->NormalV();
            } else if(type == Type::CUBIC_LINE_TANGENT) {
                Vector n;
                if(workplane == Entity::FREE_IN_3D) {
                    u = gr;
                    v = gu;
                    n = gn;
                } else {
                    EntityBase *wn = SK.GetEntity(workplane)->Normal();
                    u = wn->NormalU();
                    v = wn->NormalV();
                    n = wn->NormalN();
                }

                Entity *cubic = SK.GetEntity(entityA);
                Vector p = other ? cubic->CubicGetFinishNum() :
                                   cubic->CubicGetStartNum();
                Vector dir = SK.GetEntity(entityB)->VectorGetNum();
                Vector out = n.Cross(dir);
                textAt = p.Plus(out.WithMagnitude(14/camera.scale));
            } else {
                Vector n, dir;
                EntityBase *wn = SK.GetEntity(workplane)->Normal();
                u = wn->NormalU();
                v = wn->NormalV();
                n = wn->NormalN();
                EntityBase *eA = SK.GetEntity(entityA);
                // Big pain; we have to get a vector tangent to the curve
                // at the shared point, which could be from either a cubic
                // or an arc.
                if(other) {
                    textAt = eA->EndpointFinish();
                    if(eA->type == Entity::Type::CUBIC) {
                        dir = eA->CubicGetFinishTangentNum();
                    } else {
                        dir = SK.GetEntity(eA->point[0])->PointGetNum().Minus(
                              SK.GetEntity(eA->point[2])->PointGetNum());
                        dir = n.Cross(dir);
                    }
                } else {
                    textAt = eA->EndpointStart();
                    if(eA->type == Entity::Type::CUBIC) {
                        dir = eA->CubicGetStartTangentNum();
                    } else {
                        dir = SK.GetEntity(eA->point[0])->PointGetNum().Minus(
                              SK.GetEntity(eA->point[1])->PointGetNum());
                        dir = n.Cross(dir);
                    }
                }
                dir = n.Cross(dir);
                textAt = textAt.Plus(dir.WithMagnitude(14/camera.scale));
            }

            Vector ex = VectorFont::Builtin()->GetExtents(textHeight, "T");
            canvas->DrawVectorText("T", textHeight, textAt.Minus(ex.ScaledBy(0.5)),
                                   u.WithMagnitude(1), v.WithMagnitude(1), hcs);
            if(refs) refs->push_back(textAt);
            return;
        }

        case Type::PARALLEL: {
            for(int i = 0; i < 2; i++) {
                Entity *e = SK.GetEntity(i == 0 ? entityA : entityB);
                Vector n = e->VectorGetNum();
                n = n.WithMagnitude(25/camera.scale);
                Vector u = (gn.Cross(n)).WithMagnitude(4/camera.scale);
                Vector p = e->VectorGetRefPoint();

                DoLine(canvas, hcs, p.Plus(u), p.Plus(u).Plus(n));
                DoLine(canvas, hcs, p.Minus(u), p.Minus(u).Plus(n));
                if(refs) refs->push_back(p.Plus(n.ScaledBy(0.5)));
            }
            return;
        }

        case Type::EQUAL_RADIUS: {
            for(int i = 0; i < 2; i++) {
                Vector ref;
                DoEqualRadiusTicks(canvas, hcs, i == 0 ? entityA : entityB, &ref);
                if(refs) refs->push_back(ref);
            }
            return;
        }

        case Type::EQUAL_LINE_ARC_LEN: {
            Entity *line = SK.GetEntity(entityA);
            Vector ref;
            DoEqualLenTicks(canvas, hcs,
                SK.GetEntity(line->point[0])->PointGetNum(),
                SK.GetEntity(line->point[1])->PointGetNum(),
                gn, &ref);
            if(refs) refs->push_back(ref);
            DoEqualRadiusTicks(canvas, hcs, entityB, &ref);
            if(refs) refs->push_back(ref);
            return;
        }

        case Type::LENGTH_RATIO:
        case Type::LENGTH_DIFFERENCE:
        case Type::EQUAL_LENGTH_LINES: {
            Vector a, b = Vector::From(0, 0, 0);
            for(int i = 0; i < 2; i++) {
                Entity *e = SK.GetEntity(i == 0 ? entityA : entityB);
                a = SK.GetEntity(e->point[0])->PointGetNum();
                b = SK.GetEntity(e->point[1])->PointGetNum();

                if(workplane != Entity::FREE_IN_3D) {
                    DoProjectedPoint(canvas, hcs, &a);
                    DoProjectedPoint(canvas, hcs, &b);
                }

                Vector ref;
                DoEqualLenTicks(canvas, hcs, a, b, gn, &ref);
                if(refs) refs->push_back(ref);
            }
            if((type == Type::LENGTH_RATIO) || (type == Type::LENGTH_DIFFERENCE)) {
                Vector ref = ((a.Plus(b)).ScaledBy(0.5)).Plus(disp.offset);
                DoLabel(canvas, hcs, ref, labelPos, gr, gu);
            }
            return;
        }

        case Type::EQ_LEN_PT_LINE_D: {
            Entity *forLen = SK.GetEntity(entityA);
            Vector a = SK.GetEntity(forLen->point[0])->PointGetNum(),
                   b = SK.GetEntity(forLen->point[1])->PointGetNum();
            if(workplane != Entity::FREE_IN_3D) {
                DoProjectedPoint(canvas, hcs, &a);
                DoProjectedPoint(canvas, hcs, &b);
            }
            Vector refa;
            DoEqualLenTicks(canvas, hcs, a, b, gn, &refa);
            if(refs) refs->push_back(refa);

            Entity *ln = SK.GetEntity(entityB);
            Vector la = SK.GetEntity(ln->point[0])->PointGetNum(),
                   lb = SK.GetEntity(ln->point[1])->PointGetNum();
            Vector pt = SK.GetEntity(ptA)->PointGetNum();
            if(workplane != Entity::FREE_IN_3D) {
                DoProjectedPoint(canvas, hcs, &pt);
                la = la.ProjectInto(workplane);
                lb = lb.ProjectInto(workplane);
            }

            Vector closest = pt.ClosestPointOnLine(la, lb.Minus(la));
            DoLine(canvas, hcs, pt, closest);
            Vector refb;
            DoEqualLenTicks(canvas, hcs, pt, closest, gn, &refb);
            if(refs) refs->push_back(refb);
            return;
        }

        case Type::EQ_PT_LN_DISTANCES: {
            for(int i = 0; i < 2; i++) {
                Entity *ln = SK.GetEntity(i == 0 ? entityA : entityB);
                Vector la = SK.GetEntity(ln->point[0])->PointGetNum(),
                       lb = SK.GetEntity(ln->point[1])->PointGetNum();
                Entity *pte = SK.GetEntity(i == 0 ? ptA : ptB);
                Vector pt = pte->PointGetNum();

                if(workplane != Entity::FREE_IN_3D) {
                    DoProjectedPoint(canvas, hcs, &pt);
                    la = la.ProjectInto(workplane);
                    lb = lb.ProjectInto(workplane);
                }

                Vector closest = pt.ClosestPointOnLine(la, lb.Minus(la));
                DoLine(canvas, hcs, pt, closest);

                Vector ref;
                DoEqualLenTicks(canvas, hcs, pt, closest, gn, &ref);
                if(refs) refs->push_back(ref);
            }
            return;
        }

        {
        case Type::SYMMETRIC:
            Vector n;
            n = SK.GetEntity(entityA)->Normal()->NormalN(); goto s;
        case Type::SYMMETRIC_HORIZ:
            n = SK.GetEntity(workplane)->Normal()->NormalU(); goto s;
        case Type::SYMMETRIC_VERT:
            n = SK.GetEntity(workplane)->Normal()->NormalV(); goto s;
        case Type::SYMMETRIC_LINE: {
            Entity *ln = SK.GetEntity(entityA);
            Vector la = SK.GetEntity(ln->point[0])->PointGetNum(),
                   lb = SK.GetEntity(ln->point[1])->PointGetNum();
            la = la.ProjectInto(workplane);
            lb = lb.ProjectInto(workplane);
            n = lb.Minus(la);
            Vector nw = SK.GetEntity(workplane)->Normal()->NormalN();
            n = n.RotatedAbout(nw, PI/2);
            goto s;
        }
s:
            Vector a = SK.GetEntity(ptA)->PointGetNum();
            Vector b = SK.GetEntity(ptB)->PointGetNum();

            for(int i = 0; i < 2; i++) {
                Vector tail = (i == 0) ? a : b;
                Vector d = (i == 0) ? b : a;
                d = d.Minus(tail);
                // Project the direction in which the arrow is drawn normal
                // to the symmetry plane; for projected symmetry constraints,
                // they might not be in the same direction, even when the
                // constraint is fully solved.
                d = n.ScaledBy(d.Dot(n));
                d = d.WithMagnitude(20/camera.scale);
                Vector tip = tail.Plus(d);

                DoLine(canvas, hcs, tail, tip);
                d = d.WithMagnitude(9/camera.scale);
                DoLine(canvas, hcs, tip, tip.Minus(d.RotatedAbout(gn,  0.6)));
                DoLine(canvas, hcs, tip, tip.Minus(d.RotatedAbout(gn, -0.6)));
                if(refs) refs->push_back(tip);
            }
            return;
        }

        case Type::AT_MIDPOINT:
        case Type::HORIZONTAL:
        case Type::VERTICAL:
            if(entityA.v) {
                Vector r, u, n;
                if(workplane == Entity::FREE_IN_3D) {
                    r = gr; u = gu; n = gn;
                } else {
                    r = SK.GetEntity(workplane)->Normal()->NormalU();
                    u = SK.GetEntity(workplane)->Normal()->NormalV();
                    n = r.Cross(u);
                }
                // For "at midpoint", this branch is always taken.
                Entity *e = SK.GetEntity(entityA);
                Vector a = SK.GetEntity(e->point[0])->PointGetNum();
                Vector b = SK.GetEntity(e->point[1])->PointGetNum();
                Vector m = (a.ScaledBy(0.5)).Plus(b.ScaledBy(0.5));
                Vector offset = (a.Minus(b)).Cross(n);
                offset = offset.WithMagnitude(textHeight);
                // Draw midpoint constraint on other side of line, so that
                // a line can be midpoint and horizontal at same time.
                if(type == Type::AT_MIDPOINT) offset = offset.ScaledBy(-1);

                std::string s;
                switch(type) {
                    case Type::HORIZONTAL:  s = "H"; break;
                    case Type::VERTICAL:    s = "V"; break;
                    case Type::AT_MIDPOINT: s = "M"; break;
                    default: ssassert(false, "Unexpected constraint type");
                }
                Vector o  = m.Plus(offset).Plus(u.WithMagnitude(textHeight/5)),
                       ex = VectorFont::Builtin()->GetExtents(textHeight, s);
                Vector shift = r.WithMagnitude(ex.x).Plus(
                               u.WithMagnitude(ex.y));

                canvas->DrawVectorText(s, textHeight, o.Minus(shift.ScaledBy(0.5)),
                                       r.WithMagnitude(1), u.WithMagnitude(1), hcs);
                if(refs) refs->push_back(o);
            } else {
                Vector a = SK.GetEntity(ptA)->PointGetNum();
                Vector b = SK.GetEntity(ptB)->PointGetNum();

                Entity *w = SK.GetEntity(workplane);
                Vector cu = w->Normal()->NormalU();
                Vector cv = w->Normal()->NormalV();
                Vector cn = w->Normal()->NormalN();

                int i;
                for(i = 0; i < 2; i++) {
                    Vector o = (i == 0) ? a : b;
                    Vector oo = (i == 0) ? a.Minus(b) : b.Minus(a);
                    Vector d = (type == Type::HORIZONTAL) ? cu : cv;
                    if(oo.Dot(d) < 0) d = d.ScaledBy(-1);

                    Vector dp = cn.Cross(d);
                    d = d.WithMagnitude(14/camera.scale);
                    Vector c = o.Minus(d);
                    DoLine(canvas, hcs, o, c);
                    d = d.WithMagnitude(3/camera.scale);
                    dp = dp.WithMagnitude(2/camera.scale);
                    canvas->DrawQuad((c.Plus(d)).Plus(dp),
                                     (c.Minus(d)).Plus(dp),
                                     (c.Minus(d)).Minus(dp),
                                     (c.Plus(d)).Minus(dp),
                                     hcf);
                    if(refs) refs->push_back(c);
                }
            }
            return;

        case Type::COMMENT: {
            Vector u, v;
            if(workplane == Entity::FREE_IN_3D) {
                u = gr;
                v = gu;
            } else {
                EntityBase *norm = SK.GetEntity(workplane)->Normal();
                u = norm->NormalU();
                v = norm->NormalV();
            }

            if(disp.style.v != 0) {
                RgbaColor color = stroke.color;
                stroke = Style::Stroke(disp.style);
                stroke.layer = Canvas::Layer::FRONT;
                if(how != DrawAs::DEFAULT) {
                    stroke.color = color;
                }
                hcs = canvas->GetStroke(stroke);
            }
            DoLabel(canvas, hcs, disp.offset, labelPos, u, v);
            if(refs) refs->push_back(disp.offset);
            return;
        }
    }
    ssassert(false, "Unexpected constraint type");
}

void Constraint::Draw(DrawAs how, Canvas *canvas) {
    DoLayout(how, canvas, NULL, NULL);
}

Vector Constraint::GetLabelPos(const Camera &camera) {
    Vector p;

    ObjectPicker canvas = {};
    canvas.camera = camera;
    DoLayout(DrawAs::DEFAULT, &canvas, &p, NULL);
    canvas.Clear();

    return p;
}

void Constraint::GetReferencePoints(const Camera &camera, std::vector<Vector> *refs) {
    ObjectPicker canvas = {};
    canvas.camera = camera;
    DoLayout(DrawAs::DEFAULT, &canvas, NULL, refs);
    canvas.Clear();
}

bool Constraint::IsStylable() const {
    if(type == Type::COMMENT) return true;
    return false;
}

hStyle Constraint::GetStyle() const {
    if(disp.style.v != 0) return disp.style;
    return { Style::CONSTRAINT };
}

bool Constraint::HasLabel() const {
    switch(type) {
        case Type::COMMENT:
        case Type::PT_PT_DISTANCE:
        case Type::PT_PLANE_DISTANCE:
        case Type::PT_LINE_DISTANCE:
        case Type::PT_FACE_DISTANCE:
        case Type::PROJ_PT_DISTANCE:
        case Type::LENGTH_RATIO:
        case Type::LENGTH_DIFFERENCE:
        case Type::DIAMETER:
        case Type::ANGLE:
            return true;

        default:
            return false;
    }
}
