//-----------------------------------------------------------------------------
// Copyright (c) 2025-2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

#include "embed/api.h"
#include "embed/internalApi.h"
#include "console/consoleNamespace.h"

#include "core/memStream.h"
#include "console/consoleInternal.h"
#include "console/telnetDebugger.h"
#include "console/telnetConsole.h"

#include "console/compiler.h"
#include "console/codeBlock.h"

#include "core/simpleIntern.h"

#include <cinttypes>

namespace KorkApi
{


NamespaceId Vm::findNamespace(StringTableEntry name, StringTableEntry package)
{
   VmAllocTLS::Scope memScope(mInternal);
   return (NamespaceId)mInternal->mNSState.find(name, package);
}

NamespaceId Vm::lookupNamespace(StringTableEntry name, StringTableEntry package)
{
   VmAllocTLS::Scope memScope(mInternal);
   return (NamespaceId)mInternal->mNSState.lookup(name, package);
}

NamespaceId Vm::getObjectNamespace(VMObject* object)
{
   return (NamespaceId)object->ns;
}

void Vm::setNamespaceUsage(NamespaceId nsId, const char* usage)
{
   Namespace* ns = (Namespace*)nsId;
   ns->mUsage = usage;
}

void Vm::setNamespaceUserPtr(NamespaceId nsId, void* userPtr)
{
   Namespace* ns = (Namespace*)nsId;
   ns->mUserPtr = userPtr;
}

NamespaceId Vm::getGlobalNamespace()
{
    return mInternal->mNSState.mGlobalNamespace;
}

void Vm::activatePackage(StringTableEntry pkgName)
{
   VmAllocTLS::Scope memScope(mInternal);
   mInternal->mNSState.activatePackage(pkgName);
}

void Vm::deactivatePackage(StringTableEntry pkgName)
{
   VmAllocTLS::Scope memScope(mInternal);
   mInternal->mNSState.deactivatePackage(pkgName);
}

bool Vm::isPackage(StringTableEntry pkgName)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->mNSState.isPackage(pkgName);
}

bool Vm::linkNamespace(StringTableEntry parent, StringTableEntry child)
{
   VmAllocTLS::Scope memScope(mInternal);
   NamespaceId pns = mInternal->mNSState.find(parent);
   NamespaceId cns = mInternal->mNSState.find(child);
   if(pns && cns)
   {
      return cns->classLinkTo(pns);
   }
   return false;
}

bool Vm::unlinkNamespace(StringTableEntry parent, StringTableEntry child)
{
   VmAllocTLS::Scope memScope(mInternal);
   NamespaceId pns = mInternal->mNSState.find(parent);
   NamespaceId cns = mInternal->mNSState.find(child);
   if(pns && cns)
      return cns->unlinkClass(pns);
   return false;
}

void Vm::enumerateNamespace(NamespaceId nsId, void* userPtr, NamespaceEnumerationCallback funcPtr)
{
   VmAllocTLS::Scope memScope(mInternal);
   KorkApi::Vector<Namespace::Entry*> vec;
   Namespace* ns = (Namespace*)nsId;
   ns->getEntryList(&vec);

   for (Namespace::Entry* ent : vec)
   {
      funcPtr(userPtr, ent->mFunctionName, ent->getUsage() ? ent->getUsage() : "");
   }
}

bool Vm::linkNamespaceById(NamespaceId parent, NamespaceId child)
{
   VmAllocTLS::Scope memScope(mInternal);
   Namespace* pns = (Namespace*)parent;
   Namespace* cns = (Namespace*)child;
   if(pns && cns)
      return cns->classLinkTo(pns);
   return false;
}

bool Vm::unlinkNamespaceById(NamespaceId parent, NamespaceId child)
{
   VmAllocTLS::Scope memScope(mInternal);
   Namespace* pns = (Namespace*)parent;
   Namespace* cns = (Namespace*)child;
   if(pns && cns)
      return cns->unlinkClass(pns);
   return false;
}

const char* Vm::tabCompleteNamespace(NamespaceId nsId, const char *prevText, S32 baseLen, bool fForward)
{
   VmAllocTLS::Scope memScope(mInternal);
   Namespace* ns = (Namespace*)nsId;
   return ns->tabComplete(prevText, baseLen, fForward);
}

const char* Vm::tabCompleteVariable(const char *prevText, S32 baseLen, bool fForward)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->mGlobalVars.tabComplete(prevText, baseLen, fForward);
}

TypeId Vm::registerType(TypeInfo& info)
{
   VmAllocTLS::Scope memScope(mInternal);
   mInternal->mTypes.push_back(info);
   
   TypeInfo& chkFunc = mInternal->mTypes.back();
   
   // stubs
   
   if (chkFunc.iFuncs.CastValueFn == nullptr)
   {
      chkFunc.iFuncs.CastValueFn = [](void* userPtr,
                                    Vm* vm,
                                           TypeStorageInterface* inputStorage,
                                      TypeStorageInterface* outputStorage,
                                    void* fieldUserPtr,
                                    BitSet32 flag,
                                    U32 requestedType){
         return false;
      };
   }
   
   if (chkFunc.iFuncs.PerformOpFn == nullptr)
   {
      chkFunc.iFuncs.PerformOpFn = [](void* userPtr, Vm* vm, U32 op, ConsoleValue lhs, ConsoleValue rhs){
         return lhs;
      };
   }
   
   return mInternal->mTypes.size()-1;
}

TypeInfo* Vm::getTypeInfo(TypeId ident)
{
   return &mInternal->mTypes[ident];
}

bool Vm::castValue(TypeId inputType, TypeStorageInterface* inputStorage, TypeStorageInterface* outputStorage, void* userPtr, BitSet32 flags)
{
   VmAllocTLS::Scope memScope(mInternal);
   CastValueFnType castFn = mInternal->mTypes[inputType].iFuncs.CastValueFn;
   return castFn(
      mInternal->mTypes[inputType].userPtr,
      this,
      inputStorage,
      outputStorage,
      userPtr,
      flags,
      inputType
   );
}

ConsoleValue Vm::castToReturn(U32 argc, KorkApi::ConsoleValue* argv, U32 inputTypeId, U32 outputTypeId)
{
   VmAllocTLS::Scope memScope(mInternal);
   KorkApi::TypeStorageInterface inputStorage = KorkApi::CreateRegisterStorageFromArgs(mInternal, argc, argv);

   KorkApi::TypeStorageInterface outputStorage = KorkApi::CreateExprEvalReturnTypeStorage(mInternal,
                                                                                       1024,
                                                                                          outputTypeId);

   // NOTE: types should set head of stack to value if data pointer is nullptr in this case
   if (mInternal->mTypes[inputTypeId].iFuncs.CastValueFn(mInternal->mTypes[inputTypeId].userPtr,
                                           this,
                                                    &inputStorage,
                                                    &outputStorage,
                                                    nullptr,
                                                    0,
                                                    outputTypeId))
   {
      // NOTE: this needs to be put somewhere since the return buffer is often used elsewhere
      // TODO: maybe need a variant here for custom output storage to handle externally
      return *outputStorage.data.storageRegister;
   }
   else
   {
      return KorkApi::ConsoleValue();
   }
}

