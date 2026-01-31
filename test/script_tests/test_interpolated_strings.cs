//-----------------------------------------------------------------------------
// Copyright (c) 2025-2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

function basicTest()
{
	$STR1 = "Basic";
	$STR2 = "Pie";

	%expr = 123;
	%blank = $"";
	%literal = $"abc";
	%expr1 = $"{123}";
	%expr2 = $"{%expr}";
	%expr3 = $"abc {123} def";
	%expr4 = $"abc {123} def {456}";
	%expr5 = $"abc { "string1" } def { "string2" }";
	%expr6 = $"abc { $"string1" } def { $"string2" }";
	%expr7 = $"abc { $"strin{GAH}g1" } def { $"string{2}" }";

	testString("fn.blank",   %blank, "");
	testString("fn.literal", %literal, "abc");
	testString("fn.expr1", %expr1, "123");
	testString("fn.expr2", %expr2, "123");
	testString("fn.expr3", %expr3, "abc 123 def");
	testString("fn.expr4", %expr4, "abc 123 def 456");
	testString("fn.expr5", %expr5, "abc string1 def string2");
	testString("fn.expr6", %expr6, "abc string1 def string2");
	testString("fn.expr7", %expr7, "abc strinGAHg1 def string2");
}

basicTest();
