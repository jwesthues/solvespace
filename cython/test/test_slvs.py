# -*- coding: utf-8 -*-

"""This module will test the functions of Python-Solvespace."""

__author__ = "Yuan Chang"
__copyright__ = "Copyright (C) 2016-2019"
__license__ = "GPLv3+"
__email__ = "pyslvs@gmail.com"

from unittest import TestCase
from math import radians
from python_solvespace import ResultFlag, SolverSystem, make_quaternion


class CoreTest(TestCase):

    def test_crank_rocker(self):
        """Crank rocker example."""
        sys = SolverSystem()
        wp = sys.create_2d_base()
        p0 = sys.add_point_2d(0, 0, wp)
        sys.dragged(p0, wp)
        p1 = sys.add_point_2d(90, 0, wp)
        sys.dragged(p1, wp)
        line0 = sys.add_line_2d(p0, p1, wp)
        p2 = sys.add_point_2d(20, 20, wp)
        p3 = sys.add_point_2d(0, 10, wp)
        p4 = sys.add_point_2d(30, 20, wp)
        sys.distance(p2, p3, 40, wp)
        sys.distance(p2, p4, 40, wp)
        sys.distance(p3, p4, 70, wp)

        sys.distance(p0, p3, 35, wp)
        sys.distance(p1, p4, 70, wp)
        line1 = sys.add_line_2d(p0, p3, wp)
        sys.angle(line0, line1, 45, wp)

        result_flag = sys.solve()
        self.assertEqual(result_flag, ResultFlag.OKAY)
        x, y = sys.params(p2.params)
        self.assertAlmostEqual(39.54852, x, 4)
        self.assertAlmostEqual(61.91009, y, 4)

    def test_involute(self):
        """Involute example."""
        r = 10
        angle = 45

        sys = SolverSystem()
        wp = sys.create_2d_base()
        p0 = sys.add_point_2d(0, 0, wp)
        sys.dragged(p0, wp)

        p1 = sys.add_point_2d(0, 10, wp)
        sys.distance(p0, p1, r, wp)
        line0 = sys.add_line_2d(p0, p1, wp)

        p2 = sys.add_point_2d(10, 10, wp)
        line1 = sys.add_line_2d(p1, p2, wp)
        sys.distance(p1, p2, r * radians(angle), wp)
        sys.perpendicular(line0, line1, wp, False)

        p3 = sys.add_point_2d(10, 0, wp)
        sys.dragged(p3, wp)
        line_base = sys.add_line_2d(p0, p3, wp)
        sys.angle(line0, line_base, angle, wp)

        result_flag = sys.solve()
        self.assertEqual(result_flag, ResultFlag.OKAY)
        x, y = sys.params(p2.params)
        self.assertAlmostEqual(12.62467, x, 4)
        self.assertAlmostEqual(1.51746, y, 4)

    def test_jansen_linkage(self):
        """Jansen's linkage example."""
        sys = SolverSystem()
        wp = sys.create_2d_base()
        p0 = sys.add_point_2d(0, 0, wp)
        sys.dragged(p0, wp)

        p1 = sys.add_point_2d(0, 20, wp)
        sys.distance(p0, p1, 15, wp)
        line0 = sys.add_line_2d(p0, p1, wp)

        p2 = sys.add_point_2d(-38, -7.8, wp)
        sys.dragged(p2, wp)
        p3 = sys.add_point_2d(-50, 30, wp)
        p4 = sys.add_point_2d(-70, -15, wp)
        sys.distance(p2, p3, 41.5, wp)
        sys.distance(p3, p4, 55.8, wp)
        sys.distance(p2, p4, 40.1, wp)

        p5 = sys.add_point_2d(-50, -50, wp)
        p6 = sys.add_point_2d(-10, -90, wp)
        p7 = sys.add_point_2d(-20, -40, wp)
        sys.distance(p5, p6, 65.7, wp)
        sys.distance(p6, p7, 49.0, wp)
        sys.distance(p5, p7, 36.7, wp)

        sys.distance(p1, p3, 50, wp)
        sys.distance(p1, p7, 61.9, wp)

        p8 = sys.add_point_2d(20, 0, wp)
        line_base = sys.add_line_2d(p0, p8, wp)
        sys.angle(line0, line_base, 45, wp)

        result_flag = sys.solve()
        self.assertEqual(result_flag, ResultFlag.OKAY)
        x, y = sys.params(p8.params)
        self.assertAlmostEqual(18.93036, x, 4)
        self.assertAlmostEqual(13.63778, y, 4)

    def test_nut_cracker(self):
        """Nut cracker example."""
        h0 = 0.5
        b0 = 0.75
        r0 = 0.25
        n1 = 1.5
        n2 = 2.3
        l0 = 3.25

        sys = SolverSystem()
        wp = sys.create_2d_base()
        p0 = sys.add_point_2d(0, 0, wp)
        sys.dragged(p0, wp)

        p1 = sys.add_point_2d(2, 2, wp)
        p2 = sys.add_point_2d(2, 0, wp)
        line0 = sys.add_line_2d(p0, p2, wp)
        sys.horizontal(line0, wp)

        line1 = sys.add_line_2d(p1, p2, wp)
        p3 = sys.add_point_2d(b0 / 2, h0, wp)
        sys.dragged(p3, wp)
        sys.distance(p3, line1, r0, wp)
        sys.distance(p0, p1, n1, wp)
        sys.distance(p1, p2, n2, wp)

        result_flag = sys.solve()
        self.assertEqual(result_flag, ResultFlag.OKAY)
        x, _ = sys.params(p2.params)
        ans_min = x - b0 / 2
        ans_max = l0 - r0 - b0 / 2
        self.assertAlmostEqual(1.01576, ans_min, 4)
        self.assertAlmostEqual(2.625, ans_max, 4)

    def test_pydemo(self):
        """Some sample code for slvs.dll.
        We draw some geometric entities, provide initial guesses for their positions,
        and then constrain them. The solver calculates their new positions,
        in order to satisfy the constraints.

        Copyright 2008-2013 Jonathan Westhues.
        Copyright 2016-2017 Yuan Chang [pyslvs@gmail.com] Python-Solvespace bundled.

        An example of a constraint in 2d. In our first group, we create a workplane
        along the reference frame's xy plane. In a second group, we create some
        entities in that group and dimension them.
        """
        sys = SolverSystem()
        sys.set_group(1)

        # First, we create our workplane. Its origin corresponds to the origin
        # of our base frame (x y z) = (0 0 0)
        p101 = sys.add_point_3d(0, 0, 0)
        # and it is parallel to the xy plane, so it has basis vectors (1 0 0)
        # and (0 1 0).
        qw, qx, qy, qz = make_quaternion(1, 0, 0, 0, 1, 0)
        n102 = sys.add_normal_3d(qw, qx, qy, qz)
        wp200 = sys.add_work_plane(p101, n102)

        # Now create a second group. We'll solve group 2, while leaving group 1
        # constant; so the workplane that we've created will be locked down,
        # and the solver can't move it.
        sys.set_group(2)
        p301 = sys.add_point_2d(10, 20, wp200)
        p302 = sys.add_point_2d(20, 10, wp200)

        # And we create a line segment with those endpoints.
        l400 = sys.add_line_2d(p301, p302, wp200)

        # Now three more points.
        p303 = sys.add_point_2d(100, 120, wp200)
        p304 = sys.add_point_2d(120, 110, wp200)
        p305 = sys.add_point_2d(115, 115, wp200)

        # And arc, centered at point 303, starting at point 304, ending at
        # point 305.
        a401 = sys.add_arc(n102, p303, p304, p305, wp200)

        # Now one more point, and a distance
        p306 = sys.add_point_2d(200, 200, wp200)
        d307 = sys.add_distance(30, wp200)

        # And a complete circle, centered at point 306 with radius equal to
        # distance 307. The normal is 102, the same as our workplane.
        c402 = sys.add_circle(n102, p306, d307, wp200)

        # The length of our line segment is 30.0 units.
        sys.distance(p301, p302, 30, wp200)

        # And the distance from our line segment to the origin is 10.0 units.
        sys.distance(p101, l400, 10, wp200)

        # And the line segment is vertical.
        sys.vertical(l400, wp200)

        # And the distance from one endpoint to the origin is 15.0 units.
        sys.distance(p301, p101, 15, wp200)

        # The arc and the circle have equal radius.
        sys.equal(a401, c402, wp200)

        # The arc has radius 17.0 units.
        sys.diameter(a401, 17 * 2, wp200)

        # If the solver fails, then ask it to report which constraints caused
        # the problem.

        # And solve.
        result_flag = sys.solve()
        self.assertEqual(result_flag, ResultFlag.OKAY)
        x, y = sys.params(p301.params)
        self.assertAlmostEqual(10, x, 4)
        self.assertAlmostEqual(11.18030, y, 4)
        x, y = sys.params(p302.params)
        self.assertAlmostEqual(10, x, 4)
        self.assertAlmostEqual(-18.81966, y, 4)
        x, y = sys.params(p303.params)
        self.assertAlmostEqual(101.11418, x, 4)
        self.assertAlmostEqual(119.04153, y, 4)
        x, y = sys.params(p304.params)
        self.assertAlmostEqual(116.47661, x, 4)
        self.assertAlmostEqual(111.76171, y, 4)
        x, y = sys.params(p305.params)
        self.assertAlmostEqual(117.40922, x, 4)
        self.assertAlmostEqual(114.19676, y, 4)
        x, y = sys.params(p306.params)
        self.assertAlmostEqual(200, x, 4)
        self.assertAlmostEqual(200, y, 4)
        x, = sys.params(d307.params)
        self.assertAlmostEqual(17, x, 4)
        self.assertEqual(6, sys.dof())
