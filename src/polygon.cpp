//-----------------------------------------------------------------------------
// Operations on polygons (planar, of line segment edges).
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"

Vector STriangle::Normal() const {
    Vector ab = b.Minus(a), bc = c.Minus(b);
    return ab.Cross(bc);
}

double STriangle::MinAltitude() const {
    double altA = a.DistanceToLine(b, c.Minus(b)),
           altB = b.DistanceToLine(c, a.Minus(c)),
           altC = c.DistanceToLine(a, b.Minus(a));

    return min(altA, min(altB, altC));
}

bool STriangle::ContainsPoint(Vector p) const {
    Vector n = Normal();
    if(MinAltitude() < LENGTH_EPS) {
        // shouldn't happen; zero-area triangle
        return false;
    }
    return ContainsPointProjd(n.WithMagnitude(1), p);
}

bool STriangle::ContainsPointProjd(Vector n, Vector p) const {
    Vector ab = b.Minus(a), bc = c.Minus(b), ca = a.Minus(c);

    Vector no_ab = n.Cross(ab);
    if(no_ab.Dot(p) < no_ab.Dot(a) - LENGTH_EPS) return false;

    Vector no_bc = n.Cross(bc);
    if(no_bc.Dot(p) < no_bc.Dot(b) - LENGTH_EPS) return false;

    Vector no_ca = n.Cross(ca);
    if(no_ca.Dot(p) < no_ca.Dot(c) - LENGTH_EPS) return false;

    return true;
}

bool STriangle::Raytrace(const Vector &rayPoint, const Vector &rayDir,
                         double *t, Vector *inters) const {
    // Algorithm from: "Fast, Minimum Storage Ray/Triangle Intersection" by
    // Tomas Moeller and Ben Trumbore.

    // Find vectors for two edges sharing vertex A.
    Vector edge1 = b.Minus(a);
    Vector edge2 = c.Minus(a);

    // Begin calculating determinant - also used to calculate U parameter.
    Vector pvec = rayDir.Cross(edge2);

    // If determinant is near zero, ray lies in plane of triangle.
    // Also, cull back facing triangles here.
    double det = edge1.Dot(pvec);
    if(-det < LENGTH_EPS) return false;
    double inv_det = 1.0f / det;

    // Calculate distance from vertex A to ray origin.
    Vector tvec = rayPoint.Minus(a);

    // Calculate U parameter and test bounds.
    double u = tvec.Dot(pvec) * inv_det;
    if (u < 0.0f || u > 1.0f) return false;

    // Prepare to test V parameter.
    Vector qvec = tvec.Cross(edge1);

    // Calculate V parameter and test bounds.
    double v = rayDir.Dot(qvec) * inv_det;
    if (v < 0.0f || u + v > 1.0f) return false;

    // Calculate t, ray intersects triangle.
    *t = edge2.Dot(qvec) * inv_det;

    // Calculate intersection point.
    if(inters != NULL) *inters = rayPoint.Plus(rayDir.ScaledBy(*t));

    return true;
}

double STriangle::SignedVolume() const {
    return a.Dot(b.Cross(c)) / 6.0;
}

double STriangle::Area() const {
    Vector ab = a.Minus(b);
    Vector cb = c.Minus(b);
    return ab.Cross(cb).Magnitude() / 2.0;
}

bool STriangle::IsDegenerate() const {
    return a.OnLineSegment(b, c) ||
           b.OnLineSegment(a, c) ||
           c.OnLineSegment(a, b);
}

void STriangle::FlipNormal() {
    swap(a, b);
    swap(an, bn);
}

STriangle STriangle::Transform(Vector u, Vector v, Vector n) const {
    STriangle tr = *this;
    tr.a  = tr.a.ScaleOutOfCsys(u, v, n);
    tr.an = tr.an.ScaleOutOfCsys(u, v, n);
    tr.b  = tr.b.ScaleOutOfCsys(u, v, n);
    tr.bn = tr.bn.ScaleOutOfCsys(u, v, n);
    tr.c  = tr.c.ScaleOutOfCsys(u, v, n);
    tr.cn = tr.cn.ScaleOutOfCsys(u, v, n);
    return tr;
}

