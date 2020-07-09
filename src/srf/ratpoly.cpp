//-----------------------------------------------------------------------------
// Math on rational polynomial surfaces and curves, typically in Bezier
// form. Evaluate, root-find (by Newton's methods), evaluate derivatives,
// and so on.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "../solvespace.h"
#include "../utileigen.h"

// Converge it to better than LENGTH_EPS; we want two points, each
// independently projected into uv and back, to end up equal with the
// LENGTH_EPS. Best case that requires LENGTH_EPS/2, but more is better
// and convergence should be fast by now.
#define RATPOLY_EPS (LENGTH_EPS/(1e2))

static double Bernstein(int k, int deg, double t)
{
// indexed by [degree][k][exponent]
    static const double bernstein_coeff[4][4][4] = {
    { { 1.0,0.0,0.0,0.0 },  { 1.0,0.0,0.0,0.0 }, { 1.0,0.0,0.0,0.0 }, { 1.0,0.0,0.0,0.0 } },
    { { 1.0,-1.0,0.0,0.0 }, { 0.0,1.0,0.0,0.0 }, { 0.0,0.0,0.0,0.0 }, { 0.0,0.0,0.0,0.0 } },
    { { 1.0,-2.0,1.0,0.0 }, { 0.0,2.0,-2.0,0.0 },{ 0.0,0.0,1.0,0.0 }, { 0.0,0.0,0.0,0.0 } },
    { { 1.0,-3.0,3.0,-1.0 },{ 0.0,3.0,-6.0,3.0 },{ 0.0,0.0,3.0,-3.0}, { 0.0,0.0,0.0,1.0 } } };

    const double *c;
    c = bernstein_coeff[deg][k];
    return (((c[3]*t+c[2])*t)+c[1])*t+c[0];
}

static double BernsteinDerivative(int k, int deg, double t)
{
    static const double bernstein_derivative_coeff[4][4][3] = {
    { { 0.0,0.0,0.0 },  { 0.0,0.0,0.0 }, { 0.0,0.0,0.0 }, { 0.0,0.0,0.0 } },
    { { -1.0,0.0,0.0 }, { 1.0,0.0,0.0 }, { 0.0,0.0,0.0 }, { 0.0,0.0,0.0 } },
    { { -2.0,2.0,0.0 }, { 2.0,-4.0,0.0 },{ 0.0,2.0,0.0 }, { 0.0,0.0,0.0 } },
    { { -3.0,6.0,-3.0 },{ 3.0,-12.0,9.0 },{ 0.0,6.0,-9.0}, { 0.0,0.0,3.0 } } };

    const double *c;
    c = bernstein_derivative_coeff[deg][k];
    return ((c[2]*t)+c[1])*t+c[0];
}

Vector SBezier::PointAt(double t) const {
    Vector pt = Vector::From(0, 0, 0);
    double d = 0;

    int i;
    for(i = 0; i <= deg; i++) {
        double B = Bernstein(i, deg, t);
        pt = pt.Plus(ctrl[i].ScaledBy(B*weight[i]));
        d += weight[i]*B;
    }
    pt = pt.ScaledBy(1.0/d);
    return pt;
}

Vector SBezier::TangentAt(double t) const {
    Vector pt = Vector::From(0, 0, 0), pt_p = Vector::From(0, 0, 0);
    double d = 0, d_p = 0;

    int i;
    for(i = 0; i <= deg; i++) {
        double B  = Bernstein(i, deg, t),
               Bp = BernsteinDerivative(i, deg, t);

        pt = pt.Plus(ctrl[i].ScaledBy(B*weight[i]));
        d += weight[i]*B;

        pt_p = pt_p.Plus(ctrl[i].ScaledBy(Bp*weight[i]));
        d_p += weight[i]*Bp;
    }

    // quotient rule; f(t) = n(t)/d(t), so f' = (n'*d - n*d')/(d^2)
    Vector ret;
    ret = (pt_p.ScaledBy(d)).Minus(pt.ScaledBy(d_p));
    ret = ret.ScaledBy(1.0/(d*d));
    return ret;
}

