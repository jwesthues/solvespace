# -*- coding: utf-8 -*-
# cython: language_level=3

"""Wrapper header of Solvespace.

author: Yuan Chang
copyright: Copyright (C) 2016-2019
license: GPLv3+
email: pyslvs@gmail.com
"""

from libc.stdint cimport uint32_t
from libcpp.vector cimport vector
from libcpp.map cimport map as cmap

cdef extern from "slvs.h" nogil:

    ctypedef uint32_t Slvs_hParam
    ctypedef uint32_t Slvs_hEntity
    ctypedef uint32_t Slvs_hConstraint
    ctypedef uint32_t Slvs_hGroup

    # Virtual work plane entity
    Slvs_hEntity SLVS_FREE_IN_3D

    ctypedef struct Slvs_Param:
        Slvs_hParam h
        Slvs_hGroup group
        double val

    # Entity type
    int SLVS_E_POINT_IN_3D
    int SLVS_E_POINT_IN_2D
    int SLVS_E_NORMAL_IN_2D
    int SLVS_E_NORMAL_IN_3D
    int SLVS_E_DISTANCE
    int SLVS_E_WORKPLANE
    int SLVS_E_LINE_SEGMENT
    int SLVS_E_CUBIC
    int SLVS_E_CIRCLE
    int SLVS_E_ARC_OF_CIRCLE

    ctypedef struct Slvs_Entity:
        Slvs_hEntity h
        Slvs_hGroup group
        int type
        Slvs_hEntity wrkpl
        Slvs_hEntity point[4]
        Slvs_hEntity normal
        Slvs_hEntity distance
        Slvs_hParam param[4]

    int SLVS_C_POINTS_COINCIDENT
    int SLVS_C_PT_PT_DISTANCE
    int SLVS_C_PT_PLANE_DISTANCE
    int SLVS_C_PT_LINE_DISTANCE
    int SLVS_C_PT_FACE_DISTANCE
    int SLVS_C_PT_IN_PLANE
    int SLVS_C_PT_ON_LINE
    int SLVS_C_PT_ON_FACE
    int SLVS_C_EQUAL_LENGTH_LINES
    int SLVS_C_LENGTH_RATIO
    int SLVS_C_EQ_LEN_PT_LINE_D
    int SLVS_C_EQ_PT_LN_DISTANCES
    int SLVS_C_EQUAL_ANGLE
    int SLVS_C_EQUAL_LINE_ARC_LEN
    int SLVS_C_SYMMETRIC
    int SLVS_C_SYMMETRIC_HORIZ
    int SLVS_C_SYMMETRIC_VERT
    int SLVS_C_SYMMETRIC_LINE
    int SLVS_C_AT_MIDPOINT
    int SLVS_C_HORIZONTAL
    int SLVS_C_VERTICAL
    int SLVS_C_DIAMETER
    int SLVS_C_PT_ON_CIRCLE
    int SLVS_C_SAME_ORIENTATION
    int SLVS_C_ANGLE
    int SLVS_C_PARALLEL
    int SLVS_C_PERPENDICULAR
    int SLVS_C_ARC_LINE_TANGENT
    int SLVS_C_CUBIC_LINE_TANGENT
    int SLVS_C_EQUAL_RADIUS
    int SLVS_C_PROJ_PT_DISTANCE
    int SLVS_C_WHERE_DRAGGED
    int SLVS_C_CURVE_CURVE_TANGENT
    int SLVS_C_LENGTH_DIFFERENCE

    ctypedef struct Slvs_Constraint:
        Slvs_hConstraint h
        Slvs_hGroup group
        int type
        Slvs_hEntity wrkpl
        double valA
        Slvs_hEntity ptA
        Slvs_hEntity ptB
        Slvs_hEntity entityA
        Slvs_hEntity entityB
        Slvs_hEntity entityC
        Slvs_hEntity entityD
        int other
        int other2

    ctypedef struct Slvs_System:
        Slvs_Param *param
        int params
        Slvs_Entity *entity
        int entities
        Slvs_Constraint *constraint
        int constraints
        Slvs_hParam dragged[4]
        int calculateFaileds
        Slvs_hConstraint *failed
        int faileds
        int dof
        int result

    void Slvs_Solve(Slvs_System *sys, Slvs_hGroup hg)
    void Slvs_QuaternionU(
        double qw, double qx, double qy, double qz,
        double *x, double *y, double *z
    )
    void Slvs_QuaternionV(
        double qw, double qx, double qy, double qz,
        double *x, double *y, double *z
    )
    void Slvs_QuaternionN(
        double qw, double qx, double qy, double qz,
        double *x, double *y, double *z
    )
    void Slvs_MakeQuaternion(
        double ux, double uy, double uz,
        double vx, double vy, double vz,
        double *qw, double *qx, double *qy, double *qz
    )
    Slvs_Param Slvs_MakeParam(Slvs_hParam h, Slvs_hGroup group, double val)
    Slvs_Entity Slvs_MakePoint2d(
        Slvs_hEntity h, Slvs_hGroup group,
        Slvs_hEntity wrkpl,
        Slvs_hParam u, Slvs_hParam v
    )
    Slvs_Entity Slvs_MakePoint3d(
        Slvs_hEntity h, Slvs_hGroup group,
        Slvs_hParam x, Slvs_hParam y, Slvs_hParam z
    )
    Slvs_Entity Slvs_MakeNormal3d(
        Slvs_hEntity h, Slvs_hGroup group,
        Slvs_hParam qw, Slvs_hParam qx,
        Slvs_hParam qy, Slvs_hParam qz
    )
    Slvs_Entity Slvs_MakeNormal2d(
        Slvs_hEntity h, Slvs_hGroup group,
        Slvs_hEntity wrkpl
    )
    Slvs_Entity Slvs_MakeDistance(
        Slvs_hEntity h, Slvs_hGroup group,
        Slvs_hEntity wrkpl, Slvs_hParam d
    )
    Slvs_Entity Slvs_MakeLineSegment(
        Slvs_hEntity h, Slvs_hGroup group,
        Slvs_hEntity wrkpl,
        Slvs_hEntity ptA, Slvs_hEntity ptB
    )
    Slvs_Entity Slvs_MakeCubic(
        Slvs_hEntity h, Slvs_hGroup group,
        Slvs_hEntity wrkpl,
        Slvs_hEntity pt0, Slvs_hEntity pt1,
        Slvs_hEntity pt2, Slvs_hEntity pt3
    )
    Slvs_Entity Slvs_MakeArcOfCircle(
        Slvs_hEntity h, Slvs_hGroup group,
        Slvs_hEntity wrkpl,
        Slvs_hEntity normal,
        Slvs_hEntity center,
        Slvs_hEntity start, Slvs_hEntity end
    )
    Slvs_Entity Slvs_MakeCircle(
        Slvs_hEntity h, Slvs_hGroup group,
        Slvs_hEntity wrkpl,
        Slvs_hEntity center,
        Slvs_hEntity normal, Slvs_hEntity radius
    )
    Slvs_Entity Slvs_MakeWorkplane(
        Slvs_hEntity h, Slvs_hGroup group,
        Slvs_hEntity origin, Slvs_hEntity normal
    )
    Slvs_Constraint Slvs_MakeConstraint(
        Slvs_hConstraint h,
        Slvs_hGroup group,
        int type,
        Slvs_hEntity wrkpl,
        double valA,
        Slvs_hEntity ptA,
        Slvs_hEntity ptB,
        Slvs_hEntity entityA,
        Slvs_hEntity entityB
    )

