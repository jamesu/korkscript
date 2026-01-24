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


#ifndef _COMPILER_H_
#define _COMPILER_H_

//#define DEBUG_CODESTREAM

#ifdef DEBUG_CODESTREAM
#include <stdio.h>
#endif

class Stream;

#include "platform/platform.h"
#include "console/codeBlock.h"

#include "core/dataChunker.h"
#include "embed/compilerOpcodes.h"

namespace SimpleParser
{
class ASTGen;
}

struct StmtNode;

namespace Compiler
{
   struct Resources;

   //------------------------------------------------------------

   F64 consoleStringToNumber(const char *str, StringTableEntry file = 0, U32 line = 0);
   
   U32 compileBlock(StmtNode *block, CodeStream &codeStream, U32 ip);

   //------------------------------------------------------------

   struct CompilerIdentTable
   {
      struct Patch
      {
         Patch* next;
         U32 ip;
      };
      
      struct FullEntry
      {
         FullEntry *next;
         Patch* patch;
         StringTableEntry steName;
         U32 offset;
         U32 numInstances;
      };
      
      Resources* res;
      FullEntry *list;
      FullEntry *tail; // so we have stable ids
      U32 numIdentStrings;
      
      U32 addNoAddress(StringTableEntry ste);
      U32 add(StringTableEntry ste, U32 ip);
      void reset();
      void write(Stream &st);
      void build(StringTableEntry** strings,  U32** stringOffsets, U32* numStrings);
      U32 append(CompilerIdentTable &other);

      CompilerIdentTable(Resources* _res) : res(_res)
      {
         list = NULL;
         tail = NULL;
         numIdentStrings = 0;
      }
   };

   //------------------------------------------------------------

   struct CompilerStringTable
   {
      struct Entry
      {
         char *string;
         U32 start;
         U32 len;
         bool tag;
         Entry *next;
      };
      U32 totalLen;
      Resources* res;
      Entry *list;

      CompilerStringTable(Resources* _res) : res(_res)
      {
         totalLen = 0;
         list = NULL;
         memset(buf, 0, sizeof(buf));
      }

      char buf[256];

      U32 add(const char *str, bool caseSens = true, bool tag = false);
      U32 addIntString(U32 value);
      U32 addFloatString(F64 value);
      void reset();
      char *build();
      void write(Stream &st);
   };

   //------------------------------------------------------------

   struct CompilerFloatTable
   {
      struct Entry
      {
         F64 val;
         Entry *next;
      };
      U32 count;
      Resources* res;
      Entry *list;

      CompilerFloatTable(Resources* _res) : res(_res)
      {
         count = 0;
         list = NULL;
      }

      U32 add(F64 value);
      void reset();
      F64 *build();
      void write(Stream &st);
   };

   //------------------------------------------------------------

   struct Resources;

   void evalSTEtoCode(Resources* res, StringTableEntry ste, U32 ip, U32 *ptr);
   void compileSTEtoCode(Resources* res, StringTableEntry ste, U32 ip, U32 *ptr);

   static inline StringTableEntry CodeToSTE(Resources* res, StringTableEntry* stringList, U32 *code, U32 ip)
   {
      U32 offset = *((U32*)(code+ip));
      return offset == 0 ? NULL : stringList[offset-1];
   }

   struct VarTypeTableEntry
   {
      VarTypeTableEntry *next;
      StringTableEntry name;
      StringTableEntry typeName;
      S32 typeId;
   };

   struct VarTypeTable
   {
      VarTypeTableEntry* table;
      Resources* res;

      VarTypeTableEntry* lookupVar(StringTableEntry name);
      void reset();

      VarTypeTable()
      {
         table = NULL;
      }
   };

   struct Resources
   {
      enum 
      {
         VarTypeStackSize = 3
      };

      CompilerStringTable *currentStringTable, globalStringTable, functionStringTable;
      CompilerFloatTable  *currentFloatTable,  globalFloatTable,  functionFloatTable;
      KorkApi::VMChunker   consoleAllocator;
      CompilerIdentTable   identTable;
      CompilerIdentTable   typeTable;