void SBezier::ClosestPointTo(Vector p, double *t, bool mustConverge) const {
    int i;
    double minDist = VERY_POSITIVE;
    *t = 0;
    double res = (deg <= 2) ? 7.0 : 20.0;
    for(i = 0; i < (int)res; i++) {
        double tryt = (i/res);

        Vector tryp = PointAt(tryt);
        double d = (tryp.Minus(p)).Magnitude();
        if(d < minDist) {
            *t = tryt;
            minDist = d;
        }
    }

    Vector p0;
    for(i = 0; i < (mustConverge ? 15 : 5); i++) {
        p0 = PointAt(*t);
        if(p0.Equals(p, RATPOLY_EPS)) {
            return;
        }

        Vector dp = TangentAt(*t);
        Vector pc = p.ClosestPointOnLine(p0, dp);
        *t += (pc.Minus(p0)).DivProjected(dp);
    }
    if(mustConverge) {
        dbp("didn't converge (closest point on bezier curve)");
    }
}

bool SBezier::PointOnThisAndCurve(const SBezier *sbb, Vector *p) const {
    double ta, tb;
    this->ClosestPointTo(*p, &ta, /*mustConverge=*/false);
    sbb ->ClosestPointTo(*p, &tb, /*mustConverge=*/false);

    int i;
    for(i = 0; i < 20; i++) {
        Vector pa = this->PointAt(ta),
               pb = sbb ->PointAt(tb),
               da = this->TangentAt(ta),
               db = sbb ->TangentAt(tb);

        if(pa.Equals(pb, RATPOLY_EPS)) {
            *p = pa;
            return true;
        }

        double tta, ttb;
        Vector::ClosestPointBetweenLines(pa, da, pb, db, &tta, &ttb);
        ta += tta;
        tb += ttb;
    }
    return false;
}

void SBezier::SplitAt(double t, SBezier *bef, SBezier *aft) const {
    Vector4 ct[4];
    int i;
    for(i = 0; i <= deg; i++) {
        ct[i] = Vector4::From(weight[i], ctrl[i]);
    }

    switch(deg) {
        case 1: {
            Vector4 cts = Vector4::Blend(ct[0], ct[1], t);
            *bef = SBezier::From(ct[0], cts);
            *aft = SBezier::From(cts, ct[1]);
            break;
        }
        case 2: {
            Vector4 ct01 = Vector4::Blend(ct[0], ct[1], t),
                    ct12 = Vector4::Blend(ct[1], ct[2], t),
                    cts  = Vector4::Blend(ct01,  ct12,  t);

            *bef = SBezier::From(ct[0], ct01, cts);
            *aft = SBezier::From(cts, ct12, ct[2]);
            break;
        }
        case 3: {
            Vector4 ct01    = Vector4::Blend(ct[0], ct[1], t),
                    ct12    = Vector4::Blend(ct[1], ct[2], t),
                    ct23    = Vector4::Blend(ct[2], ct[3], t),
                    ct01_12 = Vector4::Blend(ct01,  ct12,  t),
                    ct12_23 = Vector4::Blend(ct12,  ct23,  t),
                    cts     = Vector4::Blend(ct01_12, ct12_23, t);

            *bef = SBezier::From(ct[0], ct01, ct01_12, cts);
            *aft = SBezier::From(cts, ct12_23, ct23, ct[3]);
            break;
        }
        default: ssassert(false, "Unexpected degree of spline");
    }
}

