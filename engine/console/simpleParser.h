#pragma once
#include <vector>
#include <stdexcept>
#include <string>
#include "core/stringTable.h"

namespace Compiler
{
struct Resources;
}

namespace SimpleParser
{
using TT = SimpleLexer::TokenType;
using TOK = SimpleLexer::Token;

class TokenError : public std::runtime_error
{
public:
   TokenError(const TOK& tok, TT expected, const char* msg)
   : std::runtime_error(msg ? msg : "token error"),
   mToken(tok),
   mExpected(expected) {}
   
   TokenError(const TOK& tok, TT expected, std::string_view msg)
   : TokenError(tok, expected, std::string(msg).c_str()) {}
   
   const TOK& token()    const noexcept { return mToken; }
   TT    expected() const noexcept { return mExpected; }
   
private:
   TOK     mToken;
   TT mExpected;
};

class ASTGen
{
public:
   
   ASTGen(SimpleLexer::Tokenizer* tok, Compiler::Resources* res)
   : mTokenizer(tok), mTokenPos(0), mResources(res)
   {
   }
   
   bool processTokens()
   {
      TOK t;
      for (t = mTokenizer->next(); !(t.isNone() || t.isIllegal() || t.isEnd()); t = mTokenizer->next())
      {
         mTokens.push_back(t);
      }
      
      if (t.isIllegal() || t.isNone())
      {
         mTokens.clear();
         return false;
      }
      
      return true;
   }
   
   // start : decl_list ;
   StmtNode* parseProgram()
   {
      StmtNode* list = NULL;
      while (!atEnd())
      {
         StmtNode* d = parseDecl();
         if (list == NULL)
         {
            list = d;
         }
         else
         {
            list->append(d);
         }
      }
      return list;
   }
   
private:
   
   SimpleLexer::Tokenizer* mTokenizer;
   std::vector<TOK> mTokens;
   U64 mTokenPos;
   Compiler::Resources* mResources;
   
   // Token helpers
   
   const TOK& LA(size_t k=0) const
   {
      static TOK eof; // default END
      return (mTokenPos+k < mTokens.size()) ? mTokens[mTokenPos+k] : eof;
   }
   
   bool atEnd() const
   {
      return LA().kind == TT::END;
   }
   
   bool match(TT t, S32* lineNo = NULL)
   {
      if (LA().kind == t) { if (lineNo) *lineNo = LA().pos.line; ++mTokenPos; return true; }
      return false;
   }
   
   bool matchChar(char c, S32* lineNo = NULL)
   {
      if (LA().kind == TT::opCHAR && LA().ivalue == c) { if (lineNo) *lineNo = LA().pos.line; ++mTokenPos; return true; }
      return false;
   }

   U32 LAChar(size_t k = 0) const
   {
      TOK laTok = LA(k);
      return (laTok.kind == TT::opCHAR) ? laTok.ivalue : 0;
   }
   
   TOK expect(TT t, const char* what)
   {
      if (LA().kind != t)
      {
         throw TokenError(LA(), t, what);
      }
      return mTokens[mTokenPos++];
   }
   
   TOK expectEither(TT a, TT b, const char* what)
   {
      if (!(LA().kind == a || LA().kind == b))
      {
         throw TokenError(LA(), a, what);
      }
      return mTokens[mTokenPos++];
   }
   
   TOK expectChar(char c, const char* what)
   {
      if (LA().kind != TT::opCHAR || LA().ivalue != (U64)c)
      {
         throw TokenError(LA(), TT::opCHAR, what);
      }
      return mTokens[mTokenPos++];
   }
   
   void errorHere(const TOK& tok, std::string msg)
   {
      throw TokenError(tok, TT::NONE, msg);
   }
   
   inline SimpleLexer::TokenType processCharOp(const SimpleLexer::Token& t)
   {
      if (t.kind == TT::opCHAR)
      {
         switch ((char)t.ivalue) {
            case '+': return TT::opPCHAR_PLUS;
            case '-': return TT::opPCHAR_MINUS;
            case '/': return TT::opPCHAR_SLASH;
            case '*': return TT::opPCHAR_ASTERISK;
            case '^': return TT::opPCHAR_CARET;
            case '%': return TT::opPCHAR_PERCENT;
            case '&': return TT::opPCHAR_AMPERSAND;
            case '|': return TT::opPCHAR_PIPE;
            case '<': return TT::opPCHAR_LESS;
            case '>': return TT::opPCHAR_GREATER;
            case '!': return TT::opPCHAR_EXCL;
            case '~': return TT::opPCHAR_TILDE;
             default:
               return TT::ILLEGAL;
         }
      }
      else
      {
         // Map compound-assign tokens to their underlying binary operator.
         switch (t.kind) {
             case TT::opPLASN:  return TT::opPCHAR_PLUS;       // +=  → +
             case TT::opMIASN:  return TT::opPCHAR_MINUS;      // -=  → -
             case TT::opMLASN:  return TT::opPCHAR_ASTERISK;   // *=  → *
             case TT::opDVASN:  return TT::opPCHAR_SLASH;      // /=  → /
             case TT::opMODASN: return TT::opPCHAR_PERCENT;    // %=  → %
             case TT::opANDASN: return TT::opPCHAR_AMPERSAND;  // &=  → &
             case TT::opXORASN: return TT::opPCHAR_CARET;      // ^=  → ^
             case TT::opORASN:  return TT::opPCHAR_PIPE;       // |=  → |
             case TT::opSLASN:  return TT::opSHL;              // <<= → <<
             case TT::opSRASN:  return TT::opSHR;              // >>= → >>
             default:           return t.kind;                 // fallback (shouldn’t hit for assigns)
         }
      }
   }
   
   
   // ===== decl_list / decl =====
   // decl : stmt | fn_decl_stmt | package_decl
   StmtNode* parseDecl()
   {
      switch (LA().kind)
      {
         case TT::rwDEFINE:   // "function"
            return parseFnDeclStmt();
         case TT::rwPACKAGE:  // "package"
            return parsePackageDecl();
         default:
            return parseStmtNode();
      }
   }
   
   // Parses { ... }
   StmtNode* parseBlockStmt()
   {
      expectChar('{', "'{' expected");
      StmtNode* list = parseStmtNodeListUntilSC();
      expectChar('}', "'}' expected");
      return list;
   }
   
   // Helper for if and such
   inline StmtNode* parseStmtOrBlock()
   {
      if (LA().kind == TT::opCHAR && LA().ivalue == '{')
      {
         return parseBlockStmt();
      }
      return parseStmtNode();
   }
   