cpdef tuple quaternion_u(double qw, double qx, double qy, double qz)
cpdef tuple quaternion_v(double qw, double qx, double qy, double qz)
cpdef tuple quaternion_n(double qw, double qx, double qy, double qz)
cpdef tuple make_quaternion(double ux, double uy, double uz, double vx, double vy, double vz)

cdef class Params:

    cdef vector[Slvs_hParam] param_list

    @staticmethod
    cdef Params create(Slvs_hParam *p, size_t count)

cdef class Entity:

    cdef int t
    cdef Slvs_hEntity h, wp
    cdef Slvs_hGroup g
    cdef readonly Params params

    @staticmethod
    cdef Entity create(Slvs_Entity *e, size_t p_size)

    cpdef bint is_3d(self)
    cpdef bint is_none(self)
    cpdef bint is_point_2d(self)
    cpdef bint is_point_3d(self)
    cpdef bint is_point(self)
    cpdef bint is_normal_2d(self)
    cpdef bint is_normal_3d(self)
    cpdef bint is_normal(self)
    cpdef bint is_distance(self)
    cpdef bint is_work_plane(self)
    cpdef bint is_line_2d(self)
    cpdef bint is_line_3d(self)
    cpdef bint is_line(self)
    cpdef bint is_cubic(self)
    cpdef bint is_circle(self)
    cpdef bint is_arc(self)


