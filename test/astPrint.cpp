#include "platform/platform.h"
#include "console/simpleLexer.h"
#include "console/ast.h"
#include "console/compiler.h"
#include "console/simpleParser.h"
#include "core/fileStream.h"
#include <stdio.h>
#include "embed/api.h"
#include "embed/internalApi.h"

/*
 
 Prints AST nodes for script
 
 */

bool gPrintBytecode = false;
bool gEnableExtensions = false;

void MyLogger(U32 level, const char *consoleLine, void* userPtr)
{
   printf("%s\n", consoleLine);
}

const char* opToStr(S32 op)
{
    switch ((SimpleLexer::TokenType)op)
    {
        case SimpleLexer::TokenType::opPCHAR_CARET:     return "^";
        case SimpleLexer::TokenType::opPCHAR_PERCENT:   return "%";
        case SimpleLexer::TokenType::opPCHAR_AMPERSAND: return "&";
        case SimpleLexer::TokenType::opPCHAR_PIPE:      return "|";
        case SimpleLexer::TokenType::opPCHAR_LESS:      return "<";
        case SimpleLexer::TokenType::opPCHAR_GREATER:   return ">";
        case SimpleLexer::TokenType::opPCHAR_PLUS:      return "+";
        case SimpleLexer::TokenType::opPCHAR_MINUS:     return "-";
        case SimpleLexer::TokenType::opPCHAR_ASTERISK:  return "*";
        case SimpleLexer::TokenType::opPCHAR_SLASH:     return "/";

        case SimpleLexer::TokenType::opPCHAR_EXCL:      return "!";
        case SimpleLexer::TokenType::opPCHAR_TILDE:     return "~";
        case SimpleLexer::TokenType::opCONCAT:      return "opCONCAT";
        case SimpleLexer::TokenType::opCOLONCOLON:  return "opCOLONCOLON";
        case SimpleLexer::TokenType::opMINUSMINUS:  return "opMINUSMINUS";
        case SimpleLexer::TokenType::opPLUSPLUS:    return "opPLUSPLUS";
        case SimpleLexer::TokenType::opSTREQ:   return "opSTREQ";
        case SimpleLexer::TokenType::opSTRNE:   return "opSTRNE";
        case SimpleLexer::TokenType::opPLASN:   return "opPLASN";
        case SimpleLexer::TokenType::opMIASN:   return "opMIASN";
        case SimpleLexer::TokenType::opMLASN:   return "opMLASN";
        case SimpleLexer::TokenType::opDVASN:   return "opDVASN";
        case SimpleLexer::TokenType::opMODASN:  return "opMODASN";
        case SimpleLexer::TokenType::opANDASN:  return "opANDASN";
        case SimpleLexer::TokenType::opXORASN:  return "opXORASN";
        case SimpleLexer::TokenType::opORASN:   return "opORASN";
        case SimpleLexer::TokenType::opSLASN:   return "opSLASN";
        case SimpleLexer::TokenType::opSRASN:   return "opSRASN";
        case SimpleLexer::TokenType::opINTNAME: return "opINTNAME";
        case SimpleLexer::TokenType::opINTNAMER:return "opINTNAMER";


        case SimpleLexer::TokenType::opGE:   return "opGE";
        case SimpleLexer::TokenType::opLE:   return "opLE";
        case SimpleLexer::TokenType::opEQ:   return "opEQ";
        case SimpleLexer::TokenType::opNE:   return "opNE";
        case SimpleLexer::TokenType::opOR:   return "opOR";
        case SimpleLexer::TokenType::opAND:  return "opAND";
        case SimpleLexer::TokenType::opSHR:  return "opSHR";
        case SimpleLexer::TokenType::opSHL:  return "opSHL";

        case SimpleLexer::TokenType::END: return "<NOT SET>";

        default:
            return "<!UNKNOWN!>";
    }
}

