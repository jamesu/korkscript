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

function test_fiberBasic(%id, %s1, %s2, %s3)
{
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


test_fiberBasic();

echo("Fiber tests finished");
