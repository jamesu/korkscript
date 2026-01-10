//-----------------------------------------------------------------------------
// Copyright (c) 2012 GarageGames, LLC
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

#include "platform/platform.h"

#include "console/simpleLexer.h"
#include "console/ast.h"

#include "embed/api.h"
#include "embed/internalApi.h"

#include "core/findMatch.h"
#include "console/consoleInternal.h"
#include "core/fileStream.h"
#include "console/compiler.h"

using TT = SimpleLexer::TokenType;

template< typename T >
struct Token
{
   T value;
   S32 lineNumber;
};
#include "console/simpleLexer.h"


namespace Compiler
{
   U32 compileBlock(StmtNode *block, CodeStream &codeStream, U32 ip)
   {
      for(StmtNode *walk = block; walk; walk = walk->getNext())
         ip = walk->compileStmt(codeStream, ip);
      return codeStream.tell();
   }
}

using namespace Compiler;

//-----------------------------------------------------------------------------

void StmtNode::addBreakLine(CodeStream &code)
{
   code.addBreakLine(dbgLineNumber, code.tell());
}

//------------------------------------------------------------

StmtNode::StmtNode()
{
   next = NULL;
}

void StmtNode::setPackage(StringTableEntry)
{
}

void StmtNode::append(StmtNode *next)
{
   StmtNode *walk = this;
   while(walk->next)
      walk = walk->next;
   walk->next = next;
}


void FunctionDeclStmtNode::setPackage(StringTableEntry packageName)
{
   package = packageName;
}

//------------------------------------------------------------
//
// Console language compilers
//
//------------------------------------------------------------

U32 BreakStmtNode::compileStmt(CodeStream &codeStream, U32 ip)
{
   if(codeStream.inLoop())
   {
      addBreakLine(codeStream);
      codeStream.emit(OP_JMP);
      codeStream.emitFix(CodeStream::FIXTYPE_BREAK);
   }
   else
   {
      // Con::warnf(ConsoleLogEntry::General, "%s (%d): break outside of loop... ignoring.", codeStream.getFilename(), dbgLineNumber);
   }
   return codeStream.tell();
}

//------------------------------------------------------------

U32 ContinueStmtNode::compileStmt(CodeStream &codeStream, U32 ip)
{
   if(codeStream.inLoop())
   {
      addBreakLine(codeStream);
      codeStream.emit(OP_JMP);
      codeStream.emitFix(CodeStream::FIXTYPE_CONTINUE);
   }
   else
   {
      // Con::warnf(ConsoleLogEntry::General, "%s (%d): continue outside of loop... ignoring.", codeStream.getFilename(), dbgLineNumber);
   }
   return codeStream.tell();
}

//------------------------------------------------------------

U32 ExprNode::compileStmt(CodeStream &codeStream, U32 ip)
{
   addBreakLine(codeStream);
   return compile(codeStream, ip, TypeReqNone);
}

//------------------------------------------------------------

U32 ReturnStmtNode::compileStmt(CodeStream &codeStream, U32 ip)
{
   addBreakLine(codeStream);
   if(!expr)
      codeStream.emit(OP_RETURN_VOID);
   else
   {
      TypeReq walkType = expr->getPreferredType();
      if (walkType == TypeReqNone) walkType = TypeReqString;
      ip = expr->compile(codeStream, ip, walkType);

      // Return the correct type
      switch (walkType) {
      case TypeReqUInt:
         codeStream.emit(OP_RETURN_UINT);
         break;
      case TypeReqFloat:
         codeStream.emit(OP_RETURN_FLT);
         break;
      default:
         codeStream.emit(OP_RETURN);
         break;
      }
   }
   return codeStream.tell();
}

//------------------------------------------------------------

ExprNode *IfStmtNode::getSwitchOR(Compiler::Resources* res, ExprNode *left, ExprNode *list, bool string)
{
   ExprNode *nextExpr = (ExprNode *) list->getNext();
   ExprNode *test;
   if(string)
      test = StreqExprNode::alloc( res, left->dbgLineNumber, left, list, true );
   else
      test = IntBinaryExprNode::alloc( res, left->dbgLineNumber, SimpleLexer::TokenType::opEQ, left, list );
   if(!nextExpr)
      return test;
   return IntBinaryExprNode::alloc( res, test->dbgLineNumber, SimpleLexer::TokenType::opOR, test, getSwitchOR( res, left, nextExpr, string ) );
}

void IfStmtNode::propagateSwitchExpr(Compiler::Resources* res, ExprNode *left, bool string)
{
   testExpr = getSwitchOR(res, left, testExpr, string);
   if(propagate && elseBlock)
      ((IfStmtNode *) elseBlock)->propagateSwitchExpr(res, left, string);
}

U32 IfStmtNode::compileStmt(CodeStream &codeStream, U32 ip)
{
   U32 endifIp, elseIp;
   addBreakLine(codeStream);
   
   if(testExpr->getPreferredType() == TypeReqUInt)
   {
      integer = true;
   }
   else
   {
      integer = false;
   }

   ip = testExpr->compile(codeStream, ip, integer ? TypeReqUInt : TypeReqFloat);
   codeStream.emit(integer ? OP_JMPIFNOT : OP_JMPIFFNOT);

   if(elseBlock)
   {
      elseIp = codeStream.emit(0);
      elseOffset = compileBlock(ifBlock, codeStream, ip) + 2;
      codeStream.emit(OP_JMP);
      endifIp = codeStream.emit(0);
      endifOffset = compileBlock(elseBlock, codeStream, ip);
      
      codeStream.patch(endifIp, endifOffset);
      codeStream.patch(elseIp, elseOffset);
   }
   else
   {
      endifIp = codeStream.emit(0);
      endifOffset = compileBlock(ifBlock, codeStream, ip);
      
      codeStream.patch(endifIp, endifOffset);
   }
   
   // Resolve fixes
   return codeStream.tell();
}

//------------------------------------------------------------

U32 LoopStmtNode::compileStmt(CodeStream &codeStream, U32 ip)
{
   if(testExpr->getPreferredType() == TypeReqUInt)
   {
      integer = true;
   }
   else
   {
      integer = false;
   }
   
   // if it's a for loop or a while loop it goes:
   // initExpr
   // testExpr
   // OP_JMPIFNOT to break point
   // loopStartPoint:
   // loopBlock
   // continuePoint:
   // endLoopExpr
   // testExpr
   // OP_JMPIF loopStartPoint
   // breakPoint:
   
   // otherwise if it's a do ... while() it goes:
   // initExpr
   // loopStartPoint:
   // loopBlock
   // continuePoint:
   // endLoopExpr
   // testExpr
   // OP_JMPIF loopStartPoint
   // breakPoint:
   
   // loopBlockStart == start of loop block
   // continue == skip to end
   // break == exit loop
   
   
   addBreakLine(codeStream);
   codeStream.pushFixScope(true);
   
   if(initExpr)
      ip = initExpr->compile(codeStream, ip, TypeReqNone);

   if(!isDoLoop)
   {
      ip = testExpr->compile(codeStream, ip, integer ? TypeReqUInt : TypeReqFloat);
      codeStream.emit(integer ? OP_JMPIFNOT : OP_JMPIFFNOT);
      codeStream.emitFix(CodeStream::FIXTYPE_BREAK);
   }

   // Compile internals of loop.
   loopBlockStartOffset = codeStream.tell();
   continueOffset = compileBlock(loopBlock, codeStream, ip);

   if(endLoopExpr)
      ip = endLoopExpr->compile(codeStream, ip, TypeReqNone);

   ip = testExpr->compile(codeStream, ip, integer ? TypeReqUInt : TypeReqFloat);

   codeStream.emit(integer ? OP_JMPIF : OP_JMPIFF);
   codeStream.emitFix(CodeStream::FIXTYPE_LOOPBLOCKSTART);
   
   breakOffset = codeStream.tell(); // exit loop
   
   codeStream.fixLoop(loopBlockStartOffset, breakOffset, continueOffset);
   codeStream.popFixScope();
   
   return codeStream.tell();
}

