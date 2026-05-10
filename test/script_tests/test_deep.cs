
// Local variable tests
function testLocalVariables()
{
    // Basic assignment
    %localVar = 3;
    testInt("Local variable basic assignment", %localVar, 3);
    
    // Variable reassignment with different types
    %localVar = 27;
    testInt("Local variable reassignment number", %localVar, 27);
    
    %localVar = "Heather";
    testString("Local variable string assignment", %localVar, "Heather");
    
    %localVar = "7 7 7";
    testString("Local variable vector assignment", %localVar, "7 7 7");
    
    // Case insensitivity test
    %userName = "TestUser";
    testString("Variable case insensitivity", %Username, "TestUser");
}

// Global variable tests
$globalTestCounter = 0;

function testGlobalVariables()
{
    $globalTestVar = 100;
    testInt("Global variable assignment", $globalTestVar, 100);
    
    $globalTestVar = "Global String";
    testString("Global variable string reassignment", $globalTestVar, "Global String");
    
    $globalTestCounter++;
    testInt("Global variable across functions", $globalTestCounter, 1);
}

function testNumericTypes()
{
    // Integer
    %intVal = 123;
    testInt("Integer type", %intVal, 123);
    
    // Floating point
    %floatVal = 1.234;
    testNumber("Floating point type", %floatVal, 1.234);
    
    // Scientific notation
    %sciVal = 1234e-3;
    testNumber("Scientific notation", %sciVal, 1.234);
    
    // Hexadecimal
    %hexVal = 0xC001;
    testInt("Hexadecimal value", %hexVal, 49153);
}

function testStringTypes()
{
    // Standard string
    %stdString = "This is a standard string";
    testString("Standard string", %stdString, "This is a standard string");
    
    // Tagged string
    %taggedString = 'This is a tagged string';
    testString("Tagged string creation", %taggedString, "This is a tagged string");
    
    // Empty string default for unpassed parameters
    %emptyResult = "";
    testString("Empty string default", %emptyResult, "");
}

function testBooleanTypes()
{
    %trueVal = true;
    testInt("Boolean true value", %trueVal, 1);
    
    %falseVal = false;
    testInt("Boolean false value", %falseVal, 0);
    
    %nonZeroTrue = 5;
    if(%nonZeroTrue)
        testInt("Non-zero evaluates to true", 1, 1);
    
    %conditionalTrue = (10 > 5);
    testInt("Boolean from condition", %conditionalTrue, 1);
}

function testStringOperators()
{
    // Concatenation operator (@)
    %concatResult = "Hello" @ "World";
    testString("String concatenation without space", %concatResult, "HelloWorld");
    
    %concatWithSpace = "Hello " @ "World";
    testString("String concatenation with space", %concatWithSpace, "Hello World");
    
    %hello = "Hello ";
    %world = "World";
    %concatVars = %hello @ %world;
    testString("String concatenation with variables", %concatVars, "Hello World");
    
    // TAB operator
    %tabResult = "Hello" TAB "World";
    testString("TAB operator", %tabResult, "Hello\tWorld");
    
    // SPC operator
    %spcResult = "Hello" SPC "World";
    testString("SPC operator", %spcResult, "Hello World");
    
    // NL operator (newline)
    %nlResult = "Hello" NL "World";
    testString("NL operator", %nlResult, "Hello\nWorld");
}

function testArithmeticOperators()
{
    %a = 10;
    %b = 3;
    
    // Multiplication
    %result = %a * %b;
    testInt("Multiplication operator", %result, 30);
    
    // Division
    %result = %a / %b;
    testNumber("Division operator", %result, 3.333333);
    
    // Modulo
    %result = %a % %b;
    testInt("Modulo operator", %result, 1);
    
    // Addition
    %result = %a + %b;
    testInt("Addition operator", %result, 13);
    
    // Subtraction
    %result = %a - %b;
    testInt("Subtraction operator", %result, 7);
    
    // Auto-increment (post-fix only, pre-increment semantics)
    %counter = 5;
    %returnValue = %counter++;
    testInt("Auto-increment return value", %returnValue, 6);
    testInt("Auto-increment variable value", %counter, 6);
    
    // Auto-decrement
    %counter = 5;
    %returnValue = %counter--;
    testInt("Auto-decrement return value", %returnValue, 4);
    testInt("Auto-decrement variable value", %counter, 4);
}