   // Handles statement lists
   //
   // statement_list : (empty) | statement_list stmt
   StmtNode* parseStmtNodeListUntilSC()
   {
      StmtNode* listHead = NULL;
      StmtNode* listTail = NULL;
      while (!atEnd() && !(LA().kind == TT::opCHAR && LA().ivalue == '}'))
      {
         StmtNode* s = parseStmtNode();
         if (listTail)
         {
            listTail->append(s);
            listTail = s;
         }
         else
         {
            listHead = s;
            listTail = s;
         }
      }
      return listHead;
   }

   VarNode* parseTypedVar(TOK v)
   {
      StringTableEntry assignTypeName = NULL;

      if (matchChar(':')) // is typed var
      {
         TOK typeNameTok = expect(TT::IDENT, "expected type name");
         assignTypeName = typeNameTok.stString;
      }
      else if (LAChar() == '[') // array; types not allowed here
      {
         return NULL;
      }

      return VarNode::alloc(mResources, v.pos.line, v.stString, NULL, assignTypeName);
   }
   
   // Handles var lists
   // (basically a restricted form of parseExprListOptUntil)
   //
   // var_list_decl : (empty) | var_list
   // var_list : VAR | var_list ',' VAR
   VarNode* parseVarList()
   {
      // Handle empty
      if (LA().kind == TT::opCHAR && LA().ivalue == ')')
         return NULL;
      
      // First var
      TOK v = expect(TT::VAR, "parameter name expected");
      VarNode* head = parseTypedVar(v);
      VarNode* tail = head;
      
      // Subsequent vars
      while (matchChar(','))
      {
         TOK t = expect(TT::VAR, "parameter name expected");
         VarNode* nxt = parseTypedVar(v);
         tail->append(nxt);
         tail = nxt;
      }
      
      return head;
   }
   
   // Handles generic lists of expressions
   //
   // expr_list_decl : (empty) | expr_list
   // expr_list : expr | expr_list ',' expr
   ExprNode* parseExprListOptUntil(char endCh)
   {
      // Handle empty
      if (LA().kind == TT::opCHAR && LA().asChar() == endCh)
         return NULL;
      
      // head
      ExprNode* head = parseExprNode();
      ExprNode* tail = head;
      
      // , ...
      while (matchChar(',')) {
         ExprNode* e = parseExprNode();
         tail->append(e);
         tail = e;
      }
      
      return head;
   }
   
   // Handles if conditional
   //
   // if_stmt : rwIF '(' expr ')' stmt_block | rwIF '(' expr ')' stmt_block rwELSE stmt_block
   StmtNode* parseIfStmt()
   {
      TOK ifTok = expect(TT::rwIF, "'if' expected");
      expectChar('(', "'(' expected");
      ExprNode* cond = parseExprNode();
      expectChar(')', "')' expected");
      StmtNode* thenS = parseStmtOrBlock();
      StmtNode* elseS = NULL;
      if (match(TT::rwELSE))
      {
         elseS = parseStmtOrBlock();
      }
      return IfStmtNode::alloc(mResources, ifTok.pos.line, cond, thenS, elseS, false); // propagate=false
   }
   
   // Handles try block
   //
   // try_stmt : rwTRY stmt_block catch_chain
   TryStmtNode* parseTryStmt()
   {
      if (!mResources->allowExceptions)
      {
         errorHere(LA(), "Exceptions disabled");
         return NULL;
      }
      
      TOK tryTok = expect(TT::rwTRY, "'try' expected");
      StmtNode* tryBlock = parseBlockStmt();
      CatchStmtNode* catchChain = parseCatchChain();
      if (!catchChain)
      {
         errorHere(LA(), "Expected one or more catch blocks");
         return NULL;
      }
      
      TryStmtNode* tryStmt = TryStmtNode::alloc(mResources, tryTok.pos.line, tryBlock, catchChain);
      return tryStmt;
   }
   
   // Handles series of catch blocks
   // catch_block:  rwCATCH '(' expr ')' stmt_block
   // catch_chain: catch_block | catch_block catch_chain
   CatchStmtNode* parseCatchChain()
   {
      CatchStmtNode* startNode = NULL;
      CatchStmtNode* tailNode = NULL;
      S32 catchLineNo = 0;
      
      while (match(TT::rwCATCH, &catchLineNo))
      {
         expectChar('(', "'(' expected");
         ExprNode* testExpr = parseExprNode();
         expectChar(')', "')' expected");
         StmtNode* condBlock = parseBlockStmt();
         if (!condBlock)
         {
            errorHere(LA(), "Expected {...}");
            return NULL;
         }
         
         // NOTE: Should be in definition order despite the stack implying otherwise,
         // since the actual case statements are checked in the bytecode not on the stack.
         CatchStmtNode* newCond = CatchStmtNode::alloc(mResources, catchLineNo, testExpr, condBlock);
         if (startNode)
         {
            tailNode->append(newCond);
            tailNode = newCond;
         }
         else
         {
            startNode = newCond;
            tailNode = newCond;
         }
      }
      
      return startNode;
   }
   
   // Handles do / while conditionals
   //
   // while_stmt : rwWHILE '(' expr ')' stmt_block | rwDO stmt_block rwWHILE '(' expr ')'
   StmtNode* parseWhileLike()
   {
      if (match(TT::rwWHILE))
      {
         TOK wTok = LA(-1); // last consumed
         expectChar('(', "'(' expected");
         ExprNode* test = parseExprNode();
         expectChar(')', "')' expected");
         StmtNode* body = parseStmtOrBlock();
         return LoopStmtNode::alloc(mResources, wTok.pos.line, NULL, test, NULL, body, false);
      }
      else
      {
         TOK dTok = expect(TT::rwDO, "'do' expected");
         StmtNode* body = parseStmtOrBlock();
         expect(TT::rwWHILE, "'while' expected");
         expectChar('(', "'(' expected");
         ExprNode* test = parseExprNode();
         expectChar(')', "')' expected");
         expectChar(';', "';' expected");
         return LoopStmtNode::alloc(mResources, dTok.pos.line, NULL, test, NULL, body, true);
      }
   }
   
   // Handles for iterator
   //
   // for_stmt : rwFOR '(' expr ';' expr ';' expr ')' stmt_block
   // (variants for every case of lack of expr)
   StmtNode* parseForStmt()
   {
      TOK fTok = expect(TT::rwFOR, "'for' expected");
      expectChar('(', "'(' expected");
      
      // init ;
      ExprNode* init = NULL;
      if (!(LA().kind == TT::opCHAR && LA().ivalue == ';'))
         init = parseExprNode();
      expectChar(';', "';' expected");
      
      // test ;
      ExprNode* test = NULL;
      if (!(LA().kind == TT::opCHAR && LA().ivalue == ';'))
         test = parseExprNode();
      expectChar(';', "';' expected");
      
      // end )
      ExprNode* end = NULL;
      if (!(LA().kind == TT::opCHAR && LA().ivalue == ')'))
         end = parseExprNode();
      expectChar(')', "')' expected");
      
      StmtNode* body = parseStmtOrBlock();
      
      // If test omitted, treat as true (1).
      if (!test) test = IntNode::alloc(mResources, fTok.pos.line, 1);
      return LoopStmtNode::alloc(mResources, fTok.pos.line, init, test, end, body, false);
   }
   