//------------------------------------------------------------

U32 IterStmtNode::compileStmt( CodeStream &codeStream, U32 ip )
{
   // Instruction sequence:
   //
   //   containerExpr
   //   OP_ITER_BEGIN varName .fail
   // .continue:
   //   OP_ITER .break
   //   body
   //   OP_JMP .continue
   // .break:
   //   OP_ITER_END
   // .fail:
   
   addBreakLine(codeStream);
   
   codeStream.pushFixScope(true);
   
   const U32 startIp = ip;
   containerExpr->compile( codeStream, startIp, TypeReqString );
   
   codeStream.emit(isStringIter ? OP_ITER_BEGIN_STR : OP_ITER_BEGIN);
   codeStream.emitSTE( varName );
   const U32 finalFix = codeStream.emit(0);
   const U32 continueIp = codeStream.emit(OP_ITER);
   codeStream.emitFix(CodeStream::FIXTYPE_BREAK);
   const U32 bodyIp = codeStream.tell();
   
   const U32 jmpIp = compileBlock( body, codeStream, bodyIp);
   const U32 breakIp = jmpIp + 2;
   const U32 finalIp = breakIp + 1;
   
   codeStream.emit(OP_JMP);
   codeStream.emitFix(CodeStream::FIXTYPE_CONTINUE);
   codeStream.emit(OP_ITER_END);
   
   codeStream.patch(finalFix, finalIp);
   codeStream.fixLoop(bodyIp, breakIp, continueIp);
   codeStream.popFixScope();
   
   return codeStream.tell();
}

//------------------------------------------------------------

U32 ConditionalExprNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   // code is testExpr
   // JMPIFNOT falseStart
   // trueExpr
   // JMP end
   // falseExpr
   if(testExpr->getPreferredType() == TypeReqUInt)
   {
      integer = true;
   }
   else
   {
      integer = false;
   }
   
   ip = testExpr->compile(codeStream, ip, integer ? TypeReqUInt : TypeReqFloat);
   codeStream.emit(integer ? OP_JMPIFNOT : OP_JMPIFFNOT);
   
   U32 jumpElseIp = codeStream.emit(0);
   ip = trueExpr->compile(codeStream, ip, type);
   codeStream.emit(OP_JMP);
   U32 jumpEndIp = codeStream.emit(0);
   codeStream.patch(jumpElseIp, codeStream.tell());
   ip = falseExpr->compile(codeStream, ip, type);
   codeStream.patch(jumpEndIp, codeStream.tell());
   
   return codeStream.tell();
}

TypeReq ConditionalExprNode::getPreferredType()
{
   return trueExpr->getPreferredType();
}

//------------------------------------------------------------

U32 FloatBinaryExprNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   // Ok: if either side allows type, generate a typed op THEN do a conversion
   
   TypeReq nodeOpOutputType = TypeReqFloat;
   
   bool firstIsTyped = right->canBeTyped();
   bool secondIsTyped = left->canBeTyped();
   //bool outputIsTyped = type == TypeReqTypedString;
   
   //if (outputIsTyped)
   {
      if (firstIsTyped || secondIsTyped)
      {
         nodeOpOutputType = TypeReqTypedString;
      }
   }
   
   ip = right->compile(codeStream, ip, nodeOpOutputType);
   if (nodeOpOutputType == TypeReqTypedString) codeStream.emit(OP_PUSH_TYPED);
   ip = left->compile(codeStream, ip, nodeOpOutputType);
   
   U32 operand = OP_INVALID;
   switch(op)
   {
      case SimpleLexer::TokenType::opPCHAR_PLUS:
         operand = OP_ADD;
         break;
      case SimpleLexer::TokenType::opPCHAR_MINUS:
         operand = OP_SUB;
         break;
      case SimpleLexer::TokenType::opPCHAR_SLASH:
         operand = OP_DIV;
         break;
      case SimpleLexer::TokenType::opPCHAR_ASTERISK:
         operand = OP_MUL;
         break;
      default:
         break;
   }

   if (nodeOpOutputType != TypeReqTypedString)
   {
      codeStream.emit(operand);
   }
   else
   {
      if (!firstIsTyped)
      {
         codeStream.emit(OP_TYPED_OP_REVERSE);
      }
      else
      {
         codeStream.emit(OP_TYPED_OP);
      }
      codeStream.emit(operand);
   }
   
   if(nodeOpOutputType != type)
   {
      emitStackConversion(codeStream, nodeOpOutputType, type);
   }
   
   return codeStream.tell();
}

TypeReq FloatBinaryExprNode::getPreferredType()
{
   return TypeReqFloat;
}

bool FloatBinaryExprNode::canBeTyped()
{
   return left->canBeTyped() || right->canBeTyped();
}

//------------------------------------------------------------

void IntBinaryExprNode::getSubTypeOperand()
{
   subType = TypeReqUInt;
   switch(op)
   {
   case SimpleLexer::TokenType::opPCHAR_CARET:
      operand = OP_XOR;
      break;
   case SimpleLexer::TokenType::opPCHAR_PERCENT:
      operand = OP_MOD;
      break;
   case SimpleLexer::TokenType::opPCHAR_AMPERSAND:
      operand = OP_BITAND;
      break;
   case SimpleLexer::TokenType::opPCHAR_PIPE:
      operand = OP_BITOR;
      break;
   case SimpleLexer::TokenType::opPCHAR_LESS:
      operand = OP_CMPLT;
      subType = TypeReqFloat;
      break;
   case SimpleLexer::TokenType::opPCHAR_GREATER:
      operand = OP_CMPGR;
      subType = TypeReqFloat;
      break;
      case SimpleLexer::TokenType::opGE:
      operand = OP_CMPGE;
      subType = TypeReqFloat;
      break;
      case SimpleLexer::TokenType::opLE:
      operand = OP_CMPLE;
      subType = TypeReqFloat;
      break;
      case SimpleLexer::TokenType::opEQ:
      operand = OP_CMPEQ;
      subType = TypeReqFloat;
      break;
      case SimpleLexer::TokenType::opNE:
      operand = OP_CMPNE;
      subType = TypeReqFloat;
      break;
      case SimpleLexer::TokenType::opOR:
      operand = OP_OR;
      break;
      case SimpleLexer::TokenType::opAND:
      operand = OP_AND;
      break;
      case SimpleLexer::TokenType::opSHR:
      operand = OP_SHR;
      break;
      case SimpleLexer::TokenType::opSHL:
      operand = OP_SHL;
      break;
   }
}

U32 IntBinaryExprNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   getSubTypeOperand();
   
   TypeReq nodeOpOutputType = subType;
   
   if(operand == OP_OR || operand == OP_AND)
   {
      ip = left->compile(codeStream, ip, subType);
      codeStream.emit(operand == OP_OR ? OP_JMPIF_NP : OP_JMPIFNOT_NP);
      U32 jmpIp = codeStream.emit(0);
      ip = right->compile(codeStream, ip, subType);
      codeStream.patch(jmpIp, ip);
   }
   else
   {
      // Non-OR/AND: apply typed-op selection logic like FloatBinaryExprNode
      bool firstIsTyped = right->canBeTyped();
      bool secondIsTyped = left->canBeTyped();
      bool outputIsTyped = (type == TypeReqTypedString);

      bool doTypedOp = (firstIsTyped || secondIsTyped);
      nodeOpOutputType = doTypedOp ? TypeReqTypedString : subType;
      
      ip = right->compile(codeStream, ip, nodeOpOutputType);
      if (nodeOpOutputType == TypeReqTypedString) codeStream.emit(OP_PUSH_TYPED);
      ip = left->compile(codeStream, ip, nodeOpOutputType);
      
      if (!doTypedOp)
      {
         codeStream.emit(operand);
         nodeOpOutputType = TypeReqUInt; // result is now UInt
      }
      else
      {
         if(!firstIsTyped)
            codeStream.emit(OP_TYPED_OP_REVERSE);
         else
            codeStream.emit(OP_TYPED_OP);

         codeStream.emit(operand); // result is also typed value on stack
      }
   }
   
   if(type != nodeOpOutputType) // this gets set as UInt
   {
      emitStackConversion(codeStream, nodeOpOutputType, type);
   }
   return codeStream.tell();
}

