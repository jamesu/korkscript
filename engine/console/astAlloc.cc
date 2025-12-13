//-----------------------------------------------------------------------------
// Copyright (c) 2013 GarageGames, LLC
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------


#include "embed/api.h"
#include "embed/internalApi.h"
#include "console/compiler.h"
#include "console/consoleInternal.h"

using namespace Compiler;

/// @file
///
/// TorqueScript AST node allocators.
///
/// These static methods exist to allocate new AST node for the compiler. They
/// all allocate memory from the res->consoleAllocator for efficiency, and often take
/// arguments relating to the state of the nodes. They are called from gram.y
/// (really gram.c) as the lexer analyzes the script code.

//------------------------------------------------------------

BreakStmtNode *BreakStmtNode::alloc( Compiler::Resources* res, S32 lineNumber )
{
   BreakStmtNode *ret = (BreakStmtNode *) res->consoleAlloc(sizeof(BreakStmtNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   return ret;
}

ContinueStmtNode *ContinueStmtNode::alloc( Compiler::Resources* res, S32 lineNumber )
{
   ContinueStmtNode *ret = (ContinueStmtNode *) res->consoleAlloc(sizeof(ContinueStmtNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   return ret;
}

ReturnStmtNode *ReturnStmtNode::alloc( Compiler::Resources* res, S32 lineNumber, ExprNode *expr)
{
   ReturnStmtNode *ret = (ReturnStmtNode *) res->consoleAlloc(sizeof(ReturnStmtNode));
   constructInPlace(ret);
   ret->expr = expr;
   ret->dbgLineNumber = lineNumber;
   
   return ret;
}

IfStmtNode *IfStmtNode::alloc( Compiler::Resources* res, S32 lineNumber, ExprNode *testExpr, StmtNode *ifBlock, StmtNode *elseBlock, bool propagate )
{
   IfStmtNode *ret = (IfStmtNode *) res->consoleAlloc(sizeof(IfStmtNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   
   ret->testExpr = testExpr;
   ret->ifBlock = ifBlock;
   ret->elseBlock = elseBlock;
   ret->propagate = propagate;
   ret->integer = 0;
   
   return ret;
}

LoopStmtNode *LoopStmtNode::alloc( Compiler::Resources* res, S32 lineNumber, ExprNode *initExpr, ExprNode *testExpr, ExprNode *endLoopExpr, StmtNode *loopBlock, bool isDoLoop )
{
   LoopStmtNode *ret = (LoopStmtNode *) res->consoleAlloc(sizeof(LoopStmtNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->testExpr = testExpr;
   ret->initExpr = initExpr;
   ret->endLoopExpr = endLoopExpr;
   ret->loopBlock = loopBlock;
   ret->isDoLoop = isDoLoop;
   ret->integer = 0;
   
   // Deal with setting some dummy constant nodes if we weren't provided with
   // info... This allows us to play nice with missing parts of for(;;) for
   // instance.
   if(!ret->testExpr) ret->testExpr = IntNode::alloc( res, lineNumber, 1 );
   
   return ret;
}

IterStmtNode* IterStmtNode::alloc( Compiler::Resources* res, S32 lineNumber, StringTableEntry varName, ExprNode* containerExpr, StmtNode* body, bool isStringIter )
{
   IterStmtNode* ret = ( IterStmtNode* ) res->consoleAlloc( sizeof( IterStmtNode ) );
   constructInPlace( ret );
   
   ret->dbgLineNumber = lineNumber;
   ret->varName = varName;
   ret->containerExpr = containerExpr;
   ret->body = body;
   ret->isStringIter = isStringIter;
   
   return ret;
}

FloatBinaryExprNode *FloatBinaryExprNode::alloc( Compiler::Resources* res, S32 lineNumber, const SimpleLexer::TokenType op, ExprNode *left, ExprNode *right )
{
   FloatBinaryExprNode *ret = (FloatBinaryExprNode *) res->consoleAlloc(sizeof(FloatBinaryExprNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   
   ret->op = op;
   ret->left = left;
   ret->right = right;
   
   return ret;
}

IntBinaryExprNode *IntBinaryExprNode::alloc( Compiler::Resources* res, S32 lineNumber, const SimpleLexer::TokenType op, ExprNode *left, ExprNode *right )
{
   IntBinaryExprNode *ret = (IntBinaryExprNode *) res->consoleAlloc(sizeof(IntBinaryExprNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   
   ret->op = op;
   ret->left = left;
   ret->right = right;
   ret->subType = TypeReqNone;
   ret->operand = 0;
   
   return ret;
}

StreqExprNode *StreqExprNode::alloc( Compiler::Resources* res, S32 lineNumber, ExprNode *left, ExprNode *right, bool eq )
{
   StreqExprNode *ret = (StreqExprNode *) res->consoleAlloc(sizeof(StreqExprNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->left = left;
   ret->right = right;
   ret->eq = eq;
   
   return ret;
}

StrcatExprNode *StrcatExprNode::alloc( Compiler::Resources* res, S32 lineNumber, ExprNode *left, ExprNode *right, int appendChar )
{
   StrcatExprNode *ret = (StrcatExprNode *) res->consoleAlloc(sizeof(StrcatExprNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->left = left;
   ret->right = right;
   ret->appendChar = appendChar;
   
   return ret;
}

CommaCatExprNode *CommaCatExprNode::alloc( Compiler::Resources* res, S32 lineNumber, ExprNode *left, ExprNode *right )
{
   CommaCatExprNode *ret = (CommaCatExprNode *) res->consoleAlloc(sizeof(CommaCatExprNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->left = left;
   ret->right = right;
   
   return ret;
}

IntUnaryExprNode *IntUnaryExprNode::alloc( Compiler::Resources* res, S32 lineNumber, const SimpleLexer::TokenType op, ExprNode *expr )
{
   IntUnaryExprNode *ret = (IntUnaryExprNode *) res->consoleAlloc(sizeof(IntUnaryExprNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->op = op;
   ret->expr = expr;
   ret->integer = 0;
   return ret;
}

FloatUnaryExprNode *FloatUnaryExprNode::alloc( Compiler::Resources* res, S32 lineNumber, const SimpleLexer::TokenType op, ExprNode *expr )
{
   FloatUnaryExprNode *ret = (FloatUnaryExprNode *) res->consoleAlloc(sizeof(FloatUnaryExprNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->op = op;
   ret->expr = expr;
   return ret;
}

VarNode *VarNode::alloc( Compiler::Resources* res, S32 lineNumber, StringTableEntry varName, ExprNode *arrayIndex, StringTableEntry typeName )
{
   VarNode *ret = (VarNode *) res->consoleAlloc(sizeof(VarNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->varName = varName;
   ret->arrayIndex = arrayIndex;
   ret->varType = typeName;
   return ret;
}

IntNode *IntNode::alloc( Compiler::Resources* res, S32 lineNumber, S32 value )
{
   IntNode *ret = (IntNode *) res->consoleAlloc(sizeof(IntNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->value = value;
   return ret;
}

ConditionalExprNode *ConditionalExprNode::alloc( Compiler::Resources* res, S32 lineNumber, ExprNode *testExpr, ExprNode *trueExpr, ExprNode *falseExpr )
{
   ConditionalExprNode *ret = (ConditionalExprNode *) res->consoleAlloc(sizeof(ConditionalExprNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->testExpr = testExpr;
   ret->trueExpr = trueExpr;
   ret->falseExpr = falseExpr;
   ret->integer = false;
   return ret;
}

FloatNode *FloatNode::alloc( Compiler::Resources* res, S32 lineNumber, F64 value )
{
   FloatNode *ret = (FloatNode *) res->consoleAlloc(sizeof(FloatNode));
   constructInPlace(ret);
   
   ret->dbgLineNumber = lineNumber;
   ret->value = value;
   return ret;
}

StrConstNode *StrConstNode::alloc( Compiler::Resources* res, S32 lineNumber, char *str, bool tag, bool doc, S32 forceLen)
{
   StrConstNode *ret = (StrConstNode *) res->consoleAlloc(sizeof(StrConstNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->str = (char *) res->consoleAlloc((U32)dAlignSize(forceLen >= 0 ? forceLen+1 : dStrlen(str) + 1, 8));
   ret->tag = tag;
   ret->doc = doc;
   
   if (forceLen >= 0)
   {
      memcpy(ret->str, str, forceLen);
      ret->str[forceLen] = '\0';
   }
   else
   {
      dStrcpy(ret->str, str);
   }
   
   return ret;
}

ConstantNode *ConstantNode::alloc( Compiler::Resources* res, S32 lineNumber, StringTableEntry value )
{
   ConstantNode *ret = (ConstantNode *) res->consoleAlloc(sizeof(ConstantNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->value = value;
   return ret;
}

AssignExprNode *AssignExprNode::alloc( Compiler::Resources* res, S32 lineNumber, StringTableEntry varName, ExprNode *arrayIndex, ExprNode *expr, StringTableEntry typeName )
{
   AssignExprNode *ret = (AssignExprNode *) res->consoleAlloc(sizeof(AssignExprNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->varName = varName;
   ret->rhsExpr = expr;
   ret->arrayIndex = arrayIndex;
   ret->subType = TypeReqNone;
   ret->assignTypeName = typeName;
   
   return ret;
}

AssignOpExprNode *AssignOpExprNode::alloc( Compiler::Resources* res, S32 lineNumber, StringTableEntry varName, ExprNode *arrayIndex, ExprNode *expr, const SimpleLexer::TokenType op )
{
   AssignOpExprNode *ret = (AssignOpExprNode *) res->consoleAlloc(sizeof(AssignOpExprNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->varName = varName;
   ret->rhsExpr = expr;
   ret->arrayIndex = arrayIndex;
   ret->subType = TypeReqNone;
   ret->op = op;
   ret->operand = 0;
   return ret;
}

TTagSetStmtNode *TTagSetStmtNode::alloc( Compiler::Resources* res, S32 lineNumber, StringTableEntry tag, ExprNode *valueExpr, ExprNode *stringExpr )
{
   TTagSetStmtNode *ret = (TTagSetStmtNode *) res->consoleAlloc(sizeof(TTagSetStmtNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->tag = tag;
   ret->valueExpr = valueExpr;
   ret->stringExpr = stringExpr;
   return ret;
}

TTagDerefNode *TTagDerefNode::alloc( Compiler::Resources* res, S32 lineNumber, ExprNode *expr )
{
   TTagDerefNode *ret = (TTagDerefNode *) res->consoleAlloc(sizeof(TTagDerefNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->expr = expr;
   return ret;
}

TTagExprNode *TTagExprNode::alloc( Compiler::Resources* res, S32 lineNumber, StringTableEntry tag )
{
   TTagExprNode *ret = (TTagExprNode *) res->consoleAlloc(sizeof(TTagExprNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->tag = tag;
   return ret;
}

FuncCallExprNode *FuncCallExprNode::alloc( Compiler::Resources* res, S32 lineNumber, StringTableEntry funcName, StringTableEntry nameSpace, ExprNode *args, bool dot )
{
   FuncCallExprNode *ret = (FuncCallExprNode *) res->consoleAlloc(sizeof(FuncCallExprNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->funcName = funcName;
   ret->nameSpace = nameSpace;
   ret->args = args;
   if(dot)
      ret->callType = MethodCall;
   else
   {
      if(nameSpace && !dStricmp(nameSpace, "Parent"))
         ret->callType = ParentCall;
      else
         ret->callType = FunctionCall;
   }
   return ret;
}

AssertCallExprNode *AssertCallExprNode::alloc( Compiler::Resources* res, S32 lineNumber,  ExprNode *testExpr, const char *message )
{
#ifdef TORQUE_ENABLE_SCRIPTASSERTS
   
   AssertCallExprNode *ret = (AssertCallExprNode *) res->consoleAlloc(sizeof(FuncCallExprNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->testExpr = testExpr;
   ret->message = message ? message : "TorqueScript assert!";
   return ret;
   
#else
   
   return NULL;
   
#endif
}

SlotAccessNode *SlotAccessNode::alloc( Compiler::Resources* res, S32 lineNumber, ExprNode *objectExpr, ExprNode *arrayExpr, StringTableEntry slotName )
{
   SlotAccessNode *ret = (SlotAccessNode *) res->consoleAlloc(sizeof(SlotAccessNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->objectExpr = objectExpr;
   ret->arrayExpr = arrayExpr;
   ret->slotName = slotName;
   return ret;
}

InternalSlotAccessNode *InternalSlotAccessNode::alloc( Compiler::Resources* res, S32 lineNumber, ExprNode *objectExpr, ExprNode *slotExpr, bool recurse )
{
   InternalSlotAccessNode *ret = (InternalSlotAccessNode *) res->consoleAlloc(sizeof(InternalSlotAccessNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->objectExpr = objectExpr;
   ret->slotExpr = slotExpr;
   ret->recurse = recurse;
   return ret;
}

SlotAssignNode *SlotAssignNode::alloc( Compiler::Resources* res, S32 lineNumber, ExprNode *objectExpr, ExprNode *arrayExpr, StringTableEntry slotName, ExprNode *valueExpr, StringTableEntry typeName )
{
   SlotAssignNode *ret = (SlotAssignNode *) res->consoleAlloc(sizeof(SlotAssignNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->objectExpr = objectExpr;
   ret->arrayExpr = arrayExpr;
   ret->slotName = slotName;
   ret->valueExpr = valueExpr;
   ret->varType = typeName;
   return ret;
}

SlotAssignOpNode *SlotAssignOpNode::alloc( Compiler::Resources* res, S32 lineNumber, ExprNode *objectExpr, StringTableEntry slotName, ExprNode *arrayExpr, const SimpleLexer::TokenType op, ExprNode *valueExpr )
{
   SlotAssignOpNode *ret = (SlotAssignOpNode *) res->consoleAlloc(sizeof(SlotAssignOpNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->objectExpr = objectExpr;
   ret->arrayExpr = arrayExpr;
   ret->slotName = slotName;
   ret->op = op;
   ret->operand = 0;
   ret->rhsExpr = valueExpr;
   ret->subType = TypeReqNone;
   return ret;
}

ObjectDeclNode *ObjectDeclNode::alloc( Compiler::Resources* res, S32 lineNumber, ExprNode *classNameExpr, ExprNode *objectNameExpr, ExprNode *argList, StringTableEntry parentObject, SlotAssignNode *slotDecls, ObjectDeclNode *subObjects, bool isDatablock, bool classNameInternal, bool isSingleton )
{
   ObjectDeclNode *ret = (ObjectDeclNode *) res->consoleAlloc(sizeof(ObjectDeclNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->classNameExpr = classNameExpr;
   ret->objectNameExpr = objectNameExpr;
   ret->argList = argList;
   ret->slotDecls = slotDecls;
   ret->subObjects = subObjects;
   ret->isDatablock = isDatablock;
   ret->isClassNameInternal = classNameInternal;
   ret->isSingleton = isSingleton;
   ret->failOffset = 0;
   if(parentObject)
      ret->parentObject = parentObject;
   else
      ret->parentObject = StringTable->insert("");
   return ret;
}

FunctionDeclStmtNode *FunctionDeclStmtNode::alloc( Compiler::Resources* res, S32 lineNumber, StringTableEntry fnName, StringTableEntry nameSpace, VarNode *args, StmtNode *stmts, StringTableEntry retTypeName )
{
   FunctionDeclStmtNode *ret = (FunctionDeclStmtNode *) res->consoleAlloc(sizeof(FunctionDeclStmtNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->fnName = fnName;
   ret->args = args;
   ret->stmts = stmts;
   ret->nameSpace = nameSpace;
   ret->package = NULL;
   ret->argc = 0;
   ret->returnTypeName = retTypeName;
   return ret;
}

CatchStmtNode* CatchStmtNode::alloc(Compiler::Resources* res,
                                    S32 lineNumber,
                                    ExprNode* testExpr,
                                    StmtNode* catchBlock)
{
   CatchStmtNode* ret = (CatchStmtNode *) res->consoleAlloc(sizeof(CatchStmtNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->testExpr   = testExpr;
   ret->catchBlock = catchBlock;
   return ret;
}

TryStmtNode* TryStmtNode::alloc(Compiler::Resources* res,
                                S32 lineNumber,
                                StmtNode *tryBlock,
                                CatchStmtNode* catchBlocks)
{
   TryStmtNode* ret = (TryStmtNode *) res->consoleAlloc(sizeof(TryStmtNode));
   constructInPlace(ret);
   ret->dbgLineNumber = lineNumber;
   ret->tryBlock      = tryBlock;
   ret->catchBlocks   = catchBlocks;
   ret->startTryOffset = 0;
   ret->startEndJmpOffset   = 0;
   ret->endTryFixOffset   = 0;
   ret->endTryCatchOffset   = 0;
   return ret;
}

