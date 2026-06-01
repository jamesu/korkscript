//-----------------------------------------------------------------------------
// Copyright (c) 2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

function testUntypedOperatorAssignments()
{
	%rawOps = new ScriptObject()
	{
		plusField = 10;
		minusField = 10;
		mulField = 3;
		divField = 8;
		modField = 14;
		andField = 14;
		orField = 5;
		xorField = 6;
		shlField = 3;
		shrField = 12;
	};

	%rawOps.plusField += 5;
	testInt("fn.opassign.rawOps.plusAssign", %rawOps.plusField, 15);

	%rawOps.minusField -= 3;
	testInt("fn.opassign.rawOps.minusAssign", %rawOps.minusField, 7);

	%rawOps.mulField *= 4;
	testInt("fn.opassign.rawOps.mulAssign", %rawOps.mulField, 12);

	%rawOps.divField /= 2;
	testNumber("fn.opassign.rawOps.divAssign", %rawOps.divField, 4);

	%rawOps.modField %= 5;
	testInt("fn.opassign.rawOps.modAssign", %rawOps.modField, 4);

	%rawOps.andField &= 6;
	testInt("fn.opassign.rawOps.andAssign", %rawOps.andField, 6);

	%rawOps.orField |= 8;
	testInt("fn.opassign.rawOps.orAssign", %rawOps.orField, 13);

	%rawOps.xorField ^= 3;
	testInt("fn.opassign.rawOps.xorAssign", %rawOps.xorField, 5);

	%rawOps.shlField <<= 2;
	testInt("fn.opassign.rawOps.shlAssign", %rawOps.shlField, 12);

	%rawOps.shrField >>= 2;
	testInt("fn.opassign.rawOps.shrAssign", %rawOps.shrField, 3);

	%rawOps.incField = 9;
	%rawOps.decField = 9;

	%rawOps.incField++;
	testInt("fn.opassign.rawOps.postInc", %rawOps.incField, 10);

	%rawOps.decField--;
	testInt("fn.opassign.rawOps.postDec", %rawOps.decField, 8);

	%num = 10;
	%num += 5;
	testInt("fn.opassign.numeric.plusAssign", %num, 15);

	%num -= 3;
	testInt("fn.opassign.numeric.minusAssign", %num, 12);

	%num *= 4;
	testInt("fn.opassign.numeric.mulAssign", %num, 48);

	%num /= 6;
	testNumber("fn.opassign.numeric.divAssign", %num, 8);

	%num %= 5;
	testInt("fn.opassign.numeric.modAssign", %num, 3);

	%num &= 2;
	testInt("fn.opassign.numeric.andAssign", %num, 2);

	%num |= 8;
	testInt("fn.opassign.numeric.orAssign", %num, 10);

	%num ^= 3;
	testInt("fn.opassign.numeric.xorAssign", %num, 9);

	%num <<= 1;
	testInt("fn.opassign.numeric.shlAssign", %num, 18);

	%num >>= 2;
	testInt("fn.opassign.numeric.shrAssign", %num, 4);

	%num++;
	testInt("fn.opassign.numeric.postInc", %num, 5);

	%num--;
	testInt("fn.opassign.numeric.postDec", %num, 4);
}

testUntypedOperatorAssignments();