STriangle STriangle::From(STriMeta meta, Vector a, Vector b, Vector c) {
    STriangle tr = {};
    tr.meta = meta;
    tr.a = a;
    tr.b = b;
    tr.c = c;
    return tr;
}

SEdge SEdge::From(Vector a, Vector b) {
    SEdge se = {};
    se.a = a;
    se.b = b;
    return se;
}

bool SEdge::EdgeCrosses(Vector ea, Vector eb, Vector *ppi, SPointList *spl) const {
    Vector d = eb.Minus(ea);
    double t_eps = LENGTH_EPS/d.Magnitude();

    double t, tthis;
    bool skew;
    Vector pi;
    bool inOrEdge0, inOrEdge1;

    Vector dthis = b.Minus(a);
    double tthis_eps = LENGTH_EPS/dthis.Magnitude();

    if((ea.Equals(a) && eb.Equals(b)) ||
       (eb.Equals(a) && ea.Equals(b)))
    {
        if(ppi) *ppi = a;
        if(spl) spl->Add(a);
        return true;
    }

    // Can't just test if distance between d and a equals distance between d and b;
    // they could be on opposite sides, since we don't have the sign.
    double m = sqrt(d.Magnitude()*dthis.Magnitude());
    if(sqrt(fabs(d.Dot(dthis))) > (m - LENGTH_EPS)) {
        // The edges are parallel.
        if(fabs(a.DistanceToLine(ea, d)) > LENGTH_EPS) {
            // and not coincident, so can't be intersecting
            return false;
        }
        // The edges are coincident. Make sure that neither endpoint lies
        // on the other
        bool inters = false;
        double t;
        t = a.Minus(ea).DivProjected(d);
        if(t > t_eps && t < (1 - t_eps)) inters = true;
        t = b.Minus(ea).DivProjected(d);
        if(t > t_eps && t < (1 - t_eps)) inters = true;
        t = ea.Minus(a).DivProjected(dthis);
        if(t > tthis_eps && t < (1 - tthis_eps)) inters = true;
        t = eb.Minus(a).DivProjected(dthis);
        if(t > tthis_eps && t < (1 - tthis_eps)) inters = true;

        if(inters) {
            if(ppi) *ppi = a;
            if(spl) spl->Add(a);
            return true;
        } else {
            // So coincident but disjoint, okay.
            return false;
        }
    }

    // Lines are not parallel, so look for an intersection.
    pi = Vector::AtIntersectionOfLines(ea, eb, a, b,
                                       &skew,
                                       &t, &tthis);
    if(skew) return false;

    inOrEdge0 = (t     > -t_eps)     && (t     < (1 + t_eps));
    inOrEdge1 = (tthis > -tthis_eps) && (tthis < (1 + tthis_eps));

    if(inOrEdge0 && inOrEdge1) {
        if(a.Equals(ea) || b.Equals(ea) ||
           a.Equals(eb) || b.Equals(eb))
        {
            // Not an intersection if we share an endpoint with an edge
            return false;
        }
        // But it's an intersection if a vertex of one edge lies on the
        // inside of the other (or if they cross away from either's
        // vertex).
        if(ppi) *ppi = pi;
        if(spl) spl->Add(pi);
        return true;
    }
    return false;
}

void SEdgeList::Clear() {
    l.Clear();
}

void SEdgeList::AddEdge(Vector a, Vector b, int auxA, int auxB, int tag) {
    SEdge e = {};
    e.tag = tag;
    e.a = a;
    e.b = b;
    e.auxA = auxA;
    e.auxB = auxB;
    l.Add(&e);
}

