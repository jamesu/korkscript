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

Dictionary::Dictionary(KorkApi::VmInternal *state, Dictionary::HashTableData* ref)
:  mHashTable( NULL )
{
   setState(state,ref);
}

void Dictionary::setState(KorkApi::VmInternal *state, Dictionary::HashTableData* ref)
{
   mVm = state;
   
   if (ref)
   {
      mHashTable = ref;
   }
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
   mIsRegistered = false;
   mEnforcedType = 0;

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
   return (U32)mVm->valueAsInt(e->mConsoleValue);
}

F32 Dictionary::getEntryNumberValue(Entry* e)
{
   return mVm->valueAsFloat(e->mConsoleValue);
}

const char *Dictionary::getEntryStringValue(Entry* e)
{
   return mVm->valueAsString(e->mConsoleValue);
}

KorkApi::ConsoleValue Dictionary::getEntryValue(Entry* e)
{
   return e->mConsoleValue;
}

U16 Dictionary::getEntryType(Entry* e)
{
   return e->mEnforcedType;
}

void Dictionary::setEntryUnsignedValue(Entry* e, U64 val)
{
   KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeUnsigned(val);
   return setEntryValue(e, cv);
}

void Dictionary::setEntryNumberValue(Entry* e, F32 val)
{
   KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeNumber(val);
   return setEntryValue(e, cv);
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
   KorkApi::TypeStorageInterface inputStorage = KorkApi::CreateFixedTypeStorage(mVm, (void*)value, KorkApi::ConsoleValue::TypeInternalString, false);
   return setEntryTypeValue(e, KorkApi::ConsoleValue::TypeInternalString, &inputStorage);
}

void Dictionary::setEntryTypeValue(Dictionary::Entry* e, U32 inputTypeId, KorkApi::TypeStorageInterface* inputStorage)
{
   if (e->mIsConstant)
   {
      mVm->printf(0, "Cannot assign value to constant '%s'.", e->name );
      return;
   }
   
   ConsoleVarRef cv;
   cv.dictionary = this;
   cv.var = e;
   
   // Setup storage for conversion
   U32 outputTypeId = e->mEnforcedType != 0 ? e->mEnforcedType : inputTypeId;
   KorkApi::TypeStorageInterface outputStorage;
   
   if (!e->mIsRegistered)
   {
      outputStorage = KorkApi::CreateConsoleVarTypeStorage(mVm,
                                             cv,
                                             outputTypeId);
   }
   else
   {
      outputStorage = KorkApi::CreateFixedTypeStorage(mVm, (void*)e->mConsoleValue.cvalue, e->mEnforcedType, true);
   }
   
   // Cast into value
   KorkApi::TypeInfo& info = mVm->mTypes[outputTypeId];
   
   // For fixed size types, ensure we are the correct size (avoids adding check in CastValueFn)
   if (info.valueSize != UINT_MAX && info.valueSize > 0)
   {
      outputStorage.FinalizeStorage(&outputStorage, (U32)info.valueSize);
   }
   
   if (info.iFuncs.CastValueFn(info.userPtr, mVm->mVM, inputStorage, &outputStorage, NULL, 0, outputTypeId))
   {
      // Ensure correct type is assigned and set output
      if (outputStorage.data.storageRegister)
      {
         outputStorage.data.storageRegister->typeId = outputTypeId;
         e->mConsoleValue = *outputStorage.data.storageRegister;
      }
   }
}

void Dictionary::setEntryValue(Entry* e, KorkApi::ConsoleValue value)
{
   KorkApi::TypeStorageInterface inputStorage = KorkApi::CreateRegisterStorageFromArgs(mVm, 1, &value);
   inputStorage.data.storageRegister = &value;
   setEntryTypeValue(e, value.typeId, &inputStorage);
}

