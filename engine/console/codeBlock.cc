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
#include "console/simpleLexer.h"
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

using namespace Compiler;

//-------------------------------------------------------------------------

CodeBlock::CodeBlock(KorkApi::VmInternal* vm)
{
   globalStrings = NULL;
   functionStrings = NULL;
   functionStringsMaxLen = 0;
   globalStringsMaxLen = 0;
   globalFloats = NULL;
   functionFloats = NULL;
   lineBreakPairs = NULL;
   breakList = NULL;
   breakListSize = 0;
   
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
   
   if(name)
      removeFromCodeList();
   delete[] const_cast<char*>(globalStrings);
   delete[] const_cast<char*>(functionStrings);
   delete[] globalFloats;
   delete[] functionFloats;
   delete[] code;
   delete[] breakList;
}

//-------------------------------------------------------------------------

void CodeBlock::addToCodeList()
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
   line = p[found].instLine >> 8;
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
   
   breakList = new U32[size];
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

bool CodeBlock::read(StringTableEntry fileName, bool readVersion, Stream &st)
{
   const StringTableEntry exePath = Platform::getMainDotCsDir();
   const StringTableEntry cwd = Platform::getCurrentDirectory();

   if (readVersion)
   {
      U32 version = 0;
      st.read(&version);
      if (version != KorkApi::DSOVersion)
      {
         return false;
      }
   }
   
   name = fileName;
   
   if(fileName)
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
   addToCodeList();
   
   U32 globalSize,size,i;
   st.read(&size);
   if(size)
   {
      globalStrings = new char[size];
      globalStringsMaxLen = size;
      st.read(size, globalStrings);
   }
   globalSize = size;
   st.read(&size);
   if(size)
   {
      functionStrings = new char[size];
      functionStringsMaxLen = size;
      st.read(size, functionStrings);
   }
   st.read(&size);
   if(size)
   {
      globalFloats = new F64[size];
      for(U32 i = 0; i < size; i++)
         st.read(&globalFloats[i]);
   }
   st.read(&size);
   if(size)
   {
      functionFloats = new F64[size];
      for(U32 i = 0; i < size; i++)
         st.read(&functionFloats[i]);
   }
   U32 codeSize;
   st.read(&codeSize);
   st.read(&lineBreakPairCount);
   
   U32 totSize = codeSize + lineBreakPairCount * 2;
   code = new U32[totSize];
   
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
   U32 identCount;
   st.read(&identCount);
   while(identCount--)
   {
      U32 offset;
      st.read(&offset);
      StringTableEntry ste;
      if(offset < globalSize)
         ste = StringTable->insert(globalStrings + offset);
      else
         ste = StringTable->EmptyString;
      
      U32 count;
      st.read(&count);
      while(count--)
      {
         U32 ip;
         st.read(&ip);
#ifdef TORQUE_64
         *(U64*)(code+ip) = (U64)ste;
#else
         code[ip] = (U32)ste;
#endif
      }
   }
   
   if(lineBreakPairCount)
      calcBreakList();
   
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
   
   SimpleLexer::Tokenizer lex(StringTable, inScript, fileName);
   SimpleParser::ASTGen astGen(&lex, mVM->mCompilerResources);
   
   try
   {
      astGen.processTokens();
      rootNode = astGen.parseProgram();
   }
   catch (SimpleParser::TokenError& e)
   {
      mVM->printf(0, "Error parsing (%s :: %s)", e.what(), lex.toString(e.token()).c_str());
   }
   
   if(!rootNode)
   {
      delete this;
      return "";
   }
   
   if(!rootNode)
   {
      mVM->mCompilerResources->consoleAllocReset();
      return false;
   }
   
   // Reset all our value tables...
   mVM->mCompilerResources->resetTables();
   
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
   codeStream.emitCodeStream(&codeSize, &code, &lineBreakPairs);
   
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
   
   mVM->mCompilerResources->getIdentTable().write(st);
   
   mVM->mCompilerResources->consoleAllocReset();
   
   return true;
}

