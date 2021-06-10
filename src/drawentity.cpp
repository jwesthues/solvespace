//-----------------------------------------------------------------------------
// Draw a representation of an entity on-screen, in the case of curves up
// to our chord tolerance, or return the distance from the user's mouse pointer
// to the entity for selection.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

std::string Entity::DescriptionString() const {
    if(h.isFromRequest()) {
        Request *r = SK.GetRequest(h.request());
        return r->DescriptionString();
    } else {
        Group *g = SK.GetGroup(h.group());
        return g->DescriptionString();
    }
}

void Entity::GenerateEdges(SEdgeList *el) {
    SBezierList *sbl = GetOrGenerateBezierCurves();

    for(int i = 0; i < sbl->l.n; i++) {
        SBezier *sb = &(sbl->l[i]);

        List<Vector> lv = {};
        sb->MakePwlInto(&lv);
        for(int j = 1; j < lv.n; j++) {
            el->AddEdge(lv[j-1], lv[j], Style::ForEntity(h).v, i);
        }
        lv.Clear();
    }
}

SBezierList *Entity::GetOrGenerateBezierCurves() {
    if(beziers.l.IsEmpty())
        GenerateBezierCurves(&beziers);
    return &beziers;
}

SEdgeList *Entity::GetOrGenerateEdges() {
    if(!edges.l.IsEmpty()) {
        if(EXACT(edgesChordTol == SS.ChordTolMm()))
            return &edges;
        edges.l.Clear();
    }
    if(edges.l.IsEmpty())
        GenerateEdges(&edges);
    edgesChordTol = SS.ChordTolMm();
    return &edges;
}

BBox Entity::GetOrGenerateScreenBBox(bool *hasBBox) {
    SBezierList *sbl = GetOrGenerateBezierCurves();

    // We don't bother with bounding boxes for workplanes, etc.
    *hasBBox = (IsPoint() || IsNormal() || !sbl->l.IsEmpty());
    if(!*hasBBox) return {};

    if(screenBBoxValid)
        return screenBBox;

    if(IsPoint()) {
        Vector proj = SS.GW.ProjectPoint3(PointGetNum());
        screenBBox = BBox::From(proj, proj);
    } else if(IsNormal()) {
        Vector proj = SS.GW.ProjectPoint3(SK.GetEntity(point[0])->PointGetNum());
        screenBBox = BBox::From(proj, proj);
    } else if(!sbl->l.IsEmpty()) {
        Vector first = SS.GW.ProjectPoint3(sbl->l[0].ctrl[0]);
        screenBBox = BBox::From(first, first);
        for(auto &sb : sbl->l) {
            for(int i = 0; i <= sb.deg; ++i) { screenBBox.Include(SS.GW.ProjectPoint3(sb.ctrl[i])); }
        }
    } else
        ssassert(false, "Expected entity to be a point or have beziers");

    screenBBoxValid = true;
    return screenBBox;
}

void Entity::GetReferencePoints(std::vector<Vector> *refs) {
    switch(type) {
        case Type::POINT_N_COPY:
        case Type::POINT_N_TRANS:
        case Type::POINT_N_ROT_TRANS:
        case Type::POINT_N_ROT_AA:
        case Type::POINT_N_ROT_AXIS_TRANS:
        case Type::POINT_MIRROR:
        case Type::POINT_IN_3D:
        case Type::POINT_IN_2D:
            refs->push_back(PointGetNum());
            break;

        case Type::NORMAL_N_COPY:
        case Type::NORMAL_N_ROT:
        case Type::NORMAL_N_ROT_AA:
        case Type::NORMAL_IN_3D:
        case Type::NORMAL_IN_2D:
        case Type::NORMAL_MIRROR:
        case Type::WORKPLANE:
        case Type::CIRCLE:
        case Type::ARC_OF_CIRCLE:
        case Type::CUBIC:
        case Type::CUBIC_PERIODIC:
        case Type::TTF_TEXT:
        case Type::IMAGE:
            refs->push_back(SK.GetEntity(point[0])->PointGetNum());
            break;

        case Type::LINE_SEGMENT: {
            Vector a = SK.GetEntity(point[0])->PointGetNum(),
                   b = SK.GetEntity(point[1])->PointGetNum();
            refs->push_back(b.Plus(a.Minus(b).ScaledBy(0.5)));
            break;
        }

        case Type::DISTANCE:
        case Type::DISTANCE_N_COPY:
        case Type::FACE_NORMAL_PT:
        case Type::FACE_XPROD:
        case Type::FACE_N_ROT_TRANS:
        case Type::FACE_N_TRANS:
        case Type::FACE_N_ROT_AA:
        case Type::FACE_ROT_NORMAL_PT:
        case Type::FACE_MIRROR:
        case Type::FACE_N_ROT_AXIS_TRANS:
            break;
    }
}