   // Handles foreach & foreach$ iterators
   //
   // foreach_stmt : rwFOREACH '(' VAR rwIN expr ')' stmt_block | rwFOREACHSTR '(' VAR rwIN expr ')' stmt_block
   StmtNode* parseForeachStmt()
   {
      bool isStr = false;
      TOK t;
      if (match(TT::rwFOREACHSTR)) { isStr = true; t = LA(-1); }
      else { t = expect(TT::rwFOREACH, "'foreach' expected"); }
      
      expectChar('(', "'(' expected");
      TOK v = expect(TT::VAR, "loop variable (VAR) expected");
      expect(TT::rwIN, "'in' expected");
      ExprNode* cont = parseExprNode();
      expectChar(')', "')' expected");
      
      StmtNode* body = parseStmtOrBlock();
      return IterStmtNode::alloc(mResources, t.pos.line, v.stString, cont, body, isStr);
   }
   
   // Emits either a string or int equality node
   //
   ExprNode* emitEqNode(S32 line, ExprNode* left, ExprNode* right, bool string)
   {
      if (string)
      {
         return StreqExprNode::alloc(mResources, line, left, right, true);
      }
      else
      {
         return IntBinaryExprNode::alloc(mResources, line, processCharOp(TT::opEQ), left, right);
      }
   }
   
   // case_expr : expr | case_expr rwCASEOR expr
   ExprNode* parseCaseExprList()
   {
      ExprNode* head = parseExprNode();
      ExprNode* tail = head;
      while (match(TT::rwCASEOR))
      {
         ExprNode* e = parseExprNode();
         tail->append(e);
         tail = e;
      }
      return head;
   }
   
   // Parse the case body (statement_list) until we hit 'case', 'default' or '}'
   StmtNode* parseCaseBody()
   {
      StmtNode* head = NULL;
      StmtNode* tail = NULL;
      
      while (!atEnd())
      {
         const auto& k = LA();
         if ((k.kind == TT::rwCASE) || (k.kind == TT::rwDEFAULT) ||
             (k.kind == TT::opCHAR && k.ivalue == '}'))
            break;
         
         StmtNode* s = parseStmtNode();
         if (!head) head = s;
         else tail->append(s);
         
         // walk tail to end (parseStmtNode may return lists)
         tail = head;
         while (tail && tail->getNext()) tail = tail->getNext();
      }
      return head;
   }
   
   // Handles case ...: ...
   //
   // case_block : rwCASE case_expr ':' statement_list case_block
   // (variants of case_block to handle token conflicts)
   IfStmtNode* parseCaseBlock()
   {
      // case ...
      SimpleLexer::Token caseTok = expect(TT::rwCASE, "'case' expected");
      ExprNode* list = parseCaseExprList();     // store the *list* as testExpr for now
      expectChar(':', "':' expected after case");
      
      StmtNode* body = parseCaseBody();
      
      // default? next case? or end?
      if (match(TT::rwDEFAULT))
      {
         expectChar(':', "':' expected after default");
         StmtNode* defBody = parseCaseBody();
         // CASE ... ':' stmts DEFAULT ':' stmts
         return IfStmtNode::alloc(mResources, caseTok.pos.line, list, body, defBody, false);
      }
      
      if (LA().kind == TT::rwCASE)
      {
         // CASE ... ':' stmts case_block
         IfStmtNode* rest = parseCaseBlock();
         return IfStmtNode::alloc(mResources, caseTok.pos.line, list, body, rest, true);
      }
      
      // CASE ... ':' stmts
      return IfStmtNode::alloc(mResources, caseTok.pos.line, list, body, NULL, false);
   }
   
   // Handles switch statement
   //
   // switch_stmt : rwSWITCH '(' expr ')' '{' case_block '}' | wSWITCHSTR '(' expr ')' '{' case_block '}'
   StmtNode* parseSwitchStmt()
   {
      bool isString = false;
      SimpleLexer::Token sw;
      if (match(TT::rwSWITCHSTR))
      {
         isString = true;
         sw = LA(-1);
      }
      else
      {
         sw = expect(TT::rwSWITCH, "'switch' expected");
      }
      
      expectChar('(', "'(' expected");
      ExprNode* selector = parseExprNode();
      expectChar(')', "')' expected");
      expectChar('{', "'{' expected");
      
      // Must start with 'case' per grammar; 'default' first is invalid.
      if (LA().kind != TT::rwCASE)
      {
         errorHere(LA(), "expected 'case' to start switch block");
      }
      
      IfStmtNode* root = parseCaseBlock();
      
      expectChar('}', "'}' expected");
      
      // Now attach selector to each case by expanding the stored lists into ORs of (selector == expr)
      root->propagateSwitchExpr(mResources, selector, isString);
      return root;
   }
   
   // Checks if the current token the start of a slot assignment.
   //
   // slot_assign starts with: IDENT ...  |  rwDATABLOCK
   bool beginsSlotAssign() const
   {
      using K = TT;
      const TOK& t = LA();
      if (t.kind == K::rwDATABLOCK) return true;
      if (t.kind != K::IDENT)       return false;
      
      // IDENT can be either:   IDENT '=' expr ';'
      // or typed:              TYPEIDENT IDENT '=' expr ';'
      // or array variants:     IDENT '[' ... ']' '=' expr ';' (and typed variant)
      //
      // NOTE: in KorkScript we instead use the following for types:
      // IDENT : IDENT = expr;
      // IDENT '[' ... ']' : IDENT = expr;
      //
      // We conservatively say "yes" if after one (or two) idents we find '=' or '['.
      const TOK& n1 = LA(1);
      if (n1.kind == K::opCHAR && (n1.asChar() == '=' || n1.asChar() == '[' ||
                                   n1.asChar() == ':')) // TYPED variable extension
      {
         return true;
      }
      
      // typed form: IDENT IDENT <'='|'['> ...
      // KorkScript: this doesnt seem to be used in normal scripts, so we're ditching it to simplify
      /*if (n1.kind == K::IDENT)
      {
         const TOK& n2 = LA(2);
         if (n2.kind == K::opCHAR && (n2.asChar() == '=' || n2.asChar() == '[')) 
         {
            return true;
         }
      }*/
      
      return false;
   }
   