ClassId Vm::registerClass(ClassInfo& info)
{
   VmAllocTLS::Scope memScope(mInternal);
   mInternal->mClassList.push_back(info);
   
   ClassInfo& chkFunc = mInternal->mClassList.back();
   
   // iCreate stubs
   
   if (chkFunc.iCreate.CreateClassFn == nullptr)
   {
      chkFunc.iCreate.CreateClassFn = [](void* user, Vm* vm, CreateClassReturn* outP) {
      };
   }
   if (chkFunc.iCreate.DestroyClassFn == nullptr)
   {
      chkFunc.iCreate.DestroyClassFn = [](void* user, Vm* vm, void* createdPtr) {
      };
   }
   if (chkFunc.iCreate.ProcessArgsFn == nullptr)
   {
      chkFunc.iCreate.ProcessArgsFn = [](Vm* vm, void* createdPtr, const char* name, bool isDatablock, bool internalName, int argc, const char** argv) {
         return false;
      };
   }
   if (chkFunc.iCreate.AddObjectFn == nullptr)
   {
      chkFunc.iCreate.AddObjectFn = [](Vm* vm, VMObject* object, bool placeAtRoot, U32 groupAddId) {
         return false;
      };
   }
   if (chkFunc.iCreate.RemoveObjectFn == nullptr)
   {
      chkFunc.iCreate.RemoveObjectFn = [](void* user, Vm* vm, VMObject* object) {
      };
   }
   if (chkFunc.iCreate.GetIdFn == nullptr)
   {
      chkFunc.iCreate.GetIdFn = [](VMObject* object) {
         return (SimObjectId)0;
      };
   }
   if (chkFunc.iCreate.GetNameFn == nullptr)
   {
      chkFunc.iCreate.GetNameFn = [](VMObject* object) {
         return (StringTableEntry)nullptr;
      };
   }
   
   // iEnum stubs
   
   if (chkFunc.iEnum.GetSize == nullptr)
   {
      chkFunc.iEnum.GetSize = [](VMObject* object) { return (U32)0; };
   }
   if (chkFunc.iEnum.GetObjectAtIndex == nullptr)
   {
      chkFunc.iEnum.GetObjectAtIndex = [](VMObject* object, U32 index) { return (VMObject*)nullptr; };
   }
   
   // iCustomFields stubs
   if (chkFunc.iCustomFields.IterateFields == nullptr)
   {
      chkFunc.iCustomFields.IterateFields = [](KorkApi::Vm* vm, KorkApi::VMObject* object, VMIterator& state, StringTableEntry* name){
         return false;
      };
   }
   if (chkFunc.iCustomFields.GetFieldByIterator == nullptr)
   {
      chkFunc.iCustomFields.GetFieldByIterator = [](KorkApi::Vm* vm, VMObject* object, VMIterator& state){
         return ConsoleValue();
      };
   }
   if (chkFunc.iCustomFields.GetFieldByName == nullptr)
   {
      chkFunc.iCustomFields.GetFieldByName = [](KorkApi::Vm* vm, VMObject* object, const char* name){
         return ConsoleValue();
      };
   }
   if (chkFunc.iCustomFields.SetCustomFieldByName == nullptr)
   {
      chkFunc.iCustomFields.SetCustomFieldByName = [](KorkApi::Vm* vm, VMObject* object, const char* name, const char* array, U32 argc, ConsoleValue* argv){
      };
   }
   if (chkFunc.iCustomFields.SetCustomFieldType == nullptr)
   {
      chkFunc.iCustomFields.SetCustomFieldType = [](KorkApi::Vm* vm, VMObject* object, const char* name, const char* array, U32 typeId){
         return false;
      };
   }
      
   
   return mInternal->mClassList.size()-1;
}

ClassId Vm::getClassId(const char* name)
{
   StringTableEntry klassST = mInternal->internString(name, false);
   for (U32 i=0; i<mInternal->mClassList.size(); i++)
   {
      if (mInternal->mClassList[i].name == klassST)
      {
         return i;
      }
   }
   return -1;
}

ConsoleHeapAllocRef Vm::createHeapRef(U32 size)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->createHeapRef(size);
}

void Vm::releaseHeapRef(ConsoleHeapAllocRef value)
{
   VmAllocTLS::Scope memScope(mInternal);
   mInternal->releaseHeapRef(value);
}

S32 VmInternal::lookupTypeId(StringTableEntry typeName)
{
   for (Vector<TypeInfo>::iterator itr = mTypes.begin(), itrEnd = mTypes.end(); itr != itrEnd; itr++)
   {
      if (itr->name == typeName)
      {
         return (S32)(itr - mTypes.begin());
      }
   }
   return -1;
}

ConsoleHeapAllocRef VmInternal::createHeapRef(U32 size)
{
   ConsoleHeapAlloc* ref = (ConsoleHeapAlloc*)mConfig.mallocFn(sizeof(ConsoleHeapAlloc) + size, mConfig.allocUser);
   ref->size = size;
   
   ref->prev = nullptr;
   ref->next = mHeapAllocs;
   if (mHeapAllocs)
   {
      mHeapAllocs->prev = ref;
   }
   mHeapAllocs = ref;

   return (ConsoleHeapAllocRef)ref;
}

void VmInternal::releaseHeapRef(ConsoleHeapAllocRef value)
{
   ConsoleHeapAlloc* ref = (ConsoleHeapAlloc*)value;
   ConsoleHeapAlloc* prev = ref->prev;
   ConsoleHeapAlloc* next = ref->next;
   if (prev)
   {
      prev->next = next;
   }
   if (next)
   {
      next->prev = prev;
   }
   if (ref == mHeapAllocs)
   {
      mHeapAllocs = next;
   }
   Delete(ref);
}

ConsoleValue Vm::getStringFuncBuffer(U32 size)
{
   VmAllocTLS::Scope memScope(mInternal);
    return mInternal->getStringFuncBuffer(0, size);
}

ConsoleValue Vm::getStringReturnBuffer(U32 size)
{
   VmAllocTLS::Scope memScope(mInternal);
    return mInternal->getStringReturnBuffer(size);
}

ConsoleValue Vm::getTypeReturn(TypeId typeId, U32 heapSize)
{
   VmAllocTLS::Scope memScope(mInternal);
    return mInternal->getTypeReturn(typeId, heapSize);
}

ConsoleValue Vm::getTypeFunc(TypeId typeId, U32 heapSize)
{
   VmAllocTLS::Scope memScope(mInternal);
    return mInternal->getTypeFunc(0, typeId, heapSize);
}

ConsoleValue Vm::getStringInZone(U16 zone, U32 size)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->getStringInZone(zone, size);
}

ConsoleValue Vm::getTypeInZone(U16 zone, TypeId typeId, U32 heapSize)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->getTypeInZone(zone, typeId, heapSize);
}

ConsoleValue VmInternal::getStringInZone(U16 zone, U32 size)
{
   if (zone == ConsoleValue::ZoneReturn)
   {
      return getStringReturnBuffer(size);
   }
   else if (zone >= ConsoleValue::ZoneFiberStart)
   {
      U16 fiberId = (zone - ConsoleValue::ZoneFiberStart) >> 1;
      return getStringFuncBuffer(fiberId, size);
   }
   else
   {
      return ConsoleValue();
   }
}

ConsoleValue VmInternal::getTypeInZone(U16 zone, TypeId typeId, U32 heapSize)
{
   U32 size = mTypes[typeId].valueSize == UINT_MAX ? heapSize : mTypes[typeId].valueSize;
   if (zone == ConsoleValue::ZoneReturn)
   {
      return getStringReturnBuffer(size);
   }
   else if (zone >= ConsoleValue::ZoneFiberStart)
   {
      U16 fiberId = (zone - ConsoleValue::ZoneFiberStart) >> 1;
      return getStringFuncBuffer(fiberId, size);
   }
   else
   {
      return ConsoleValue();
   }
}

void VmInternal::validateReturnBufferSize(U32 size)
{
   if (mReturnBuffer.size() < size)
   {
      mReturnBuffer.resize(size + 2048);
      mAllocBase.arg = &mReturnBuffer[0];
   }
}

ConsoleValue VmInternal::getStringFuncBuffer(U32 fiberIndex, U32 size)
{
   return mFiberStates.mItems[fiberIndex] ? mFiberStates.mItems[fiberIndex]->mSTR.getFuncBuffer(KorkApi::ConsoleValue::TypeInternalString, size) : ConsoleValue();
}

ConsoleValue VmInternal::getStringReturnBuffer(U32 size)
{
   KorkApi::ConsoleValue ret;
   validateReturnBufferSize(size);
   ret.setTyped(0, KorkApi::ConsoleValue::TypeInternalString, KorkApi::ConsoleValue::ZoneReturn);
   return ret;
}

ConsoleValue VmInternal::getTypeFunc(U32 fiberIndex, TypeId typeId, U32 heapSize)
{
   U32 size = mTypes[typeId].valueSize == UINT_MAX ? heapSize : mTypes[typeId].valueSize;
   return mFiberStates.mItems[fiberIndex] ? mFiberStates.mItems[fiberIndex]->mSTR.getFuncBuffer(typeId, size) : ConsoleValue();
}

ConsoleValue VmInternal::getTypeReturn(TypeId typeId, U32 heapSize)
{
   KorkApi::ConsoleValue ret;
   U32 size = mTypes[typeId].valueSize == UINT_MAX ? heapSize : mTypes[typeId].valueSize;
   validateReturnBufferSize(size);
   ret.setTyped(0, KorkApi::ConsoleValue::TypeInternalString, KorkApi::ConsoleValue::ZoneReturn);
   return ret;
}



void Vm::pushValueFrame()
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->mCurrentFiberState->mSTR.pushFrame();
}

void Vm::popValueFrame()
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->mCurrentFiberState->mSTR.popFrame();
}

// Public
VMObject* Vm::constructObject(ClassId klassId, const char* name, int argc, const char** argv)
{
   ClassInfo* ci = &mInternal->mClassList[klassId];
   VMObject* object = mInternal->New<VMObject>();
   mInternal->incVMRef(object);

   CreateClassReturn ret = {};
   
   if (ci->iCreate.CreateClassFn)
   {
      object->klass = ci;
      object->ns = nullptr;
      ci->iCreate.CreateClassFn(ci->userPtr, this, &ret);
      object->userPtr = ret.userPtr;
      object->flags = ret.initialFlags;

      if (object->userPtr)
      {
         if (!ci->iCreate.ProcessArgsFn(this, object->userPtr, name, false, false, argc, argv))
         {
            ci->iCreate.DestroyClassFn(ci->userPtr, this, object->userPtr);
            return nullptr;
         }
         else
         {
            return object;
         }
      }
   }
   
   mInternal->decVMRef(object);
   return nullptr;
}

