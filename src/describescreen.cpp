//-----------------------------------------------------------------------------
// The screens when an entity is selected, that show some description of it--
// endpoints of the lines, diameter of the circle, etc.
//
// Copyright 2008-2013 Jonathan Westhues.
//-----------------------------------------------------------------------------
#include "solvespace.h"
#include "iteration.h"

void TextWindow::ScreenUnselectAll(int link, uint32_t v) {
    GraphicsWindow::MenuEdit(Command::UNSELECT_ALL);
}

void TextWindow::ScreenEditTtfText(int link, uint32_t v) {
    hRequest hr = { v };
    Request *r = SK.GetRequest(hr);

    SS.TW.ShowEditControl(10, r->str);
    SS.TW.edit.meaning = Edit::TTF_TEXT;
    SS.TW.edit.request = hr;
}

void TextWindow::ScreenSetTtfFont(int link, uint32_t v) {
    int i = (int)v;
    if(i < 0) return;
    if(i >= SS.fonts.l.n) return;

    SS.GW.GroupSelection();
    auto const &gs = SS.GW.gs;
    if(gs.entities != 1 || gs.n != 1) return;

    Entity *e = SK.entity.FindByIdNoOops(gs.entity[0]);
    if(!e || e->type != Entity::Type::TTF_TEXT || !e->h.isFromRequest()) return;

    Request *r = SK.request.FindByIdNoOops(e->h.request());
    if(!r) return;

    SS.UndoRemember();
    r->font = SS.fonts.l[i].FontFileBaseName();
    SS.MarkGroupDirty(r->group);
    SS.ScheduleShowTW();
}

void TextWindow::ScreenConstraintShowAsRadius(int link, uint32_t v) {
    hConstraint hc = { v };
    Constraint *c = SK.GetConstraint(hc);

    SS.UndoRemember();
    c->other = !c->other;

    SS.ScheduleShowTW();
}