TypeReq IntBinaryExprNode::getPreferredType()
{
   return TypeReqUInt;
}

bool IntBinaryExprNode::canBeTyped()
{
   return left->canBeTyped() || right->canBeTyped();
}

//------------------------------------------------------------

U32 StreqExprNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   // eval str left
   // OP_ADVANCE_STR_NUL
   // eval str right
   // OP_COMPARE_STR
   // optional conversion
   
   ip = left->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_ADVANCE_STR_NUL);
   ip = right->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_COMPARE_STR);
   if(!eq)
      codeStream.emit(OP_NOT);
   if(type != TypeReqUInt)
      emitStackConversion(codeStream, TypeReqUInt, type);
   return codeStream.tell();
}

TypeReq StreqExprNode::getPreferredType()
{
   return TypeReqUInt;
}

//------------------------------------------------------------

U32 StrcatExprNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   ip = left->compile(codeStream, ip, TypeReqString);
   if(!appendChar)
      codeStream.emit(OP_ADVANCE_STR);
   else
   {
      codeStream.emit(OP_ADVANCE_STR_APPENDCHAR);
      codeStream.emit(appendChar);
   }
   ip = right->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_REWIND_STR);
   if(type == TypeReqUInt)
      codeStream.emit(OP_STR_TO_UINT);
   else if(type == TypeReqFloat)
      codeStream.emit(OP_STR_TO_FLT);
   return codeStream.tell();
}

TypeReq StrcatExprNode::getPreferredType()
{
   return TypeReqString;
}

//------------------------------------------------------------

U32 CommaCatExprNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   ip = left->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_ADVANCE_STR_COMMA);
   ip = right->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_REWIND_STR);

   // At this point the stack has the concatenated string.

   // But we're paranoid, so accept (but whine) if we get an oddity...
   if(type == TypeReqUInt || type == TypeReqFloat)
   {
      // TOFIX Con::warnf(ConsoleLogEntry::General, "%s (%d): converting comma string to a number... probably wrong.", codeStream.getFilename(), dbgLineNumber);
   }
   if(type == TypeReqUInt)
      codeStream.emit(OP_STR_TO_UINT);
   else if(type == TypeReqFloat)
      codeStream.emit(OP_STR_TO_FLT);
   return codeStream.tell();
}

TypeReq CommaCatExprNode::getPreferredType()
{
   return TypeReqString;
}

//------------------------------------------------------------

U32 IntUnaryExprNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   integer = true;
   TypeReq prefType = expr->getPreferredType();
   if(op == SimpleLexer::TokenType::opPCHAR_EXCL && // !
      (prefType == TypeReqFloat || prefType == TypeReqString || prefType == TypeReqTypedString))
      integer = false;
   
   
   bool operandTyped = expr->canBeTyped();
   bool outputTyped  = (type == TypeReqTypedString);
   TypeReq nodeOpOutputType = operandTyped ? TypeReqTypedString : (integer ? TypeReqUInt : TypeReqFloat);
   
   ip = expr->compile(codeStream, ip, nodeOpOutputType);
   
   if (operandTyped)
   {
      codeStream.emit(OP_TYPED_UNARY_OP);
   }
   
   // Actual op
   if(op == SimpleLexer::TokenType::opPCHAR_EXCL) // !
      codeStream.emit(integer ? OP_NOT : OP_NOTF);
   else if(op == SimpleLexer::TokenType::opPCHAR_TILDE) // ~
      codeStream.emit(OP_ONESCOMPLEMENT);
   
   if (type != nodeOpOutputType)
   {
      emitStackConversion(codeStream, nodeOpOutputType, type);
   }
   
   return codeStream.tell();
}

TypeReq IntUnaryExprNode::getPreferredType()
{
   return TypeReqUInt;
}

//------------------------------------------------------------

U32 FloatUnaryExprNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   bool operandTyped = expr->canBeTyped();
   bool outputTyped  = (type == TypeReqTypedString);
   TypeReq nodeOpOutputType = operandTyped ? TypeReqTypedString : TypeReqFloat;
   
   ip = expr->compile(codeStream, ip, nodeOpOutputType);
   
   if (operandTyped)
   {
      codeStream.emit(OP_TYPED_UNARY_OP);
   }
   
   codeStream.emit(OP_NEG);
   
   if(type != nodeOpOutputType)
   {
      emitStackConversion(codeStream, nodeOpOutputType, type);
   }
   
   return codeStream.tell();
}

TypeReq FloatUnaryExprNode::getPreferredType()
{
   return TypeReqFloat;
}

//------------------------------------------------------------

U32 VarNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   // if this has an arrayIndex...
   // OP_LOADIMMED_IDENT
   // varName
   // OP_ADVANCE_STR
   // evaluate arrayIndex TypeReqString
   // OP_REWIND_STR
   // OP_SETCURVAR_ARRAY
   // OP_LOADVAR (type)
   
   // else
   // OP_SETCURVAR
   // varName
   // OP_LOADVAR (type)
   
   if(type == TypeReqNone)
      return codeStream.tell();
   
   codeStream.mResources->precompileIdent(varName);

   codeStream.emit(arrayIndex ? OP_LOADIMMED_IDENT : OP_SETCURVAR);
   codeStream.emitSTE(varName);
   
   if(arrayIndex)
   {
      codeStream.emit(OP_ADVANCE_STR);
      ip = arrayIndex->compile(codeStream, ip, TypeReqString);
      codeStream.emit(OP_REWIND_STR);
      codeStream.emit(OP_SETCURVAR_ARRAY);
   }
   
   // Set type
   S32 typeID = varType ? codeStream.mResources->precompileType(varType) : -1;
   
   if(typeID != -1)
   {
      codeStream.emit(OP_SETCURVAR_TYPE);
      codeStream.emit(typeID);
   }
   
   switch(type)
   {
   case TypeReqUInt:
      codeStream.emit(OP_LOADVAR_UINT);
      break;
   case TypeReqFloat:
      codeStream.emit(OP_LOADVAR_FLT);
      break;
   case TypeReqString:
      codeStream.emit(OP_LOADVAR_STR);
      break;
   // This is now handled externally
   //case TypeReqVar:
   //   codeStream.emit(OP_LOADVAR_VAR);
   //   break;
   case TypeReqTypedString:
      codeStream.emit(OP_LOADVAR_TYPED);
      break;
   case TypeReqNone:
   default:
      break;
   }
   return codeStream.tell();
}

TypeReq VarNode::getPreferredType()
{
   return TypeReqNone; // no preferred type
}

TypeReq VarNode::getReturnLoadType()
{
   return TypeReqVar;
}

bool VarNode::canBeTyped()
{
   return (varInfo && !disableTypes) ? varInfo->typeId >= 0 : false;
}

//------------------------------------------------------------

U32 IntNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   if(type == TypeReqString)
      index = codeStream.mResources->getCurrentStringTable()->addIntString(value);
   else if(type == TypeReqFloat || type == TypeReqTypedString)
      index = codeStream.mResources->getCurrentFloatTable()->add(value);
   
   switch(type)
   {
   case TypeReqUInt:
      codeStream.emit(OP_LOADIMMED_UINT);
      codeStream.emit(value);
      break;
   case TypeReqString:
      codeStream.emit(OP_LOADIMMED_STR);
      codeStream.emit(index);
      break;
   case TypeReqFloat:
      codeStream.emit(OP_LOADIMMED_FLT);
      codeStream.emit(index);
      break;
   case TypeReqTypedString:
         codeStream.emit(OP_LOADIMMED_FLT);
         codeStream.emit(index);
         codeStream.emit(OP_SET_DYNAMIC_TYPE_TO_NULL);
         codeStream.emit(OP_FLT_TO_TYPED);
         break;
   case TypeReqNone:
   default:
      break;
   }
   return codeStream.tell();
}