void Dictionary::setEntryValues(Entry* e, U32 argc, KorkApi::ConsoleValue* values)
{
   if (e->mIsConstant)
   {
      mVm->printf(0, "Cannot assign value to constant '%s'.", e->name );
      return;
   }
   
   ConsoleVarRef cv;
   cv.dictionary = this;
   cv.var = e;
   
   // Setup storage for conversion
   U32 outputTypeId = e->mEnforcedType != 0 ? e->mEnforcedType : KorkApi::ConsoleValue::TypeInternalString;
   KorkApi::TypeStorageInterface outputStorage = KorkApi::CreateConsoleVarTypeStorage(mVm,
                                                                                       cv,
                                                                                       outputTypeId);
   KorkApi::TypeStorageInterface inputStorage = KorkApi::CreateRegisterStorageFromArgs(mVm,
                                                                                       argc,
                                                                                       values);
   
   // Cast into value
   KorkApi::TypeInfo& info = mVm->mTypes[outputTypeId];
   
   // For fixed size types, ensure we are the correct size (avoids adding check in CastValueFn)
   if (info.valueSize != UINT_MAX && info.valueSize > 0)
   {
      outputStorage.FinalizeStorage(&outputStorage, (U32)info.valueSize);
   }
   
   if (info.iFuncs.CastValueFn(info.userPtr, mVm->mVM, &inputStorage, &outputStorage, NULL, 0, outputTypeId))
   {
      // Ensure correct type is assigned and set output
      outputStorage.data.storageRegister->typeId = outputTypeId;
      e->mConsoleValue = *outputStorage.data.storageRegister;
   }
}

void Dictionary::setEntryType(Entry* e, U16 typeId)
{
   e->mEnforcedType = typeId;
   if (e->mConsoleValue.typeId != typeId)
   {
      KorkApi::ConsoleValue value = KorkApi::ConsoleValue();
      value.typeId = typeId;
      e->mConsoleValue = KorkApi::ConsoleValue();
      
      // Clear existing heap value
      if (e->mHeapAlloc &&
          e->mHeapAlloc->ptr())
      {
         memset(e->mHeapAlloc->ptr(), '\0', e->mHeapAlloc->size);
      }
      
      setEntryValue(e, value);
   }
}

void Dictionary::resizeHeap(Entry* e, U32 newSize, bool force)
{
   bool shouldRealloc = (e->mHeapAlloc == NULL) || (force && (newSize != e->mHeapAlloc->size)) || (newSize > e->mHeapAlloc->size);
   if (shouldRealloc && e->mHeapAlloc)
   {
      mVm->releaseHeapRef(e->mHeapAlloc);
      e->mHeapAlloc = NULL;
   }

   if (!e->mHeapAlloc)
   {
      e->mHeapAlloc = mVm->createHeapRef(newSize);
   }
}