int Entity::GetPositionOfPoint(const Camera &camera, Point2d p) {
    int position;

    ObjectPicker canvas = {};
    canvas.camera      = camera;
    canvas.point       = p;
    canvas.minDistance = 1e12;
    Draw(DrawAs::DEFAULT, &canvas);
    position = canvas.position;
    canvas.Clear();

    return position;
}

bool Entity::IsStylable() const {
    if(IsPoint()) return false;
    if(IsWorkplane()) return false;
    if(IsNormal()) return false;
    return true;
}

bool Entity::IsVisible() const {
    Group *g = SK.GetGroup(group);

    if(g->h == Group::HGROUP_REFERENCES && IsNormal()) {
        // The reference normals are always shown
        return true;
    }
    if(!(g->IsVisible())) return false;

    if(IsPoint() && !SS.GW.showPoints) return false;
    if(IsNormal() && !SS.GW.showNormals) return false;
    if(construction && !SS.GW.showConstruction) return false;

    if(!SS.GW.showWorkplanes) {
        if(IsWorkplane() && !h.isFromRequest()) {
            if(g->h != SS.GW.activeGroup) {
                // The group-associated workplanes are hidden outside
                // their group.
                return false;
            }
        }
    }

    if(style.v) {
        Style *s = Style::Get(style);
        if(!s->visible) return false;
    }

    if(forceHidden) return false;

    return true;
}

// entities that were created via some copy types will not be
// draggable with the mouse. We identify the undraggables here
bool Entity::CanBeDragged() const {
    // a numeric copy can not move
    if(type == Entity::Type::POINT_N_COPY) return false;
    // these transforms applied zero times can not be moved
    if(((type == Entity::Type::POINT_N_TRANS) ||
       (type == Entity::Type::POINT_N_ROT_AA) ||
       (type == Entity::Type::POINT_N_ROT_AXIS_TRANS))
        && (timesApplied == 0)) return false;
    // for these types of entities the first point will indicate draggability
    if(HasEndpoints() || type == Entity::Type::CIRCLE) {
        return SK.GetEntity(point[0])->CanBeDragged();
    }
    // if we're not certain it can't be dragged then default to true
    return true;
}

void Entity::CalculateNumerical(bool forExport) {
    if(IsPoint()) actPoint = PointGetNum();
    if(IsNormal()) actNormal = NormalGetNum();
    if(type == Type::DISTANCE || type == Type::DISTANCE_N_COPY) {
        actDistance = DistanceGetNum();
    }
    if(IsFace()) {
        actPoint  = FaceGetPointNum();
        Vector n = FaceGetNormalNum();
        actNormal = Quaternion::From(0, n.x, n.y, n.z);
    }
    if(forExport) {
        // Visibility in copied linked entities follows source file
        actVisible = IsVisible();
    } else {
        // Copied entities within a file are always visible
        actVisible = true;
    }
}

