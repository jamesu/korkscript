#include <vector>
#include <stdexcept>
#include <string>
#include "core/stringTable.h"

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
   
   ASTGen(SimpleLexer::Tokenizer* tok)
   : mTokenizer(tok), mTokenPos(0)
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
   
   TOK expect(TT t, const char* what)
   {
      if (LA().kind != t)
      {
         throw TokenError(LA(), t, what);
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
   
   inline int toBisonOpTok(const SimpleLexer::Token& t)
   {
      if (t.kind == TT::opCHAR)
      {
         return t.asChar();
      }
      
      switch (t.kind)
      {
            // ---- comparison / logic ----
         case TT::opEQ:         return 317;
         case TT::opNE:         return 318;
         case TT::opGE:         return 319;
         case TT::opLE:         return 320;
         case TT::opAND:        return 321;  // &&
         case TT::opOR:         return 322;  // ||
            
            // ---- namespace & internal-slot ----
         case TT::opCOLONCOLON: return 324;  // ::
         case TT::opINTNAME:    return 299;  // ->
         case TT::opINTNAMER:   return 300;  // -->
            
            // ---- inc/dec ----
         case TT::opMINUSMINUS: return 301;  // --
         case TT::opPLUSPLUS:   return 302;  // ++
            
            // ---- string/concat ----
         case TT::opSTREQ:      return 323;  // $=
         case TT::opSTRNE:      return 328;  // !$=
            
            // ---- shifts ----
         case TT::opSHL:        return 304;  // <<
         case TT::opSHR:        return 305;  // >>
            
            // ---- compound assigns ----
         case TT::opPLASN:      return '+';  // +=
         case TT::opMIASN:      return '-';  // -=
         case TT::opMLASN:      return '*';  // *=
         case TT::opDVASN:      return '/';  // /=
         case TT::opMODASN:     return '%';  // %=
         case TT::opANDASN:     return '&';  // &=
         case TT::opXORASN:     return '^';  // ^=
         case TT::opORASN:      return '|';  // |=
         case TT::opSLASN:      return 304;  // <<=
         case TT::opSRASN:      return 305;  // >>=
            
         default:
            return -1;
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
      VarNode* head = VarNode::alloc(v.pos.line, v.stString, NULL);
      VarNode* tail = head;
      
      // Subsequent vars
      while (matchChar(','))
      {
         TOK t = expect(TT::VAR, "parameter name expected");
         VarNode* nxt = VarNode::alloc(t.pos.line, t.stString, NULL);
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
      return IfStmtNode::alloc(ifTok.pos.line, cond, thenS, elseS, false); // propagate=false
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
         return LoopStmtNode::alloc(wTok.pos.line, NULL, test, NULL, body, false);
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
         return LoopStmtNode::alloc(dTok.pos.line, NULL, test, NULL, body, true);
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
      if (!test) test = IntNode::alloc(fTok.pos.line, 1);
      return LoopStmtNode::alloc(fTok.pos.line, init, test, end, body, false);
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
      return IterStmtNode::alloc(t.pos.line, v.stString, cont, body, isStr);
   }
   
   // Emits either a string or int equality node
   //
   ExprNode* emitEqNode(S32 line, ExprNode* left, ExprNode* right, bool string)
   {
      if (string)
      {
         return StreqExprNode::alloc(line, left, right, true);
      }
      else
      {
         return IntBinaryExprNode::alloc(line, toBisonOpTok(TT::opEQ), left, right);
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
         return IfStmtNode::alloc(caseTok.pos.line, list, body, defBody, false);
      }
      
      if (LA().kind == TT::rwCASE)
      {
         // CASE ... ':' stmts case_block
         IfStmtNode* rest = parseCaseBlock();
         return IfStmtNode::alloc(caseTok.pos.line, list, body, rest, true);
      }
      
      // CASE ... ':' stmts
      return IfStmtNode::alloc(caseTok.pos.line, list, body, NULL, false);
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
      root->propagateSwitchExpr(selector, isString);
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
      // We conservatively say "yes" if after one (or two) idents we find '=' or '['.
      const TOK& n1 = LA(1);
      if (n1.kind == K::opCHAR && (n1.asChar() == '=' || n1.asChar() == '[')) return true;
      
      // typed form: IDENT IDENT <'='|'['> ...
      if (n1.kind == K::IDENT) {
         const TOK& n2 = LA(2);
         if (n2.kind == K::opCHAR && (n2.asChar() == '=' || n2.asChar() == '[')) return true;
      }
      return false;
   }
   
   // Handles obj.foo = expr;
   //
   //
   SlotAssignNode* parseSlotAssign(ExprNode* object)
   {
      using K = TT;
      
      S32 line = LA().pos.line;
      U32 typeID = (U32)-1;
      StringTableEntry slotName = 0;
      ExprNode* aidx = NULL;
      
      // Special case: datablock is a keyword
      if (LA().kind == K::rwDATABLOCK)
      {
         // rwDATABLOCK '=' expr ';'
         TOK kw = mTokens[mTokenPos++];  // 'datablock'
         expectChar('=', "= expected");
         ExprNode* rhs = parseExprNode();
         expectChar(';', "; expected");
         
         slotName = mTokenizer->mStringTable->insert("datablock");
         return SlotAssignNode::alloc(line, object, aidx, slotName, rhs, typeID);
      }
      
      // IDENT ... (maybe typed)
      // First token could be either TYPEIDENT (treated as IDENT by our lexer) or the slot name.
      TOK first = expect(K::IDENT, "identifier expected");
      
      // NOTE: for now ignoring typed fields
      slotName = first.stString;
      
      // Optional '[' aidx_expr ']'
      if (matchChar('['))
      {
         aidx = parseAidxExprNode();  // you already have this
         expectChar(']', "] expected");
      }
      
      // '=' expr ';'
      expectChar('=', "= expected");
      ExprNode* rhs = parseExprNode();
      expectChar(';', "; expected");
      
      return SlotAssignNode::alloc(line, object, aidx, slotName, rhs, typeID);
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
         objectNameExpr = StrConstNode::alloc(line, empty, false);
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
      
      return ObjectDeclNode::alloc(line, klassName, objectNameExpr, argList,
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
         return ConstantNode::alloc(identName.pos.line, identName.stString);
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
      
      return ObjectDeclNode::alloc(line, klassNameNode, nameExpr, NULL, parentObject, slotAssignNode, NULL, /*isDatablock*/true, /*isSingleton*/false, /*isNewExpr*/false);
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
      
      // { ... }
      StmtNode* body = parseBlockStmt();
      
      return FunctionDeclStmtNode::alloc(line ? line : a.pos.line, fn, ns, args, body);
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
            return BreakStmtNode::alloc((S32)tok.pos.line);
         }
         case TT::rwCONTINUE: {
            TOK tok = mTokens[mTokenPos++]; expectChar(';', "; expected");
            return ContinueStmtNode::alloc((S32)tok.pos.line);
         }
         case TT::rwRETURN: {
            TOK tok = mTokens[mTokenPos++];
            if (!matchChar(';'))
            {
               ExprNode*  e = parseExprNode();
               expectChar(';', "; expected");
               return ReturnStmtNode::alloc((S32)tok.pos.line, e);
            }
            return ReturnStmtNode::alloc((S32)tok.pos.line, NULL);
         }
         case TT::rwASSERT:
            return parseAssertStmt();
            
         case TT::DOCBLOCK: { // also valid inside blocks
            TOK tok = mTokens[mTokenPos++];
            return StrConstNode::alloc(tok.pos.line, mTokenizer->bufferAtOffset(tok.stringValue.offset), false, true, tok.stringValue.len);
         }
         default: {
            // expression_stmt ';'
            ExprNode*  e = parseStmtNodeExprNode();
            expectChar(';', "; expected");
            return e;
         }
      }
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
      
      return AssertCallExprNode::alloc(kw.pos.line, cond, msg);
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
         ExprNode* catOp = CommaCatExprNode::alloc(lineNo, head, next);
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
   
   // Assignments (right-assoc). Only supported to VAR targets here.
   inline ExprNode* parseAssignRHS(int bp)
   {
      return parseExpression(bp - 1);
   }
   
   // Compound assigns map to AssignOpExprNode; '=' to AssignExprNode.
   ExprNode* makeAssign(const TOK& tok, ExprNode* l, ExprNode* r)
   {
      if (VarNode* v = dynamic_cast<VarNode*>(l))
      {
         if (tok.kind == TT::opCHAR && tok.asChar() == '=')
         {
            return AssignExprNode::alloc(tok.pos.line, v->varName, v->arrayIndex, r);            // =
         }
         // all op*ASN kinds go through AssignOpExprNode with tok.kind payload
         return AssignOpExprNode::alloc(tok.pos.line, v->varName, v->arrayIndex, r, toBisonOpTok(TOK(tok)));
      }
      if (SlotAccessNode* s = dynamic_cast<SlotAccessNode*>(l))
      {
         if (tok.kind == TT::opCHAR && tok.asChar() == '=')
         {
            return SlotAssignNode::alloc(tok.pos.line, s->objectExpr, s->arrayExpr, s->slotName, r);
         }
         return SlotAssignOpNode::alloc(tok.pos.line, s->objectExpr, s->slotName, s->arrayExpr, toBisonOpTok(TOK(tok)), r);
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
                     v->arrayIndex = CommaCatExprNode::alloc(op.pos.line, v->arrayIndex, idx);
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
               return ConditionalExprNode::alloc(op.pos.line, left, mid, rhs);
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
                     return SlotAssignNode::alloc(op.pos.line, s->objectExpr, s->arrayExpr, s->slotName, list);
                  }
                  
                  // Normal RHS
                  ExprNode* rhs = parseAssignRHS(opBP);
                  return SlotAssignNode::alloc(op.pos.line, s->objectExpr, s->arrayExpr, s->slotName, rhs);
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
                  return FuncCallExprNode::alloc(ln, id.stString, /*nameSpace*/0, argHead, /*dot*/true);
               }
               
               // Slot access: .IDENT [ '[' aidx ']' ]   -> SlotAccessNode
               ExprNode* arr = NULL;
               if (matchChar('[')) {
                  arr = parseAidxExprNode();
                  expectChar(']', "] expected");
               }
               return SlotAccessNode::alloc(op.pos.line, left, arr, id.stString);
            }
            
            // Ordinary binary ops
            {
               ExprNode* right = parseExpression(associativity(op) == LEFT ? opBP : (opBP-1));
               switch (op.asChar())
               {
                     // Single-char arithmetic etc.
                  case '+': case '-': case '*': case '/':
                     return FloatBinaryExprNode::alloc(op.pos.line, op.asChar(), left, right);
                  case '%': case '^': case '&': case '|': case '<': case '>':
                     return IntBinaryExprNode::alloc(op.pos.line, op.asChar(), left, right);
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
               ExprNode* one = FloatNode::alloc(op.pos.line, 1);
               int asn = (op.kind == TT::opPLUSPLUS) ? '+' : '-';
               return AssignOpExprNode::alloc(op.pos.line, v->varName, v->arrayIndex, one, asn);
            }
            else if (SlotAccessNode* s = dynamic_cast<SlotAccessNode*>(left))
            {
               ExprNode* one = FloatNode::alloc(op.pos.line, 1);
               int asn = (op.kind == TT::opPLUSPLUS) ? '+' : '-';
               return SlotAssignOpNode::alloc(op.pos.line, s->objectExpr, s->slotName, s->arrayExpr, asn, one);
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
            return IntBinaryExprNode::alloc(op.pos.line, toBisonOpTok(op.kind), left, right);
         }
            
         case TT::opSTREQ:
         case TT::opSTRNE:
         {
            ExprNode* right = parseExpression(associativity(op)==LEFT ? opBP : (opBP-1));
            return StreqExprNode::alloc(op.pos.line, left, right, op.kind==TT::opSTREQ);
         }
            
         case TT::opCONCAT:
         {
            ExprNode* right = parseExpression(associativity(op)==LEFT ? opBP : (opBP-1));
            char glue = (char)op.ivalue;
            return StrcatExprNode::alloc(op.pos.line, left, right, glue);
         }
            
            // Internal slot access: -> (opINTNAME) and --> (opINTNAMER)
         case TT::opINTNAME:
         case TT::opINTNAMER:
         {
            bool recurse = (op.kind == TT::opINTNAMER);
            // tight parse for the "slot expression" on the right
            ExprNode* slotExpr = parseExpression( (int)130 ); // bind tighter than '.'
            return InternalSlotAccessNode::alloc(op.pos.line, left, slotExpr, recurse);
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
         case TT::INTCONST:   return IntNode::alloc(t.pos.line, (S32)t.ivalue);
         case TT::FLTCONST:   return FloatNode::alloc(t.pos.line, t.value);
         case TT::STRATOM:    return StrConstNode::alloc(t.pos.line, mTokenizer->bufferAtOffset(t.stringValue.offset), false);
         case TT::TAGATOM:    return StrConstNode::alloc(t.pos.line, mTokenizer->bufferAtOffset(t.stringValue.offset), true);
         case TT::DOCBLOCK:   return StrConstNode::alloc(t.pos.line, mTokenizer->bufferAtOffset(t.stringValue.offset), false, true, t.stringValue.len);
            
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
               return FuncCallExprNode::alloc(nsTok.pos.line, fnTok.stString, nsTok.stString, args, /*dot*/false);
            }
            
            // func( ... )
            if (LA().kind == TT::opCHAR && LA().asChar() == '(')
            {
               expectChar('(', "'(' expected");
               ExprNode* args = parseExprListOptUntil(')');
               expectChar(')', "')' expected");
               return FuncCallExprNode::alloc(t.pos.line, t.stString, /*nameSpace*/0, args, /*dot*/false);
            }
            
            // bare name
            return ConstantNode::alloc(t.pos.line, t.stString);
         }
         case TT::VAR:        return VarNode::alloc(t.pos.line, t.stString, NULL);
            
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
               case '-':  return FloatUnaryExprNode::alloc(t.pos.line, '-', parseExpression(110)); // bind tighter than *,/,%  (any high > 100)
               case '!':  return IntUnaryExprNode::alloc(t.pos.line,   '!', parseExpression(110));
               case '~':  return IntUnaryExprNode::alloc(t.pos.line,   '~', parseExpression(110));
               case '*':  return TTagDerefNode::alloc(t.pos.line,          parseExpression(110));
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

