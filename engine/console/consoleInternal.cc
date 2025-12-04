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
#include "console/simpleLexer.h"
#include "console/ast.h"
#include "console/consoleNamespace.h"

#include "core/findMatch.h"
#include "console/consoleInternal.h"
#include "core/fileStream.h"
#include "console/compiler.h"
#include "core/escape.h"

//#define DEBUG_SPEW


#define ST_INIT_SIZE 15

//---------------------------------------------------------------
//
// Dictionary functions
//
//---------------------------------------------------------------

bool varCompare(const Dictionary::Entry* a, const Dictionary::Entry* b)
{
    return dStricmp(a->name, b->name) < 0;
}

void Dictionary::exportVariables(const char *varString, const char *fileName, bool append)
{
   const char *searchStr = varString;
   Vector<Entry *> sortList(__FILE__, __LINE__);
   
   for(S32 i = 0; i < mHashTable->size; i++)
   {
      Entry *walk = mHashTable->data[i];
      while(walk)
      {
         if(FindMatch::isMatch((char *) searchStr, (char *) walk->name))
            sortList.push_back(walk);
         
         walk = walk->nextEntry;
      }
   }
   
   if(!sortList.size())
      return;
   
   std::sort(sortList.begin(), sortList.end(), varCompare);
   
   Vector<Entry *>::iterator s;
   char expandBuffer[1024];
   FileStream strm;
   
   if(fileName)
   {
      if(!strm.open(fileName, append ? FileStream::ReadWrite : FileStream::Write))
      {
         mVm->printf(0, "Unable to open file '%s for writing.", fileName);
         return;
      }
      if(append)
         strm.setPosition(strm.getStreamSize());
   }
   
   char buffer[1024];
   const char *cat = fileName ? "\r\n" : "";
   
   for(s = sortList.begin(); s != sortList.end(); s++)
   {
      switch((*s)->mConsoleValue.typeId)
      {
         case KorkApi::ConsoleValue::TypeInternalUnsigned:
            dSprintf(buffer, sizeof(buffer), "%s = %d;%s", (*s)->name, (*s)->mConsoleValue.getInt(), cat);
            break;
         case KorkApi::ConsoleValue::TypeInternalNumber:
            dSprintf(buffer, sizeof(buffer), "%s = %g;%s", (*s)->name, (*s)->mConsoleValue.getFloat(), cat);
            break;
         default:
            expandEscape(expandBuffer, (const char*)(*s)->mConsoleValue.evaluatePtr(mVm->mAllocBase));
            dSprintf(buffer, sizeof(buffer), "%s = \"%s\";%s", (*s)->name, expandBuffer, cat);
            break;
      }
      if(fileName)
      {
         strm.write(dStrlen(buffer), buffer);
      }
      else
      {
         mVm->printf(0, "%s", buffer);
      }
   }
   if(fileName)
      strm.close();
}

void Dictionary::deleteVariables(const char *varString)
{
   const char *searchStr = varString;
   
   for(S32 i = 0; i < mHashTable->size; i++)
   {
      Entry *walk = mHashTable->data[i];
      while(walk)
      {
         Entry *matchedEntry = (FindMatch::isMatch((char *) searchStr, (char *) walk->name)) ? walk : NULL;
         walk = walk->nextEntry;
         if (matchedEntry)
            remove(matchedEntry); // assumes remove() is a stable remove (will not reorder entries on remove)
      }
   }
}

U32 HashPointer(StringTableEntry ptr)
{
   return (U32)(((dsize_t)ptr) >> 2);
}

Dictionary::Entry *Dictionary::lookup(StringTableEntry name)
{
   Entry *walk = mHashTable->data[HashPointer(name) % mHashTable->size];
   while(walk)
   {
      if(walk->name == name)
         return walk;
      else
         walk = walk->nextEntry;
   }
   
   return NULL;
}