cdef class SolverSystem:

    cdef Slvs_hGroup g
    cdef Slvs_System sys
    cdef cmap[Slvs_hParam, Slvs_Param] param_list
    cdef vector[Slvs_Entity] entity_list
    cdef vector[Slvs_Constraint] cons_list
    cdef vector[Slvs_hConstraint] failed_list

    cdef void copy_to_sys(self) nogil
    cdef void copy_from_sys(self) nogil
    cpdef void clear(self)
    cdef void failed_collecting(self) nogil
    cdef void free(self)
    cpdef void set_group(self, size_t g)
    cpdef int group(self)
    cpdef void set_params(self, Params p, object params)
    cpdef tuple params(self, Params p)
    cpdef int dof(self)
    cpdef object constraints(self)
    cpdef list failures(self)
    cpdef int solve(self)
    cpdef Entity create_2d_base(self)
    cdef Slvs_hParam new_param(self, double val) nogil
    cdef Slvs_hEntity eh(self) nogil

    cpdef Entity add_point_2d(self, double u, double v, Entity wp)
    cpdef Entity add_point_3d(self, double x, double y, double z)
    cpdef Entity add_normal_2d(self, Entity wp)
    cpdef Entity add_normal_3d(self, double qw, double qx, double qy, double qz)
    cpdef Entity add_distance(self, double d, Entity wp)
    cpdef Entity add_line_2d(self, Entity p1, Entity p2, Entity wp)
    cpdef Entity add_line_3d(self, Entity p1, Entity p2)
    cpdef Entity add_cubic(self, Entity p1, Entity p2, Entity p3, Entity p4, Entity wp)
    cpdef Entity add_arc(self, Entity nm, Entity ct, Entity start, Entity end, Entity wp)
    cpdef Entity add_circle(self, Entity nm, Entity ct, Entity radius, Entity wp)
    cpdef Entity add_work_plane(self, Entity origin, Entity nm)
    cpdef void add_constraint(
        self,
        int c_type,
        Entity wp,
        double v,
        Entity p1,
        Entity p2,
        Entity e1,
        Entity e2,
        Entity e3 = *,
        Entity e4 = *,
        int other = *,
        int other2 = *
    )

    cpdef void coincident(self, Entity e1, Entity e2, Entity wp = *)
    cpdef void distance(self, Entity e1, Entity e2, double value, Entity wp = *)
    cpdef void equal(self, Entity e1, Entity e2, Entity wp = *)
    cpdef void equal_included_angle(self, Entity e1, Entity e2, Entity e3, Entity e4, Entity wp)
    cpdef void equal_point_to_line(self, Entity e1, Entity e2, Entity e3, Entity e4, Entity wp)
    cpdef void ratio(self, Entity e1, Entity e2, double value, Entity wp)
    cpdef void symmetric(self, Entity e1, Entity e2, Entity e3 = *, Entity wp = *)
    cpdef void symmetric_h(self, Entity e1, Entity e2, Entity wp)
    cpdef void symmetric_v(self, Entity e1, Entity e2, Entity wp)
    cpdef void midpoint(self, Entity e1, Entity e2, Entity wp = *)
    cpdef void horizontal(self, Entity e1, Entity wp)
    cpdef void vertical(self, Entity e1, Entity wp)
    cpdef void diameter(self, Entity e1, double value, Entity wp)
    cpdef void same_orientation(self, Entity e1, Entity e2)
    cpdef void angle(self, Entity e1, Entity e2, double value, Entity wp, bint inverse = *)
    cpdef void perpendicular(self, Entity e1, Entity e2, Entity wp, bint inverse = *)
    cpdef void parallel(self, Entity e1, Entity e2, Entity wp = *)
    cpdef void tangent(self, Entity e1, Entity e2, Entity wp = *)
    cpdef void distance_proj(self, Entity e1, Entity e2, double value)
    cpdef void dragged(self, Entity e1, Entity wp = *)
