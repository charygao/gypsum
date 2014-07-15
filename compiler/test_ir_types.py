# Copyright 2014, Jay Conrod. All rights reserved.
#
# This file is part of Gypsum. Use of this source code is governed by
# the GPL license that can be found in the LICENSE.txt file.


import unittest

from builtins import *
from ir_types import *
from utils import *


class TestIrTypes(unittest.TestCase):
    registerBuiltins(lambda name, ir: None)

    def makeClass(self, name, superclass):
        clas = Class(name, [], [ClassType(superclass)], None, None, None, None, frozenset())
        clas.id = self.nextId()
        return clas

    def setUp(self):
        self.nextId = Counter()
        self.A = self.makeClass("A", getRootClass())
        self.B = self.makeClass("B", self.A)
        self.C = self.makeClass("C", self.B)

    def testSubtypeSelf(self):
        self.assertTrue(ClassType(self.A).isSubtypeOf(ClassType(self.A)))

    def testSubtypeParent(self):
        self.assertTrue(ClassType(self.B).isSubtypeOf(ClassType(self.A)))
        self.assertFalse(ClassType(self.A).isSubtypeOf(ClassType(self.B)))

    def testParameterSelf(self):
        T = TypeParameter("T", ClassType(self.A), ClassType(self.B), frozenset())
        ty = VariableType(T)
        self.assertTrue(ty.isSubtypeOf(ty))

    def testParametersOverlapping(self):
        T = TypeParameter("T", ClassType(self.A), ClassType(self.C), frozenset())
        S = TypeParameter("S", ClassType(self.B), ClassType(self.C), frozenset())
        self.assertFalse(VariableType(S).isSubtypeOf(VariableType(T)))

    def testParametersNonOverlapping(self):
        T = TypeParameter("T", ClassType(self.A), ClassType(self.B), frozenset())
        S = TypeParameter("S", ClassType(self.B), ClassType(self.C), frozenset())
        self.assertTrue(VariableType(S).isSubtypeOf(VariableType(T)))
