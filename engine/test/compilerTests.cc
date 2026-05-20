//-----------------------------------------------------------------------------
// Copyright (c) 2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "embed/api.h"
#include "embed/internalApi.h"
#include "console/compiler.h"
#include "console/simpleLexer.h"
#include "engine/test/testVmFixture.h"

#include <catch2/catch.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
   struct CapturedLine
   {
      U32 level;
      std::string text;
   };

   class LogCaptureGuard
   {
   public:
      LogCaptureGuard(KorkApi::Vm* vm, KorkApi::ConsumerCallback cb, void* userPtr)
         : mVm(vm)
      {
         mVm->getLogConsumer(&mPrevLogFn, &mPrevLogUser);
         mVm->setLogConsumer(cb, userPtr);
      }

      ~LogCaptureGuard()
      {
         mVm->setLogConsumer(mPrevLogFn, mPrevLogUser);
      }

   private:
      KorkApi::Vm* mVm;
      KorkApi::ConsumerCallback mPrevLogFn = nullptr;
      void* mPrevLogUser = nullptr;
   };

   void captureLine(U32 level, const char* text, void* userPtr)
   {
      auto* lines = static_cast<std::vector<CapturedLine>*>(userPtr);
      lines->push_back({level, text ? text : ""});
   }

   bool hasLineContaining(const std::vector<CapturedLine>& lines, const std::string& needle)
   {
      return std::any_of(lines.begin(), lines.end(), [&](const CapturedLine& line) {
         return line.text.find(needle) != std::string::npos;
      });
   }

   struct AstStats
   {
      U32 totalNodes = 0;
      U32 stmtNodes = 0;
      U32 functionDecls = 0;
      U32 ifStmts = 0;
      U32 returnStmts = 0;
      U32 binaryExprs = 0;
   };

   KorkApi::AstEnumerationControl collectAstStats(void* userPtr, const KorkApi::AstEnumerationInfo* info)
   {
      auto* stats = static_cast<AstStats*>(userPtr);
      stats->totalNodes++;

      if (info->kind == KorkApi::AstEnumerationNodeStmt)
         stats->stmtNodes++;

      switch (info->nodeType)
      {
         case ASTNodeFunctionDeclStmt:
            stats->functionDecls++;
            break;
         case ASTNodeIfStmt:
            stats->ifStmts++;
            break;
         case ASTNodeReturnStmt:
            stats->returnStmts++;
            break;
         case ASTNodeBinaryExpr:
         case ASTNodeIntBinaryExpr:
         case ASTNodeFloatBinaryExpr:
            stats->binaryExprs++;
            break;
         default:
            break;
      }

      return KorkApi::AstEnumerationContinue;
   }

   std::vector<std::string> lexTokenStrings(TestVmFixture& fx, const char* source, const char* fileName)
   {
      KorkApi::VmAllocTLS::Scope allocScope(fx.vm->mInternal);
      SimpleLexer::Tokenizer<KorkApi::VMStringTable> lex(
         KorkApi::VMStringTable(fx.vm->mInternal),
         source,
         fileName,
         fx.internal->mCompilerResources->allowStringInterpolation,
         fx.internal->mCompilerResources->allowScriptClasses);

      std::vector<std::string> tokens;
      for (;;)
      {
         const SimpleLexer::Token tok = lex.next();
         REQUIRE_FALSE(tok.isNone());
         REQUIRE_FALSE(tok.isIllegal());
         tokens.emplace_back(lex.toString(tok).c_str());
         if (tok.isEnd())
            break;
      }
      return tokens;
   }

   void requireParseStats(TestVmFixture& fx, const char* source, const char* fileName, U32 expectedFunctions, U32 expectedReturns)
   {
      AstStats stats;
      KorkApi::AstParseErrorInfo parseError = {};
      const KorkApi::AstEnumerationResult result = fx.vm->enumerateAst(source, fileName, &stats, collectAstStats, &parseError);
      REQUIRE(result == KorkApi::AstEnumerationCompleted);
      REQUIRE(parseError.stage == KorkApi::AstParseErrorNone);
      REQUIRE(stats.functionDecls == expectedFunctions);
      REQUIRE(stats.returnStmts == expectedReturns);
   }
}

