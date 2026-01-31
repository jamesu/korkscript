//-----------------------------------------------------------------------------
// Copyright (c) 2025-2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

function fiber_entry(%id)
{
   echo("fiber_entry:" @ %id);

   // First phase
   $fiberLog[%id] = "A";
   %vc = yieldFiber(123);
   echo("in-fiber yield returned:" @ %vc);

   // Second phase
   $fiberLog[%id] = $fiberLog[%id] @ "B";
   %vc = yieldFiber(%vc + 4);
   echo("in-fiber yield returned:" @ %vc);

   // Final phase
   $fiberLog[%id] = $fiberLog[%id] @ "C";
   return %vc @ "RET";
}

function test_fiberBasic()
{
   $FIBFIN = 0;
   %fiberId = createFiber();
   %code = "fiber_entry(" @ %fiberId @ "); $FIBFIN=1;";
   %yield1 = evalInFiber(%fiberId, %code);
   testInt("fiberBasic.chk1", $FIBFIN, 0);
   testString("fiberBasic.chk1L", $fiberLog[%fiberId], "A");
   %yield2 = resumeFiber(%fiberId, 7) @ "R1";
   testInt("fiberBasic.chk2", $FIBFIN, 0);
   testString("fiberBasic.chk2LocalVar", readFiberLocalVariable(%fiberId, "%vc"), "7");
   testString("fiberBasic.chk2L", $fiberLog[%fiberId], "AB");
   %yield3 = resumeFiber(%fiberId, "TEN") @ "R2";
   testInt("fiberBasic.chk3", $FIBFIN, 1);
   testString("fiberBasic.chk3LocalVar", readFiberLocalVariable(%fiberId, "%vc"), ""); // i.e. should have finished
   testString("fiberBasic.chk3L", $fiberLog[%fiberId], "ABC");

   // NOTE: to keep things simple, we just yield with a number at the moment; 
   // right now yielding strings (via getReturnBuffer) is slightly problematic as 
   // they need to be copied to a fiber specific safe buffer.
   testInt("fiberBasic.step1", %yield1, 123);
   testString("fiberBasic.step2", %yield2, "11R1");
   testString("fiberBasic.step3", %yield3, "TENRETR2");
}

function test_fiberSaveLoad()
{
   $FIBFIN = 0;
   %fiberId = createFiber();
   %code = "fiber_entry(" @ %fiberId @ "); $FIBFIN=1;";
   %yield1 = evalInFiber(%fiberId, %code);

   %didSave = saveFibers(%fiberId, "test.dat");
   echo("STOPPING SERIALIZED FIBER Y1");
   stopFiber(%fiberId);
   %restoredId = restoreFibers("test.dat");

   testInt("fiberSaveLoad.chk1", $FIBFIN, 0);
   testString("fiberSaveLoad.chk1L", $fiberLog[%fiberId], "A");

   %yield2 = resumeFiber(%restoredId, 26);

   %didSave = saveFibers(%restoredId, "test.dat");
   echo("STOPPING SERIALIZED FIBER Y2");
   stopFiber(%restoredId);
   %restoredId2 = restoreFibers("test.dat");

   testInt("fiberSaveLoad.chk2", $FIBFIN, 0);
   testString("fiberSaveLoad.chk2LocalVar", readFiberLocalVariable(%restoredId, "%vc"), "26");
   testString("fiberSaveLoad.chk2L", $fiberLog[%fiberId], "AB");

   %yield3 = resumeFiber(%restoredId2, "FUDGE");
   testInt("fiberSaveLoad.chk3", $FIBFIN, 1);
   testString("fiberSaveLoad.chk3LocalVar", readFiberLocalVariable(%restoredId, "%vc"), ""); // i.e. should have finished
   testString("fiberSaveLoad.chk3L", $fiberLog[%fiberId], "ABC");

   testInt("fiberSaveLoad.step1", %yield1, 123);
   testString("fiberSaveLoad.step2", %yield2, 30); // 26+4
   testString("fiberSaveLoad.step3", %yield3, "FUDGERET");
}


test_fiberBasic();
echo("--");
test_fiberSaveLoad();

echo("Fiber tests finished");