Dictionary::Entry *Dictionary::add(StringTableEntry name)
{
   Entry *walk = mHashTable->data[HashPointer(name) % mHashTable->size];
   while(walk)
   {
      if(walk->name == name)
         return walk;
      else
         walk = walk->nextEntry;
   }
   Entry *ret;
   mHashTable->count++;
   
   if(mHashTable->count > mHashTable->size * 2)
   {
      Entry head(NULL), *walk;
      S32 i;
      walk = &head;
      walk->nextEntry = 0;
      for(i = 0; i < mHashTable->size; i++) {
         while(walk->nextEntry) {
            walk = walk->nextEntry;
         }
         walk->nextEntry = mHashTable->data[i];
      }
      delete[] mHashTable->data;
      mHashTable->size = mHashTable->size * 4 - 1;
      mHashTable->data = new Entry *[mHashTable->size];
      for(i = 0; i < mHashTable->size; i++)
         mHashTable->data[i] = NULL;
      walk = head.nextEntry;
      while(walk)
      {
         Entry *temp = walk->nextEntry;
         U32 idx = HashPointer(walk->name) % mHashTable->size;
         walk->nextEntry = mHashTable->data[idx];
         mHashTable->data[idx] = walk;
         walk = temp;
      }
   }
   
   ret = new Entry(name);
   U32 idx = HashPointer(name) % mHashTable->size;
   ret->nextEntry = mHashTable->data[idx];
   mHashTable->data[idx] = ret;
   return ret;
}

// deleteVariables() assumes remove() is a stable remove (will not reorder entries on remove)
void Dictionary::remove(Dictionary::Entry *ent)
{
   Entry **walk = &mHashTable->data[HashPointer(ent->name) % mHashTable->size];
   while(*walk != ent)
      walk = &((*walk)->nextEntry);
   
   *walk = (ent->nextEntry);
   clearEntry(ent);
   delete ent;
   mHashTable->count--;
}

Dictionary::Dictionary()
:  mHashTable( NULL )
{
}

Dictionary::Dictionary(KorkApi::VmInternal *state, Dictionary* ref)
:  mHashTable( NULL )
{
   setState(state,ref);
}

void Dictionary::setState(KorkApi::VmInternal *state, Dictionary* ref)
{
   mVm = state;
   
   if (ref)
      mHashTable = ref->mHashTable;
   else
   {
      mHashTable = new HashTableData;
      mHashTable->owner = this;
      mHashTable->count = 0;
      mHashTable->size = ST_INIT_SIZE;
      mHashTable->data = new Entry *[mHashTable->size];
      
      for(S32 i = 0; i < mHashTable->size; i++)
         mHashTable->data[i] = NULL;
   }
}

Dictionary::~Dictionary()
{
   if ( mHashTable->owner == this )
   {
      reset();
      delete [] mHashTable->data;
      delete mHashTable;
   }
}

void Dictionary::reset()
{
   S32 i;
   Entry *walk, *temp;
   
   for(i = 0; i < mHashTable->size; i++)
   {
      walk = mHashTable->data[i];
      while(walk)
      {
         temp = walk->nextEntry;
         clearEntry(walk);
         delete walk;
         walk = temp;
      }
      mHashTable->data[i] = NULL;
   }
   mHashTable->size = ST_INIT_SIZE;
   mHashTable->count = 0;
}


const char *Dictionary::tabComplete(const char *prevText, S32 baseLen, bool fForward)
{
   S32 i;
   
   const char *bestMatch = NULL;
   for(i = 0; i < mHashTable->size; i++)
   {
      Entry *walk = mHashTable->data[i];
      while(walk)
      {
         if (mVm->mNSState.canTabComplete(prevText, bestMatch, walk->name, baseLen, fForward))
            bestMatch = walk->name;
         walk = walk->nextEntry;
      }
   }
   return bestMatch;
}


char *typeValueEmpty = "";

Dictionary::Entry::Entry(StringTableEntry in_name)
{
   name = in_name;
   nextEntry = NULL;
   mUsage = NULL;
   mIsConstant = false;

   mConsoleValue = KorkApi::ConsoleValue();
   mHeapAlloc = NULL;
}

Dictionary::Entry::~Entry()
{
   AssertFatal(mHeapAlloc == NULL, "Heap alloc still present")
}

