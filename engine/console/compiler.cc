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

#include "embed/api.h"
#include "embed/internalApi.h"
#include "console/ast.h"


#include "console/consoleInternal.h"
#include "console/compiler.h"
#include "console/telnetDebugger.h"

namespace Compiler
{

   void evalSTEtoCode(Resources* res, StringTableEntry ste, U32 ip, U32 *ptr)
   {
   }

   void compileSTEtoCode(Resources* res, StringTableEntry ste, U32 ip, U32 *ptr)
   {
      U32 idx = 0;
      
      if(ste)
      {
         idx = res->getIdentTable().add(ste, ip)+1;
      }
      
      *ptr = idx;
      *(ptr+1) = 0;
   }

   F64 consoleStringToNumber(const char *str, StringTableEntry file, U32 line)
   {
      F64 val = dAtof(str);
      if(val != 0)
         return val;
      else if(!dStricmp(str, "true"))
         return 1;
      else if(!dStricmp(str, "false"))
         return 0;
      else if(file)
      {
       // TOFIX  Con::warnf(ConsoleLogEntry::General, "%s (%d): string always evaluates to 0.", file, line);
         return 0;
      }
      return 0;
   }

   //------------------------------------------------------------

   void Resources::precompileIdent(StringTableEntry ident)
   {
      if(ident)
         globalStringTable.add(ident);
   }

   S32 Resources::precompileType(StringTableEntry ident)
   {
      return ident ? getTypeTable().addNoAddress(ident) : -1;
   }

   void Resources::resetTables()
   {
      setCurrentStringTable(&globalStringTable);
      setCurrentFloatTable(&globalFloatTable);
      getGlobalFloatTable().reset();
      getGlobalStringTable().reset();
      getFunctionFloatTable().reset();
      getFunctionStringTable().reset();
      getIdentTable().reset();
      getTypeTable().reset();

      globalVarTypes.reset();

      for (U32 i=0; i<VarTypeStackSize; i++)
      {
         localVarTypes[i].reset();
      }

      curLocalVarStackPos = 0;
   }

   void Resources::pushLocalVarContext()
   {
      if (curLocalVarStackPos == VarTypeStackSize-1)
      {
         return;
      }
      
      curLocalVarStackPos++;
   }

   void Resources::popLocalVarContext()
   {
      if (curLocalVarStackPos > 0)
      {
         curLocalVarStackPos--;
      }

      localVarTypes[curLocalVarStackPos].reset();
   }

   VarTypeTableEntry* Resources::getVarInfo(StringTableEntry varName, StringTableEntry typeName)
   {
      VarTypeTableEntry* tt = NULL;

      if (varName[0] == '$')
      {
         tt = globalVarTypes.lookupVar(varName);
      }
      else if (curLocalVarStackPos == 0)
      {
         AssertFatal(false, "Bad variable type stack\n");
      }
      else
      {
         tt = localVarTypes[curLocalVarStackPos-1].lookupVar(varName);
      }

      if (tt &&
         typeName != NULL)
      {
         if (tt->typeName &&
             tt->typeName != typeName)
         {
            //printf("Variable type redefined: %s vs %s\n");
         }
         
         tt->typeName = typeName;
         tt->typeId = allowTypes ? precompileType(typeName) : -1;
      }

      return tt;
   }

   VarTypeTableEntry* VarTypeTable::lookupVar(StringTableEntry name)
   {
      for (VarTypeTableEntry* entry = table; entry; entry = entry->next)
      {
         if (entry->name == name)
         {
            return entry;
         }
      }

      VarTypeTableEntry* newEntry = (VarTypeTableEntry *) res->consoleAlloc(sizeof(VarTypeTableEntry));
      newEntry->next = table;
      table = newEntry;
      
      newEntry->name = name;
      newEntry->typeName = NULL;
      newEntry->typeId = -1;
      return newEntry;
   }

   void VarTypeTable::reset()
   {
      table = NULL;
   }

}

//-------------------------------------------------------------------------

using namespace Compiler;

//-------------------------------------------------------------------------


U32 CompilerStringTable::add(const char *str, bool caseSens, bool tag)
{
   // Is it already in?
   Entry **walk;
   for(walk = &list; *walk; walk = &((*walk)->next))
   {
      if((*walk)->tag != tag)
         continue;

      if(caseSens)
      {
         if(!dStrcmp((*walk)->string, str))
            return (*walk)->start;
      }
      else
      {
         if(!dStricmp((*walk)->string, str))
            return (*walk)->start;
      }
   }

   // Write it out.
   Entry *newStr = (Entry *) res->consoleAlloc(sizeof(Entry));
   *walk = newStr;
   newStr->next = NULL;
   newStr->start = totalLen;
   U32 len = dStrlen(str) + 1;
   if(tag && len < 7) // alloc space for the numeric tag 1 for tag, 5 for # and 1 for nul
      len = 7;
   totalLen += len;
   newStr->string = (char *) res->consoleAlloc(dAlignSize(len, 8));
   newStr->len = len;
   newStr->tag = tag;
   memcpy(newStr->string, str, len);
   return newStr->start;
}