TypeReq IntNode::getPreferredType()
{
   return TypeReqUInt;
}

//------------------------------------------------------------

U32 FloatNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   if(type == TypeReqString)
      index = codeStream.mResources->getCurrentStringTable()->addFloatString(value);
   else if(type == TypeReqFloat || type == TypeReqTypedString)
      index = codeStream.mResources->getCurrentFloatTable()->add(value);
   
   switch(type)
   {
   case TypeReqUInt:
      codeStream.emit(OP_LOADIMMED_UINT);
      codeStream.emit(U32(value));
      break;
   case TypeReqString:
      codeStream.emit(OP_LOADIMMED_STR);
      codeStream.emit(index);
      break;
   case TypeReqFloat:
      codeStream.emit(OP_LOADIMMED_FLT);
      codeStream.emit(index);
      break;
   case TypeReqTypedString:
         codeStream.emit(OP_LOADIMMED_FLT);
         codeStream.emit(index);
         codeStream.emit(OP_SET_DYNAMIC_TYPE_TO_NULL);
         codeStream.emit(OP_FLT_TO_TYPED);
         break;
   case TypeReqNone:
   default:
      break;
   }
   return codeStream.tell();
}

TypeReq FloatNode::getPreferredType()
{
   return TypeReqFloat;
}

//------------------------------------------------------------

U32 StrConstNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   // Early out for documentation block.
   if( doc )
   {
      index = codeStream.mResources->getCurrentStringTable()->add(str, true, tag);
   }
   else if(type == TypeReqString || type == TypeReqTypedString)
   {
      index = codeStream.mResources->getCurrentStringTable()->add(str, true, tag);
   }
   else if (type != TypeReqNone)
   {
      fVal = consoleStringToNumber(str, codeStream.getFilename(), dbgLineNumber);
      if(type == TypeReqFloat)
      {
         index = codeStream.mResources->getCurrentFloatTable()->add(fVal);
      }
   }
   
   // If this is a DOCBLOCK, then process w/ appropriate op...
   if( doc )
   {
      codeStream.emit(OP_DOCBLOCK_STR);
      codeStream.emit(index);
      return ip;
   }

   // Otherwise, deal with it normally as a string literal case.
   switch(type)
   {
   case TypeReqTypedString:
   case TypeReqString:
      codeStream.emit(tag ? OP_TAG_TO_STR : OP_LOADIMMED_STR);
      codeStream.emit(index);
      break;
   case TypeReqUInt:
      codeStream.emit(OP_LOADIMMED_UINT);
      codeStream.emit(U32(fVal));
      break;
   case TypeReqFloat:
      codeStream.emit(OP_LOADIMMED_FLT);
      codeStream.emit(index);
      break;         
   case TypeReqNone:
   default:
      break;
   }
   return codeStream.tell();
}

TypeReq StrConstNode::getPreferredType()
{
   return TypeReqString;
}

//------------------------------------------------------------

U32 ConstantNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   if(type == TypeReqString)
   {
      codeStream.mResources->precompileIdent(value);
   }
   else if (type != TypeReqNone)
   {
      fVal = consoleStringToNumber(value, codeStream.getFilename(), dbgLineNumber);
      if(type == TypeReqFloat)
         index = codeStream.mResources->getCurrentFloatTable()->add(fVal);
   }
   
   switch(type)
   {
   case TypeReqString:
      codeStream.emit(OP_LOADIMMED_IDENT);
      codeStream.emitSTE(value);
      break;
   case TypeReqUInt:
      codeStream.emit(OP_LOADIMMED_UINT);
      codeStream.emit(U32(fVal));
      break;
   case TypeReqFloat:
      codeStream.emit(OP_LOADIMMED_FLT);
      codeStream.emit(index);
      break;
   case TypeReqNone:
   default:
      break;
   }
   return ip;
}

TypeReq ConstantNode::getPreferredType()
{
   return TypeReqString;
}

//------------------------------------------------------------

U32 AssignExprNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   subType = rhsExpr->getPreferredType();
   
   if (rhsExpr->canBeTyped())
   {
      subType = TypeReqTypedString;
   }
   
   if(subType == TypeReqNone)
      subType = type;

   TupleExprNode* tupleExpr = dynamic_cast<TupleExprNode*>(rhsExpr);
   if(subType == TypeReqNone)
   {
      // What we need to do in this case is turn it into a VarNode reference. 
      if (dynamic_cast<VarNode*>(rhsExpr) != NULL)
      {
         subType = TypeReqVar;
      }
      else
      {
         subType = TypeReqString;
         AssertFatal(tupleExpr == NULL, "Can't chain tuple assignments");
      }
   }
   
   // if it's an array expr, the formula is:
   // eval expr
   // (push and pop if it's TypeReqString) OP_ADVANCE_STR
   // OP_LOADIMMED_IDENT
   // varName
   // OP_ADVANCE_STR
   // eval array
   // OP_REWIND_STR
   // OP_SETCURVAR_ARRAY_CREATE
   // OP_TERMINATE_REWIND_STR
   // OP_SAVEVAR
   
   //else
   // eval expr
   // OP_SETCURVAR_CREATE
   // varname
   // OP_SAVEVAR
   
   codeStream.mResources->precompileIdent(varName);

   bool usingStringStack = (tupleExpr == NULL) && (subType == TypeReqString);

   TypeReq rhsType = tupleExpr ? TypeReqTuple : subType;
   // NOTE: compiling rhs first is compulsory in this case
   ip = rhsExpr->compile(codeStream, ip, rhsType);

   // Save var so we can copy to the new one
   if (subType == TypeReqVar)
   {
      codeStream.emit(OP_LOADVAR_VAR);
   }
   
   if(arrayIndex)
   {
      if (usingStringStack)
      {
         codeStream.emit(OP_ADVANCE_STR);
      }

      codeStream.emit(OP_LOADIMMED_IDENT);
      codeStream.emitSTE(varName);
      
      codeStream.emit(OP_ADVANCE_STR);
      ip = arrayIndex->compile(codeStream, ip, TypeReqString);
      codeStream.emit(OP_REWIND_STR);
      codeStream.emit(OP_SETCURVAR_ARRAY_CREATE);

      if (usingStringStack)
      {
         codeStream.emit(OP_TERMINATE_REWIND_STR);
      }
   }
   else
   {
      codeStream.emit(OP_SETCURVAR_CREATE);
      codeStream.emitSTE(varName);
   }
   
   // Set type (NOTE: this should be optimized out at some point for duplicates)
   if (varInfo->typeId != -1)
   {
      codeStream.emit(OP_SETCURVAR_TYPE);
      codeStream.emit(varInfo->typeId);
   }

   // Tuples need to be emitted here
   if (tupleExpr)
   {
      AssertFatal(subType == TypeReqVar, "something went wrong here");
      codeStream.emit(OP_SAVEVAR_MULTIPLE);
      
      if (type == TypeReqVar)
      {
         subType = TypeReqVar;
      }
   }
   else
   {
      // This bit is already emitted to in tuple
      switch(subType)
      {
         case TypeReqString:
            codeStream.emit(OP_SAVEVAR_STR);
            break;
         case TypeReqUInt:
            codeStream.emit(OP_SAVEVAR_UINT);
            break;
         case TypeReqFloat:
            codeStream.emit(OP_SAVEVAR_FLT);
            break;
         case TypeReqVar:
            codeStream.emit(OP_SAVEVAR_VAR);
            break;
         case TypeReqTypedString:
            codeStream.emit(OP_SAVEVAR_TYPED);
            break;
         case TypeReqNone:
         default:
            break;
      }
   }
   
   if (type != subType ||
       type == TypeReqVar) // need this as we need to copy the var to the output
   {
      emitStackConversion(codeStream, subType, type);
   }
   
   return ip;
}

