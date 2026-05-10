//-----------------------------------------------------------------------------
// Copyright (c) 2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

function test_advancedFieldPointComponents()
{
   %point : TypeMyPoint3F = 1,2,3;

   testNumber("advancedFields.point.x.read", %point.x, 1);
   testNumber("advancedFields.point.y.read", %point.y, 2);
   testNumber("advancedFields.point.z.read", %point.z, 3);

   %point.x = 10;
   %point.y = 20;
   %point.z = 30;

   testString("advancedFields.point.writeback", %point, "10 20 30");
   testNumber("advancedFields.point.assignedComponent", (%point.x = 44), 44);
   testString("advancedFields.point.assignedComponent.writeback", %point, "44 20 30");
}

function test_advancedFieldVectorIndexing()
{
   %vec : TypeS32Vector = 5,6,7;

   testInt("advancedFields.s32Vector.read0", %vec{0}, 5);
   testInt("advancedFields.s32Vector.read1", %vec{1}, 6);
   testInt("advancedFields.s32Vector.read2", %vec{2}, 7);

   %vec{1} = 42;
   testInt("advancedFields.s32Vector.write.result", %vec{1}, 42);
   testString("advancedFields.s32Vector.writeback", %vec, "5 42 7");

   %idx = 2;
   %vec{%idx} = 99;
   testString("advancedFields.s32Vector.dynamicIndex", %vec, "5 42 99");

   %floats : TypeF32Vector = 1.5,2.5,3.5;
   testNumber("advancedFields.f32Vector.read", %floats{1}, 2.5);
   %floats{2} = 4.5;
   testString("advancedFields.f32Vector.writeback", %floats, "1.5 2.5 4.5");
}

function test_advancedFieldConstructedExpressionIndexing()
{
   %vec : TypeS32Vector = 7,8,9;
   testInt("advancedFields.parenthesizedVector.read", (%vec){1}, 8);
}

function test_advancedFieldGlobalAccess()
{
   $globalPoint : TypeMyPoint3F = 100,200,300;
   $globalVec : TypeS32Vector = 9,8,7;

   testNumber("advancedFields.globalPoint.x.read", $globalPoint.x, 100);
   testNumber("advancedFields.globalPoint.y.read", $globalPoint.y, 200);
   testNumber("advancedFields.globalPoint.z.read", $globalPoint.z, 300);

   $globalPoint.x = 10;
   $globalPoint.y = 20;
   $globalPoint.z = 30;

   testString("advancedFields.globalPoint.writeback", $globalPoint, "10 20 30");

   testInt("advancedFields.globalVec.read0", $globalVec{0}, 9);
   testInt("advancedFields.globalVec.read1", $globalVec{1}, 8);
   testInt("advancedFields.globalVec.read2", $globalVec{2}, 7);

   $globalVec{1} = 42;
   testInt("advancedFields.globalVec.write.result", $globalVec{1}, 42);
   testString("advancedFields.globalVec.writeback", $globalVec, "9 42 7");

   $globalIdx = 2;
   $globalVec{$globalIdx} = 99;
   testString("advancedFields.globalVec.dynamicIndex", $globalVec, "9 42 99");
}

function test_advancedFieldTupleRhs()
{
   %probe : TypeTupleProbe = "";

   %probe.capture = 1,2,3;
   testString("advancedFields.tupleCapture.writeback", %probe.capture, "3 1 2 3");

   %probe.capture = 4,5;
   testString("advancedFields.tupleCapture.writeback2", %probe.capture, "2 4 5 0");
}

test_advancedFieldPointComponents();
test_advancedFieldVectorIndexing();
test_advancedFieldConstructedExpressionIndexing();
test_advancedFieldGlobalAccess();
test_advancedFieldTupleRhs();
