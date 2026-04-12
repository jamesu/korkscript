//-----------------------------------------------------------------------------
// Copyright (c) 2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

class TestScriptBase
{
   value = 3;
   label : string = "base";
};

function TestScriptBase::doubleValue(%this)
{
   return %this.value * 2;
}

$seedValue = 11;

class TestScriptChild : TestScriptBase
{
   value = 7;
   seeded = $seedValue;
   internalName = "script-priority";
};

class TestScriptTupleHolder
{
   point : TypeMyPoint3F = 1,2,3;
};

class TestFieldEnumBase
{
   baseField : string = "base";
};

class TestFieldEnumChild : TestFieldEnumBase
{
   childField : string = "child";
};

%baseObj = new TestScriptBase();
testNumber("scriptClass.base.default", %baseObj.value, 3);
testString("scriptClass.base.label", %baseObj.label, "base");
testNumber("scriptClass.base.method", %baseObj.doubleValue(), 6);

%childObj = new TestScriptChild()
{
   value = 19;
};

testNumber("scriptClass.child.overrideWins", %childObj.value, 19);
testString("scriptClass.child.inheritedDefault", %childObj.label, "base");
testNumber("scriptClass.child.seededDefault", %childObj.seeded, 11);
testString("scriptClass.child.nativeFieldPriority", %childObj.internalName, "script-priority");
testString("scriptClass.child.nativeFieldPriority.getFieldValue", %childObj.getFieldValue("internalName"), "script-priority");

%tupleObj = new TestScriptTupleHolder();
testString("scriptClass.tuple.default", %tupleObj.point, "1 2 3");

%tupleOverrideObj = new TestScriptTupleHolder()
{
   point = 4,5,6;
};
testString("scriptClass.tuple.override", %tupleOverrideObj.point, "4 5 6");

%templateChild = new TestScriptChild(TemplateChild)
{
   seeded = 99;
   label = "template";
};

%copiedChild = new TestScriptChild(CopiedChild : TemplateChild);
testNumber("scriptClass.child.parentCopy.seededPreserved", %copiedChild.seeded, 99);
testString("scriptClass.child.parentCopy.labelPreserved", %copiedChild.label, "template");

class TestScriptChild : TestScriptBase
{
   value = 42;
   seeded = 21;
};

%redefinedChild = new TestScriptChild();
testNumber("scriptClass.redefine.newDefault", %redefinedChild.value, 42);
testNumber("scriptClass.redefine.newSeed", %redefinedChild.seeded, 21);

%fieldListLocal = getClassFieldList("TestFieldEnumChild", false);
%fieldListFull = getClassFieldList("TestFieldEnumChild", true);
echo("script class local field list:\n" @ %fieldListLocal);
echo("script class full field list:\n" @ %fieldListFull);

testNumber("scriptClass.fieldEnum.local.hasChild", strstr(%fieldListLocal, "TestFieldEnumChild\tscript\tchildField\tstring") >= 0, 1);
testNumber("scriptClass.fieldEnum.local.noParent", strstr(%fieldListLocal, "TestFieldEnumBase\tscript\tbaseField\tstring"), -1);
testNumber("scriptClass.fieldEnum.full.hasChild", strstr(%fieldListFull, "TestFieldEnumChild\tscript\tchildField\tstring") >= 0, 1);
testNumber("scriptClass.fieldEnum.full.hasParent", strstr(%fieldListFull, "TestFieldEnumBase\tscript\tbaseField\tstring") >= 0, 1);
testNumber("scriptClass.fieldEnum.full.hasNative", strstr(%fieldListFull, "SimObject\tnative\tinternalName\t") >= 0, 1);