void SBezier::MakePwlInto(SEdgeList *sel, double chordTol) const {
    List<Vector> lv = {};
    MakePwlInto(&lv, chordTol);
    int i;
    for(i = 1; i < lv.n; i++) {
        sel->AddEdge(lv[i-1], lv[i]);
    }
    lv.Clear();
}
void SBezier::MakePwlInto(List<SCurvePt> *l, double chordTol) const {
    List<Vector> lv = {};
    MakePwlInto(&lv, chordTol);
    int i;
    for(i = 0; i < lv.n; i++) {
        SCurvePt scpt;
        scpt.tag    = 0;
        scpt.p      = lv[i];
        scpt.vertex = (i == 0) || (i == (lv.n - 1));
        l->Add(&scpt);
    }
    lv.Clear();
}
void SBezier::MakePwlInto(SContour *sc, double chordTol) const {
    List<Vector> lv = {};
    MakePwlInto(&lv, chordTol);
    int i;
    for(i = 0; i < lv.n; i++) {
        sc->AddPoint(lv[i]);
    }
    lv.Clear();
}
void SBezier::MakePwlInto(List<Vector> *l, double chordTol) const {
    if(EXACT(chordTol == 0)) {
        // Use the default chord tolerance.
        chordTol = SS.ChordTolMm();
    }
    l->Add(&(ctrl[0]));
    if(deg == 1) {
        l->Add(&(ctrl[1]));
    } else {
        // Never do fewer than one intermediate point; people seem to get
        // unhappy when their circles turn into squares, but maybe less
        // unhappy with octagons.
        MakePwlInitialWorker(l, 0.0, 0.5, chordTol);
        MakePwlInitialWorker(l, 0.5, 1.0, chordTol);
    }
}
void SBezier::MakePwlWorker(List<Vector> *l, double ta, double tb, double chordTol) const
{
    Vector pa = PointAt(ta);
    Vector pb = PointAt(tb);

    Vector pm = PointAt((ta + tb) / 2.0);
    double d = pm.DistanceToLine(pa, pb.Minus(pa));

    double step = 1.0/SS.GetMaxSegments();
    if((tb - ta) < step || d < chordTol) {
        // A previous call has already added the beginning of our interval.
        l->Add(&pb);
    } else {
        double tm = (ta + tb) / 2;
        MakePwlWorker(l, ta, tm, chordTol);
        MakePwlWorker(l, tm, tb, chordTol);
    }
}
void SBezier::MakePwlInitialWorker(List<Vector> *l, double ta, double tb, double chordTol) const
{
    Vector pa = PointAt(ta);
    Vector pb = PointAt(tb);

    double tm1 = ta + (tb - ta) * 0.25;
    double tm2 = ta + (tb - ta) * 0.5;
    double tm3 = ta + (tb - ta) * 0.75;

    Vector pm1 = PointAt(tm1);
    Vector pm2 = PointAt(tm2);
    Vector pm3 = PointAt(tm3);
    Vector dir = pb.Minus(pa);

    double d = max({
                   pm1.DistanceToLine(pa, dir),
                   pm2.DistanceToLine(pa, dir),
                   pm3.DistanceToLine(pa, dir)
                });

    double step = 1.0/SS.GetMaxSegments();
    if( ((tb - ta) < step || d < chordTol) && ((tb-ta) < 0.2) ) {
        // A previous call has already added the beginning of our interval.
        l->Add(&pb);
    } else {
        double tm = (ta + tb) / 2;
        MakePwlWorker(l, ta, tm, chordTol);
        MakePwlWorker(l, tm, tb, chordTol);
    }
}

void SBezier::MakeNonrationalCubicInto(SBezierList *bl, double tolerance, int depth) const {
    Vector t0 = TangentAt(0), t1 = TangentAt(1);
    // The curve is correct, and the first derivatives are correct, at the
    // endpoints.
    SBezier bnr = SBezier::From(
                        Start(),
                        Start().Plus(t0.ScaledBy(1.0/3)),
                        Finish().Minus(t1.ScaledBy(1.0/3)),
                        Finish());

    bool closeEnough = true;
    int i;
    for(i = 1; i <= 3; i++) {
        double t = i/4.0;
        Vector p0 = PointAt(t),
               pn = bnr.PointAt(t);
        double d = (p0.Minus(pn)).Magnitude();
        if(d > tolerance) {
            closeEnough = false;
        }
    }

    if(closeEnough || depth > 3) {
        bl->l.Add(this);
    } else {
        SBezier bef, aft;
        SplitAt(0.5, &bef, &aft);
        bef.MakeNonrationalCubicInto(bl, tolerance, depth+1);
        aft.MakeNonrationalCubicInto(bl, tolerance, depth+1);
    }
}