//-----------------------------------------------------------------------------
// Compute a cubic, second derivative continuous, interpolating spline. Same
// routine for periodic splines (in a loop) or open splines (with specified
// end tangents).
//-----------------------------------------------------------------------------
void Entity::ComputeInterpolatingSpline(SBezierList *sbl, bool periodic) const {
    static const int MAX_N = BandedMatrix::MAX_UNKNOWNS;
    int ep = extraPoints;

    // The number of unknowns to solve for.
    int n   = periodic ? 3 + ep : ep;
    ssassert(n < MAX_N, "Too many unknowns");
    // The number of on-curve points, one more than the number of segments.
    int pts = periodic ? 4 + ep : 2 + ep;

    int i, j, a;

    // The starting and finishing control points that define our end tangents
    // (if the spline isn't periodic), and the on-curve points.
    Vector ctrl_s = Vector::From(0, 0, 0);
    Vector ctrl_f = Vector::From(0, 0, 0);
    Vector pt[MAX_N+4];
    if(periodic) {
        for(i = 0; i < ep + 3; i++) {
            pt[i] = SK.GetEntity(point[i])->PointGetNum();
        }
        pt[i++] = SK.GetEntity(point[0])->PointGetNum();
    } else {
        ctrl_s = SK.GetEntity(point[1])->PointGetNum();
        ctrl_f = SK.GetEntity(point[ep+2])->PointGetNum();
        j = 0;
        pt[j++] = SK.GetEntity(point[0])->PointGetNum();
        for(i = 2; i <= ep + 1; i++) {
            pt[j++] = SK.GetEntity(point[i])->PointGetNum();
        }
        pt[j++] = SK.GetEntity(point[ep+3])->PointGetNum();
    }

    // The unknowns that we will be solving for, a set for each coordinate.
    double Xx[MAX_N], Xy[MAX_N], Xz[MAX_N];
    // For a cubic Bezier section f(t) as t goes from 0 to 1,
    //    f' (0) = 3*(P1 - P0)
    //    f' (1) = 3*(P3 - P2)
    //    f''(0) = 6*(P0 - 2*P1 + P2)
    //    f''(1) = 6*(P3 - 2*P2 + P1)
    for(a = 0; a < 3; a++) {
        BandedMatrix bm = {};
        bm.n = n;

        for(i = 0; i < n; i++) {
            int im, it, ip;
            if(periodic) {
                im = WRAP(i - 1, n);
                it = i;
                ip = WRAP(i + 1, n);
            } else {
                im = i;
                it = i + 1;
                ip = i + 2;
            }
            // All of these are expressed in terms of a constant part, and
            // of X[i-1], X[i], and X[i+1]; so let these be the four
            // components of that vector;
            Vector4 A, B, C, D, E;
            // The on-curve interpolated point
            C = Vector4::From((pt[it]).Element(a), 0, 0, 0);
            // control point one back, C - X[i]
            B = C.Plus(Vector4::From(0, 0, -1, 0));
            // control point one forward, C + X[i]
            D = C.Plus(Vector4::From(0, 0, 1, 0));
            // control point two back
            if(i == 0 && !periodic) {
                A = Vector4::From(ctrl_s.Element(a), 0, 0, 0);
            } else {
                // pt[im] + X[i-1]
                A = Vector4::From(pt[im].Element(a), 1, 0, 0);
            }
            // control point two forward
            if(i == (n - 1) && !periodic) {
                E = Vector4::From(ctrl_f.Element(a), 0, 0, 0);
            } else {
                // pt[ip] - X[i+1]
                E = Vector4::From((pt[ip]).Element(a), 0, 0, -1);
            }
            // Write the second derivatives of each segment, dropping constant
            Vector4 fprev_pp = (C.Minus(B.ScaledBy(2))).Plus(A),
                    fnext_pp = (C.Minus(D.ScaledBy(2))).Plus(E),
                    eq       = fprev_pp.Minus(fnext_pp);

            bm.B[i] = -eq.w;
            if(periodic) {
                bm.A[i][WRAP(i-2, n)] = eq.x;
                bm.A[i][WRAP(i-1, n)] = eq.y;
                bm.A[i][i]            = eq.z;
            } else {
                // The wrapping would work, except when n = 1 and everything
                // wraps to zero...
                if(i > 0) {
                    bm.A[i][i - 1] = eq.x;
                }
                bm.A[i][i] = eq.y;
                if(i < (n-1)) {
                    bm.A[i][i + 1] = eq.z;
                }
            }
        }
        bm.Solve();
        double *X = (a == 0) ? Xx :
                    (a == 1) ? Xy :
                               Xz;
        memcpy(X, bm.X, n*sizeof(double));
    }

    for(i = 0; i < pts - 1; i++) {
        Vector p0, p1, p2, p3;
        if(periodic) {
            p0 = pt[i];
            int iw = WRAP(i - 1, n);
            p1 = p0.Plus(Vector::From(Xx[iw], Xy[iw], Xz[iw]));
        } else if(i == 0) {
            p0 = pt[0];
            p1 = ctrl_s;
        } else {
            p0 = pt[i];
            p1 = p0.Plus(Vector::From(Xx[i-1], Xy[i-1], Xz[i-1]));
        }
        if(periodic) {
            p3 = pt[i+1];
            int iw = WRAP(i, n);
            p2 = p3.Minus(Vector::From(Xx[iw], Xy[iw], Xz[iw]));
        } else if(i == (pts - 2)) {
            p3 = pt[pts-1];
            p2 = ctrl_f;
        } else {
            p3 = pt[i+1];
            p2 = p3.Minus(Vector::From(Xx[i], Xy[i], Xz[i]));
        }
        SBezier sb = SBezier::From(p0, p1, p2, p3);
        sbl->l.Add(&sb);
    }
}

