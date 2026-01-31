//-----------------------------------------------------------------------------
// Copyright (c) 2025-2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

function myTestPointOperators()
{
   // ------------------------------------------------------------
   // Setup typed points
   %a : TypeMyPoint3F = 1,2,3;
   %b : TypeMyPoint3F = 4,5,6;

   // Another set for division tests
   %c : TypeMyPoint3F = 8,9,10;
   %d : TypeMyPoint3F = 2,3,5;

   // A point with zeros to test divide-by-zero behavior (per-component)
   %z : TypeMyPoint3F = 0,2,0;

   // Scalars (raw number + numeric strings)
   %s  = 2;
   %sf = 2.5;
   %ss = "2";
   %ssf = "2.5";

   // ------------------------------------------------------------
   // point <op> point
   testString("P+P (1 2 3)+(4 5 6)",        (%a + %b), "5 7 9");
   testString("P-P (1 2 3)-(4 5 6)",        (%a - %b), "-3 -3 -3");
   testString("P*P (1 2 3)*(4 5 6)",        (%a * %b), "4 10 18");
   testString("P/P (8 9 10)/(2 3 5)",       (%c / %d), "4 3 2");

   // unary negation
   //testString("NEG P -(1 2 3)",             (-%a),     "-1 -2 -3");

   // ------------------------------------------------------------
   // point <op> scalar (raw numbers)
   testString("P+S (1 2 3)+2",              (%a + %s),  "3 4 5");
   testString("P-S (1 2 3)-2",              (%a - %s),  "-1 0 1");
   testString("P*S (1 2 3)*2",              (%a * %s),  "2 4 6");
   testString("P/S (1 2 3)/2",              (%a / %s),  "0.5 1 1.5");

   // scalar <op> point (raw numbers) — both operand orders
   testString("S+P 2+(1 2 3)",              (%s + %a),  "3 4 5");
   testString("S-P 2-(1 2 3)",              (%s - %a),  "1 0 -1");
   testString("S*P 2*(1 2 3)",              (%s * %a),  "2 4 6");

   // float scalar variants (just to ensure float paths work)
   testString("P*Sf (1 2 3)*2.5",           (%a * %sf), "2.5 5 7.5");
   testString("Sf*P 2.5*(1 2 3)",           (%sf * %a), "2.5 5 7.5");

   // ------------------------------------------------------------
   // point <op> numeric-string scalar
   testString("P+\"2\" (1 2 3)+\"2\"",      (%a + %ss),  "3 4 5");
   testString("P-\"2\" (1 2 3)-\"2\"",      (%a - %ss),  "-1 0 1");
   testString("P*\"2\" (1 2 3)*\"2\"",      (%a * %ss),  "2 4 6");
   testString("P/\"2\" (1 2 3)/\"2\"",      (%a / %ss),  "0.5 1 1.5");

   // numeric-string scalar <op> point — both operand orders
   testString("\"2\"+P \"2\"+(1 2 3)",      (%ss + %a),  "3 4 5");
   testString("\"2\"-P \"2\"-(1 2 3)",      (%ss - %a),  "1 0 -1");
   testString("\"2\"*P \"2\"*(1 2 3)",      (%ss * %a),  "2 4 6");

   // float numeric-string variants
   testString("P*\"2.5\" (1 2 3)*\"2.5\"",  (%a * %ssf), "2.5 5 7.5");
   testString("\"2.5\"*P \"2.5\"*(1 2 3)",  (%ssf * %a), "2.5 5 7.5");

   // ------------------------------------------------------------
   // divide-by-zero behavior
   // (1 2 3) / (0 2 0) => (0 1 0)
   testString("P/P div0 (1 2 3)/(0 2 0)",   (%a / %z), "0 1 0");

   // scalar / point with zeros: 6 / (0 2 0) => (0 3 0)
   testString("S/P div0 6/(0 2 0)",         (6 / %z), "0 3 0");

   // ------------------------------------------------------------
   // assign-op coverage (+=, -=, *=, /=) with point + scalar + point
   %t : TypeMyPoint3F = 1, 1, 1;
   %t += %a; // (1 1 1)+(1 2 3) = (2 3 4)
   testString("ASSIGN += P (1 1 1)+=(1 2 3)", %t, "2 3 4");

   %t = 10, 10, 10;
   %t -= 2;  // 2 gets turned into scalar thus 8,8,8
   testString("ASSIGN -= S (10 10 10)-=2",    %t, "8 8 8");

   %t = 1,2,3;
   %t *= "2"; // (2 4 6)
   testString("ASSIGN *= \"2\" (1 2 3)*=\"2\"", %t, "2 4 6");

   %t = 1,2,3;
   %t /= 2; // (0.5 1 1.5)
   testString("ASSIGN /= 2 (1 2 3)/=2",       %t, "0.5 1 1.5");

   // t against itself
   %t = 1,2,3;
   %t += %t;
   testString("ASSIGN += self", %t, "2 4 6");

   // unary neg on an assign result
   %t = 1,2,3;
   %t = -%t;
   testString("ASSIGN unary NEG %t=-%t", %t, "-1 -2 -3");

   testString("Typed return", testFunctionTyped(), "4 3 1");
}

function testFunctionTyped() : TypeMyPoint3F
{
	return "4 3 1"; // should cast to TypeMyPoint3F
}

myTestPointOperators();