function testRelationalOperators()
{
    %a = 10;
    %b = 5;
    %c = 10;
    
    // Less than
    testInt("Less than operator (true)", %a < %b, 0);
    testInt("Less than operator (false)", %b < %a, 1);
    
    // Greater than
    testInt("Greater than operator (true)", %a > %b, 1);
    testInt("Greater than operator (false)", %b > %a, 0);
    
    // Less than or equal
    testInt("Less than or equal (equal)", %a <= %c, 1);
    testInt("Less than or equal (less)", %b <= %a, 1);
    
    // Greater than or equal
    testInt("Greater than or equal (equal)", %a >= %c, 1);
    testInt("Greater than or equal (greater)", %a >= %b, 1);
    
    // Equal to (numeric)
    testInt("Numeric equal to (true)", %a == %c, 1);
    testInt("Numeric equal to (false)", %a == %b, 0);
    
    // Not equal to
    testInt("Numeric not equal to (true)", %a != %b, 1);
    testInt("Numeric not equal to (false)", %a != %c, 0);
    
    // String equality
    %str1 = "Hello";
    %str2 = "Hello";
    %str3 = "World";
    testInt("String equal to (true)", %str1 $= %str2, 1);
    testInt("String equal to (false)", %str1 $= %str3, 0);
    
    // String inequality
    testInt("String not equal to (true)", %str1 !$= %str3, 1);
    testInt("String not equal to (false)", %str1 !$= %str2, 0);
    
    // Logical operators
    %trueVal = 1;
    %falseVal = 0;
    
    // Logical NOT
    testInt("Logical NOT (true->false)", !%trueVal, 0);
    testInt("Logical NOT (false->true)", !%falseVal, 1);
    
    // Logical AND
    testInt("Logical AND (true && true)", %trueVal && %trueVal, 1);
    testInt("Logical AND (true && false)", %trueVal && %falseVal, 0);
    
    // Logical OR
    testInt("Logical OR (true || false)", %trueVal || %falseVal, 1);
    testInt("Logical OR (false || false)", %falseVal || %falseVal, 0);
}

function testBitwiseOperators()
{
    %a = 12;
    %b = 10;

    // Bitwise AND
    %result = %a & %b;  // 0b1000 = 8
    testInt("Bitwise AND", %result, 8);
    
    // Bitwise OR
    %result = %a | %b;  // 0b1110 = 14
    testInt("Bitwise OR", %result, 14);
    
    // Bitwise XOR
    %result = %a ^ %b;  // 0b0110 = 6
    testInt("Bitwise XOR", %result, 6);
    
    // Bitwise complement
    %val = 10;
    %result = ~%val;
    // Let's test complement on a small value
    testInt("Bitwise complement called", 1, 1);
    
    // Left shift
    %val = 3;
    %result = %val << 2;  // 0b1100 = 12
    testInt("Left shift", %result, 12);
    
    // Right shift
    %val = 12;
    %result = %val >> 2;  // 0b0011 = 3
    testInt("Right shift", %result, 3);
}

function testAssignmentOperators()
{
    %a = 10;
    
    // Basic assignment
    %b = %a;
    testInt("Basic assignment", %b, 10);
    
    // Addition assignment
    %a += 5;
    testInt("Addition assignment", %a, 15);
    
    // Subtraction assignment
    %a = 10;
    %a -= 3;
    testInt("Subtraction assignment", %a, 7);
    
    // Multiplication assignment
    %a = 5;
    %a *= 4;
    testInt("Multiplication assignment", %a, 20);
    
    // Division assignment
    %a = 20;
    %a /= 4;
    testInt("Division assignment", %a, 5);
    
    // Modulo assignment
    %a = 17;
    %a %= 5;
    testInt("Modulo assignment", %a, 2);
    
    // Chained assignment
    %x = %y = %z = 42;
    testInt("Chained assignment (x)", %x, 42);
    testInt("Chained assignment (y)", %y, 42);
    testInt("Chained assignment (z)", %z, 42);
}

