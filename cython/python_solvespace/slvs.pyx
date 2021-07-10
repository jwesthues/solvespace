# -*- coding: utf-8 -*-
# cython: language_level=3

"""Wrapper source code of Solvespace.

author: Yuan Chang
copyright: Copyright (C) 2016-2019
license: GPLv3+
email: pyslvs@gmail.com
"""

from cpython.mem cimport PyMem_Malloc, PyMem_Free
from cpython.object cimport Py_EQ, Py_NE
from libcpp.pair cimport pair
from collections import Counter


cpdef tuple quaternion_u(double qw, double qx, double qy, double qz):
    """Input quaternion, return unit vector of U axis.

    Where `qw`, `qx`, `qy`, `qz` are corresponded to the W, X, Y, Z value of
    quaternion.
    """
    cdef double x, y, z
    Slvs_QuaternionU(qw, qx, qy, qz, &x, &y, &z)
    return x, y, z


cpdef tuple quaternion_v(double qw, double qx, double qy, double qz):
    """Input quaternion, return unit vector of V axis.

    Signature is same as [quaternion_u](#quaternion_u).
    """
    cdef double x, y, z
    Slvs_QuaternionV(qw, qx, qy, qz, &x, &y, &z)
    return x, y, z


cpdef tuple quaternion_n(double qw, double qx, double qy, double qz):
    """Input quaternion, return unit vector of normal.

    Signature is same as [quaternion_u](#quaternion_u).
    """
    cdef double x, y, z
    Slvs_QuaternionN(qw, qx, qy, qz, &x, &y, &z)
    return x, y, z


cpdef tuple make_quaternion(double ux, double uy, double uz, double vx, double vy, double vz):
    """Input two unit vector, return quaternion.

    Where `ux`, `uy`, `uz` are corresponded to the value of U vector;
    `vx`, `vy`, `vz` are corresponded to the value of V vector.
    """
    cdef double qw, qx, qy, qz
    Slvs_MakeQuaternion(ux, uy, uz, vx, vy, vz, &qw, &qx, &qy, &qz)
    return qw, qx, qy, qz


cdef class Params:

    """Python object to handle multiple parameter handles."""

    @staticmethod
    cdef Params create(Slvs_hParam *p, size_t count):
        """Constructor."""
        cdef Params params = Params.__new__(Params)
        cdef size_t i
        for i in range(count):
            params.param_list.push_back(p[i])
        return params

    def __richcmp__(self, Params other, int op) -> bint:
        """Compare the parameters."""
        if op == Py_EQ:
            return self.param_list == other.param_list
        elif op == Py_NE:
            return self.param_list != other.param_list
        else:
            raise TypeError(
                f"'{op}' not support between instances of "
                f"{type(self)} and {type(other)}"
            )

    def __repr__(self) -> str:
        m = f"{type(self).__name__}(["
        cdef size_t i
        cdef size_t s = self.param_list.size()
        for i in range(s):
            m += str(<int>self.param_list[i])
            if i != s - 1:
                m += ", "
        m += "])"
        return m

# A virtual work plane that present 3D entity or constraint.
cdef Entity _E_FREE_IN_3D = Entity.__new__(Entity)
_E_FREE_IN_3D.t = SLVS_E_WORKPLANE
_E_FREE_IN_3D.h = SLVS_FREE_IN_3D
_E_FREE_IN_3D.g = 0
_E_FREE_IN_3D.params = Params.create(NULL, 0)

# A "None" entity used to fill in constraint option.
cdef Entity _E_NONE = Entity.__new__(Entity)
_E_NONE.t = 0
_E_NONE.h = 0
_E_NONE.g = 0
_E_NONE.params = Params.create(NULL, 0)

# Entity names
_NAME_OF_ENTITIES = {
    SLVS_E_POINT_IN_3D: "point 3d",
    SLVS_E_POINT_IN_2D: "point 2d",
    SLVS_E_NORMAL_IN_2D: "normal 2d",
    SLVS_E_NORMAL_IN_3D: "normal 3d",
    SLVS_E_DISTANCE: "distance",
    SLVS_E_WORKPLANE: "work plane",
    SLVS_E_LINE_SEGMENT: "line segment",
    SLVS_E_CUBIC: "cubic",
    SLVS_E_CIRCLE: "circle",
    SLVS_E_ARC_OF_CIRCLE: "arc",
}

