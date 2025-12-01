function fn_add(%a, %b)
{
   return %a + %b;
}

function fn_defaultReturn()
{
   %x = 10;
   %x++;
   // no return;
}

function fn_fact(%n)
{
   if (%n <= 1)
      return 1;

   return %n * fn_fact(%n - 1);
}

function test_functions()
{
   %ok = 1;

   %ok *= testInt("fn.simple",        fn_add(2, 3), 5);
   %ok *= testInt("fn.recursive",     fn_fact(5),   120);

   %implicit = fn_defaultReturn();
   %ok *= testInt("fn.defaultReturn", %implicit, 0);

   TestNamespace::func("sample", 123);
   TestNamespace::func("sample", 123, "");

   return %ok;
}

function test_object_functions()
{
   echo("ADDING ScriptObject");
   %obj = new ScriptObject() {
      class = TestScriptClass;
   };
   echo("Added ScriptObject: " @ %obj);
   %ret = %obj.doThis();
   echo("Exited doThis");
   // NOTE: object fields not working properly yet, thus this workaround
   testString("fn.objectAdd", $didAdd, "YES");
   testInt("fn.objectCall", $pie, 23);
   testString("fn.objectCall.ret", %ret, "LEMON");
   %obj.delete();
}


function TestScriptClass::onAdd(%this)
{
   $didAdd = "YES";
}

function TestScriptClass::onRemove(%this)
{
   $didRemove = "YES";
}

function TestScriptClass::doThis(%this)
{
   $pie = 23;
   return "LEMON";
}

function TestNamespace::func(%a, %b, %c)
{
   testString("fn.nsFunca", %a, "sample");
   testNumber("fn.nsFuncb", %b, 123);
   testString("fn.nsFuncc", %c, "");
}

test_functions();
test_object_functions();
echo("Function tests finished");
