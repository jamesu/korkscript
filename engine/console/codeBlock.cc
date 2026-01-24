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
#include "console/ast.h"
#include "console/consoleNamespace.h"

#include "console/compiler.h"
#include "console/simpleParser.h"
#include "console/codeBlock.h"
#include "console/telnetDebugger.h"

#include "core/stream.h"
#include "core/fileStream.h"
#include "core/stringTable.h"
#include "core/unicode.h"
#include "platform/platformProcess.h" // TOFIX: remove

using namespace Compiler;

//-------------------------------------------------------------------------

CodeBlock::CodeBlock(KorkApi::VmInternal* vm, bool _isExecBlock)
{
   globalStrings = NULL;
   functionStrings = NULL;
   functionStringsMaxLen = 0;
   globalStringsMaxLen = 0;
   numGlobalFloats = 0;
   numFunctionFloats = 0;
   globalFloats = NULL;
   functionFloats = NULL;
   lineBreakPairs = NULL;
   breakList = NULL;
   breakListSize = 0;
   
   identStrings = NULL;
   identStringOffsets = NULL;
   numFunctionCalls = 0;
   functionCalls = NULL;
   numIdentStrings = 0;
   startTypeStrings = 0;
   numTypeStrings = 0;
   typeStringMap = NULL;

   isExecBlock = _isExecBlock;
   inList = false;
   didFlushFunctions = false;
   
   refCount = 0;
   code = NULL;
   name = NULL;
   fullPath = NULL;
   modPath = NULL;
   mRoot = StringTable->EmptyString;
   mVM = vm;
   mVMPublic = vm->mVM;
}

CodeBlock::~CodeBlock()
{
   // Make sure we aren't lingering in the current code block...
   AssertFatal(mVM && mVM->mCurrentCodeBlock != this, "CodeBlock::~CodeBlock - Caught lingering in smCurrentCodeBlock!")

   removeFromCodeList();

   if (mVM == NULL)
   {
      return;
   }

   mVM->DeleteArray(const_cast<char*>(globalStrings));
   mVM->DeleteArray(const_cast<char*>(functionStrings));
   mVM->DeleteArray(globalFloats);
   mVM->DeleteArray(functionFloats);
   mVM->DeleteArray(code);
   mVM->DeleteArray(breakList);
   
   if (functionCalls)
      mVM->DeleteArray(functionCalls);
   if (identStrings)
      mVM->DeleteArray(identStrings);
   if (identStringOffsets)
      mVM->DeleteArray(identStringOffsets);
   if (typeStringMap)
      mVM->DeleteArray(typeStringMap);
}

//-------------------------------------------------------------------------

void CodeBlock::addToCodeList()
{
   if (inList)
   {
      return;
   }

   if (!isExecBlock)
   {
      // remove any code blocks with my name
      for(CodeBlock **walk = &mVM->mCodeBlockList; *walk;walk = &((*walk)->nextFile))
      {
         if((*walk)->name == name)
         {
            *walk = (*walk)->nextFile;
            break;
         }
      }
      nextFile = mVM->mCodeBlockList;
      mVM->mCodeBlockList = this;
   }
   else
   {
      nextFile = mVM->mExecCodeBlockList;
      mVM->mExecCodeBlockList = this;
   }

   inList = true;
}

void CodeBlock::clearAllBreaks()
{
   if(!lineBreakPairs)
      return;
   for(U32 i = 0; i < lineBreakPairCount; i++)
   {
      U32 *p = lineBreakPairs + i * 2;
      code[p[1]] = p[0] & 0xFF;
   }
}

void CodeBlock::clearBreakpoint(U32 lineNumber)
{
   if(!lineBreakPairs)
      return;
   for(U32 i = 0; i < lineBreakPairCount; i++)
   {
      U32 *p = lineBreakPairs + i * 2;
      if((p[0] >> 8) == lineNumber)
      {
         code[p[1]] = p[0] & 0xFF;
         return;
      }
   }
}

void CodeBlock::setAllBreaks()
{
   if(!lineBreakPairs)
      return;
   for(U32 i = 0; i < lineBreakPairCount; i++)
   {
      U32 *p = lineBreakPairs + i * 2;
      code[p[1]] = OP_BREAK;
   }
}

bool CodeBlock::setBreakpoint(U32 lineNumber)
{
   if(!lineBreakPairs)
      return false;
   
   for(U32 i = 0; i < lineBreakPairCount; i++)
   {
      U32 *p = lineBreakPairs + i * 2;
      if((p[0] >> 8) == lineNumber)
      {
         code[p[1]] = OP_BREAK;
         return true;
      }
   }
   
   return false;
}

U32 CodeBlock::findFirstBreakLine(U32 lineNumber)
{
   if(!lineBreakPairs)
      return 0;
   
   for(U32 i = 0; i < lineBreakPairCount; i++)
   {
      U32 *p = lineBreakPairs + i * 2;
      U32 line = (p[0] >> 8);
      
      if( lineNumber <= line )
         return line;
   }
   
   return 0;
}

struct LinePair
{
   U32 instLine;
   U32 ip;
};

void CodeBlock::findBreakLine(U32 ip, U32 &line, U32 &instruction)
{
   U32 min = 0;
   U32 max = lineBreakPairCount - 1;
   LinePair *p = (LinePair *) lineBreakPairs;
   
   U32 found;
   if(!lineBreakPairCount || p[min].ip > ip || p[max].ip < ip)
   {
      line = 0;
      instruction = OP_INVALID;
      return;
   }
   else if(p[min].ip == ip)
      found = min;
   else if(p[max].ip == ip)
      found = max;
   else
   {
      for(;;)
      {
         if(min == max - 1)
         {
            found = min;
            break;
         }
         U32 mid = (min + max) >> 1;
         if(p[mid].ip == ip)
         {
            found = mid;
            break;
         }
         else if(p[mid].ip > ip)
            max = mid;
         else
            min = mid;
      }
   }
   instruction = p[found].instLine & 0xFF;
   line = p[found].instLine;// >> 8;
}

const char *CodeBlock::getFileLine(U32 ip)
{
   char* nameBuffer = mVM->mFileLineBuffer;
   U32 line, inst;
   findBreakLine(ip, line, inst);
   
   dSprintf(nameBuffer, KorkApi::VmInternal::FileLineBufferSize, "%s (%d)", name ? name : "<input>", line);
   return nameBuffer;
}

