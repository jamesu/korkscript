
function eh_leaf_ok()
{
   $ehLog = $ehLog @ "L1";
}

function eh_leaf_throw()
{
   $ehLog = $ehLog @ "L2";
   throwFiber(4, false);
   // Should not continue
   $ehLog = $ehLog @ "L2X";
}

function eh_mid()
{
   $ehLog = $ehLog @ "M1";
   eh_leaf_throw();
   // Should not continue
   $ehLog = $ehLog @ "M2";
}

function test_basicThrow()
{
   $ehLog = "";
   echo("PPP");
   try
   {
      echo("basicthrow !start");
      $ehLog = $ehLog @ "L2";
      throwFiber(4, false);
      echo("basicthrow WTF");
      // Should not continue
      $ehLog = $ehLog @ "L2X";
   }
   catch (4)
   {
      $ehLog = $ehLog @ "CATCH4";
   }
   $fudge2 = "caramel";
   testString("exc.basicThrow", $ehLog, "L2CATCH4");
}

function test_basicMidThrow()
{
   $ehLog = "";
   try
   {
      $ehLog = $ehLog @ "L2";
      eh_mid();
      $ehLog = $ehLog @ "L2X";
   }
   catch (4)
   {
      $ehLog = $ehLog @ "CATCH4";
   }
   $fudge3 = "caramel";
   testString("exc.basicMidThrow", $ehLog, "L2M1L2CATCH4");
}

function test_softThrow()
{
   $ehLog = "";
   try
   {
      $ehLog = $ehLog @ "L2";
      throwFiber(8, true);
      $ehLog = $ehLog @ "L2X";
   }
   catch (4)
   {
      $ehLog = $ehLog @ "CATCH4";
   }
   $fudge4 = "caramel";
   testString("exc.softMidThrow", $ehLog, "L2L2X");
}


function test_multiCatch1()
{
   $ehLog = "";
   echo("PPP");
   try
   {
      $ehLog = $ehLog @ "L2";
      throwFiber(4 | 8, false);
      // Should not continue
      $ehLog = $ehLog @ "L2X";
   }
   catch (4)
   {
      $ehLog = $ehLog @ "CATCH4";
   }
   catch (8)
   {
      $ehLog = $ehLog @ "CATCH5";
   }

   $fudge10 = "pie";
   testString("exc.multiCatch1", $ehLog, "L2CATCH4");
}

function test_multiCatch2()
{
   $ehLog = "";
   echo("PPP");
   try
   {
      $ehLog = $ehLog @ "L2";
      throwFiber(4 | 8, false);
      // Should not continue
      $ehLog = $ehLog @ "L2X";
   }
   catch (8)
   {
      $ehLog = $ehLog @ "CATCH5";
   }
   catch (4)
   {
      $ehLog = $ehLog @ "CATCH4";
   }

   $fudge11 = "pie";
   testString("exc.multiCatch2", $ehLog, "L2CATCH5");
}

function test_nestedFrameThrow1()
{
   $ehLog = "";
   try
   {
      try
      {
         $ehLog = $ehLog @ "L2";
         throwFiber(4, true); // should go to other try block
         $ehLog = $ehLog @ "L2X";
      }
      catch (4)
      {
         $ehLog = $ehLog @ "CATCH3";
      }

      $ehLog = $ehLog @ "RC1";

      // should exit to here
      $fudge5 = "caramel";
   }
   catch (4)
   {
      $ehLog = $ehLog @ "CATCH4";
   }
   $fudge6 = "lemon";
   testString("exc.nestedThrow", $ehLog, "L2CATCH3RC1");
}

function test_nestedFrameThrow2()
{
   $ehLog = "";
   try
   {
      try
      {
         $ehLog = $ehLog @ "L2";
         throwFiber(8, true); // should go to outer try block
         $ehLog = $ehLog @ "L2X";
      }
      catch (4)
      {
         $ehLog = $ehLog @ "CATCH3";
      }

      $ehLog = $ehLog @ "RC1";

      // should exit to here
      $fudge7 = "caramel";
   }
   catch (8)
   {
      $ehLog = $ehLog @ "CATCH4";
   }
   $fudge8 = "lemon";
   testString("exc.nestedThrow2", $ehLog, "L2CATCH4");
}



function test_nativeBound()
{
   $ehLog = "";
   try
   {
      try
      {
         $ehLog = $ehLog @ "L2";
         eval("throwFiber(4, false);"); // should continue
         $ehLog = $ehLog @ "L2X";
      }
      catch (4)
      {
         $ehLog = $ehLog @ "CATCH3";
      }

      $ehLog = $ehLog @ "RC1";

      // should exit to here
      $fudge21 = "caramel";
   }
   catch (4)
   {
      $ehLog = $ehLog @ "CATCH4";
   }
   $fudge22 = "lemon";
   testString("exc.nativeBound", $ehLog, "L2L2XRC1");
}


test_basicThrow();
testString("exc.test2", $fudge2, "caramel");
test_basicMidThrow();
testString("exc.test3", $fudge3, "caramel");
test_softThrow();
testString("exc.test4", $fudge4, "caramel");
test_multiCatch1();
testString("exc.test5.1", $fudge10, "pie");
test_multiCatch2();
testString("exc.test5.1", $fudge11, "pie");

test_nestedFrameThrow1();
testString("exc.test6.1", $fudge5, "caramel");
testString("exc.test6.2", $fudge6, "lemon");
test_nestedFrameThrow2();
testString("exc.test7.1", $fudge7, "");
testString("exc.test7.2", $fudge8, "lemon");


test_nativeBound();
testString("exc.test8.1", $fudge21, "caramel");
testString("exc.test8.1", $fudge22, "lemon");


