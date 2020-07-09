//-----------------------------------------------------------------------------
// Inlined implementations of some math operations using Eigen.
//
// Copyright 2008-2013 Jonathan Westhues.
// Copyright 2018 Ryan Pavlik.
//-----------------------------------------------------------------------------
#ifndef __UTILEIGEN_H
#define __UTILEIGEN_H

#include "solvespace.h"

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <unsupported/Eigen/AlignedVector3>

namespace SolveSpace {

using EiVector = Eigen::AlignedVector3<double>;
using EiVector4 = Eigen::Vector4d;
using EiQuaternion = Eigen::Quaterniond;

inline EiVector to_eigen(Vector const& v) {
    return EiVector(v.x, v.y, v.z);
}

inline Vector from_eigen(EiVector const& v) {
    return Vector::From(v.x(), v.y(), v.z());
}

inline EiVector4 to_eigen(Vector4 const& v) {
    return EiVector4(v.x, v.y, v.z, v.w);
}

inline double DistanceToLine(Vector const& point, Vector const& p0, Vector const& dp) {
    double m = dp.Magnitude();
    return (to_eigen(point) - to_eigen(p0)).cross(to_eigen(dp)).norm() / m;
}


inline double DistanceToLineFromEndpoints(Vector const& point, Vector const& p0, Vector const& p1) {
    EiVector dp = to_eigen(p1) - to_eigen(p0);
    return (to_eigen(point) - to_eigen(p0)).cross(dp).norm() / dp.norm();
}

} // namespace SolveSpace
#endif