void Vm::setObjectNamespace(VMObject* object, NamespaceId nsId)
{
   object->ns = (Namespace*)nsId;
}

// Internal
VMObject* Vm::createVMObject(ClassId klassId, void* klassPtr)
{
   VmAllocTLS::Scope memScope(mInternal);
   VMObject* object = mInternal->New<VMObject>();
   mInternal->incVMRef(object);
   object->klass = &mInternal->mClassList[klassId];
   object->ns = nullptr;
   object->userPtr = klassPtr;
   return object;
}

void Vm::incVMRef(VMObject* object)
{
   VmAllocTLS::Scope memScope(mInternal);
   mInternal->incVMRef(object);
}

void Vm::decVMRef(VMObject* object)
{
   VmAllocTLS::Scope memScope(mInternal);
   mInternal->decVMRef(object);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, StringFuncCallback cb, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs)
{
   VmAllocTLS::Scope memScope(mInternal);
   Namespace* ns = (Namespace*)nsId;
   ns->addCommand(name, cb, userPtr, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, IntFuncCallback cb, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs)
{
   VmAllocTLS::Scope memScope(mInternal);
   Namespace* ns = (Namespace*)nsId;
   ns->addCommand(name, cb, userPtr, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, FloatFuncCallback cb, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs)
{
   VmAllocTLS::Scope memScope(mInternal);
   Namespace* ns = (Namespace*)nsId;
   ns->addCommand(name, cb, userPtr, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, VoidFuncCallback cb, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs)
{
   VmAllocTLS::Scope memScope(mInternal);
   Namespace* ns = (Namespace*)nsId;
   ns->addCommand(name, cb, userPtr, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, BoolFuncCallback cb, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs)
{
   VmAllocTLS::Scope memScope(mInternal);
   Namespace* ns = (Namespace*)nsId;
   ns->addCommand(name, cb, userPtr, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, ValueFuncCallback cb, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs)
{
   VmAllocTLS::Scope memScope(mInternal);
   Namespace* ns = (Namespace*)nsId;
   ns->addCommand(name, cb, userPtr, usage, minArgs, maxArgs);
}

bool Vm::isNamespaceFunction(NamespaceId nsId, StringTableEntry name)
{
   VmAllocTLS::Scope memScope(mInternal);
   Namespace* ns = (Namespace*)nsId;
   return ns->lookup(name) != nullptr;
}

void Vm::markNamespaceGroup(NamespaceId nsId, StringTableEntry groupName, StringTableEntry usage)
{
   VmAllocTLS::Scope memScope(mInternal);
   Namespace* ns = (Namespace*)nsId;
   return ns->markGroup(groupName, usage);
}

bool Vm::compileCodeBlock(const char* code, const char* filename, CompiledBlock* outBlock)
{
   VmAllocTLS::Scope memScope(mInternal);
   CodeBlock* block = mInternal->New<CodeBlock>(mInternal, false);
   
   U8* buffer = (U8*)mInternal->NewArray<U8>(1024 * 1024);
   outBlock->data = nullptr;
   outBlock->size = 0;
   MemStream outS(1024*1024, buffer, true, true);
   
   if (!block->compileToStream(outS, filename, code))
   {
      mInternal->Delete(block);
      mInternal->Delete(buffer); // []
      return -1;
   }
   
   outBlock->data = buffer;
   outBlock->size = outS.getPosition();
   return true;
}

void Vm::freeCompiledBlock(CompiledBlock block)
{
   if (block.data)
   {
      mInternal->DeleteArray(block.data);
   }
}

ConsoleValue Vm::execCodeBlock(U32 codeSize, U8* code, const char* filename, const char* modPath, bool noCalls, int setFrame)
{
   VmAllocTLS::Scope memScope(mInternal);
   CodeBlock* block = mInternal->New<CodeBlock>(mInternal, (filename == nullptr || *filename == '\0') ? true : false);
   
   MemStream stream(codeSize, code, true, false);
   
   if (!block->read(mInternal->internString(filename, false),
                    mInternal->internString(modPath, false),
                    stream, 0))
   {
      mInternal->Delete(block);
      return ConsoleValue();
   }
   
   return block->exec(0, filename, nullptr, 0, 0, noCalls, true, nullptr, setFrame);
}

ConsoleValue Vm::evalCode(const char* code, const char* filename, const char* modPath, S32 setFrame)
{
   VmAllocTLS::Scope memScope(mInternal);
   CodeBlock *newCodeBlock = mInternal->New<CodeBlock>(mInternal, (filename == nullptr || *filename == '\0') ? true : false);
   return newCodeBlock->compileExec(mInternal->internString(filename, false),
                                    mInternal->internString(modPath, false),
                                    code, false, true, (!filename || setFrame < 0) ? -1 : setFrame);
}

ConsoleValue Vm::call(int argc, ConsoleValue* argv, bool startSuspended)
{
   ConsoleValue retValue = ConsoleValue();
   callNamespaceFunction(getGlobalNamespace(), mInternal->internString(mInternal->valueAsString(argv[0]), false), argc, argv, retValue, startSuspended);
   return retValue;
}

ConsoleValue Vm::callObject(VMObject* h, int argc, ConsoleValue* argv, bool startSuspended)
{
   ConsoleValue retValue = ConsoleValue();
   callObjectFunction(h, mInternal->internString(mInternal->valueAsString(argv[0]), false), argc, argv, retValue, startSuspended);
   return retValue;
}

bool Vm::callObjectFunction(VMObject* self, StringTableEntry funcName, int argc, KorkApi::ConsoleValue* argv, ConsoleValue& retValue, bool startSuspended)
{
   VmAllocTLS::Scope memScope(mInternal);
   char idBuf[16];
   if (argc < 2)
   {
      // no args
      return false;
   }
   else if (!self)
   {
      // no object
      return false;
   }
   else if (!self->ns)
   {
      // no ns
      mInternal->printf(0, " Vm::callObjectFunction - %d has no namespace: %s", self->klass->iCreate.GetIdFn(self), argv[0]);
      return false;
   }

   Namespace::Entry *ent = self->ns->lookup(funcName);

   if(ent == nullptr)
   {
      mInternal->printf(0, "%s: undefined for object id %d", funcName, self->klass->iCreate.GetIdFn(self));

      // Clean up arg buffers, if any.
      mInternal->mCurrentFiberState->mSTR.clearFunctionOffset();
      return "";
   }

   // Twiddle %this argument
   KorkApi::ConsoleValue oldArg1 = argv[1];
   SimObjectId cv = self->klass->iCreate.GetIdFn(self);
   argv[1] = KorkApi::ConsoleValue::makeUnsigned(cv);
   
   // NOTE: previously it was possible to destroy vm objects during execute, however VMObject itself is now
   // refCounted. Any further checks regarding this should be done at a higher level.
   
   KorkApi::ConsoleValue ret = ent->execute(argc, argv, mInternal->mCurrentFiberState, self, startSuspended);
   
   retValue = ret;

   // Twiddle it back
   argv[1] = oldArg1;

   // Reset the function offset so the stack
   // doesn't continue to grow unnecessarily
   mInternal->mCurrentFiberState->mSTR.clearFunctionOffset();

   return true;
}

bool Vm::callNamespaceFunction(NamespaceId nsId, StringTableEntry name, int argc, KorkApi::ConsoleValue* argv, ConsoleValue& retValue, bool startSuspended)
{
   VmAllocTLS::Scope memScope(mInternal);
   Namespace* ns = (Namespace*)nsId;
   Namespace::Entry* ent = ns->lookup(name);

   if (!ent)
   {
      mInternal->printf(0, "%s: Unknown command.", valueAsString(argv[0]));
      // Clean up arg buffers, if any.
      mInternal->mCurrentFiberState->mSTR.clearFunctionOffset();
      return false;
   }

   retValue = ent->execute(argc, argv, mInternal->mCurrentFiberState, nullptr, startSuspended);

   // Reset the function offset so the stack
   // doesn't continue to grow unnecessarily
   mInternal->mCurrentFiberState->mSTR.clearFunctionOffset();

   return true;
}

// Helpers (should call into user funcs)
VMObject* Vm::findObjectByName(const char* name)
{
    return mInternal->mConfig.iFind.FindObjectByNameFn(mInternal->mConfig.findUser, name, nullptr);
}

VMObject* Vm::findObjectByPath(const char* path)
{
    return mInternal->mConfig.iFind.FindObjectByPathFn(mInternal->mConfig.findUser, path);
}

VMObject* Vm::findObjectById(SimObjectId ident)
{
   return mInternal->mConfig.iFind.FindObjectByIdFn(mInternal->mConfig.findUser, ident);
}

bool Vm::setObjectField(VMObject* object, StringTableEntry fieldName, ConsoleValue nativeValue, ConsoleValue arrayIndex)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->setObjectField(object, fieldName, arrayIndex, nativeValue);
}

bool Vm::setObjectFieldTuple(VMObject* object, StringTableEntry fieldName, U32 argc, ConsoleValue* argv, ConsoleValue arrayIndex)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->setObjectFieldTuple(object, fieldName, arrayIndex, argc, argv);
}

bool Vm::setObjectFieldString(VMObject* object, StringTableEntry fieldName, const char* stringValue, ConsoleValue arrayIndex)
{
   VmAllocTLS::Scope memScope(mInternal);
   ConsoleValue val = ConsoleValue::makeString(stringValue);
   return mInternal->setObjectField(object, fieldName, arrayIndex, val);
}

ConsoleValue Vm::getObjectField(VMObject* object, StringTableEntry fieldName, ConsoleValue arrayIndex)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->getObjectField(object, fieldName, arrayIndex, KorkApi::TypeDirectCopy, KorkApi::ConsoleValue::ZoneExternal);
}

const char* Vm::getObjectFieldString(VMObject* object, StringTableEntry fieldName, ConsoleValue arrayIndex)
{
   VmAllocTLS::Scope memScope(mInternal);
   ConsoleValue foundValue = mInternal->getObjectField(object, fieldName, arrayIndex, KorkApi::TypeDirectCopy, KorkApi::ConsoleValue::ZoneFunc);
   return (const char*)foundValue.evaluatePtr(mInternal->mAllocBase);
}

void Vm::setGlobalVariable(StringTableEntry name, KorkApi::ConsoleValue value)
{
   VmAllocTLS::Scope memScope(mInternal);
   mInternal->mGlobalVars.setVariableValue(name, value);
}

ConsoleValue Vm::getGlobalVariable(StringTableEntry name)
{
   VmAllocTLS::Scope memScope(mInternal);
   Dictionary::Entry* e = mInternal->mGlobalVars.getVariable(name);
   
   if (!e)
   {
      return ConsoleValue();
   }
   
   return mInternal->mGlobalVars.getEntryValue(e);
}

void Vm::setLocalVariable(StringTableEntry name, KorkApi::ConsoleValue value)
{
   VmAllocTLS::Scope memScope(mInternal);
   mInternal->mCurrentFiberState->setLocalFrameVariable(name, value);
}

ConsoleValue Vm::getLocalVariable(StringTableEntry name)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->mCurrentFiberState->getLocalFrameVariable(name);
}


bool Vm::registerGlobalVariable(StringTableEntry name, S32 type, void *dptr, const char* usage)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->mGlobalVars.addVariable( name, type, dptr, usage );
}

