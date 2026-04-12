//-----------------------------------------------------------------------------
// Copyright (c) 2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

function LegacyParentA::resolveLegacy(%this)
{
   return "parentA";
}

function LegacyParentB::resolveLegacy(%this)
{
   return "parentB";
}

function LegacySharedClass::resolveLegacy(%this)
{
   return "class";
}

function LegacySharedClass::classOnly(%this)
{
   return "classOnly";
}

function LegacyAssignedClass::assignedMethod(%this)
{
   return "assigned";
}

function LegacyParentA::superOnly(%this)
{
   return "superOnly";
}

%legacyOk = new ScriptObject();
%legacyOk.setSuperClassNamespace("LegacyParentA");
%legacyOk.setClassNamespace("LegacySharedClass");
%legacyOkChain = getObjectNamespaceChain(%legacyOk);
echo("legacy ok chain: " @ %legacyOkChain);

testString("legacyNamespace.link.classWins", %legacyOk.resolveLegacy(), "class");
testString("legacyNamespace.link.className", %legacyOk.getClassNamespace(), "LegacySharedClass");
testString("legacyNamespace.link.superClassName", %legacyOk.getSuperClassNamespace(), "LegacyParentA");
testString("legacyNamespace.link.chain", %legacyOkChain, "SimObject -> ScriptObject -> LegacyParentA -> LegacySharedClass");
testString("legacyNamespace.link.classOnly", %legacyOk.classOnly(), "classOnly");
testString("legacyNamespace.link.superOnly", %legacyOk.superOnly(), "superOnly");
testString("legacyNamespace.link.nativeMethod", %legacyOk.getClassName(), "ScriptObject");

%legacyAssigned = new ScriptObject()
{
   class = "LegacyAssignedClass";
};
%legacyAssignedChain = getObjectNamespaceChain(%legacyAssigned);
echo("legacy assigned chain: " @ %legacyAssignedChain);

testString("legacyNamespace.assigned.className", %legacyAssigned.getClassNamespace(), "LegacyAssignedClass");
testString("legacyNamespace.assigned.chain", %legacyAssignedChain, "SimObject -> ScriptObject -> LegacyAssignedClass");
testString("legacyNamespace.assigned.method", %legacyAssigned.assignedMethod(), "assigned");

%legacyConflict = new ScriptObject()
{
   superclass = "LegacyParentB";
   class = "LegacySharedClass";
};
%legacyConflictChain = getObjectNamespaceChain(%legacyConflict);
echo("legacy conflict chain: " @ %legacyConflictChain);

testString("legacyNamespace.conflict.classCleared", %legacyConflict.getClassNamespace(), "");
testString("legacyNamespace.conflict.superClassRetained", %legacyConflict.getSuperClassNamespace(), "LegacyParentB");
testString("legacyNamespace.conflict.fallbackToSuper", %legacyConflict.resolveLegacy(), "parentB");
testString("legacyNamespace.conflict.chain", %legacyConflictChain, "SimObject -> ScriptObject -> LegacyParentB");
testString("legacyNamespace.conflict.nativeMethod", %legacyConflict.getClassName(), "ScriptObject");
testString("legacyNamespace.conflict.objectStillAlive", %legacyConflict.getClassName(), "ScriptObject");