bool SEdgeList::AssembleContour(Vector first, Vector last, SContour *dest,
                                SEdge *errorAt, bool keepDir, int start) const
{
    int i;

    dest->AddPoint(first);
    dest->AddPoint(last);

    do {
        for(i = start; i < l.n; i++) {
            /// @todo fix const!
            SEdge *se = const_cast<SEdge*>(&(l[i]));
            if(se->tag) continue;

            if(se->a.Equals(last)) {
                dest->AddPoint(se->b);
                last = se->b;
                se->tag = 1;
                break;
            }
            // Don't allow backwards edges if keepDir is true.
            if(!keepDir && se->b.Equals(last)) {
                dest->AddPoint(se->a);
                last = se->a;
                se->tag = 1;
                break;
            }
        }
        if(i >= l.n) {
            // Couldn't assemble a closed contour; mark where.
            if(errorAt) {
                errorAt->a = first;
                errorAt->b = last;
            }
            return false;
        }

    } while(!last.Equals(first));

    return true;
}

bool SEdgeList::AssemblePolygon(SPolygon *dest, SEdge *errorAt, bool keepDir) const {
    dest->Clear();

    bool allClosed = true;
    Vector first = Vector::From(0, 0, 0);
    Vector last  = Vector::From(0, 0, 0);
    int i;
    for(i = 0; i < l.n; i++) {
        if(!l[i].tag) {
            first = l[i].a;
            last = l[i].b;
            /// @todo fix const!
            const_cast<SEdge*>(&(l[i]))->tag = 1;
            // Create a new empty contour in our polygon, and finish assembling
            // into that contour.
            dest->AddEmptyContour();
            if(!AssembleContour(first, last, dest->l.Last(), errorAt, keepDir, i+1)) {
                allClosed = false;
            }
            // But continue assembling, even if some of the contours are open
        }
    }
    return allClosed;
}

//-----------------------------------------------------------------------------
// Test if the specified edge crosses any of the edges in our list. Two edges
// are not considered to cross if they share an endpoint (within LENGTH_EPS),
// but they are considered to cross if they are coincident and overlapping.
// If pi is not NULL, then a crossing is returned in that.
//-----------------------------------------------------------------------------
int SEdgeList::AnyEdgeCrossings(Vector a, Vector b, Vector *ppi, SPointList *spl) const {
    auto cnt = std::count_if(l.begin(), l.end(),
                             [&](SEdge const &se) { return se.EdgeCrosses(a, b, ppi, spl); });
    return static_cast<int>(cnt);
}

//-----------------------------------------------------------------------------
// Returns true if the intersecting edge list contains an edge that shares
// an endpoint with one of our edges.
//-----------------------------------------------------------------------------
bool SEdgeList::ContainsEdgeFrom(const SEdgeList *sel) const {
    auto i = FindIndexIf(l, [&](const SEdge &se) { return sel->ContainsEdge(&se); });
    // did we find one?
    return i >= 0;
}
bool SEdgeList::ContainsEdge(const SEdge *set) const {
    auto i = FindIndexIf(l, [&](const SEdge &se) {
        return ((se.a).Equals(set->a) && (se.b).Equals(set->b)) ||
               ((se.b).Equals(set->a) && (se.a).Equals(set->b));
    });
    // did we find one?
    return i >= 0;
}

//-----------------------------------------------------------------------------
// Remove unnecessary edges:
// - if two are anti-parallel then
//     if both=true, remove both
//     else remove only one.
// - if two are parallel then remove one.
//-----------------------------------------------------------------------------
void SEdgeList::CullExtraneousEdges(bool both) {
    l.ClearTags();
    for(int i = 0; i < l.n; i++) {
        SEdge *se = &(l[i]);
        for(int j = i + 1; j < l.n; j++) {
            SEdge *set = &(l[j]);
            if((set->a).Equals(se->a) && (set->b).Equals(se->b)) {
                // Two parallel edges exist; so keep only the first one.
                set->tag = 1;
            }
            if((set->a).Equals(se->b) && (set->b).Equals(se->a)) {
                // Two anti-parallel edges exist; if both=true, keep neither,
                // otherwise keep only one.
                if (both) se->tag = 1;
                set->tag = 1;
            }
        }
    }
    l.RemoveTagged();
}

