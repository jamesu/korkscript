#include "platform/platform.h"
#include "console/console.h"
#include "console/ast.h"
#include "console/compiler.h"
#include "console/simpleLexer.h"
#include "console/simpleParser.h"
#include "core/fileStream.h"
#include <stdio.h>

/*

Code to verify lexer generates identical output to original lexer.

*/

bool gPrintTokens = false;

void MyLogger(ConsoleLogEntry::Level level, const char *consoleLine)
{
   printf("%s\n", consoleLine);
}

#include <typeinfo>
#include <cstring> // std::strcmp

// --- small helpers ----------------------------------------------------------

static inline bool eqStrTbl(StringTableEntry a, StringTableEntry b) {
    return a == b;
}

static inline bool eqCStr(const char* a, const char* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    return std::strcmp(a, b) == 0;
}

template <class T>
static bool eqPtr(const T* a, const T* b);

// Forward decl – single unified entrypoint for *any* node:
static bool eqNode(const StmtNode* a, const StmtNode* b);

// Compare the linked list starting at a & b (i.e., .next chains)
static bool eqNext(const StmtNode* a, const StmtNode* b) {
    return eqNode(a ? a->next : nullptr, b ? b->next : nullptr);
}

template <class T>
static bool eqPtr(const T* a, const T* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    return eqNode(a, b); // safe since every node type derives from StmtNode
}

// Convenience macro to reduce boilerplate for simple binary/unary exprs
#define CMP_FIELDS_1(n, f1)            if (! (n->f1 == other->f1)) return false;
#define CMP_FIELDS_2(n, f1, f2)        CMP_FIELDS_1(n, f1)        CMP_FIELDS_1(n, f2)
#define CMP_FIELDS_3(n, f1, f2, f3)    CMP_FIELDS_2(n, f1, f2)    CMP_FIELDS_1(n, f3)
#define CMP_FIELDS_4(n, f1, f2, f3, f4)CMP_FIELDS_3(n, f1, f2, f3)CMP_FIELDS_1(n, f4)

// --- per-type comparisons ----------------------------------------------------