Vector SSurface::PointAt(Point2d puv) const {
    return PointAt(puv.x, puv.y);
}
Vector SSurface::PointAt(double u, double v) const {
    Vector num = Vector::From(0, 0, 0);
    double den = 0;

    int i, j;
    for(i = 0; i <= degm; i++) {
        for(j = 0; j <= degn; j++) {
            double Bi = Bernstein(i, degm, u),
                   Bj = Bernstein(j, degn, v);

            num = num.Plus(ctrl[i][j].ScaledBy(Bi*Bj*weight[i][j]));
            den += weight[i][j]*Bi*Bj;
        }
    }
    num = num.ScaledBy(1.0/den);
    return num;
}

void SSurface::TangentsAt(double u, double v, Vector *tu, Vector *tv) const {
    EiVector num = EiVector::Zero(),
             num_u = EiVector::Zero(),
             num_v = EiVector::Zero();
    double den   = 0,
           den_u = 0,
           den_v = 0;

    int i, j;
    for(i = 0; i <= degm; i++) {
        for(j = 0; j <= degn; j++) {
            double Bi  = Bernstein(i, degm, u),
                   Bj  = Bernstein(j, degn, v),
                   Bip = BernsteinDerivative(i, degm, u),
                   Bjp = BernsteinDerivative(j, degn, v);
            EiVector ctrlij = to_eigen(ctrl[i][j]);
            num += ctrlij * (Bi*Bj*weight[i][j]);
            den += weight[i][j]*Bi*Bj;

            num_u += ctrlij * (Bip*Bj*weight[i][j]);
            den_u += weight[i][j]*Bip*Bj;

            num_v += ctrlij * (Bi*Bjp*weight[i][j]);
            den_v += weight[i][j]*Bi*Bjp;
        }
    }
    // quotient rule; f(t) = n(t)/d(t), so f' = (n'*d - n*d')/(d^2)
    *tu = from_eigen(((num_u * den) - (num * den_u))/(den * den));

    *tv = from_eigen(((num_v * den) - (num * den_v))/(den * den));
}

Vector SSurface::NormalAt(Point2d puv) const {
    return NormalAt(puv.x, puv.y);
}

Vector SSurface::NormalAt(double u, double v) const {
    Vector tu, tv;
    TangentsAt(u, v, &tu, &tv);
    return tu.Cross(tv);
}

void SSurface::ClosestPointTo(Vector p, Point2d *puv, bool mustConverge) {
    ClosestPointTo(p, &(puv->x), &(puv->y), mustConverge);
}

void SSurface::ClosestPointTo(Vector p, double *u, double *v, bool mustConverge) {
    // A few special cases first; when control points are coincident the
    // derivative goes to zero at the control points, and would result in
    // nonconvergence. We avoid that here, and also guarantee a consistent
    // (u, v) (of the infinitely many possible in one parameter).
    if(p.Equals(ctrl[0]   [0]   )) { *u = 0; *v = 0; return; }
    if(p.Equals(ctrl[degm][0]   )) { *u = 1; *v = 0; return; }
    if(p.Equals(ctrl[degm][degn])) { *u = 1; *v = 1; return; }
    if(p.Equals(ctrl[0]   [degn])) { *u = 0; *v = 1; return; }

    // And planes are trivial, so don't waste time iterating over those.
    if(degm == 1 && degn == 1) {
        Vector orig =  ctrl[0][0],
               bu   = (ctrl[1][0]).Minus(orig),
               bv   = (ctrl[0][1]).Minus(orig);
        if((ctrl[1][1]).Equals(orig.Plus(bu).Plus(bv))) {

            Vector n = bu.Cross(bv);
            Vector ty = n.Cross(bu).ScaledBy(1.0/bu.MagSquared());
            Vector tx = bv.Cross(n).ScaledBy(1.0/bv.MagSquared());

            Vector dp = p.Minus(orig);
            *u = dp.Dot(bu) / tx.MagSquared();
            *v = dp.Dot(bv) / ty.MagSquared();
            return;
        }
    }

    // Try whatever the previous guess was. This is likely to do something
    // good if we're working our way along a curve or something else where
    // we project successive points that are close to each other; something
    // like a 20% speedup empirically.
    if(mustConverge) {
        double ut = cached.x, vt = cached.y;
        if(ClosestPointNewton(p, &ut, &vt, mustConverge)) {
            cached.x = *u = ut;
            cached.y = *v = vt;
            return;
        }
    }

    // Search for a reasonable initial guess
    int i, j;
    double minDist = VERY_POSITIVE;
    int res = (max(degm, degn) == 2) ? 7 : 20;
    for(i = 0; i < res; i++) {
        for(j = 0; j < res; j++) {
            double tryu = (i + 0.5)/res, tryv = (j + 0.5)/res;

            Vector tryp = PointAt(tryu, tryv);
            double d = (tryp.Minus(p)).Magnitude();
            if(d < minDist) {
                *u = tryu;
                *v = tryv;
                minDist = d;
            }
        }
    }

    if(ClosestPointNewton(p, u, v, mustConverge)) {
        cached.x = *u;
        cached.y = *v;
        return;
    }

    // If we failed to converge, then at least don't return NaN.
    if(mustConverge) {
        Vector p0 = PointAt(*u, *v);
        dbp("didn't converge");
        dbp("have %.3f %.3f %.3f", CO(p0));
        dbp("want %.3f %.3f %.3f", CO(p));
        dbp("distance = %g", (p.Minus(p0)).Magnitude());
    }
    if(IsReasonable(*u) || IsReasonable(*v)) {
        *u = *v = 0;
    }
}