//-----------------------------------------------------------------------------
// Make a kd-tree of edges. This is used for O(log(n)) implementations of stuff
// that would naively be O(n).
//-----------------------------------------------------------------------------
SKdNodeEdges *SKdNodeEdges::Alloc() {
    SKdNodeEdges *ne = (SKdNodeEdges *)AllocTemporary(sizeof(SKdNodeEdges));
    *ne = {};
    return ne;
}
SEdgeLl *SEdgeLl::Alloc() {
    SEdgeLl *sell = (SEdgeLl *)AllocTemporary(sizeof(SEdgeLl));
    *sell = {};
    return sell;
}
SKdNodeEdges *SKdNodeEdges::From(SEdgeList *sel) {
    SEdgeLl *sell = NULL;
    SEdge *se;
    for(se = sel->l.First(); se; se = sel->l.NextAfter(se)) {
        SEdgeLl *n = SEdgeLl::Alloc();
        n->se = se;
        n->next = sell;
        sell = n;
    }
    return SKdNodeEdges::From(sell);
}
SKdNodeEdges *SKdNodeEdges::From(SEdgeLl *sell) {
    SKdNodeEdges *n = SKdNodeEdges::Alloc();

    // Compute the midpoints (just mean, though median would be better) of
    // each component.
    Vector ptAve = Vector::From(0, 0, 0);
    SEdgeLl *flip;
    int totaln = 0;
    for(flip = sell; flip; flip = flip->next) {
        ptAve = ptAve.Plus(flip->se->a);
        ptAve = ptAve.Plus(flip->se->b);
        totaln++;
    }
    ptAve = ptAve.ScaledBy(1.0 / (2*totaln));

    // For each component, see how it splits.
    int ltln[3] = { 0, 0, 0 }, gtln[3] = { 0, 0, 0 };
    double badness[3];
    for(flip = sell; flip; flip = flip->next) {
        for(int i = 0; i < 3; i++) {
            if(flip->se->a.Element(i) < ptAve.Element(i) + KDTREE_EPS ||
               flip->se->b.Element(i) < ptAve.Element(i) + KDTREE_EPS)
            {
                ltln[i]++;
            }
            if(flip->se->a.Element(i) > ptAve.Element(i) - KDTREE_EPS ||
               flip->se->b.Element(i) > ptAve.Element(i) - KDTREE_EPS)
            {
                gtln[i]++;
            }
        }
    }
    for(int i = 0; i < 3; i++) {
        badness[i] = pow((double)ltln[i], 4) + pow((double)gtln[i], 4);
    }

    // Choose the least bad coordinate to split along.
    if(badness[0] < badness[1] && badness[0] < badness[2]) {
        n->which = 0;
    } else if(badness[1] < badness[2]) {
        n->which = 1;
    } else {
        n->which = 2;
    }
    n->c = ptAve.Element(n->which);

    if(totaln < 3 || totaln == gtln[n->which] || totaln == ltln[n->which]) {
        n->edges = sell;
        // and we're a leaf node
        return n;
    }

    // Sort the edges according to which side(s) of the split plane they're on.
    SEdgeLl *gtl = NULL, *ltl = NULL;
    for(flip = sell; flip; flip = flip->next) {
        if(flip->se->a.Element(n->which) < n->c + KDTREE_EPS ||
           flip->se->b.Element(n->which) < n->c + KDTREE_EPS)
        {
            SEdgeLl *selln = SEdgeLl::Alloc();
            selln->se = flip->se;
            selln->next = ltl;
            ltl = selln;
        }
        if(flip->se->a.Element(n->which) > n->c - KDTREE_EPS ||
           flip->se->b.Element(n->which) > n->c - KDTREE_EPS)
        {
            SEdgeLl *selln = SEdgeLl::Alloc();
            selln->se = flip->se;
            selln->next = gtl;
            gtl = selln;
        }
    }

    n->lt = SKdNodeEdges::From(ltl);
    n->gt = SKdNodeEdges::From(gtl);
    return n;
}