   // Handles 
   //   foo = expr;
   // Inside an object decl.
   //
   SlotAssignNode* parseSlotAssign(ExprNode* object)
   {
      using K = TT;
      
      S32 line = LA().pos.line;
      StringTableEntry slotName = 0;
      StringTableEntry typeName = NULL;
      ExprNode* aidx = NULL;
      
      // IDENT ... (maybe typed)
      // DATABLOCK ... (maybe typed)
      TOK startToken = expectEither(K::rwDATABLOCK, K::IDENT, "ident or 'datablock' expected");
      if (startToken.kind == K::rwDATABLOCK)
      {
         slotName = StringTable->insert("datablock");
      }
      else
      {
         // NOTE: for now ignoring typed fields
         slotName = startToken.stString;
      }

      // Optional '[' aidx_expr ']'
      if (matchChar('['))
      {
         aidx = parseAidxExprNode();  // you already have this
         expectChar(']', "] expected");
      }

      // Type name
      if (matchChar(':'))
      {
         TOK typeNameTok = expect(TT::IDENT, "type name expected");
         typeName = typeNameTok.stString;
      }
      
      // '=' expr ';'
      expectChar('=', "= expected");
      ExprNode* rhs = parseExprNode();
      rhs = handleExpressionTuples(rhs, true); // handle tuple expr after
      expectChar(';', "; expected");
      
      return SlotAssignNode::alloc(mResources, line, object, aidx, slotName, rhs, typeName);
   }
   
   // Handles list of slot_asign ending with '}'
   //
   // slot_assign_list : slot_assign | slot_assign_list slot_assign
   SlotAssignNode* parseSlotAssignList(ExprNode* objectNode)
   {
      SlotAssignNode* head = NULL;
      SlotAssignNode* tail = NULL;
      
      while (!atEnd())
      {
         // list ends at '}'
         if (LA().kind == TT::opCHAR && LA().asChar() == '}')
            break;
         
         if (!beginsSlotAssign())
            break; // be strict: anything else ends the list (caller will error if unexpected)
         
         SlotAssignNode* one = parseSlotAssign(objectNode);
         if (!head) head = tail = one;
         else       { tail->append(one); tail = one; }
      }
      return head;
   }
   
   // Handles optional list of slot_asign
   //
   // slot_assign_list_opt : (empty) | slot_assign_list
   SlotAssignNode* parseSlotAssignListOpt(ExprNode* object)
   {
      // empty if we’re right before '}'
      if (LA().kind == TT::opCHAR && LA().asChar() == '}')
         return NULL;
      
      return parseSlotAssignList(object);
   }
   
   inline bool beginsObjectDecl() const
   {
      using K = TT;
      TT k = LA().kind;
      return k == K::rwDECLARE || k == K::rwDECLARESINGLETON;
   }
   
   // handles new ObjectClass(name) { ... }
   //
   //
   ObjectDeclNode* parseObjectDecl(bool isExpr)
   {
      using K = TT;
      S32 line = LA().pos.line;
      
      // new | singleton
      TOK startToken;
      if (isExpr)
      {
         startToken = LA(-1);
      }
      else
      {
         startToken = LA();
         mTokenPos++;
      }
      
      // class_name_expr
      ExprNode* klassName = parseClassNameExpr();
      if (!klassName) errorHere(LA(), "class name expression expected");
      
      // Can be:
      // class_name_expr ( object_name parent_block object_args )
      // class_name_expr ( [ object_name ] parent_block object_args )     (object_name becomes internal name)
      
      expectChar('(', "'(' expected");
      ExprNode* objectNameExpr = NULL;
      ExprNode* argList = NULL;
      StringTableEntry parentObject = 0;
      bool isInternal = false;
      
      if (!matchChar(')'))
      {
         if (matchChar('['))
         {
            mTokenPos++;
            objectNameExpr = parseExprNode();
            expectChar(']', "need closing ] on object name");
            isInternal = true;
         }
         else
         {
            objectNameExpr = parseExprNode();
         }
         
         if (matchChar(':')) {
            TOK p = expect(K::IDENT, "identifier expected after ':' (parent object)");
            parentObject = p.stString;
         }
         
         // args
         argList = matchChar(',') ? parseExprListOptUntil(')') : NULL;
         
         expectChar(')', "')' expected");
      }
      
      // If no object name, alloc ""
      if (objectNameExpr == NULL)
      {
         char empty[1];
         empty[0] = '\0';
         objectNameExpr = StrConstNode::alloc(mResources, line, empty, false);
      }
      
      // Optional { slots }
      SlotAssignNode* slots = NULL;
      ObjectDeclNode* subs = NULL;
      if (matchChar('{'))
      {
         // 1) slots first (relative to this object; pass NULL handles correctly on compile)
         slots = parseSlotAssignListOpt(/*curObjExpr*/NULL);
         
         // 2) then nested objects (zero or more)
         ObjectDeclNode* head = NULL;
         ObjectDeclNode* tail = NULL;
         
         while (beginsObjectDecl())
         {
            ObjectDeclNode* child = parseObjectDecl(false);
            if (!child) { errorHere(LA(-1), "nested object parse failure"); break; }
            
            if (!head) head = tail = child;
            else { tail->append(child); tail = child; }
         }
         
         subs = head;
         
         expectChar('}', "'}' expected");
      }
      
      if (!isExpr)
      {
         expectChar(';', "';' expected");
      }
      
      return ObjectDeclNode::alloc(mResources, line, klassName, objectNameExpr, argList,
                                   parentObject, slots, subs,
                                   /*isDatablock*/false, isInternal,
                                   startToken.kind == K::rwDECLARESINGLETON);
   }
   
   // Handles parsing a class name to an expr
   //
   ExprNode* parseClassNameExpr()
   {
      if (matchChar('('))
      {
         ExprNode* expr = parseExpression(0);
         expectChar(')', "expected closing brace");
         return expr;
      }
      else
      {
         TOK identName = LA();
         expect(TT::IDENT, "expected ident");
         return ConstantNode::alloc(mResources, identName.pos.line, identName.stString);
      }
   }
   