Dictionary::Entry* Dictionary::getVariable(StringTableEntry name)
{
   Entry *ent = lookup(name);
   if(ent)
   {
      return ent;
   }
   
   // Warn users when they access a variable that isn't defined.
   if(mVm->mConfig.warnUndefinedScriptVariables)
   {
      mVm->printf(0, " *** Accessed undefined variable '%s'", name);
   }
   return NULL;
}

U32 Dictionary::getEntryUnsignedValue(Entry* e)
{
   switch (e->mConsoleValue.typeId)
   {
      case KorkApi::ConsoleValue::TypeInternalUnsigned:
      return (U32)e->mConsoleValue.getInt();
      break;
      case KorkApi::ConsoleValue::TypeInternalNumber:
      return e->mConsoleValue.getFloat();
      break;
      case KorkApi::ConsoleValue::TypeInternalString:
      return atoll((const char*)e->mConsoleValue.evaluatePtr(mVm->mAllocBase));
      break;
      default:
      {
         KorkApi::TypeInfo& info = mVm->mTypes[e->mConsoleValue.typeId];
         void* typePtr = e->mConsoleValue.evaluatePtr(mVm->mAllocBase);

         return info.iFuncs.CopyValue(info.userPtr,
                                      mVm->mVM,
                      typePtr,
                      NULL,
                      0,
                                            KorkApi::ConsoleValue::TypeInternalNumber,
                                            KorkApi::ConsoleValue::ZoneExternal).getInt();
      }
      break;
   }
}

F32 Dictionary::getEntryNumberValue(Entry* e)
{
   switch (e->mConsoleValue.typeId)
   {
      case KorkApi::ConsoleValue::TypeInternalUnsigned:
      return e->mConsoleValue.getInt();
      break;
      case KorkApi::ConsoleValue::TypeInternalNumber:
      return e->mConsoleValue.getFloat();
      break;
      case KorkApi::ConsoleValue::TypeInternalString:
      return atof((const char*)e->mConsoleValue.evaluatePtr(mVm->mAllocBase));
      break;
      default:
      {
         KorkApi::TypeInfo& info = mVm->mTypes[e->mConsoleValue.typeId];
         void* typePtr = e->mConsoleValue.evaluatePtr(mVm->mAllocBase);

         return info.iFuncs.CopyValue(info.userPtr,
                                      mVm->mVM,
                      typePtr,
                      NULL,
                      0,
                                            KorkApi::ConsoleValue::TypeInternalUnsigned,
                                            KorkApi::ConsoleValue::ZoneExternal).getFloat();
      }
      break;
   }
}

const char *Dictionary::getEntryStringValue(Entry* e)
{
   switch (e->mConsoleValue.typeId)
   {
      case KorkApi::ConsoleValue::TypeInternalUnsigned:
      return mVm->tempIntConv(e->mConsoleValue.getInt());
      break;
      case KorkApi::ConsoleValue::TypeInternalNumber:
      return mVm->tempFloatConv(e->mConsoleValue.getFloat());
      break;
      case KorkApi::ConsoleValue::TypeInternalString:
      return (const char*)e->mConsoleValue.evaluatePtr(mVm->mAllocBase);
      break;
   default:
      {
         KorkApi::TypeInfo& info = mVm->mTypes[e->mConsoleValue.typeId];
         void* typePtr = e->mConsoleValue.evaluatePtr(mVm->mAllocBase);

         return (const char*)info.iFuncs.CopyValue(info.userPtr,
                                                   mVm->mVM,
                      typePtr,
                      NULL,
                      0,
                                            KorkApi::ConsoleValue::TypeInternalString,
                                            KorkApi::ConsoleValue::ZoneReturn).evaluatePtr(mVm->mAllocBase);
      }
      break;
   }
}

KorkApi::ConsoleValue Dictionary::getEntryValue(Entry* e)
{
   return e->mConsoleValue;
}