TEST_CASE_METHOD(TestVmFixture, "Compiler can parse code, emit AST, and produce bytecode", "[Compiler]") {
   const char* fileName = "compilerSmoke.cs";
   const char* source =
      "function compilerSmoke(%value)\n"
      "{\n"
      "   if (%value > 1)\n"
      "      return %value + 1;\n"
      "   return %value;\n"
      "}\n";

   AstStats stats;
   KorkApi::AstParseErrorInfo parseError = {};
   const KorkApi::AstEnumerationResult astResult = vm->enumerateAst(source, fileName, &stats, collectAstStats, &parseError);
   REQUIRE(astResult == KorkApi::AstEnumerationCompleted);
   REQUIRE(parseError.stage == KorkApi::AstParseErrorNone);
   REQUIRE(stats.totalNodes > 0);
   REQUIRE(stats.stmtNodes > 0);
   REQUIRE(stats.functionDecls >= 1);
   REQUIRE(stats.ifStmts >= 1);
   REQUIRE(stats.returnStmts >= 2);
   REQUIRE(stats.binaryExprs >= 1);

   KorkApi::CompiledBlock compiled = {};
   std::vector<CapturedLine> compilerLines;
   {
      LogCaptureGuard guard(vm, captureLine, &compilerLines);
      REQUIRE(vm->compileCodeBlock(source, fileName, &compiled));
   }

   REQUIRE(compiled.data != nullptr);
   REQUIRE(compiled.size > 0);

   vm->freeCompiledBlock(compiled);
}

TEST_CASE_METHOD(TestVmFixture, "Lexer tokenizes 1030.dump(1) without swallowing the dot", "[Compiler]") {
   REQUIRE(lexTokenStrings(*this, "1030.dump(1);", "numericDotCallSmoke.cs") ==
      std::vector<std::string>{"INTCONST=INT(1030)", "opCHAR=CHAR(.)", "dump", "opCHAR=CHAR(()", "INTCONST=INT(1)", "opCHAR=CHAR())", "opCHAR=CHAR(;)", "END"});
   requireParseStats(*this, "1030.dump(1);", "numericDotCallSmoke.cs", 0, 0);
}

TEST_CASE_METHOD(TestVmFixture, "Lexer tokenizes 1.dump() without swallowing the dot", "[Compiler]") {
   REQUIRE(lexTokenStrings(*this, "1.dump();", "numericDotCallSmoke.cs") ==
      std::vector<std::string>{"INTCONST=INT(1)", "opCHAR=CHAR(.)", "dump", "opCHAR=CHAR(()", "opCHAR=CHAR())", "opCHAR=CHAR(;)", "END"});
   requireParseStats(*this, "1.dump();", "numericDotCallSmoke.cs", 0, 0);
}

TEST_CASE_METHOD(TestVmFixture, "Lexer tokenizes 1.foo() as integer field access", "[Compiler]") {
   REQUIRE(lexTokenStrings(*this, "1.foo();", "numericFieldSmoke.cs") ==
      std::vector<std::string>{"INTCONST=INT(1)", "opCHAR=CHAR(.)", "foo", "opCHAR=CHAR(()", "opCHAR=CHAR())", "opCHAR=CHAR(;)", "END"});
   requireParseStats(*this, "1.foo();", "numericFieldSmoke.cs", 0, 0);
}

