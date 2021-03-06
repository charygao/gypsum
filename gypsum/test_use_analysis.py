# Copyright Jay Conrod. All rights reserved.
#
# This file is part of Gypsum. Use of this source code is governed by
# the GPL license that can be found in the LICENSE.txt file.


import unittest

from compile_info import *
from errors import *
from flags import LET
from ids import *
from inheritance_analysis import analyzeInheritance
from ir import *
from ir_types import *
from lexer import *
from location import NoLoc
from name import CLASS_INIT_SUFFIX, Name, RECEIVER_SUFFIX
from parser import *
from scope_analysis import *
from type_analysis import *
from utils_test import FakePackageLoader, TestCaseWithDefinitions


class TestUseAnalysis(TestCaseWithDefinitions):
    def parseFromSource(self, source):
        filename = "(test)"
        tokens = lex(filename, source)
        ast = parse(filename, tokens)
        return ast

    def analyzeFromSource(self, source):
        ast = self.parseFromSource(source)
        package = Package(id=TARGET_PACKAGE_ID)
        packageLoader = FakePackageLoader([])
        info = CompileInfo(ast, package, packageLoader, isUsingStd=False)
        analyzeDeclarations(info)
        analyzeTypeDeclarations(info)
        analyzeInheritance(info)
        return info

    def analyzeFromSourceWithTypes(self, source):
        info = self.analyzeFromSource(source)
        analyzeTypes(info)
        return info

    def testUndefinedReference(self):
        source = "var x = y"
        info = self.analyzeFromSource(source)
        self.assertRaises(ScopeException,
                          info.getScope(GLOBAL_SCOPE_ID).lookupFromSelf, "y", NoLoc)

    def testUseVarBeforeDefinition(self):
        source = "def f =\n" + \
                 "  var x = y\n" + \
                 "  var y = 12"
        info = self.analyzeFromSource(source)
        scope = info.getScope(info.ast.modules[0].definitions[0])
        self.assertRaises(ScopeException, scope.lookupFromSelf, "y", NoLoc)

    def testUseFunctionBeforeDefinition(self):
        source = "def f = g\n" + \
                 "def g = 12"
        info = self.analyzeFromSource(source)
        gDefnInfo = info.getDefnInfo(info.ast.modules[0].definitions[1])
        gNameInfo = info.getScope(info.ast.modules[0].definitions[0]).lookupFromSelf("g", NoLoc)
        self.assertIs(gDefnInfo, gNameInfo.getDefnInfo())

    def testUseCapturedVarBeforeDefinition(self):
        info = self.analyzeFromSource("def f =\n" + \
                                      "  def g =\n" + \
                                      "    i = 1\n" + \
                                      "  var i: i64 = 0\n")
        statements = info.ast.modules[0].definitions[0].body.statements
        iDefnInfo = info.getDefnInfo(statements[1].pattern)
        gScope = info.getScope(statements[0])
        iNameInfo = gScope.lookupFromSelf("i", NoLoc)
        self.assertIs(iDefnInfo, iNameInfo.getDefnInfo())

    def testUseClassBeforeDefinition(self):
        source = "def f = C\n" + \
                 "class C"
        info = self.analyzeFromSource(source)
        cDefnInfo = info.getDefnInfo(info.ast.modules[0].definitions[1])
        cNameInfo = info.getScope(GLOBAL_SCOPE_ID).lookupFromSelf("C", NoLoc)
        self.assertIs(cDefnInfo, cNameInfo.getDefnInfo())

    def testUseInLocalScope(self):
        source = "def f(x: i64) =\n" + \
                 "  if (true)\n" + \
                 "    x"
        info = self.analyzeFromSource(source)
        fAst = info.ast.modules[0].definitions[0]
        fScope = info.getScope(fAst)
        fScope.define("x")
        xDefnInfo = info.getDefnInfo(fAst.parameters[0].pattern)
        localScope = info.getScope(fAst.body.statements[0].trueExpr)
        xNameInfo = localScope.lookupFromSelf("x", NoLoc)
        self.assertIs(xDefnInfo, xNameInfo.getDefnInfo())

    def testUseThisInInitializer(self):
        source = "class Foo\n" + \
                 "  var x = this"
        info = self.analyzeFromSource(source)
        classScope = info.getScope(info.ast.modules[0].definitions[0])
        thisNameInfo = classScope.lookupFromSelf("this", NoLoc)
        self.assertEquals(DefnInfo(self.makeVariable(Name(["Foo", CLASS_INIT_SUFFIX, RECEIVER_SUFFIX]),
                                                     kind=PARAMETER, flags=frozenset([LET])),
                                   classScope.scopeId, False,
                                   classScope.scopeId, NOT_HERITABLE),
                          thisNameInfo.getDefnInfo())

    def testUsePrivateOuter(self):
        source = "class C\n" + \
                 "  private def f = ()\n" + \
                 "def g(o: C) = o.f"
        self.assertRaises(ScopeException, self.analyzeFromSourceWithTypes, source)

    def testUsePrivateSubclass(self):
        source = "class A\n" + \
                 "  private def f = ()\n" + \
                 "class B <: A\n" + \
                 "  def g = f"
        self.assertRaises(ScopeException, self.analyzeFromSourceWithTypes, source)

    def testUsePrivateSibling(self):
        source = "class C\n" + \
                 "  private def f = ()\n" + \
                 "  def g = f"
        info = self.analyzeFromSourceWithTypes(source)
        use = info.getUseInfo(info.ast.modules[0].definitions[0].members[1].body)
        self.assertEquals(info.getScope(info.ast.modules[0].definitions[0].members[1]).scopeId,
                          use.useScopeId)
        self.assertEquals(USE_AS_VALUE, use.kind)

    def testUsePrivateChild(self):
        source = "class C\n" + \
                 "  private def f = ()\n" + \
                 "  def g =\n" + \
                 "    def h = f"
        info = self.analyzeFromSourceWithTypes(source)
        useScopeAst = info.ast.modules[0].definitions[0].members[1].body.statements[0]
        use = info.getUseInfo(useScopeAst.body)
        self.assertEquals(info.getScope(useScopeAst).scopeId, use.useScopeId)
        self.assertEquals(USE_AS_VALUE, use.kind)

    def testUsePrivateInnerWithReceiver(self):
        source = "class C\n" + \
                 "  private var x: i64\n" + \
                 "  def f(other: C) = other.x"
        info = self.analyzeFromSourceWithTypes(source)
        use = info.getUseInfo(info.ast.modules[0].definitions[0].members[1].body)
        self.assertEquals(info.getScope(info.ast.modules[0].definitions[0].members[1]).scopeId,
                          use.useScopeId)
        self.assertEquals(USE_AS_PROPERTY, use.kind)

    def testUseProtectedOuter(self):
        source = "class C\n" + \
                 "  protected def f = ()\n" + \
                 "def g(o: C) = o.f"
        self.assertRaises(ScopeException, self.analyzeFromSourceWithTypes, source)

    def testUseProtectedSibling(self):
        source = "class C\n" + \
                 "  protected def f = ()\n" + \
                 "  def g = f"
        info = self.analyzeFromSourceWithTypes(source)
        useScopeAst = info.ast.modules[0].definitions[0].members[1]
        use = info.getUseInfo(useScopeAst.body)
        self.assertEquals(info.getScope(useScopeAst).scopeId, use.useScopeId)
        self.assertEquals(USE_AS_VALUE, use.kind)

    def testUseProtectedInherited(self):
        source = "class A\n" + \
                 "  protected def f = ()\n" + \
                 "class B <: A\n" + \
                 "  def g = f"
        info = self.analyzeFromSourceWithTypes(source)
        useScopeAst = info.ast.modules[0].definitions[1].members[0]
        use = info.getUseInfo(useScopeAst.body)
        self.assertEquals(info.getScope(useScopeAst).scopeId, use.useScopeId)
        self.assertEquals(USE_AS_VALUE, use.kind)

    def testCallAbstractClassCtor(self):
        source = "abstract class A\n" + \
                 "def f = A()"
        self.assertRaises(ScopeException, self.analyzeFromSourceWithTypes, source)

    def testUseSuperCtorFromPrimaryCtor(self):
        source = "class Foo(x: i64)\n" + \
                 "class Bar(y: i64) <: Foo(y)"
        info = self.analyzeFromSourceWithTypes(source)
        fooPrimaryCtorDefnInfo = \
          info.getDefnInfo(info.ast.modules[0].definitions[0].constructor)
        use = info.getUseInfo(info.ast.modules[0].definitions[1])
        self.assertIs(fooPrimaryCtorDefnInfo, use.defnInfo)

    def testUseSuperCtorFromDefaultCtor(self):
        source = "class Foo(x: i64)\n" + \
                 "class Bar <: Foo(12)"
        info = self.analyzeFromSourceWithTypes(source)
        fooPrimaryCtorDefnInfo = \
          info.getDefnInfo(info.ast.modules[0].definitions[0].constructor)
        use = info.getUseInfo(info.ast.modules[0].definitions[1])
        self.assertIs(fooPrimaryCtorDefnInfo, use.defnInfo)

    def testUseSuperCtorImplicit(self):
        source = "class Foo\n" + \
                 "class Bar <: Foo"
        info = self.analyzeFromSourceWithTypes(source)
        fooDefaultCtor = info.package.findClass(name="Foo").constructors[0]
        use = info.getUseInfo(info.ast.modules[0].definitions[1])
        self.assertIs(use.defnInfo.irDefn, fooDefaultCtor)

    def testUseGlobalFromStaticMethod(self):
        source = "let x = 12\n" + \
                 "class Foo\n" + \
                 "  static def f = x"
        info = self.analyzeFromSourceWithTypes(source)
        x = info.package.findGlobal(name="x")
        use = info.getUseInfo(info.ast.modules[0].definitions[1].members[0].body)
        self.assertIs(x, use.defnInfo.irDefn)

    def testUseFieldFromStaticMethod(self):
        source = "class Foo\n" + \
                 "  let x = 12\n" + \
                 "  static def f = x"
        self.assertRaises(ScopeException, self.analyzeFromSourceWithTypes, source)

    def testUseFieldPropertyFromStaticMethod(self):
        source = "class Foo(x: i32)\n" + \
                 "class Bar\n" + \
                 "  let foo = Foo(12i32)\n" + \
                 "  static def f = foo.x"
        self.assertRaises(ScopeException, self.analyzeFromSourceWithTypes, source)

    def testUseMethodFromStaticMethod(self):
        source = "class Foo\n" + \
                 "  def f = 12\n" + \
                 "  static def g = f"
        self.assertRaises(ScopeException, self.analyzeFromSourceWithTypes, source)

    def testUseStaticMethodFromStaticMethod(self):
        source = "class Foo\n" + \
                 "  static def f = 12\n" + \
                 "  static def g = f"
        info = self.analyzeFromSourceWithTypes(source)
        f = info.package.findFunction(name="Foo.f")
        use = info.getUseInfo(info.ast.modules[0].definitions[0].members[1].body)
        self.assertIs(f, use.defnInfo.irDefn)

    def testUsePrivateStaticMethodFromStaticMethod(self):
        source = "class Foo\n" + \
                 "  private static def f = 12\n" + \
                 "  static def g = f"
        info = self.analyzeFromSourceWithTypes(source)
        f = info.package.findFunction(name="Foo.f")
        use = info.getUseInfo(info.ast.modules[0].definitions[0].members[1].body)
        self.assertIs(f, use.defnInfo.irDefn)

    def testUseInheritedStaticMethod(self):
        source = "class Foo\n" + \
                 "  static def f = 12\n" + \
                 "class Bar <: Foo\n" + \
                 "  static def g = f"
        info = self.analyzeFromSourceWithTypes(source)
        f = info.package.findFunction(name="Foo.f")
        use = info.getUseInfo(info.ast.modules[0].definitions[1].members[0].body)
        self.assertIs(f, use.defnInfo.irDefn)

    def testUseInheritedStaticPrivateMethod(self):
        source = "class Foo\n" + \
                 "  private static def f = 12\n" + \
                 "class Bar <: Foo\n" + \
                 "  static def g = f"
        self.assertRaises(ScopeException, self.analyzeFromSourceWithTypes, source)

    def testUseInheritedStaticProtectedMethod(self):
        source = "class Foo\n" + \
                 "  protected static def f = 12\n" + \
                 "class Bar <: Foo\n" + \
                 "  static def g = f"
        info = self.analyzeFromSourceWithTypes(source)
        f = info.package.findFunction(name="Foo.f")
        use = info.getUseInfo(info.ast.modules[0].definitions[1].members[0].body)
        self.assertIs(f, use.defnInfo.irDefn)

    def testUseStaticMethodFromGlobal(self):
        source = "class Foo\n" + \
                 "  static def f = 12\n" + \
                 "def g = Foo.f"
        info = self.analyzeFromSourceWithTypes(source)
        f = info.package.findFunction(name="Foo.f")
        use = info.getUseInfo(info.ast.modules[0].definitions[1].body)
        self.assertIs(f, use.defnInfo.irDefn)

    def testUseStaticPrivateMethodFromGlobal(self):
        source = "class Foo\n" + \
                 "  private static def f = 12\n" + \
                 "def g = Foo.f"
        self.assertRaises(ScopeException, self.analyzeFromSourceWithTypes, source)

    def testUseInheritedStaticMethodFromGlobal(self):
        source = "class Foo\n" + \
                 "  static def f = 12\n" + \
                 "class Bar <: Foo\n" + \
                 "def g = Bar.f"
        info = self.analyzeFromSourceWithTypes(source)
        f = info.package.findFunction(name="Foo.f")
        use = info.getUseInfo(info.ast.modules[0].definitions[2].body)
        self.assertIs(f, use.defnInfo.irDefn)

    def testUseNonStaticMethodFromGlobal(self):
        source = "class Foo\n" + \
                 "  def f = 12\n" + \
                 "def g = Foo.f"
        self.assertRaises(TypeException, self.analyzeFromSourceWithTypes, source)

    def testUseExistentialVariableOutOfScope(self):
        source = "var x: forsome [X] X\n" + \
                 "var y: X"
        self.assertRaises(ScopeException, self.analyzeFromSourceWithTypes, source)

    # Regression tests
    def testUseTypeParameterInLaterPrimaryCtor(self):
        source = "class Foo\n" + \
                 "  def make-bar = Bar[Foo](this)\n" + \
                 "class Bar[static +T](value: T)"
        info = self.analyzeFromSourceWithTypes(source)
        T = info.package.findTypeParameter(name="Bar.T")
        astTy = info.ast.modules[0].definitions[1].constructor.parameters[0].pattern.ty
        use = info.getUseInfo(astTy)
        self.assertIs(T, use.defnInfo.irDefn)

    def testUseTypeParameterInLaterPrimaryCtorField(self):
        source = "class Foo\n" + \
                 "  def get(bar: Bar[String]) = bar.value\n" + \
                 "class Bar[static +T](value: T)"
        info = self.analyzeFromSourceWithTypes(source)
        self.assertEquals(getStringType(),
                          info.getType(info.ast.modules[0].definitions[0].members[0].body))

    def testUseTypeParameterInLaterField(self):
        source = "class Foo\n" + \
                 "  def get(bar: Bar[String]) = bar.value\n" + \
                 "class Bar[static T]\n" + \
                 "  var value: T"
        info = self.analyzeFromSourceWithTypes(source)
        self.assertEquals(getStringType(),
                          info.getType(info.ast.modules[0].definitions[0].members[0].body))

    def testUseTypeParameterInBound(self):
        source = "class A[static T]\n" + \
                 "def f[static S <: A[S]] = ()"
        info = self.analyzeFromSourceWithTypes(source)
        S = info.package.findTypeParameter(name="f.S")
        use = info.getUseInfo(info.ast.modules[0].definitions[1].typeParameters[0].upperBound.typeArguments[0])
        self.assertIs(S, use.defnInfo.irDefn)

    def testUseTypeParameterAsVariable(self):
        source = "def f[static T] = T"
        self.assertRaises(TypeException, self.analyzeFromSourceWithTypes, source)

    def testUseFieldInStaticMatchCase(self):
        source = "class Foo(x: i64)\n" + \
                 "  static def f(obj: Object) =\n" + \
                 "    match (obj)\n" + \
                 "      case _ => x"
        self.assertRaises(ScopeException, self.analyzeFromSourceWithTypes, source)


if __name__ == "__main__":
    unittest.main()