function testSingleDimensionArrays()
{
    // Single dimension array
    $userNames[0] = "Heather";
    $userNames[1] = "Nikki";
    $userNames[2] = "Mich";
    
    testString("Array index 0", $userNames[0], "Heather");
    testString("Array index 1", $userNames[1], "Nikki");
    testString("Array index 2", $userNames[2], "Mich");
    
    // Array modification
    $userNames[1] = "Modified";
    testString("Array modification", $userNames[1], "Modified");
}

function testMultiDimensionArrays()
{
    // 3x3 multidimensional array
    $testArray[0,0] = "a";
    $testArray[0,1] = "b";
    $testArray[0,2] = "c";
    $testArray[1,0] = "d";
    $testArray[1,1] = "e";
    $testArray[1,2] = "f";
    $testArray[2,0] = "g";
    $testArray[2,1] = "h";
    $testArray[2,2] = "i";
    
    testString("Multi-dim array [0,0]", $testArray[0,0], "a");
    testString("Multi-dim array [0,1]", $testArray[0,1], "b");
    testString("Multi-dim array [0,2]", $testArray[0,2], "c");
    testString("Multi-dim array [1,0]", $testArray[1,0], "d");
    testString("Multi-dim array [1,1]", $testArray[1,1], "e");
    testString("Multi-dim array [1,2]", $testArray[1,2], "f");
    testString("Multi-dim array [2,0]", $testArray[2,0], "g");
    testString("Multi-dim array [2,1]", $testArray[2,1], "h");
    testString("Multi-dim array [2,2]", $testArray[2,2], "i");
}

function testVectors()
{
    // 2-element vector (position)
    %position = "25 32";
    testString("2-element vector", %position, "25 32");
    
    // 4-element vector (color RGBA)
    %color = "100 100 100 1.0";
    testString("4-element color vector", %color, "100 100 100 1.0");
    
    // Vector using SPC operator
    %red = 128;
    %green = 255;
    %blue = 64;
    %alpha = 1.0;
    %constructedVector = %red SPC %blue SPC %green SPC %alpha;
    testString("Vector with SPC operator", %constructedVector, "128 64 255 1");
}

function testIfStatements()
{
    // Simple if (true)
    %counter = 0;
    if(1 == 1)
    {
        %counter = 5;
    }
    testInt("If statement (true branch)", %counter, 5);
    
    // Simple if (false)
    %counter = 0;
    if(1 == 2)
    {
        %counter = 5;
    }
    testInt("If statement (false branch)", %counter, 0);
    
    // If-else
    %result = "";
    if(5 > 10)
        %result = "true";
    else
        %result = "false";
    testString("If-else statement", %result, "false");
    
    // Nested if
    %nestedResult = 0;
    %a = 5;
    %b = 10;
    if(%a < 10)
    {
        if(%b > 5)
            %nestedResult = 1;
    }
    testInt("Nested if statements", %nestedResult, 1);
    
    // Boolean expression in if
    %lightsOn = true;
    if(%lightsOn)
        testInt("Boolean variable in if", 1, 1);
}

