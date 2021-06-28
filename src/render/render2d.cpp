//-----------------------------------------------------------------------------
// Rendering projections to 2d surfaces: z-sorting, occlusion testing, etc.
//
// Copyright 2016 whitequark
//-----------------------------------------------------------------------------
#include "solvespace.h"

namespace SolveSpace {

// FIXME: The export coordinate system has a different handedness than display
// coordinate system; lighting and occlusion calculations are right-handed.
static Vector ProjectPoint3RH(const Camera &camera, Vector p) {
    p = p.Plus(camera.offset);

    Vector r;
    r.x = p.Dot(camera.projRight);
    r.y = p.Dot(camera.projUp);
    r.z = p.Dot(camera.projRight.Cross(camera.projUp));

    double w = 1 + r.z*camera.tangent*camera.scale;
    return r.ScaledBy(camera.scale/w);
}

//-----------------------------------------------------------------------------
// Accumulation of geometry
//-----------------------------------------------------------------------------

void SurfaceRenderer::DrawLine(const Vector &a, const Vector &b, hStroke hcs) {
    edges[hcs].AddEdge(ProjectPoint3RH(camera, a),
                       ProjectPoint3RH(camera, b));
}

void SurfaceRenderer::DrawEdges(const SEdgeList &el, hStroke hcs) {
    for(const SEdge &e : el.l) {
        edges[hcs].AddEdge(ProjectPoint3RH(camera, e.a),
                           ProjectPoint3RH(camera, e.b));
    }
}

bool SurfaceRenderer::DrawBeziers(const SBezierList &bl, hStroke hcs) {
    if(!CanOutputCurves())
        return false;

    for(const SBezier &b : bl.l) {
        SBezier pb = camera.ProjectBezier(b);
        beziers[hcs].l.Add(&pb);
    }
    return true;
}

void SurfaceRenderer::DrawOutlines(const SOutlineList &ol, hStroke hcs, DrawOutlinesAs drawAs) {
    Vector projDir = camera.projRight.Cross(camera.projUp);
    for(const SOutline &o : ol.l) {
        if(drawAs == DrawOutlinesAs::EMPHASIZED_AND_CONTOUR &&
                !(o.IsVisible(projDir) || o.tag != 0))
            continue;
        if(drawAs == DrawOutlinesAs::EMPHASIZED_WITHOUT_CONTOUR &&
                !(!o.IsVisible(projDir) && o.tag != 0))
            continue;
        if(drawAs == DrawOutlinesAs::CONTOUR_ONLY &&
                !(o.IsVisible(projDir)))
            continue;

        edges[hcs].AddEdge(ProjectPoint3RH(camera, o.a),
                           ProjectPoint3RH(camera, o.b));
    }
}

void SurfaceRenderer::DrawVectorText(const std::string &text, double height,
                                     const Vector &o, const Vector &u, const Vector &v,
                                     hStroke hcs) {
    auto traceEdge = [&](Vector a, Vector b) {
        edges[hcs].AddEdge(ProjectPoint3RH(camera, a),
                           ProjectPoint3RH(camera, b));
    };
    VectorFont::Builtin()->Trace(height, o, u, v, text, traceEdge, camera);
}

void SurfaceRenderer::DrawQuad(const Vector &a, const Vector &b, const Vector &c, const Vector &d,
                               hFill hcf) {
    Fill *fill = fills.FindById(hcf);
    ssassert(fill->layer == Layer::NORMAL ||
             fill->layer == Layer::DEPTH_ONLY ||
             fill->layer == Layer::FRONT ||
             fill->layer == Layer::BACK, "Unexpected mesh layer");

    Vector zOffset = {};
    if(fill->layer == Layer::BACK) {
        zOffset.z -= 1e6;
    } else if(fill->layer == Layer::FRONT) {
        zOffset.z += 1e6;
    }
    zOffset.z += camera.scale * fill->zIndex;

    STriMeta meta = {};
    if(fill->layer != Layer::DEPTH_ONLY) {
        meta.color = fill->color;
    }
    Vector ta = ProjectPoint3RH(camera, a).Plus(zOffset),
           tb = ProjectPoint3RH(camera, b).Plus(zOffset),
           tc = ProjectPoint3RH(camera, c).Plus(zOffset),
           td = ProjectPoint3RH(camera, d).Plus(zOffset);
    mesh.AddTriangle(meta, tc, tb, ta);
    mesh.AddTriangle(meta, ta, td, tc);
}

void SurfaceRenderer::DrawPoint(const Vector &o, Canvas::hStroke hcs) {
    Stroke *stroke = strokes.FindById(hcs);

    Fill fill = {};
    fill.layer  = stroke->layer;
    fill.zIndex = stroke->zIndex;
    fill.color  = stroke->color;
    hFill hcf = GetFill(fill);

    Vector u = camera.projRight.ScaledBy(stroke->width/2.0/camera.scale),
           v = camera.projUp.ScaledBy(stroke->width/2.0/camera.scale);
    DrawQuad(o.Minus(u).Minus(v), o.Minus(u).Plus(v),
             o.Plus(u).Plus(v),   o.Plus(u).Minus(v), hcf);
}

void SurfaceRenderer::DrawPolygon(const SPolygon &p, hFill hcf) {
    SMesh m = {};
    p.TriangulateInto(&m);
    DrawMesh(m, hcf, {});
    m.Clear();
}

void SurfaceRenderer::DrawMesh(const SMesh &m,
                               hFill hcfFront, hFill hcfBack) {
    Fill *fill = fills.FindById(hcfFront);
    ssassert(fill->layer == Layer::NORMAL ||
             fill->layer == Layer::DEPTH_ONLY, "Unexpected mesh layer");

    Vector l0 = (lighting.lightDirection[0]).WithMagnitude(1),
           l1 = (lighting.lightDirection[1]).WithMagnitude(1);
    for(STriangle tr : m.l) {
        tr.a = ProjectPoint3RH(camera, tr.a);
        tr.b = ProjectPoint3RH(camera, tr.b);
        tr.c = ProjectPoint3RH(camera, tr.c);

        if(CanOutputTriangles() && fill->layer == Layer::NORMAL) {
            if(fill->color.IsEmpty()) {
                // Compute lighting, since we're going to draw the shaded triangles.
                Vector n = tr.Normal().WithMagnitude(1);
                double intensity = lighting.ambientIntensity +
                                        max(0.0, (lighting.lightIntensity[0])*(n.Dot(l0))) +
                                        max(0.0, (lighting.lightIntensity[1])*(n.Dot(l1)));
                double r = min(1.0, tr.meta.color.redF()   * intensity),
                       g = min(1.0, tr.meta.color.greenF() * intensity),
                       b = min(1.0, tr.meta.color.blueF()  * intensity);
                tr.meta.color = RGBf(r, g, b);
            } else {
                // We're going to draw this triangle, but it's not shaded.
                tr.meta.color = fill->color;
            }
        } else {
            // This triangle is just for occlusion testing.
            tr.meta.color = {};
        }
        mesh.AddTriangle(&tr);
    }
}

void SurfaceRenderer::DrawFaces(const SMesh &m, const std::vector<uint32_t> &faces, hFill hcf) {
    Fill *fill = fills.FindById(hcf);
    ssassert(fill->layer == Layer::NORMAL ||
             fill->layer == Layer::DEPTH_ONLY, "Unexpected mesh layer");

    Vector zOffset = {};
    zOffset.z += camera.scale * fill->zIndex;

    size_t facesSize = faces.size();
    for(STriangle tr : m.l) {
        uint32_t face = tr.meta.face;
        for(size_t j = 0; j < facesSize; j++) {
            if(faces[j] != face) continue;
            if(!fill->color.IsEmpty()) {
                tr.meta.color = fill->color;
            }
            mesh.AddTriangle(tr.meta,
                ProjectPoint3RH(camera, tr.a).Plus(zOffset),
                ProjectPoint3RH(camera, tr.b).Plus(zOffset),
                ProjectPoint3RH(camera, tr.c).Plus(zOffset));
            break;
        }
    }
}

void SurfaceRenderer::DrawPixmap(std::shared_ptr<const Pixmap> pm,
                    const Vector &o, const Vector &u, const Vector &v,
                    const Point2d &ta, const Point2d &tb, hFill hcf) {
    dbp("Not implemented");
}

void SurfaceRenderer::InvalidatePixmap(std::shared_ptr<const Pixmap> pm) {
    dbp("Not implemented");
}

//-----------------------------------------------------------------------------
// Processing of geometry
//-----------------------------------------------------------------------------

void SurfaceRenderer::CalculateBBox() {
    bbox.minp = Vector::From(VERY_POSITIVE, VERY_POSITIVE, VERY_POSITIVE);
    bbox.maxp = Vector::From(VERY_NEGATIVE, VERY_NEGATIVE, VERY_NEGATIVE);

    for(auto &it : edges) {
        SEdgeList &el = it.second;
        for(SEdge &e : el.l) {
            bbox.Include(e.a);
            bbox.Include(e.b);
        }
    }

    for(auto &it : beziers) {
        SBezierList &bl = it.second;
        for(SBezier &b : bl.l) {
            for(int i = 0; i <= b.deg; i++) {
                bbox.Include(b.ctrl[i]);
            }
        }
    }

    for(STriangle &tr : mesh.l) {
        for(int i = 0; i < 3; i++) {
            bbox.Include(tr.vertices[i]);
        }
    }
}


void SurfaceRenderer::ConvertBeziersToEdges() {
    for(auto &it : beziers) {
        hStroke hcs = it.first;
        SBezierList &bl = it.second;

        SEdgeList &el = edges[hcs];
        for(const SBezier &b : bl.l) {
            if(b.deg == 1) {
                el.AddEdge(b.ctrl[0], b.ctrl[1]);
            } else {
                List<Vector> lv = {};
                b.MakePwlInto(&lv, chordTolerance);
                for(int i = 1; i < lv.n; i++) {
                    el.AddEdge(lv[i-1], lv[i]);
                }
                lv.Clear();
            }
        }
        bl.l.Clear();
    }
    beziers.clear();
}

void SurfaceRenderer::CullOccludedStrokes() {
    // Perform occlusion testing, if necessary.
    if(mesh.l.IsEmpty())
        return;

    // We can't perform hidden line removal on exact curves.
    ConvertBeziersToEdges();

    // Remove hidden lines (on NORMAL layers), or remove visible lines (on OCCLUDED layers).
    SKdNode *root = SKdNode::From(&mesh);
    root->ClearTags();

    int cnt = 1234;
    for(auto &eit : edges) {
        hStroke hcs = eit.first;
        SEdgeList &el = eit.second;

        Stroke *stroke = strokes.FindById(hcs);
        if(stroke->layer != Layer::NORMAL &&
           stroke->layer != Layer::OCCLUDED) continue;

        SEdgeList nel = {};
        for(const SEdge &e : el.l) {
            SEdgeList oel = {};
            oel.AddEdge(e.a, e.b);
            root->OcclusionTestLine(e, &oel, cnt);

            if(stroke->layer == Layer::OCCLUDED) {
                for(SEdge &oe : oel.l) {
                    oe.tag = !oe.tag;
                }
            }
            oel.l.RemoveTagged();

            oel.MergeCollinearSegments(e.a, e.b);
            for(const SEdge &oe : oel.l) {
                nel.AddEdge(oe.a, oe.b);
            }

            oel.Clear();
            cnt++;
        }

        el.l.Clear();
        el.l = nel.l;
    }
}

void SurfaceRenderer::OutputInPaintOrder() {
    // Sort our strokes in paint order.
    std::vector<std::pair<Layer, int>> paintOrder;
    paintOrder.emplace_back(Layer::NORMAL, 0); // mesh
    for(const Stroke &cs : strokes) {
        paintOrder.emplace_back(cs.layer, cs.zIndex);
    }

    const Layer stackup[] = {
        Layer::BACK, Layer::NORMAL, Layer::DEPTH_ONLY, Layer::OCCLUDED, Layer::FRONT
    };
    std::sort(paintOrder.begin(), paintOrder.end(),
              [&](std::pair<Layer, int> a, std::pair<Layer, int> b) {
        Layer aLayer  = a.first,
              bLayer  = b.first;
        int   aZIndex = a.second,
              bZIndex = b.second;

        auto aLayerIndex = FindIndex(stackup, aLayer);
        auto bLayerIndex = FindIndex(stackup, bLayer);
        if(aLayerIndex == bLayerIndex) {
            return aZIndex < bZIndex;
        }
        return aLayerIndex < bLayerIndex;
    });

    auto last = std::unique(paintOrder.begin(), paintOrder.end());
    paintOrder.erase(last, paintOrder.end());

    // Output geometry in paint order.
    OutputStart();
    for(auto &it : paintOrder) {
        Layer layer  = it.first;
        int   zIndex = it.second;

        if(layer == Layer::NORMAL && zIndex == 0) {
            SMesh mp = {};
            SBsp3 *bsp = SBsp3::FromMesh(&mesh);
            if(bsp) bsp->GenerateInPaintOrder(&mp);

            for(const STriangle &tr : mp.l) {
                // Cull back-facing and invisible triangles.
                if(tr.Normal().z < 0) continue;
                if(tr.meta.color.IsEmpty()) continue;
                OutputTriangle(tr);
            }

            mp.Clear();
        }

        for(auto eit : edges) {
            hStroke hcs = eit.first;
            const SEdgeList &el = eit.second;

            Stroke *stroke = strokes.FindById(hcs);
            if(stroke->layer != layer || stroke->zIndex != zIndex) continue;

            for(const SEdge &e : el.l) {
                OutputBezier(SBezier::From(e.a, e.b), hcs);
            }
        }

        for(auto &bit : beziers) {
            hStroke hcs = bit.first;
            const SBezierList &bl = bit.second;

            Stroke *stroke = strokes.FindById(hcs);
            if(stroke->layer != layer || stroke->zIndex != zIndex) continue;

            for(const SBezier &b : bl.l) {
                OutputBezier(b, hcs);
            }
        }
    }
    OutputEnd();
}

void SurfaceRenderer::Clear() {
    Canvas::Clear();

    for(auto &eit : edges) {
        SEdgeList &el = eit.second;
        el.l.Clear();
    }
    edges.clear();

    for(auto &bit : beziers) {
        SBezierList &bl = bit.second;
        bl.l.Clear();
    }
    beziers.clear();

    mesh.Clear();
}

void SurfaceRenderer::OutputBezierAsNonrationalCubic(const SBezier &b, hStroke hcs) {
    // Arbitrary choice of tolerance; make it a little finer than pwl tolerance since
    // it should be easier to achieve that with the smooth curves.
    SBezierList bl;
    b.MakeNonrationalCubicInto(&bl, chordTolerance / 2);
    for(const SBezier &cb : bl.l) {
        OutputBezier(cb, hcs);
    }
    bl.Clear();
}

}