void CodeBlock::removeFromCodeList()
{
   if (!inList)
   {
      return;
   }

   inList = false;

   if (!isExecBlock)
   {
      for(CodeBlock **walk = &mVM->mCodeBlockList; *walk; walk = &((*walk)->nextFile))
      {
         if(*walk == this)
         {
            *walk = nextFile;
            
            // clear out all breakpoints
            clearAllBreaks();
            return;
         }
      }
   }
   else
   {
      for(CodeBlock **walk = &mVM->mExecCodeBlockList; *walk; walk = &((*walk)->nextFile))
      {
         if(*walk == this)
         {
            *walk = nextFile;
            return;
         }
      }
   }
}

void CodeBlock::calcBreakList()
{
   U32 size = 0;
   S32 line = -1;
   U32 seqCount = 0;
   U32 i;
   for(i = 0; i < lineBreakPairCount; i++)
   {
      U32 lineNumber = lineBreakPairs[i * 2];
      if(lineNumber == U32(line + 1))
         seqCount++;
      else
      {
         if(seqCount)
            size++;
         size++;
         seqCount = 1;
      }
      line = lineNumber;
   }
   if(seqCount)
      size++;
   
   breakList = mVM->NewArray<U32>(size);
   breakListSize = size;
   line = -1;
   seqCount = 0;
   size = 0;
   
   for(i = 0; i < lineBreakPairCount; i++)
   {
      U32 lineNumber = lineBreakPairs[i * 2];
      
      if(lineNumber == U32(line + 1))
         seqCount++;
      else
      {
         if(seqCount)
            breakList[size++] = seqCount;
         breakList[size++] = lineNumber - getMax(0, line) - 1;
         seqCount = 1;
      }
      
      line = lineNumber;
   }
   
   if(seqCount)
      breakList[size++] = seqCount;
   
   for(i = 0; i < lineBreakPairCount; i++)
   {
      U32 *p = lineBreakPairs + i * 2;
      p[0] = (p[0] << 8) | code[p[1]];
   }
   
   // Let the telnet debugger know that this code
   // block has been loaded and that it can add break
   // points it has for it.
   if ( mVM->mTelDebugger )
      mVM->mTelDebugger->addAllBreakpoints( this );
}

void* CodeBlock::getNSEntry(U32 index)
{
   return functionCalls && index < numFunctionCalls ? functionCalls[index] : NULL;
}

void CodeBlock::setNSEntry(U32 index, void* entry)
{
   if (functionCalls && index < numFunctionCalls)
   {
      didFlushFunctions = false;
      functionCalls[index] = entry;
   }
}

void CodeBlock::flushNSEntries()
{
   if (didFlushFunctions)
   {
      return;
   }
   
   memset(functionCalls, '\0', sizeof(void*) * numFunctionCalls);
   didFlushFunctions = true;
}

bool CodeBlock::read(StringTableEntry fileName, Stream &st, U32 readVersion)
{
   const StringTableEntry exePath = Platform::getMainDotCsDir();
   const StringTableEntry cwd = Platform::getCurrentDirectory();

   if (readVersion == 0)
   {
      st.read(&readVersion);

      if (readVersion != KorkApi::DSOVersion)
      {
         return false;
      }
   }

   if (readVersion < KorkApi::MinDSOVersion || readVersion > KorkApi::MaxDSOVersion)
   {
      return false;
   }
   
   name = fileName;
   
   if(fileName && fileName[0])
   {
      fullPath = NULL;
      
      if(Platform::isFullPath(fileName))
         fullPath = fileName;
      
      if(dStrnicmp(exePath, fileName, dStrlen(exePath)) == 0)
         name = StringTable->insert(fileName + dStrlen(exePath) + 1, true);
      else if(dStrnicmp(cwd, fileName, dStrlen(cwd)) == 0)
         name = StringTable->insert(fileName + dStrlen(cwd) + 1, true);
      
      if(fullPath == NULL)
      {
         char buf[1024];
         fullPath = StringTable->insert(Platform::makeFullPathName(fileName, buf, sizeof(buf)), true);
      }
      
      modPath = "";// TOFIX Con::getModNameFromPath(fileName);
   }
   
   //
   if (name)
   {
      if (const char *slash = dStrchr(this->name, '/'))
      {
         char root[512];
         dStrncpy(root, this->name, slash-this->name);
         root[slash-this->name] = 0;
         mRoot = StringTable->insert(root);
      }
   }
   
   //
   if (isExecBlock || (name && name[0]))
   {
      addToCodeList();
   }
   
   U32 size,i;
   st.read(&size);
   if(size)
   {
      globalStrings = mVM->NewArray<char>(size);
      globalStringsMaxLen = size;
      st.read(size, globalStrings);
   }
   st.read(&size);
   if(size)
   {
      functionStrings = mVM->NewArray<char>(size);
      functionStringsMaxLen = size;
      st.read(size, functionStrings);
   }
   st.read(&size);
   if(size)
   {
      globalFloats = mVM->NewArray<F64>(size);
      numGlobalFloats = size;
      for(U32 i = 0; i < size; i++)
         st.read(&globalFloats[i]);
   }
   st.read(&size);
   if(size)
   {
      functionFloats = mVM->NewArray<F64>(size);
      numFunctionFloats = size;
      for(U32 i = 0; i < size; i++)
         st.read(&functionFloats[i]);
   }
   
   codeSize = 0;
   st.read(&codeSize);
   st.read(&lineBreakPairCount);
   
   U32 totSize = codeSize + lineBreakPairCount * 2;
   code = mVM->NewArray<U32>(totSize);
   
   for(i = 0; i < codeSize; i++)
   {
      U8 b;
      st.read(&b);
      if(b == 0xFF)
         st.read(&code[i]);
      else
         code[i] = b;
   }
   
   for(i = codeSize; i < totSize; i++)
      st.read(&code[i]);
   
   lineBreakPairs = code + codeSize;
   
   // StringTable-ize our identifiers.
   U32 identCount = 0;
   st.read(&identCount);
   numIdentStrings = identCount;
   
   identStringOffsets = mVM->NewArray<U32>(identCount);
   identStrings = mVM->NewArray<StringTableEntry>(identCount);

   i = 0;
   while(identCount--)
   {
      U32 offset;
      st.read(&offset);
      StringTableEntry ste;
      if(offset < globalStringsMaxLen)
         ste = StringTable->insert(globalStrings + offset);
      else
         ste = StringTable->EmptyString;
      
      identStrings[i] = ste;
      identStringOffsets[i] = offset;
      
      U32 count;
      st.read(&count);
      while(count--)
      {
         U32 ip;
         st.read(&ip);
         // NOTE: this technically should no longer be needed
         // for new codeblocks.
         code[ip] = i;
      }
      
      i++;
   }

   startTypeStrings = 0;
   numTypeStrings = 0;
   typeStringMap = NULL;

   if (readVersion > 77)
   {
      st.read(&numFunctionCalls);
      st.read(&startTypeStrings);
      st.read(&numTypeStrings);
      typeStringMap = mVM->NewArray<S32>(numTypeStrings);
      for (U32 i=0; i<numTypeStrings; i++)
      {
         typeStringMap[i] = -1;
      }
      
      if (numFunctionCalls == 0)
      {
         numFunctionCalls = 1;
      }
   }
   else
   {
      numFunctionCalls = 1;
   }
   
   // Alloc memory for func call ptrs
   functionCalls = mVM->NewArray<void*>(numFunctionCalls);
   memset(functionCalls, '\0', sizeof(void*) * numFunctionCalls);
   
   if(lineBreakPairCount)
      calcBreakList();
   
   linkTypes();
   
   return true;
}