bool SSurface::ClosestPointNewton(Vector p, double *u, double *v, bool mustConverge) const
{
    // Initial guess is in u, v; refine by Newton iteration.
    Vector p0 = Vector::From(0, 0, 0);
    for(int i = 0; i < (mustConverge ? 25 : 5); i++) {
        p0 = PointAt(*u, *v);
        if(mustConverge) {
            if(p0.Equals(p, RATPOLY_EPS)) {
                return true;
            }
        }

        Vector tu, tv, tx, ty;
        TangentsAt(*u, *v, &tu, &tv);
        Vector n = tu.Cross(tv);
        // since tu and tv may not be orthogonal, use y in place of v.
        // |y| = |v|sin(theta) where theta is the angle between tu and tv.
        ty = n.Cross(tu).ScaledBy(1.0/tu.MagSquared());
        tx = tv.Cross(n).ScaledBy(1.0/tv.MagSquared());

        // Project the point into a plane through p0, with basis tu, tv; a
        // second-order thing would converge faster but needs second
        // derivatives.
        Vector dp = p.Minus(p0);
        double du = dp.Dot(tx),
               dv = dp.Dot(ty);
        *u += du / (tx.MagSquared());
        *v += dv / (ty.MagSquared());

        if (*u < 0.0) *u = 0.0;
        else if (*u > 1.0) *u = 1.0;
        if (*v < 0.0) *v = 0.0;
        else if (*v > 1.0) *v = 1.0;

    }

    return false;
}

bool SSurface::PointIntersectingLine(Vector p0, Vector p1, double *u, double *v) const
{
    int i;
    for(i = 0; i < 15; i++) {
        Vector pi, p, tu, tv;
        p = PointAt(*u, *v);
        TangentsAt(*u, *v, &tu, &tv);

        Vector n = (tu.Cross(tv)).WithMagnitude(1);
        double d = p.Dot(n);

        bool parallel;
        pi = Vector::AtIntersectionOfPlaneAndLine(n, d, p0, p1, &parallel);
        if(parallel) break;

        // Check for convergence
        if(pi.Equals(p, RATPOLY_EPS)) return true;

        n = tu.Cross(tv);
        Vector ty = n.Cross(tu).ScaledBy(1.0/tu.MagSquared());
        Vector tx = tv.Cross(n).ScaledBy(1.0/tv.MagSquared());

        // Adjust our guess and iterate
        Vector dp = pi.Minus(p);
        double du = dp.Dot(tx), dv = dp.Dot(ty);
        *u += du / tx.MagSquared();
        *v += dv / ty.MagSquared();
    }
    dbp("didn't converge (surface intersecting line)");
    return false;
}