void TextWindow::DescribeSelection() {
    Printf(false, "");

    auto const &gs = SS.GW.gs;
    if(gs.n == 1 && (gs.points == 1 || gs.entities == 1)) {
        Entity *e = SK.GetEntity(gs.points == 1 ? gs.point[0] : gs.entity[0]);
        Vector p;

#define COSTR(p) \
    SS.MmToString((p).x).c_str(), \
    SS.MmToString((p).y).c_str(), \
    SS.MmToString((p).z).c_str()
#define PT_AS_STR "(%Fi%s%E, %Fi%s%E, %Fi%s%E)"
#define PT_AS_NUM "(%Fi%3%E, %Fi%3%E, %Fi%3%E)"
        switch(e->type) {
            case Entity::Type::POINT_IN_3D:
            case Entity::Type::POINT_IN_2D:
            case Entity::Type::POINT_N_TRANS:
            case Entity::Type::POINT_N_ROT_TRANS:
            case Entity::Type::POINT_N_COPY:
            case Entity::Type::POINT_N_ROT_AA:
                p = e->PointGetNum();
                Printf(false, "%FtPOINT%E at " PT_AS_STR, COSTR(p));
                break;

            case Entity::Type::NORMAL_IN_3D:
            case Entity::Type::NORMAL_IN_2D:
            case Entity::Type::NORMAL_N_COPY:
            case Entity::Type::NORMAL_N_ROT:
            case Entity::Type::NORMAL_N_ROT_AA: {
                Quaternion q = e->NormalGetNum();
                p = q.RotationN();
                Printf(false, "%FtNORMAL / COORDINATE SYSTEM%E");
                Printf(true,  "  basis n = " PT_AS_NUM, CO(p));
                p = q.RotationU();
                Printf(false, "        u = " PT_AS_NUM, CO(p));
                p = q.RotationV();
                Printf(false, "        v = " PT_AS_NUM, CO(p));
                break;
            }
            case Entity::Type::WORKPLANE: {
                p = SK.GetEntity(e->point[0])->PointGetNum();
                Printf(false, "%FtWORKPLANE%E");
                Printf(true, "   origin = " PT_AS_STR, COSTR(p));
                Quaternion q = e->Normal()->NormalGetNum();
                p = q.RotationN();
                Printf(true, "   normal = " PT_AS_NUM, CO(p));
                break;
            }
            case Entity::Type::LINE_SEGMENT: {
                Vector p0 = SK.GetEntity(e->point[0])->PointGetNum();
                p = p0;
                Printf(false, "%FtLINE SEGMENT%E");
                Printf(true,  "   thru " PT_AS_STR, COSTR(p));
                Vector p1 = SK.GetEntity(e->point[1])->PointGetNum();
                p = p1;
                Printf(false, "        " PT_AS_STR, COSTR(p));
                Printf(true,  "   len = %Fi%s%E",
                    SS.MmToString((p1.Minus(p0).Magnitude())).c_str());
                break;
            }
            case Entity::Type::CUBIC_PERIODIC:
            case Entity::Type::CUBIC:
                int pts;
                if(e->type == Entity::Type::CUBIC_PERIODIC) {
                    Printf(false, "%FtPERIODIC C2 CUBIC SPLINE%E");
                    pts = (3 + e->extraPoints);
                } else if(e->extraPoints > 0) {
                    Printf(false, "%FtINTERPOLATING C2 CUBIC SPLINE%E");
                    pts = (4 + e->extraPoints);
                } else {
                    Printf(false, "%FtCUBIC BEZIER CURVE%E");
                    pts = 4;
                }
                for(int i = 0; i < pts; i++) {
                    p = SK.GetEntity(e->point[i])->PointGetNum();
                    Printf((i==0), "   p%d = " PT_AS_STR, i, COSTR(p));
                }
                break;

            case Entity::Type::ARC_OF_CIRCLE: {
                Printf(false, "%FtARC OF A CIRCLE%E");
                p = SK.GetEntity(e->point[0])->PointGetNum();
                Printf(true,  "     center = " PT_AS_STR, COSTR(p));
                p = SK.GetEntity(e->point[1])->PointGetNum();
                Printf(true,  "  endpoints = " PT_AS_STR, COSTR(p));
                p = SK.GetEntity(e->point[2])->PointGetNum();
                Printf(false, "              " PT_AS_STR, COSTR(p));
                double r = e->CircleGetRadiusNum();
                Printf(true, "   diameter =  %Fi%s", SS.MmToString(r*2).c_str());
                Printf(false, "     radius =  %Fi%s", SS.MmToString(r).c_str());
                double thetas, thetaf, dtheta;
                e->ArcGetAngles(&thetas, &thetaf, &dtheta);
                Printf(false, "    arc len =  %Fi%s", SS.MmToString(dtheta*r).c_str());
                break;
            }
            case Entity::Type::CIRCLE: {
                Printf(false, "%FtCIRCLE%E");
                p = SK.GetEntity(e->point[0])->PointGetNum();
                Printf(true,  "     center = " PT_AS_STR, COSTR(p));
                double r = e->CircleGetRadiusNum();
                Printf(true,  "   diameter =  %Fi%s", SS.MmToString(r*2).c_str());
                Printf(false, "     radius =  %Fi%s", SS.MmToString(r).c_str());
                break;
            }
            case Entity::Type::FACE_NORMAL_PT:
            case Entity::Type::FACE_XPROD:
            case Entity::Type::FACE_N_ROT_TRANS:
            case Entity::Type::FACE_N_ROT_AA:
            case Entity::Type::FACE_N_TRANS:
                Printf(false, "%FtPLANE FACE%E");
                p = e->FaceGetNormalNum();
                Printf(true,  "   normal = " PT_AS_NUM, CO(p));
                p = e->FaceGetPointNum();
                Printf(false, "     thru = " PT_AS_STR, COSTR(p));
                break;

            case Entity::Type::TTF_TEXT: {
                Printf(false, "%FtTRUETYPE FONT TEXT%E");
                Printf(true, "  font = '%Fi%s%E'", e->font.c_str());
                if(e->h.isFromRequest()) {
                    Printf(false, "  text = '%Fi%s%E' %Fl%Ll%f%D[change]%E",
                        e->str.c_str(), &ScreenEditTtfText, e->h.request().v);
                    Printf(true, "  select new font");
                    SS.fonts.LoadAll();
                    for (auto tf : indices(SS.fonts.l)) {
                        bool parity = (tf.GetIndex() & 1);
                        if(e->font == tf->FontFileBaseName()) {
                            Printf(false, "%Bp    %s",
                                parity ? 'd' : 'a',
                                tf->name.c_str());
                        } else {
                            Printf(false, "%Bp    %f%D%Fl%Ll%s%E%Bp",
                                parity ? 'd' : 'a',
                                &ScreenSetTtfFont, tf.GetIndex(),
                                tf->name.c_str(),
                                parity ? 'd' : 'a');
                        }
                    }
                } else {
                    Printf(false, "  text = '%Fi%s%E'", e->str.c_str());
                }
                break;
            }
            case Entity::Type::IMAGE: {
                Printf(false, "%FtIMAGE%E");
                Platform::Path relativePath = e->file.RelativeTo(SS.saveFile.Parent());
                if(relativePath.IsEmpty()) {
                    Printf(true, "  file = '%Fi%s%E'", e->file.raw.c_str());
                } else {
                    Printf(true, "  file = '%Fi%s%E'", relativePath.raw.c_str());
                }
                break;
            }

            default:
                Printf(true, "%Ft?? ENTITY%E");
                break;
        }

        Group *g = SK.GetGroup(e->group);
        Printf(false, "");
        Printf(false, "%FtIN GROUP%E      %s", g->DescriptionString().c_str());
        if(e->workplane == Entity::FREE_IN_3D) {
            Printf(false, "%FtNOT LOCKED IN WORKPLANE%E");
        } else {
            Entity *w = SK.GetEntity(e->workplane);
            Printf(false, "%FtIN WORKPLANE%E  %s", w->DescriptionString().c_str());
        }
        if(e->style.v) {
            Style *s = Style::Get(e->style);
            Printf(false, "%FtIN STYLE%E      %s", s->DescriptionString().c_str());
        } else {
            Printf(false, "%FtIN STYLE%E      none");
        }
        if(e->construction) {
            Printf(false, "%FtCONSTRUCTION");
        }

        std::vector<hConstraint> lhc = {};
        for(const Constraint &c : SK.constraint) {
            if(!(c.ptA == e->h ||
                 c.ptB == e->h ||
                 c.entityA == e->h ||
                 c.entityB == e->h ||
                 c.entityC == e->h ||
                 c.entityD == e->h))
                continue;
            lhc.push_back(c.h);
        }

        if(!lhc.empty()) {
            Printf(true, "%FtCONSTRAINED BY:%E");

            int a = 0;
            for(hConstraint hc : lhc) {
                Constraint *c = SK.GetConstraint(hc);
                std::string s = c->DescriptionString();
                Printf(false, "%Bp   %Fl%Ll%D%f%h%s%E %s",
                    (a & 1) ? 'd' : 'a',
                    c->h.v, (&TextWindow::ScreenSelectConstraint),
                    (&TextWindow::ScreenHoverConstraint), s.c_str(),
                    c->reference ? "(ref)" : "");
                a++;
            }
        }
    } else if(gs.n == 2 && gs.points == 2) {
        Printf(false, "%FtTWO POINTS");
        Vector p0 = SK.GetEntity(gs.point[0])->PointGetNum();
        Printf(true,  "   at " PT_AS_STR, COSTR(p0));
        Vector p1 = SK.GetEntity(gs.point[1])->PointGetNum();
        Printf(false, "      " PT_AS_STR, COSTR(p1));
        double d = (p1.Minus(p0)).Magnitude();
        Printf(true, "  d = %Fi%s", SS.MmToString(d).c_str());
    } else if(gs.n == 2 && gs.points == 1 && gs.circlesOrArcs == 1) {
        Entity *ec = SK.GetEntity(gs.entity[0]);
        if(ec->type == Entity::Type::CIRCLE) {
            Printf(false, "%FtPOINT AND A CIRCLE");
        } else if(ec->type == Entity::Type::ARC_OF_CIRCLE) {
            Printf(false, "%FtPOINT AND AN ARC");
        } else ssassert(false, "Unexpected entity type");
        Vector p = SK.GetEntity(gs.point[0])->PointGetNum();
        Printf(true,  "        pt at " PT_AS_STR, COSTR(p));
        Vector c = SK.GetEntity(ec->point[0])->PointGetNum();
        Printf(true,  "     center = " PT_AS_STR, COSTR(c));
        double r = ec->CircleGetRadiusNum();
        Printf(false, "   diameter =  %Fi%s", SS.MmToString(r*2).c_str());
        Printf(false, "     radius =  %Fi%s", SS.MmToString(r).c_str());
        double d = (p.Minus(c)).Magnitude() - r;
        Printf(true,  "   distance = %Fi%s", SS.MmToString(d).c_str());
    } else if(gs.n == 2 && gs.faces == 1 && gs.points == 1) {
        Printf(false, "%FtA POINT AND A PLANE FACE");
        Vector pt = SK.GetEntity(gs.point[0])->PointGetNum();
        Printf(true,  "        point = " PT_AS_STR, COSTR(pt));
        Vector n = SK.GetEntity(gs.face[0])->FaceGetNormalNum();
        Printf(true,  " plane normal = " PT_AS_NUM, CO(n));
        Vector pl = SK.GetEntity(gs.face[0])->FaceGetPointNum();
        Printf(false, "   plane thru = " PT_AS_STR, COSTR(pl));
        double dd = n.Dot(pl) - n.Dot(pt);
        Printf(true,  "     distance = %Fi%s", SS.MmToString(dd).c_str());
    } else if(gs.n == 3 && gs.points == 2 && gs.vectors == 1) {
        Printf(false, "%FtTWO POINTS AND A VECTOR");
        Vector p0 = SK.GetEntity(gs.point[0])->PointGetNum();
        Printf(true,  "  pointA = " PT_AS_STR, COSTR(p0));
        Vector p1 = SK.GetEntity(gs.point[1])->PointGetNum();
        Printf(false, "  pointB = " PT_AS_STR, COSTR(p1));
        Vector v  = SK.GetEntity(gs.vector[0])->VectorGetNum();
        v = v.WithMagnitude(1);
        Printf(true,  "  vector = " PT_AS_NUM, CO(v));
        double d = (p1.Minus(p0)).Dot(v);
        Printf(true,  "  proj_d = %Fi%s", SS.MmToString(d).c_str());
    } else if(gs.n == 2 && gs.lineSegments == 1 && gs.points == 1) {
        Entity *ln = SK.GetEntity(gs.entity[0]);
        Vector lp0 = SK.GetEntity(ln->point[0])->PointGetNum(),
               lp1 = SK.GetEntity(ln->point[1])->PointGetNum();
        Printf(false, "%FtLINE SEGMENT AND POINT%E");
        Printf(true,  "   ln thru " PT_AS_STR, COSTR(lp0));
        Printf(false, "           " PT_AS_STR, COSTR(lp1));
        Entity *p  = SK.GetEntity(gs.point[0]);
        Vector pp = p->PointGetNum();
        Printf(true,  "     point " PT_AS_STR, COSTR(pp));
        Printf(true,  " pt-ln distance = %Fi%s%E",
            SS.MmToString(pp.DistanceToLine(lp0, lp1.Minus(lp0))).c_str());
        hEntity wrkpl = SS.GW.ActiveWorkplane();
        if(wrkpl != Entity::FREE_IN_3D && !(p->workplane == wrkpl && ln->workplane == wrkpl)) {
            Vector ppw  = pp.ProjectInto(wrkpl);
            Vector lp0w = lp0.ProjectInto(wrkpl);
            Vector lp1w = lp1.ProjectInto(wrkpl);
            Printf(false, "    or distance = %Fi%s%E (in workplane)",
                SS.MmToString(ppw.DistanceToLine(lp0w, lp1w.Minus(lp0w))).c_str());
        }
    } else if(gs.n == 2 && gs.vectors == 2) {
        Printf(false, "%FtTWO VECTORS");

        Vector v0 = SK.GetEntity(gs.entity[0])->VectorGetNum(),
               v1 = SK.GetEntity(gs.entity[1])->VectorGetNum();
        v0 = v0.WithMagnitude(1);
        v1 = v1.WithMagnitude(1);

        Printf(true,  "  vectorA = " PT_AS_NUM, CO(v0));
        Printf(false, "  vectorB = " PT_AS_NUM, CO(v1));

        double theta = acos(v0.Dot(v1));
        Printf(true,  "    angle = %Fi%2%E degrees", theta*180/PI);
        while(theta < PI/2) theta += PI;
        while(theta > PI/2) theta -= PI;
        Printf(false, " or angle = %Fi%2%E (mod 180)", theta*180/PI);
    } else if(gs.n == 2 && gs.faces == 2) {
        Printf(false, "%FtTWO PLANE FACES");

        Vector n0 = SK.GetEntity(gs.face[0])->FaceGetNormalNum();
        Printf(true,  " planeA normal = " PT_AS_NUM, CO(n0));
        Vector p0 = SK.GetEntity(gs.face[0])->FaceGetPointNum();
        Printf(false, "   planeA thru = " PT_AS_STR, COSTR(p0));

        Vector n1 = SK.GetEntity(gs.face[1])->FaceGetNormalNum();
        Printf(true,  " planeB normal = " PT_AS_NUM, CO(n1));
        Vector p1 = SK.GetEntity(gs.face[1])->FaceGetPointNum();
        Printf(false, "   planeB thru = " PT_AS_STR, COSTR(p1));

        double theta = acos(n0.Dot(n1));
        Printf(true,  "         angle = %Fi%2%E degrees", theta*180/PI);
        while(theta < PI/2) theta += PI;
        while(theta > PI/2) theta -= PI;
        Printf(false, "      or angle = %Fi%2%E (mod 180)", theta*180/PI);

        if(fabs(theta) < 0.01) {
            double d = (p1.Minus(p0)).Dot(n0);
            Printf(true,  "      distance = %Fi%s", SS.MmToString(d).c_str());
        }
    } else if(gs.n == 0 && gs.stylables > 0) {
        Printf(false, "%FtSELECTED:%E comment text");
    } else if(gs.n == 0 && gs.constraints == 1) {
        Constraint *c = SK.GetConstraint(gs.constraint[0]);

        if(c->type == Constraint::Type::DIAMETER) {
            Printf(false, "%FtDIAMETER CONSTRAINT");

            Printf(true, "  %Fd%f%D%Ll%s  show as radius",
                   &ScreenConstraintShowAsRadius, gs.constraint[0].v,
                   c->other ? CHECK_TRUE : CHECK_FALSE);
        } else {
            Printf(false, "%FtSELECTED:%E %s",
            c->DescriptionString().c_str());
        }

        std::vector<hEntity> lhe = {};
        lhe.push_back(c->ptA);
        lhe.push_back(c->ptB);
        lhe.push_back(c->entityA);
        lhe.push_back(c->entityB);
        lhe.push_back(c->entityC);
        lhe.push_back(c->entityD);

        auto it = std::remove_if(lhe.begin(), lhe.end(), [](hEntity he) {
            return he == Entity::NO_ENTITY || !he.isFromRequest();
        });
        lhe.erase(it, lhe.end());

        if(!lhe.empty()) {
            Printf(true, "%FtCONSTRAINS:%E");

            int a = 0;
            for(hEntity he : lhe) {
                Request *r = SK.GetRequest(he.request());
                std::string s = r->DescriptionString();
                Printf(false, "%Bp   %Fl%Ll%D%f%h%s%E",
                    (a & 1) ? 'd' : 'a',
                    r->h.v, (&TextWindow::ScreenSelectRequest),
                    &(TextWindow::ScreenHoverRequest), s.c_str());
                a++;
            }
        }
    } else {
        int n = SS.GW.selection.n;
        Printf(false, "%FtSELECTED:%E %d item%s", n, n == 1 ? "" : "s");
    }

    if(shown.screen == Screen::STYLE_INFO &&
       shown.style.v >= Style::FIRST_CUSTOM && gs.stylables > 0)
    {
        // If we are showing a screen for a particular style, then offer the
        // option to assign our selected entities to that style.
        Style *s = Style::Get(shown.style);
        Printf(true, "%Fl%D%f%Ll(assign to style %s)%E",
            shown.style.v,
            &ScreenAssignSelectionToStyle,
            s->DescriptionString().c_str());
    }
    // If any of the selected entities have an assigned style, then offer
    // the option to remove that style.
    bool styleAssigned = false;
    for(int i = 0; i < gs.entities; i++) {
        Entity *e = SK.GetEntity(gs.entity[i]);
        if(e->style.v != 0) {
            styleAssigned = true;
        }
    }
    for(int i = 0; i < gs.constraints; i++) {
        Constraint *c = SK.GetConstraint(gs.constraint[i]);
        if(c->type == Constraint::Type::COMMENT && c->disp.style.v != 0) {
            styleAssigned = true;
        }
    }
    if(styleAssigned) {
        Printf(true, "%Fl%D%f%Ll(remove assigned style)%E",
            0,
            &ScreenAssignSelectionToStyle);
    }

    Printf(true, "%Fl%f%Ll(unselect all)%E", &TextWindow::ScreenUnselectAll);
}

void TextWindow::GoToScreen(Screen screen) {
    shown.screen = screen;
}