U32 CompilerStringTable::addIntString(U32 value)
{
   dSprintf(buf, sizeof(buf), "%d", value);
   return add(buf);
}

U32 CompilerStringTable::addFloatString(F64 value)
{
   dSprintf(buf, sizeof(buf), "%g", value);
   return add(buf);
}

void CompilerStringTable::reset()
{
   list = NULL;
   totalLen = 0;
}

char *CompilerStringTable::build()
{
   char *ret = KorkApi::VMem::NewArray<char>(totalLen);
   for(Entry *walk = list; walk; walk = walk->next)
      dStrcpy(ret + walk->start, walk->string);
   return ret;
}

void CompilerStringTable::write(Stream &st)
{
   st.write(totalLen);
   for(Entry *walk = list; walk; walk = walk->next)
      st.write(walk->len, walk->string);
}

//------------------------------------------------------------

U32 CompilerFloatTable::add(F64 value)
{
   Entry **walk;
   U32 i = 0;
   for(walk = &list; *walk; walk = &((*walk)->next), i++)
      if(value == (*walk)->val)
         return i;
   Entry *newFloat = (Entry *) res->consoleAlloc(sizeof(Entry));
   newFloat->val = value;
   newFloat->next = NULL;
   count++;
   *walk = newFloat;
   return count-1;
}
void CompilerFloatTable::reset()
{
   list = NULL;
   count = 0;
}
F64 *CompilerFloatTable::build()
{
   F64 *ret = KorkApi::VMem::NewArray<F64>(count);
   U32 i = 0;
   for(Entry *walk = list; walk; walk = walk->next, i++)
      ret[i] = walk->val;
   return ret;
}

void CompilerFloatTable::write(Stream &st)
{
   st.write(count);
   for(Entry *walk = list; walk; walk = walk->next)
      st.write(walk->val);
}

//------------------------------------------------------------

void CompilerIdentTable::reset()
{
   list = NULL;
   tail = NULL;
   numIdentStrings = 0;
}

U32 CompilerIdentTable::addNoAddress(StringTableEntry ste)
{
   U32 index = res->globalStringTable.add(ste, false);
   
   FullEntry* patchEntry = NULL;
   
   U32 elementIndex = 0;
   for(FullEntry *walk = list; walk; walk = walk->next)
   {
      if(walk->offset == index)
      {
         patchEntry = walk;
         break;
      }
      elementIndex++;
   }
   
   if (patchEntry == NULL)
   {
      patchEntry = (FullEntry *) res->consoleAlloc(sizeof(FullEntry));
      patchEntry->patch = NULL;
      patchEntry->steName = ste;
      patchEntry->offset = index;
      patchEntry->next = NULL;
      
      if (tail == NULL)
      {
         list = patchEntry;
         tail = patchEntry;
      }
      else
      {
         tail->next = patchEntry;
         tail = patchEntry;
      }
      
      elementIndex = numIdentStrings++;
   }
   
   return elementIndex;

}

U32 CompilerIdentTable::add(StringTableEntry ste, U32 ip)
{
   U32 index = res->globalStringTable.add(ste, false);
   Patch* newPatch = (Patch*)res->consoleAlloc(sizeof(Patch));
   newPatch->ip = ip;
   
   FullEntry* patchEntry = NULL;
   
   U32 elementIndex = 0;
   for(FullEntry *walk = list; walk; walk = walk->next)
   {
      if(walk->offset == index)
      {
         patchEntry = walk;
         break;
      }
      elementIndex++;
   }
   
   if (patchEntry == NULL)
   {
      patchEntry = (FullEntry *) res->consoleAlloc(sizeof(FullEntry));
      patchEntry->patch = NULL;
      patchEntry->steName = ste;
      patchEntry->offset = index;
      patchEntry->next = NULL;
      
      if (tail == NULL)
      {
         list = patchEntry;
         tail = patchEntry;
      }
      else
      {
         tail->next = patchEntry;
         tail = patchEntry;
      }
      
      elementIndex = numIdentStrings++;
   }
   
   // Add to patch list
   newPatch->next = patchEntry->patch;
   patchEntry->patch = newPatch;
   patchEntry->numInstances++;
   
   return elementIndex;
}

void CompilerIdentTable::write(Stream &st)
{
   FullEntry * walk;
   st.write(numIdentStrings);
   
   for(walk = list; walk; walk = walk->next)
   {
      st.write(walk->offset);
      st.write(walk->numInstances);
      
      for (Patch* el = walk->patch; el; el = el->next)
      {
         st.write(el->ip);
      }
   }
}