static bool eqNodeSameType(const StmtNode* a, const StmtNode* b) {
    // BREAK/CONTINUE (no payload)
    if (auto n = dynamic_cast<const BreakStmtNode*>(a)) {
        (void)static_cast<const BreakStmtNode*>(b); // type already same
        return eqNext(n, b);
    }
    if (auto n = dynamic_cast<const ContinueStmtNode*>(a)) {
        (void)static_cast<const ContinueStmtNode*>(b);
        return eqNext(n, b);
    }

    // return <expr>;
    if (auto n = dynamic_cast<const ReturnStmtNode*>(a)) {
        auto other = static_cast<const ReturnStmtNode*>(b);
        if (!eqPtr(n->expr, other->expr)) return false;
        return eqNext(n, b);
    }

    // if (...) {...} else {...}
    if (auto n = dynamic_cast<const IfStmtNode*>(a)) {
        auto other = static_cast<const IfStmtNode*>(b);
        if (!eqPtr(n->testExpr, other->testExpr)) return false;
        if (!eqPtr(n->ifBlock,  other->ifBlock))  return false;
        if (!eqPtr(n->elseBlock,other->elseBlock))return false;
        if (n->integer != other->integer) return false;
        if (n->propagate != other->propagate) return false;
        return eqNext(n, b);
    }

    // loops
    if (auto n = dynamic_cast<const LoopStmtNode*>(a)) {
        auto other = static_cast<const LoopStmtNode*>(b);
        if (!eqPtr(n->testExpr,   other->testExpr))   return false;
        if (!eqPtr(n->initExpr,   other->initExpr))   return false;
        if (!eqPtr(n->endLoopExpr,other->endLoopExpr))return false;
        if (!eqPtr(n->loopBlock,  other->loopBlock))  return false;
        if (n->isDoLoop != other->isDoLoop) return false;
        if (n->integer  != other->integer)  return false;
        return eqNext(n, b);
    }

    // foreach
    if (auto n = dynamic_cast<const IterStmtNode*>(a)) {
        auto other = static_cast<const IterStmtNode*>(b);
        if (!eqStrTbl(n->varName, other->varName)) return false;
        if (!eqPtr(n->containerExpr, other->containerExpr)) return false;
        if (!eqPtr(n->body, other->body)) return false;
        if (n->isStringIter != other->isStringIter) return false;
        return eqNext(n, b);
    }

    // Expressions -------------------------------------------------------------

    // Ternary
    if (auto n = dynamic_cast<const ConditionalExprNode*>(a)) {
        auto other = static_cast<const ConditionalExprNode*>(b);
        if (!eqPtr(n->testExpr,  other->testExpr))  return false;
        if (!eqPtr(n->trueExpr,  other->trueExpr))  return false;
        if (!eqPtr(n->falseExpr, other->falseExpr)) return false;
        if (n->integer != other->integer) return false;
        return eqNext(n, b);
    }

    // Binary families share left/right + op or extra flags:
    if (auto n = dynamic_cast<const BinaryExprNode*>(a)) {
        auto ob = static_cast<const BinaryExprNode*>(b);
        if (!eqPtr(n->left,  ob->left))  return false;
        if (!eqPtr(n->right, ob->right)) return false;

        if (auto fn = dynamic_cast<const FloatBinaryExprNode*>(a)) {
            auto other = static_cast<const FloatBinaryExprNode*>(b);
            if (fn->op != other->op) return false;
            return eqNext(fn, b);
        }
        if (auto in = dynamic_cast<const IntBinaryExprNode*>(a)) {
            auto other = static_cast<const IntBinaryExprNode*>(b);
            if (in->op != other->op) return false;
            // subtype/operand are derived from op; compare for exactness anyway:
            if (in->subType != other->subType) return false;
            if (in->operand != other->operand) return false;
            return eqNext(in, b);
        }
        if (auto se = dynamic_cast<const StreqExprNode*>(a)) {
            auto other = static_cast<const StreqExprNode*>(b);
            if (se->eq != other->eq) return false;
            return eqNext(se, b);
        }
        if (auto sc = dynamic_cast<const StrcatExprNode*>(a)) {
            auto other = static_cast<const StrcatExprNode*>(b);
            if (sc->appendChar != other->appendChar) return false;
            return eqNext(sc, b);
        }
        if (auto cc = dynamic_cast<const CommaCatExprNode*>(a)) {
            (void)static_cast<const CommaCatExprNode*>(b);
            return eqNext(cc, b);
        }
    }

    // Unary
    if (auto n = dynamic_cast<const IntUnaryExprNode*>(a)) {
        auto other = static_cast<const IntUnaryExprNode*>(b);
        if (n->op != other->op) return false;
        if (!eqPtr(n->expr, other->expr)) return false;
        if (n->integer != other->integer) return false;
        return eqNext(n, b);
    }
    if (auto n = dynamic_cast<const FloatUnaryExprNode*>(a)) {
        auto other = static_cast<const FloatUnaryExprNode*>(b);
        if (n->op != other->op) return false;
        if (!eqPtr(n->expr, other->expr)) return false;
        return eqNext(n, b);
    }

    // Variables and literals
    if (auto n = dynamic_cast<const VarNode*>(a)) {
        auto other = static_cast<const VarNode*>(b);
        if (!eqStrTbl(n->varName, other->varName)) return false;
        if (!eqPtr(n->arrayIndex, other->arrayIndex)) return false;
        return eqNext(n, b);
    }
    if (auto n = dynamic_cast<const IntNode*>(a)) {
        auto other = static_cast<const IntNode*>(b);
        if (n->value != other->value) return false;
        return eqNext(n, b);
    }
    if (auto n = dynamic_cast<const FloatNode*>(a)) {
        auto other = static_cast<const FloatNode*>(b);
        if (n->value != other->value) return false;
        return eqNext(n, b);
    }
    if (auto n = dynamic_cast<const StrConstNode*>(a)) {
        auto other = static_cast<const StrConstNode*>(b);
        if (!eqCStr(n->str, other->str)) return false;
        if (n->tag != other->tag) return false;
        if (n->doc != other->doc) return false;
        return eqNext(n, b);
    }
    if (auto n = dynamic_cast<const ConstantNode*>(a)) {
        auto other = static_cast<const ConstantNode*>(b);
        if (!eqStrTbl(n->value, other->value)) return false;
        return eqNext(n, b);
    }

    // Assignments
    if (auto n = dynamic_cast<const AssignExprNode*>(a)) {
        auto other = static_cast<const AssignExprNode*>(b);
        if (!eqStrTbl(n->varName, other->varName)) return false;
        if (!eqPtr(n->arrayIndex, other->arrayIndex)) return false;
        if (!eqPtr(n->expr, other->expr)) return false;
        if (n->subType != other->subType) return false;
        return eqNext(n, b);
    }
    if (auto n = dynamic_cast<const AssignOpExprNode*>(a)) {
        auto other = static_cast<const AssignOpExprNode*>(b);
        if (!eqStrTbl(n->varName, other->varName)) return false;
        if (!eqPtr(n->arrayIndex, other->arrayIndex)) return false;
        if (!eqPtr(n->expr, other->expr)) return false;
        if (n->op != other->op) return false;
        if (n->operand != other->operand) return false;
        if (n->subType != other->subType) return false;
        return eqNext(n, b);
    }

    // TTags
    if (auto n = dynamic_cast<const TTagSetStmtNode*>(a)) {
        auto other = static_cast<const TTagSetStmtNode*>(b);
        if (!eqStrTbl(n->tag, other->tag)) return false;
        if (!eqPtr(n->valueExpr, other->valueExpr)) return false;
        if (!eqPtr(n->stringExpr, other->stringExpr)) return false;
        return eqNext(n, b);
    }
    if (auto n = dynamic_cast<const TTagDerefNode*>(a)) {
        auto other = static_cast<const TTagDerefNode*>(b);
        if (!eqPtr(n->expr, other->expr)) return false;
        return eqNext(n, b);
    }
    if (auto n = dynamic_cast<const TTagExprNode*>(a)) {
        auto other = static_cast<const TTagExprNode*>(b);
        if (!eqStrTbl(n->tag, other->tag)) return false;
        return eqNext(n, b);
    }

    // Calls
    if (auto n = dynamic_cast<const FuncCallExprNode*>(a)) {
        auto other = static_cast<const FuncCallExprNode*>(b);
        if (!eqStrTbl(n->funcName,  other->funcName))  return false;
        if (!eqStrTbl(n->nameSpace, other->nameSpace)) return false;
        if (!eqPtr(n->args, other->args)) return false;    // args compared via .next chain too
        if (n->callType != other->callType) return false;
        return eqNext(n, b);
    }
    if (auto n = dynamic_cast<const AssertCallExprNode*>(a)) {
        auto other = static_cast<const AssertCallExprNode*>(b);
        if (!eqPtr(n->testExpr, other->testExpr)) return false;
        if (!eqCStr(n->message, other->message)) return false;
        return eqNext(n, b);
    }

    // Slot access / assign
    if (auto n = dynamic_cast<const SlotAccessNode*>(a)) {
        auto other = static_cast<const SlotAccessNode*>(b);
        if (!eqPtr(n->objectExpr, other->objectExpr)) return false;
        if (!eqPtr(n->arrayExpr,  other->arrayExpr))  return false;
        if (!eqStrTbl(n->slotName, other->slotName))  return false;
        return eqNext(n, b);
    }
    if (auto n = dynamic_cast<const InternalSlotAccessNode*>(a)) {
        auto other = static_cast<const InternalSlotAccessNode*>(b);
        if (!eqPtr(n->objectExpr, other->objectExpr)) return false;
        if (!eqPtr(n->slotExpr,   other->slotExpr))   return false;
        if (n->recurse != other->recurse) return false;
        return eqNext(n, b);
    }
    if (auto n = dynamic_cast<const SlotAssignNode*>(a)) {
        auto other = static_cast<const SlotAssignNode*>(b);
        if (!eqPtr(n->objectExpr, other->objectExpr)) return false;
        if (!eqPtr(n->arrayExpr,  other->arrayExpr))  return false;
        if (!eqStrTbl(n->slotName, other->slotName))  return false;
        if (!eqPtr(n->valueExpr,  other->valueExpr))  return false;
        if (n->typeID != other->typeID) return false;
        return eqNext(n, b);
    }
    if (auto n = dynamic_cast<const SlotAssignOpNode*>(a)) {
        auto other = static_cast<const SlotAssignOpNode*>(b);
        if (!eqPtr(n->objectExpr, other->objectExpr)) return false;
        if (!eqPtr(n->arrayExpr,  other->arrayExpr))  return false;
        if (!eqStrTbl(n->slotName, other->slotName))  return false;
        if (!eqPtr(n->valueExpr,  other->valueExpr))  return false;
        if (n->op != other->op) return false;
        if (n->operand != other->operand) return false;
        if (n->subType != other->subType) return false;
        return eqNext(n, b);
    }

    // Object declarations
    if (auto n = dynamic_cast<const ObjectDeclNode*>(a)) {
        auto other = static_cast<const ObjectDeclNode*>(b);
        if (!eqPtr(n->classNameExpr, other->classNameExpr)) return false;
        if (!eqStrTbl(n->parentObject, other->parentObject)) return false;
        if (!eqPtr(n->objectNameExpr, other->objectNameExpr)) return false;
        if (!eqPtr(n->argList, other->argList)) return false;
        if (!eqPtr(n->slotDecls, other->slotDecls)) return false;          // these are linked lists, next-compared too
        if (!eqPtr(n->subObjects, other->subObjects)) return false;        // ditto
        if (n->isDatablock != other->isDatablock) return false;
        if (n->isClassNameInternal != other->isClassNameInternal) return false;
        if (n->isSingleton != other->isSingleton) return false;
        return eqNext(n, b);
    }

    // Function declarations
    if (auto n = dynamic_cast<const FunctionDeclStmtNode*>(a)) {
        auto other = static_cast<const FunctionDeclStmtNode*>(b);
        if (!eqStrTbl(n->fnName,  other->fnName))  return false;
        if (!eqStrTbl(n->nameSpace, other->nameSpace)) return false;
        if (!eqStrTbl(n->package, other->package)) return false;
        if (!eqPtr(n->args,  other->args))  return false; // args are VarNode linked by next
        if (!eqPtr(n->stmts, other->stmts)) return false; // body list
        if (n->argc != other->argc) return false;
        return eqNext(n, b);
    }
   
    // Fallback
    return eqNext(a, b);
}