int SKdNodeEdges::AnyEdgeCrossings(Vector a, Vector b, int cnt,
        Vector *pi, SPointList *spl) const
{
    int inters = 0;
    if(gt && lt) {
        if(a.Element(which) < c + KDTREE_EPS ||
           b.Element(which) < c + KDTREE_EPS)
        {
            inters += lt->AnyEdgeCrossings(a, b, cnt, pi, spl);
        }
        if(a.Element(which) > c - KDTREE_EPS ||
           b.Element(which) > c - KDTREE_EPS)
        {
            inters += gt->AnyEdgeCrossings(a, b, cnt, pi, spl);
        }
    } else {
        SEdgeLl *sell;
        for(sell = edges; sell; sell = sell->next) {
            SEdge *se = sell->se;
            if(se->tag == cnt) continue;
            if(se->EdgeCrosses(a, b, pi, spl)) {
                inters++;
            }
            se->tag = cnt;
        }
    }
    return inters;
}

//-----------------------------------------------------------------------------
// We have an edge list that contains only collinear edges, maybe with more
// splits than necessary. Merge any collinear segments that join.
//-----------------------------------------------------------------------------
void SEdgeList::MergeCollinearSegments(Vector a, Vector b) {
    const Vector lineStart = a;
    const Vector lineDirection = b.Minus(a);
    std::sort(l.begin(), l.end(), [&](const SEdge &a, const SEdge &b) {
        double ta = (a.a.Minus(lineStart)).DivProjected(lineDirection);
        double tb = (b.a.Minus(lineStart)).DivProjected(lineDirection);

        return (ta < tb);
    });

    l.ClearTags();
    SEdge *prev = nullptr;
    for(auto &now : l) {
        if(prev != nullptr) {
            if((prev->b).Equals(now.a) && prev->auxA == now.auxA) {
                // The previous segment joins up to us; so merge it into us.
                prev->tag = 1;
                now.a     = prev->a;
            }
        }
        prev = &now;
    }
    l.RemoveTagged();
}

void SPointList::Clear() {
    l.Clear();
}

bool SPointList::ContainsPoint(Vector pt) const {
    return (IndexForPoint(pt) >= 0);
}

int SPointList::IndexForPoint(Vector pt) const {
    return FindIndexIf(l, [&](SPoint const &p) { return pt.Equals(p.p); });
}

void SPointList::IncrementTagFor(Vector pt) {
    int i = IndexForPoint(pt);
    if(i < 0) {
        SPoint pa;
        pa.p   = pt;
        pa.tag = 1;
        l.Add(&pa);
    } else {
        l[i].tag++;
    }
}

void SPointList::Add(Vector pt) {
    SPoint p = {};
    p.p = pt;
    l.Add(&p);
}

void SContour::AddPoint(Vector p) {
    SPoint sp;
    sp.tag = 0;
    sp.p = p;

    l.Add(&sp);
}

void SContour::MakeEdgesInto(SEdgeList *el) const {
    int i;
    for(i = 0; i < (l.n - 1); i++) {
        el->AddEdge(l[i].p, l[i+1].p);
    }
}

void SContour::CopyInto(SContour *dest) const {
    for(const SPoint *sp = l.First(); sp; sp = l.NextAfter(sp)) {
        dest->AddPoint(sp->p);
    }
}

void SContour::FindPointWithMinX() {
    xminPt = Vector::From(1e10, 1e10, 1e10);
    for(const SPoint *sp = l.First(); sp; sp = l.NextAfter(sp)) {
        if(sp->p.x < xminPt.x) {
            xminPt = sp->p;
        }
    }
}