bool CodeBlock::linkTypes()
{
   for (U32 i=0; i<numTypeStrings; i++)
   {
      typeStringMap[i] = mVM->lookupTypeId(identStrings[startTypeStrings + i]);
      if (typeStringMap[i] == -1)
      {
         mVM->printf(0, "Type %s used in script is undefined", identStrings[startTypeStrings + i]);
         return false;
      }
   }
   return true;
}

StringTableEntry CodeBlock::getTypeName(U32 typeID)
{
   if (typeID < numTypeStrings)
   {
      return identStrings[startTypeStrings + typeID];
   }
   return NULL;
}

U32 CodeBlock::getRealTypeID(U32 typeID)
{
   return typeID < numTypeStrings ? typeStringMap[typeID] : 0;
}

bool CodeBlock::write(Stream &st)
{
   U32 version = KorkApi::DSOVersion;
   st.write(version);
   
   if (globalStrings &&
       globalStringsMaxLen)
   {
      st.write(globalStringsMaxLen);
      st.write(globalStringsMaxLen, globalStrings);
   }
   else
   {
      st.write((U32)0);
   }
   
   if (functionStrings &&
       functionStringsMaxLen)
   {
      st.write(functionStringsMaxLen);
      st.write(functionStringsMaxLen, functionStrings);
   }
   else
   {
      st.write((U32)0);
   }
   
   if (globalFloats &&
       numGlobalFloats)
   {
      st.write(numGlobalFloats);
      for (U32 i=0; i<numGlobalFloats; i++)
      {
         st.write(globalFloats[i]);
      }
   }
   else
   {
      st.write((U32)0);
   }
   
   if (functionFloats &&
       numFunctionFloats)
   {
      st.write(numFunctionFloats);
      for (U32 i=0; i<numFunctionFloats; i++)
      {
         st.write(functionFloats[i]);
      }
   }
   else
   {
      st.write((U32)0);
   }
   
   U32 codeSize = this->codeSize;
   st.write(codeSize);
   st.write(lineBreakPairCount);
   
   const U32 total = codeSize + lineBreakPairCount * 2;
   
   for (U32 i=0; i<codeSize; i++)
   {
      U32 v = code[i];
      
      if (v < 0xFF)
      {
         U8 b = (U8)v;
         st.write(b);
      }
      else
      {
         st.write((U8)0xFF);
         st.write(v);
      }
   }
   
   for (U32 i=codeSize; i<total; i++)
   {
      st.write(code[i]);
   }
   
   st.write(numIdentStrings);
   
   for (U32 i=0; i<numIdentStrings; i++)
   {
      st.write(identStringOffsets[i]);
      st.write((U32)0);
   }
   
   st.write(numFunctionCalls);
   st.write(startTypeStrings);
   st.write(numTypeStrings);
   
   return true;
}


bool CodeBlock::compile(const char *codeFileName, StringTableEntry fileName, const char *inScript)
{
   FileStream st;
   if(!st.open(codeFileName, FileStream::Write))
      return false;
   
   return compileToStream(st, fileName, inScript);
}