      VarTypeTable globalVarTypes;
      VarTypeTable localVarTypes[VarTypeStackSize];
      U32 curLocalVarStackPos;

      SimpleParser::ASTGen* currentASTGen;

      bool syntaxError;
      bool allowExceptions;
      bool allowTuples;
      bool allowTypes;
      bool allowStringInterpolation;

      void (*STEtoCode)(Resources* res, StringTableEntry ste, U32 ip, U32 *ptr);


      //------------------------------------------------------------

      CompilerStringTable *getCurrentStringTable()  { return currentStringTable;  }
      CompilerStringTable &getGlobalStringTable()   { return globalStringTable;   }
      CompilerStringTable &getFunctionStringTable() { return functionStringTable; }

      void setCurrentStringTable (CompilerStringTable* cst) { currentStringTable  = cst; }

      CompilerFloatTable *getCurrentFloatTable()    { return currentFloatTable;   }
      CompilerFloatTable &getGlobalFloatTable()     { return globalFloatTable;    }
      CompilerFloatTable &getFunctionFloatTable()   { return functionFloatTable; }

      void setCurrentFloatTable (CompilerFloatTable* cst) { currentFloatTable  = cst; }

      CompilerIdentTable &getIdentTable() { return identTable; }
      CompilerIdentTable &getTypeTable() { return typeTable; }

      void precompileIdent(StringTableEntry ident);
      S32 precompileType(StringTableEntry ident);
      void resetTables();

      void *consoleAlloc(U32 size) { return consoleAllocator.alloc(size);  }
      void consoleAllocReset()     { consoleAllocator.freeBlocks(); }

      void pushLocalVarContext(); 
      void popLocalVarContext();
      VarTypeTableEntry* getVarInfo(StringTableEntry varName, StringTableEntry typeName = NULL);

      Resources() : globalStringTable(this), functionStringTable(this), globalFloatTable(this), functionFloatTable(this), identTable(this), typeTable(this)
      {
         STEtoCode = evalSTEtoCode;
         curLocalVarStackPos = 0;
         syntaxError = false;
         allowExceptions = false;
         allowTuples = false;
         allowTypes = false;
         currentASTGen = NULL;
         
         globalVarTypes.res = this;
         for (U32 i=0; i<VarTypeStackSize; i++)
         {
            localVarTypes[i].res = this;
         }
         
         curLocalVarStackPos = 0;
         currentASTGen = NULL;
         
         resetTables();
      }
   };
};

/// Utility class to emit and patch bytecode
class CodeStream
{
public:
   
   enum FixType
   {
      // For loops
      FIXTYPE_LOOPBLOCKSTART,
      FIXTYPE_BREAK,
      FIXTYPE_CONTINUE
   };
   
   enum Constants
   {
      BlockSize = 16384,
      MaxCalls = 65535,
      MaxVarStackDepth
   };
   
protected:
   
   typedef struct PatchEntry
   {
      U32 addr;  ///< Address to patch
      U32 value; ///< Value to place at addr
      
      PatchEntry() {;}
      PatchEntry(U32 a, U32 v)  : addr(a), value(v) {;}
   } PatchEntry;
   
   typedef struct CodeData
   {
      U8 *data;       ///< Allocated data (size is BlockSize)
      U32 size;       ///< Bytes used in data
      CodeData *next; ///< Next block
   } CodeData;
   
   /// @name Emitted code
   /// {
   CodeData *mCode;
   CodeData *mCodeHead;
   U32 mCodePos;
   /// }
   
   /// @name Code fixing stacks
   /// {
   KorkApi::Vector<U32> mFixList;
   KorkApi::Vector<U32> mFixStack;
   KorkApi::Vector<bool> mFixLoopStack;
   KorkApi::Vector<PatchEntry> mPatchList;
   /// }


   KorkApi::Vector<S32> mReturnTypeStack;
   
   KorkApi::Vector<U32> mBreakLines; ///< Line numbers
   