void Entity::GenerateBezierCurves(SBezierList *sbl) const {
    SBezier sb;

    int i = sbl->l.n;

    switch(type) {
        case Type::LINE_SEGMENT: {
            Vector a = SK.GetEntity(point[0])->PointGetNum();
            Vector b = SK.GetEntity(point[1])->PointGetNum();
            sb = SBezier::From(a, b);
            sb.entity = h.v;
            sbl->l.Add(&sb);
            break;
        }
        case Type::CUBIC:
            ComputeInterpolatingSpline(sbl, /*periodic=*/false);
            break;

        case Type::CUBIC_PERIODIC:
            ComputeInterpolatingSpline(sbl, /*periodic=*/true);
            break;

        case Type::CIRCLE:
        case Type::ARC_OF_CIRCLE: {
            Vector center = SK.GetEntity(point[0])->PointGetNum();
            Quaternion q = SK.GetEntity(normal)->NormalGetNum();
            Vector u = q.RotationU(), v = q.RotationV();
            double r = CircleGetRadiusNum();
            double thetaa, thetab, dtheta;

            if(r < LENGTH_EPS) {
                // If a circle or an arc gets dragged through zero radius,
                // then we just don't generate anything.
                break;
            }

            if(type == Type::CIRCLE) {
                thetaa = 0;
                thetab = 2*PI;
                dtheta = 2*PI;
            } else {
                ArcGetAngles(&thetaa, &thetab, &dtheta);
            }
            int i, n;
            if(dtheta > (3*PI/2 + 0.01)) {
                n = 4;
            } else if(dtheta > (PI + 0.01)) {
                n = 3;
            } else if(dtheta > (PI/2 + 0.01)) {
                n = 2;
            } else {
                n = 1;
            }
            dtheta /= n;

            for(i = 0; i < n; i++) {
                double s, c;

                c = cos(thetaa);
                s = sin(thetaa);
                // The start point of the curve, and the tangent vector at
                // that start point.
                Vector p0 = center.Plus(u.ScaledBy( r*c)).Plus(v.ScaledBy(r*s)),
                       t0 =             u.ScaledBy(-r*s). Plus(v.ScaledBy(r*c));

                thetaa += dtheta;

                c = cos(thetaa);
                s = sin(thetaa);
                Vector p2 = center.Plus(u.ScaledBy( r*c)).Plus(v.ScaledBy(r*s)),
                       t2 =             u.ScaledBy(-r*s). Plus(v.ScaledBy(r*c));

                // The control point must lie on both tangents.
                Vector p1 = Vector::AtIntersectionOfLines(p0, p0.Plus(t0),
                                                          p2, p2.Plus(t2),
                                                          NULL);

                SBezier sb = SBezier::From(p0, p1, p2);
                sb.weight[1] = cos(dtheta/2);
                sbl->l.Add(&sb);
            }
            break;
        }

        case Type::TTF_TEXT: {
            Vector topLeft = SK.GetEntity(point[0])->PointGetNum();
            Vector botLeft = SK.GetEntity(point[1])->PointGetNum();
            Vector n = Normal()->NormalN();
            Vector v = topLeft.Minus(botLeft);
            Vector u = (v.Cross(n)).WithMagnitude(v.Magnitude());

            SS.fonts.PlotString(font, str, sbl, botLeft, u, v);
            break;
        }

        default:
            // Not a problem, points and normals and such don't generate curves
            break;
    }

    // Record our style for all of the Beziers that we just created.
    for(; i < sbl->l.n; i++) {
        sbl->l[i].auxA = Style::ForEntity(h).v;
    }
}