const char *CodeBlock::compileExec(StringTableEntry fileName, const char *inString, bool noCalls, int setFrame)
{
   // Check for a UTF8 script file
   char *string;
   chompUTF8BOM( inString, &string );

   mVM->mCompilerResources->STEtoCode = &Compiler::evalSTEtoCode;
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
   
   if(name)
      addToCodeList();
   
   StmtNode* rootNode = NULL;
   
   SimpleLexer::Tokenizer lex(StringTable, inString, fileName ? fileName : "");
   SimpleParser::ASTGen astGen(&lex, mVM->mCompilerResources);
   
   try
   {
      astGen.processTokens();
      rootNode = astGen.parseProgram();
   }
   catch (SimpleParser::TokenError& e)
   {
      mVM->printf(0, "Error parsing (%s :: %s)", e.what(), lex.toString(e.token()).c_str());
   }
   
   if(!rootNode)
   {
      delete this;
      return "";
   }
   
   mVM->mCompilerResources->resetTables();
   
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
   
   codeStream.emit(OP_RETURN);
   codeStream.emitCodeStream(&codeSize, &code, &lineBreakPairs);
   
   mVM->mCompilerResources->consoleAllocReset();
   
   if(lineBreakPairCount && fileName)
      calcBreakList();
   
   if(lastIp+1 != codeSize)
   {
      mVM->printf(0, "precompile size mismatch");
   }
   
   return exec(0, fileName, NULL, 0, 0, noCalls, NULL, setFrame);
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
      delete this;
}

//-------------------------------------------------------------------------