   // Handles parsing datablock statement
   //
   // rwDATABLOCK class_name_expr '(' expr parent_block ')'  '{' slot_assign_list_opt '}' ';'
   // NOTE: datablocks only contain slots
   StmtNode* parseDatablockDecl()
   {
      S32 line = LA().pos.line;
      StringTableEntry parentObject = NULL;
      expect(TT::rwDATABLOCK, "datablock expected");
      
      // class_name_expr
      TOK startToken = LA();
      ExprNode* klassNameNode = parseClassNameExpr();
      if (klassNameNode == NULL)
      {
         errorHere(startToken, "class name expression expected");
      }
      
      expectChar('(', "'(' expected");
      // expr (: IDENT)?
      ExprNode* nameExpr = parseExprNode();
      if (matchChar(':'))
      {
         TOK p = expect(TT::IDENT, "identifier expected after ':' (parent datablock name)");
         parentObject = p.stString;
      }
      expectChar(')', "')' expected");
      
      SlotAssignNode* slotAssignNode = NULL;
      expectChar('{', "{ expected for datablock");
      slotAssignNode = parseSlotAssignListOpt(NULL);
      expectChar('}', "} expected");
      expectChar(';', "; expected");
      
      return ObjectDeclNode::alloc(mResources, line, klassNameNode, nameExpr, NULL, parentObject, slotAssignNode, NULL, /*isDatablock*/true, /*isSingleton*/false, /*isNewExpr*/false);
   }
   
   // Handles function definitions such as function foo(...) { ... }
   //
   StmtNode* parseFnDeclStmt()
   {
      S32 line = 0;
      expect(TT::rwDEFINE, "'function' expected"); // "function"
      
      // [Namespace::]Ident
      TOK a = expect(TT::IDENT, "identifier expected");
      StringTableEntry ns  = 0;
      StringTableEntry fn  = a.stString;
      
      if (match(TT::opCOLONCOLON))
      {
         TOK b = expect(TT::IDENT, "identifier expected after '::'");
         ns = a.stString;  // first is namespace
         fn = b.stString;  // second is function name
      }
      
      // ( ... )
      expectChar('(', "'(' expected");
      VarNode* args = parseVarList();
      expectChar(')', "')' expected");

      // Type decl (optional)
      StringTableEntry retTypeName = nullptr;
      if (matchChar(':'))
      {
         auto typeTok = expect(SimpleLexer::TokenType::IDENT, "return type expected");
         retTypeName = typeTok.stString;
      }
      
      // { ... }
      StmtNode* body = parseBlockStmt();
      
      FunctionDeclStmtNode* stmt = FunctionDeclStmtNode::alloc(mResources, line ? line : a.pos.line, fn, ns, args, body, retTypeName);
      return stmt;
   }
   
   // Handles a list of function definitions
   //
   // fn_decl_list : fn_decl_stmt | fn_decl_list fn_decl_stmt
   StmtNode* parseFnDeclList()
   {
      // fn_decl_list : fn_decl_stmt | fn_decl_list fn_decl_stmt
      StmtNode* head = parseFnDeclStmt();
      StmtNode* tail = head;
      while (LA().kind == TT::rwDEFINE)
      {
         StmtNode* nxt = parseFnDeclStmt();
         tail->append(nxt);
         tail = tail->getTail();
      }
      return head;
   }
   
   // Handles package block
   //
   // package_decl : rwPACKAGE IDENT '{' fn_decl_list '}' ';'
   StmtNode* parsePackageDecl()
   {
      S32 line = 0;
      expect(TT::rwPACKAGE, "'package' expected");
      TOK nameTok = expect(TT::IDENT, "package name expected");
      
      expectChar('{', "'{' expected");
      StmtNode* fns = NULL;
      if (!(LA().kind == TT::opCHAR && LA().ivalue == '}'))
      {
         fns = parseFnDeclList();
      }
      expectChar('}', "'}' expected");
      expectChar(';', "';' expected");
      
      // Attach package name to each function in list (if any)
      for (StmtNode* w = fns; w; w = w->getNext())
      {
         w->setPackage(nameTok.stString);
      }
      
      return fns;
   }
   
   // Handles expression node
   ExprNode* parseExprNode()
   {
      return parseExpression(0);
   }
   
   // Handles all statement nodes
   //
   StmtNode* parseStmtNode()
   {
      const TOK& t = LA();
      switch (t.kind)
      {
         case TT::rwIF:
            return parseIfStmt();
         
         case TT::rwTRY:
            return parseTryStmt();
         
         case TT::rwWHILE:
         case TT::rwDO:
            return parseWhileLike();
            
         case TT::rwFOR:
            return parseForStmt();
            
         case TT::rwFOREACH:
         case TT::rwFOREACHSTR:
            return parseForeachStmt();
            
         case TT::rwSWITCH:
         case TT::rwSWITCHSTR:
            return parseSwitchStmt();
            
         case TT::rwDATABLOCK:
            return parseDatablockDecl();
            
         case TT::rwDECLARE:             // 'new'
         case TT::rwDECLARESINGLETON:  { // 'singleton'
            return parseObjectDecl(false);
         }
            
         case TT::rwBREAK: {
            TOK tok = mTokens[mTokenPos++]; expectChar(';', "; expected");
            return BreakStmtNode::alloc(mResources, (S32)tok.pos.line);
         }
         case TT::rwCONTINUE: {
            TOK tok = mTokens[mTokenPos++]; expectChar(';', "; expected");
            return ContinueStmtNode::alloc(mResources, (S32)tok.pos.line);
         }
         case TT::rwRETURN: {
            TOK tok = mTokens[mTokenPos++];
            if (!matchChar(';'))
            {
               ExprNode*  e = parseExprNode();
               expectChar(';', "; expected");
               return ReturnStmtNode::alloc(mResources, (S32)tok.pos.line, e);
            }
            return ReturnStmtNode::alloc(mResources, (S32)tok.pos.line, NULL);
         }
         case TT::rwASSERT:
            return parseAssertStmt();
            
         case TT::DOCBLOCK: { // also valid inside blocks
            TOK tok = mTokens[mTokenPos++];
            return StrConstNode::alloc(mResources, tok.pos.line, mTokenizer->bufferAtOffset(tok.stringValue.offset), false, true, tok.stringValue.len);
         }

         // NOTE: in effect this allows:
         //   %var : type = expr
         //   %var[expr]
         // If there is a typed expression without an assignment, this will only set the type hint 
         // for the variable name. Typed array accessors are not permitted.
         // ALSO: %var.slot : type is not allowed here; instead thats handled by parseSlotAssign.
         case TT::VAR: {
            mTokenPos++; // NEXT token
            VarNode* node = parseTypedVar(t); // NOTE: parses the initial %var and optional type (IGNORES arrays)
            if (node == NULL)
            {
               mTokenPos--; // rewind; something went wrong
            }
            ExprNode* firstExpr = node ? parseExpressionFrom(node) : parseExpression(0); // should end up with an assignment
            ExprNode* tailExpr = firstExpr;

            firstExpr = handleExpressionTuples(firstExpr);
            
            // Finally ends with ;
            expectChar(';', "; expected");
            return firstExpr;
         }

         // NOTE: handles expressions which dont start with VAR
         default: {
            // expression_stmt ';'
            ExprNode*  e = parseStmtNodeExprNode();
            expectChar(';', "; expected");
            return e;
         }
      }
   }