TypeReq AssignExprNode::getPreferredType()
{
   return (assignTypeName != NULL && assignTypeName[0] != '\0' ? TypeReqTypedString : rhsExpr->getPreferredType());
}

TypeReq AssignExprNode::getReturnLoadType()
{
   return TypeReqVar;
}

void AssignExprNode::setAssignType(StringTableEntry typeName)
{
   assignTypeName = typeName;
}

//------------------------------------------------------------

static void getAssignOpTypeOp(SimpleLexer::TokenType op, TypeReq &type, U32 &operand)
{
   switch(op)
   {
   case SimpleLexer::TokenType::opPCHAR_PLUS:
      type = TypeReqFloat;
      operand = OP_ADD;
      break;
   case SimpleLexer::TokenType::opPCHAR_MINUS:
      type = TypeReqFloat;
      operand = OP_SUB;
      break;
   case SimpleLexer::TokenType::opPCHAR_ASTERISK:
      type = TypeReqFloat;
      operand = OP_MUL;
      break;
   case SimpleLexer::TokenType::opPCHAR_SLASH:
      type = TypeReqFloat;
      operand = OP_DIV;
      break;
   case SimpleLexer::TokenType::opPCHAR_PERCENT:
      type = TypeReqUInt;
      operand = OP_MOD;
      break;
   case SimpleLexer::TokenType::opPCHAR_AMPERSAND:
      type = TypeReqUInt;
      operand = OP_BITAND;
      break;
   case SimpleLexer::TokenType::opPCHAR_CARET:
      type = TypeReqUInt;
      operand = OP_XOR;
      break;
   case SimpleLexer::TokenType::opPCHAR_PIPE:
      type = TypeReqUInt;
      operand = OP_BITOR;
      break;
      case SimpleLexer::TokenType::opSHL:
      type = TypeReqUInt;
      operand = OP_SHL;
      break;
      case SimpleLexer::TokenType::opSHR:
      type = TypeReqUInt;
      operand = OP_SHR;
      break;
   }   
}

U32 AssignOpExprNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   
   // goes like this...
   // eval expr as float or int
   // if there's an arrayIndex
   
   // OP_LOADIMMED_IDENT
   // varName
   // OP_ADVANCE_STR
   // eval arrayIndex stringwise
   // OP_REWIND_STR
   // OP_SETCURVAR_ARRAY_CREATE
   
   // else
   // OP_SETCURVAR_CREATE
   // varName
   
   // OP_LOADVAR_FLT or UINT
   // operand
   // OP_SAVEVAR_FLT or UINT
   
   // conversion OP if necessary.
   getAssignOpTypeOp(op, subType, operand); // op -> subType, operand
   codeStream.mResources->precompileIdent(varName);

   // Change to typed op if var is typed
   bool isTyped = varInfo->typeId != -1;
   if (isTyped)
   {
      subType = TypeReqTypedString;
   }
   
   if (dynamic_cast<TupleExprNode*>(rhsExpr))
   {
      AssertFatal(false, "Something went seriously wrong in handleExpressionTuples");
      return ip;
   }
   
   ip = rhsExpr->compile(codeStream, ip, subType);
   if (subType == TypeReqTypedString) codeStream.emit(OP_PUSH_TYPED);
   
   if(!arrayIndex)
   {
      codeStream.emit(OP_SETCURVAR_CREATE);
      codeStream.emitSTE(varName);
   }
   else
   {
      codeStream.emit(OP_LOADIMMED_IDENT);
      codeStream.emitSTE(varName);
      
      codeStream.emit(OP_ADVANCE_STR);
      ip = arrayIndex->compile(codeStream, ip, TypeReqString);
      codeStream.emit(OP_REWIND_STR);
      codeStream.emit(OP_SETCURVAR_ARRAY_CREATE);
   }
   
   // NOTE: no mechanism to set type here.
   
   emitStackConversion(codeStream, TypeReqVar, subType);
   
   if (subType == TypeReqTypedString)
   {
      codeStream.emit(OP_TYPED_OP);
   }
   
   codeStream.emit(operand);
   emitStackConversion(codeStream, subType, TypeReqVar); // usually goes for FLT or UINT here
   
   // -> output
   if(type != TypeReqVar)
   {
      emitStackConversion(codeStream, TypeReqVar, type);
   }
   
   return codeStream.tell();
}

TypeReq AssignOpExprNode::getPreferredType()
{
   getAssignOpTypeOp(op, subType, operand);
   return subType;// TOFIX assignTypeName != NULL && assignTypeName[0] != '\0' ? TypeReqTypedString : subType;
}

TypeReq AssignOpExprNode::getReturnLoadType()
{
   return TypeReqVar;
}

//------------------------------------------------------------

U32 TTagSetStmtNode::compileStmt(CodeStream&, U32 ip)
{
   return ip;
}

//------------------------------------------------------------

U32 TTagDerefNode::compile(CodeStream&, U32 ip, TypeReq)
{
   return ip;
}

TypeReq TTagDerefNode::getPreferredType()
{
   return TypeReqNone;
}

//------------------------------------------------------------

U32 TTagExprNode::compile(CodeStream&, U32 ip, TypeReq)
{
   return ip;
}

TypeReq TTagExprNode::getPreferredType()
{
   return TypeReqNone;
}

//------------------------------------------------------------

U32 FuncCallExprNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   // OP_PUSH_FRAME
   // arg OP_PUSH arg OP_PUSH arg OP_PUSH
   // eval all the args, then call the function.
   
   // OP_CALLFUNC
   // function
   // namespace
   // isDot
   
   codeStream.mResources->precompileIdent(funcName);
   codeStream.mResources->precompileIdent(nameSpace);
   
   codeStream.emit(OP_PUSH_FRAME);
   for(ExprNode *walk = args; walk; walk = (ExprNode *) walk->getNext())
   {
      TypeReq walkType = walk->getPreferredType();
      TypeReq loadType = walk->getReturnLoadType();

      if (loadType == TypeReqVar) walkType = TypeReqVar;
      else if (loadType == TypeReqField) walkType = TypeReqField;

      if (walkType == TypeReqNone) walkType = TypeReqString;
      ip = walk->compile(codeStream, ip, walkType);
      
      switch (walkType)
      {
         case TypeReqFloat:
            codeStream.emit(OP_PUSH_FLT);
            break;
         case TypeReqUInt:
            codeStream.emit(OP_PUSH_UINT);
            break;
         case TypeReqTypedString:
            codeStream.emit(OP_PUSH_TYPED);
            break;
         case TypeReqVar:
            codeStream.emit(OP_PUSH_VAR);
            break;
         case TypeReqField:
            codeStream.emit(OP_LOADFIELD_TYPED);
            codeStream.emit(OP_PUSH_TYPED);
            break;
         default:
            codeStream.emit(OP_PUSH);
            break;
      }
   }
   if(callType == MethodCall || callType == ParentCall)
      codeStream.emit(OP_CALLFUNC);
   else
      codeStream.emit(OP_CALLFUNC_RESOLVE);

   codeStream.emitSTE(funcName);
   codeStream.emitSTE(nameSpace);
   
   codeStream.emit(callType);
   if(type != TypeReqString)
      emitStackConversion(codeStream, TypeReqString, type);
   return codeStream.tell();
}

TypeReq FuncCallExprNode::getPreferredType()
{
   return TypeReqString;
}


//------------------------------------------------------------