# Constraint names
_NAME_OF_CONSTRAINTS = {
    SLVS_C_POINTS_COINCIDENT: "points coincident",
    SLVS_C_PT_PT_DISTANCE: "point point distance",
    SLVS_C_PT_PLANE_DISTANCE: "point plane distance",
    SLVS_C_PT_LINE_DISTANCE: "point line distance",
    SLVS_C_PT_FACE_DISTANCE: "point face distance",
    SLVS_C_PT_IN_PLANE: "point in plane",
    SLVS_C_PT_ON_LINE: "point on line",
    SLVS_C_PT_ON_FACE: "point on face",
    SLVS_C_EQUAL_LENGTH_LINES: "equal length lines",
    SLVS_C_LENGTH_RATIO: "length ratio",
    SLVS_C_EQ_LEN_PT_LINE_D: "equal length point line distance",
    SLVS_C_EQ_PT_LN_DISTANCES: "equal point line distance",
    SLVS_C_EQUAL_ANGLE: "equal angle",
    SLVS_C_EQUAL_LINE_ARC_LEN: "equal line arc length",
    SLVS_C_SYMMETRIC: "symmetric",
    SLVS_C_SYMMETRIC_HORIZ: "symmetric horizontal",
    SLVS_C_SYMMETRIC_VERT: "symmetric vertical",
    SLVS_C_SYMMETRIC_LINE: "symmetric line",
    SLVS_C_AT_MIDPOINT: "at midpoint",
    SLVS_C_HORIZONTAL: "horizontal",
    SLVS_C_VERTICAL: "vertical",
    SLVS_C_DIAMETER: "diameter",
    SLVS_C_PT_ON_CIRCLE: "point on circle",
    SLVS_C_SAME_ORIENTATION: "same orientation",
    SLVS_C_ANGLE: "angle",
    SLVS_C_PARALLEL: "parallel",
    SLVS_C_PERPENDICULAR: "perpendicular",
    SLVS_C_ARC_LINE_TANGENT: "arc line tangent",
    SLVS_C_CUBIC_LINE_TANGENT: "cubic line tangent",
    SLVS_C_EQUAL_RADIUS: "equal radius",
    SLVS_C_PROJ_PT_DISTANCE: "project point distance",
    SLVS_C_WHERE_DRAGGED: "where dragged",
    SLVS_C_CURVE_CURVE_TANGENT: "curve curve tangent",
    SLVS_C_LENGTH_DIFFERENCE: "length difference",
}


cdef class Entity:

    """The handles of entities."""

    FREE_IN_3D = _E_FREE_IN_3D
    NONE = _E_NONE

    @staticmethod
    cdef Entity create(Slvs_Entity *e, size_t p_size):
        """Constructor."""
        cdef Entity entity = Entity.__new__(Entity)
        with nogil:
            entity.t = e.type
            entity.h = e.h
            entity.wp = e.wrkpl
            entity.g = e.group
        entity.params = Params.create(e.param, p_size)
        return entity

    def __richcmp__(self, Entity other, int op) -> bint:
        """Compare the entities."""
        if op == Py_EQ:
            return (
                self.t == other.t and
                self.h == other.h and
                self.wp == other.wp and
                self.g == other.g and
                self.params == other.params
            )
        elif op == Py_NE:
            return not (self == other)
        else:
            raise TypeError(
                f"'{op}' not support between instances of "
                f"{type(self)} and {type(other)}"
            )

    cpdef bint is_3d(self):
        """Return True if this is a 3D entity."""
        return self.wp == SLVS_FREE_IN_3D

    cpdef bint is_none(self):
        """Return True if this is a empty entity."""
        return self.h == 0

    cpdef bint is_point_2d(self):
        """Return True if this is a 2D point."""
        return self.t == SLVS_E_POINT_IN_2D

    cpdef bint is_point_3d(self):
        """Return True if this is a 3D point."""
        return self.t == SLVS_E_POINT_IN_3D

    cpdef bint is_point(self):
        """Return True if this is a point."""
        return self.is_point_2d() or self.is_point_3d()

    cpdef bint is_normal_2d(self):
        """Return True if this is a 2D normal."""
        return self.t == SLVS_E_NORMAL_IN_2D

    cpdef bint is_normal_3d(self):
        """Return True if this is a 3D normal."""
        return self.t == SLVS_E_NORMAL_IN_3D

    cpdef bint is_normal(self):
        """Return True if this is a normal."""
        return self.is_normal_2d() or self.is_normal_3d()

    cpdef bint is_distance(self):
        """Return True if this is a distance."""
        return self.t == SLVS_E_DISTANCE

    cpdef bint is_work_plane(self):
        """Return True if this is a work plane."""
        return self.t == SLVS_E_WORKPLANE

    cpdef bint is_line_2d(self):
        """Return True if this is a 2D line."""
        return self.is_line() and not self.is_3d()

    cpdef bint is_line_3d(self):
        """Return True if this is a 3D line."""
        return self.is_line() and self.is_3d()

    cpdef bint is_line(self):
        """Return True if this is a line."""
        return self.t == SLVS_E_LINE_SEGMENT

    cpdef bint is_cubic(self):
        """Return True if this is a cubic."""
        return self.t == SLVS_E_CUBIC

    cpdef bint is_circle(self):
        """Return True if this is a circle."""
        return self.t == SLVS_E_CIRCLE

    cpdef bint is_arc(self):
        """Return True if this is a arc."""
        return self.t == SLVS_E_ARC_OF_CIRCLE

    def __repr__(self) -> str:
        cdef int h = <int>self.h
        cdef int g = <int>self.g
        cdef str t = _NAME_OF_ENTITIES[<int>self.t]
        return (
            f"{type(self).__name__}"
            f"(handle={h}, group={g}, type=<{t}>, is_3d={self.is_3d()}, params={self.params})"
        )