Vector SContour::ComputeNormal() const {
    Vector n = Vector::From(0, 0, 0);

    for(int i = 0; i < l.n - 2; i++) {
        Vector u = (l[i+1].p).Minus(l[i+0].p).WithMagnitude(1);
        Vector v = (l[i+2].p).Minus(l[i+1].p).WithMagnitude(1);
        Vector nt = u.Cross(v);
        if(nt.Magnitude() > n.Magnitude()) {
            n = nt;
        }
    }
    return n.WithMagnitude(1);
}

Vector SContour::AnyEdgeMidpoint() const {
    ssassert(l.n >= 2, "Need two points to find a midpoint");
    return ((l[0].p).Plus(l[1].p)).ScaledBy(0.5);
}

bool SContour::IsClockwiseProjdToNormal(Vector n) const {
    // Degenerate things might happen as we draw; doesn't really matter
    // what we do then.
    if(n.Magnitude() < 0.01) return true;

    return (SignedAreaProjdToNormal(n) < 0);
}

double SContour::SignedAreaProjdToNormal(Vector n) const {
    // An arbitrary 2d coordinate system that has n as its normal
    Vector u = n.Normal(0);
    Vector v = n.Normal(1);

    double area = 0;
    for(int i = 0; i < (l.n - 1); i++) {
        double u0 = (l[i  ].p).Dot(u);
        double v0 = (l[i  ].p).Dot(v);
        double u1 = (l[i+1].p).Dot(u);
        double v1 = (l[i+1].p).Dot(v);

        area += ((v0 + v1)/2)*(u1 - u0);
    }
    return area;
}

bool SContour::ContainsPointProjdToNormal(Vector n, Vector p) const {
    Vector u = n.Normal(0);
    Vector v = n.Normal(1);

    double up = p.Dot(u);
    double vp = p.Dot(v);

    bool inside = false;
    for(int i = 0; i < (l.n - 1); i++) {
        double ua = (l[i  ].p).Dot(u);
        double va = (l[i  ].p).Dot(v);
        // The curve needs to be exactly closed; approximation is death.
        double ub = (l[(i+1)%(l.n-1)].p).Dot(u);
        double vb = (l[(i+1)%(l.n-1)].p).Dot(v);

        if ((((va <= vp) && (vp < vb)) ||
             ((vb <= vp) && (vp < va))) &&
            (up < (ub - ua) * (vp - va) / (vb - va) + ua))
        {
          inside = !inside;
        }
    }

    return inside;
}

void SContour::Reverse() {
    l.Reverse();
}


void SPolygon::Clear() {
    int i;
    for(i = 0; i < l.n; i++) {
        (l[i]).l.Clear();
    }
    l.Clear();
}

void SPolygon::AddEmptyContour() {
    SContour c = {};
    l.Add(&c);
}

void SPolygon::MakeEdgesInto(SEdgeList *el) const {
    int i;
    for(i = 0; i < l.n; i++) {
        (l[i]).MakeEdgesInto(el);
    }
}

Vector SPolygon::ComputeNormal() const {
    if(l.IsEmpty())
        return Vector::From(0, 0, 0);
    return (l[0]).ComputeNormal();
}

double SPolygon::SignedArea() const {
    double area = 0;
    // This returns the true area only if the contours are all oriented
    // correctly, with the holes backwards from the outer contours.
    for(const SContour *sc = l.First(); sc; sc = l.NextAfter(sc)) {
        area += sc->SignedAreaProjdToNormal(normal);
    }
    return area;
}

bool SPolygon::ContainsPoint(Vector p) const {
    return (WindingNumberForPoint(p) % 2) == 1;
}

size_t SPolygon::WindingNumberForPoint(Vector p) const {
    auto winding = std::count_if(l.begin(), l.end(), [&](const SContour &sc) {
        return sc.ContainsPointProjdToNormal(normal, p);
    });
    return winding;
}