bool CodeBlock::compileToStream(Stream &st, StringTableEntry fileName, const char *inScript)
{
   // Check for a UTF8 script file
   char *script;
   chompUTF8BOM( inScript, &script );
   
   mVM->mCompilerResources->syntaxError = false;
   
   mVM->mCompilerResources->consoleAllocReset();
   
   mVM->mCompilerResources->STEtoCode = &Compiler::compileSTEtoCode;
   
   StmtNode* rootNode = NULL;
   
   SimpleLexer::Tokenizer lex(StringTable, inScript, fileName, mVM->mCompilerResources->allowStringInterpolation);
   SimpleParser::ASTGen astGen(&lex, mVM->mCompilerResources);
   
   // Reset all our value tables...
   mVM->mCompilerResources->resetTables();
   
   try
   {
      astGen.processTokens();
      rootNode = astGen.parseProgram();
   }
   catch (SimpleParser::TokenError& e)
   {
      mVM->printf(0, "Error parsing (\"%s\"; token is %s) at %i:%i", e.what(), lex.toString(e.token()).c_str(), e.token().pos.line, e.token().pos.col);
   }
   
   if(!rootNode)
   {
      mVM->Delete(this);
      return "";
   }
   
   if(!rootNode)
   {
      mVM->mCompilerResources->consoleAllocReset();
      return false;
   }
   
   CodeStream codeStream(mVM->mCompilerResources);
   codeStream.setFilename(fileName);
   U32 lastIp;
   if(rootNode)
   {
      lastIp = compileBlock(rootNode, codeStream, 0) + 1;
   }
   else
   {
      codeSize = 1;
      lastIp = 0;
   }
   
   codeStream.emit(OP_RETURN);
   codeStream.emitCodeStream(&codeSize, &code, &lineBreakPairs, &numFunctionCalls, &functionCalls);
   
   lineBreakPairCount = codeStream.getNumLineBreaks();
   
   st.write(U32(KorkApi::DSOVersion));
   
   // Write string table data...
   mVM->mCompilerResources->getGlobalStringTable().write(st);
   mVM->mCompilerResources->getFunctionStringTable().write(st);
   
   // Write float table data...
   mVM->mCompilerResources->getGlobalFloatTable().write(st);
   mVM->mCompilerResources->getFunctionFloatTable().write(st);
   
   if(lastIp != codeSize)
   {
      mVM->printf(0, "CodeBlock::compile - precompile size mismatch, a precompile/compile function pair is probably mismatched.");
   }
   
   U32 totSize = codeSize + codeStream.getNumLineBreaks() * 2;
   st.write(codeSize);
   st.write(lineBreakPairCount);
   
   // Write out our bytecode, doing a bit of compression for low numbers.
   U32 i;
   for(i = 0; i < codeSize; i++)
   {
      if(code[i] < 0xFF)
         st.write(U8(code[i]));
      else
      {
         st.write(U8(0xFF));
         st.write(code[i]);
      }
   }
   
   // Write the break info...
   for(i = codeSize; i < totSize; i++)
      st.write(code[i]);
   
   Compiler::CompilerIdentTable& mainTable = mVM->mCompilerResources->getIdentTable();
   Compiler::CompilerIdentTable& typeTable = mVM->mCompilerResources->getTypeTable();

   startTypeStrings = mainTable.numIdentStrings;
   numTypeStrings = typeTable.numIdentStrings;

   mainTable.append(typeTable);
   mainTable.write(st);
   
   typeStringMap = mVM->NewArray<S32>(numTypeStrings);
   for (U32 i=0; i<numTypeStrings; i++)
   {
      typeStringMap[i] = -1;
   }

   // Write offsets
   
   st.write(startTypeStrings);
   st.write(numTypeStrings);
   
   mVM->mCompilerResources->consoleAllocReset();
   
   return true;
}

 KorkApi::ConsoleValue CodeBlock::compileExec(StringTableEntry fileName, const char *inString, bool noCalls, bool isNativeFrame, int setFrame)
{
   // Check for a UTF8 script file
   char *string;
   chompUTF8BOM( inString, &string );

   mVM->mCompilerResources->STEtoCode = &Compiler::compileSTEtoCode;
   mVM->mCompilerResources->consoleAllocReset();
   
   name = fileName;
   
   if(fileName)
   {
      const StringTableEntry exePath = Platform::getMainDotCsDir();
      const StringTableEntry cwd = Platform::getCurrentDirectory();
      
      fullPath = NULL;
      
      if(Platform::isFullPath(fileName))
         fullPath = fileName;
      
      if(exePath && dStrnicmp(exePath, fileName, dStrlen(exePath)) == 0)
         name = StringTable->insert(fileName + dStrlen(exePath) + 1, true);
      else if(cwd && dStrnicmp(cwd, fileName, dStrlen(cwd)) == 0)
         name = StringTable->insert(fileName + dStrlen(cwd) + 1, true);
      
      if(fullPath == NULL)
      {
         char buf[1024];
         fullPath = StringTable->insert(Platform::makeFullPathName(fileName, buf, sizeof(buf)), true);
      }
      
      modPath = ""; // TOFIX Con::getModNameFromPath(fileName);
   }
   
   if (isExecBlock || (name && name[0]))
   {
      addToCodeList();
   }
   
   StmtNode* rootNode = NULL;
   
   SimpleLexer::Tokenizer lex(StringTable, inString, fileName ? fileName : "", mVM->mCompilerResources->allowStringInterpolation);
   SimpleParser::ASTGen astGen(&lex, mVM->mCompilerResources);
    
    // Need to do this here as ast node gen stores stuff in tables
    mVM->mCompilerResources->resetTables();
   
   try
   {
      astGen.processTokens();
      rootNode = astGen.parseProgram();
   }
   catch (SimpleParser::TokenError& e)
   {
      mVM->printf(0, "Error parsing (\"%s\"; token is %s) at %i:%i", e.what(), lex.toString(e.token()).c_str(), e.token().pos.line, e.token().pos.col);
   }
   
   if(!rootNode)
   {
      mVM->Delete(this);
      return KorkApi::ConsoleValue();
   }
   
   CodeStream codeStream(mVM->mCompilerResources);
   codeStream.setFilename(fileName);
   U32 lastIp = compileBlock(rootNode, codeStream, 0);
   
   lineBreakPairCount = codeStream.getNumLineBreaks();
   
   globalStrings   = mVM->mCompilerResources->getGlobalStringTable().build();
   globalStringsMaxLen = mVM->mCompilerResources->getGlobalStringTable().totalLen;
   
   functionStrings = mVM->mCompilerResources->getFunctionStringTable().build();
   functionStringsMaxLen = mVM->mCompilerResources->getFunctionStringTable().totalLen;
   
   globalFloats    = mVM->mCompilerResources->getGlobalFloatTable().build();
   functionFloats  = mVM->mCompilerResources->getFunctionFloatTable().build();
    numGlobalFloats = mVM->mCompilerResources->getGlobalFloatTable().count;
    numFunctionFloats = mVM->mCompilerResources->getFunctionFloatTable().count;
    
    // Combine ident with type table and set offsets
   Compiler::CompilerIdentTable& mainTable = mVM->mCompilerResources->getIdentTable();
   Compiler::CompilerIdentTable& typeTable = mVM->mCompilerResources->getTypeTable();

   startTypeStrings = mainTable.numIdentStrings;
   numTypeStrings = typeTable.numIdentStrings;
    
   typeStringMap = mVM->NewArray<S32>(numTypeStrings);
   for (U32 i=0; i<numTypeStrings; i++)
   {
      typeStringMap[i] = -1;
   }

   mainTable.append(typeTable);
   mainTable.build(&identStrings, &identStringOffsets, &numIdentStrings);
   
   codeStream.emit(OP_RETURN);
   codeStream.emitCodeStream(&codeSize, &code, &lineBreakPairs, &numFunctionCalls, &functionCalls);
   
   mVM->mCompilerResources->consoleAllocReset();
   
   if (lineBreakPairCount)
      calcBreakList();
   
   if(lastIp+1 != codeSize)
   {
      mVM->printf(0, "precompile size mismatch");
   }
    
   if (!linkTypes())
   {
      mVM->printf(0, "Invalid types in script");
      return KorkApi::ConsoleValue();
   }
   
   return exec(0, fileName, NULL, 0, 0, noCalls, isNativeFrame, NULL, setFrame);
}

//-------------------------------------------------------------------------

void CodeBlock::incRefCount()
{
   refCount++;
}

void CodeBlock::decRefCount()
{
   refCount--;
   if(!refCount)
   {
      mVM->Delete(this);
   }
}

//-------------------------------------------------------------------------