U32 AssertCallExprNode::compile( CodeStream &codeStream, U32 ip, TypeReq type )
{
   #ifdef TORQUE_ENABLE_SCRIPTASSERTS
   
      messageIndex = codeStream.mResources->getCurrentStringTable()->add( message, true, false );
   
      ip = testExpr->compile( codeStream, ip, TypeReqUInt );
      codeStream.emit(OP_ASSERT);
      codeStream.emit(messageIndex);

   #endif

   return codeStream.tell();
}

TypeReq AssertCallExprNode::getPreferredType()
{
   return TypeReqNone;
}

//------------------------------------------------------------

U32 SlotAccessNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   if(type == TypeReqNone)
      return ip;
   
   codeStream.mResources->precompileIdent(slotName);

   if(arrayExpr)
   {
      // eval array
      // OP_ADVANCE_STR
      // evaluate object expression sub (OP_SETCURFIELD)
      // OP_TERMINATE_REWIND_STR
      // OP_SETCURFIELDARRAY
      // total add of 4 + array precomp
      
      ip = arrayExpr->compile(codeStream, ip, TypeReqString);
      codeStream.emit(OP_ADVANCE_STR);
   }
   ip = objectExpr->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_SETCUROBJECT);
   
   codeStream.emit(OP_SETCURFIELD);
   
   codeStream.emitSTE(slotName);

   if(arrayExpr)
   {
      codeStream.emit(OP_TERMINATE_REWIND_STR);
      codeStream.emit(OP_SETCURFIELD_ARRAY);
   }
   
   switch(type)
   {
      case TypeReqUInt:
         codeStream.emit(OP_LOADFIELD_UINT);
         break;
      case TypeReqFloat:
         codeStream.emit(OP_LOADFIELD_FLT);
         break;
      case TypeReqString:
         codeStream.emit(OP_LOADFIELD_STR);
         break;
      case TypeReqField:
      case TypeReqNone:
      default:
         break;
   }
   return codeStream.tell();
}

TypeReq SlotAccessNode::getPreferredType()
{
   return TypeReqNone;
}

TypeReq SlotAccessNode::getReturnLoadType()
{
   return TypeReqField;
}

//-----------------------------------------------------------------------------

U32 InternalSlotAccessNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   if(type == TypeReqNone)
      return ip;

   ip = objectExpr->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_SETCUROBJECT);

   ip = slotExpr->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_SETCUROBJECT_INTERNAL);
   codeStream.emit(recurse);

   if(type != TypeReqUInt)
      emitStackConversion(codeStream, TypeReqUInt, type);
   return codeStream.tell();
}

TypeReq InternalSlotAccessNode::getPreferredType()
{
   return TypeReqUInt;
}

//-----------------------------------------------------------------------------

U32 SlotAssignNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   // first eval the expression TypeReqString
   
   // if it's an array:
   
   // if OP_ADVANCE_STR 1
   // eval array
   
   // OP_ADVANCE_STR 1
   // evaluate object expr
   // OP_SETCUROBJECT 1
   // OP_SETCURFIELD 1
   // fieldName 1
   // OP_TERMINATE_REWIND_STR 1
   
   // OP_SETCURFIELDARRAY 1
   // OP_TERMINATE_REWIND_STR 1
   
   // else
   // OP_ADVANCE_STR
   // evaluate object expr
   // OP_SETCUROBJECT
   // OP_SETCURFIELD
   // fieldName
   // OP_TERMINATE_REWIND_STR
   
   // OP_SAVEFIELD
   // convert to return type if necessary.

   TypeReq subType = rhsExpr->getPreferredType();

   if (rhsExpr->canBeTyped())
   {
      subType = TypeReqTypedString;
   }
   
   TypeReq outputType = getPreferredType();
   
   codeStream.mResources->precompileIdent(slotName);


   TupleExprNode* tupleExpr = dynamic_cast<TupleExprNode*>(rhsExpr);

   TypeReq realType = tupleExpr ? TypeReqTuple : subType;

   // NOTE: We always use StringStack, but for tuples 
   // we use a frame instead so dont need to advance/push the rhs here.
   bool usingStringStack = (realType == TypeReqString || realType == TypeReqTypedString);
   
   // NOTE: compiling rhs first is compulsory in this case
   ip = rhsExpr->compile(codeStream, ip, tupleExpr ? TypeReqTuple : subType);

   if (usingStringStack)
   {
      // Normally this is a StringStack element
      codeStream.emit(OP_ADVANCE_STR);
   }

   if(arrayExpr)
   {
      ip = arrayExpr->compile(codeStream, ip, TypeReqString);
      codeStream.emit(OP_ADVANCE_STR);
   }

   if(objectExpr)
   {
      ip = objectExpr->compile(codeStream, ip, TypeReqString);
      codeStream.emit(OP_SETCUROBJECT);
   }
   else
   {
      codeStream.emit(OP_SETCUROBJECT_NEW);
   }
   
   codeStream.emit(OP_SETCURFIELD); // sets curField; curFieldArray = 0
   codeStream.emitSTE(slotName);

   if (arrayExpr)
   {
      // terminate array expr
      codeStream.emit(OP_TERMINATE_REWIND_STR);
      codeStream.emit(OP_SETCURFIELD_ARRAY);
   }
   
   // Set type FIRST
   S32 typeID = varType ? codeStream.mResources->precompileType(varType) : -1;

   // Need to set this
   if(typeID != -1)
   {
      codeStream.emit(OP_SETCURFIELD_TYPE);
      codeStream.emit(typeID);
   }

   // Need to emit tuple or stack entry
   if (tupleExpr)
   {
      codeStream.emit(OP_SAVEFIELD_MULTIPLE);
      
      // Convert back to relevant required stack by reading the field again
      // since the field could have additional transformations applied.
      switch (type)
      {
         case TypeReqUInt:
            codeStream.emit(OP_LOADFIELD_UINT);
            break;
         case TypeReqFloat:
            codeStream.emit(OP_LOADFIELD_FLT);
            break;
         case TypeReqString:
            codeStream.emit(OP_LOADFIELD_STR);
            break;
         case TypeReqVar:
            // NOTE: this is currently never set since VarNode is the only one that gets this TypeReq
            AssertFatal(false, "wtf");
            break;
         case TypeReqField:
            // do nothing
            break;
         case TypeReqTypedString:
            codeStream.emit(OP_LOADFIELD_TYPED);
            break;
         default:
            codeStream.emit(OP_SETCURFIELD_NONE);
            break;
      }
   }
   else
   {
      // Normal value assign
      // (in this case rhsExpr is on top of the stack)
      // NOTE: this is technically incorrect as any transformations made by 
      // the field will not be applied. TOFIX!!
      if (usingStringStack)
      {
         codeStream.emit(OP_TERMINATE_REWIND_STR);
      }
      
      // NOTE: this still retains the string or FLT or whatever in this case
      emitStackConversion(codeStream, subType, TypeReqField); // i.e. usually OP_SAVEFIELD_STR
      
      if (type != subType)
      {
         if (type == TypeReqField)
         {
            type = TypeReqNone;
         }
         emitStackConversion(codeStream, subType, type);
      }
   }
   
   return codeStream.tell();
}

TypeReq SlotAssignNode::getPreferredType()
{
   return disableTypes ? TypeReqString : TypeReqTypedString;
}

TypeReq SlotAssignNode::getReturnLoadType()
{
   return TypeReqField;
}

//------------------------------------------------------------