void Entity::Draw(DrawAs how, Canvas *canvas) {
    if(!(how == DrawAs::HOVERED || how == DrawAs::SELECTED) &&
       !IsVisible()) return;

    int zIndex;
    if(IsPoint()) {
        zIndex = 6;
    } else if(how == DrawAs::HIDDEN) {
        zIndex = 2;
    } else if(group != SS.GW.activeGroup) {
        zIndex = 3;
    } else {
        zIndex = 5;
    }

    hStyle hs;
    if(IsPoint()) {
        hs.v = Style::DATUM;
    } else if(IsNormal() || type == Type::WORKPLANE) {
        hs.v = Style::NORMALS;
    } else {
        hs = Style::ForEntity(h);
        if (hs.v == Style::CONSTRUCTION) {
            zIndex = 4;
        }
    }

    Canvas::Stroke stroke = Style::Stroke(hs);
    switch(how) {
        case DrawAs::DEFAULT:
            stroke.layer = Canvas::Layer::NORMAL;
            break;

        case DrawAs::OVERLAY:
            stroke.layer = Canvas::Layer::FRONT;
            break;

        case DrawAs::HIDDEN:
            stroke.layer = Canvas::Layer::OCCLUDED;
            stroke.stipplePattern = Style::PatternType({ Style::HIDDEN_EDGE });
            stroke.stippleScale   = Style::Get({ Style::HIDDEN_EDGE })->stippleScale;
            break;

        case DrawAs::HOVERED:
            stroke.layer = Canvas::Layer::FRONT;
            stroke.color = Style::Color(Style::HOVERED);
            break;

        case DrawAs::SELECTED:
            stroke.layer = Canvas::Layer::FRONT;
            stroke.color = Style::Color(Style::SELECTED);
            break;
    }
    stroke.zIndex = zIndex;
    Canvas::hStroke hcs = canvas->GetStroke(stroke);

    switch(type) {
        case Type::POINT_N_COPY:
        case Type::POINT_N_TRANS:
        case Type::POINT_N_ROT_TRANS:
        case Type::POINT_N_ROT_AA:
        case Type::POINT_N_ROT_AXIS_TRANS:
        case Type::POINT_IN_3D:
        case Type::POINT_IN_2D:
        case Type::POINT_MIRROR: {
            if(how == DrawAs::HIDDEN) return;

            // If we're analyzing the sketch to show the degrees of freedom,
            // then we draw big colored squares over the points that are
            // free to move.
            bool free = false;
            if(type == Type::POINT_IN_3D) {
                Param *px = SK.GetParam(param[0]),
                      *py = SK.GetParam(param[1]),
                      *pz = SK.GetParam(param[2]);

                free = px->free || py->free || pz->free;
            } else if(type == Type::POINT_IN_2D) {
                Param *pu = SK.GetParam(param[0]),
                      *pv = SK.GetParam(param[1]);

                free = pu->free || pv->free;
            }

            Canvas::Stroke pointStroke = {};
            pointStroke.layer  = (free) ? Canvas::Layer::FRONT : stroke.layer;
            pointStroke.zIndex = stroke.zIndex;
            pointStroke.color  = stroke.color;
            pointStroke.width  = 7.0;
            pointStroke.unit   = Canvas::Unit::PX;
            Canvas::hStroke hcsPoint = canvas->GetStroke(pointStroke);

            if(free) {
                Canvas::Stroke analyzeStroke = Style::Stroke(Style::ANALYZE);
                analyzeStroke.width = 14.0;
                analyzeStroke.layer = Canvas::Layer::FRONT;
                Canvas::hStroke hcsAnalyze = canvas->GetStroke(analyzeStroke);

                canvas->DrawPoint(PointGetNum(), hcsAnalyze);
            }

            canvas->DrawPoint(PointGetNum(), hcsPoint);
            return;
        }

        case Type::NORMAL_N_COPY:
        case Type::NORMAL_N_ROT:
        case Type::NORMAL_N_ROT_AA:
        case Type::NORMAL_MIRROR:
        case Type::NORMAL_IN_3D:
        case Type::NORMAL_IN_2D: {
            const Camera &camera = canvas->GetCamera();

            if(how == DrawAs::HIDDEN) return;

            for(int i = 0; i < 2; i++) {
                bool asReference = (i == 1);
                if(asReference) {
                    if(!h.request().IsFromReferences()) continue;
                } else {
                    if(!SK.GetGroup(group)->IsVisible() || !SS.GW.showNormals) continue;
                }

                stroke.layer = (asReference) ? Canvas::Layer::FRONT : Canvas::Layer::NORMAL;
                if(how != DrawAs::HOVERED && how != DrawAs::SELECTED) {
                    // Always draw the x, y, and z axes in red, green, and blue;
                    // brighter for the ones at the bottom left of the screen,
                    // dimmer for the ones at the model origin.
                    hRequest hr   = h.request();
                    uint8_t  luma = (asReference) ? 255 : 100;
                    if(hr == Request::HREQUEST_REFERENCE_XY) {
                        stroke.color = RgbaColor::From(0, 0, luma);
                    } else if(hr == Request::HREQUEST_REFERENCE_YZ) {
                        stroke.color = RgbaColor::From(luma, 0, 0);
                    } else if(hr == Request::HREQUEST_REFERENCE_ZX) {
                        stroke.color = RgbaColor::From(0, luma, 0);
                    }
                }

                Quaternion q = NormalGetNum();
                Vector tail;
                if(asReference) {
                    // Draw an extra copy of the x, y, and z axes, that's
                    // always in the corner of the view and at the front.
                    // So those are always available, perhaps useful.
                    stroke.width = 2;
                    double s = camera.scale;
                    double h = 60 - camera.height / 2.0;
                    double w = 60 - camera.width  / 2.0;
                    // Shift the axis to the right if they would overlap with the toolbar.
                    if(SS.showToolbar) {
                        if(h + 30 > -(32*18 + 3*16 + 8) / 2)
                            w += 60;
                    }
                    tail = camera.projRight.ScaledBy(w/s).Plus(
                           camera.projUp.   ScaledBy(h/s)).Minus(camera.offset);
                } else {
                    tail = SK.GetEntity(point[0])->PointGetNum();
                }
                tail = camera.AlignToPixelGrid(tail);

                hcs = canvas->GetStroke(stroke);
                Vector v = (q.RotationN()).WithMagnitude(50.0 / camera.scale);
                Vector tip = tail.Plus(v);
                canvas->DrawLine(tail, tip, hcs);

                v = v.WithMagnitude(12.0 / camera.scale);
                Vector axis = q.RotationV();
                canvas->DrawLine(tip, tip.Minus(v.RotatedAbout(axis,  0.6)), hcs);
                canvas->DrawLine(tip, tip.Minus(v.RotatedAbout(axis, -0.6)), hcs);

                if(type == Type::NORMAL_IN_3D) {
                    Param *nw = SK.GetParam(param[0]),
                          *nx = SK.GetParam(param[1]),
                          *ny = SK.GetParam(param[2]),
                          *nz = SK.GetParam(param[3]);

                    if(nw->free || nx->free || ny->free || nz->free) {
                        Canvas::Stroke analyzeStroke = Style::Stroke(Style::ANALYZE);
                        analyzeStroke.layer = Canvas::Layer::FRONT;
                        Canvas::hStroke hcsAnalyze = canvas->GetStroke(analyzeStroke);
                        canvas->DrawLine(tail, tip, hcsAnalyze);
                    }
                }
            }
            return;
        }

        case Type::DISTANCE:
        case Type::DISTANCE_N_COPY:
            // These are used only as data structures, nothing to display.
            return;

        case Type::WORKPLANE: {
            const Camera &camera = canvas->GetCamera();

            Vector p = SK.GetEntity(point[0])->PointGetNum();
            p = camera.AlignToPixelGrid(p);

            Vector u = Normal()->NormalU();
            Vector v = Normal()->NormalV();

            double s = (std::min(camera.width, camera.height)) * 0.45 / camera.scale;

            Vector us = u.ScaledBy(s);
            Vector vs = v.ScaledBy(s);

            Vector pp = p.Plus (us).Plus (vs);
            Vector pm = p.Plus (us).Minus(vs);
            Vector mm = p.Minus(us).Minus(vs), mm2 = mm;
            Vector mp = p.Minus(us).Plus (vs);

            Canvas::Stroke strokeBorder = stroke;
            strokeBorder.zIndex        -= 3;
            strokeBorder.stipplePattern = StipplePattern::SHORT_DASH;
            strokeBorder.stippleScale   = 8.0;
            Canvas::hStroke hcsBorder = canvas->GetStroke(strokeBorder);

            double textHeight = Style::TextHeight(hs) / camera.scale;

            if(!h.isFromRequest()) {
                mm = mm.Plus(v.ScaledBy(textHeight * 4.7));
                mm2 = mm2.Plus(u.ScaledBy(textHeight * 4.7));
                canvas->DrawLine(mm2, mm, hcsBorder);
            }
            canvas->DrawLine(pp,  pm, hcsBorder);
            canvas->DrawLine(mm2, pm, hcsBorder);
            canvas->DrawLine(mm,  mp, hcsBorder);
            canvas->DrawLine(pp,  mp, hcsBorder);

            Vector o = mm2.Plus(u.ScaledBy(3.0 / camera.scale)).Plus(
                                v.ScaledBy(3.0 / camera.scale));
            std::string shortDesc = DescriptionString().substr(5);
            canvas->DrawVectorText(shortDesc, textHeight, o, u, v, hcs);
            return;
        }

        case Type::LINE_SEGMENT:
        case Type::CIRCLE:
        case Type::ARC_OF_CIRCLE:
        case Type::CUBIC:
        case Type::CUBIC_PERIODIC:
        case Type::TTF_TEXT: {
            // Generate the rational polynomial curves, then piecewise linearize
            // them, and display those.
            if(!canvas->DrawBeziers(*GetOrGenerateBezierCurves(),  hcs)) {
                canvas->DrawEdges(*GetOrGenerateEdges(), hcs);
            }
            if(type == Type::CIRCLE) {
                Entity *dist = SK.GetEntity(distance);
                if(dist->type == Type::DISTANCE) {
                    Param *p = SK.GetParam(dist->param[0]);
                    if(p->free) {
                        Canvas::Stroke analyzeStroke = Style::Stroke(Style::ANALYZE);
                        analyzeStroke.layer = Canvas::Layer::FRONT;
                        Canvas::hStroke hcsAnalyze = canvas->GetStroke(analyzeStroke);
                        if(!canvas->DrawBeziers(*GetOrGenerateBezierCurves(), hcsAnalyze)) {
                            canvas->DrawEdges(*GetOrGenerateEdges(), hcsAnalyze);
                        }
                    }
                }
            }
            return;
        }
        case Type::IMAGE: {
            Canvas::Fill fill = {};
            std::shared_ptr<Pixmap> pixmap;
            switch(how) {
                case DrawAs::HIDDEN: return;

                case DrawAs::HOVERED: {
                    fill.color   = Style::Color(Style::HOVERED).WithAlpha(180);
                    fill.pattern = Canvas::FillPattern::CHECKERED_A;
                    fill.zIndex  = 2;
                    break;
                }

                case DrawAs::SELECTED: {
                    fill.color   = Style::Color(Style::SELECTED).WithAlpha(180);
                    fill.pattern = Canvas::FillPattern::CHECKERED_B;
                    fill.zIndex  = 1;
                    break;
                }

                default:
                    fill.color   = RgbaColor::FromFloat(1.0f, 1.0f, 1.0f);
                    pixmap       = SS.images[file];
                    break;
            }

            Canvas::hFill hf = canvas->GetFill(fill);
            Vector v[4] = {};
            for(int i = 0; i < 4; i++) {
                v[i] = SK.GetEntity(point[i])->PointGetNum();
            }
            Vector iu = v[3].Minus(v[0]);
            Vector iv = v[1].Minus(v[0]);

            if(how == DrawAs::DEFAULT && pixmap == NULL) {
                Canvas::Stroke stroke = Style::Stroke(Style::DRAW_ERROR);
                stroke.color = stroke.color.WithAlpha(50);
                Canvas::hStroke hs = canvas->GetStroke(stroke);
                canvas->DrawLine(v[0], v[2], hs);
                canvas->DrawLine(v[1], v[3], hs);
                for(int i = 0; i < 4; i++) {
                    canvas->DrawLine(v[i], v[(i + 1) % 4], hs);
                }
            } else {
                canvas->DrawPixmap(pixmap, v[0], iu, iv,
                                   Point2d::From(0.0, 0.0), Point2d::From(1.0, 1.0), hf);
            }
        }

        case Type::FACE_NORMAL_PT:
        case Type::FACE_XPROD:
        case Type::FACE_N_ROT_TRANS:
        case Type::FACE_N_TRANS:
        case Type::FACE_N_ROT_AA:
        case Type::FACE_ROT_NORMAL_PT:
        case Type::FACE_MIRROR:
        case Type::FACE_N_ROT_AXIS_TRANS:
            // Do nothing; these are drawn with the triangle mesh
            return;
    }
    ssassert(false, "Unexpected entity type");
}
