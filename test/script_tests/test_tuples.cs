// NOTE: tuples are set via types; theres no typeless conversion YET


function basicTuples()
{
	echo(start);
	%a = 1;
	%z[0] = 22;
	%b = 2,3,1; // will only set the first param
	%c : TypeS32Vector = 1,2,3;
	%d : TypeS32Vector = %c = 1,2,3;
	echo(blorp);

	echo(%a);
	echo(%b);
	echo(%c);
	echo(%d);

	testString("fn.tuples.a", %a, "1");
	testString("fn.tuples.b", %b, "2");
	testString("fn.tuples.c", %c, "1 2 3");
	testString("fn.tuples.d", %d, "1 2 3");
}

function basicMyPoint()
{
	echo(basicMyPoint);
	%a : TypeMyPoint3F = 4,5,1;
	%b = %a;
	%c : TypeMyPoint3F = %b;
	echo(%a);
	echo(%b);
	echo(%c);
	%a = %b = %c = 1,2,3; // should make 1 2 3 across all
	echo("POST SET");
	echo(%a);
	echo(%b);
	echo(%c);
	echo("**");
	testString("basicMyPoint.a.first", %a, "1 2 3");
	testString("basicMyPoint.b.first", %b, "1 2 3");
	testString("basicMyPoint.c.first", %c, "1 2 3");
	echo(%a);
	echo(%b);
	echo(%c);
	echo(pre);
	echo(mult);
	%a = 2 * %a; // should continue with the point
	%z = %a * 2; // should continue
	echo(%a);
	echo(%z);
	testString("basicMyPoint.a.last", %a, "2 4 6");
	testString("basicMyPoint.z.last", %z, "4 8 12");
}


function objectTuples()
{
	%so = new ScriptObject() {
		testField : TypeF32Vector = 1,2,3;
		assignField : TypeF32Vector = "4 5 6";
		assignField2 : TypeS32Vector = "";
	};

	%so.assignField = 456;

	%v = 1456;
	%so.assignField2 = %v;// = 4457;

	testString("fn.objtuples.so.testField", %so.testField, "1 2 3");
	testString("fn.objtuples.so.assignField", %so.assignField, "456");
	testString("fn.objtuples.so.assignField2", %so.assignField2, "1456");
}

basicTuples();
basicMyPoint();
objectTuples();