void Dictionary::getHeapPtrSize(Entry* e, U32* size, void** ptr)
{
   *ptr = e->mHeapAlloc ? e->mHeapAlloc->ptr() : NULL;
   *size = e->mHeapAlloc ? e->mHeapAlloc->size : 0;
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
   ent->mConsoleValue = KorkApi::ConsoleValue::makeTyped(dataPtr, type);
   ent->mUsage = usage;
   ent->mIsRegistered = true;
   ent->mEnforcedType = type;
   
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

ExprEvalState::ExprEvalState(KorkApi::VmInternal* vm): mSTR(&vm->mAllocBase, vm->mTypes.root()), globalVars(vm, vm->mGlobalVars.mHashTable)
{
   mAllocNumber = 0;
   mGeneration = 0;
   
   VECTOR_SET_ASSOCIATION(stack);
   vmInternal = vm;
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

namespace KorkApi
{

static void Resize_Fixed(TypeStorageInterface* state, U32 /*newSize*/)
{
}

static void Finalize_Fixed(TypeStorageInterface* state, U32 /*newSize*/)
{
}


static void Resize_ConsoleVar(TypeStorageInterface* state, U32 newSize)
{
   void* ptr = NULL;
   U32 size = 0;
   
   Dictionary* dict = (Dictionary*)state->userPtr1;
   Dictionary::Entry* entry = (Dictionary::Entry*)state->userPtr2;
   if (!dict || !entry)
      return;
   
   dict->resizeHeap(entry, newSize, false);
   dict->getHeapPtrSize(entry, &size, &ptr);
   
   state->data.size = size;
   state->data.storageRegister = state->vmInternal->getTempValuePtr();
   state->data.storageAddress = ConsoleValue::makeRaw(
      (U64)ptr,
      state->data.storageAddress.typeId,
      ConsoleValue::ZoneVmHeap
   );
}


static void Finalize_ConsoleVar(TypeStorageInterface* state, U32 newSize)
{
   void* ptr = NULL;
   U32 size = 0;
   
   Dictionary* dict = (Dictionary*)state->userPtr1;
   Dictionary::Entry* entry = (Dictionary::Entry*)state->userPtr2;
   if (!dict || !entry)
      return;
   
   dict->resizeHeap(entry, newSize, true);
   dict->getHeapPtrSize(entry, &size, &ptr);
   
   state->data.size = size;
   state->data.storageRegister = state->vmInternal->getTempValuePtr();
   state->data.storageAddress = ConsoleValue::makeRaw(
      (U64)ptr,
      state->data.storageAddress.typeId,
      ConsoleValue::ZoneVmHeap
   );
}

static void Resize_ExprEval(TypeStorageInterface* state, U32 newSize)
{
   auto* eval = reinterpret_cast<ExprEvalState*>(state->userPtr1);
   if (!eval)
      return;

   eval->mSTR.validateBufferSize(newSize);

   state->data.size = newSize;
}

static void Finalize_ExprEval(TypeStorageInterface* state, U32 newSize)
{
   auto* eval = reinterpret_cast<ExprEvalState*>(state->userPtr1);
   if (!eval)
      return;

   eval->mSTR.validateBufferSize(newSize);
   eval->mSTR.setConsoleValueSize(newSize);
   state->data.size = newSize;
}

static void Resize_ReturnEval(TypeStorageInterface* state, U32 newSize)
{
   KorkApi::VmInternal* vmInternal = reinterpret_cast<KorkApi::VmInternal*>(state->userPtr1);
   if (!vmInternal)
      return;

   vmInternal->validateReturnBufferSize(newSize);
   state->data.size = newSize;
}

TypeStorageInterface CreateFixedTypeStorage(KorkApi::VmInternal* vmInternal,
   void* ptr,
   U16 typeId,
   bool isField)
{
   TypeStorageInterface s{};
   
   TypeInfo& info = vmInternal->mTypes[typeId];
   s.vmInternal = vmInternal;
   s.ResizeStorage = &Resize_Fixed;
   s.FinalizeStorage = &Finalize_Fixed;
   s.data.size = isField ? info.fieldsize : info.valueSize;
   if (s.data.size == UINT_MAX)
   {
      s.data.size = 0;
   }
   s.data.argc = 0;
   s.data.storageRegister = NULL;
   s.data.storageAddress = ConsoleValue::makeRaw((U64)ptr, KorkApi::ConsoleValue::TypeInternalString, KorkApi::ConsoleValue::ZoneExternal);
   s.userPtr1 = NULL;
   s.userPtr2 = NULL;
   s.isField = isField;
   return s;
}

TypeStorageInterface CreateConsoleVarTypeStorage(KorkApi::VmInternal* vmInternal,
   ConsoleVarRef ref,
   U16 typeId)
{
   TypeStorageInterface s{};
   s.vmInternal = vmInternal;
   s.ResizeStorage = &Resize_ConsoleVar;
   s.FinalizeStorage = &Finalize_ConsoleVar;
   s.userPtr1 = ref.dictionary;
   s.userPtr2 = ref.var;
   s.isField = false;

   if (ref.var)
   {
      void* ptr = NULL;
      U32 size = 0;
      
      ref.dictionary->getHeapPtrSize(ref.var, &size, &ptr);

      s.data.size  = size;

      s.data.argc = 1;
      s.data.storageRegister = vmInternal->getTempValuePtr();
      *s.data.storageRegister = *ref.var->getCVPtr();
      s.data.storageAddress = ConsoleValue::makeRaw(
         (U64)ptr,
         typeId,
         ConsoleValue::ZoneVmHeap
      );
   }

   return s;
}

TypeStorageInterface CreateExprEvalTypeStorage(KorkApi::VmInternal* vmInternal,
   ExprEvalState& eval,
   U32 minSize,
   U16 typeId) // ZoneFunc + n
{
   TypeStorageInterface s{};
   s.vmInternal = vmInternal;
   s.ResizeStorage = &Resize_ExprEval;
   s.FinalizeStorage = &Finalize_ExprEval;
   
   s.userPtr1 = &eval;
   s.userPtr2 = NULL;
   s.isField = false;
   
   s.data.storageRegister = vmInternal->getTempValuePtr();
   *s.data.storageRegister = eval.mSTR.getConsoleValue();
   s.data.storageAddress  = ConsoleValue::makeRaw(eval.mSTR.mStart,
      typeId,
      (KorkApi::ConsoleValue::Zone)(KorkApi::ConsoleValue::ZoneFiberStart + eval.mSTR.mFuncId)
   );

   return s;
}


TypeStorageInterface CreateExprEvalReturnTypeStorage(KorkApi::VmInternal* vmInternal, U32 minSize, U16 typeId)
{
   TypeStorageInterface s{};
   s.vmInternal = vmInternal;
   s.ResizeStorage = &Resize_ReturnEval;
   s.FinalizeStorage = &Resize_ReturnEval;
   s.userPtr1 = vmInternal;
   s.userPtr2 = NULL;
   s.isField = false;

   s.data.size = minSize;
   
   s.data.argc = 1;
   s.data.storageRegister = vmInternal->getTempValuePtr();
   s.data.storageAddress = ConsoleValue::makeRaw(
      0,
      typeId,
      KorkApi::ConsoleValue::ZoneReturn
   );
   *s.data.storageRegister = s.data.storageAddress;

   return s;
}


TypeStorageInterface CreateRegisterStorage(KorkApi::VmInternal* vmInternal, U16 typeId)
{
   TypeStorageInterface s{};
   s.vmInternal = vmInternal;
   s.ResizeStorage = &Resize_Fixed;
   s.FinalizeStorage = &Resize_Fixed;
   s.userPtr1 = vmInternal;
   s.userPtr2 = NULL;
   s.isField = false;
   
   TypeInfo& info = vmInternal->mTypes[typeId];
   s.data.size = info.valueSize;
   if (s.data.size == UINT_MAX)
   {
      s.data.size = 0;
   }
   
   s.data.argc = 1;
   s.data.storageRegister = vmInternal->getTempValuePtr();
   s.data.storageAddress = KorkApi::ConsoleValue();
   
   return s;
}

TypeStorageInterface CreateRegisterStorageFromArg(KorkApi::VmInternal* vmInternal, KorkApi::ConsoleValue arg)
{
   TypeStorageInterface s{};
   s.vmInternal = vmInternal;
   s.ResizeStorage = &Resize_Fixed;
   s.FinalizeStorage = &Resize_Fixed;
   s.userPtr1 = vmInternal;
   s.userPtr2 = NULL;
   s.isField = false;
   s.data.size = 0;
   
   s.data.argc = 1;
   s.data.storageRegister = vmInternal->getTempValuePtr();
   *s.data.storageRegister = arg;
   s.data.storageAddress = arg;
   
   return s;
}

TypeStorageInterface CreateRegisterStorageFromArgs(KorkApi::VmInternal* vmInternal, U32 argc, KorkApi::ConsoleValue* argv)
{
   TypeStorageInterface s{};
   s.vmInternal = vmInternal;
   s.ResizeStorage = &Resize_Fixed;
   s.FinalizeStorage = &Resize_Fixed;
   s.userPtr1 = vmInternal;
   s.userPtr2 = NULL;
   s.isField = false;
   s.data.size = 0;
   
   s.data.argc = argc;
   s.data.storageRegister = argv;
   s.data.storageAddress = KorkApi::ConsoleValue();
   
   return s;
}

}
