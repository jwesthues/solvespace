# -*- coding: utf-8 -*-

from typing import Tuple, List, Sequence, Counter, ClassVar

def quaternion_u(
    qw: float,
    qx: float,
    qy: float,
    qz: float
) -> Tuple[float, float, float]:
    ...

def quaternion_v(
    qw: float,
    qx: float,
    qy: float,
    qz: float
) -> Tuple[float, float, float]:
    ...

def quaternion_n(
    qw: float,
    qx: float,
    qy: float,
    qz: float
) -> Tuple[float, float, float]:
    ...

def make_quaternion(
    ux: float,
    uy: float,
    uz: float,
    vx: float,
    vy: float,
    vz: float
) -> Tuple[float, float, float, float]:
    ...

class Params:
    pass

class Entity:

    FREE_IN_3D: ClassVar[Entity] = ...
    NONE: ClassVar[Entity] = ...
    params: Params

    def is_3d(self) -> bool:
        ...

    def is_none(self) -> bool:
        ...

    def is_point_2d(self) -> bool:
        ...

    def is_point_3d(self) -> bool:
        ...

    def is_point(self) -> bool:
        ...

    def is_normal_2d(self) -> bool:
        ...

    def is_normal_3d(self) -> bool:
        ...

    def is_normal(self) -> bool:
        ...

    def is_distance(self) -> bool:
        ...

    def is_work_plane(self) -> bool:
        ...

    def is_line_2d(self) -> bool:
        ...

    def is_line_3d(self) -> bool:
        ...

    def is_line(self) -> bool:
        ...

    def is_cubic(self) -> bool:
        ...

    def is_circle(self) -> bool:
        ...

    def is_arc(self) -> bool:
        ...

class SolverSystem:

    def __init__(self) -> None:
        """Initialization method. Create a solver system."""
        ...

    def clear(self) -> None:
        ...

    def set_group(self, g: int) -> None:
        ...

    def group(self) -> int:
        ...

    def set_params(self, p: Params, params: Sequence[float]) -> None:
        ...

    def params(self, p: Params) -> Tuple[float, ...]:
        ...

    def dof(self) -> int:
        ...

    def constraints(self) -> Counter[str]:
        ...

    def failures(self) -> List[int]:
        ...

    def solve(self) -> int:
        ...

    def create_2d_base(self) -> Entity:
        ...

    def add_point_2d(self, u: float, v: float, wp: Entity) -> Entity:
        ...

    def add_point_3d(self, x: float, y: float, z: float) -> Entity:
        ...

    def add_normal_2d(self, wp: Entity) -> Entity:
        ...

    def add_normal_3d(self, qw: float, qx: float, qy: float, qz: float) -> Entity:
        ...

    def add_distance(self, d: float, wp: Entity) -> Entity:
        ...

    def add_line_2d(self, p1: Entity, p2: Entity, wp: Entity) -> Entity:
        ...

    def add_line_3d(self, p1: Entity, p2: Entity) -> Entity:
        ...

    def add_cubic(self, p1: Entity, p2: Entity, p3: Entity, p4: Entity, wp: Entity) -> Entity:
        ...

    def add_arc(self, nm: Entity, ct: Entity, start: Entity, end: Entity, wp: Entity) -> Entity:
        ...

    def add_circle(self, nm: Entity, ct: Entity, radius: Entity, wp: Entity) -> Entity:
        ...

    def add_work_plane(self, origin: Entity, nm: Entity) -> Entity:
        ...

    def add_constraint(
        self,
        c_type: int,
        wp: Entity,
        v: float,
        p1: Entity,
        p2: Entity,
        e1: Entity,
        e2: Entity,
        e3: Entity = Entity.NONE,
        e4: Entity = Entity.NONE,
        other: int = 0,
        other2: int = 0
    ) -> None:
        ...

    def coincident(self, e1: Entity, e2: Entity, wp: Entity = Entity.FREE_IN_3D) -> None:
        ...

    def distance(
        self,
        e1: Entity,
        e2: Entity,
        value: float,
        wp: Entity = Entity.FREE_IN_3D
    ) -> None:
        ...

    def equal(self, e1: Entity, e2: Entity, wp: Entity = Entity.FREE_IN_3D) -> None:
        ...

    def equal_included_angle(
        self,
        e1: Entity,
        e2: Entity,
        e3: Entity,
        e4: Entity,
        wp: Entity
    ) -> None:
        ...

    def equal_point_to_line(
        self,
        e1: Entity,
        e2: Entity,
        e3: Entity,
        e4: Entity,
        wp: Entity
    ) -> None:
        ...

    def ratio(self, e1: Entity, e2: Entity, value: float, wp: Entity) -> None:
        ...

    def symmetric(
        self,
        e1: Entity,
        e2: Entity,
        e3: Entity = Entity.NONE,
        wp: Entity = Entity.FREE_IN_3D
    ) -> None:
        ...

    def symmetric_h(self, e1: Entity, e2: Entity, wp: Entity) -> None:
        ...

    def symmetric_v(self, e1: Entity, e2: Entity, wp: Entity) -> None:
        ...

    def midpoint(
        self,
        e1: Entity,
        e2: Entity,
        wp: Entity = Entity.FREE_IN_3D
    ) -> None:
        ...

    def horizontal(self, e1: Entity, wp: Entity) -> None:
        ...

    def vertical(self, e1: Entity, wp: Entity) -> None:
        ...

    def diameter(self, e1: Entity, value: float, wp: Entity) -> None:
        ...

    def same_orientation(self, e1: Entity, e2: Entity) -> None:
        ...

    def angle(self, e1: Entity, e2: Entity, value: float, wp: Entity, inverse: bool = False) -> None:
        ...

    def perpendicular(self, e1: Entity, e2: Entity, wp: Entity, inverse: bool = False) -> None:
        ...

    def parallel(self, e1: Entity, e2: Entity, wp: Entity = Entity.FREE_IN_3D) -> None:
        ...

    def tangent(self, e1: Entity, e2: Entity, wp: Entity = Entity.FREE_IN_3D) -> None:
        ...

    def distance_proj(self, e1: Entity, e2: Entity, value: float) -> None:
        ...

    def dragged(self, e1: Entity, wp: Entity = Entity.FREE_IN_3D) -> None:
        ...