   // Handles case where statement may be a tuple
   ExprNode* handleExpressionTuples(ExprNode* firstExpr, bool isSlotAssign=false)
   {
      // Additional items get appended onto the root expr; if this needs  
      // to be a list that will get handled there.
      if (LAChar(0) == ',')
      {
         // NOTE: in this case we allow:
         // %var : type, %var2 : type ...
         // %var : type = 1, 2, 3;
         // %var : type = %otherVar = 1,2,3;
         //
         // In the assign case, everything gets assigned to the
         // ALSO: all dependent assigns will get assigned the type at the root.
         BaseAssignExprNode* firstAssign = firstExpr->asAssign();
         BaseAssignExprNode* lastAssign = firstAssign ? firstAssign->findDeepestAssign() : NULL;
         TupleExprNode* tupleExpr = TupleExprNode::alloc(mResources, firstAssign ? firstAssign->dbgLineNumber : firstExpr->dbgLineNumber, firstExpr);

         if (lastAssign)
         {
            // Replace RHS of last assignment with the tuple
            tupleExpr->items = lastAssign->rhsExpr;
            lastAssign->rhsExpr = tupleExpr;
            
            // %var = ... case
            while (matchChar(','))
            {
               ExprNode* nextExpr = parseExpression(0);
               if (nextExpr)
               {
                  tupleExpr->items->append(nextExpr);
               }
            }
         }
         else
         {
            // list of expressions; emit a distinct tuple
            while (matchChar(','))
            {
               VarNode* nextVar = isSlotAssign ? NULL : parseTypedVar(LA());
               ExprNode* nextExpr = nextVar ? parseExpressionFrom(nextVar) : parseExpression(0);
               if (nextExpr)
               {
                  tupleExpr->items->append(nextExpr);
               }
            }
            firstExpr = tupleExpr;
         }
      }

      return firstExpr;
   }

   // Handles typed var decl or assignment
   //
   // typed_var : VAR ':'' IDENT ';'
   // typed_var : VAR ':'' IDENT '=' expr ';'
   StmtNode* parseTypedVarAssignment()
   {
      // Look ahead in this case since we fall back to using parseStmtNodeExprNode
      if (LA(0).kind == TT::VAR)
      {
         if (LAChar(1) == ':' &&
             LA(2).kind != TT::IDENT)
         {
            errorHere(LA(2), "expected type name after ':'");
            return NULL;
         }
         else
         {
            return NULL;
         }
      }

      // Parse typed var
      // NOTE: var nodes get emitted regardless (even if not directly used) so this is valid.

      VarNode* var = parseTypedVar(expect(TT::VAR, "var expected"));
      return NULL;
   }
   
   // Handles assert expression
   //
   // assert_expr : rwASSERT '(' expr ')' | rwASSERT '(' expr ',' STRATOM ')'
   StmtNode* parseAssertStmt()
   {
      TOK kw = expect(TT::rwASSERT, "'assert' expected");
      expectChar('(', "'(' expected after assert");
      
      ExprNode* cond = parseExprNode();
      const char* msg  = NULL;
      
      if (matchChar(','))
      {
         expect(TT::STRATOM, "assert requires message string");
         msg = mTokenizer->bufferAtOffset(LA(-1).stringValue.offset);
      }
      
      expectChar(')', "')' expected after assert(...)");
      expectChar(';', "';' expected after assert(...)");
      
      return AssertCallExprNode::alloc(mResources, kw.pos.line, cond, msg);
   }
   
   // Handles array expression a,b,c ...
   //
   // aidx_expr : expr (',' expr)*
   ExprNode*  parseAidxExprNode()
   {
      ExprNode* headExpr = parseExprNode(); // first param
      ExprNode* head = headExpr;
      S32 lineNo = 0;
      
      while (matchChar(',', &lineNo))
      {
         ExprNode* next = parseExprNode();
         ExprNode* catOp = CommaCatExprNode::alloc(mResources, lineNo, head, next);
         head = catOp;
      }
      
      return head;
   }
   
   
   // Statement node wrapper
   ExprNode*  parseStmtNodeExprNode()
   {
      return parseExprNode();
   }
   
   // Precedence helpers (bind powers)
   enum Assoc { LEFT, RIGHT };
   
   // Scores precedence of expressions
   //
   int lbp(const TOK& t) const
   {
      switch (t.kind)
      {
            // Assignments (right-assoc) — lowest
         case TT::opPLASN: case TT::opMIASN: case TT::opMLASN: case TT::opDVASN:
         case TT::opMODASN: case TT::opANDASN: case TT::opXORASN: case TT::opORASN:
         case TT::opSLASN:  case TT::opSRASN:
            return 10;
            
         case TT::opPLUSPLUS:            return 145;   // postfix ++
         case TT::opMINUSMINUS:          return 145;   // postfix --
            
            // ||, &&, |, ^, &, == !=, rel, concat/string-eq, shifts, add, mul
         case TT::opOR:                     return 20;    // ||
         case TT::opAND:                    return 30;    // &&
         case TT::opEQ: case TT::opNE:     return 60;     // == !=
         case TT::opLE: case TT::opGE:     return 70;     // <= >= (same as < >)
         case TT::opCONCAT:                return 75;     // @ / NL/TAB/SPC glue
         case TT::opSTREQ: case TT::opSTRNE:return 75;    // $= !$=
         case TT::opSHL: case TT::opSHR:   return 80;     // << >>
         case TT::opINTNAME:               return 135;    // ->   (higher than '.')
         case TT::opINTNAMER:              return 135;    // -->  (higher than '.')
            
         case TT::opCHAR: {
            switch (t.asChar()) {
               case '=': return 10; // plain '='
               case '?': return 15; // Ternary ?: (handled in led('?'); keep a little above assignment)
               case '|': return 40;           // bitwise |
               case '^': return 45;           // bitwise ^
               case '&': return 50;           // bitwise &
               case '<':
               case '>': return 70;           // < >
               case '+':
               case '-': return 90;           // + -
               case '*':
               case '/':
               case '%': return 100;          // * / %
               case '.': return 130;          // member access .
               case '[': return 140;          // postfix indexing [  ]  (highest)
               case ':': return 0;            // allow for ":" after expression
               default: break;
            }
         }
            
         default: return 0;
      }
   }
   
   Assoc associativity(const TOK& t) const
   {
      // Right-assoc operators:
      if ((t.kind == TT::opCHAR && (t.asChar() == '=' || t.asChar() == '?')) ||
          t.kind == TT::opPLASN || t.kind == TT::opMIASN || t.kind == TT::opMLASN ||
          t.kind == TT::opDVASN || t.kind == TT::opMODASN || t.kind == TT::opANDASN ||
          t.kind == TT::opXORASN || t.kind == TT::opORASN  || t.kind == TT::opSLASN  ||
          t.kind == TT::opSRASN)
         return RIGHT;
      
      // Everything else is left-assoc
      return LEFT;
   }
   
