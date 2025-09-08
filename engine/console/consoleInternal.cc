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

static char scratchBuffer[1024];

//---------------------------------------------------------------
//
// Dictionary functions
//
//---------------------------------------------------------------

static S32 QSORT_CALLBACK varCompare(const void* a,const void* b)
{
   return dStricmp( (*((Dictionary::Entry **) a))->name, (*((Dictionary::Entry **) b))->name );
}

void Dictionary::exportVariables(const char *varString, const char *fileName, bool append)
{
   const char *searchStr = varString;
   Vector<Entry *> sortList(__FILE__, __LINE__);
   
   for(S32 i = 0; i < hashTable->size;i ++)
   {
      Entry *walk = hashTable->data[i];
      while(walk)
      {
         if(FindMatch::isMatch((char *) searchStr, (char *) walk->name))
            sortList.push_back(walk);
         
         walk = walk->nextEntry;
      }
   }
   
   if(!sortList.size())
      return;
   
   dQsort((void *)sortList.address(), sortList.size(), sizeof(Entry *), varCompare);
   
   Vector<Entry *>::iterator s;
   char expandBuffer[1024];
   FileStream strm;
   
   if(fileName)
   {
      if(!strm.open(fileName, append ? FileStream::ReadWrite : FileStream::Write))
      {
         vm->printf(0, "Unable to open file '%s for writing.", fileName);
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
         case KorkApi::ConsoleValue::TypeInternalInt:
            dSprintf(buffer, sizeof(buffer), "%s = %d;%s", (*s)->name, (*s)->mConsoleValue.getInt(), cat);
            break;
         case KorkApi::ConsoleValue::TypeInternalFloat:
            dSprintf(buffer, sizeof(buffer), "%s = %g;%s", (*s)->name, (*s)->mConsoleValue.getFloat(), cat);
            break;
         default:
            expandEscape(expandBuffer, (const char*)(*s)->mConsoleValue.evaluatePtr(vm->mAllocBase));
            dSprintf(buffer, sizeof(buffer), "%s = \"%s\";%s", (*s)->name, expandBuffer, cat);
            break;
      }
      if(fileName)
      {
         strm.write(dStrlen(buffer), buffer);
      }
      else
      {
         vm->printf(0, "%s", buffer);
      }
   }
   if(fileName)
      strm.close();
}