cdef class SolverSystem:

    """A solver system of Python-Solvespace.

    The operation of entities and constraints are using the methods of this
    class.
    """

    def __cinit__(self):
        self.g = 0
        self.sys.params = self.sys.entities = self.sys.constraints = 0

    def __dealloc__(self):
        self.free()

    cdef inline void copy_to_sys(self) nogil:
        """Copy data from stack into system."""
        cdef int i = 0
        cdef pair[Slvs_hParam, Slvs_Param] param
        for param in self.param_list:
            self.sys.param[i] = param.second
            i += 1

        i = 0
        cdef Slvs_Entity entity
        for entity in self.entity_list:
            self.sys.entity[i] = entity
            i += 1

        i = 0
        cdef Slvs_Constraint con
        for con in self.cons_list:
            self.sys.constraint[i] = con
            i += 1

    cdef inline void copy_from_sys(self) nogil:
        """Copy data from system into stack."""
        self.param_list.clear()
        self.entity_list.clear()
        self.cons_list.clear()
        cdef int i
        for i in range(self.sys.params):
            self.param_list[self.sys.param[i].h] = self.sys.param[i]
        for i in range(self.sys.entities):
            self.entity_list.push_back(self.sys.entity[i])
        for i in range(self.sys.constraints):
            self.cons_list.push_back(self.sys.constraint[i])

    cpdef void clear(self):
        """Clear the system."""
        self.g = 0
        self.param_list.clear()
        self.entity_list.clear()
        self.cons_list.clear()
        self.failed_list.clear()
        self.free()

    cdef inline void failed_collecting(self) nogil:
        """Collecting the failed constraints."""
        cdef int i
        for i in range(self.sys.faileds):
            self.failed_list.push_back(self.sys.failed[i])

    cdef inline void free(self):
        PyMem_Free(self.sys.param)
        PyMem_Free(self.sys.entity)
        PyMem_Free(self.sys.constraint)
        PyMem_Free(self.sys.failed)
        self.sys.param = NULL
        self.sys.entity = NULL
        self.sys.constraint = NULL
        self.sys.failed = NULL
        self.sys.params = self.sys.entities = self.sys.constraints = 0

    cpdef void set_group(self, size_t g):
        """Set the current group (`g`)."""
        self.g = <Slvs_hGroup>g

    cpdef int group(self):
        """Return the current group."""
        return <int>self.g

    cpdef void set_params(self, Params p, object params):
        """Set the parameters from a [Params] handle (`p`) belong to this
        system.
        The values is come from `params`, length must be equal to the handle.
        """
        params = tuple(params)
        cdef int i = p.param_list.size()
        if i != len(params):
            raise ValueError(f"number of parameters {len(params)} are not match {i}")

        i = 0
        cdef Slvs_hParam h
        for h in p.param_list:
            self.param_list[h].val = params[i]
            i += 1

    cpdef tuple params(self, Params p):
        """Get the parameters from a [Params] handle (`p`) belong to this
        system.
        The length of tuple is decided by handle.
        """
        param_list = []
        cdef Slvs_hParam h
        for h in p.param_list:
            param_list.append(self.param_list[h].val)
        return tuple(param_list)

    cpdef int dof(self):
        """Return the degrees of freedom of current group.
        Only can be called after solving.
        """
        return self.sys.dof

    cpdef object constraints(self):
        """Return the number of each constraint type.
        The name of constraints is represented by string.
        """
        cons_list = []
        cdef Slvs_Constraint con
        for con in self.cons_list:
            cons_list.append(_NAME_OF_CONSTRAINTS[con.type])
        return Counter(cons_list)

    cpdef list failures(self):
        """Return a list of failed constraint numbers."""
        failed_list = []
        cdef Slvs_hConstraint error
        for error in self.failed_list:
            failed_list.append(<int>error)
        return failed_list

    cpdef int solve(self):
        """Start the solving, return the result flag."""
        # Parameters
        self.sys.param = <Slvs_Param *>PyMem_Malloc(self.param_list.size() * sizeof(Slvs_Param))
        # Entities
        self.sys.entity = <Slvs_Entity *>PyMem_Malloc(self.entity_list.size() * sizeof(Slvs_Entity))
        # Constraints
        cdef size_t cons_size = self.cons_list.size()
        self.sys.constraint = <Slvs_Constraint *>PyMem_Malloc(cons_size * sizeof(Slvs_Constraint))
        self.sys.failed = <Slvs_hConstraint *>PyMem_Malloc(cons_size * sizeof(Slvs_hConstraint))
        self.sys.faileds = cons_size

        # Copy to system
        self.copy_to_sys()
        # Solve
        Slvs_Solve(&self.sys, self.g)
        # Failed constraints and free memory.
        self.copy_from_sys()
        self.failed_collecting()
        self.free()
        return self.sys.result

    cpdef Entity create_2d_base(self):
        """Create a 2D system on current group,
        return the handle of work plane.
        """
        cdef double qw, qx, qy, qz
        qw, qx, qy, qz = make_quaternion(1, 0, 0, 0, 1, 0)
        cdef Entity nm = self.add_normal_3d(qw, qx, qy, qz)
        return self.add_work_plane(self.add_point_3d(0, 0, 0), nm)

    cdef inline Slvs_hParam new_param(self, double val) nogil:
        """Add a parameter."""
        self.sys.params += 1
        cdef Slvs_hParam h = <Slvs_hParam>self.sys.params
        self.param_list[h] = Slvs_MakeParam(h, self.g, val)
        return h

    cdef inline Slvs_hEntity eh(self) nogil:
        """Return new entity handle."""
        self.sys.entities += 1
        return <Slvs_hEntity>self.sys.entities

    cpdef Entity add_point_2d(self, double u, double v, Entity wp):
        """Add a 2D point to specific work plane (`wp`) then return the handle.

        Where `u`, `v` are corresponded to the value of U, V axis on the work
        plane.
        """
        if wp is None or not wp.is_work_plane():
            raise TypeError(f"{wp} is not a work plane")

        cdef Slvs_hParam u_p = self.new_param(u)
        cdef Slvs_hParam v_p = self.new_param(v)
        cdef Slvs_Entity e = Slvs_MakePoint2d(self.eh(), self.g, wp.h, u_p, v_p)
        self.entity_list.push_back(e)

        return Entity.create(&e, 2)

    cpdef Entity add_point_3d(self, double x, double y, double z):
        """Add a 3D point then return the handle.

        Where `x`, `y`, `z` are corresponded to the value of X, Y, Z axis.
        """
        cdef Slvs_hParam x_p = self.new_param(x)
        cdef Slvs_hParam y_p = self.new_param(y)
        cdef Slvs_hParam z_p = self.new_param(z)
        cdef Slvs_Entity e = Slvs_MakePoint3d(self.eh(), self.g, x_p, y_p, z_p)
        self.entity_list.push_back(e)

        return Entity.create(&e, 3)

    cpdef Entity add_normal_2d(self, Entity wp):
        """Add a 2D normal orthogonal to specific work plane (`wp`)
        then return the handle.
        """
        if wp is None or not wp.is_work_plane():
            raise TypeError(f"{wp} is not a work plane")
        cdef Slvs_Entity e = Slvs_MakeNormal2d(self.eh(), self.g, wp.h)
        self.entity_list.push_back(e)
        return Entity.create(&e, 0)

    cpdef Entity add_normal_3d(self, double qw, double qx, double qy, double qz):
        """Add a 3D normal from quaternion then return the handle.

        Where `qw`, `qx`, `qy`, `qz` are corresponded to
        the W, X, Y, Z value of quaternion.
        """
        cdef Slvs_hParam w_p = self.new_param(qw)
        cdef Slvs_hParam x_p = self.new_param(qx)
        cdef Slvs_hParam y_p = self.new_param(qy)
        cdef Slvs_hParam z_p = self.new_param(qz)
        self.entity_list.push_back(Slvs_MakeNormal3d(
            self.eh(), self.g, w_p, x_p, y_p, z_p))
        return Entity.create(&self.entity_list.back(), 4)

    cpdef Entity add_distance(self, double d, Entity wp):
        """Add a distance to specific work plane (`wp`) then return the handle.

        Where `d` is distance value.
        """
        if wp is None or not wp.is_work_plane():
            raise TypeError(f"{wp} is not a work plane")

        cdef Slvs_hParam d_p = self.new_param(d)
        self.entity_list.push_back(Slvs_MakeDistance(
            self.eh(), self.g, wp.h, d_p))
        return Entity.create(&self.entity_list.back(), 1)

    cpdef Entity add_line_2d(self, Entity p1, Entity p2, Entity wp):
        """Add a 2D line to specific work plane (`wp`) then return the handle.

        Where `p1` is the start point; `p2` is the end point.
        """
        if wp is None or not wp.is_work_plane():
            raise TypeError(f"{wp} is not a work plane")
        if p1 is None or not p1.is_point_2d():
            raise TypeError(f"{p1} is not a 2d point")
        if p2 is None or not p2.is_point_2d():
            raise TypeError(f"{p2} is not a 2d point")

        self.entity_list.push_back(Slvs_MakeLineSegment(
            self.eh(), self.g, wp.h, p1.h, p2.h))
        return Entity.create(&self.entity_list.back(), 0)

    cpdef Entity add_line_3d(self, Entity p1, Entity p2):
        """Add a 3D line then return the handle.

        Where `p1` is the start point; `p2` is the end point.
        """
        if p1 is None or not p1.is_point_3d():
            raise TypeError(f"{p1} is not a 3d point")
        if p2 is None or not p2.is_point_3d():
            raise TypeError(f"{p2} is not a 3d point")

        self.entity_list.push_back(Slvs_MakeLineSegment(
            self.eh(), self.g, SLVS_FREE_IN_3D, p1.h, p2.h))
        return Entity.create(&self.entity_list.back(), 0)

    cpdef Entity add_cubic(self, Entity p1, Entity p2, Entity p3, Entity p4, Entity wp):
        """Add a cubic curve to specific work plane (`wp`) then return the
        handle.

        Where `p1` to `p4` is the control points.
        """
        if wp is None or not wp.is_work_plane():
            raise TypeError(f"{wp} is not a work plane")
        if p1 is None or not p1.is_point_2d():
            raise TypeError(f"{p1} is not a 2d point")
        if p2 is None or not p2.is_point_2d():
            raise TypeError(f"{p2} is not a 2d point")
        if p3 is None or not p3.is_point_2d():
            raise TypeError(f"{p3} is not a 2d point")
        if p4 is None or not p4.is_point_2d():
            raise TypeError(f"{p4} is not a 2d point")

        self.entity_list.push_back(Slvs_MakeCubic(
            self.eh(), self.g, wp.h, p1.h, p2.h, p3.h, p4.h))
        return Entity.create(&self.entity_list.back(), 0)

    cpdef Entity add_arc(self, Entity nm, Entity ct, Entity start, Entity end, Entity wp):
        """Add an arc to specific work plane (`wp`) then return the handle.

        Where `nm` is the orthogonal normal; `ct` is the center point;
        `start` is the start point; `end` is the end point.
        """
        if wp is None or not wp.is_work_plane():
            raise TypeError(f"{wp} is not a work plane")
        if nm is None or not nm.is_normal_3d():
            raise TypeError(f"{nm} is not a 3d normal")
        if ct is None or not ct.is_point_2d():
            raise TypeError(f"{ct} is not a 2d point")
        if start is None or not start.is_point_2d():
            raise TypeError(f"{start} is not a 2d point")
        if end is None or not end.is_point_2d():
            raise TypeError(f"{end} is not a 2d point")
        self.entity_list.push_back(Slvs_MakeArcOfCircle(
            self.eh(), self.g, wp.h, nm.h, ct.h, start.h, end.h))
        return Entity.create(&self.entity_list.back(), 0)

    cpdef Entity add_circle(self, Entity nm, Entity ct, Entity radius, Entity wp):
        """Add an circle to specific work plane (`wp`) then return the handle.

        Where `nm` is the orthogonal normal;
        `ct` is the center point;
        `radius` is the distance value represent radius.
        """
        if wp is None or not wp.is_work_plane():
            raise TypeError(f"{wp} is not a work plane")
        if nm is None or not nm.is_normal_3d():
            raise TypeError(f"{nm} is not a 3d normal")
        if ct is None or not ct.is_point_2d():
            raise TypeError(f"{ct} is not a 2d point")
        if radius is None or not radius.is_distance():
            raise TypeError(f"{radius} is not a distance")

        self.entity_list.push_back(Slvs_MakeCircle(self.eh(), self.g, wp.h,
                                                   ct.h, nm.h, radius.h))
        return Entity.create(&self.entity_list.back(), 0)

    cpdef Entity add_work_plane(self, Entity origin, Entity nm):
        """Add a work plane then return the handle.

        Where `origin` is the origin point of the plane;
        `nm` is the orthogonal normal.
        """
        if origin is None or origin.t != SLVS_E_POINT_IN_3D:
            raise TypeError(f"{origin} is not a 3d point")
        if nm is None or nm.t != SLVS_E_NORMAL_IN_3D:
            raise TypeError(f"{nm} is not a 3d normal")

        self.entity_list.push_back(Slvs_MakeWorkplane(self.eh(), self.g, origin.h, nm.h))
        return Entity.create(&self.entity_list.back(), 0)

    cpdef void add_constraint(
        self,
        int c_type,
        Entity wp,
        double v,
        Entity p1,
        Entity p2,
        Entity e1,
        Entity e2,
        Entity e3 = _E_NONE,
        Entity e4 = _E_NONE,
        int other = 0,
        int other2 = 0
    ):
        """Add a constraint by type code `c_type`.
        This is an origin function mapping to different constraint methods.

        Where `wp` represents work plane; `v` represents constraint value;
        `p1` and `p2` represent point entities; `e1` to `e4` represent other
        types of entity;
        `other` and `other2` are control options of the constraint.
        """
        if wp is None or not wp.is_work_plane():
            raise TypeError(f"{wp} is not a work plane")

        cdef Entity e
        for e in (p1, p2):
            if e is None or not (e.is_none() or e.is_point()):
                raise TypeError(f"{e} is not a point")
        for e in (e1, e2, e3, e4):
            if e is None:
                raise TypeError(f"{e} is not a entity")

        self.sys.constraints += 1
        cdef Slvs_Constraint c
        c.h = <Slvs_hConstraint>self.sys.constraints
        c.group = self.g
        c.type = c_type
        c.wrkpl = wp.h
        c.valA = v
        c.ptA = p1.h
        c.ptB = p2.h
        c.entityA = e1.h
        c.entityB = e2.h
        c.entityC = e3.h
        c.entityD = e4.h
        c.other = other
        c.other2 = other2
        self.cons_list.push_back(c)

    #####
    # Constraint methods.
    #####

    cpdef void coincident(self, Entity e1, Entity e2, Entity wp = _E_FREE_IN_3D):
        """Coincident two entities.

        | Entity 1 (`e1`) | Entity 2 (`e2`) | Work plane (`wp`) |
        |:---------------:|:---------------:|:-----------------:|
        | [is_point] | [is_point] | Optional |
        | [is_point] | [is_work_plane] | [Entity.FREE_IN_3D] |
        | [is_point] | [is_line] | Optional |
        | [is_point] | [is_circle] | Optional |
        """
        cdef int t
        if e1.is_point() and e2.is_point():
            self.add_constraint(SLVS_C_POINTS_COINCIDENT, wp, 0., e1, e2,
                                _E_NONE, _E_NONE)
        elif e1.is_point() and e2.is_work_plane() and wp is _E_FREE_IN_3D:
            self.add_constraint(SLVS_C_PT_IN_PLANE, e2, 0., e1, _E_NONE, e2,
                                _E_NONE)
        elif e1.is_point() and (e2.is_line() or e2.is_circle()):
            if e2.is_line():
                t = SLVS_C_PT_ON_LINE
            else:
                t = SLVS_C_PT_ON_CIRCLE
            self.add_constraint(t, wp, 0., e1, _E_NONE, e2, _E_NONE)
        else:
            raise TypeError(f"unsupported entities: {e1}, {e2}, {wp}")

    cpdef void distance(
        self,
        Entity e1,
        Entity e2,
        double value,
        Entity wp = _E_FREE_IN_3D
    ):
        """Distance constraint between two entities.

        If `value` is equal to zero, then turn into
        [coincident](#solversystemcoincident)

        | Entity 1 (`e1`) | Entity 2 (`e2`) | Work plane (`wp`) |
        |:---------------:|:---------------:|:-----------------:|
        | [is_point] | [is_point] | Optional |
        | [is_point] | [is_work_plane] | [Entity.FREE_IN_3D] |
        | [is_point] | [is_line] | Optional |
        """
        if value == 0.:
            self.coincident(e1, e2, wp)
            return
        if e1.is_point() and e2.is_point():
            self.add_constraint(SLVS_C_PT_PT_DISTANCE, wp, value, e1, e2,
                                _E_NONE, _E_NONE)
        elif e1.is_point() and e2.is_work_plane() and wp is _E_FREE_IN_3D:
            self.add_constraint(SLVS_C_PT_PLANE_DISTANCE, e2, value, e1,
                                _E_NONE, e2, _E_NONE)
        elif e1.is_point() and e2.is_line():
            self.add_constraint(SLVS_C_PT_LINE_DISTANCE, wp, value, e1,
                                _E_NONE, e2, _E_NONE)
        else:
            raise TypeError(f"unsupported entities: {e1}, {e2}, {wp}")

    cpdef void equal(self, Entity e1, Entity e2, Entity wp = _E_FREE_IN_3D):
        """Equal constraint between two entities.

       | Entity 1 (`e1`) | Entity 2 (`e2`) | Work plane (`wp`) |
       |:---------------:|:---------------:|:-----------------:|
       | [is_line] | [is_line] | Optional |
       | [is_line] | [is_arc] | Optional |
       | [is_line] | [is_circle] | Optional |
       | [is_arc] | [is_arc] | Optional |
       | [is_arc] | [is_circle] | Optional |
       | [is_circle] | [is_circle] | Optional |
       | [is_circle] | [is_arc] | Optional |
       """
        if e1.is_line() and e2.is_line():
            self.add_constraint(SLVS_C_EQUAL_LENGTH_LINES, wp, 0., _E_NONE,
                                _E_NONE, e1, e2)
        elif e1.is_line() and (e2.is_arc() or e2.is_circle()):
            self.add_constraint(SLVS_C_EQUAL_LINE_ARC_LEN, wp, 0., _E_NONE,
                                _E_NONE, e1, e2)
        elif (e1.is_arc() or e1.is_circle()) and (e2.is_arc() or e2.is_circle()):
            self.add_constraint(SLVS_C_EQUAL_RADIUS, wp, 0., _E_NONE, _E_NONE, e1, e2)
        else:
            raise TypeError(f"unsupported entities: {e1}, {e2}, {wp}")

    cpdef void equal_included_angle(
        self,
        Entity e1,
        Entity e2,
        Entity e3,
        Entity e4,
        Entity wp
    ):
        """Constraint that 2D line 1 (`e1`) and line 2 (`e2`),
        line 3 (`e3`) and line 4 (`e4`) must have same included angle on work
        plane `wp`.
        """
        if wp is _E_FREE_IN_3D:
            raise ValueError("this is a 2d constraint")
        if e1.is_line_2d() and e2.is_line_2d() and e3.is_line_2d() and e4.is_line_2d():
            self.add_constraint(SLVS_C_EQUAL_ANGLE, wp, 0., _E_NONE, _E_NONE,
                                e1, e2, e3, e4)
        else:
            raise TypeError(f"unsupported entities: {e1}, {e2}, {e3}, {e4}, {wp}")

    cpdef void equal_point_to_line(
        self,
        Entity e1,
        Entity e2,
        Entity e3,
        Entity e4,
        Entity wp
    ):
        """Constraint that point 1 (`e1`) and line 1 (`e2`),
        point 2 (`e3`) and line 2  (`e4`) must have same distance on work
        plane `wp`.
        """
        if wp is _E_FREE_IN_3D:
            raise ValueError("this is a 2d constraint")
        if e1.is_point_2d() and e2.is_line_2d() and e3.is_point_2d() and e4.is_line_2d():
            self.add_constraint(SLVS_C_EQ_PT_LN_DISTANCES, wp, 0., e1, e3, e2, e4)
        else:
            raise TypeError(f"unsupported entities: {e1}, {e2}, {e3}, {e4}, {wp}")

    cpdef void ratio(self, Entity e1, Entity e2, double value, Entity wp):
        """The ratio (`value`) constraint between two 2D lines (`e1` and
        `e2`).
        """
        if wp is _E_FREE_IN_3D:
            raise ValueError("this is a 2d constraint")
        if e1.is_line_2d() and e2.is_line_2d():
            self.add_constraint(SLVS_C_LENGTH_RATIO, wp, value, _E_NONE, _E_NONE, e1, e2)
        else:
            raise TypeError(f"unsupported entities: {e1}, {e2}, {wp}")

    cpdef void symmetric(
        self,
        Entity e1,
        Entity e2,
        Entity e3 = _E_NONE,
        Entity wp = _E_FREE_IN_3D
    ):
        """Symmetric constraint between two points.

        | Entity 1 (`e1`) | Entity 2 (`e2`) | Entity 3 (`e3`) | Work plane (`wp`) |
        |:---------------:|:---------------:|:---------------:|:-----------------:|
        | [is_point_3d] | [is_point_3d] | [is_work_plane] | [Entity.FREE_IN_3D] |
        | [is_point_2d] | [is_point_2d] | [is_work_plane] | [Entity.FREE_IN_3D] |
        | [is_point_2d] | [is_point_2d] | [is_line_2d] | Is not [Entity.FREE_IN_3D] |
        """
        if e1.is_point_3d() and e2.is_point_3d() and e3.is_work_plane() and wp is _E_FREE_IN_3D:
            self.add_constraint(SLVS_C_SYMMETRIC, wp, 0., e1, e2, e3, _E_NONE)
        elif e1.is_point_2d() and e2.is_point_2d() and e3.is_work_plane() and wp is _E_FREE_IN_3D:
            self.add_constraint(SLVS_C_SYMMETRIC, e3, 0., e1, e2, e3, _E_NONE)
        elif e1.is_point_2d() and e2.is_point_2d() and e3.is_line_2d():
            if wp is _E_FREE_IN_3D:
                raise ValueError("this is a 2d constraint")
            self.add_constraint(SLVS_C_SYMMETRIC_LINE, wp, 0., e1, e2, e3, _E_NONE)
        else:
            raise TypeError(f"unsupported entities: {e1}, {e2}, {e3}, {wp}")

    cpdef void symmetric_h(self, Entity e1, Entity e2, Entity wp):
        """Symmetric constraint between two 2D points (`e1` and `e2`)
        with horizontal line on the work plane (`wp` can not be
        [Entity.FREE_IN_3D]).
        """
        if wp is _E_FREE_IN_3D:
            raise ValueError("this is a 2d constraint")
        if e1.is_point_2d() and e2.is_point_2d():
            self.add_constraint(SLVS_C_SYMMETRIC_HORIZ, wp, 0., e1, e2, _E_NONE, _E_NONE)
        else:
            raise TypeError(f"unsupported entities: {e1}, {e2}, {wp}")

    cpdef void symmetric_v(self, Entity e1, Entity e2, Entity wp):
        """Symmetric constraint between two 2D points (`e1` and `e2`)
        with vertical line on the work plane (`wp` can not be
        [Entity.FREE_IN_3D]).
        """
        if wp is _E_FREE_IN_3D:
            raise ValueError("this is a 2d constraint")
        if e1.is_point_2d() and e2.is_point_2d():
            self.add_constraint(SLVS_C_SYMMETRIC_VERT, wp, 0., e1, e2, _E_NONE, _E_NONE)
        else:
            raise TypeError(f"unsupported entities: {e1}, {e2}, {wp}")

    cpdef void midpoint(
        self,
        Entity e1,
        Entity e2,
        Entity wp = _E_FREE_IN_3D
    ):
        """Midpoint constraint between a point (`e1`) and
        a line (`e2`) on work plane (`wp`).
        """
        if e1.is_point() and e2.is_line():
            self.add_constraint(SLVS_C_AT_MIDPOINT, wp, 0., e1, _E_NONE, e2, _E_NONE)
        else:
            raise TypeError(f"unsupported entities: {e1}, {e2}, {wp}")

    cpdef void horizontal(self, Entity e1, Entity wp):
        """Vertical constraint of a 2d point (`e1`) on
        work plane (`wp` can not be [Entity.FREE_IN_3D]).
        """
        if wp is _E_FREE_IN_3D:
            raise ValueError("this is a 2d constraint")
        if e1.is_line_2d():
            self.add_constraint(SLVS_C_HORIZONTAL, wp, 0., _E_NONE, _E_NONE, e1, _E_NONE)
        else:
            raise TypeError(f"unsupported entities: {e1}, {wp}")

    cpdef void vertical(self, Entity e1, Entity wp):
        """Vertical constraint of a 2d point (`e1`) on
        work plane (`wp` can not be [Entity.FREE_IN_3D]).
        """
        if wp is _E_FREE_IN_3D:
            raise ValueError("this is a 2d constraint")
        if e1.is_line_2d():
            self.add_constraint(SLVS_C_VERTICAL, wp, 0., _E_NONE, _E_NONE, e1, _E_NONE)
        else:
            raise TypeError(f"unsupported entities: {e1}, {wp}")

    cpdef void diameter(self, Entity e1, double value, Entity wp):
        """Diameter (`value`) constraint of a circular entities.

        | Entity 1 (`e1`) | Work plane (`wp`) |
        |:---------------:|:-----------------:|
        | [is_arc] | Optional |
        | [is_circle] | Optional |
        """
        if wp is _E_FREE_IN_3D:
            raise ValueError("this is a 2d constraint")
        if e1.is_arc() or e1.is_circle():
            self.add_constraint(SLVS_C_DIAMETER, wp, value, _E_NONE, _E_NONE,
                                e1, _E_NONE)
        else:
            raise TypeError(f"unsupported entities: {e1}, {wp}")

    cpdef void same_orientation(self, Entity e1, Entity e2):
        """Equal orientation constraint between two 3d normals (`e1` and
        `e2`).
        """
        if e1.is_normal_3d() and e2.is_normal_3d():
            self.add_constraint(SLVS_C_SAME_ORIENTATION, _E_FREE_IN_3D, 0.,
                                _E_NONE, _E_NONE, e1, e2)
        else:
            raise TypeError(f"unsupported entities: {e1}, {e2}")

    cpdef void angle(self, Entity e1, Entity e2, double value, Entity wp, bint inverse = False):
        """Degrees angle (`value`) constraint between two 2d lines (`e1` and
        `e2`) on the work plane (`wp` can not be [Entity.FREE_IN_3D]).
        """
        if wp is _E_FREE_IN_3D:
            raise ValueError("this is a 2d constraint")
        if e1.is_line_2d() and e2.is_line_2d():
            self.add_constraint(SLVS_C_ANGLE, wp, value, _E_NONE, _E_NONE,
                                e1, e2, _E_NONE, _E_NONE, inverse)
        else:
            raise TypeError(f"unsupported entities: {e1}, {e2}, {wp}")

    cpdef void perpendicular(self, Entity e1, Entity e2, Entity wp, bint inverse = False):
        """Perpendicular constraint between two 2d lines (`e1` and `e2`)
        on the work plane (`wp` can not be [Entity.FREE_IN_3D]) with
        `inverse` option.
        """
        if wp is _E_FREE_IN_3D:
            raise ValueError("this is a 2d constraint")
        if e1.is_line_2d() and e2.is_line_2d():
            self.add_constraint(SLVS_C_PERPENDICULAR, wp, 0., _E_NONE, _E_NONE,
                                e1, e2, _E_NONE, _E_NONE, inverse)
        else:
            raise TypeError(f"unsupported entities: {e1}, {e2}, {wp}")

    cpdef void parallel(self, Entity e1, Entity e2, Entity wp = _E_FREE_IN_3D):
        """Parallel constraint between two lines (`e1` and `e2`) on
        the work plane (`wp`).
        """
        if e1.is_line() and e2.is_line():
            self.add_constraint(SLVS_C_PARALLEL, wp, 0., _E_NONE, _E_NONE, e1, e2)
        else:
            raise TypeError(f"unsupported entities: {e1}, {e2}, {wp}")

    cpdef void tangent(self, Entity e1, Entity e2, Entity wp = _E_FREE_IN_3D):
        """Parallel constraint between two entities (`e1` and `e2`) on the
        work plane (`wp`).

        | Entity 1 (`e1`) | Entity 2 (`e2`) | Work plane (`wp`) |
        |:---------------:|:---------------:|:-----------------:|
        | [is_arc] | [is_line_2d] | Is not [Entity.FREE_IN_3D] |
        | [is_cubic] | [is_line_3d] | [Entity.FREE_IN_3D] |
        | [is_arc] | [is_cubic] | Is not [Entity.FREE_IN_3D] |
        | [is_arc] | [is_arc] | Is not [Entity.FREE_IN_3D] |
        | [is_cubic] | [is_cubic] | Optional |
        """
        if e1.is_arc() and e2.is_line_2d():
            if wp is _E_FREE_IN_3D:
                raise ValueError("this is a 2d constraint")
            self.add_constraint(SLVS_C_ARC_LINE_TANGENT, wp, 0., _E_NONE, _E_NONE, e1, e2)
        elif e1.is_cubic() and e2.is_line_3d() and wp is _E_FREE_IN_3D:
            self.add_constraint(SLVS_C_CUBIC_LINE_TANGENT, wp, 0., _E_NONE, _E_NONE, e1, e2)
        elif (e1.is_arc() or e1.is_cubic()) and (e2.is_arc() or e2.is_cubic()):
            if (e1.is_arc() or e2.is_arc()) and wp is _E_FREE_IN_3D:
                raise ValueError("this is a 2d constraint")
            self.add_constraint(SLVS_C_CURVE_CURVE_TANGENT, wp, 0., _E_NONE, _E_NONE, e1, e2)
        else:
            raise TypeError(f"unsupported entities: {e1}, {e2}, {wp}")

    cpdef void distance_proj(self, Entity e1, Entity e2, double value):
        """Projected distance (`value`) constraint between
        two 3d points (`e1` and `e2`).
        """
        if e1.is_point_3d() and e2.is_point_3d():
            self.add_constraint(SLVS_C_CURVE_CURVE_TANGENT, _E_FREE_IN_3D,
                                value, e1, e2, _E_NONE, _E_NONE)
        else:
            raise TypeError(f"unsupported entities: {e1}, {e2}")

    cpdef void dragged(self, Entity e1, Entity wp = _E_FREE_IN_3D):
        """Dragged constraint of a point (`e1`) on the work plane (`wp`)."""
        if e1.is_point():
            self.add_constraint(SLVS_C_WHERE_DRAGGED, wp, 0., e1, _E_NONE, _E_NONE, _E_NONE)
        else:
            raise TypeError(f"unsupported entities: {e1}, {wp}")