U32 SlotAssignOpNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   // first eval the expression as its type
   
   // if it's an array:
   // eval array
   // OP_ADVANCE_STR
   // evaluate object expr
   // OP_SETCUROBJECT
   // OP_SETCURFIELD
   // fieldName
   // OP_TERMINATE_REWIND_STR
   // OP_SETCURFIELDARRAY
   
   // else
   // evaluate object expr
   // OP_SETCUROBJECT
   // OP_SETCURFIELD
   // fieldName
   
   // OP_LOADFIELD of appropriate type
   // operand
   // OP_SAVEFIELD of appropriate type
   // convert to return type if necessary.
   
   getAssignOpTypeOp(op, subType, operand);
   codeStream.mResources->precompileIdent(slotName);
   
   if (dynamic_cast<TupleExprNode*>(rhsExpr))
   {
      AssertFatal(false, "Something went seriously wrong in handleExpressionTuples");
      return ip;
   }

   ip = rhsExpr->compile(codeStream, ip, subType);
   if (subType == TypeReqTypedString) codeStream.emit(OP_PUSH_TYPED);
   
   if(arrayExpr)
   {
      ip = arrayExpr->compile(codeStream, ip, TypeReqString);
      codeStream.emit(OP_ADVANCE_STR);
   }
   ip = objectExpr->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_SETCUROBJECT);
   codeStream.emit(OP_SETCURFIELD);
   codeStream.emitSTE(slotName);
   
   if(arrayExpr)
   {
      codeStream.emit(OP_TERMINATE_REWIND_STR);
      codeStream.emit(OP_SETCURFIELD_ARRAY);
   }
   
   emitStackConversion(codeStream, TypeReqField, subType);
   if (subType == TypeReqTypedString)
   {
      codeStream.emit(OP_TYPED_OP);
   }
   codeStream.emit(operand);
   // usually goes for FLT or UINT here; doesn't consume FLT or UINT
   emitStackConversion(codeStream, subType, TypeReqField);
   
   if(subType != type)
   {
      if (type == TypeReqField)
      {
         type = TypeReqNone;
      }
      emitStackConversion(codeStream, subType, type);
   }
   
   return codeStream.tell();
}

TypeReq SlotAssignOpNode::getPreferredType()
{
   getAssignOpTypeOp(op, subType, operand);
   return subType;
}

TypeReq SlotAssignOpNode::getReturnLoadType()
{
   return TypeReqField;
}

//------------------------------------------------------------

U32 ObjectDeclNode::compileSubObject(CodeStream &codeStream, U32 ip, bool root)
{
   // goes
   
   // OP_PUSHFRAME 1
   // name expr
   // OP_PUSH 1
   // args... PUSH
   // OP_CREATE_OBJECT 1
   // parentObject 1
   // isDatablock 1
   // internalName 1
   // isSingleton 1
   // lineNumber 1
   // fail point 1
   
   // for each field, eval
   // OP_ADD_OBJECT (to UINT[0]) 1
   // root? 1
   
   // add all the sub objects.
   // OP_END_OBJECT 1
   // root? 1
   // To fix the stack issue [7/9/2007 Black]
   // OP_FINISH_OBJECT <-- fail point jumps to this opcode
   
   codeStream.emit(OP_PUSH_FRAME);

   ip = classNameExpr->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_PUSH);

   ip = objectNameExpr->compile(codeStream, ip, TypeReqString);
   codeStream.emit(OP_PUSH);
   for(ExprNode *exprWalk = argList; exprWalk; exprWalk = (ExprNode *) exprWalk->getNext())
   {
      TypeReq walkType = exprWalk->getPreferredType();
      if (walkType == TypeReqNone) walkType = TypeReqString;
      ip = exprWalk->compile(codeStream, ip, walkType);
      switch (walkType)
      {
         case TypeReqFloat:
            codeStream.emit(OP_PUSH_FLT);
            break;
         case TypeReqUInt:
            codeStream.emit(OP_PUSH_UINT);
            break;
         case TypeReqTypedString:
            codeStream.emit(OP_PUSH_TYPED);
            break;
         default:
            codeStream.emit(OP_PUSH);
            break;      
      }
   }
   codeStream.emit(OP_CREATE_OBJECT);
   codeStream.emitSTE(parentObject);

   codeStream.emit(isDatablock);
   codeStream.emit(isClassNameInternal);
   codeStream.emit(isSingleton);
   codeStream.emit(dbgLineNumber);
   const U32 failIp = codeStream.emit(0);
   for(SlotAssignNode *slotWalk = slotDecls; slotWalk; slotWalk = (SlotAssignNode *) slotWalk->getNext())
      ip = slotWalk->compile(codeStream, ip, TypeReqNone);
   codeStream.emit(OP_ADD_OBJECT);
   codeStream.emit(root);
   for(ObjectDeclNode *objectWalk = subObjects; objectWalk; objectWalk = (ObjectDeclNode *) objectWalk->getNext())
      ip = objectWalk->compileSubObject(codeStream, ip, false);
   codeStream.emit(OP_END_OBJECT);
   codeStream.emit(root || isDatablock);
   // Added to fix the object creation issue [7/9/2007 Black]
   failOffset = codeStream.emit(OP_FINISH_OBJECT);
   
   codeStream.patch(failIp, failOffset);
   
   return codeStream.tell();
}

U32 ObjectDeclNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   // root object decl does:
   
   // push 0 onto the UINT stack OP_LOADIMMED_UINT
   // precompiles the subObject(true)
   // UINT stack now has object id
   // type conv to type
   
   codeStream.emit(OP_LOADIMMED_UINT);
   codeStream.emit(0);
   ip = compileSubObject(codeStream, ip, true);
   if(type != TypeReqUInt)
      emitStackConversion(codeStream, TypeReqUInt, type);
   return codeStream.tell();
}

TypeReq ObjectDeclNode::getPreferredType()
{
   return TypeReqUInt;
}

//------------------------------------------------------------

U32 FunctionDeclStmtNode::compileStmt(CodeStream &codeStream, U32 ip)
{
   // OP_FUNC_DECL
   // func name
   // namespace
   // package
   // hasBody?
   // func end ip
   // argc
   // ident array[argc]
   // code
   // OP_RETURN_VOID
   codeStream.mResources->setCurrentStringTable(&codeStream.mResources->getFunctionStringTable());
   codeStream.mResources->setCurrentFloatTable(&codeStream.mResources->getFunctionFloatTable());
   
   argc = 0;
   for(VarNode *walk = args; walk; walk = (VarNode *)((StmtNode*)walk)->getNext())
   {
      codeStream.mResources->precompileIdent(walk->varName);
      argc++;
   }
   
   codeStream.mResources->precompileIdent(fnName);
   codeStream.mResources->precompileIdent(nameSpace);
   codeStream.mResources->precompileIdent(package);
   
   codeStream.emit(OP_FUNC_DECL);
   codeStream.emitSTE(fnName);
   codeStream.emitSTE(nameSpace);
   codeStream.emitSTE(package);
   
   codeStream.emit(U32( bool(stmts != NULL) ? 1 : 0 ) + U32( dbgLineNumber << 1 ));
   const U32 endIp = codeStream.emit(0);
   codeStream.emit(argc);
   for(VarNode *walk = args; walk; walk = (VarNode *)((StmtNode*)walk)->getNext())
   {
      codeStream.emitSTE(walk->varName);
   }

   ip = compileBlock(stmts, codeStream, ip);

   // Add break so breakpoint can be set at closing brace or
   // in empty function.
   addBreakLine(codeStream);

   codeStream.emit(OP_RETURN_VOID);
   
   codeStream.patch(endIp, codeStream.tell());
   
   codeStream.mResources->setCurrentStringTable(&codeStream.mResources->getGlobalStringTable());
   codeStream.mResources->setCurrentFloatTable(&codeStream.mResources->getGlobalFloatTable());
   
   return codeStream.tell();
}


U32 CatchStmtNode::compileStmt(CodeStream &codeStream, U32 ip)
{
   if (catchBlock)
   {
      ip = compileBlock(catchBlock, codeStream, ip);
   }
   return ip;
}

