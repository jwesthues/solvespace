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
        case Type::LENGTH_DIFFERENCE:   s = C_("constr-name", "length-difference"); break;
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

