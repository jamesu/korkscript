// Basic core tests

function test_coreIntExpr()
{
   %ok = 1;

   // Arithmetic + precedence
   %a = 1 + 2 * 3;
   %b = (1 + 2) * 3;
   %c = -5 + 10;
   testInt("coreExpr.a", %a, 7);
   testInt("coreExpr.b", %b, 9);
   testInt("coreExpr.c", %c, 5);

   // Relational + logical + ternary
   %cond = (%a == 7) && (%b == 9) && (%c == 5);
   %t    = %cond ? 42 : 0;
   testInt("coreExpr.cond", %t, 42);

   // Bitwise ops
   %x = 1 | 4;
   %y = %x & 6;
   testInt("coreExpr.bitwise", %y, 4);
}

function test_coreFloatExpr()
{
   %ok = 1;

   // Arithmetic + precedence
   %a = 1.1 + 2.0 * 3.0;
   %b = (1.0 + 2.0) * 3.0;
   %c = -5.0 + 10.0;
   testNumber("coreExpr.a", %a, 7.1);
   testNumber("coreExpr.b", %b, 9.0);
   testNumber("coreExpr.c", %c, 5.0);

   // Relational + logical + ternary
   %cond = (%a == 7.1) && (%b == 9.0) && (%c == 5.0);
   %t    = %cond ? 42 : 0;
   testNumber("coreExpr.cond", %t, 42.0);
}

function test_precedence()
{
   // Generic examples
   %a = 1 + 2 * 3;
   %b = (1 + 2) * 3;
   %c = 1 + 2 + 3;
   %d = 10 - 3 - 2;
   %e = 2 * 3 / 4;
   %f = 2 + 3 * 4 - 5;
   %g = -1 + 2;
   %h = !0 && 1 || 0;
   %i = 1 << 2 + 1;
   %j = 1 & 2 | 4;

   testNumber("prec.a", %a, 7);
   testNumber("prec.b", %b, 9);
   testNumber("prec.c", %c, 6);
   testNumber("prec.d", %d, 5);
   testNumber("prec.e", %e, 1.5);
   testNumber("prec.f", %f, 9);
   testNumber("prec.g", %g, 1);
   testNumber("prec.h", %h, 1);
   testNumber("prec.i", %i, 8);
   testNumber("prec.j", %j, 4);

   %k = (%a == 7) && (%b == 9);
   %l = (%c != %d) || (%e >= %f);

   testNumber("prec.k", %k, 1);
   testNumber("prec.l", %l, 1);

   %m = %k ? %a : %b;
   %n = %k ? %a + %b : %c * %d;

   testNumber("prec.m", %m, 7);
   testNumber("prec.n", %n, 16);

   // Concat operator
   %s1 = "foo" @ "bar" @ 123;
   %eq = (%s1 $= "foobar123");

   testNumber("prec.eq", %eq, 1);
}


function test_coreStringExpr()
{
   %a = 7;

   // String concat + string equality
   %s  = "foo" @ "bar";
   testString("coreExpr.strcat", %s, "foobar");

   // Conditional string
   %res = (%a > 3) ? "big" : "small";
   testString("coreExpr.ternary", %res, "big");

   // Concat alias
   %s2 = "x" NL "y";
   %s3 = "x" TAB "y";

   testString("coreExpr.strcat.NL", %s2, "x\ny");
   testString("coreExpr.strcat.TAB", %s3, "x\ty");

   return %ok;
}

function test_controlFlow()
{
   // while loop
   %i   = 0;
   %sum = 0;
   while (%i < 5)
   {
      %sum = %sum + %i;
      %i++;
   }
   // 0+1+2+3+4 = 10
   testInt("loop.while.sum", %sum, 10);

   // for loop
   %sum2 = 0;
   for (%j = 0; %j <= 5; %j++)
   {
      %sum2 += %j;
   }
   // 0+1+2+3+4+5 = 15
   testInt("loop.for.sum2", %sum2, 15);

   // do while
   %k   = 0;
   %sum3 = 0;
   do
   {
      %sum3 = %sum3 + %k;
      %k++;
   }
   while (%k < 4);
   testInt("loop.doWhile.sum3", %sum3, 6); // 0+1+2+3

   // switch
   %v = 1;
   switch (%v)
   {
      case 0:
         return "zero";
      case 2:
         return "two";
      default:
         return "other";
   }
   testString("switch.int", %v, "two");

   // switch$ 
   %v = "b";
   switch$ (%v)
   {
      case "a": %x = 1;
      case "b": %x = 2;
      default:  %x = 3;
   }
   testInt("switch.string", %x, 2);
}


test_coreIntExpr();
test_coreFloatExpr();
test_precedence();
test_coreStringExpr();
test_controlFlow();