bool Vm::removeGlobalVariable(StringTableEntry name)
{
   VmAllocTLS::Scope memScope(mInternal);
   return name!=0 && mInternal->mGlobalVars.removeVariable(name);
}

ConsoleValue::AllocBase Vm::getAllocBase() const
{
   return mInternal->mAllocBase;
}

bool Vm::isTracing()
{
   return mInternal->mCurrentFiberState->traceOn;
}

S32 Vm::getTracingStackPos()
{
   return mInternal->mCurrentFiberState->vmFrames.size();
}

void Vm::setTracing(bool value)
{
   mInternal->mCurrentFiberState->traceOn = value;
}


// -------------------- factory functions --------------------

Vm* createVM(Config* cfg)
{
   if (cfg->mallocFn == nullptr || cfg->freeFn == nullptr)
   {
      return nullptr;
   }
   
   Vm* vm = (Vm*)cfg->mallocFn(sizeof(Vm), cfg->allocUser);
   constructInPlace(vm);
   
   vm->mInternal = (VmInternal*)cfg->mallocFn(sizeof(VmInternal), cfg->allocUser);
   VmAllocTLS::Scope memScope(vm->mInternal);
   vm->mInternal->mConfig = *cfg;
   constructInPlace(vm->mInternal, vm, cfg);
   
   return vm;
}

void destroyVM(Vm* vm)
{
   FreeFn freeFn = vm->mInternal->mConfig.freeFn;
   void* freeUser = vm->mInternal->mConfig.allocUser;
   
   destructInPlace(vm->mInternal);
   freeFn(vm->mInternal, freeUser);
   destructInPlace(vm);
   freeFn(vm, freeUser);
}


static KorkApi::ConsoleValue performOpNumeric(void* userPtr, KorkApi::Vm* vm, U32 op, KorkApi::ConsoleValue lhs, KorkApi::ConsoleValue rhs)
{
   F64 valueL = vm->valueAsFloat(lhs);
   F64 valueR = vm->valueAsFloat(rhs);
   
   switch (op)
   {
      // unary
      case Compiler::OP_NOT:
         valueL = !((U64)valueL);
         break;
      case Compiler::OP_NOTF:
         valueL = !valueL;
         break;
      case Compiler::OP_ONESCOMPLEMENT:
         valueL = ~((U64)valueL);
         break;
      case Compiler::OP_NEG:
         valueL = -valueL;
         break;
         
      // comparisons (return 0/1)
      case Compiler::OP_CMPEQ: valueL = (valueL == valueR) ? 1.0f : 0.0f; break;
      case Compiler::OP_CMPNE: valueL = (valueL != valueR) ? 1.0f : 0.0f; break;
      case Compiler::OP_CMPGR: valueL = (valueL >  valueR) ? 1.0f : 0.0f; break;
      case Compiler::OP_CMPGE: valueL = (valueL >= valueR) ? 1.0f : 0.0f; break;
      case Compiler::OP_CMPLT: valueL = (valueL <  valueR) ? 1.0f : 0.0f; break;
      case Compiler::OP_CMPLE: valueL = (valueL <= valueR) ? 1.0f : 0.0f; break;
      
      // bitwise (operate on integer views)
      case Compiler::OP_XOR:
         valueL = (F32)(((U64)valueL) ^ ((U64)valueR));
         break;
         
      case Compiler::OP_BITAND:
         valueL = (F32)(((U64)valueL) & ((U64)valueR));
         break;
         
      case Compiler::OP_BITOR:
         valueL = (F32)(((U64)valueL) | ((U64)valueR));
         break;
         
      case Compiler::OP_SHR:
      {
         const U64 a = (U64)valueL;
         const U64 b = (U64)valueR;
         valueL = (F32)(a >> b);
         break;
      }
         
      case Compiler::OP_SHL:
      {
         const U64 a = (U64)valueL;
         const U64 b = (U64)valueR;
         valueL = (F32)(a << b);
         break;
      }
         
      // logical (return 0/1)
      case Compiler::OP_AND:
         valueL = (valueL != 0.0f && valueR != 0.0f) ? 1.0f : 0.0f;
         break;
         
      case Compiler::OP_OR:
         valueL = (valueL != 0.0f || valueR != 0.0f) ? 1.0f : 0.0f;
         break;
         
      // arithmetic
      case Compiler::OP_ADD: valueL = valueL + valueR; break;
      case Compiler::OP_SUB: valueL = valueL - valueR; break;
      case Compiler::OP_MUL: valueL = valueL * valueR; break;
         
      case Compiler::OP_DIV:
         valueL = (valueR == 0.0f) ? 0.0f : (valueL / valueR);
         break;
         
      case Compiler::OP_MOD:
      {
         const U64 a = (U64)valueL;
         const U64 b = (U64)valueR;
         valueL = (b == 0u) ? 0.0f : (F32)(a % b);
         break;
      }
         
      default:
         break;
   }
   
   return KorkApi::ConsoleValue::makeNumber(valueL);
}

