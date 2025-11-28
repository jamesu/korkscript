#include "embed/api.h"
#include "embed/internalApi.h"
#include "console/simpleLexer.h"
#include "console/ast.h"
#include "console/consoleNamespace.h"

#include "core/memStream.h"
#include "console/consoleInternal.h"
#include "console/telnetDebugger.h"
#include "console/telnetConsole.h"


namespace KorkApi
{


NamespaceId Vm::findNamespace(StringTableEntry name, StringTableEntry package)
{
   return (NamespaceId)mInternal->mNSState.find(name, package);
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

NamespaceId Vm::getGlobalNamespace()
{
    return mInternal->mNSState.mGlobalNamespace;
}

void Vm::activatePackage(StringTableEntry pkgName)
{
   mInternal->mNSState.activatePackage(pkgName);
}

void Vm::deactivatePackage(StringTableEntry pkgName)
{
   mInternal->mNSState.deactivatePackage(pkgName);
}

bool Vm::linkNamespace(StringTableEntry parent, StringTableEntry child)
{
   NamespaceId pns = mInternal->mNSState.find(parent);
   NamespaceId cns = mInternal->mNSState.find(child);
   if(pns && cns)
      return cns->classLinkTo(pns);
   return false;
}

bool Vm::unlinkNamespace(StringTableEntry parent, StringTableEntry child)
{
   NamespaceId pns = mInternal->mNSState.find(parent);
   NamespaceId cns = mInternal->mNSState.find(child);
   if(pns && cns)
      return cns->unlinkClass(pns);
   return false;
}

bool Vm::linkNamespaceById(NamespaceId parent, NamespaceId child)
{
   Namespace* pns = (Namespace*)parent;
   Namespace* cns = (Namespace*)child;
   if(pns && cns)
      return cns->classLinkTo(pns);
   return false;
}

bool Vm::unlinkNamespaceById(NamespaceId parent, NamespaceId child)
{
   Namespace* pns = (Namespace*)parent;
   Namespace* cns = (Namespace*)child;
   if(pns && cns)
      return cns->unlinkClass(pns);
   return false;
}

const char* Vm::tabCompleteNamespace(NamespaceId nsId, const char *prevText, S32 baseLen, bool fForward)
{
   Namespace* ns = (Namespace*)nsId;
   return ns->tabComplete(prevText, baseLen, fForward);
}

const char* Vm::tabCompleteVariable(const char *prevText, S32 baseLen, bool fForward)
{
   return mInternal->mGlobalVars.tabComplete(prevText, baseLen, fForward);
}

TypeId Vm::registerType(TypeInfo& info)
{
   mInternal->mTypes.push_back(info);
   
   TypeInfo& chkFunc = mInternal->mTypes.last();
   
   // stubs
   
   if (chkFunc.iFuncs.CopyValue == NULL)
   {
      chkFunc.iFuncs.CopyValue = [](void* userPtr,
                                    Vm* vm,
                                           void* sptr,
                                           const EnumTable* tbl,
                                           BitSet32 flag,
                                           U32 requestedType,
                                           U32 requestedZone){
         return ConsoleValue();
      };
   }
   
   if (chkFunc.iFuncs.SetValue == NULL)
   {
      chkFunc.iFuncs.SetValue = [](void* userPtr,
                                   Vm* vm,
                                           void* dptr,
                                           S32 argc,
                                           ConsoleValue* argv,
                                           const EnumTable* tbl,
                                           BitSet32 flag,
                                           U32 typeId){
         
      };
   }
   
   return mInternal->mTypes.size()-1;
}

TypeInfo* Vm::getTypeInfo(TypeId ident)
{
   return &mInternal->mTypes[ident];
}

ClassId Vm::registerClass(ClassInfo& info)
{
   mInternal->mClassList.push_back(info);
   
   ClassInfo& chkFunc = mInternal->mClassList.last();
   
   // iCreate stubs
   
   if (chkFunc.iCreate.CreateClassFn == NULL)
   {
      chkFunc.iCreate.CreateClassFn = [](void* user, Vm* vm, CreateClassReturn* outP) {
      };
   }
   if (chkFunc.iCreate.DestroyClassFn == NULL)
   {
      chkFunc.iCreate.DestroyClassFn = [](void* user, Vm* vm, void* createdPtr) {
      };
   }
   if (chkFunc.iCreate.ProcessArgsFn == NULL)
   {
      chkFunc.iCreate.ProcessArgsFn = [](Vm* vm, void* createdPtr, const char* name, bool isDatablock, bool internalName, int argc, const char** argv) {
         return false;
      };
   }
   if (chkFunc.iCreate.AddObjectFn == NULL)
   {
      chkFunc.iCreate.AddObjectFn = [](Vm* vm, VMObject* object, bool placeAtRoot, U32 groupAddId) {
         return false;
      };
   }
   if (chkFunc.iCreate.RemoveObjectFn == NULL)
   {
      chkFunc.iCreate.RemoveObjectFn = [](void* user, Vm* vm, VMObject* object) {
      };
   }
   if (chkFunc.iCreate.GetIdFn == NULL)
   {
      chkFunc.iCreate.GetIdFn = [](VMObject* object) {
         return (SimObjectId)0;
      };
   }
   
   // iEnum stubs
   
   if (chkFunc.iEnum.GetSize == NULL)
   {
      chkFunc.iEnum.GetSize = [](VMObject* object) { return (U32)0; };
   }
   if (chkFunc.iEnum.GetObjectAtIndex == NULL)
   {
      chkFunc.iEnum.GetObjectAtIndex = [](VMObject* object, U32 index) { return (VMObject*)NULL; };
   }
   
   // iCustomFields stubs
   if (chkFunc.iCustomFields.IterateFields == NULL)
   {
      chkFunc.iCustomFields.IterateFields = [](KorkApi::Vm* vm, KorkApi::VMObject* object, VMIterator& state, StringTableEntry* name){
         return false;
      };
   }
   if (chkFunc.iCustomFields.GetFieldByIterator == NULL)
   {
      chkFunc.iCustomFields.GetFieldByIterator = [](KorkApi::Vm* vm, VMObject* object, VMIterator& state){
         return ConsoleValue();
      };
   }
   if (chkFunc.iCustomFields.GetFieldByName == NULL)
   {
      chkFunc.iCustomFields.GetFieldByName = [](KorkApi::Vm* vm, VMObject* object, const char* name){
         return ConsoleValue();
      };
   }
   if (chkFunc.iCustomFields.SetFieldByName == NULL)
   {
      chkFunc.iCustomFields.SetFieldByName = [](KorkApi::Vm* vm, VMObject* object, const char* name, ConsoleValue value){
      };
   }
      
   
   return mInternal->mClassList.size()-1;
}

ClassId Vm::getClassId(const char* name)
{
   StringTableEntry klassST = StringTable->insert(name);
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
   return mInternal->createHeapRef(size);
}

void Vm::releaseHeapRef(ConsoleHeapAllocRef value)
{
   mInternal->releaseHeapRef(value);
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
   mConfig.freeFn(ref, mConfig.allocUser);
}

ConsoleValue Vm::getStringFuncBuffer(U32 size)
{
    return mInternal->getStringFuncBuffer(0, size);
}

ConsoleValue Vm::getStringReturnBuffer(U32 size)
{
    return mInternal->getStringReturnBuffer(size);
}

ConsoleValue Vm::getTypeReturn(TypeId typeId)
{
    return mInternal->getTypeReturn(typeId);
}

ConsoleValue Vm::getTypeFunc(TypeId typeId)
{
    return mInternal->getTypeFunc(0, typeId);
}

ConsoleValue Vm::getStringInZone(U16 zone, U32 size)
{
   return mInternal->getStringInZone(zone, size);
}

ConsoleValue Vm::getTypeInZone(U16 zone, TypeId typeId)
{
   return mInternal->getTypeInZone(zone, typeId);
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

ConsoleValue VmInternal::getTypeInZone(U16 zone, TypeId typeId)
{
   U32 size = mTypes[typeId].size;
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
      mReturnBuffer.setSize(size + 2048);
      mAllocBase.arg = mReturnBuffer.address();
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

ConsoleValue VmInternal::getTypeFunc(U32 fiberIndex, TypeId typeId)
{
   U32 size = mTypes[typeId].size;
   return mFiberStates.mItems[fiberIndex] ? mFiberStates.mItems[fiberIndex]->mSTR.getFuncBuffer(typeId, size) : ConsoleValue();
}

ConsoleValue VmInternal::getTypeReturn(TypeId typeId)
{
   KorkApi::ConsoleValue ret;
   U32 size = mTypes[typeId].size;
   validateReturnBufferSize(size);
   ret.setTyped(0, KorkApi::ConsoleValue::TypeInternalString, KorkApi::ConsoleValue::ZoneReturn);
   return ret;
}



void Vm::pushValueFrame()
{
   return mInternal->mCurrentFiberState->mSTR.pushFrame();
}

void Vm::popValueFrame()
{
   return mInternal->mCurrentFiberState->mSTR.popFrame();
}

// Public
VMObject* Vm::constructObject(ClassId klassId, const char* name, int argc, const char** argv)
{
   ClassInfo* ci = &mInternal->mClassList[klassId];
   VMObject* object = new VMObject();
   mInternal->incVMRef(object);

   CreateClassReturn ret = {};
   
   if (ci->iCreate.CreateClassFn)
   {
      object->klass = ci;
      object->ns = NULL;
      ci->iCreate.CreateClassFn(ci->userPtr, this, &ret);
      object->userPtr = ret.userPtr;
      object->flags = ret.initialFlags;

      if (object->userPtr)
      {
         if (!ci->iCreate.ProcessArgsFn(this, object->userPtr, name, false, false, argc, argv))
         {
            ci->iCreate.DestroyClassFn(ci->userPtr, this, object->userPtr);
            return NULL;
         }
         else
         {
            return object;
         }
      }
   }
   
   mInternal->decVMRef(object);
   return NULL;
}

void Vm::setObjectNamespace(VMObject* object, NamespaceId nsId)
{
   object->ns = (Namespace*)nsId;
}

// Internal
VMObject* Vm::createVMObject(ClassId klassId, void* klassPtr)
{
   VMObject* object = new VMObject();
   mInternal->incVMRef(object);
   object->klass = &mInternal->mClassList[klassId];
   object->ns = NULL;
   object->userPtr = klassPtr;
    return object;
}

void Vm::incVMRef(VMObject* object)
{
   mInternal->incVMRef(object);
}

void Vm::decVMRef(VMObject* object)
{
   mInternal->decVMRef(object);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, StringFuncCallback cb, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs)
{
   Namespace* ns = (Namespace*)nsId;
   ns->addCommand(name, cb, userPtr, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, IntFuncCallback cb, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs)
{
   Namespace* ns = (Namespace*)nsId;
   ns->addCommand(name, cb, userPtr, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, FloatFuncCallback cb, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs)
{
   Namespace* ns = (Namespace*)nsId;
   ns->addCommand(name, cb, userPtr, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, VoidFuncCallback cb, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs)
{
   Namespace* ns = (Namespace*)nsId;
   ns->addCommand(name, cb, userPtr, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, BoolFuncCallback cb, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs)
{
   Namespace* ns = (Namespace*)nsId;
   ns->addCommand(name, cb, userPtr, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, ValueFuncCallback cb, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs)
{
   Namespace* ns = (Namespace*)nsId;
   ns->addCommand(name, cb, userPtr, usage, minArgs, maxArgs);
}

bool Vm::isNamespaceFunction(NamespaceId nsId, StringTableEntry name)
{
   Namespace* ns = (Namespace*)nsId;
   return ns->lookup(name) != NULL;
}

bool Vm::compileCodeBlock(const char* code, const char* filename, U32* outCodeSize, U8** outCode)
{
   CodeBlock* block = new CodeBlock(mInternal);
   
   U8* buffer = (U8*)dMalloc(1024 * 1024);
   *outCode = NULL;
   *outCodeSize = 0;
   MemStream outS(1024*1024, buffer, true, true);
   
   if (!block->compileToStream(outS, filename, code))
   {
      delete block;
      delete[] code;
      return -1;
   }
   
   *outCode = buffer;
   *outCodeSize = outS.getPosition();
   return true;
}

ConsoleValue Vm::execCodeBlock(U32 codeSize, U8* code, const char* filename, bool noCalls, int setFrame)
{
   CodeBlock* block = new CodeBlock(mInternal);
   
   MemStream stream(codeSize, code, true, false);
   
   if (!block->read(filename, true, stream))
   {
      delete block;
      return ConsoleValue();
   }
   
   return block->exec(0, filename, NULL, 0, 0, noCalls, NULL, setFrame);
}

ConsoleValue Vm::evalCode(const char* code, const char* filename)
{
    CodeBlock *newCodeBlock = new CodeBlock(mInternal);
    return newCodeBlock->compileExec(filename, code, false, filename ? -1 : 0); // TODO: should this be 0 or -1?
}

ConsoleValue Vm::call(int argc, ConsoleValue* argv, bool startSuspended)
{
   ConsoleValue retValue = ConsoleValue();
   callNamespaceFunction(getGlobalNamespace(), StringTable->insert(mInternal->valueAsString(argv[1])), argc, argv, retValue, startSuspended);
   return retValue;
}

ConsoleValue Vm::callObject(VMObject* h, int argc, ConsoleValue* argv, bool startSuspended)
{
   ConsoleValue retValue = ConsoleValue();
   callObjectFunction(h, StringTable->insert(mInternal->valueAsString(argv[1])), argc, argv, retValue, startSuspended);
   return retValue;
}

bool Vm::callObjectFunction(VMObject* self, StringTableEntry funcName, int argc, KorkApi::ConsoleValue* argv, ConsoleValue& retValue, bool startSuspended)
{
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

   if(ent == NULL)
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

   if (ent->mType == Namespace::Entry::ScriptFunctionType)
   {
      // TODO: need a better way of dealing with this
      // TOFIX
      //object->pushScriptCallbackGuard();
   }

   KorkApi::ConsoleValue ret = ent->execute(argc, argv, mInternal->mCurrentFiberState, self, startSuspended);
   
   retValue = ret;

   if (ent->mType == Namespace::Entry::ScriptFunctionType)
   {
      //object->popScriptCallbackGuard();
   }

   // Twiddle it back
   argv[1] = oldArg1;

   // Reset the function offset so the stack
   // doesn't continue to grow unnecessarily
   mInternal->mCurrentFiberState->mSTR.clearFunctionOffset();

   return true;
}

bool Vm::callNamespaceFunction(NamespaceId nsId, StringTableEntry name, int argc, KorkApi::ConsoleValue* argv, ConsoleValue& retValue, bool startSuspended)
{
   Namespace* ns = (Namespace*)nsId;
   Namespace::Entry* ent = ns->lookup(name);

   if (!ent)
   {
      mInternal->printf(0, "%s: Unknown command.", argv[0]);
      // Clean up arg buffers, if any.
      mInternal->mCurrentFiberState->mSTR.clearFunctionOffset();
      return false;
   }

   retValue = ent->execute(argc, argv, mInternal->mCurrentFiberState, NULL, startSuspended);

   // Reset the function offset so the stack
   // doesn't continue to grow unnecessarily
   mInternal->mCurrentFiberState->mSTR.clearFunctionOffset();

   return true;
}

// Helpers (should call into user funcs)
VMObject* Vm::findObjectByName(const char* name)
{
    return mInternal->mConfig.iFind.FindObjectByNameFn(mInternal->mConfig.findUser, name, NULL);
}

VMObject* Vm::findObjectByPath(const char* path)
{
    return mInternal->mConfig.iFind.FindObjectByPathFn(mInternal->mConfig.findUser, path);
}

VMObject* Vm::findObjectById(SimObjectId ident)
{
   return mInternal->mConfig.iFind.FindObjectByIdFn(mInternal->mConfig.findUser, ident);
}

bool Vm::setObjectField(VMObject* object, StringTableEntry fieldName, ConsoleValue nativeValue, const char* arrayIndex)
{
   return mInternal->setObjectField(object, fieldName, arrayIndex, nativeValue);
}

bool Vm::setObjectFieldString(VMObject* object, StringTableEntry fieldName, const char* stringValue, const char* arrayIndex)
{
   ConsoleValue val = ConsoleValue::makeString(stringValue);
   return mInternal->setObjectField(object, fieldName, arrayIndex, val);
}

ConsoleValue Vm::getObjectField(VMObject* object, StringTableEntry fieldName, ConsoleValue nativeValue, const char* arrayIndex)
{
    return mInternal->getObjectField(object, fieldName, arrayIndex, KorkApi::TypeDirectCopy, KorkApi::ConsoleValue::ZoneExternal);
}

const char* Vm::getObjectFieldString(VMObject* object, StringTableEntry fieldName, const char** stringValue, const char* arrayIndex)
{
   ConsoleValue foundValue = mInternal->getObjectField(object, fieldName, arrayIndex, KorkApi::TypeDirectCopy, KorkApi::ConsoleValue::ZoneFunc);
   
   return (const char*)foundValue.evaluatePtr(mInternal->mAllocBase);
}

void Vm::setGlobalVariable(StringTableEntry name, KorkApi::ConsoleValue value)
{
   mInternal->mGlobalVars.setVariableValue(name, value);
}

ConsoleValue Vm::getGlobalVariable(StringTableEntry name)
{
   Dictionary::Entry* e = mInternal->mGlobalVars.getVariable(name);
   
   if (!e)
   {
      return ConsoleValue();
   }
   
   return mInternal->mGlobalVars.getEntryValue(e);
}

void Vm::setLocalVariable(StringTableEntry name, KorkApi::ConsoleValue value)
{
   mInternal->mCurrentFiberState->setLocalFrameVariable(name, value);
}

ConsoleValue Vm::getLocalVariable(StringTableEntry name)
{
   return mInternal->mCurrentFiberState->getLocalFrameVariable(name);
}


bool Vm::registerGlobalVariable(StringTableEntry name, S32 type, void *dptr, const char* usage)
{
   return mInternal->mGlobalVars.addVariable( name, type, dptr, usage );
}

bool Vm::removeGlobalVariable(StringTableEntry name)
{
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
   if (cfg->mallocFn == NULL || cfg->freeFn == NULL)
   {
      return NULL;
   }
   
   Vm* vm = new Vm();
   vm->mInternal = new VmInternal(vm, cfg);
   
   return vm;
}

void destroyVM(Vm* vm)
{
   delete vm->mInternal;
   delete vm;
}

VmInternal::VmInternal(Vm* vm, Config* cfg) : mGlobalVars(this)
{
   mVM = vm;
   mConfig = *cfg;
   mCodeBlockList = NULL;
   mCurrentCodeBlock = NULL;
   mReturnBuffer.setSize(2048);
   mNSState.init(this);
   
   if (cfg->maxFibers == 0)
   {
      cfg->maxFibers = 1024;
   }
   mAllocBase.func = new void*[cfg->maxFibers];
   mAllocBase.arg = mReturnBuffer.address();
   memset(mAllocBase.func, 0, sizeof(void*)*cfg->maxFibers);

   if (mConfig.initTelnet)
   {
      mTelDebugger = new TelnetDebugger(this);
      mTelConsole = new TelnetConsole(this);
   }
   else
   {
      mTelDebugger = NULL;
      mTelConsole = NULL;
   }
   
   mHeapAllocs = NULL;
   mConvIndex = 0;
   mNSCounter = 0;

   if (cfg->userResources)
   {
      mCompilerResources = cfg->userResources;
      mOwnsResources = false;
   }
   else
   {
      mCompilerResources = new Compiler::Resources();
      mOwnsResources = true;
   }

   mCompilerResources->allowExceptions = cfg->enableExceptions;
   
   TypeInfo typeInfo = {};
   
   typeInfo.name = StringTable->insert("string");
   typeInfo.inspectorFieldType = NULL;
   typeInfo.userPtr = NULL;
   typeInfo.size = sizeof(const char*);
   // TODO
   
   mTypes.push_back(typeInfo);
   
   typeInfo.name = StringTable->insert("float");
   typeInfo.inspectorFieldType = NULL;
   typeInfo.userPtr = NULL;
   typeInfo.size = sizeof(F64);
   // TODO
   
   mTypes.push_back(typeInfo);
   
   typeInfo.name = StringTable->insert("uint");
   typeInfo.inspectorFieldType = NULL;
   typeInfo.userPtr = NULL;
   typeInfo.size = sizeof(U64);
   // TODO
   
   mTypes.push_back(typeInfo);
   
   // Config Stubs
   
   if (mConfig.logFn == NULL) {
      mConfig.logFn = [](U32 level, const char* buffer, void* user) {
      };
   }
   if (mConfig.addTagFn == NULL) {
      mConfig.addTagFn = [](const char* vmString, void* user) {
         return (U32)0;
      };
   }

   if (mConfig.iFind.FindObjectByNameFn == NULL) {
      mConfig.iFind.FindObjectByNameFn = [](void* userPtr, StringTableEntry name, VMObject* parent) {
           return (VMObject*)NULL;
       };
   }

   if (mConfig.iFind.FindObjectByPathFn == NULL) {
      mConfig.iFind.FindObjectByPathFn = [](void* userPtr, const char* path) {
         return (VMObject*)NULL;
       };
   }

   if (mConfig.iFind.FindObjectByInternalNameFn == NULL) {
      mConfig.iFind.FindObjectByInternalNameFn = [](void* userPtr, StringTableEntry name, bool recursive, VMObject* parent) {
         return (VMObject*)NULL;
       };
   }

   if (mConfig.iFind.FindObjectByIdFn == NULL) {
      mConfig.iFind.FindObjectByIdFn = [](void* userPtr, SimObjectId ident) {
         return (VMObject*)NULL;
       };
   }
   
   if (mConfig.iFind.FindDatablockGroup == NULL) {
      mConfig.iFind.FindDatablockGroup = [](void* userPtr){
         return (VMObject*)NULL;
      };
   }
   
   FiberId baseFiber = createFiber(NULL);
   mCurrentFiberState = mFiberStates.mItems[0];
}

VmInternal::~VmInternal()
{
   ExprEvalState* mCurrentFiberState;
   InternalFiberList mFiberStates;
   ClassChunker<ExprEvalState> mFiberAllocator;

   delete[] mAllocBase.func;
   
   
   delete mTelDebugger;
   delete mTelConsole;
   if (mOwnsResources)
   {
      delete mCompilerResources;
   }
   mNSState.shutdown();
   
   // Cleanup remaining fibers
   for (Vector<ExprEvalState*>::iterator itr = mFiberStates.mItems.begin(), itrEnd = mFiberStates.mItems.end(); itr != itrEnd; itr++)
   {
      delete *itr;
      *itr = NULL;
   }
   mFiberStates.clear();
   mFiberAllocator.freeBlocks();

   for (ConsoleHeapAlloc* alloc = mHeapAllocs; alloc; alloc = alloc->next)
   {
      mConfig.freeFn(alloc, mConfig.allocUser);
   }
   mHeapAllocs = NULL;
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
   ExprEvalState* newState = new ExprEvalState(this);
   InternalFiberList::HandleType handle = mFiberStates.allocListHandle(newState);
   newState->mSTR.mFuncId = handle.getIndex();
   newState->mUserPtr = userPtr;
   mAllocBase.func[handle.getIndex()] = newState->mSTR.mBuffer;
   return handle.getValue();
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
      delete state;
      mAllocBase.func[vh.getIndex()] = NULL;
   }
}

FiberRunResult VmInternal::resumeCurrentFiber(ConsoleValue value)
{
   if (mCurrentFiberState == NULL)
   {
      FiberRunResult r = {};
      return r;
   }
   
   return mCurrentFiberState->resume(value);
}

void VmInternal::suspendCurrentFiber()
{
   if (mCurrentFiberState == NULL)
      return;
   
   mCurrentFiberState->suspend();
}

void VmInternal::throwFiber(U32 mask)
{
   if (mCurrentFiberState == NULL)
      return;
   
   mCurrentFiberState->throwMask(mask);
}

FiberRunResult::State VmInternal::getCurrentFiberState()
{
   return mCurrentFiberState ? mCurrentFiberState->mState : FiberRunResult::ERROR;
}

void* VmInternal::getCurrentFiberUserPtr()
{
   return mCurrentFiberState ? mCurrentFiberState->mUserPtr : NULL;
}

StringTableEntry VmInternal::getCurrentCodeBlockName()
{
   if (mCurrentCodeBlock)
      return mCurrentCodeBlock->name;
   else
      return NULL;
}

StringTableEntry VmInternal::getCurrentCodeBlockFullPath()
{
   if (mCurrentCodeBlock)
      return mCurrentCodeBlock->fullPath;
   else
      return NULL;
}

StringTableEntry VmInternal::getCurrentCodeBlockModName()
{
   if (mCurrentCodeBlock)
      return mCurrentCodeBlock->modPath;
   else
      return NULL;
}

CodeBlock *VmInternal::findCodeBlock(StringTableEntry name)
{
   for(CodeBlock *walk = mCodeBlockList; walk; walk = walk->nextFile)
      if(walk->name == name)
         return walk;
   return NULL;
}

ClassInfo* VmInternal::getClassInfoByName(StringTableEntry name)
{
   auto itr = std::find_if(mClassList.begin(), mClassList.end(), [name](ClassInfo& info){
      return info.name == name;
   });

   if (itr != mClassList.end())
   {
      return itr;
   }
   else
   {
      return NULL;
   }
}


const char* VmInternal::tempFloatConv(F64 val)
{
   if (mConvIndex == MaxStringConvs)
      mConvIndex = 0;

   snprintf(mTempStringConversions[mConvIndex], MaxTempStringSize, "%g", val);
   return mTempStringConversions[mConvIndex++];
}

const char* VmInternal::tempIntConv(U64 val)
{
   if (mConvIndex == MaxStringConvs)
      mConvIndex = 0;

   snprintf(mTempStringConversions[mConvIndex], MaxTempStringSize, "%llu", val);
   return mTempStringConversions[mConvIndex++];
}

bool VmInternal::setObjectField(VMObject* obj, StringTableEntry name, const char* array, ConsoleValue value)
{
   if ((obj->flags & KorkApi::ModStaticFields) != 0)
   {
      for (U32 i=0; i<obj->klass->numFields; i++)
      {
         FieldInfo& f = obj->klass->fields[i];
         
         if (f.pFieldname != name)
            continue;
         
         U32 idx = dAtoi(array);
         U32 elemCount = f.elementCount > 0 ? (U32)f.elementCount : 1;
         if (idx >= elemCount)
            break;
         
         TypeId tid = f.type;
         if (tid < 0 || (U32)tid >= mTypes.size())
            break;
         
         TypeInfo& tinfo = mTypes[tid];
         if (!tinfo.iFuncs.SetValue || tinfo.size == 0)
            break;
         
         U8* base = static_cast<U8*>(obj->userPtr);
         U8* dptr = base + f.offset + (idx * (U32)tinfo.size);
         
         SetValueFn setFn = f.ovrSetValue ? f.ovrSetValue : tinfo.iFuncs.SetValue;

         setFn(f.ovrSetValue ? obj->userPtr : tinfo.userPtr,
               mVM,
               dptr,
               1,
               &value,
               f.table,
               f.flag,
               tid);
      }
      return true;
   }

   if ((obj->flags & KorkApi::ModDynamicFields) != 0)
   {
      obj->klass->iCustomFields.SetFieldByName(mVM, obj, name, value);
      return true;
   }
   
   return false;
}

ConsoleValue VmInternal::getObjectField(VMObject* obj, StringTableEntry name, const char* array, U32 requestedType, U32 requestedZone)
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
      
      U32 idx = dAtoi(array);
      U32 elemCount = f.elementCount > 0 ? (U32)f.elementCount : 1;
      if (idx >= elemCount)
         break;

      TypeId tid = (TypeId)f.type;
      if (tid < 0 || (U32)tid >= mTypes.size())
         break;

      TypeInfo& tinfo = mTypes[tid];
      if (!tinfo.iFuncs.CopyValue || tinfo.size == 0)
         return def;

      U8* base = static_cast<U8*>(obj->userPtr);
      U8* dptr = base + f.offset + (idx * (U32)tinfo.size);
      
      // Add requested type
      if ((requestedType & KorkApi::TypeDirectCopy) != 0)
      {
         requestedType |= f.type;
      }

      CopyValueFn copyFn = f.ovrCopyValue ? f.ovrCopyValue : tinfo.iFuncs.CopyValue;

      return copyFn(
         f.ovrCopyValue ? obj->userPtr : tinfo.userPtr,
         mVM,
         dptr,
         f.table,
         f.flag,
         requestedType,
         requestedZone
      );
   }
   
   return def;
}

F64 Vm::valueAsFloat(ConsoleValue v)
{
   return mInternal->valueAsFloat(v);
}

S64 Vm::valueAsInt(ConsoleValue v)
{
   return mInternal->valueAsInt(v);
}

bool Vm::valueAsBool(ConsoleValue v)
{
   return mInternal->valueAsBool(v);
}

const char* Vm::valueAsString(ConsoleValue v)
{
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
      return atof((const char*)v.evaluatePtr(mAllocBase));
      break;
      default:
         {
            KorkApi::TypeInfo& info = mTypes[v.typeId];
            void* typePtr = v.evaluatePtr(mAllocBase);

            return info.iFuncs.CopyValue(info.userPtr,
                                               mVM,
                         typePtr,
                         NULL,
                         0,
                                               KorkApi::ConsoleValue::TypeInternalNumber,
                                               KorkApi::ConsoleValue::ZoneReturn).getFloat();
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
      return dAtob((const char*)v.evaluatePtr(mAllocBase));
      break;
      default:
         {
            KorkApi::TypeInfo& info = mTypes[v.typeId];
            void* typePtr = v.evaluatePtr(mAllocBase);

            return info.iFuncs.CopyValue(info.userPtr,
                                               mVM,
                         typePtr,
                         NULL,
                         0,
                                               KorkApi::ConsoleValue::TypeInternalUnsigned,
                                               KorkApi::ConsoleValue::ZoneReturn).getInt();
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
      return atoi((const char*)v.evaluatePtr(mAllocBase));
      break;
      default:
         {
            KorkApi::TypeInfo& info = mTypes[v.typeId];
            void* typePtr = v.evaluatePtr(mAllocBase);

            return info.iFuncs.CopyValue(info.userPtr,
                                               mVM,
                         typePtr,
                         NULL,
                         0,
                                               KorkApi::ConsoleValue::TypeInternalUnsigned,
                                               KorkApi::ConsoleValue::ZoneReturn).getInt();
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
         KorkApi::TypeInfo& info = mTypes[v.typeId];
         void* typePtr = v.evaluatePtr(mAllocBase);

         return (const char*)info.iFuncs.CopyValue(info.userPtr,
                                            mVM,
                      typePtr,
                      NULL,
                      0,
                                            KorkApi::ConsoleValue::TypeInternalString,
                                            KorkApi::ConsoleValue::ZoneReturn).evaluatePtr(mAllocBase);
      }
      break;
   }
}

void VmInternal::printf(int level, const char* fmt, ...)
{
   if (mConfig.logFn == NULL && 
       mConfig.extraConsumers[0].cbFunc == NULL &&
       mConfig.extraConsumers[1].cbFunc == NULL)
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
   if (mConfig.logFn == NULL)
      return;
   
   mConfig.logFn(level, buf, mConfig.logUser);

}



void Vm::dumpNamespaceClasses(bool dumpScript, bool dumpEngine)
{
   mInternal->mNSState.dumpClasses(dumpScript, dumpEngine);
}

void Vm::dumpNamespaceFunctions(bool dumpScript, bool dumpEngine)
{
   mInternal->mNSState.dumpFunctions(dumpScript, dumpEngine);
}

void Vm::dbgSetParameters(int port, const char* password, bool waitForClient)
{
   if (mInternal->mTelDebugger)
   {
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
      mInternal->mTelDebugger->disconnect();
   }
}

void Vm::telnetSetParameters(int port, const char* consolePass, const char* listenPass, bool remoteEcho)
{
   if (mInternal->mTelConsole)
   {
      mInternal->mTelConsole->setTelnetParameters(port, consolePass, listenPass, remoteEcho);
   }
}

void Vm::telnetDisconnect()
{
   if (mInternal->mTelConsole)
   {
      mInternal->mTelConsole->disconnect();
   }
}

void Vm::processTelnet()
{
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
   mInternal->setCurrentFiberMain();
}

void Vm::setCurrentFiber(FiberId fiber)
{
   mInternal->setCurrentFiber(fiber);
}

FiberId Vm::createFiber(void* userPtr)
{
   return mInternal->createFiber(userPtr);
}

FiberId Vm::getCurrentFiber()
{
   return mInternal->getCurrentFiber();
}

void Vm::cleanupFiber(FiberId fiber)
{
   mInternal->cleanupFiber(fiber);
}

FiberRunResult Vm::resumeCurrentFiber(ConsoleValue value)
{
   return mInternal->resumeCurrentFiber(value);
}

void Vm::suspendCurrentFiber()
{
   return mInternal->suspendCurrentFiber();
}

FiberRunResult::State Vm::getCurrentFiberState()
{
   return mInternal->getCurrentFiberState();
}

void* Vm::getCurrentFiberUserPtr()
{
   return mInternal->getCurrentFiberUserPtr();
}


} // namespace KorkApi