void SPolygon::FixContourDirections() {
    // At output, the contour's tag will be 1 if we reversed it, else 0.
    l.ClearTags();

    // Outside curve looks counterclockwise, projected against our normal.
    int i, j;
    for(i = 0; i < l.n; i++) {
        SContour *sc = &(l[i]);
        if(sc->l.n < 2) continue;
        // The contours may not intersect, but they may share vertices; so
        // testing a vertex for point-in-polygon may fail, but the midpoint
        // of an edge is okay.
        Vector pt = (((sc->l[0]).p).Plus(sc->l[1].p)).ScaledBy(0.5);

        sc->timesEnclosed = 0;
        bool outer = true;
        for(j = 0; j < l.n; j++) {
            if(i == j) continue;
            SContour *sct = &(l[j]);
            if(sct->ContainsPointProjdToNormal(normal, pt)) {
                outer = !outer;
                (sc->timesEnclosed)++;
            }
        }

        bool clockwise = sc->IsClockwiseProjdToNormal(normal);
        if((clockwise && outer) || (!clockwise && !outer)) {
            sc->Reverse();
            sc->tag = 1;
        }
    }
}

bool SPolygon::IsEmpty() const {
    if(l.IsEmpty() || l[0].l.IsEmpty())
        return true;
    return false;
}

Vector SPolygon::AnyPoint() const {
    ssassert(!IsEmpty(), "Need at least one point");
    return l[0].l[0].p;
}

bool SPolygon::SelfIntersecting(Vector *intersectsAt) const {
    SEdgeList el = {};
    MakeEdgesInto(&el);
    SKdNodeEdges *kdtree = SKdNodeEdges::From(&el);

    int cnt = 1;
    el.l.ClearTags();

    bool ret = false;
    SEdge *se;
    for(se = el.l.First(); se; se = el.l.NextAfter(se)) {
        int inters = kdtree->AnyEdgeCrossings(se->a, se->b, cnt, intersectsAt);
        if(inters != 1) {
            ret = true;
            break;
        }
        cnt++;
    }

    el.Clear();
    return ret;
}

void SPolygon::InverseTransformInto(SPolygon *sp, Vector u, Vector v, Vector n) const {
    for(const SContour &sc : l) {
        SContour tsc = {};
        tsc.timesEnclosed = sc.timesEnclosed;
        for(const SPoint &sp : sc.l) {
            tsc.AddPoint(sp.p.DotInToCsys(u, v, n));
        }
        sp->l.Add(&tsc);
    }
}