Vector SSurface::ClosestPointOnThisAndSurface(SSurface *srf2, Vector p) {
    // This is untested.
    int i, j;
    Point2d puv[2];
    SSurface *srf[2] = { this, srf2 };

    for(j = 0; j < 2; j++) {
        (srf[j])->ClosestPointTo(p, &(puv[j]), /*mustConverge=*/false);
    }

    for(i = 0; i < 10; i++) {
        Vector tu[2], tv[2], cp[2], n[2];
        double d[2];

        for(j = 0; j < 2; j++) {
            (srf[j])->TangentsAt(puv[j].x, puv[j].y, &(tu[j]), &(tv[j]));

            cp[j] = (srf[j])->PointAt(puv[j]);

            n[j] = ((tu[j]).Cross(tv[j])).WithMagnitude(1);
            d[j] = (n[j]).Dot(cp[j]);
        }

        if((cp[0]).Equals(cp[1], RATPOLY_EPS)) break;

        Vector p0 = Vector::AtIntersectionOfPlanes(n[0], d[0], n[1], d[1]),
               dp = (n[0]).Cross(n[1]);

        Vector pc = p.ClosestPointOnLine(p0, dp);

        // Adjust our guess and iterate
        for(j = 0; j < 2; j++) {
            Vector n = tu[j].Cross(tv[j]);
            Vector ty = n.Cross(tu[j]).ScaledBy(1.0/tu[j].MagSquared());
            Vector tx = tv[j].Cross(n).ScaledBy(1.0/tv[j].MagSquared());

            Vector dc = pc.Minus(cp[j]);
            double du = dc.Dot(tx), dv = dc.Dot(ty);
            puv[j].x += du / tx.MagSquared();
            puv[j].y += dv / ty.MagSquared();
        }
    }
    if(i >= 10) {
        dbp("this and srf, didn't converge, d=%g",
            (puv[0].Minus(puv[1])).Magnitude());
    }

    // If this converged, then the two points are actually equal.
    return ((srf[0])->PointAt(puv[0])).Plus(
           ((srf[1])->PointAt(puv[1]))).ScaledBy(0.5);
}

void SSurface::PointOnSurfaces(SSurface *s1, SSurface *s2, double *up, double *vp)
{
    double u[3] = { *up, 0, 0 }, v[3] = { *vp, 0, 0 };
    SSurface *srf[3] = { this, s1, s2 };

    // Get initial guesses for (u, v) in the other surfaces
    Vector p = PointAt(*u, *v);
    (srf[1])->ClosestPointTo(p, &(u[1]), &(v[1]), /*mustConverge=*/false);
    (srf[2])->ClosestPointTo(p, &(u[2]), &(v[2]), /*mustConverge=*/false);

    int i, j;
    for(i = 0; i < 20; i++) {
        // Approximate each surface by a plane
        Vector p[3], tu[3], tv[3], n[3];
        double d[3];
        for(j = 0; j < 3; j++) {
            p[j] = (srf[j])->PointAt(u[j], v[j]);
            (srf[j])->TangentsAt(u[j], v[j], &(tu[j]), &(tv[j]));
            n[j] = ((tu[j]).Cross(tv[j])).WithMagnitude(1);
            d[j] = (n[j]).Dot(p[j]);
        }

        // If a = b and b = c, then does a = c? No, it doesn't.
        if((p[0]).Equals(p[1], RATPOLY_EPS) &&
           (p[1]).Equals(p[2], RATPOLY_EPS) &&
           (p[2]).Equals(p[0], RATPOLY_EPS))
        {
            *up = u[0];
            *vp = v[0];
            return;
        }

        bool parallel;
        Vector pi = Vector::AtIntersectionOfPlanes(n[0], d[0],
                                                   n[1], d[1],
                                                   n[2], d[2], &parallel);
        if(parallel) break;

        for(j = 0; j < 3; j++) {
            Vector n = tu[j].Cross(tv[j]);
            Vector ty = n.Cross(tu[j]).ScaledBy(1.0/tu[j].MagSquared());
            Vector tx = tv[j].Cross(n).ScaledBy(1.0/tv[j].MagSquared());

            Vector dp = pi.Minus(p[j]);
            double du = dp.Dot(tx), dv = dp.Dot(ty);

            u[j] += du / tx.MagSquared();
            v[j] += dv / ty.MagSquared();
        }
    }
    dbp("didn't converge (three surfaces intersecting)");
}