   // Parse expression with right-binding power 'rbp'
   ExprNode* parseExpression(int rbp)
   {
      // prefix / primary
      TOK t = mTokens[mTokenPos++];
      ExprNode* left = nud(t);
      
      // infix / postfix loop
      for (;;)
      {
         const TOK& next = LA();
         int bp = lbp(next);
         if (bp <= rbp)
            break;
         
         TOK op = mTokens[mTokenPos++];
         left = led(op, left, bp);
      }
      
      return left;
   }

   ExprNode* parseExpressionFrom(ExprNode* left, int rbp = 0)
   {
       for (;;)
       {
           const TOK& next = LA();
           int bp = lbp(next);
           if (bp <= rbp)
               break;

           TOK op = mTokens[mTokenPos++];
           left = led(op, left, bp);
       }
       return left;
   }
   
   // Assignments (right-assoc). Only supported to VAR targets here.
   inline ExprNode* parseAssignRHS(int bp)
   {
      return parseExpression(bp - 1);
   }
   
   // Compound assigns map to AssignOpExprNode; '=' to AssignExprNode.
   // NOTE: to keep things simple, this DOES NOT factor in types; 
   // they are not allowed within expressions 
   // (besides the start which is handled in parseStmtNode).
   ExprNode* makeAssign(const TOK& tok, ExprNode* l, ExprNode* r)
   {
      if (VarNode* v = dynamic_cast<VarNode*>(l))
      {
         if (tok.kind == TT::opCHAR && tok.asChar() == '=')
         {
            return AssignExprNode::alloc(mResources, tok.pos.line, v->varName, v->arrayIndex, r);            // =
         }
         // all op*ASN kinds go through AssignOpExprNode with tok.kind payload
         return AssignOpExprNode::alloc(mResources, tok.pos.line, v->varName, v->arrayIndex, r, processCharOp(TOK(tok)));
      }
      if (SlotAccessNode* s = dynamic_cast<SlotAccessNode*>(l))
      {
         if (tok.kind == TT::opCHAR && tok.asChar() == '=')
         {
            return SlotAssignNode::alloc(mResources, tok.pos.line, s->objectExpr, s->arrayExpr, s->slotName, r);
         }
         return SlotAssignOpNode::alloc(mResources, tok.pos.line, s->objectExpr, s->slotName, s->arrayExpr, processCharOp(TOK(tok)), r);
      }
      errorHere(tok, "left-hand side of assignment must be a variable");
      return NULL;
   }
   
   // Handles postfix expressions
   //
   ExprNode* led(const TOK& op, ExprNode* left, int opBP)
   {
      switch (op.kind)
      {
         case TT::opCHAR: {
            
            // Postfix indexing: [...]  (highest precedence)
            if (op.asChar() == '[')
            {
               ExprNode* idx = parseAidxExprNode();
               expectChar(']', "] expected");
               
               if (VarNode* v = dynamic_cast<VarNode*>(left)) {
                  if (v->arrayIndex)
                     v->arrayIndex = CommaCatExprNode::alloc(mResources, op.pos.line, v->arrayIndex, idx);
                  else
                     v->arrayIndex = idx;
                  return v;
               }
               // Generic slot/object indexing not implemented here.
               errorHere(op, "indexing allowed only on variables at this point");
               return NULL;
            }
            
            // Ternary ?:  (right-assoc)
            if (op.asChar() == '?')
            {
               ExprNode* mid = parseExpression(0);
               expectChar(':', ": expected");
               ExprNode* rhs = parseExpression(opBP - 1);
               return ConditionalExprNode::alloc(mResources, op.pos.line, left, mid, rhs);
            }
            
            if (op.asChar() == '=') // '='
            {
               // Slot assignment?
               if (SlotAccessNode* s = dynamic_cast<SlotAccessNode*>(left))
               {
                  // Special brace form: slot = { a, b, c }
                  if (LA().kind == TT::opCHAR && LA().asChar() == '{')
                  {
                     mTokenPos++; // consume '{'
                     ExprNode* list = parseExprListOptUntil('}');
                     expectChar('}', "'}' expected");
                     return SlotAssignNode::alloc(mResources, op.pos.line, s->objectExpr, s->arrayExpr, s->slotName, list);
                  }
                  
                  // Normal RHS
                  ExprNode* rhs = parseAssignRHS(opBP);
                  return SlotAssignNode::alloc(mResources, op.pos.line, s->objectExpr, s->arrayExpr, s->slotName, rhs);
               }
               
               return makeAssign(op, left, parseAssignRHS(opBP));
            }
            else if (op.asChar() == '.') // Member access '.'  -> SlotAccessNode(left, array?, IDENT)
            {
               // IDENT after '.'
               TOK id = expect(TT::IDENT, "identifier expected after '.'");
               
               // Method call: .IDENT '(' ... ')'
               if (LA().kind == TT::opCHAR && LA().asChar() == '(')
               {
                  expectChar('(', "'(' expected");
                  ExprNode* argsTail = parseExprListOptUntil(')');   // next-CHAIN list (or null)
                  expectChar(')', "')' expected");
                  
                  // Build arg chain: [objectExpr] -> argsTail
                  ExprNode* argHead = left;
                  if (argsTail) argHead->append(argsTail);
                  
                  // Use the object's dbg line if available, else token line
                  S32 ln = left && left->dbgLineNumber ? left->dbgLineNumber : (S32)id.pos.line;
                  return FuncCallExprNode::alloc(mResources, ln, id.stString, /*nameSpace*/0, argHead, /*dot*/true);
               }
               
               // Slot access: .IDENT [ '[' aidx ']' ]   -> SlotAccessNode
               ExprNode* arr = NULL;
               if (matchChar('[')) {
                  arr = parseAidxExprNode();
                  expectChar(']', "] expected");
               }
               return SlotAccessNode::alloc(mResources, op.pos.line, left, arr, id.stString);
            }
            
            // Ordinary binary ops
            {
               ExprNode* right = parseExpression(associativity(op) == LEFT ? opBP : (opBP-1));
               switch (op.asChar())
               {
                     // Single-char arithmetic etc.
                  case '+': case '-': case '*': case '/':
                     return FloatBinaryExprNode::alloc(mResources, op.pos.line, processCharOp(op), left, right);
                  case '%': case '^': case '&': case '|': case '<': case '>':
                     return IntBinaryExprNode::alloc(mResources, op.pos.line, processCharOp(op), left, right);
                  default:
                     errorHere(op, "unsupported operator in expression");
                     break;
               }
            }
         }
            
            break;
            
            // op*ASN
         case TT::opPLASN: case TT::opMIASN: case TT::opMLASN: case TT::opDVASN:
         case TT::opMODASN: case TT::opANDASN: case TT::opXORASN: case TT::opORASN:
         case TT::opSLASN:  case TT::opSRASN:
            return makeAssign(op, left, parseAssignRHS(opBP));
            
         case TT::opPLUSPLUS:
         case TT::opMINUSMINUS:
         {
            if (VarNode* v = dynamic_cast<VarNode*>(left))
            {
               ExprNode* one = FloatNode::alloc(mResources, op.pos.line, 1);
               TT asn = (op.kind == TT::opPLUSPLUS) ? TT::opPCHAR_PLUS : TT::opPCHAR_MINUS;
               return AssignOpExprNode::alloc(mResources, op.pos.line, v->varName, v->arrayIndex, one, asn);
            }
            else if (SlotAccessNode* s = dynamic_cast<SlotAccessNode*>(left))
            {
               ExprNode* one = FloatNode::alloc(mResources, op.pos.line, 1);
               TT asn = (op.kind == TT::opPLUSPLUS) ? TT::opPCHAR_PLUS : TT::opPCHAR_MINUS;
               return SlotAssignOpNode::alloc(mResources, op.pos.line, s->objectExpr, s->slotName, s->arrayExpr, asn, one);
            }
            errorHere(op, "postfix ++/-- requires a variable");
            return NULL;
         }
            
            // Logical / bitwise / arithmetic / shift / eq / rel / concat family
         case TT::opLE: case TT::opGE: case TT::opEQ: case TT::opNE:
         case TT::opOR: case TT::opAND:
         case TT::opSHL: case TT::opSHR:
         {
            ExprNode* right = parseExpression(associativity(op)==LEFT ? opBP : (opBP-1));
            return IntBinaryExprNode::alloc(mResources, op.pos.line, processCharOp(op.kind), left, right);
         }
            
         case TT::opSTREQ:
         case TT::opSTRNE:
         {
            ExprNode* right = parseExpression(associativity(op)==LEFT ? opBP : (opBP-1));
            return StreqExprNode::alloc(mResources, op.pos.line, left, right, op.kind==TT::opSTREQ);
         }
            
         case TT::opCONCAT:
         {
            ExprNode* right = parseExpression(associativity(op)==LEFT ? opBP : (opBP-1));
            char glue = (char)op.ivalue;
            return StrcatExprNode::alloc(mResources, op.pos.line, left, right, glue);
         }
            
            // Internal slot access: -> (opINTNAME) and --> (opINTNAMER)
         case TT::opINTNAME:
         case TT::opINTNAMER:
         {
            bool recurse = (op.kind == TT::opINTNAMER);
            // tight parse for the "slot expression" on the right
            ExprNode* slotExpr = parseExpression( (int)130 ); // bind tighter than '.'
            return InternalSlotAccessNode::alloc(mResources, op.pos.line, left, slotExpr, recurse);
         }
            
         default:
            break;
      }
      
      errorHere(op, "unsupported operator in expression");
      return NULL;
   }
   