VmInternal::VmInternal(Vm* vm, Config* cfg) : mGlobalVars(this)
{
   mVM = vm;
   mLocalIntern = nullptr;
   mConfig = *cfg;
   mCodeBlockList = nullptr;
   mCurrentCodeBlock = nullptr;
   mReturnBuffer.resize(2048);
   mNSState.init(this);
   
   if (cfg->maxFibers == 0)
   {
      cfg->maxFibers = 1024;
   }
   mAllocBase.func = NewArray<void*>(cfg->maxFibers);
   mAllocBase.arg = &mReturnBuffer[0];
   memset(mAllocBase.func, 0, sizeof(void*)*cfg->maxFibers);

   if (mConfig.initTelnet)
   {
      mTelDebugger = New<TelnetDebugger>(this);
      mTelConsole = New<TelnetConsole>(this);
   }
   else
   {
      mTelDebugger = nullptr;
      mTelConsole = nullptr;
   }
   
   // Use inbuilt string interner

   if (mConfig.iIntern.intern == nullptr) 
   {
      mLocalIntern = new SimpleStringInterner();
      mConfig.iIntern.intern = [](void* user, const char* value, bool caseSens){
         SimpleStringInterner* localIntern = (SimpleStringInterner*)user;
         if (value == nullptr)
         {
            return localIntern->empty();
         }
         return (StringTableEntry)localIntern->internSV(std::string_view(value), caseSens);
      };
      mConfig.iIntern.internN = [](void* user, const char* value, size_t len, bool caseSens){
         SimpleStringInterner* localIntern = (SimpleStringInterner*)user;
         if (value == nullptr)
         {
            return localIntern->empty();
         }
         return (StringTableEntry)localIntern->internSV(std::string_view(value, len), caseSens);
      };
      mConfig.iIntern.lookup = [](void* user, const char* value, bool caseSens){
         SimpleStringInterner* localIntern = (SimpleStringInterner*)user;
         if (value == nullptr)
         {
            return localIntern->empty();
         }
         return (StringTableEntry)localIntern->lookupSV(std::string_view(value), caseSens);
      };
      mConfig.iIntern.lookupN = [](void* user, const char* value, size_t len, bool caseSens){
         SimpleStringInterner* localIntern = (SimpleStringInterner*)user;
         if (value == nullptr)
         {
            return localIntern->empty();
         }
         return (StringTableEntry)localIntern->lookupSV(std::string_view(value, len), caseSens);
      };
      mConfig.internUser = mLocalIntern;
   }
   mHeapAllocs = nullptr;
   mConvIndex = 0;
   mCVConvIndex = 0;
   mNSCounter = 0;

   if (cfg->userResources)
   {
      mCompilerResources = cfg->userResources;
      mOwnsResources = false;
   }
   else
   {
      mCompilerResources = New<Compiler::Resources>();
      mOwnsResources = true;
   }

   mCompilerResources->emptyString = internString("", false);

   mCompilerResources->allowExceptions = cfg->enableExceptions;
   mCompilerResources->allowTuples = cfg->enableTuples;
   mCompilerResources->allowTypes = cfg->enableTypes;
   mCompilerResources->allowStringInterpolation = cfg->enableStringInterpolation;
   mLastExceptionInfo = {};
   
   TypeInfo typeInfo = {};
   
   typeInfo.name = internString("string", false);
   typeInfo.inspectorFieldType = nullptr;
   typeInfo.userPtr = nullptr;
   typeInfo.fieldSize = sizeof(const char*);
   typeInfo.valueSize = UINT_MAX;
   
   auto noOpFunc = [](void* userPtr, Vm* vm, U32 op, ConsoleValue lhs, ConsoleValue rhs){
      return lhs;
   };
   
   // NOTE: we expect the storageRegister to be the effective "return value" here since
   // cast is never used for fields.
   auto genericCastFunc = [](void* userPtr,
                             Vm* vm,
                               TypeStorageInterface* inputStorage,
                               TypeStorageInterface* outputStorage,
                             void* fieldUserPtr,
                             BitSet32 flag,
                             U32 requestedType) {
     switch (requestedType)
     {
        case KorkApi::ConsoleValue::TypeInternalString: {
           const char* strValue = vm->valueAsString(inputStorage->data.storageRegister ? *inputStorage->data.storageRegister : inputStorage->data.storageAddress);
           U32 strLen = strlen(strValue)+1;
           outputStorage->ResizeStorage(outputStorage, strLen);
           memcpy(outputStorage->data.storageAddress.evaluatePtr(vm->getAllocBase()), strValue, strLen);
           *(outputStorage->data.storageRegister) = outputStorage->data.storageAddress;
        }
           break;
        case KorkApi::ConsoleValue::TypeInternalNumber:
           *(outputStorage->data.storageRegister) = KorkApi::ConsoleValue::makeNumber(inputStorage->data.storageRegister ? vm->valueAsFloat(*inputStorage->data.storageRegister) : 0.0f);
           break;
        case KorkApi::ConsoleValue::TypeInternalUnsigned:
           *(outputStorage->data.storageRegister) = KorkApi::ConsoleValue::makeUnsigned(inputStorage->data.storageRegister ? (U64)vm->valueAsInt(*inputStorage->data.storageRegister) : 0);
           break;
        default:
           return false;
     }
      
      return true;
   };
   
   typeInfo.iFuncs.CastValueFn = genericCastFunc;
   
   typeInfo.iFuncs.PerformOpFn = performOpNumeric;
   
   // TODO
   
   mTypes.push_back(typeInfo);
   
   typeInfo.name = internString("float", false);
   typeInfo.inspectorFieldType = nullptr;
   typeInfo.userPtr = nullptr;
   typeInfo.fieldSize = typeInfo.valueSize = sizeof(F64);
   // TODO
   
   mTypes.push_back(typeInfo);
   
   typeInfo.name = internString("uint", false);
   typeInfo.inspectorFieldType = nullptr;
   typeInfo.userPtr = nullptr;
   typeInfo.fieldSize = typeInfo.valueSize = sizeof(U64);
   // TODO
   
   mTypes.push_back(typeInfo);
   
   // Config Stubs
   
   if (mConfig.logFn == nullptr) {
      mConfig.logFn = [](U32 level, const char* buffer, void* user) {
      };
   }
   if (mConfig.addTagFn == nullptr) {
      mConfig.addTagFn = [](const char* vmString, void* user) {
         return (U32)0;
      };
   }

   if (mConfig.iFind.FindObjectByNameFn == nullptr) {
      mConfig.iFind.FindObjectByNameFn = [](void* userPtr, StringTableEntry name, VMObject* parent) {
           return (VMObject*)nullptr;
       };
   }

   if (mConfig.iFind.FindObjectByPathFn == nullptr) {
      mConfig.iFind.FindObjectByPathFn = [](void* userPtr, const char* path) {
         return (VMObject*)nullptr;
       };
   }

   if (mConfig.iFind.FindObjectByInternalNameFn == nullptr) {
      mConfig.iFind.FindObjectByInternalNameFn = [](void* userPtr, StringTableEntry name, bool recursive, VMObject* parent) {
         return (VMObject*)nullptr;
       };
   }

   if (mConfig.iFind.FindObjectByIdFn == nullptr) {
      mConfig.iFind.FindObjectByIdFn = [](void* userPtr, SimObjectId ident) {
         return (VMObject*)nullptr;
       };
   }

   // Init empty string

   mEmptyString = internString("", false);
   
   // Setup base fiber

   FiberId baseFiber = createFiber(nullptr);
   mCurrentFiberState = mFiberStates.mItems[0];
   if (mTelDebugger)
   {
      mTelDebugger->setWatchFiberFromVm();
   }
}

VmInternal::~VmInternal()
{
   ExprEvalState* mCurrentFiberState;
   InternalFiberList mFiberStates;
   ClassChunker<ExprEvalState> mFiberAllocator;

   Delete(mAllocBase.func);
   
   
   Delete(mTelDebugger);
   Delete(mTelConsole);
   
   if (mOwnsResources)
   {
      Delete(mCompilerResources);
   }
   mNSState.shutdown();
   
   // Cleanup remaining fibers
   for (Vector<ExprEvalState*>::iterator itr = mFiberStates.mItems.begin(), itrEnd = mFiberStates.mItems.end(); itr != itrEnd; itr++)
   {
      Delete(*itr);
      *itr = nullptr;
   }
   mFiberStates.clear();
   mFiberAllocator.freeBlocks();

   for (ConsoleHeapAlloc* alloc = mHeapAllocs; alloc; alloc = alloc->next)
   {
      mConfig.freeFn(alloc, mConfig.allocUser);
   }
   mHeapAllocs = nullptr;

   if (mLocalIntern)
   {
      Delete(mLocalIntern);
   }
}


void VmInternal::setCurrentFiberMain()
{
   ExprEvalState* state = mFiberStates.mItems[0];
   if (state)
   {
      mCurrentFiberState = state;
   }
}

void VmInternal::setCurrentFiber(FiberId fiber)
{
   ExprEvalState* state = mFiberStates.getItem(fiber);
   if (state)
   {
      mCurrentFiberState = state;
   }
}

FiberId VmInternal::createFiber(void* userPtr)
{
   ExprEvalState* newState = New<ExprEvalState>(this);
   InternalFiberList::HandleType handle = mFiberStates.allocListHandle(newState);
   newState->mSTR.initForFiber(handle.getIndex());
   newState->mUserPtr = userPtr;
   return handle.getWeakValue();
}

ExprEvalState* VmInternal::createFiberPtr(void* userPtr)
{
   ExprEvalState* newState =  New<ExprEvalState>(this);
   InternalFiberList::HandleType handle = mFiberStates.allocListHandle(newState);
   newState->mSTR.initForFiber(handle.getIndex());
   newState->mUserPtr = userPtr;
   return newState;
}

FiberId VmInternal::getCurrentFiber()
{
   return mCurrentFiberState ? mFiberStates.getHandleValue(mCurrentFiberState) : 0;
}

void VmInternal::cleanupFiber(FiberId fiber)
{
   InternalFiberList::HandleType vh = InternalFiberList::HandleType::fromValue(fiber);
   ExprEvalState* state = mFiberStates.getItem(fiber);
   if (state && state != mFiberStates.mItems[0])
   {
      mFiberStates.freeListPtr(state);
      Delete(state);
      mAllocBase.func[vh.getIndex()] = nullptr;
   }
}

FiberRunResult VmInternal::resumeCurrentFiber(ConsoleValue value)
{
   if (mCurrentFiberState == nullptr)
   {
      FiberRunResult r = {};
      return r;
   }
   
   return mCurrentFiberState->resume(value);
}