void Dictionary::deleteVariables(const char *varString)
{
   const char *searchStr = varString;
   
   for(S32 i = 0; i < hashTable->size; i++)
   {
      Entry *walk = hashTable->data[i];
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
   Entry *walk = hashTable->data[HashPointer(name) % hashTable->size];
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
   Entry *walk = hashTable->data[HashPointer(name) % hashTable->size];
   while(walk)
   {
      if(walk->name == name)
         return walk;
      else
         walk = walk->nextEntry;
   }
   Entry *ret;
   hashTable->count++;
   
   if(hashTable->count > hashTable->size * 2)
   {
      Entry head(NULL), *walk;
      S32 i;
      walk = &head;
      walk->nextEntry = 0;
      for(i = 0; i < hashTable->size; i++) {
         while(walk->nextEntry) {
            walk = walk->nextEntry;
         }
         walk->nextEntry = hashTable->data[i];
      }
      delete[] hashTable->data;
      hashTable->size = hashTable->size * 4 - 1;
      hashTable->data = new Entry *[hashTable->size];
      for(i = 0; i < hashTable->size; i++)
         hashTable->data[i] = NULL;
      walk = head.nextEntry;
      while(walk)
      {
         Entry *temp = walk->nextEntry;
         U32 idx = HashPointer(walk->name) % hashTable->size;
         walk->nextEntry = hashTable->data[idx];
         hashTable->data[idx] = walk;
         walk = temp;
      }
   }
   
   ret = new Entry(name);
   U32 idx = HashPointer(name) % hashTable->size;
   ret->nextEntry = hashTable->data[idx];
   hashTable->data[idx] = ret;
   return ret;
}

// deleteVariables() assumes remove() is a stable remove (will not reorder entries on remove)
void Dictionary::remove(Dictionary::Entry *ent)
{
   Entry **walk = &hashTable->data[HashPointer(ent->name) % hashTable->size];
   while(*walk != ent)
      walk = &((*walk)->nextEntry);
   
   *walk = (ent->nextEntry);
   clearEntry(ent);
   delete ent;
   hashTable->count--;
}

Dictionary::Dictionary()
:  hashTable( NULL ),
exprState( NULL ),
scopeName( NULL ),
scopeNamespace( NULL ),
code( NULL ),
ip( 0 )
{
}

Dictionary::Dictionary(ExprEvalState *state, Dictionary* ref)
:  hashTable( NULL ),
exprState( NULL ),
scopeName( NULL ),
scopeNamespace( NULL ),
code( NULL ),
ip( 0 )
{
   setState(state,ref);
}

void Dictionary::setState(ExprEvalState *state, Dictionary* ref)
{
   exprState = state;
   vm = state->vmInternal;
   
   if (ref)
      hashTable = ref->hashTable;
   else
   {
      hashTable = new HashTableData;
      hashTable->owner = this;
      hashTable->count = 0;
      hashTable->size = ST_INIT_SIZE;
      hashTable->data = new Entry *[hashTable->size];
      
      for(S32 i = 0; i < hashTable->size; i++)
         hashTable->data[i] = NULL;
   }
}

Dictionary::~Dictionary()
{
   if ( hashTable->owner == this )
   {
      reset();
      delete [] hashTable->data;
      delete hashTable;
   }
}

void Dictionary::reset()
{
   S32 i;
   Entry *walk, *temp;
   
   for(i = 0; i < hashTable->size; i++)
   {
      walk = hashTable->data[i];
      while(walk)
      {
         temp = walk->nextEntry;
         clearEntry(walk);
         delete walk;
         walk = temp;
      }
      hashTable->data[i] = NULL;
   }
   hashTable->size = ST_INIT_SIZE;
   hashTable->count = 0;
}


const char *Dictionary::tabComplete(const char *prevText, S32 baseLen, bool fForward)
{
   S32 i;
   
   const char *bestMatch = NULL;
   for(i = 0; i < hashTable->size; i++)
   {
      Entry *walk = hashTable->data[i];
      while(walk)
      {
         if (vm->mNSState.canTabComplete(prevText, bestMatch, walk->name, baseLen, fForward))
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
   /* TOFIX if(gWarnUndefinedScriptVariables)
      Con::warnf(" *** Accessed undefined variable '%s'", name);
   */
   return NULL;
}

U32 Dictionary::getEntryIntValue(Entry* e)
{
   switch (e->mConsoleValue.typeId)
   {
      case KorkApi::ConsoleValue::TypeInternalInt:
      return e->mConsoleValue.getInt();
      break;
      case KorkApi::ConsoleValue::TypeInternalFloat:
      return e->mConsoleValue.getFloat();
      break;
      case KorkApi::ConsoleValue::TypeInternalString:
      return atoll((const char*)e->mConsoleValue.evaluatePtr(vm->mAllocBase));
      break;
      default:
      {
         KorkApi::TypeInfo& info = vm->mTypes[e->mConsoleValue.typeId];
         void* typePtr = e->mConsoleValue.evaluatePtr(vm->mAllocBase);

         return atoll(info.iFuncs.GetDataFn(info.userPtr,
                      typePtr,
                      NULL,
                      0));
      }
      break;
   }
}

F32 Dictionary::getEntryFloatValue(Entry* e)
{
   switch (e->mConsoleValue.typeId)
   {
      case KorkApi::ConsoleValue::TypeInternalInt:
      return e->mConsoleValue.getInt();
      break;
      case KorkApi::ConsoleValue::TypeInternalFloat:
      return e->mConsoleValue.getFloat();
      break;
      case KorkApi::ConsoleValue::TypeInternalString:
      return atof((const char*)e->mConsoleValue.evaluatePtr(vm->mAllocBase));
      break;
      default:
      {
         KorkApi::TypeInfo& info = vm->mTypes[e->mConsoleValue.typeId];
         void* typePtr = e->mConsoleValue.evaluatePtr(vm->mAllocBase);

         return atof(info.iFuncs.GetDataFn(info.userPtr,
                      typePtr,
                      NULL,
                      0));
      }
      break;
   }
}

const char *Dictionary::getEntryStringValue(Entry* e)
{
   switch (e->mConsoleValue.typeId)
   {
      case KorkApi::ConsoleValue::TypeInternalInt:
      return vm->tempIntConv(e->mConsoleValue.getInt());
      break;
      case KorkApi::ConsoleValue::TypeInternalFloat:
      return vm->tempFloatConv(e->mConsoleValue.getFloat());
      break;
      case KorkApi::ConsoleValue::TypeInternalString:
      return (const char*)e->mConsoleValue.evaluatePtr(vm->mAllocBase);
      break;
   default:
      {
         KorkApi::TypeInfo& info = vm->mTypes[e->mConsoleValue.typeId];
         void* typePtr = e->mConsoleValue.evaluatePtr(vm->mAllocBase);

         return info.iFuncs.GetDataFn(info.userPtr,
                      typePtr,
                      NULL,
                      0);
      }
      break;
   }
}

void Dictionary::setEntryIntValue(Entry* e, U32 val)
{
   if( e->mIsConstant )
   {
      vm->printf(0, "Cannot assign value to constant '%s'.", e->name );
      return;
   }

   if (e->mHeapAlloc)
   {
      clearEntry(e);
   }
   e->mConsoleValue.setInt(val);
}

void Dictionary::setEntryFloatValue(Entry* e, F32 val)
{
   if( e->mIsConstant )
   {
      vm->printf(0, "Cannot assign value to constant '%s'.", e->name );
      return;
   }

   if (e->mHeapAlloc)
   {
      clearEntry(e);
   }
   e->mConsoleValue.setFloat(val);
}

void Dictionary::clearEntry(Entry* e)
{
   if (e->mHeapAlloc)
   {
      vm->releaseHeapRef(e->mHeapAlloc);
      e->mHeapAlloc = NULL;
   }
}

void Dictionary::setEntryStringValue(Dictionary::Entry* e, const char * value)
{
   if( e->mIsConstant )
   {
      vm->printf(0, "Cannot assign value to constant '%s'.", e->name );
      return;
   }

   U32 expectedSize = dStrlen(value)+1;
   
   if (e->mHeapAlloc && e->mHeapAlloc->size < expectedSize)
   {
      vm->releaseHeapRef(e->mHeapAlloc);
      e->mHeapAlloc = NULL;
   }

   if (!e->mHeapAlloc)
   {
      e->mHeapAlloc = vm->createHeapRef(expectedSize);
   }

   memcpy(e->mHeapAlloc->ptr(), value, expectedSize);
   e->mConsoleValue.setString((const char*)e->mHeapAlloc->ptr(),
                              KorkApi::ConsoleValue::ZoneVmHeap);
}

void Dictionary::setVariable(StringTableEntry name, const char *value)
{
   Entry *ent = add(name);
   if(!value)
      value = "";
   setEntryStringValue(ent, value);
}

Dictionary::Entry* Dictionary::addVariable(  const char *name,
                                           S32 type,
                                           void *dataPtr,
                                           const char* usage )
{
   AssertFatal( type >= 0, "Dictionary::addVariable - Got bad type!" );
   
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
   AssertFatal( !hashTable || hashTable->owner == this,
               "Dictionary::validate() - Dictionary not owner of own hashtable!" );
}

void ExprEvalState::pushFrame(StringTableEntry frameName, Namespace *ns)
{
   Dictionary *newFrame = new Dictionary(this);
   newFrame->scopeName = frameName;
   newFrame->scopeNamespace = ns;
   stack.push_back(newFrame);
   mStackDepth ++;
   //Con::printf("ExprEvalState::pushFrame");
}

void ExprEvalState::popFrame()
{
   Dictionary *last = stack.last();
   stack.pop_back();
   delete last;
   mStackDepth --;
   //Con::printf("ExprEvalState::popFrame");
}

void ExprEvalState::pushFrameRef(S32 stackIndex)
{
   AssertFatal( stackIndex >= 0 && stackIndex < stack.size(), "You must be asking for a valid frame!" );
   Dictionary *newFrame = new Dictionary(this, stack[stackIndex]);
   stack.push_back(newFrame);
   mStackDepth ++;
   //Con::printf("ExprEvalState::pushFrameRef");
}

ExprEvalState::ExprEvalState(KorkApi::VmInternal* vm)
{
   VECTOR_SET_ASSOCIATION(stack);
   vmInternal = vm;
   globalVars.setState(this);
   thisObject = NULL;
   traceOn = false;
   mStackDepth = 0;
   
   currentVariable = NULL;
   copyVariable = NULL;
   currentDictionary = NULL;
   copyDictionary = NULL;
}

ExprEvalState::~ExprEvalState()
{
   while(stack.size())
      popFrame();
}

void ExprEvalState::validate()
{
   AssertFatal( mStackDepth <= stack.size(),
               "ExprEvalState::validate() - Stack depth pointing beyond last stack frame!" );
   
   for( U32 i = 0; i < stack.size(); ++ i )
      stack[ i ]->validate();
}
// !!!!! FOLLOWING NOT CHECKED YET !!!!!

#if TOFIX
ConsoleFunction(backtrace, void, 1, 1, "Print the call stack.")
{
   argc; argv;
   U32 totalSize = 1;

   for(U32 i = 0; i < gEvalState.stack.size(); i++)
   {
      totalSize += dStrlen(gEvalState.stack[i]->scopeName) + 3;
      if(gEvalState.stack[i]->scopeNamespace && gEvalState.stack[i]->scopeNamespace->mName)
         totalSize += dStrlen(gEvalState.stack[i]->scopeNamespace->mName) + 2;
   }

   char *buf = Con::getReturnBuffer(totalSize);
   buf[0] = 0;
   for(U32 i = 0; i < gEvalState.stack.size(); i++)
   {
      dStrcat(buf, "->");
      if(gEvalState.stack[i]->scopeNamespace && gEvalState.stack[i]->scopeNamespace->mName)
      {
         dStrcat(buf, gEvalState.stack[i]->scopeNamespace->mName);
         dStrcat(buf, "::");
      }
      dStrcat(buf, gEvalState.stack[i]->scopeName);
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