void Dictionary::setEntryUnsignedValue(Entry* e, U64 val)
{
   if( e->mIsConstant )
   {
      mVm->printf(0, "Cannot assign value to constant '%s'.", e->name );
      return;
   }

   if (e->mHeapAlloc)
   {
      clearEntry(e);
   }
   e->mConsoleValue.setUnsigned(val);
}

void Dictionary::setEntryNumberValue(Entry* e, F32 val)
{
   if( e->mIsConstant )
   {
      mVm->printf(0, "Cannot assign value to constant '%s'.", e->name );
      return;
   }

   if (e->mHeapAlloc)
   {
      clearEntry(e);
   }
   e->mConsoleValue.setNumber(val);
}

void Dictionary::clearEntry(Entry* e)
{
   if (e->mHeapAlloc)
   {
      mVm->releaseHeapRef(e->mHeapAlloc);
      e->mHeapAlloc = NULL;
   }
}

void Dictionary::setEntryStringValue(Dictionary::Entry* e, const char * value)
{
   if( e->mIsConstant )
   {
      mVm->printf(0, "Cannot assign value to constant '%s'.", e->name );
      return;
   }

   U32 expectedSize = dStrlen(value)+1;
   
   if (e->mHeapAlloc && expectedSize > e->mHeapAlloc->size)
   {
      mVm->releaseHeapRef(e->mHeapAlloc);
      e->mHeapAlloc = NULL;
   }

   if (!e->mHeapAlloc)
   {
      e->mHeapAlloc = mVm->createHeapRef(expectedSize);
   }

   memcpy(e->mHeapAlloc->ptr(), value, expectedSize);
   e->mConsoleValue.setString((const char*)e->mHeapAlloc->ptr(),
                              KorkApi::ConsoleValue::ZoneVmHeap);
}

void Dictionary::setEntryTypeValue(Dictionary::Entry* e, U32 typeId, void * value)
{
   if( e->mIsConstant )
   {
      mVm->printf(0, "Cannot assign value to constant '%s'.", e->name );
      return;
   }

   KorkApi::TypeInfo& info = mVm->mTypes[e->mConsoleValue.typeId];
   U32 expectedSize = (U32)info.size;
   
   if (e->mHeapAlloc && e->mHeapAlloc->size < expectedSize)
   {
      mVm->releaseHeapRef(e->mHeapAlloc);
      e->mHeapAlloc = NULL;
   }

   if (!e->mHeapAlloc)
   {
      e->mHeapAlloc = mVm->createHeapRef(expectedSize);
   }

   memcpy(e->mHeapAlloc->ptr(), value, expectedSize);
   e->mConsoleValue.setString((const char*)e->mHeapAlloc->ptr(),
                              KorkApi::ConsoleValue::ZoneVmHeap);
}

void Dictionary::setEntryValue(Entry* e, KorkApi::ConsoleValue value)
{
   if (value.typeId == KorkApi::ConsoleValue::TypeInternalString)
   {
      setEntryStringValue(e, (const char*)value.evaluatePtr(mVm->mAllocBase));
   }
   else if (value.typeId >= KorkApi::ConsoleValue::TypeBeginCustom)
   {
      setEntryTypeValue(e, value.typeId, value.evaluatePtr(mVm->mAllocBase));
   }
   else
   {
      e->mConsoleValue = value;

      if (e->mHeapAlloc)
      {
         mVm->releaseHeapRef(e->mHeapAlloc);
         e->mHeapAlloc = NULL;
      }
   }
}


void Dictionary::setVariable(StringTableEntry name, const char *value)
{
   Entry *ent = add(name);
   if(!value)
      value = "";
   setEntryStringValue(ent, value);
}

void Dictionary::setVariableValue(StringTableEntry name, KorkApi::ConsoleValue value)
{
   Entry *ent = add(name);
   setEntryValue(ent, value);
}