U32 TryStmtNode::compileStmt(CodeStream &codeStream, U32 ip)
{
   // If there are no catch blocks, just compile the try-block as a normal block.
   if (!catchBlocks || !tryBlock)
   {
      if (tryBlock)
         ip = compileBlock(tryBlock, codeStream, ip);
      return ip;
   }

   // Pushed combined catch mask to uint stack
   bool first = true;
   for (CatchStmtNode* c = (CatchStmtNode*)catchBlocks; c; c=(CatchStmtNode*)c->next)
   {
      // Load to uint stack
      ip = c->testExpr->compile(codeStream, codeStream.tell(), TypeReqUInt);

      if (!first)
      {
         codeStream.emit(OP_BITOR);
      }
      else
      {
         first = false;
      }
   }

   // Emit the main try block + its jmp at the end
   codeStream.emit(OP_PUSH_TRY_STACK);
   endTryFixOffset = codeStream.emit(0); // -> catch block code
   ip = compileBlock(tryBlock, codeStream, ip);
   codeStream.emit(OP_POP_TRY);
   // Jump past catch blocks to end
   startEndJmpOffset = codeStream.emit(OP_JMP);
   endTryCatchOffset = codeStream.emit(0);
   codeStream.patch(endTryFixOffset, codeStream.tell());

   // Add catch handling code; input UINT stack contains value.
   for (CatchStmtNode* c = (CatchStmtNode*)catchBlocks; c; c=(CatchStmtNode*)c->next)
   {
      // Test error int
      codeStream.emit(OP_DUP_UINT);
      ip = c->testExpr->compile(codeStream, codeStream.tell(), TypeReqUInt);
      codeStream.emit(OP_BITOR);
      codeStream.emit(OP_JMPIFNOT); // next check statement
      U32 afterCatchBlockIp = codeStream.emit(0);

      // If test passes, we run the catch block
      compileBlock(c->catchBlock, codeStream, codeStream.tell());

      // Use try block exit JMP to exit
      codeStream.emit(OP_JMP);
      codeStream.emit(startEndJmpOffset);

      // Patch after block
      codeStream.patch(afterCatchBlockIp, codeStream.tell());
   }

   // Ignore exception code if for some bizarre reason it isn't handled
   codeStream.emit(OP_UINT_TO_NONE);

   // Patch JMP at end of try block
   codeStream.patch(endTryCatchOffset, codeStream.tell());

   return codeStream.tell();
}

BaseAssignExprNode* BaseAssignExprNode::findDeepestAssign()
{
   BaseAssignExprNode* lastAssign = this;
   for (BaseAssignExprNode* sn = nextAssign(); sn; sn = sn->nextAssign())
   {
      lastAssign = sn;
   }
   return lastAssign;
}

U32 TupleExprNode::compile(CodeStream &codeStream, U32 ip, TypeReq type)
{
   // if none: should be a list of statements
   // if var goes straight to var
   // all other cases should be invalid
   if (type == TypeReqTuple)
   {
      codeStream.emit(OP_PUSH_FRAME);
      for(ExprNode *walk = items; walk; walk = (ExprNode *) walk->getNext())
      {
         TypeReq walkType = walk->getPreferredType();
         if (walkType == TypeReqNone) walkType = TypeReqString;
         ip = walk->compile(codeStream, ip, walkType);
         // TODO: could do with a VarNode short-circuit here

         switch (walkType)
         {
            case TypeReqFloat:
               codeStream.emit(OP_PUSH_FLT);
               break;
            case TypeReqUInt:
               codeStream.emit(OP_PUSH_UINT);
               break;
            case TypeReqTypedString:
               codeStream.emit(OP_PUSH_TYPED);
               break;
            default:
               codeStream.emit(OP_PUSH);
               break;
         }
      }
   }
   else if (type == TypeReqNone)
   {
      for(StmtNode *walk = items; walk; walk = (ExprNode *) walk->getNext())
      {
         walk->compileStmt(codeStream, ip);
      }
   }
   else
   {
      AssertFatal(false, "Invalid type req for tuple");
   }
   return ip;
}

TypeReq TupleExprNode::getPreferredType()
{
   return TypeReqNone;
}


static U32 conversionOp(TypeReq src, TypeReq dst)
{
   // NOTE: any _TYPED conversions require the type to be set via 
   // OP_SET_DYNAMIC_TYPE_FROM_VAR or OP_SET_DYNAMIC_TYPE_FROM_FIELD
   if(src == TypeReqString)
   {
      switch(dst)
      {
      case TypeReqUInt:
         return OP_STR_TO_UINT;
      case TypeReqFloat:
         return OP_STR_TO_FLT;
      case TypeReqNone:
         return OP_STR_TO_NONE;
      case TypeReqVar:
         return OP_SAVEVAR_STR;
      case TypeReqTypedString:
         return OP_STR_TO_TYPED;
      case TypeReqField:
         return OP_SAVEFIELD_STR;
      default:
         break;
      }
   }
   else if(src == TypeReqFloat)
   {
      switch(dst)
      {
      case TypeReqUInt:
         return OP_FLT_TO_UINT;
      case TypeReqString:
         return OP_FLT_TO_STR;
      case TypeReqNone:
         return OP_FLT_TO_NONE;
      case TypeReqVar:
         return OP_SAVEVAR_FLT;
      case TypeReqTypedString:
         return OP_FLT_TO_TYPED;
      case TypeReqField:
         return OP_SAVEFIELD_FLT;
      default:
         break;
      }
   }
   else if(src == TypeReqUInt)
   {
      switch(dst)
      {
      case TypeReqFloat:
         return OP_UINT_TO_FLT;
      case TypeReqString:
         return OP_UINT_TO_STR;
      case TypeReqNone:
         return OP_UINT_TO_NONE;
      case TypeReqVar:
         return OP_SAVEVAR_UINT;
      case TypeReqTypedString:
         return OP_UINT_TO_TYPED;
      case TypeReqField:
         return OP_SAVEFIELD_UINT;
      default:
         break;
      }
   }
   else if(src == TypeReqVar)
   {
      switch(dst)
      {
      case TypeReqUInt:
         return OP_LOADVAR_UINT;
      case TypeReqFloat:
         return OP_LOADVAR_FLT;
      case TypeReqString:
         return OP_LOADVAR_STR;
      case TypeReqNone:
         return OP_COPYVAR_TO_NONE;
      // NOTE: instead we handle this manually
      //case TypeReqVar:
      //   return OP_LOADVAR_VAR; // i.e. copy this var we just set
      case TypeReqField:
         return OP_SAVEFIELD_VAR;
      case TypeReqTypedString:
         return OP_LOADVAR_TYPED;
      default:
         break;
      }
   }
   else if(src == TypeReqField)
   {
      switch(dst)
      {
      case TypeReqUInt:
         return OP_LOADFIELD_UINT;
      case TypeReqFloat:
         return OP_LOADFIELD_FLT;
      case TypeReqString:
         return OP_LOADFIELD_STR;
      case TypeReqNone:
         return OP_SETCURFIELD_NONE;
      case TypeReqVar:
         return OP_LOADFIELD_VAR; // i.e. copy this var we just set
      case TypeReqTypedString:
         return OP_LOADFIELD_TYPED;
      default:
         break;
      }
   }
   else if (src == TypeReqTypedString)
   {
      switch(dst)
      {
      case TypeReqUInt:
         return OP_TYPED_TO_UINT;
      case TypeReqFloat:
         return OP_TYPED_TO_FLT;
      case TypeReqString:
         return OP_TYPED_TO_STR;
      case TypeReqNone:
         return OP_TYPED_TO_NONE;
      case TypeReqVar:
         return OP_SAVEVAR_TYPED; // i.e. copy this var we just set
      case TypeReqTypedString:
      default:
         break;
      }
      
   }
   return OP_INVALID;
}

void StmtNode::emitStackConversion(CodeStream& codeStream, TypeReq inputType, TypeReq outputType)
{
   U32 convOp = conversionOp(inputType, outputType);
   codeStream.emit(convOp);
}