   // Handles prefix expressions
   //
   ExprNode* nud(const TOK& t)
   {
      switch (t.kind)
      {
            // Literals
         case TT::INTCONST:   return IntNode::alloc(mResources, t.pos.line, (S32)t.ivalue);
         case TT::FLTCONST:   return FloatNode::alloc(mResources, t.pos.line, t.value);
         case TT::STRATOM:    return StrConstNode::alloc(mResources, t.pos.line, mTokenizer->bufferAtOffset(t.stringValue.offset), false);
         case TT::TAGATOM:    return StrConstNode::alloc(mResources, t.pos.line, mTokenizer->bufferAtOffset(t.stringValue.offset), true);
         case TT::DOCBLOCK:   return StrConstNode::alloc(mResources, t.pos.line, mTokenizer->bufferAtOffset(t.stringValue.offset), false, true, t.stringValue.len);
            
         case TT::opPLUSPLUS:
         case TT::opMINUSMINUS:
         {
            //ExprNode* target = parseExpression(110);
            errorHere(t, "prefix ++/-- not supported");
            return NULL;
         }
            
            // Names
         case TT::IDENT: {
            
            // namespace::func( ... )
            if (LA().kind == TT::opCOLONCOLON)
            {
               TOK nsTok = t;                // first IDENT = namespace
               mTokenPos++;                   // consume '::'
               TOK fnTok = expect(TT::IDENT, "identifier expected after '::'");
               expectChar('(', "'(' expected");
               ExprNode* args = parseExprListOptUntil(')');
               expectChar(')', "')' expected");
               return FuncCallExprNode::alloc(mResources, nsTok.pos.line, fnTok.stString, nsTok.stString, args, /*dot*/false);
            }
            
            // func( ... )
            if (LA().kind == TT::opCHAR && LA().asChar() == '(')
            {
               expectChar('(', "'(' expected");
               ExprNode* args = parseExprListOptUntil(')');
               expectChar(')', "')' expected");
               return FuncCallExprNode::alloc(mResources, t.pos.line, t.stString, /*nameSpace*/0, args, /*dot*/false);
            }
            
            // bare name
            return ConstantNode::alloc(mResources, t.pos.line, t.stString);
         }
         case TT::VAR:        return VarNode::alloc(mResources, t.pos.line, t.stString, NULL);
            
         case TT::rwDECLARE:           // 'new'
         case TT::rwDECLARESINGLETON:  // 'singleton'
            return parseObjectDecl(true);
            
            // Prefix operators and grouping
         case TT::opCHAR:
         {
            char ch = (char)t.ivalue;
            switch (ch)
            {
               case '(':
               {
                  ExprNode* e = parseExpression(0);
                  expectChar(')', ") expected");
                  return e;
               }
               case '-':  return FloatUnaryExprNode::alloc(mResources, t.pos.line, TT::opPCHAR_MINUS, parseExpression(110)); // bind tighter than *,/,%  (any high > 100)
               case '!':  return IntUnaryExprNode::alloc(mResources, t.pos.line,   TT::opPCHAR_EXCL, parseExpression(110));
               case '~':  return IntUnaryExprNode::alloc(mResources, t.pos.line,   TT::opPCHAR_TILDE, parseExpression(110));
               case '*':  return TTagDerefNode::alloc(mResources, t.pos.line,          parseExpression(110));
            }
            break;
         }
            
         default:
            break;
      }
      
      errorHere(t, "unexpected token in expression");
      return NULL;
   }
};

}