Dictionary::Entry* Dictionary::addVariable(  const char *name,
                                           S32 type,
                                           void *dataPtr,
                                           const char* usage )
{
   AssertFatal( type >= 0, "Dictionary::addVariable - Got bad type!" );
   
   char scratchBuffer[1024];
   
   if(name[0] != '$')
   {
      scratchBuffer[0] = '$';
      dStrcpy(scratchBuffer + 1, name);
      name = scratchBuffer;
   }
   
   Entry *ent = add(StringTable->insert(name));
   clearEntry(ent);
   ent->mConsoleValue.makeTyped(dataPtr, type);
   ent->mUsage = usage;
   
   return ent;
}

bool Dictionary::removeVariable(StringTableEntry name)
{
   if( Entry *ent = lookup(name) )
   {
      remove( ent );
      return true;
   }
   return false;
}

void Dictionary::validate()
{
   AssertFatal( !mHashTable || mHashTable->owner == this,
               "Dictionary::validate() - Dictionary not owner of own hashtable!" );
}

ExprEvalState::ExprEvalState(KorkApi::VmInternal* vm): mSTR(&vm->mAllocBase, vm->mTypes.root())
{
   mAllocNumber = 0;
   mGeneration = 0;
   
   VECTOR_SET_ASSOCIATION(stack);
   vmInternal = vm;
   globalVars = &vm->mGlobalVars;
   traceOn = false;
   traceBuffer[0] = '\0';
   lastThrow = 0;
   mStackPopBreakIndex = -1;
   
   memset(iterStack, 0, sizeof(iterStack));
   memset(floatStack, 0, sizeof(floatStack));
   memset(intStack, 0, sizeof(intStack));
   memset(objectCreationStack, 0, sizeof(objectCreationStack));
   memset(tryStack, 0, sizeof(tryStack));
   memset(vmStack, 0, sizeof(vmStack));

   _VM = 0;
   
   mCurrentFile = NULL;
   mCurrentRoot = NULL;
   
   mState = KorkApi::FiberRunResult::INACTIVE;
   mUserPtr = NULL;
}

ExprEvalState::~ExprEvalState()
{
   reset();
}

void ExprEvalState::reset()
{
   while(vmFrames.size())
      popFrame();
   mSTR.reset();
}

// !!!!! FOLLOWING NOT CHECKED YET !!!!!

#if TOFIX
ConsoleFunction(backtrace, void, 1, 1, "Print the call stack.")
{
   argc; argv;
   U32 totalSize = 1;

   for(U32 i = 0; i < gEvalState.stack.size(); i++)
   {
      totalSize += dStrlen(gEvalState.vmFrames[i]->scopeName) + 3;
      if(gEvalState.vmFrames[i]->scopeNamespace && gEvalState.vmFrames[i]->scopeNamespace->mName)
         totalSize += dStrlen(gEvalState.vmFrames[i]->scopeNamespace->mName) + 2;
   }

   char *buf = Con::getReturnBuffer(totalSize);
   buf[0] = 0;
   for(U32 i = 0; i < gEvalState.stack.size(); i++)
   {
      dStrcat(buf, "->");
      if(gEvalState.vmFrames[i]->scopeNamespace && gEvalState.vmFramesi]->scopeNamespace->mName)
      {
         dStrcat(buf, gEvalState.vmFramesi]->scopeNamespace->mName);
         dStrcat(buf, "::");
      }
      dStrcat(buf, gEvalState.vmFramesi]->scopeName);
   }
   Con::printf("BackTrace: %s", buf);

}


ConsoleFunctionGroupBegin( Packages, "Functions relating to the control of packages.");

ConsoleFunction(isPackage,bool,2,2,"isPackage(packageName)")
{
   argc;
   StringTableEntry packageName = StringTable->insert(argv[1]);
   return Namespace::isPackage(packageName);
}

ConsoleFunction(activatePackage, void,2,2,"activatePackage(packageName)")
{
   argc;
   StringTableEntry packageName = StringTable->insert(argv[1]);
   Namespace::activatePackage(packageName);
}

ConsoleFunction(deactivatePackage, void,2,2,"deactivatePackage(packageName)")
{
   argc;
   StringTableEntry packageName = StringTable->insert(argv[1]);
   Namespace::deactivatePackage(packageName);
}

ConsoleFunctionGroupEnd( Packages );
#endif