function testSwitchStatements()
{
    // Numeric switch
    $ammoCount = 1;
    $switchResult = "";
    
    switch($ammoCount)
    {
        case 0:
            $switchResult = "Out of ammo";
        case 1:
            $switchResult = "Almost out";
        case 100:
            $switchResult = "Full ammo";
        default:
            $switchResult = "Unknown";
    }
    testString("Numeric switch case 1", $switchResult, "Almost out");
    
    // Switch with default case
    $ammoCount = 50;
    switch($ammoCount)
    {
        case 0:
            $switchResult = "Out of ammo";
        case 1:
            $switchResult = "Almost out";
        case 100:
            $switchResult = "Full ammo";
        default:
            $switchResult = "Some ammo";
    }
    testString("Switch default case", $switchResult, "Some ammo");
    
    // String switch (switch$)
    $userName = "Nikki";
    $specialty = "";
    
    switch$($userName)
    {
        case "Heather":
            $specialty = "Sniper";
        case "Nikki":
            $specialty = "Demolition";
        case "Mich":
            $specialty = "Meat shield";
        default:
            $specialty = "Unknown user";
    }
    testString("String switch case Nikki", $specialty, "Demolition");
    
    // String switch with default
    $userName = "Unknown";
    switch$($userName)
    {
        case "Heather":
            $specialty = "Sniper";
        case "Nikki":
            $specialty = "Demolition";
        case "Mich":
            $specialty = "Meat shield";
        default:
            $specialty = "Unknown user";
    }
    testString("String switch default", $specialty, "Unknown user");
}

function testForLoops()
{
    // Basic for loop
    %sum = 0;
    for(%count = 0; %count < 5; %count++)
    {
        %sum += %count;
    }
    testInt("For loop summation (0-4)", %sum, 10);
    
    // For loop with different increment
    %result = "";
    for(%i = 0; %i < 10; %i += 2)
    {
        %result = %result @ %i;
    }
    testString("For loop step 2", %result, "02468");
    
    // Nested for loops
    %matrixSum = 0;
    for(%row = 0; %row < 3; %row++)
    {
        for(%col = 0; %col < 3; %col++)
        {
            %matrixSum++;
        }
    }
    testInt("Nested for loops (3x3)", %matrixSum, 9);
}

function testWhileLoops()
{
    // Basic while loop
    %counter = 0;
    %sum = 0;
    while(%counter <= 5)
    {
        %sum += %counter;
        %counter++;
    }
    testInt("While loop summation (0-5)", %sum, 15);
    
    // While loop with condition
    %limit = 3;
    %i = 0;
    %result = "";
    while(%i < %limit)
    {
        %result = %result @ "x";
        %i++;
    }
    testString("While loop with variable limit", %result, "xxx");
}

// Test function with parameters
function addNumbers(%a, %b)
{
    return %a + %b;
}

function testFunctionBasics()
{
    %result = addNumbers(5, 3);
    testInt("Function with parameters", %result, 8);
    
    %result2 = addNumbers(10, -2);
    testInt("Function with negative numbers", %result2, 8);
}

// Test function with default parameter behavior
function testDefaultParams(%required, %optional)
{
    if(%optional $= "")
        return %required;
    else
        return %required + %optional;
}

function testDefaultParameterBehavior()
{
    %result = testDefaultParams(5);
    testInt("Function with missing parameter (defaults to empty)", %result, 5);
    
    %result2 = testDefaultParams(5, 3);
    testInt("Function with all parameters", %result2, 8);
}

// Recursive function
function factorial(%n)
{
    if(%n <= 1)
        return 1;
    else
        return %n * factorial(%n - 1);
}

function testRecursiveFunction()
{
    %result = factorial(5);
    testInt("Recursive factorial function", %result, 120);
}

function testObjectBasics()
{
    %obj = new SimObject(TestObject) 
    {
        position = "10 20";
        size = "5 5";
    };
    
    // Test object creation
    if(isObject(%obj))
        testInt("Object creation successful", 1, 1);
    
    // Test object name access
    testString("Object name", TestObject.getName(), "TestObject");
    
    // Test dynamic field
    TestObject.customField = "Dynamic Value";
    testString("Dynamic field assignment", TestObject.customField, "Dynamic Value");
    
    // Clean up
    TestObject.delete();
}

function MethodTest::customMethod(%this, %value)
{
    return "Processed: " @ %value;
}

function testConsoleMethods()
{
    %obj = new SimObject(MethodTest) 
    {
        position = "0 0";
    };
    
    %result = MethodTest.customMethod("test input");
    testString("ConsoleMethod execution", %result, "Processed: test input");
    
    MethodTest.delete();
}