// Public entrypoint
static bool eqNode(const StmtNode* a, const StmtNode* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    // exact concrete type match
    if (typeid(*a) != typeid(*b)) return false;

    return eqNodeSameType(a, b);
}



bool itrCheckASTNodes(StmtNode* rootNode)
{
   return true;
}

bool ensureASTMatches(const char* buf, const char* filename)
{
   Compiler::ConsoleParser myParser;
   myParser.next = NULL;
   myParser.getCurrentFile = [](){
      return "input";
   };
   myParser.getCurrentLine = [](){
      return (S32)0;
   };
   
   // Parse using old API
   Compiler::STEtoCode = &Compiler::evalSTEtoCode;
   Compiler::consoleAllocReset();
   
   gStatementList = NULL;
   
   // Set up the parser.
   CodeBlock::smCurrentParser = Compiler::getParserForFile("input.cs");
   
   // Now do some parsing.
   CodeBlock::smCurrentParser->setScanBuffer(buf, filename);
   CodeBlock::smCurrentParser->restart(NULL);
   CodeBlock::smCurrentParser->parse();
   
   // Now parse using new API
   CodeBlock::smCurrentParser = &myParser;

   std::string theBuf(buf);
   SimpleLexer::Tokenizer lex(StringTable, theBuf, filename);
   SimpleParser::ASTGen astGen(&lex);

   StmtNode* rootNode = NULL;

   try
   {
      astGen.processTokens();
      rootNode = astGen.parseProgram();
      if (!eqNode(rootNode, gStatementList))
      {
         Con::printf("%s: AST Nodes don't match!", filename);
         return 1;
      }
   }
   catch (SimpleParser::TokenError& e)
   {
      Con::printf("Error parsing (%s :: %s)", e.what(), lex.toString(e.token()).c_str());
      
   }

   Con::printf("%s: Parser matches (%i nodes)!\n", filename, 0);
   return true;
}


int procMain(int argc, char **argv)
{
   if (argc < 2)
   {
      Con::printf("Not enough args");
      return 1;
   }
   
   for (int i=2; i<argc; i++)
   {
      if (strcmp(argv[i], "-v") == 0)
      {
         gPrintTokens = true;
      }
   }
   
   FileStream fs;
   if (!fs.open(argv[1], FileStream::Read))
   {
      Con::printf("Error loading file %s\n", argv[1]);
      return 1;
   }

   char* data = new char[fs.getStreamSize()+1];
   fs.read(fs.getStreamSize(), data);
   data[fs.getStreamSize()] = '\0';

   int ret = ensureASTMatches(data, argv[1]) ? 0 : 1;
   delete[] data;
   return ret;
}

int main(int argc, char **argv)
{
   Con::init();
   Con::addConsumer(MyLogger);

   int ret = procMain(argc, argv);

   Con::shutdown();

   return ret;
}