void VmInternal::suspendCurrentFiber()
{
   if (mCurrentFiberState == nullptr)
      return;
   
   mCurrentFiberState->suspend();
}

void Vm::throwFiber(U32 mask)
{
   VmAllocTLS::Scope memScope(mInternal);
   mInternal->throwFiber(mask);
}

void VmInternal::throwFiber(U32 mask)
{
   if (mCurrentFiberState == nullptr)
      return;
   
   mCurrentFiberState->throwMask(mask);
}

S32 Vm::getCurrentFiberFrameDepth()
{
   return mInternal->mCurrentFiberState ? mInternal->mCurrentFiberState->vmFrames.size()-1 : -1;
}

bool Vm::getCurrentFiberFileLine(StringTableEntry* outFile, U32* outLine)
{
   return mInternal->getCurrentFiberFileLine(outFile, outLine);
}

FiberRunResult::State VmInternal::getCurrentFiberState()
{
   return mCurrentFiberState ? mCurrentFiberState->mState : FiberRunResult::ERROR;
}

FiberRunResult::State VmInternal::getFiberState(KorkApi::FiberId fid)
{
   ExprEvalState* state = mFiberStates.getItem(fid);
   return state ? state->mState : FiberRunResult::State::ERROR;
}

void VmInternal::clearCurrentFiberError()
{
   // NOTE: this is needed as its possible for native functions to throw errors,
   // but we want to continue executing so long as we have active frames.
   // (e.g. throw from an eval)
   if (mCurrentFiberState &&
       mCurrentFiberState->mState == FiberRunResult::ERROR &&
       mCurrentFiberState->vmFrames.size() > 0)
   {
      mCurrentFiberState->mState = FiberRunResult::RUNNING;
      mCurrentFiberState->lastThrow = 0;
   }
}

void* VmInternal::getCurrentFiberUserPtr()
{
   return mCurrentFiberState ? mCurrentFiberState->mUserPtr : nullptr;
}

StringTableEntry VmInternal::getCurrentCodeBlockName()
{
   if (mCurrentCodeBlock)
      return mCurrentCodeBlock->name;
   else
      return nullptr;
}

StringTableEntry VmInternal::getCurrentCodeBlockFullPath()
{
   if (mCurrentCodeBlock)
      return mCurrentCodeBlock->fullPath;
   else
      return nullptr;
}

StringTableEntry VmInternal::getCurrentCodeBlockModName()
{
   if (mCurrentCodeBlock)
      return mCurrentCodeBlock->modPath;
   else
      return nullptr;
}

CodeBlock *VmInternal::findCodeBlock(StringTableEntry name)
{
   for(CodeBlock *walk = mCodeBlockList; walk; walk = walk->nextFile)
      if(walk->name == name)
         return walk;
   return nullptr;
}

ClassInfo* VmInternal::getClassInfoByName(StringTableEntry name)
{
   auto itr = std::find_if(mClassList.begin(), mClassList.end(), [name](ClassInfo& info){
      return info.name == name;
   });

   if (itr != mClassList.end())
   {
      return &*itr;
   }
   else
   {
      return nullptr;
   }
}


const char* VmInternal::tempFloatConv(F64 val)
{
   if (mConvIndex == MaxStringConvs)
      mConvIndex = 0;

   snprintf(mTempStringConversions[mConvIndex], MaxTempStringSize, "%.9g", val);
   return mTempStringConversions[mConvIndex++];
}

const char* VmInternal::tempIntConv(U64 val)
{
   if (mConvIndex == MaxStringConvs)
      mConvIndex = 0;

   snprintf(mTempStringConversions[mConvIndex], MaxTempStringSize, "%" PRIu64, val);
   return mTempStringConversions[mConvIndex++];
}

const char* VmInternal::tempStringConv(const char* val)
{
   if (mConvIndex == MaxStringConvs)
      mConvIndex = 0;

   snprintf(mTempStringConversions[mConvIndex], MaxTempStringSize, "%s", val);
   return mTempStringConversions[mConvIndex++];
}

KorkApi::ConsoleValue* VmInternal::getTempValuePtr()
{
   if (mCVConvIndex == MaxStringConvs)
      mCVConvIndex = 0;

   return &mTempConversionValue[mCVConvIndex++];
}

bool VmInternal::setObjectField(VMObject* obj, StringTableEntry name, KorkApi::ConsoleValue array, ConsoleValue value)
{
   return setObjectFieldTuple(obj, name, array, 1, &value);
}

bool VmInternal::setObjectFieldTuple(VMObject* obj, StringTableEntry fieldName, KorkApi::ConsoleValue arrayIndex, U32 argc, ConsoleValue* argv)
{
   if ((obj->flags & KorkApi::ModStaticFields) != 0)
   {
      for (U32 i=0; i<obj->klass->numFields; i++)
      {
         FieldInfo& f = obj->klass->fields[i];
         
         if (f.pFieldname != fieldName)
            continue;
         
         TypeId tid = f.type;
         if (tid < 0 || (U32)tid >= mTypes.size())
            break;
         
         TypeInfo& tinfo = mTypes[tid];
         if (!tinfo.iFuncs.CastValueFn || tinfo.fieldSize == 0)
            break;

         TypeStorageInterface outputStorage;

         if (f.allocStorageFn)
         {
            if (!f.allocStorageFn(this->mVM, obj->userPtr, &f, arrayIndex, &outputStorage, true))
            {
               return false;
            }
         }
         else
         {
            U32 idx = valueAsInt(arrayIndex);
            U32 elemCount = f.elementCount > 0 ? (U32)f.elementCount : 1;
            if (idx >= elemCount)
               break;
            
            U8* base = static_cast<U8*>(obj->userPtr);
            U8* dptr = base + f.offset + (idx * (U32)tinfo.fieldSize);
            outputStorage = KorkApi::CreateFixedTypeStorage(this, dptr, tid, true);
         }
         
         CastValueFnType castFn = f.ovrCastValue ? f.ovrCastValue : tinfo.iFuncs.CastValueFn;

         TypeStorageInterface inputStorage = KorkApi::CreateRegisterStorageFromArgs(this, argc, argv);
         
         outputStorage.fieldObject = obj->userPtr;
         
         castFn(tinfo.userPtr,
               mVM,
               &inputStorage,
               &outputStorage,
               f.fieldUserPtr,
               f.flag,
               tid);
      }
   }

   if ((obj->flags & KorkApi::ModDynamicFields) != 0)
   {
      obj->klass->iCustomFields.SetCustomFieldByName(mVM, obj, fieldName, valueAsString(arrayIndex), argc, argv);
      return true;
   }
   
   return false;
}

ConsoleValue VmInternal::getObjectField(VMObject* obj, StringTableEntry name, KorkApi::ConsoleValue array, U32 requestedType, U32 requestedZone)
{
   // Default result if nothing matches.
   ConsoleValue def;
   if (!obj || !obj->klass || !obj->klass->fields)
      return def;
   
   for (U32 i = 0; i < obj->klass->numFields; ++i)
   {
      FieldInfo& f = obj->klass->fields[i];

      if (f.pFieldname != name)
         continue;

      TypeId tid = (TypeId)f.type;
      if (tid < 0 || (U32)tid >= mTypes.size())
         break;

      TypeInfo& tinfo = mTypes[tid];

      if (!tinfo.iFuncs.CastValueFn || tinfo.fieldSize == 0)
         return def;

      TypeStorageInterface inputStorage;

      if (f.allocStorageFn)
      {
         if (!f.allocStorageFn(this->mVM, obj, &f, array, &inputStorage, false))
         {
            return def;
         }
      }
      else
      {  
         U32 idx = valueAsInt(array);
         U32 elemCount = f.elementCount > 0 ? (U32)f.elementCount : 1;
         if (idx >= elemCount)
         {
            return def;
         }

         U8* base = static_cast<U8*>(obj->userPtr);
         U8* dptr = base + f.offset + (idx * (U32)tinfo.fieldSize);

         inputStorage = KorkApi::CreateFixedTypeStorage(this, dptr, tid, true);
      }
      
      // Add requested type
      if ((requestedType & KorkApi::TypeDirectCopy) != 0)
      {
         requestedType |= f.type;
      }

      TypeStorageInterface outputStorage = KorkApi::CreateExprEvalReturnTypeStorage(this, 0, 0);
      
      CastValueFnType castFn = f.ovrCastValue ? f.ovrCastValue : tinfo.iFuncs.CastValueFn;
      
      inputStorage.fieldObject = obj->userPtr;

      // For fixed size types, ensure we are the correct size (avoids adding check in CastValueFn)
      if (tinfo.valueSize != UINT_MAX && tinfo.valueSize > 0)
      {
         outputStorage.FinalizeStorage(&outputStorage, (U32)tinfo.valueSize);
      }
      
      castFn(
         tinfo.userPtr,
         mVM,
         &inputStorage,
         &outputStorage,
         f.fieldUserPtr,
         f.flag,
         requestedType
      );
      
      return *outputStorage.data.storageRegister;
   }

   // Try dynamic fields
   return obj->klass->iCustomFields.GetFieldByName(mVM, obj, name);
}

