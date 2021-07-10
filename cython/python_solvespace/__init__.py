# -*- coding: utf-8 -*-

"""'python_solvespace' module is a wrapper of
Python binding Solvespace solver libraries.
"""

__author__ = "Yuan Chang"
__copyright__ = "Copyright (C) 2016-2019"
__license__ = "GPLv3+"
__email__ = "pyslvs@gmail.com"
__version__ = "3.0.3.post1"

from enum import IntEnum, auto
from .slvs import (
    quaternion_u,
    quaternion_v,
    quaternion_n,
    make_quaternion,
    Params,
    Entity,
    SolverSystem,
)

__all__ = [
    'quaternion_u',
    'quaternion_v',
    'quaternion_n',
    'make_quaternion',
    'Constraint',
    'ResultFlag',
    'Params',
    'Entity',
    'SolverSystem',
]


class Constraint(IntEnum):
    # Expose macro of constraint types
    POINTS_COINCIDENT = 100000
    PT_PT_DISTANCE = auto()
    PT_PLANE_DISTANCE = auto()
    PT_LINE_DISTANCE = auto()
    PT_FACE_DISTANCE = auto()
    PT_IN_PLANE = auto()
    PT_ON_LINE = auto()
    PT_ON_FACE = auto()
    EQUAL_LENGTH_LINES = auto()
    LENGTH_RATIO = auto()
    EQ_LEN_PT_LINE_D = auto()
    EQ_PT_LN_DISTANCES = auto()
    EQUAL_ANGLE = auto()
    EQUAL_LINE_ARC_LEN = auto()
    SYMMETRIC = auto()
    SYMMETRIC_HORIZ = auto()
    SYMMETRIC_VERT = auto()
    SYMMETRIC_LINE = auto()
    AT_MIDPOINT = auto()
    HORIZONTAL = auto()
    VERTICAL = auto()
    DIAMETER = auto()
    PT_ON_CIRCLE = auto()
    SAME_ORIENTATION = auto()
    ANGLE = auto()
    PARALLEL = auto()
    PERPENDICULAR = auto()
    ARC_LINE_TANGENT = auto()
    CUBIC_LINE_TANGENT = auto()
    EQUAL_RADIUS = auto()
    PROJ_PT_DISTANCE = auto()
    WHERE_DRAGGED = auto()
    CURVE_CURVE_TANGENT = auto()
    LENGTH_DIFFERENCE = auto()


class ResultFlag(IntEnum):
    # Expose macro of result flags
    OKAY = 0
    INCONSISTENT = auto()
    DIDNT_CONVERGE = auto()
    TOO_MANY_UNKNOWNS = auto()