void CodeBlock::dumpInstructions( U32 startIp, bool upToReturn, bool downcaseStrings, bool includeLines )
{
   U32 ip = startIp;
   U32 endFuncIp = 0;
   bool inFunction = false;

   auto codeToSte = [downcaseStrings, this](Resources* res, U32 *code, U32 ip) {
      StringTableEntry ste = Compiler::CodeToSTE(res, identStrings, code,  ip);
      if (!ste || !downcaseStrings)
         return ste;

      const char *src = ste;
      static char buf[4096];
      U32 i = 0;

      for (; src[i] && i < sizeof(buf) - 1; ++i)
         buf[i] = dTolower(src[i]);

      buf[i] = '\0';
      return StringTable->insert(buf, true);
   };

   U32 lastLine = 0;
   
   while( ip < codeSize )
   {
      if (ip >= endFuncIp)
      {
         inFunction = false;
      }

      if (includeLines)
      {
         U32 breakLine = 0;
         U32 breakInst = 0;
         findBreakLine(ip, breakLine, breakInst);

         if (breakLine != lastLine)
         {
            mVM->printf(0, "# Line %u", lastLine+1);
            lastLine = breakLine;
         }
      }
      
      switch( code[ ip ++ ] )
      {
            
         case OP_FUNC_DECL:
         {
            StringTableEntry fnName       = codeToSte(NULL, code, ip);
            StringTableEntry fnNamespace  = codeToSte(NULL, code, ip+2);
            StringTableEntry fnPackage    = codeToSte(NULL, code, ip+4);
            bool hasBody = bool(code[ip+6]);
            U32 newIp = code[ ip + 7 ];
            U32 argc = code[ ip + 8 ];
            endFuncIp = newIp;
            
            mVM->printf(0, "%i: OP_FUNC_DECL name=%s nspace=%s package=%s hasbody=%i newip=%i argc=%i",
               ip - 1, fnName, fnNamespace, fnPackage, hasBody, newIp, argc );
               
            // Skip args.
                           
            ip += 9 + (argc * 2);
            inFunction = true;
            break;
         }
            
         case OP_CREATE_OBJECT:
         {
            StringTableEntry objParent = codeToSte(NULL, code, ip);
            bool isDataBlock =          code[ip + 2];
            bool isInternal  =          code[ip + 3];
            bool isSingleton =          code[ip + 4];
            U32  lineNumber  =          code[ip + 5];
            U32 failJump     =          code[ip + 6];
            
            mVM->printf(0, "%i: OP_CREATE_OBJECT objParent=%s isDataBlock=%i isInternal=%i isSingleton=%i lineNumber=%i failJump=%i",
               ip - 1, objParent, isDataBlock, isInternal, isSingleton, lineNumber, failJump );

            ip += 7;
            break;
         }

         case OP_ADD_OBJECT:
         {
            bool placeAtRoot = code[ip++];
            mVM->printf(0, "%i: OP_ADD_OBJECT placeAtRoot=%i", ip - 1, placeAtRoot );
            break;
         }
         
         case OP_END_OBJECT:
         {
            bool placeAtRoot = code[ip++];
            mVM->printf(0, "%i: OP_END_OBJECT placeAtRoot=%i", ip - 1, placeAtRoot );
            break;
         }
         
         case OP_FINISH_OBJECT:
         {
            mVM->printf(0, "%i: OP_FINISH_OBJECT", ip - 1 );
            break;
         }
         
         case OP_JMPIFFNOT:
         {
            mVM->printf(0, "%i: OP_JMPIFFNOT ip=%i", ip - 1, code[ ip ] );
            ++ ip;
            break;
         }
         
         case OP_JMPIFNOT:
         {
            mVM->printf(0, "%i: OP_JMPIFNOT ip=%i", ip - 1, code[ ip ] );
            ++ ip;
            break;
         }
         
         case OP_JMPIFF:
         {
            mVM->printf(0, "%i: OP_JMPIFF ip=%i", ip - 1, code[ ip ] );
            ++ ip;
            break;
         }

         case OP_JMPIF:
         {
            mVM->printf(0, "%i: OP_JMPIF ip=%i", ip - 1, code[ ip ] );
            ++ ip;
            break;
         }

         case OP_JMPIFNOT_NP:
         {
            mVM->printf(0, "%i: OP_JMPIFNOT_NP ip=%i", ip - 1, code[ ip ] );
            ++ ip;
            break;
         }

         case OP_JMPIF_NP:
         {
            mVM->printf(0, "%i: OP_JMPIF_NP ip=%i", ip - 1, code[ ip ] );
            ++ ip;
            break;
         }

         case OP_JMP:
         {
            mVM->printf(0, "%i: OP_JMP ip=%i", ip - 1, code[ ip ] );
            ++ ip;
            break;
         }

         case OP_RETURN:
         {
            mVM->printf(0, "%i: OP_RETURN", ip - 1 );
            
            if( upToReturn )
               return;
               
            break;
         }

         case OP_RETURN_VOID:
         {
            mVM->printf(0, "%i: OP_RETURNVOID", ip - 1 );

            if( upToReturn )
               return;

            break;
         }

         case OP_RETURN_UINT:
         {
            mVM->printf(0, "%i: OP_RETURNUINT", ip - 1 );

            if( upToReturn )
               return;

            break;
         }

         case OP_RETURN_FLT:
         {
            mVM->printf(0, "%i: OP_RETURNFLT", ip - 1 );

            if( upToReturn )
               return;

            break;
         }

         case OP_CMPEQ:
         {
            mVM->printf(0, "%i: OP_CMPEQ", ip - 1 );
            break;
         }

         case OP_CMPGR:
         {
            mVM->printf(0, "%i: OP_CMPGR", ip - 1 );
            break;
         }

         case OP_CMPGE:
         {
            mVM->printf(0, "%i: OP_CMPGE", ip - 1 );
            break;
         }

         case OP_CMPLT:
         {
            mVM->printf(0, "%i: OP_CMPLT", ip - 1 );
            break;
         }

         case OP_CMPLE:
         {
            mVM->printf(0, "%i: OP_CMPLE", ip - 1 );
            break;
         }

         case OP_CMPNE:
         {
            mVM->printf(0, "%i: OP_CMPNE", ip - 1 );
            break;
         }

         case OP_XOR:
         {
            mVM->printf(0, "%i: OP_XOR", ip - 1 );
            break;
         }

         case OP_MOD:
         {
            mVM->printf(0, "%i: OP_MOD", ip - 1 );
            break;
         }

         case OP_BITAND:
         {
            mVM->printf(0, "%i: OP_BITAND", ip - 1 );
            break;
         }

         case OP_BITOR:
         {
            mVM->printf(0, "%i: OP_BITOR", ip - 1 );
            break;
         }

         case OP_NOT:
         {
            mVM->printf(0, "%i: OP_NOT", ip - 1 );
            break;
         }

         case OP_NOTF:
         {
            mVM->printf(0, "%i: OP_NOTF", ip - 1 );
            break;
         }

         case OP_ONESCOMPLEMENT:
         {
            mVM->printf(0, "%i: OP_ONESCOMPLEMENT", ip - 1 );
            break;
         }

         case OP_SHR:
         {
            mVM->printf(0, "%i: OP_SHR", ip - 1 );
            break;
         }

         case OP_SHL:
         {
            mVM->printf(0, "%i: OP_SHL", ip - 1 );
            break;
         }

         case OP_AND:
         {
            mVM->printf(0, "%i: OP_AND", ip - 1 );
            break;
         }

         case OP_OR:
         {
            mVM->printf(0, "%i: OP_OR", ip - 1 );
            break;
         }

         case OP_ADD:
         {
            mVM->printf(0, "%i: OP_ADD", ip - 1 );
            break;
         }

         case OP_SUB:
         {
            mVM->printf(0, "%i: OP_SUB", ip - 1 );
            break;
         }

         case OP_MUL:
         {
            mVM->printf(0, "%i: OP_MUL", ip - 1 );
            break;
         }

         case OP_DIV:
         {
            mVM->printf(0, "%i: OP_DIV", ip - 1 );
            break;
         }

         case OP_NEG:
         {
            mVM->printf(0, "%i: OP_NEG", ip - 1 );
            break;
         }

         case OP_SETCURVAR:
         {
            StringTableEntry var = codeToSte(NULL, code, ip);
            
            mVM->printf(0, "%i: OP_SETCURVAR var=%s", ip - 1, var );
            ip += 2;
            break;
         }
         
         case OP_SETCURVAR_CREATE:
         {
            StringTableEntry var = codeToSte(NULL, code, ip);
            
            mVM->printf(0, "%i: OP_SETCURVAR_CREATE var=%s", ip - 1, var );
            ip += 2;
            break;
         }
         
         case OP_SETCURVAR_ARRAY:
         {
            mVM->printf(0, "%i: OP_SETCURVAR_ARRAY", ip - 1 );
            break;
         }
         
         case OP_SETCURVAR_ARRAY_CREATE:
         {
            mVM->printf(0, "%i: OP_SETCURVAR_ARRAY_CREATE", ip - 1 );
            break;
         }
         
         case OP_LOADVAR_UINT:
         {
            mVM->printf(0, "%i: OP_LOADVAR_UINT", ip - 1 );
            break;
         }
         
         case OP_LOADVAR_FLT:
         {
            mVM->printf(0, "%i: OP_LOADVAR_FLT", ip - 1 );
            break;
         }

         case OP_LOADVAR_STR:
         {
            mVM->printf(0, "%i: OP_LOADVAR_STR", ip - 1 );
            break;
         }

         case OP_LOADVAR_VAR:
         {
            mVM->printf(0, "%i: OP_LOADVAR_VAR", ip - 1 );
            break;
         }

         case OP_SAVEVAR_UINT:
         {
            mVM->printf(0, "%i: OP_SAVEVAR_UINT", ip - 1 );
            break;
         }

         case OP_SAVEVAR_FLT:
         {
            mVM->printf(0, "%i: OP_SAVEVAR_FLT", ip - 1 );
            break;
         }

         case OP_SAVEVAR_STR:
         {
            mVM->printf(0, "%i: OP_SAVEVAR_STR", ip - 1 );
            break;
         }

         case OP_SAVEVAR_VAR:
         {
            mVM->printf(0, "%i: OP_SAVEVAR_VAR", ip - 1 );
            break;
         }

         case OP_SETCUROBJECT:
         {
            mVM->printf(0, "%i: OP_SETCUROBJECT", ip - 1 );
            break;
         }

         case OP_SETCUROBJECT_NEW:
         {
            mVM->printf(0, "%i: OP_SETCUROBJECT_NEW", ip - 1 );
            break;
         }
         
         case OP_SETCUROBJECT_INTERNAL:
         {
            mVM->printf(0, "%i: OP_SETCUROBJECT_INTERNAL", ip - 1 );
            ++ ip;
            break;
         }
         
         case OP_SETCURFIELD:
         {
            StringTableEntry curField = codeToSte(NULL, code, ip);
            mVM->printf(0, "%i: OP_SETCURFIELD field=%s", ip - 1, curField );
            ip += 2;
            break;
         }
         
         case OP_SETCURFIELD_ARRAY:
         {
            mVM->printf(0, "%i: OP_SETCURFIELD_ARRAY", ip - 1 );
            break;
         }

         case OP_SETCURFIELD_TYPE:
         {
            U32 type = code[ ip ];
            mVM->printf(0, "%i: OP_SETCURFIELD_TYPE type=%i", ip - 1, type );
            ++ ip;
            break;
         }

         case OP_LOADFIELD_UINT:
         {
            mVM->printf(0, "%i: OP_LOADFIELD_UINT", ip - 1 );
            break;
         }

         case OP_LOADFIELD_FLT:
         {
            mVM->printf(0, "%i: OP_LOADFIELD_FLT", ip - 1 );
            break;
         }

         case OP_LOADFIELD_STR:
         {
            mVM->printf(0, "%i: OP_LOADFIELD_STR", ip - 1 );
            break;
         }

         case OP_SAVEFIELD_UINT:
         {
            mVM->printf(0, "%i: OP_SAVEFIELD_UINT", ip - 1 );
            break;
         }

         case OP_SAVEFIELD_FLT:
         {
            mVM->printf(0, "%i: OP_SAVEFIELD_FLT", ip - 1 );
            break;
         }

         case OP_SAVEFIELD_STR:
         {
            mVM->printf(0, "%i: OP_SAVEFIELD_STR", ip - 1 );
            break;
         }

         case OP_STR_TO_UINT:
         {
            mVM->printf(0, "%i: OP_STR_TO_UINT", ip - 1 );
            break;
         }

         case OP_STR_TO_FLT:
         {
            mVM->printf(0, "%i: OP_STR_TO_FLT", ip - 1 );
            break;
         }

         case OP_STR_TO_NONE:
         {
            mVM->printf(0, "%i: OP_STR_TO_NONE", ip - 1 );
            break;
         }

         case OP_FLT_TO_UINT:
         {
            mVM->printf(0, "%i: OP_FLT_TO_UINT", ip - 1 );
            break;
         }

         case OP_FLT_TO_STR:
         {
            mVM->printf(0, "%i: OP_FLT_TO_STR", ip - 1 );
            break;
         }

         case OP_FLT_TO_NONE:
         {
            mVM->printf(0, "%i: OP_FLT_TO_NONE", ip - 1 );
            break;
         }

         case OP_UINT_TO_FLT:
         {
            mVM->printf(0, "%i: OP_SAVEFIELD_STR", ip - 1 );
            break;
         }

         case OP_UINT_TO_STR:
         {
            mVM->printf(0, "%i: OP_UINT_TO_STR", ip - 1 );
            break;
         }

         case OP_UINT_TO_NONE:
         {
            mVM->printf(0, "%i: OP_UINT_TO_NONE", ip - 1 );
            break;
         }

         case OP_COPYVAR_TO_NONE:
         {
            mVM->printf(0, "%i: OP_COPYVAR_TO_NONE", ip - 1 );
            break;
         }

         case OP_LOADIMMED_UINT:
         {
            U32 val = code[ ip ];
            mVM->printf(0, "%i: OP_LOADIMMED_UINT val=%i", ip - 1, val );
            ++ ip;
            break;
         }

         case OP_LOADIMMED_FLT:
         {
            F64 val = (inFunction ? functionFloats : globalFloats)[ code[ ip ] ];
            mVM->printf(0, "%i: OP_LOADIMMED_FLT val=%f", ip - 1, val );
            ++ ip;
            break;
         }

         case OP_TAG_TO_STR:
         {
            const char* str = (inFunction ? functionStrings : globalStrings) + code[ ip ];
            mVM->printf(0, "%i: OP_TAG_TO_STR str=%s", ip - 1, str );
            ++ ip;
            break;
         }
         
         case OP_LOADIMMED_STR:
         {
            const char* str = (inFunction ? functionStrings : globalStrings) + code[ ip ];
            mVM->printf(0, "%i: OP_LOADIMMED_STR str=%s", ip - 1, str );
            ++ ip;
            break;
         }

         case OP_DOCBLOCK_STR:
         {
            const char* str = (inFunction ? functionStrings : globalStrings) + code[ ip ];
            mVM->printf(0, "%i: OP_DOCBLOCK_STR str=%s", ip - 1, str );
            ++ ip;
            break;
         }
         
         case OP_LOADIMMED_IDENT:
         {
            StringTableEntry str = codeToSte(NULL, code, ip);
            mVM->printf(0, "%i: OP_LOADIMMED_IDENT str=%s", ip - 1, str );
            ip += 2;
            break;
         }

         case OP_CALLFUNC_RESOLVE:
         {
            StringTableEntry fnNamespace = codeToSte(NULL, code, ip+2);
            StringTableEntry fnName      = codeToSte(NULL, code, ip);
            U32 callType = code[ip+2];

            mVM->printf(0, "%i: OP_CALLFUNC_RESOLVE name=%s nspace=%s callType=%s", ip - 1, fnName, fnNamespace,
               callType == FuncCallExprNode::FunctionCall ? "FunctionCall"
                  : callType == FuncCallExprNode::MethodCall ? "MethodCall" : "ParentCall" );
            
            ip += 5;
            break;
         }
         
         case OP_CALLFUNC:
         {
            StringTableEntry fnNamespace = codeToSte(NULL, code, ip+2);
            StringTableEntry fnName      = codeToSte(NULL, code, ip);
            U32 callType = code[ip+4];

            mVM->printf(0, "%i: OP_CALLFUNC name=%s nspace=%s callType=%s", ip - 1, fnName, fnNamespace,
               callType == FuncCallExprNode::FunctionCall ? "FunctionCall"
                  : callType == FuncCallExprNode::MethodCall ? "MethodCall" : "ParentCall" );
            
            ip += 5;
            break;
         }

         case OP_ADVANCE_STR:
         {
            mVM->printf(0, "%i: OP_ADVANCE_STR", ip - 1 );
            break;
         }

         case OP_ADVANCE_STR_APPENDCHAR:
         {
            char ch = code[ ip ];
            mVM->printf(0, "%i: OP_ADVANCE_STR_APPENDCHAR char=%c", ip - 1, ch );
            ++ ip;
            break;
         }

         case OP_ADVANCE_STR_COMMA:
         {
            mVM->printf(0, "%i: OP_ADVANCE_STR_COMMA", ip - 1 );
            break;
         }

         case OP_ADVANCE_STR_NUL:
         {
            mVM->printf(0, "%i: OP_ADVANCE_STR_NUL", ip - 1 );
            break;
         }

         case OP_REWIND_STR:
         {
            mVM->printf(0, "%i: OP_REWIND_STR", ip - 1 );
            break;
         }

         case OP_TERMINATE_REWIND_STR:
         {
            mVM->printf(0, "%i: OP_TERMINATE_REWIND_STR", ip - 1 );
            break;
         }

         case OP_COMPARE_STR:
         {
            mVM->printf(0, "%i: OP_COMPARE_STR", ip - 1 );
            break;
         }

         case OP_PUSH:
         {
            mVM->printf(0, "%i: OP_PUSH", ip - 1 );
            break;
         }

         case OP_PUSH_UINT:
         {
            mVM->printf(0, "%i: OP_PUSH_UINT", ip - 1 );
            break;
         }

         case OP_PUSH_FLT:
         {
            mVM->printf(0, "%i: OP_PUSH_FLT", ip - 1 );
            break;
         }

         case OP_PUSH_VAR:
         {
            mVM->printf(0, "%i: OP_PUSH_VAR", ip - 1 );
            break;
         }

         case OP_PUSH_FRAME:
         {
            mVM->printf(0, "%i: OP_PUSH_FRAME", ip - 1 );
            break;
         }

         case OP_ASSERT:
         {
            const char* message = (inFunction ? functionStrings : globalStrings) + code[ ip ];
            mVM->printf(0, "%i: OP_ASSERT message=%s", ip - 1, message );
            ++ ip;
            break;
         }

         case OP_BREAK:
         {
            mVM->printf(0, "%i: OP_BREAK", ip - 1 );
            break;
         }
         
         case OP_ITER_BEGIN:
         {
            StringTableEntry varName = codeToSte(NULL, code, ip);
            U32 failIp = code[ ip + 2 ];
            
            mVM->printf(0, "%i: OP_ITER_BEGIN varName=%s failIp=%i", ip - 1, varName, failIp );

            ip += 3;
            break;
         }

         case OP_ITER_BEGIN_STR:
         {
            StringTableEntry varName = codeToSte(NULL, code, ip);
            U32 failIp = code[ ip + 2 ];
            
            mVM->printf(0, "%i: OP_ITER_BEGIN varName=%s failIp=%i", ip - 1, varName, failIp );

            ip += 3;
            break;
         }
         
         case OP_ITER:
         {
            U32 breakIp = code[ ip ];
            
            mVM->printf(0, "%i: OP_ITER breakIp=%i", ip - 1, breakIp );

            ++ ip;
            break;
         }
         
         case OP_ITER_END:
         {
            mVM->printf(0, "%i: OP_ITER_END", ip - 1 );
            break;
         }
         
         case OP_PUSH_TRY:
         {
            U32 jmpMask = code[ip];
            U32 jmpIP = code[ip+1];
            mVM->printf(0, "%i: OP_PUSH_TRY mask=%u jmpIP=%u", ip - 1,  jmpMask, jmpIP);
            ip += 2;
            break;
         }
         
         case OP_PUSH_TRY_STACK:
         {
            U32 jmpIP = code[ip];
            mVM->printf(0, "%i: OP_PUSH_TRY_STACK jmpIP=%u", ip - 1, jmpIP);
            ++ip;
            break;
         }
         
         case OP_THROW:
         {
            U32 throwMask = code[ip];
            mVM->printf(0, "%i: OP_THROW %u", ip - 1, throwMask);
            ++ip;
            break;
         }
         
         case OP_POP_TRY:
         {
            mVM->printf(0, "%i: OP_POP_TRY", ip - 1);
            break;
         }

         case OP_DUP_UINT:
         {
            mVM->printf(0, "%i: OP_DUP_UINT", ip - 1);
            break;
         }
            
         // Typed vars
         
         case OP_PUSH_TYPED:
         {
            mVM->printf(0, "%i: OP_PUSH_TYPED", ip - 1 );
            break;
         }
         case OP_LOADVAR_TYPED:
         {
            // Takes from OP_SETCURVAR
            mVM->printf(0, "%i: OP_LOADVAR_TYPED", ip - 1 );
            //++ip;
            break;
         }
         case OP_LOADVAR_TYPED_REF:
         {
            // Takes from OP_SETCURVAR
            mVM->printf(0, "%i: OP_LOADVAR_TYPED_REF", ip - 1 );
            //++ip;
            break;
         }
         case OP_LOADFIELD_TYPED:
         {
            // Takes from OP_SETCURFIELD*
            mVM->printf(0, "%i: OP_LOADFIELD_TYPED", ip - 1 );
            //++ip;
            break;
         }
         case OP_SAVEVAR_TYPED:
         {
            // Sets from OP_SETCURVAR
            mVM->printf(0, "%i: OP_SAVEVAR_TYPED", ip - 1 );
            //++ip;
            break;
         }
         case OP_SAVEFIELD_TYPED:
         {
            // Sets from OP_SETCURFIELD*
            mVM->printf(0, "%i: OP_SAVEFIELD_TYPED", ip - 1 );
            //++ip;
            break;
         }
         case OP_STR_TO_TYPED:
         {
            // Casts current StringStack head to input local type id
            mVM->printf(0, "%i: OP_STR_TO_TYPED", ip - 1);
            //++ ip;
            break;
         }
         case OP_FLT_TO_TYPED:
         {
            // Casts current float value to input local type id
            U32 typeID = code[ip];
            mVM->printf(0, "%i: OP_FLT_TO_TYPED", ip - 1);
            break;
         }
         case OP_UINT_TO_TYPED:
         {
            // Casts current uint value to input local type id
            mVM->printf(0, "%i: OP_UINT_TO_TYPED", ip - 1);
            break;
         }
         case OP_TYPED_TO_STR:
         {
            mVM->printf(0, "%i: OP_TYPED_TO_STR", ip - 1);
            break;
         }
         case OP_TYPED_TO_FLT:
         {
            mVM->printf(0, "%i: OP_TYPED_TO_FLT", ip - 1);
            break;
         }
         case OP_TYPED_TO_UINT:
         {
            mVM->printf(0, "%i: OP_TYPED_TO_UINT", ip - 1);
            break;
         }
         case OP_TYPED_TO_NONE:
         {
            mVM->printf(0, "%i: OP_TYPED_TO_NONE", ip - 1);
            break;
         }
         case OP_TYPED_OP:
         {
            // Performs op on current two items on StringStack
            // i.e. stack-2 OP stack-1 / left OP right
            U32 opID = code[ip];
            mVM->printf(0, "%i: OP_TYPED_OP op=%i", ip - 1, opID);
            ++ ip;
            break;
         }
         case OP_TYPED_UNARY_OP:
         {
            // Performs op on item on StringStack
            U32 opID = code[ip];
            mVM->printf(0, "%i: OP_TYPED_UNARY_OP op=%i", ip - 1, opID);
            ++ ip;
            break;
         }
         case OP_TYPED_OP_REVERSE:
         {
            // Performs op on current two items on StringStack
            // i.e. stack-2 OP stack-1 / left OP right
            U32 opID = code[ip];
            mVM->printf(0, "%i: OP_TYPED_OP_REVERSE op=%i", ip - 1, opID);
            ++ ip;
            break;
         }
         case OP_SETCURFIELD_NONE:
         {
            // Unsets current field ref
            mVM->printf(0, "%i: OP_SETCURFIELD_NONE", ip - 1);
            break;
         }
            
         case OP_SETVAR_FROM_COPY:
         {
            // Sets cur var to copy var
            mVM->printf(0, "%i: OP_SETVAR_FROM_COPY", ip - 1);
            break;
         }
            
         case OP_LOADFIELD_VAR:
         {
            mVM->printf(0, "%i: OP_LOADFIELD_VAR", ip - 1);
            break;
         }
         case OP_SAVEFIELD_VAR:
         {
            mVM->printf(0, "%i: OP_SAVEFIELD_VAR", ip - 1);
            break;
         }
         case OP_SAVEVAR_MULTIPLE:
         {
            // Acts like a function call (i.e. relies on popping the frame)
            // Uses the vars current type.
            mVM->printf(0, "%i: OP_SAVEVAR_MULTIPLE", ip - 1);
            break;
         }
         case OP_SAVEFIELD_MULTIPLE:
         {
            // Pops n values from the StringStack
            // Uses the fields type.
            mVM->printf(0, "%i: OP_SAVEFIELD_MULTIPLE", ip - 1);
            break;
         }
         case OP_SET_DYNAMIC_TYPE_FROM_VAR:
         {
            mVM->printf(0, "%i: OP_SET_DYNAMIC_TYPE_FROM_VAR", ip - 1);
            break;
         }

         case OP_SET_DYNAMIC_TYPE_FROM_FIELD:
         {
            mVM->printf(0, "%i: OP_SET_DYNAMIC_TYPE_FROM_FIELD", ip - 1);
            break;
         }

         case OP_SET_DYNAMIC_TYPE_FROM_ID:
         {
            U32 typeID = code[ip];
            mVM->printf(0, "%i: OP_SET_DYNAMIC_TYPE_FROM_ID %i", ip - 1, typeID);
            ip++;
            break;
         }

         case OP_SET_DYNAMIC_TYPE_TO_NULL:
         {
            mVM->printf(0, "%i: OP_SET_DYNAMIC_TYPE_TO_NULL", ip - 1);
            break;
         }
         
         case OP_SETCURVAR_TYPE:
         {
            U32 typeId = code[ip++];
            mVM->printf(0, "%i: OP_SETCURVAR_TYPE (type=%s(%i)", ip - 1, getTypeName(typeId), typeId);
            break;
         }
            
         default:
            mVM->printf(0, "%i: !!INVALID!!", ip - 1 );
            break;
      }
   }
}