   const char* mFilename;

   U32 mCurrentReturnType;
   
   U32 mNumFuncCalls;
   
public:
   Compiler::Resources* mResources;
   
public:

   CodeStream(Compiler::Resources* res) : mCode(0), mCodeHead(NULL), mCodePos(0), mFilename(NULL), mResources(res)
   {
   }
   
   ~CodeStream()
   {
      reset();
      
      if (mCode)
      {
         KorkApi::VMem::Delete(mCode->data);
         KorkApi::VMem::Delete(mCode);
      }
   }
   
   void setFilename(const char* name) { mFilename = name; }
   const char* getFilename() const { return mFilename; }
   
   U8 *allocCode(U32 sz);
   
   inline U32 emit(U32 code)
   {
      U32 *ptr = (U32*)allocCode(4);
      *ptr = code;
#ifdef DEBUG_CODESTREAM
      printf("code[%u] = %u\n", mCodePos, code);
#endif
      return mCodePos++;
   }
   
   inline void patch(U32 addr, U32 code)
   {
#ifdef DEBUG_CODESTREAM
      printf("patch[%u] = %u\n", addr, code);
#endif
      mPatchList.push_back(PatchEntry(addr, code));
   }
   
   inline U32 emitSTE(const char *code)
   {
      U32* ptr = (U32*)allocCode(8);
      ptr[0] = 0;
      ptr[1] = 0;
      mResources->STEtoCode(mResources, code, mCodePos, (U32*)ptr);
#ifdef DEBUG_CODESTREAM
      printf("code[%u] = %s\n", mCodePos, code);
#endif
      mCodePos += 2;
      return mCodePos-2;
   }
   
   inline U32 tell()
   {
      return mCodePos;
   }
   
   inline bool inLoop()
   {
      for (U32 i=0; i<mFixLoopStack.size(); i++)
      {
         if (mFixLoopStack[i])
            return true;
      }
      return false;
   }
   
   inline U32 emitFix(FixType type)
   {
      U32 *ptr = (U32*)allocCode(4);
      *ptr = (U32)type;
      
#ifdef DEBUG_CODESTREAM
      printf("code[%u] = [FIX:%u]\n", mCodePos, (U32)type);
#endif
      
      mFixList.push_back(mCodePos);
      mFixList.push_back((U32)type);
      return mCodePos++;
   }
   
   inline void pushFixScope(bool isLoop)
   {
      mFixStack.push_back(mFixList.size());
      mFixLoopStack.push_back(isLoop);
   }
   
   inline void popFixScope()
   {
      AssertFatal(mFixStack.size() > 0, "Fix stack mismatch");
      
      U32 newSize = mFixStack[mFixStack.size()-1];
      while (mFixList.size() > newSize)
         mFixList.pop_back();
      mFixStack.pop_back();
      mFixLoopStack.pop_back();
   }
   
   void fixLoop(U32 loopBlockStart, U32 breakPoint, U32 continuePoint);
   
   inline void addBreakLine(U32 lineNumber, U32 ip)
   {
      mBreakLines.push_back(lineNumber);
      mBreakLines.push_back(ip);
   }
   
   inline U32 getNumLineBreaks()
   {
      return mBreakLines.size() / 2;
   }
   
   void emitCodeStream(U32 *size, U32 **stream, U32 **lineBreaks, U32* numFuncCalls, void*** funcCallsPtr);
   
   void reset();

   void pushReturnType(S32 typeId)
   {
      mReturnTypeStack.push_back(typeId);
   }

   void popReturnType()
   {
      mReturnTypeStack.pop_back();
   }

   S32 getReturnType()
   {
      if (mReturnTypeStack.empty())
      {
         return -1;
      }
      else
      {
         return mReturnTypeStack.back();
      }
   }
   
   U32 addFuncCall()
   {
      U32 cn = ++mNumFuncCalls;
      if (cn > MaxCalls)
      {
         // Fallback for max calls reached
         cn = 0;
      }
      return cn;
   }
};

#endif