U16 VmInternal::getObjectFieldType(VMObject* obj, StringTableEntry name, KorkApi::ConsoleValue array)
{
   // Default result if nothing matches.
   if (!obj || !obj->klass || !obj->klass->fields)
      return 0;
   
   for (U32 i = 0; i < obj->klass->numFields; ++i)
   {
      FieldInfo& f = obj->klass->fields[i];

      if (f.pFieldname != name)
         continue;

      TypeId tid = (TypeId)f.type;
      if (tid < 0 || (U32)tid >= mTypes.size())
         break;

      return tid;
   }
   
   return 0;
}

void Vm::assignFieldsFromTo(VMObject* from, VMObject* to)
{
   return mInternal->assignFieldsFromTo(from, to);
}

F64 Vm::valueAsFloat(ConsoleValue v)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->valueAsFloat(v);
}

S64 Vm::valueAsInt(ConsoleValue v)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->valueAsInt(v);
}

bool Vm::valueAsBool(ConsoleValue v)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->valueAsBool(v);
}

const char* Vm::valueAsString(ConsoleValue v)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->valueAsString(v);
}

void* Vm::getUserPtr() const
{
   return mInternal->mConfig.vmUser;
}


void VmInternal::assignFieldsFromTo(VMObject* from, VMObject* to)
{
   // TODO
}

F64 VmInternal::valueAsFloat(ConsoleValue v)
{
   switch (v.typeId)
   {
      case KorkApi::ConsoleValue::TypeInternalUnsigned:
      return v.getInt();
      break;
      case KorkApi::ConsoleValue::TypeInternalNumber:
      return v.getFloat();
      break;
      case KorkApi::ConsoleValue::TypeInternalString:
      {
         const char* ptr = (const char*)v.evaluatePtr(mAllocBase);
         return ptr ? strtod(ptr, nullptr) : 0.0;
      }
      break;
      default:
         {
               KorkApi::TypeStorageInterface inputStorage = KorkApi::CreateRegisterStorageFromArg(this, v);

               KorkApi::TypeStorageInterface outputStorage = KorkApi::CreateRegisterStorage(this,
                                                                                       KorkApi::ConsoleValue::TypeInternalNumber);
               
               // NOTE: types should set head of stack to value if data pointer is nullptr in this case
               mTypes[v.typeId].iFuncs.CastValueFn(mTypes[v.typeId].userPtr,
                                                                   mVM,
                                                                   &inputStorage,
                                                                   &outputStorage,
                                                                   nullptr,
                                                                   0,
                                                                   KorkApi::ConsoleValue::TypeInternalNumber);

               return outputStorage.data.storageRegister->quickCastToNumeric();
         }
         break;
   }
   return 0.0f;
}

S64 VmInternal::valueAsBool(ConsoleValue v)
{
   switch (v.typeId)
   {
      case KorkApi::ConsoleValue::TypeInternalUnsigned:
      return v.getInt();
      break;
      case KorkApi::ConsoleValue::TypeInternalNumber:
      return v.getFloat();
      break;
      case KorkApi::ConsoleValue::TypeInternalString:
      {
         const char* ptr = (const char*)v.evaluatePtr(mAllocBase);
         return ptr ? dAtob(ptr) : false;
      }
      break;
      default:
         {
               KorkApi::TypeStorageInterface inputStorage = KorkApi::CreateRegisterStorageFromArg(this, v);

               KorkApi::TypeStorageInterface outputStorage = KorkApi::CreateRegisterStorage(this,
                                                                                    KorkApi::ConsoleValue::TypeInternalUnsigned);
               
               // NOTE: types should set head of stack to value if data pointer is nullptr in this case
               mTypes[v.typeId].iFuncs.CastValueFn(mTypes[v.typeId].userPtr,
                                                                   mVM,
                                                                   &inputStorage,
                                                                   &outputStorage,
                                                                   nullptr,
                                                                   0,
                                                                   KorkApi::ConsoleValue::TypeInternalUnsigned);

               return outputStorage.data.storageRegister->quickCastToNumeric();
         }
         break;
   }
   return 0;
}


S64 VmInternal::valueAsInt(ConsoleValue v)
{
   switch (v.typeId)
   {
      case KorkApi::ConsoleValue::TypeInternalUnsigned:
      return v.getInt();
      break;
      case KorkApi::ConsoleValue::TypeInternalNumber:
      return v.getFloat();
      break;
      case KorkApi::ConsoleValue::TypeInternalString:
      {
         const char* ptr = (const char*)v.evaluatePtr(mAllocBase);
         return ptr ? strtoll(ptr, nullptr, 10) : 0;
      }
      break;
      default:
         {
               KorkApi::TypeStorageInterface inputStorage = KorkApi::CreateRegisterStorageFromArg(this, v);

               KorkApi::TypeStorageInterface outputStorage = KorkApi::CreateRegisterStorage(this,
                                                                                       KorkApi::ConsoleValue::TypeInternalNumber);
               
               // NOTE: types should set head of stack to value if data pointer is nullptr in this case
               mTypes[v.typeId].iFuncs.CastValueFn(mTypes[v.typeId].userPtr,
                                                                   mVM,
                                                                   &inputStorage,
                                                                   &outputStorage,
                                                                   nullptr,
                                                                   0,
                                                                   KorkApi::ConsoleValue::TypeInternalNumber);

               return outputStorage.data.storageRegister->quickCastToNumeric();
         }
         break;
   }
   return 0;
}

const char* VmInternal::valueAsString(ConsoleValue v)
{
   switch (v.typeId)
   {
      case KorkApi::ConsoleValue::TypeInternalUnsigned:
      return tempIntConv(v.getInt());
      break;
      case KorkApi::ConsoleValue::TypeInternalNumber:
      return tempFloatConv(v.getFloat());
      break;
      case KorkApi::ConsoleValue::TypeInternalString:
      {
         const char* r = (const char*)v.evaluatePtr(mAllocBase);
         return r ? r : "";
      }
         
      break;
   default:
      {
            KorkApi::TypeStorageInterface inputStorage = KorkApi::CreateRegisterStorageFromArg(this, v);

            KorkApi::TypeStorageInterface outputStorage = KorkApi::CreateExprEvalReturnTypeStorage(this,
                                                                                                   1024,
                                                                                    KorkApi::ConsoleValue::TypeInternalString);
            
            // NOTE: types should set head of stack to value if data pointer is nullptr in this case
            mTypes[v.typeId].iFuncs.CastValueFn(mTypes[v.typeId].userPtr,
                                                                mVM,
                                                                &inputStorage,
                                                                &outputStorage,
                                                                nullptr,
                                                                0,
                                                                KorkApi::ConsoleValue::TypeInternalString);
            
            // NOTE: this needs to be put somewhere since the return buffer is often used elsewhere
            // TODO: maybe need a variant here for custom output storage to handle externally
            return tempStringConv((const char*)outputStorage.data.storageRegister->evaluatePtr(mAllocBase));
      }
      break;
   }
}

KorkApi::ConsoleValue VmInternal::valueAsCVString(ConsoleValue v)
{
   switch (v.typeId)
   {
      case KorkApi::ConsoleValue::TypeInternalUnsigned:
      return  KorkApi::ConsoleValue::makeString(tempIntConv(v.getInt()));
      break;
      case KorkApi::ConsoleValue::TypeInternalNumber:
      return  KorkApi::ConsoleValue::makeString(tempFloatConv(v.getFloat()));
      break;
      case KorkApi::ConsoleValue::TypeInternalString:
      {
         const char* r = (const char*)v.evaluatePtr(mAllocBase);
         return  KorkApi::ConsoleValue::makeString(r ? r : "");
      }
         
      break;
   default:
      {
            KorkApi::TypeStorageInterface inputStorage = KorkApi::CreateRegisterStorageFromArg(this, v);

            KorkApi::TypeStorageInterface outputStorage = KorkApi::CreateExprEvalReturnTypeStorage(this,
                                                                                                   1024,
                                                                                    KorkApi::ConsoleValue::TypeInternalString);
            
            // NOTE: types should set head of stack to value if data pointer is nullptr in this case
            mTypes[v.typeId].iFuncs.CastValueFn(mTypes[v.typeId].userPtr,
                                                                mVM,
                                                                &inputStorage,
                                                                &outputStorage,
                                                                nullptr,
                                                                0,
                                                                KorkApi::ConsoleValue::TypeInternalString);
            
            // NOTE: this needs to be put somewhere since the return buffer is often used elsewhere
            // TODO: maybe need a variant here for custom output storage to handle externally
            return KorkApi::ConsoleValue::makeString(tempStringConv((const char*)outputStorage.data.storageRegister->evaluatePtr(mAllocBase)));
      }
      break;
   }
}