void CompilerIdentTable::build(StringTableEntry** strings,  U32** stringOffsets, U32* numStrings)
{
   *numStrings = numIdentStrings;
   *stringOffsets = KorkApi::VMem::NewArray<U32>(numIdentStrings);
   *strings = KorkApi::VMem::NewArray<StringTableEntry>(numIdentStrings);
   
   U32 i = 0;
   for(FullEntry* walk = list; walk; walk = walk->next)
   {
      (*stringOffsets)[i] = walk->offset;
      (*strings)[i++] = walk->steName;
   }
}

U32 CompilerIdentTable::append(CompilerIdentTable &other)
{
   U32 offset = numIdentStrings;
   
   if (other.list == NULL)
   {
      return numIdentStrings;
   }
   
   if (list == NULL)
   {
      list = other.list;
      numIdentStrings = other.numIdentStrings;
      return 0;
   }
   
   tail->next = other.list;
   tail = other.tail;
   numIdentStrings += other.numIdentStrings;
   return offset;
}

//-------------------------------------------------------------------------
  
U8 *CodeStream::allocCode(U32 sz)
{
   U8 *ptr = NULL;
   if (mCodeHead)
   {
      const U32 bytesLeft = BlockSize - mCodeHead->size;
      if (bytesLeft > sz)
      {
         ptr = mCodeHead->data + mCodeHead->size;
         mCodeHead->size += sz;
         return ptr;
      }
   }
   
   CodeData *data = KorkApi::VMem::New<CodeData>();
   data->data = KorkApi::VMem::NewArray<U8>(BlockSize);
   data->size = sz;
   data->next = NULL;
   
   if (mCodeHead)
      mCodeHead->next = data;
   mCodeHead = data;
   if (mCode == NULL)
      mCode = data;
   return data->data;
}
  
//-------------------------------------------------------------------------
  
void CodeStream::fixLoop(U32 loopBlockStart, U32 breakPoint, U32 continuePoint)
{
   AssertFatal(mFixStack.size() > 0, "Fix stack mismatch");
   
   U32 fixStart = mFixStack[mFixStack.size()-1];
   for (U32 i=fixStart; i<mFixList.size(); i += 2)
   {
      FixType type = (FixType)mFixList[i+1];
      
      U32 fixedIp = 0;
      bool valid = true;
      
      switch (type)
      {
         case FIXTYPE_LOOPBLOCKSTART:
            fixedIp = loopBlockStart;
            break;
         case FIXTYPE_BREAK:
            fixedIp = breakPoint;
            break;
         case FIXTYPE_CONTINUE:
            fixedIp = continuePoint;
            break;
         default:
            //Con::warnf("Address %u fixed as %u", mFixList[i], mFixList[i+1]);
            valid = false;
            break;
      }
      
      if (valid)
      {
         patch(mFixList[i], fixedIp);
      }
   }
}

//-------------------------------------------------------------------------
  
void CodeStream::emitCodeStream(U32 *size, U32 **stream, U32 **lineBreaks, U32* numFuncCalls, void*** funcCallsPtr)
{
   // Alloc stream
   U32 numLineBreaks = getNumLineBreaks();
   *stream = KorkApi::VMem::NewArray<U32>(mCodePos + (numLineBreaks * 2));
   dMemset(*stream, '\0', mCodePos + (numLineBreaks * 2));
   *size = mCodePos;
   
   // Dump chunks & line breaks
   U32 outBytes = mCodePos * sizeof(U32);
   U8 *outPtr = *((U8**)stream);
   for (CodeData *itr = mCode; itr != NULL; itr = itr->next)
   {
      U32 bytesToCopy = itr->size > outBytes ? outBytes : itr->size;
      dMemcpy(outPtr, itr->data, bytesToCopy);
      outPtr += bytesToCopy;
      outBytes -= bytesToCopy;
   }
   
   *lineBreaks = *stream + mCodePos;
   std::copy(mBreakLines.begin(), mBreakLines.end(), *lineBreaks);
   
   // Dump func calls
   mNumFuncCalls++; // reserve 0
   *numFuncCalls = mNumFuncCalls;
   *funcCallsPtr = new void*[mNumFuncCalls];
   memset(*funcCallsPtr, '\0', sizeof(void*) * mNumFuncCalls);
   
   // Apply patches on top
   for (U32 i=0; i<mPatchList.size(); i++)
   {
      PatchEntry &e = mPatchList[i];
      (*stream)[e.addr] = e.value;
   }
}
  
//-------------------------------------------------------------------------
  
void CodeStream::reset()
{
   mCodePos = 0;
   mFixStack.clear();
   mFixLoopStack.clear();
   mFixList.clear();
   mBreakLines.clear();
   
   // Pop down to one code block
   CodeData *itr = mCode ? mCode->next : NULL;
   while (itr != NULL)
   {
      CodeData *next = itr->next;
      KorkApi::VMem::Delete(itr->data);
      KorkApi::VMem::Delete(itr);
      itr = next;
   }
   
   if (mCode)
   {
      mCode->size = 0;
      mCode->next = NULL;
      mCodeHead = mCode;
   }
}