TEST_CASE_METHOD(TestVmFixture, "Lexer tokenizes e-prefixed names after a dot as field access", "[Compiler]") {
   REQUIRE(lexTokenStrings(*this, "1.eFoo();", "numericFieldSmoke.cs") ==
      std::vector<std::string>{"INTCONST=INT(1)", "opCHAR=CHAR(.)", "eFoo", "opCHAR=CHAR(()", "opCHAR=CHAR())", "opCHAR=CHAR(;)", "END"});
   requireParseStats(*this, "1.eFoo();", "numericFieldSmoke.cs", 0, 0);

   REQUIRE(lexTokenStrings(*this, "1.EFoo();", "numericFieldSmoke.cs") ==
      std::vector<std::string>{"INTCONST=INT(1)", "opCHAR=CHAR(.)", "eFoo", "opCHAR=CHAR(()", "opCHAR=CHAR())", "opCHAR=CHAR(;)", "END"});
   requireParseStats(*this, "1.EFoo();", "numericFieldSmoke.cs", 0, 0);
}

TEST_CASE_METHOD(TestVmFixture, "Lexer tokenizes representative floating point literals", "[Compiler]") {
   REQUIRE(lexTokenStrings(*this, "1.;", "floatSmoke.cs") ==
      std::vector<std::string>{"FLTCONST=FLT(1)", "opCHAR=CHAR(;)", "END"});
   requireParseStats(*this, "1.;", "floatSmoke.cs", 0, 0);

   REQUIRE(lexTokenStrings(*this, "0.5;", "floatSmoke.cs") ==
      std::vector<std::string>{"FLTCONST=FLT(0.5)", "opCHAR=CHAR(;)", "END"});
   requireParseStats(*this, "0.5;", "floatSmoke.cs", 0, 0);

   REQUIRE(lexTokenStrings(*this, "12.34;", "floatSmoke.cs") ==
      std::vector<std::string>{"FLTCONST=FLT(12.34)", "opCHAR=CHAR(;)", "END"});
   requireParseStats(*this, "12.34;", "floatSmoke.cs", 0, 0);

   REQUIRE(lexTokenStrings(*this, "1.0;", "floatSmoke.cs") ==
      std::vector<std::string>{"FLTCONST=FLT(1)", "opCHAR=CHAR(;)", "END"});
   requireParseStats(*this, "1.0;", "floatSmoke.cs", 0, 0);

   REQUIRE(lexTokenStrings(*this, "1.e3;", "floatSmoke.cs") ==
      std::vector<std::string>{"FLTCONST=FLT(1000)", "opCHAR=CHAR(;)", "END"});
   requireParseStats(*this, "1.e3;", "floatSmoke.cs", 0, 0);

   REQUIRE(lexTokenStrings(*this, "1e3;", "floatSmoke.cs") ==
      std::vector<std::string>{"FLTCONST=FLT(1000)", "opCHAR=CHAR(;)", "END"});
   requireParseStats(*this, "1e3;", "floatSmoke.cs", 0, 0);

   REQUIRE(lexTokenStrings(*this, "1e-3;", "floatSmoke.cs") ==
      std::vector<std::string>{"FLTCONST=FLT(0.001)", "opCHAR=CHAR(;)", "END"});
   requireParseStats(*this, "1e-3;", "floatSmoke.cs", 0, 0);
}

TEST_CASE_METHOD(TestVmFixture, "Compiler reports parse errors for invalid input", "[Compiler]") {
   const char* fileName = "compilerError.cs";
   const char* source =
      "function brokenCompilerSmoke(%value)\n"
      "{\n"
      "   if (%value > 1)\n"
      "      return %value + ;\n"
      "}\n";

   AstStats stats;
   KorkApi::AstParseErrorInfo parseError = {};
   std::vector<CapturedLine> compilerLines;
   {
      LogCaptureGuard guard(vm, captureLine, &compilerLines);
      REQUIRE(vm->enumerateAst(source, fileName, &stats, collectAstStats, &parseError) == KorkApi::AstEnumerationParseFailed);
   }

   REQUIRE((parseError.stage == KorkApi::AstParseErrorParser || parseError.stage == KorkApi::AstParseErrorLexer));
   REQUIRE(parseError.filename == vm->internString(fileName, false));
   REQUIRE(parseError.message != nullptr);
   REQUIRE(parseError.line > 0);
   REQUIRE(parseError.column > 0);
   (void)compilerLines;
}