namespace astprint {

static inline void indent(int n) { while (n--) std::fputc(' ', stdout); }

static inline const char* yesno(bool b) { return b ? "true" : "false"; }

static inline const char* show(const char* s)
{
    if (!s) return "null";

    static std::string buf;
    buf.clear();
    buf.reserve(strlen(s));

    for (const char* p = s; *p; ++p)
        buf.push_back(std::tolower((unsigned char)*p));

    return buf.c_str();
}
static void printNode(const StmtNode* n, int pad = 0);

template <class T>
static void printChild(const char* key, const T* child, int pad) {
   indent(pad); printf("%s = ", key);
   if (!child) { puts("null"); return; }
   
   // If we are NOT an expression, print everything on the same level and dont recurse
   if (dynamic_cast<const ExprNode*>(child) == NULL) {
      if (child->next) {
         puts("{");
         const StmtNode* it = child;
         while (it) {
            printNode(it, pad + 2);
            it = it->next;
            if (it) puts("");
         }
         indent(pad);
         puts("}");
         return;
      }
   }
   
   printNode(child, pad);
}

template <class T>
static void printList(const char* key, const T* head, int pad) {
   if (!head) { indent(pad); printf("%s = null\n", key); return; }
   indent(pad); printf("%s = {\n", key);
   const StmtNode* it = head;
   while (it) {
      printNode(it, pad + 2);
      it = it->next;
      if (it) puts("");
   }
   indent(pad); puts("}");
}

static const char* typeReqName(TypeReq t) {
   switch (t) {
      case TypeReqNone:   return "TypeReqNone";
      case TypeReqUInt:   return "TypeReqUInt";
      case TypeReqFloat:  return "TypeReqFloat";
      case TypeReqString: return "TypeReqString";
      case TypeReqVar:    return "TypeReqVar";
      default:            return "TypeReq(?)";
   }
}

static void open(const char* klass, int pad) {
   indent(pad); printf("%s {\n", klass);
}

static void close(const StmtNode* n, int pad) {
   indent(pad); puts("}");
}

static void printNode(const StmtNode* n, int pad) {
   if (!n) { indent(pad); puts("null"); return; }
   
   if (auto x = dynamic_cast<const BreakStmtNode*>(n)) {
      open("BreakStmtNode", pad);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const ContinueStmtNode*>(n)) {
      open("ContinueStmtNode", pad);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const ReturnStmtNode*>(n)) {
      open("ReturnStmtNode", pad);
      printChild("expr", x->expr, pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const IfStmtNode*>(n)) {
      open("IfStmtNode", pad);
      printChild("testExpr", x->testExpr, pad + 2);
      printChild("ifBlock",  x->ifBlock,  pad + 2);
      printChild("elseBlock",x->elseBlock,pad + 2);
      indent(pad + 2); printf("integer = %s\n",   yesno(x->integer));
      indent(pad + 2); printf("propagate = %s\n", yesno(x->propagate));
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const LoopStmtNode*>(n)) {
      open("LoopStmtNode", pad);
      printChild("testExpr",   x->testExpr,   pad + 2);
      printChild("initExpr",   x->initExpr,   pad + 2);
      printChild("endLoopExpr",x->endLoopExpr,pad + 2);
      printChild("loopBlock",  x->loopBlock,  pad + 2);
      indent(pad + 2); printf("isDoLoop = %s\n", yesno(x->isDoLoop));
      indent(pad + 2); printf("integer = %s\n",  yesno(x->integer));
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const IterStmtNode*>(n)) {
      open("IterStmtNode", pad);
      indent(pad + 2); printf("varName = \"%s\"\n", show(x->varName));
      printChild("containerExpr", x->containerExpr, pad + 2);
      printChild("body",          x->body,          pad + 2);
      indent(pad + 2); printf("isStringIter = %s\n", yesno(x->isStringIter));
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const TTagSetStmtNode*>(n)) {
      open("TTagSetStmtNode", pad);
      indent(pad + 2); printf("tag = \"%s\"\n", show(x->tag));
      printChild("valueExpr",  x->valueExpr,  pad + 2);
      printChild("stringExpr", x->stringExpr, pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const FunctionDeclStmtNode*>(n)) {
      open("FunctionDeclStmtNode", pad);
      indent(pad + 2); printf("fnName = \"%s\"\n",    show(x->fnName));
      indent(pad + 2); printf("nameSpace = \"%s\"\n", show(x->nameSpace));
      indent(pad + 2); printf("package = \"%s\"\n",   show(x->package));
      indent(pad + 2); printf("argc = %i\n", x->argc);
      printList("args",  x->args,  pad + 2);
      printList("stmts", x->stmts, pad + 2);
      close(n, pad);
      return;
   }
   
   
   if (auto x = dynamic_cast<const ConditionalExprNode*>(n)) {
      open("ConditionalExprNode", pad);
      printChild("testExpr",  x->testExpr,  pad + 2);
      printChild("trueExpr",  x->trueExpr,  pad + 2);
      printChild("falseExpr", x->falseExpr, pad + 2);
      indent(pad + 2); printf("integer = %s\n", yesno(x->integer));
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const FloatBinaryExprNode*>(n)) {
      open("FloatBinaryExprNode", pad);
      indent(pad + 2); printf("op = %s\n", opToStr((S32)x->op));
      printChild("left",  x->left,  pad + 2);
      printChild("right", x->right, pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const IntBinaryExprNode*>(n)) {
      open("IntBinaryExprNode", pad);
      indent(pad + 2); printf("op = %s\n", opToStr((S32)x->op));
      indent(pad + 2); printf("subType = %s\n", typeReqName(x->subType));
      indent(pad + 2); printf("operand = %s\n", opToStr((S32)x->operand));
      printChild("left",  x->left,  pad + 2);
      printChild("right", x->right, pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const StreqExprNode*>(n)) {
      open("StreqExprNode", pad);
      indent(pad + 2); printf("eq = %s\n", yesno(x->eq));
      printChild("left",  x->left,  pad + 2);
      printChild("right", x->right, pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const StrcatExprNode*>(n)) {
      open("StrcatExprNode", pad);
      indent(pad + 2); printf("appendChar = %d\n", x->appendChar);
      printChild("left",  x->left,  pad + 2);
      printChild("right", x->right, pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const CommaCatExprNode*>(n)) {
      open("CommaCatExprNode", pad);
      printChild("left",  x->left,  pad + 2);
      printChild("right", x->right, pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const IntUnaryExprNode*>(n)) {
      open("IntUnaryExprNode", pad);
      indent(pad + 2); printf("op = %s\n", opToStr((S32)x->op));
      indent(pad + 2); printf("integer = %s\n", yesno(x->integer));
      printChild("expr", x->expr, pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const FloatUnaryExprNode*>(n)) {
      open("FloatUnaryExprNode", pad);
      indent(pad + 2); printf("op = %s\n", opToStr((S32)x->op));
      printChild("expr", x->expr, pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const VarNode*>(n)) {
      open("VarNode", pad);
      indent(pad + 2); printf("varName = \"%s\"\n", show(x->varName));
      printChild("arrayIndex", x->arrayIndex, pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const IntNode*>(n)) {
      open("IntNode", pad);
      indent(pad + 2); printf("value = %d\n", x->value);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const FloatNode*>(n)) {
      open("FloatNode", pad);
      indent(pad + 2); printf("value = %g\n", x->value);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const StrConstNode*>(n)) {
      open("StrConstNode", pad);
      indent(pad + 2); printf("str = \"%s\"\n", show(x->str));
      indent(pad + 2); printf("tag = %s\n", yesno(x->tag));
      indent(pad + 2); printf("doc = %s\n", yesno(x->doc));
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const ConstantNode*>(n)) {
      open("ConstantNode", pad);
      indent(pad + 2); printf("value = \"%s\"\n", show(x->value));
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const AssignExprNode*>(n)) {
      open("AssignExprNode", pad);
      indent(pad + 2); printf("varName = \"%s\"\n", show(x->varName));
      indent(pad + 2); printf("subType = %s\n",    typeReqName(x->subType));
      printChild("arrayIndex", x->arrayIndex, pad + 2);
      printChild("expr",       x->rhsExpr,       pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const AssignOpExprNode*>(n)) {
      open("AssignOpExprNode", pad);
      indent(pad + 2); printf("varName = \"%s\"\n", show(x->varName));
      indent(pad + 2); printf("op = %s\n",          opToStr((S32)x->op));
      indent(pad + 2); printf("operand = %s\n", opToStr((S32)x->operand));
      indent(pad + 2); printf("subType = %s\n",     typeReqName(x->subType));
      printChild("arrayIndex", x->arrayIndex, pad + 2);
      printChild("expr",       x->rhsExpr,       pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const TTagDerefNode*>(n)) {
      open("TTagDerefNode", pad);
      printChild("expr", x->expr, pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const TTagExprNode*>(n)) {
      open("TTagExprNode", pad);
      indent(pad + 2); printf("tag = \"%s\"\n", show(x->tag));
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const FuncCallExprNode*>(n)) {
      open("FuncCallExprNode", pad);
      indent(pad + 2); printf("funcName = \"%s\"\n", show(x->funcName));
      indent(pad + 2); printf("nameSpace = \"%s\"\n", show(x->nameSpace));
      indent(pad + 2); printf("callType = %i\n", x->callType);
      printList("args", x->args, pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const SlotAccessNode*>(n)) {
      open("SlotAccessNode", pad);
      indent(pad + 2); printf("slotName = \"%s\"\n", show(x->slotName));
      printChild("objectExpr", x->objectExpr, pad + 2);
      printChild("arrayExpr",  x->arrayExpr,  pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const InternalSlotAccessNode*>(n)) {
      open("InternalSlotAccessNode", pad);
      indent(pad + 2); printf("recurse = %s\n", yesno(x->recurse));
      printChild("objectExpr", x->objectExpr, pad + 2);
      printChild("slotExpr",   x->slotExpr,   pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const SlotAssignNode*>(n)) {
      open("SlotAssignNode", pad);
      indent(pad + 2); printf("slotName = \"%s\"\n", show(x->slotName));
      indent(pad + 2); printf("typeID = %i\n", -1); // TOFIX
      printChild("objectExpr", x->objectExpr, pad + 2);
      printChild("arrayExpr",  x->arrayExpr,  pad + 2);
      printChild("valueExpr",  x->rhsExpr,  pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const SlotAssignOpNode*>(n)) {
      open("SlotAssignOpNode", pad);
      indent(pad + 2); printf("slotName = \"%s\"\n", show(x->slotName));
      indent(pad + 2); printf("op = %s\n", opToStr((S32)x->op));
      indent(pad + 2); printf("operand = %s\n", opToStr((S32)x->operand));
      indent(pad + 2); printf("subType = %s\n", typeReqName(x->subType));
      printChild("objectExpr", x->objectExpr, pad + 2);
      printChild("arrayExpr",  x->arrayExpr,  pad + 2);
      printChild("valueExpr",  x->rhsExpr,  pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const ObjectDeclNode*>(n)) {
      open("ObjectDeclNode", pad);
      indent(pad + 2); printf("parentObject = \"%s\"\n", show(x->parentObject));
      indent(pad + 2); printf("isDatablock = %s\n", yesno(x->isDatablock));
      indent(pad + 2); printf("isClassNameInternal = %s\n", yesno(x->isClassNameInternal));
      indent(pad + 2); printf("isSingleton = %s\n", yesno(x->isSingleton));
      printChild("classNameExpr", x->classNameExpr, pad + 2);
      printChild("objectNameExpr", x->objectNameExpr, pad + 2);
      printList ("argList",       x->argList,       pad + 2);
      printList ("slotDecls",     x->slotDecls,     pad + 2);
      printList ("subObjects",    x->subObjects,    pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const TryStmtNode*>(n)) {
      open("TryStmtNode", pad);
      printChild("tryBlock",   x->tryBlock,    pad + 2);
      printList("catchBlocks", x->catchBlocks, pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const CatchStmtNode*>(n)) {
      open("CatchStmtNode", pad);
      printChild("testExpr",    x->testExpr,    pad + 2);
      printList("catchBlock",  x->catchBlock,  pad + 2);
      close(n, pad);
      return;
   }
   if (auto x = dynamic_cast<const TupleExprNode*>(n)) {
      open("TupleExprNode", pad);
      printList("items",  x->items,  pad + 2);
      close(n, pad);
      return;
   }
   
   open(typeid(*n).name(), pad);
   close(n, pad);
}

static inline void printTree(const StmtNode* root) {
   const StmtNode* it = root;
   while (it) {
      printNode(it, 0);
      it = it->next;
      if (it) puts("");
   }
}

}

void dumpToInstructionsPrint(Compiler::Resources& res, StmtNode* rootNode)
{
   // Convert AST to bytecode
   CodeStream codeStream(&res);
   codeStream.setFilename("input");

   KorkApi::Config cfg{};
   cfg.mallocFn = [](size_t sz, void* user) {
      return (void*)malloc(sz);
   };
   cfg.freeFn = [](void* ptr, void* user){
      free(ptr);
   };
   cfg.logFn = MyLogger;
   cfg.userResources = &res;
   cfg.enableExceptions = gEnableExtensions;
   cfg.enableTuples = gEnableExtensions;
   cfg.enableTypes = gEnableExtensions;

   KorkApi::Vm* vm = KorkApi::createVM(&cfg);

   CodeBlock* cb = new CodeBlock(vm->mInternal, true);
   res.STEtoCode = &Compiler::compileSTEtoCode;
   //res.resetTables(); // NOTE: should be done before
   
   U32 lastIP = Compiler::compileBlock(rootNode, codeStream, 0) + 1;
   
   codeStream.emit(Compiler::OP_RETURN);
   codeStream.emitCodeStream(&cb->codeSize, &cb->code, &cb->lineBreakPairs);
   
   
   cb->lineBreakPairCount = codeStream.getNumLineBreaks();
   
   cb->globalStrings   = res.getGlobalStringTable().build();
   cb->globalStringsMaxLen = res.getGlobalStringTable().totalLen;
   
   cb->functionStrings = res.getFunctionStringTable().build();
   cb->functionStringsMaxLen = res.getFunctionStringTable().totalLen;
   
   cb->globalFloats    = res.getGlobalFloatTable().build();
   cb->functionFloats  = res.getFunctionFloatTable().build();

   res.getIdentTable().build(&cb->identStrings, &cb->identStringOffsets, &cb->numIdentStrings);

   cb->dumpInstructions(0, false, true);

   KorkApi::destroyVM(vm);
}

bool printAST(const char* buf, const char* filename)
{
   std::string theBuf(buf);
   Compiler::Resources res;
   SimpleLexer::Tokenizer lex(StringTable, theBuf, filename);
   SimpleParser::ASTGen astGen(&lex, &res);

   res.allowExceptions = gEnableExtensions;
   res.allowTuples = gEnableExtensions;
   res.allowTypes = gEnableExtensions;
   
   StmtNode* rootNode = NULL;
   
   try
   {
      astGen.processTokens();
      rootNode = astGen.parseProgram();
      
      if (gPrintBytecode)
      {
         // Convert AST to bytecode
         printf("== Parser Bytecode ==\n");
         dumpToInstructionsPrint(res, rootNode);
      }
      else
      {
         astprint::printTree(rootNode);
      }
   }
   catch (SimpleParser::TokenError& e)
   {
      printf("Error parsing (%s :: %s)\n", e.what(), lex.toString(e.token()).c_str());
   }
   
   return true;
}


int procMain(int argc, char **argv)
{
   if (argc < 2)
   {
      printf("Not enough args\n");
      return 1;
   }
   
   for (int i=2; i<argc; i++)
   {
      if (strcmp(argv[i], "-b") == 0)
      {
         gPrintBytecode = true;
      }
      if (strcmp(argv[i], "-e") == 0)
      {
         gEnableExtensions = true;
      }
   }
   
   FileStream fs;
   if (!fs.open(argv[1], FileStream::Read))
   {
      printf("Error loading file %s\n", argv[1]);
      return 1;
   }
   
   char* data = new char[fs.getStreamSize()+1];
   fs.read(fs.getStreamSize(), data);
   data[fs.getStreamSize()] = '\0';
   
   int ret = printAST(data, argv[1]) ? 0 : 1;
   delete[] data;
   return ret;
}

int main(int argc, char **argv)
{
   int ret = procMain(argc, argv);
   
   return ret;
}