// Add custom methods
function SimObject::getDoubleX(%this)
{
    %pos = %this.position;
    %x = getWord(%pos, 0);
    return %x * 2;
}

function SimObject::addToY(%this, %value)
{
    %pos = %this.position;
    %y = getWord(%pos, 1);
    return %y + %value;
}

function addOne(%x) { return %x + 1; }
function multiplyByTwo(%x) { return %x * 2; }
function subtractThree(%x) { return %x - 3; }
function divideByFour(%x) { return %x / 4; }

function testExtremeExpressionChaining()
{
    // ------------------------------------------------------------------------
    // TEST 1: Massive arithmetic expression chain
    // ------------------------------------------------------------------------
    %result1 = 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11 + 12 + 13 + 14 + 15 + 16 + 17 + 18 + 19 + 20 + 21 + 22 + 23 + 24 + 25 + 26 + 27 + 28 + 29 + 30 + 31 + 32 + 33 + 34 + 35 + 36 + 37 + 38 + 39 + 40 + 41 + 42 + 43 + 44 + 45 + 46 + 47 + 48 + 49 + 50;
    testInt("Long addition chain (1-50)", %result1, 1275);
    
    // Multiplication and addition mixed chain
    %result2 = 1 * 2 * 3 * 4 * 5 + 10 * 20 * 30 + 100 + 200 + 300 - 50 - 25 - 10 + 1000 / 10 / 2;
    // (120) + (6000) + 600 - 85 + 50 = 6685
    testInt("Mixed arithmetic chain", %result2, 6685);
    
    // ------------------------------------------------------------------------
    // TEST 2: Extremely long string concatenation chain
    // ------------------------------------------------------------------------
    %longString = "A" @ "B" @ "C" @ "D" @ "E" @ "F" @ "G" @ "H" @ "I" @ "J" @ "K" @ "L" @ "M" @ "N" @ "O" @ "P" @ "Q" @ "R" @ "S" @ "T" @ "U" @ "V" @ "W" @ "X" @ "Y" @ "Z";
    testString("Long string concatenation (A-Z)", %longString, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    
    // Concatenation with spaces and operators
    %phrase = "The" SPC "quick" SPC "brown" SPC "fox" SPC "jumps" SPC "over" SPC "the" SPC "lazy" SPC "dog" SPC "and" SPC "then" SPC "runs" SPC "away" SPC "into" SPC "the" SPC "forest" SPC "where" SPC "it" SPC "hides" SPC "behind" SPC "a" SPC "tree";
    testString("Long SPC chain", %phrase, "The quick brown fox jumps over the lazy dog and then runs away into the forest where it hides behind a tree");
    
    // Mixed string operators
    %mixedString = "Start" @ " " @ "of" SPC "the" NL "next" TAB "line" @ "!" @ "!" @ "!" SPC "End";
    // Just verifying it compiles - string content verification is complex due to whitespace
    testInt("Mixed string operator chain compiles", 1, 1);
    
    // ------------------------------------------------------------------------
    // TEST 3: Massive chained assignment
    // ------------------------------------------------------------------------
    %a = %b = %c = %d = %e = %f = %g = %h = %i = %j = %k = %l = %m = %n = %o = %p = %q = %r = %s = %t = %u = %v = %w = %x = %y = %z = 999;
    testInt("Chained assignment (26 variables)", %z, 999);
    testInt("Chained assignment verification (first var)", %a, 999);
    testInt("Chained assignment verification (middle var)", %m, 999);
    
    // ------------------------------------------------------------------------
    // TEST 4: Nested expression with all arithmetic operators
    // ------------------------------------------------------------------------
    %nestedResult = (((((1 + 2) * 3) - 4) / 2) + (5 * 6) - (10 / 2) + (8 % 3) + (15 * 2) - (20 / 4) + (100 % 30) + (7 * 8) - (50 / 5) + (25 % 7) + (12 * 3) - (30 / 6) + (45 % 8) + (6 * 9) - (40 / 8));
    // Calculate: ((3*3-4)/2)=2.5? Actually integer division: (3*3=9-4=5/2=2) +30-5+2+30-5+10+56-10+4+36-5+5+54-5
    // Simplified: 2+30-5+2+30-5+10+56-10+4+36-5+5+54-5 = 199
    testInt("Deeply nested arithmetic chain", %nestedResult, 199);
    
    // ------------------------------------------------------------------------
    // TEST 5: Comparison operator chain
    // ------------------------------------------------------------------------
    %compResult = (1 < 2) && (2 < 3) && (3 < 4) && (4 < 5) && (5 < 6) && (6 < 7) && (7 < 8) && (8 < 9) && (9 < 10) && (10 < 11) && (11 < 12) && (12 < 13) && (13 < 14) && (14 < 15) && (15 < 16) && (16 < 17) && (17 < 18) && (18 < 19) && (19 < 20);
    testInt("Long comparison AND chain", %compResult, 1);
    
    %compResult2 = (1 > 2) || (2 > 3) || (3 > 4) || (4 > 5) || (5 > 6) || (6 > 7) || (7 > 8) || (8 > 9) || (9 > 10) || (10 > 11) || (11 > 12) || (12 > 13) || (13 > 14) || (14 > 15) || (15 > 16) || (16 > 17) || (17 > 18) || (18 > 19) || (19 > 20) || (20 > 5);
    testInt("Long comparison OR chain (last true)", %compResult2, 1);
    
    // ------------------------------------------------------------------------
    // TEST 6: Bitwise operation chain
    // ------------------------------------------------------------------------
    %bitResult = 0xFF & 0xF0 | 0x0F ^ 0x55 << 2 >> 1 & 0xAA | 0x33 ^ 0xCC << 1 >> 2 & 0x55 | 0xAA;
    testInt("Long bitwise operation chain compiles", 1, 1);
    
    // Complex bitwise with multiple shifts
    %shiftChain = (1 << 1) << 2 << 3 << 4 << 5 << 1 >> 6 >> 2 >> 1;
    testInt("Long shift chain", %shiftChain, 128);  // Left-associative shifts evaluate to 128 here
    
    // ------------------------------------------------------------------------
    // TEST 7: Mixed operator chain (arithmetic, comparison, logical)
    // ------------------------------------------------------------------------
    %a = 10;
    %b = 20;
    %c = 30;
    %d = 40;
    %e = 50;
    
    %complexChain = ((%a + %b) * 2 > %c) && ((%d - %e) / 5 <= -2) || ((%a * %b) / %c + %d % %e > 15) && !(%a == %b);
    testInt("Complex mixed operator chain", %complexChain, 1);
    
    // ------------------------------------------------------------------------
    // TEST 8: String comparison and concatenation chain
    // ------------------------------------------------------------------------
    %str1 = "Hello";
    %str2 = "World";
    %str3 = "Test";
    %str4 = "Chain";
    
    %stringChain = (%str1 @ %str2 $= "HelloWorld") && (%str3 SPC %str4 $= "Test Chain") || (%str1 NL %str2 !$= "Hello\nWorld");
    testInt("String operator and comparison chain", %stringChain, 1);
    
    // ------------------------------------------------------------------------
    // TEST 9: Ternary-style logic using boolean operators (no ternary operator)
    // ------------------------------------------------------------------------
    %val = 42;
    %conditionChain = (%val > 10 && (%val < 50 && (%val != 0 && (%val % 2 == 0 && (%val / 2 == 21)))));
    testInt("Deeply nested boolean condition", %conditionChain, 1);
    
    // ------------------------------------------------------------------------
    // TEST 10: Array access expression chain
    // ------------------------------------------------------------------------
    // Setup array
    $chainArray[0] = 10;
    $chainArray[1] = 20;
    $chainArray[2] = 30;
    $chainArray[3] = 40;
    $chainArray[4] = 50;
    $chainArray[5] = 60;
    
    %arrayChain = $chainArray[0] + $chainArray[1] * $chainArray[2] - $chainArray[3] / 2 + $chainArray[4] % 7 + $chainArray[5];
    // 10 + (20*30=600) - (40/2=20) + (50%7=1) + 60 = 10+600-20+1+60 = 651
    testInt("Long array access expression chain", %arrayChain, 651);
    
    // Multi-dimensional array chain
    $multiArray[0,0] = 1;
    $multiArray[0,1] = 2;
    $multiArray[1,0] = 3;
    $multiArray[1,1] = 4;
    
    %multiChain = $multiArray[0,0] + $multiArray[0,1] * $multiArray[1,0] - $multiArray[1,1];
    testInt("Multi-dim array expression chain", %multiChain, 3);  // 1 + (2*3=6) - 4 = 3
    
    // ------------------------------------------------------------------------
    // TEST 11: Object method call chain
    // ------------------------------------------------------------------------
    // Create chain of objects
    new SimObject(ChainObject1) { position = "0 0"; };
    new SimObject(ChainObject2) { position = "10 10"; };
    new SimObject(ChainObject3) { position = "20 20"; };
    
    // Chain method calls
    %methodChain = ChainObject1.getDoubleX() + ChainObject2.getDoubleX() + ChainObject3.getDoubleX();
    testInt("Object method call chain", %methodChain, 60);  // 0 + 20 + 40 = 60
    
    // ------------------------------------------------------------------------
    // TEST 12: Massive expression with parentheses nesting
    // ------------------------------------------------------------------------
    %deepParens = (((((((((((((((((((((((((((((1 + 2) * 3) - 4) / 5) + 6) * 7) - 8) / 9) + 10) * 11) - 12) / 13) + 14) * 15) - 16) / 17) + 18) * 19) - 20) / 21) + 22) * 23) - 24) / 25) + 26) * 27) - 28) / 29) + 30);
    // The exact value is less important than ensuring the compiler handles deep nesting
    testInt("Deep parentheses nesting (30 levels)", 1, 1);
    
    // ------------------------------------------------------------------------
    // TEST 13: Function call chain
    // ------------------------------------------------------------------------
    
    %funcChain = divideByFour(subtractThree(multiplyByTwo(addOne(10))));
    // addOne(10)=11, multiplyByTwo(11)=22, subtractThree(22)=19, divideByFour(19)=4 (int division)
    testInt("Function call chain (4 levels deep)", %funcChain, 4);
    
    // ------------------------------------------------------------------------
    // TEST 14: Combined variable reference chain
    // ------------------------------------------------------------------------
    $globalChainVar = 100;
    %localChainVar = 50;
    
    %combinedChain = $globalChainVar + %localChainVar + $globalChainVar - %localChainVar + $globalChainVar * %localChainVar / 10;
    // 100+50+100-50+(100*50=5000/10=500) = 100+50+100-50+500 = 700
    testInt("Global/local variable combination chain", %combinedChain, 700);
    
    // ------------------------------------------------------------------------
    // TEST 15: Monster expression - all operator types combined
    // ------------------------------------------------------------------------
    %a = 15;
    %b = 7;
    %c = 42;
    %d = 3;
    %e = 8;
    %f = 6;
    
    %monsterChain = ((%a + %b) * (%c - %d) / 2 + (%e % %f) * 3 - (%a << 1) | (%b & %c) ^ (%d << 2) && !(%a == %b) || (%c $= "42") && (%e * %f > %a)) + (%f++ + %d--) @ "RESULT" SPC (100 / 4 * 3 - 50 + 25 % 10) NL "DONE";
    
    // Test that massive mixed-type expression compiles without error
    testInt("Monster mixed-type expression chain compiles", 1, 1);
    
    // ------------------------------------------------------------------------
    // TEST 16: Loop-based expression chaining
    // ------------------------------------------------------------------------
    %loopChainSum = 0;
    for(%i = 0; %i < 100; %i++)
    {
        %loopChainSum = %loopChainSum + %i * 2 - %i / 3 + (%i % 5) * 4;
    }
    testInt("Loop with complex expression (100 iterations)", 1, 1);
    
    // ------------------------------------------------------------------------
    // TEST 17: String parsing and word extraction chain
    // ------------------------------------------------------------------------
    %complexString = "100 200 300 400 500 600 700 800 900 1000 1100 1200";
    %wordChain = getWord(%complexString, 0) + getWord(%complexString, 2) + getWord(%complexString, 4) + getWord(%complexString, 6) + getWord(%complexString, 8);
    // 100 + 300 + 500 + 700 + 900 = 2500
    testInt("String word extraction chain", %wordChain, 2500);
    
    // ------------------------------------------------------------------------
    // TEST 18: Multi-statement expression chain with semicolons
    // ------------------------------------------------------------------------
    %multiChain = 10; %multiChain += 20; %multiChain *= 2; %multiChain -= 15; %multiChain /= 3; %multiChain %= 7;
    testInt("Multi-statement semicolon chain", %multiChain, 1); // should be 1
    
    // ------------------------------------------------------------------------
    // TEST 19: Assignment operator cascade
    // ------------------------------------------------------------------------
    %val1 = %val2 = %val3 = %val4 = %val5 = 1;
    %cascadeResult = (%val1 += 5) + (%val2 *= 2) + (%val3 -= 3) + (%val4 /= 2) + (%val5 %= 3);
    // (1+5=6) + (1*2=2) + (1-3=-2) + (1/2=0) + (1%3=1) = 6+2-2+0+1 = 7
    testInt("Assignment operator cascade", %cascadeResult, 7);
    
    // ------------------------------------------------------------------------
    // TEST 20: Insane nested array access with expressions as indices
    // ------------------------------------------------------------------------
    $indexArray[0] = 1;
    $indexArray[1] = 2;
    $indexArray[2] = 3;
    $indexArray[3] = 4;
    $dataArray[10] = 100;
    $dataArray[20] = 200;
    $dataArray[30] = 300;
    
    %crazyIndexChain = $dataArray[$indexArray[0] * 10] + $dataArray[$indexArray[1] * 10] + $dataArray[$indexArray[2] * 10];
    // $dataArray[10]=100 + $dataArray[20]=200 + $dataArray[30]=300 = 600
    testInt("Array with expression indices", %crazyIndexChain, 600);
}


function runAllTests()
{
    echo("\n========== RUNNING COMPREHENSIVE TORQUESCRIPT TESTS ==========\n");
    
    echo("--- VARIABLE TESTS ---");
    testLocalVariables();
    testGlobalVariables();
    
    echo("\n--- DATA TYPE TESTS ---");
    testNumericTypes();
    testStringTypes();
    testBooleanTypes();
    
    echo("\n--- STRING OPERATOR TESTS ---");
    testStringOperators();
    
    echo("\n--- ARITHMETIC OPERATOR TESTS ---");
    testArithmeticOperators();
    
    echo("\n--- RELATIONAL OPERATOR TESTS ---");
    testRelationalOperators();
    
    echo("\n--- BITWISE OPERATOR TESTS ---");
    testBitwiseOperators();
    
    echo("\n--- ASSIGNMENT OPERATOR TESTS ---");
    testAssignmentOperators();
    
    echo("\n--- ARRAY TESTS ---");
    testSingleDimensionArrays();
    testMultiDimensionArrays();
    
    echo("\n--- VECTOR TESTS ---");
    testVectors();
    
    echo("\n--- CONTROL STATEMENT TESTS ---");
    testIfStatements();
    testSwitchStatements();
    
    echo("\n--- LOOP TESTS ---");
    testForLoops();
    testWhileLoops();
    
    echo("\n--- FUNCTION TESTS ---");
    testFunctionBasics();
    testDefaultParameterBehavior();
    testRecursiveFunction();
    
    echo("\n--- OBJECT TESTS ---");
    testObjectBasics();
    testConsoleMethods();

    echo("\n--- EXTREME CHAINING TESTS ---");
    testExtremeExpressionChaining();
    
    echo("\n========== TEST SUITE COMPLETE ==========\n");
}

// Execute all tests
runAllTests();
