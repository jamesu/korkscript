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
	%w = %a * 2 * 3;
	%p = (3 * 2) * %a;
	echo(%a);
	echo(%z);
	echo(%w);
	echo(%p);
	testString("basicMyPoint.a.last", %a, "2 4 6");
	testString("basicMyPoint.z.last", %z, "4 8 12");
	testString("basicMyPoint.w.last", %w, "12 24 36");
	testString("basicMyPoint.p.last", %p, "12 24 36");
}


function objectTuples()
{
	%so = new ScriptObject() {
		testField : TypeF32Vector = 1,2,3;
		assignField : TypeF32Vector = "4 5 6";
		assignField2 : TypeS32Vector = "";
		myPointTest: TypeMyPoint3F = 10,20,30;
	};

	%so.assignField = 456;

	%v = 1456;
	%so.assignField2 = %v;// = 4457;


	testString("fn.objtuples.so.myPointTest", %so.myPointTest, "10 20 30");
	%fudge = %so.myPointTest * 3;
	%so.myPointTest *= 2;
	testString("fn.objtuples.so.fudge", %so.myPointTest, "30 60 90");
	testString("fn.objtuples.so.myPointTest2", %so.myPointTest, "20 40 60");


	testString("fn.objtuples.so.testField", %so.testField, "1 2 3");
	testString("fn.objtuples.so.assignField", %so.assignField, "456");
	testString("fn.objtuples.so.assignField2", %so.assignField2, "1456");
}

basicTuples();
basicMyPoint();
objectTuples();