void VmInternal::printf(int level, const char* fmt, ...)
{
   if (mConfig.logFn == nullptr && 
       mConfig.extraConsumers[0].cbFunc == nullptr &&
       mConfig.extraConsumers[1].cbFunc == nullptr)
      return;

   char buffer[4096];
   va_list args;
   va_start(args, fmt);
   vsnprintf(buffer, sizeof(buffer), fmt, args);
   va_end(args);
   
   if (mConfig.logFn)
   {
      mConfig.logFn(level, buffer, mConfig.logUser);
   }

   for (U32 i=0; i<2; i++)
   {
      if (mConfig.extraConsumers[i].cbFunc)
      {
         mConfig.extraConsumers[i].cbFunc(level, buffer, mConfig.extraConsumers[i].cbUser);
      }
   }
}

void VmInternal::print(int level, const char* buf)
{
   if (mConfig.logFn == nullptr)
      return;
   
   mConfig.logFn(level, buf, mConfig.logUser);

}



void Vm::dumpNamespaceClasses(bool dumpScript, bool dumpEngine)
{
   VmAllocTLS::Scope memScope(mInternal);
   mInternal->mNSState.dumpClasses(dumpScript, dumpEngine);
}

void Vm::dumpNamespaceFunctions(bool dumpScript, bool dumpEngine)
{
   VmAllocTLS::Scope memScope(mInternal);
   mInternal->mNSState.dumpFunctions(dumpScript, dumpEngine);
}

void Vm::dbgSetParameters(int port, const char* password, bool waitForClient)
{
   if (mInternal->mTelDebugger)
   {
      VmAllocTLS::Scope memScope(mInternal);
      mInternal->mTelDebugger->setDebugParameters(port, password, waitForClient);
   }
}

bool Vm::dbgIsConnected()
{
   return mInternal->mTelDebugger && mInternal->mTelDebugger->isConnected();
}

void Vm::dbgDisconnect()
{
   if (mInternal->mTelDebugger)
   {
      VmAllocTLS::Scope memScope(mInternal);
      mInternal->mTelDebugger->disconnect();
   }
}

void Vm::telnetSetParameters(int port, const char* consolePass, const char* listenPass, bool remoteEcho)
{
   if (mInternal->mTelConsole)
   {
      VmAllocTLS::Scope memScope(mInternal);
      mInternal->mTelConsole->setTelnetParameters(port, consolePass, listenPass, remoteEcho);
   }
}

void Vm::telnetDisconnect()
{
   if (mInternal->mTelConsole)
   {
      VmAllocTLS::Scope memScope(mInternal);
      mInternal->mTelConsole->disconnect();
   }
}

void Vm::processTelnet()
{
   VmAllocTLS::Scope memScope(mInternal);
   if (mInternal->mTelConsole)
   {
      mInternal->mTelConsole->process();
   }
   if (mInternal->mTelDebugger)
   {
      mInternal->mTelDebugger->process();
   }
}


void Vm::setCurrentFiberMain()
{
   VmAllocTLS::Scope memScope(mInternal);
   mInternal->setCurrentFiberMain();
}

void Vm::setCurrentFiber(FiberId fiber)
{
   VmAllocTLS::Scope memScope(mInternal);
   mInternal->setCurrentFiber(fiber);
}

FiberId Vm::createFiber(void* userPtr)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->createFiber(userPtr);
}

FiberId Vm::getCurrentFiber()
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->getCurrentFiber();
}

void Vm::cleanupFiber(FiberId fiber)
{
   VmAllocTLS::Scope memScope(mInternal);
   mInternal->cleanupFiber(fiber);
}

FiberRunResult Vm::resumeCurrentFiber(ConsoleValue value)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->resumeCurrentFiber(value);
}

bool Vm::dumpFiberStateToBlob(U32 numFibers, KorkApi::FiberId* fibers, U32* outBlobSize, U8** outBlob)
{
   VmAllocTLS::Scope memScope(mInternal);
   const U32 maxBlobSize = 1024*1024*16;
   U8* buffer = mInternal->NewArray<U8>(maxBlobSize);
   *outBlob = nullptr;
   *outBlobSize = 0;
   
   MemStream outS(maxBlobSize, buffer, true, true);
   ConsoleSerializer serializer(mInternal, nullptr, false, &outS);
   
   Vector<ExprEvalState*> fiberList;
   for (U32 i=0; i<numFibers; i++)
   {
      ExprEvalState* state = mInternal->mFiberStates.getItem(fibers[i]);
      if (state)
      {
         fiberList.push_back(state);
      }
   }
   
   if (fiberList.size() == 0)
   {
      mInternal->DeleteArray(buffer);
      return false;
   }
   
   if (serializer.write(fiberList))
   {
      *outBlob = buffer;
      *outBlobSize = outS.getPosition();
      return true;
   }
   else
   {
      mInternal->DeleteArray(buffer);
   }
   
   serializer.reset(false);
   return false;
}

bool Vm::restoreFiberStateFromBlob(U32* outNumFibers, KorkApi::FiberId** outFibers, U32 blobSize, U8* blob)
{
   VmAllocTLS::Scope memScope(mInternal);
   MemStream inS(blobSize, blob, true, false);
   ConsoleSerializer serializer(mInternal, nullptr, false, &inS);
   
   Vector<ExprEvalState*> fiberList;
   
   if (serializer.read(fiberList))
   {
      *outFibers = fiberList.size() > 0 ? (KorkApi::FiberId*)mInternal->NewArray<KorkApi::FiberId>(fiberList.size()) : 0;
      *outNumFibers = fiberList.size();
      
      for (U32 i=0; i<fiberList.size(); i++)
      {
         (*outFibers)[i] = mInternal->mFiberStates.getHandleValue(fiberList[i]);
      }
      
      serializer.reset(false);
      return true;
   }
   
   serializer.reset(true);
   return false;
}

void Vm::suspendCurrentFiber()
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->suspendCurrentFiber();
}

FiberRunResult::State Vm::getFiberState(KorkApi::FiberId fid)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->getFiberState(fid);
}

FiberRunResult::State Vm::getCurrentFiberState()
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->getCurrentFiberState();
}

void Vm::clearCurrentFiberError()
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->clearCurrentFiberError();
}

void* Vm::getCurrentFiberUserPtr()
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->getCurrentFiberUserPtr();
}

const char* Vm::getExceptionFileLine(ExceptionInfo* info)
{
   if (!info || !info->cb)
   {
      return "";
   }

   return info->cb->getFileLine(info->ip);
}

StringTableEntry Vm::internString(const char* str, bool caseSens)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->internString(str, caseSens);
}

StringTableEntry Vm::internStringN(const char* str, U32 len, bool caseSens)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->internStringN(str, len, caseSens);
}

StringTableEntry Vm::lookupString(const char* str, bool caseSens)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->lookupString(str, caseSens);
}

StringTableEntry Vm::lookupStringN(const char* str, U32 len, bool caseSens)
{
   VmAllocTLS::Scope memScope(mInternal);
   return mInternal->lookupStringN(str, len, caseSens);
}

bool Vm::initFixedTypeStorage(void* ptr, U16 typeId, bool isField, TypeStorageInterface* outInterface)
{
   *outInterface = KorkApi::CreateFixedTypeStorage(mInternal, ptr, typeId, isField);
   return true;
}

bool Vm::initReturnTypeStorage(U32 minSize, U16 typeId, TypeStorageInterface* outInterface)
{
   *outInterface = KorkApi::CreateExprEvalReturnTypeStorage(mInternal, minSize, typeId);
   return true;
}

bool Vm::initRegisterTypeStorage(U32 argc, KorkApi::ConsoleValue* argv, TypeStorageInterface* outInterface)
{
   *outInterface = KorkApi::CreateRegisterStorageFromArgs(mInternal, argc, argv);
   return true;
}



const char* FiberRunResult::stateAsString(State inState)
{
   switch (inState)
   {
   case FiberRunResult::INACTIVE:
      return "INACTIVE";
      break;
   case FiberRunResult::RUNNING:
      return "RUNNING";
      break;
   case FiberRunResult::SUSPENDED:
      return "SUSPENDED";
      break;
   case FiberRunResult::ERROR:
      return "ERROR";
      break;
   case FiberRunResult::FINISHED:
      return "FINISHED";
      break;
   default:
      return "UNKNOWN";
   }
}

namespace VmAllocTLS
{
   thread_local VmInternal* sVM;

   VmInternal* get()
   {
      return sVM;
   }

   void set(VmInternal* theVM) 
   {
      sVM = theVM;
   }
}

namespace VMem
{

void* allocBytes(std::size_t n)
{
   VmInternal* vm = VmAllocTLS::sVM;
   if (vm)
   {
      return vm->mConfig.mallocFn(n, vm->mConfig.allocUser);
   }
   else
   {
      return nullptr;
   }
}

void  freeBytes(void* p)
{
   VmInternal* vm = VmAllocTLS::sVM;
   if (vm)
   {
      return vm->mConfig.freeFn(p, vm->mConfig.allocUser);
   }
}

}



} // namespace KorkApi