//-----------------------------------------------------------------------------
// Low-quality routines to cutter radius compensate a polygon. Assumes the
// polygon is in the xy plane, and the contours all go in the right direction
// with respect to normal (0, 0, -1).
//-----------------------------------------------------------------------------
void SPolygon::OffsetInto(SPolygon *dest, double r) const {
    int i;
    dest->Clear();
    for(i = 0; i < l.n; i++) {
        const SContour *sc = &(l[i]);
        dest->AddEmptyContour();
        sc->OffsetInto(&(dest->l[dest->l.n-1]), r);
    }
}
//-----------------------------------------------------------------------------
// Calculate the intersection point of two lines. Returns true for success,
// false if they're parallel.
//-----------------------------------------------------------------------------
static bool IntersectionOfLines(double x0A, double y0A, double dxA, double dyA,
                                double x0B, double y0B, double dxB, double dyB,
                                double *xi, double *yi)
{
    double A[2][2];
    double b[2];

    // The line is given to us in the form:
    //    (x(t), y(t)) = (x0, y0) + t*(dx, dy)
    // so first rewrite it as
    //    (x - x0, y - y0) dot (dy, -dx) = 0
    //    x*dy - x0*dy - y*dx + y0*dx = 0
    //    x*dy - y*dx = (x0*dy - y0*dx)

    // So write the matrix, pre-pivoted.
    if(fabs(dyA) > fabs(dyB)) {
        A[0][0] = dyA;  A[0][1] = -dxA;  b[0] = x0A*dyA - y0A*dxA;
        A[1][0] = dyB;  A[1][1] = -dxB;  b[1] = x0B*dyB - y0B*dxB;
    } else {
        A[1][0] = dyA;  A[1][1] = -dxA;  b[1] = x0A*dyA - y0A*dxA;
        A[0][0] = dyB;  A[0][1] = -dxB;  b[0] = x0B*dyB - y0B*dxB;
    }

    // Check the determinant; if it's zero then no solution.
    if(fabs(A[0][0]*A[1][1] - A[0][1]*A[1][0]) < LENGTH_EPS) {
        return false;
    }

    // Solve
    double v = A[1][0] / A[0][0];
    A[1][0] -= A[0][0]*v;
    A[1][1] -= A[0][1]*v;
    b[1] -= b[0]*v;

    // Back-substitute.
    *yi = b[1] / A[1][1];
    *xi = (b[0] - A[0][1]*(*yi)) / A[0][0];

    return true;
}
void SContour::OffsetInto(SContour *dest, double r) const {
    int i;

    for(i = 0; i < l.n; i++) {
        Vector a, b, c;
        Vector dp, dn;
        double thetan, thetap;

        a = l[WRAP(i-1, (l.n-1))].p;
        b = l[WRAP(i,   (l.n-1))].p;
        c = l[WRAP(i+1, (l.n-1))].p;

        dp = a.Minus(b);
        thetap = atan2(dp.y, dp.x);

        dn = b.Minus(c);
        thetan = atan2(dn.y, dn.x);

        // A short line segment in a badly-generated polygon might look
        // okay but screw up our sense of direction.
        if(dp.Magnitude() < LENGTH_EPS || dn.Magnitude() < LENGTH_EPS) {
            continue;
        }

        if(thetan > thetap && (thetan - thetap) > PI) {
            thetap += 2*PI;
        }
        if(thetan < thetap && (thetap - thetan) > PI) {
            thetan += 2*PI;
        }

        if(fabs(thetan - thetap) < (1*PI)/180) {
            Vector p = { b.x - r*sin(thetap), b.y + r*cos(thetap), 0 };
            dest->AddPoint(p);
        } else if(thetan < thetap) {
            // This is an inside corner. We have two edges, Ep and En. Move
            // out from their intersection by radius, normal to En, and
            // then draw a line parallel to En. Move out from their
            // intersection by radius, normal to Ep, and then draw a second
            // line parallel to Ep. The point that we want to generate is
            // the intersection of these two lines--it removes as much
            // material as we can without removing any that we shouldn't.
            double px0, py0, pdx, pdy;
            double nx0, ny0, ndx, ndy;
            double x = 0.0, y = 0.0;

            px0 = b.x - r*sin(thetap);
            py0 = b.y + r*cos(thetap);
            pdx = cos(thetap);
            pdy = sin(thetap);

            nx0 = b.x - r*sin(thetan);
            ny0 = b.y + r*cos(thetan);
            ndx = cos(thetan);
            ndy = sin(thetan);

            IntersectionOfLines(px0, py0, pdx, pdy,
                                nx0, ny0, ndx, ndy,
                                &x, &y);

            dest->AddPoint(Vector::From(x, y, 0));
        } else {
            if(fabs(thetap - thetan) < (6*PI)/180) {
                Vector pp = { b.x - r*sin(thetap),
                              b.y + r*cos(thetap), 0 };
                dest->AddPoint(pp);

                Vector pn = { b.x - r*sin(thetan),
                              b.y + r*cos(thetan), 0 };
                dest->AddPoint(pn);
            } else {
                double theta;
                for(theta = thetap; theta <= thetan; theta += (6*PI)/180) {
                    Vector p = { b.x - r*sin(theta),
                                 b.y + r*cos(theta), 0 };
                    dest->AddPoint(p);
                }
            }
        }
    }
}