void CodeBlock::dumpInstructions( U32 startIp, bool upToReturn )
{
   U32 ip = startIp;
   U32 endFuncIp = 0;
   bool inFunction = false;
   
   while( ip < codeSize )
   {
      if (ip > endFuncIp)
      {
         inFunction = false;
      }
      
      switch( code[ ip ++ ] )
      {
            
         case OP_FUNC_DECL:
         {
            StringTableEntry fnName       = CodeToSTE(NULL, code, ip);
            StringTableEntry fnNamespace  = CodeToSTE(NULL, code, ip+2);
            StringTableEntry fnPackage    = CodeToSTE(NULL, code, ip+4);
            bool hasBody = bool(code[ip+6]);
            U32 newIp = code[ ip + 7 ];
            U32 argc = code[ ip + 8 ];
            endFuncIp = newIp;
            
            printf( "%i: OP_FUNC_DECL name=%s nspace=%s package=%s hasbody=%i newip=%i argc=%i",
               ip - 1, fnName, fnNamespace, fnPackage, hasBody, newIp, argc );
               
            // Skip args.
                           
            ip += 9 + (argc * 2);
            inFunction = true;
            break;
         }
            
         case OP_CREATE_OBJECT:
         {
            StringTableEntry objParent = CodeToSTE(NULL, code, ip);
            bool isDataBlock =          code[ip + 2];
            bool isInternal  =          code[ip + 3];
            bool isSingleton =          code[ip + 4];
            U32  lineNumber  =          code[ip + 5];
            U32 failJump     =          code[ip + 6];
            
            printf( "%i: OP_CREATE_OBJECT objParent=%s isDataBlock=%i isInternal=%i isSingleton=%i lineNumber=%i failJump=%i",
               ip - 1, objParent, isDataBlock, isInternal, isSingleton, lineNumber, failJump );

            ip += 7;
            break;
         }

         case OP_ADD_OBJECT:
         {
            bool placeAtRoot = code[ip++];
            printf( "%i: OP_ADD_OBJECT placeAtRoot=%i", ip - 1, placeAtRoot );
            break;
         }
         
         case OP_END_OBJECT:
         {
            bool placeAtRoot = code[ip++];
            printf( "%i: OP_END_OBJECT placeAtRoot=%i", ip - 1, placeAtRoot );
            break;
         }
         
         case OP_FINISH_OBJECT:
         {
            printf( "%i: OP_FINISH_OBJECT", ip - 1 );
            break;
         }
         
         case OP_JMPIFFNOT:
         {
            printf( "%i: OP_JMPIFFNOT ip=%i", ip - 1, code[ ip ] );
            ++ ip;
            break;
         }
         
         case OP_JMPIFNOT:
         {
            printf( "%i: OP_JMPIFNOT ip=%i", ip - 1, code[ ip ] );
            ++ ip;
            break;
         }
         
         case OP_JMPIFF:
         {
            printf( "%i: OP_JMPIFF ip=%i", ip - 1, code[ ip ] );
            ++ ip;
            break;
         }

         case OP_JMPIF:
         {
            printf( "%i: OP_JMPIF ip=%i", ip - 1, code[ ip ] );
            ++ ip;
            break;
         }

         case OP_JMPIFNOT_NP:
         {
            printf( "%i: OP_JMPIFNOT_NP ip=%i", ip - 1, code[ ip ] );
            ++ ip;
            break;
         }

         case OP_JMPIF_NP:
         {
            printf( "%i: OP_JMPIF_NP ip=%i", ip - 1, code[ ip ] );
            ++ ip;
            break;
         }

         case OP_JMP:
         {
            printf( "%i: OP_JMP ip=%i", ip - 1, code[ ip ] );
            ++ ip;
            break;
         }

         case OP_RETURN:
         {
            printf( "%i: OP_RETURN", ip - 1 );
            
            if( upToReturn )
               return;
               
            break;
         }

         case OP_RETURN_VOID:
         {
            printf( "%i: OP_RETURNVOID", ip - 1 );

            if( upToReturn )
               return;

            break;
         }

         case OP_RETURN_UINT:
         {
            printf( "%i: OP_RETURNUINT", ip - 1 );

            if( upToReturn )
               return;

            break;
         }

         case OP_RETURN_FLT:
         {
            printf( "%i: OP_RETURNFLT", ip - 1 );

            if( upToReturn )
               return;

            break;
         }

         case OP_CMPEQ:
         {
            printf( "%i: OP_CMPEQ", ip - 1 );
            break;
         }

         case OP_CMPGR:
         {
            printf( "%i: OP_CMPGR", ip - 1 );
            break;
         }

         case OP_CMPGE:
         {
            printf( "%i: OP_CMPGE", ip - 1 );
            break;
         }

         case OP_CMPLT:
         {
            printf( "%i: OP_CMPLT", ip - 1 );
            break;
         }

         case OP_CMPLE:
         {
            printf( "%i: OP_CMPLE", ip - 1 );
            break;
         }

         case OP_CMPNE:
         {
            printf( "%i: OP_CMPNE", ip - 1 );
            break;
         }

         case OP_XOR:
         {
            printf( "%i: OP_XOR", ip - 1 );
            break;
         }

         case OP_MOD:
         {
            printf( "%i: OP_MOD", ip - 1 );
            break;
         }

         case OP_BITAND:
         {
            printf( "%i: OP_BITAND", ip - 1 );
            break;
         }

         case OP_BITOR:
         {
            printf( "%i: OP_BITOR", ip - 1 );
            break;
         }

         case OP_NOT:
         {
            printf( "%i: OP_NOT", ip - 1 );
            break;
         }

         case OP_NOTF:
         {
            printf( "%i: OP_NOTF", ip - 1 );
            break;
         }

         case OP_ONESCOMPLEMENT:
         {
            printf( "%i: OP_ONESCOMPLEMENT", ip - 1 );
            break;
         }

         case OP_SHR:
         {
            printf( "%i: OP_SHR", ip - 1 );
            break;
         }

         case OP_SHL:
         {
            printf( "%i: OP_SHL", ip - 1 );
            break;
         }

         case OP_AND:
         {
            printf( "%i: OP_AND", ip - 1 );
            break;
         }

         case OP_OR:
         {
            printf( "%i: OP_OR", ip - 1 );
            break;
         }

         case OP_ADD:
         {
            printf( "%i: OP_ADD", ip - 1 );
            break;
         }

         case OP_SUB:
         {
            printf( "%i: OP_SUB", ip - 1 );
            break;
         }

         case OP_MUL:
         {
            printf( "%i: OP_MUL", ip - 1 );
            break;
         }

         case OP_DIV:
         {
            printf( "%i: OP_DIV", ip - 1 );
            break;
         }

         case OP_NEG:
         {
            printf( "%i: OP_NEG", ip - 1 );
            break;
         }

         case OP_SETCURVAR:
         {
            StringTableEntry var = CodeToSTE(NULL, code, ip);
            
            printf( "%i: OP_SETCURVAR var=%s", ip - 1, var );
            ip += 2;
            break;
         }
         
         case OP_SETCURVAR_CREATE:
         {
            StringTableEntry var = CodeToSTE(NULL, code, ip);
            
            printf( "%i: OP_SETCURVAR_CREATE var=%s", ip - 1, var );
            ip += 2;
            break;
         }
         
         case OP_SETCURVAR_ARRAY:
         {
            printf( "%i: OP_SETCURVAR_ARRAY", ip - 1 );
            break;
         }
         
         case OP_SETCURVAR_ARRAY_CREATE:
         {
            printf( "%i: OP_SETCURVAR_ARRAY_CREATE", ip - 1 );
            break;
         }
         
         case OP_LOADVAR_UINT:
         {
            printf( "%i: OP_LOADVAR_UINT", ip - 1 );
            break;
         }
         
         case OP_LOADVAR_FLT:
         {
            printf( "%i: OP_LOADVAR_FLT", ip - 1 );
            break;
         }

         case OP_LOADVAR_STR:
         {
            printf( "%i: OP_LOADVAR_STR", ip - 1 );
            break;
         }

         case OP_LOADVAR_VAR:
         {
            printf( "%i: OP_LOADVAR_VAR", ip - 1 );
            break;
         }

         case OP_SAVEVAR_UINT:
         {
            printf( "%i: OP_SAVEVAR_UINT", ip - 1 );
            break;
         }

         case OP_SAVEVAR_FLT:
         {
            printf( "%i: OP_SAVEVAR_FLT", ip - 1 );
            break;
         }

         case OP_SAVEVAR_STR:
         {
            printf( "%i: OP_SAVEVAR_STR", ip - 1 );
            break;
         }

         case OP_SAVEVAR_VAR:
         {
            printf( "%i: OP_SAVEVAR_VAR", ip - 1 );
            break;
         }

         case OP_SETCUROBJECT:
         {
            printf( "%i: OP_SETCUROBJECT", ip - 1 );
            break;
         }

         case OP_SETCUROBJECT_NEW:
         {
            printf( "%i: OP_SETCUROBJECT_NEW", ip - 1 );
            break;
         }
         
         case OP_SETCUROBJECT_INTERNAL:
         {
            printf( "%i: OP_SETCUROBJECT_INTERNAL", ip - 1 );
            ++ ip;
            break;
         }
         
         case OP_SETCURFIELD:
         {
            StringTableEntry curField = CodeToSTE(NULL, code, ip);
            printf( "%i: OP_SETCURFIELD field=%s", ip - 1, curField );
            ip += 2;
            break;
         }
         
         case OP_SETCURFIELD_ARRAY:
         {
            printf( "%i: OP_SETCURFIELD_ARRAY", ip - 1 );
            break;
         }

         case OP_SETCURFIELD_TYPE:
         {
            U32 type = code[ ip ];
            printf( "%i: OP_SETCURFIELD_TYPE type=%i", ip - 1, type );
            ++ ip;
            break;
         }

         case OP_LOADFIELD_UINT:
         {
            printf( "%i: OP_LOADFIELD_UINT", ip - 1 );
            break;
         }

         case OP_LOADFIELD_FLT:
         {
            printf( "%i: OP_LOADFIELD_FLT", ip - 1 );
            break;
         }

         case OP_LOADFIELD_STR:
         {
            printf( "%i: OP_LOADFIELD_STR", ip - 1 );
            break;
         }

         case OP_SAVEFIELD_UINT:
         {
            printf( "%i: OP_SAVEFIELD_UINT", ip - 1 );
            break;
         }

         case OP_SAVEFIELD_FLT:
         {
            printf( "%i: OP_SAVEFIELD_FLT", ip - 1 );
            break;
         }

         case OP_SAVEFIELD_STR:
         {
            printf( "%i: OP_SAVEFIELD_STR", ip - 1 );
            break;
         }

         case OP_STR_TO_UINT:
         {
            printf( "%i: OP_STR_TO_UINT", ip - 1 );
            break;
         }

         case OP_STR_TO_FLT:
         {
            printf( "%i: OP_STR_TO_FLT", ip - 1 );
            break;
         }

         case OP_STR_TO_NONE:
         {
            printf( "%i: OP_STR_TO_NONE", ip - 1 );
            break;
         }

         case OP_FLT_TO_UINT:
         {
            printf( "%i: OP_FLT_TO_UINT", ip - 1 );
            break;
         }

         case OP_FLT_TO_STR:
         {
            printf( "%i: OP_FLT_TO_STR", ip - 1 );
            break;
         }

         case OP_FLT_TO_NONE:
         {
            printf( "%i: OP_FLT_TO_NONE", ip - 1 );
            break;
         }

         case OP_UINT_TO_FLT:
         {
            printf( "%i: OP_SAVEFIELD_STR", ip - 1 );
            break;
         }

         case OP_UINT_TO_STR:
         {
            printf( "%i: OP_UINT_TO_STR", ip - 1 );
            break;
         }

         case OP_UINT_TO_NONE:
         {
            printf( "%i: OP_UINT_TO_NONE", ip - 1 );
            break;
         }

         case OP_COPYVAR_TO_NONE:
         {
            printf( "%i: OP_COPYVAR_TO_NONE", ip - 1 );
            break;
         }

         case OP_LOADIMMED_UINT:
         {
            U32 val = code[ ip ];
            printf( "%i: OP_LOADIMMED_UINT val=%i", ip - 1, val );
            ++ ip;
            break;
         }

         case OP_LOADIMMED_FLT:
         {
            F64 val = (inFunction ? functionFloats : globalFloats)[ code[ ip ] ];
            printf( "%i: OP_LOADIMMED_FLT val=%f", ip - 1, val );
            ++ ip;
            break;
         }

         case OP_TAG_TO_STR:
         {
            const char* str = (inFunction ? functionStrings : globalStrings) + code[ ip ];
            printf( "%i: OP_TAG_TO_STR str=%s", ip - 1, str );
            ++ ip;
            break;
         }
         
         case OP_LOADIMMED_STR:
         {
            const char* str = (inFunction ? functionStrings : globalStrings) + code[ ip ];
            printf( "%i: OP_LOADIMMED_STR str=%s", ip - 1, str );
            ++ ip;
            break;
         }

         case OP_DOCBLOCK_STR:
         {
            const char* str = (inFunction ? functionStrings : globalStrings) + code[ ip ];
            printf( "%i: OP_DOCBLOCK_STR str=%s", ip - 1, str );
            ++ ip;
            break;
         }
         
         case OP_LOADIMMED_IDENT:
         {
            StringTableEntry str = CodeToSTE(NULL, code, ip);
            printf( "%i: OP_LOADIMMED_IDENT str=%s", ip - 1, str );
            ip += 2;
            break;
         }

         case OP_CALLFUNC_RESOLVE:
         {
            StringTableEntry fnNamespace = CodeToSTE(NULL, code, ip+2);
            StringTableEntry fnName      = CodeToSTE(NULL, code, ip);
            U32 callType = code[ip+2];

            printf( "%i: OP_CALLFUNC_RESOLVE name=%s nspace=%s callType=%s", ip - 1, fnName, fnNamespace,
               callType == FuncCallExprNode::FunctionCall ? "FunctionCall"
                  : callType == FuncCallExprNode::MethodCall ? "MethodCall" : "ParentCall" );
            
            ip += 5;
            break;
         }
         
         case OP_CALLFUNC:
         {
            StringTableEntry fnNamespace = CodeToSTE(NULL, code, ip+2);
            StringTableEntry fnName      = CodeToSTE(NULL, code, ip);
            U32 callType = code[ip+4];

            printf( "%i: OP_CALLFUNC name=%s nspace=%s callType=%s", ip - 1, fnName, fnNamespace,
               callType == FuncCallExprNode::FunctionCall ? "FunctionCall"
                  : callType == FuncCallExprNode::MethodCall ? "MethodCall" : "ParentCall" );
            
            ip += 5;
            break;
         }

         case OP_ADVANCE_STR:
         {
            printf( "%i: OP_ADVANCE_STR", ip - 1 );
            break;
         }

         case OP_ADVANCE_STR_APPENDCHAR:
         {
            char ch = code[ ip ];
            printf( "%i: OP_ADVANCE_STR_APPENDCHAR char=%c", ip - 1, ch );
            ++ ip;
            break;
         }

         case OP_ADVANCE_STR_COMMA:
         {
            printf( "%i: OP_ADVANCE_STR_COMMA", ip - 1 );
            break;
         }

         case OP_ADVANCE_STR_NUL:
         {
            printf( "%i: OP_ADVANCE_STR_NUL", ip - 1 );
            break;
         }

         case OP_REWIND_STR:
         {
            printf( "%i: OP_REWIND_STR", ip - 1 );
            break;
         }

         case OP_TERMINATE_REWIND_STR:
         {
            printf( "%i: OP_TERMINATE_REWIND_STR", ip - 1 );
            break;
         }

         case OP_COMPARE_STR:
         {
            printf( "%i: OP_COMPARE_STR", ip - 1 );
            break;
         }

         case OP_PUSH:
         {
            printf( "%i: OP_PUSH", ip - 1 );
            break;
         }

         case OP_PUSH_UINT:
         {
            printf( "%i: OP_PUSH_UINT", ip - 1 );
            break;
         }

         case OP_PUSH_FLT:
         {
            printf( "%i: OP_PUSH_FLT", ip - 1 );
            break;
         }

         case OP_PUSH_VAR:
         {
            printf( "%i: OP_PUSH_VAR", ip - 1 );
            break;
         }

         case OP_PUSH_FRAME:
         {
            printf( "%i: OP_PUSH_FRAME", ip - 1 );
            break;
         }

         case OP_ASSERT:
         {
            const char* message = (inFunction ? functionStrings : globalStrings) + code[ ip ];
            printf( "%i: OP_ASSERT message=%s", ip - 1, message );
            ++ ip;
            break;
         }

         case OP_BREAK:
         {
            printf( "%i: OP_BREAK", ip - 1 );
            break;
         }
         
         case OP_ITER_BEGIN:
         {
            StringTableEntry varName = CodeToSTE(NULL, code, ip);
            U32 failIp = code[ ip + 2 ];
            
            printf( "%i: OP_ITER_BEGIN varName=%s failIp=%i", ip - 1, varName, failIp );

            ip += 3;
            break;
         }

         case OP_ITER_BEGIN_STR:
         {
            StringTableEntry varName = CodeToSTE(NULL, code, ip);
            U32 failIp = code[ ip + 2 ];
            
            printf( "%i: OP_ITER_BEGIN varName=%s failIp=%i", ip - 1, varName, failIp );

            ip += 3;
            break;
         }
         
         case OP_ITER:
         {
            U32 breakIp = code[ ip ];
            
            printf( "%i: OP_ITER breakIp=%i", ip - 1, breakIp );

            ++ ip;
            break;
         }
         
         case OP_ITER_END:
         {
            printf( "%i: OP_ITER_END", ip - 1 );
            break;
         }

         default:
            printf( "%i: !!INVALID!!", ip - 1 );
            break;
      }
   }
}
