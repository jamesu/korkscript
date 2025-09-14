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
   return mInternal->mEvalState.globalVars.tabComplete(prevText, baseLen, fForward);
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
      chkFunc.iCreate.CreateClassFn = [](void* user, Vm* vm, VMObject* object) {
         return (void*)NULL;
      };
   }
   if (chkFunc.iCreate.DestroyClassFn == NULL)
   {
      chkFunc.iCreate.DestroyClassFn = [](void* user, Vm* vm, void* createdPtr) {
      };
   }
   if (chkFunc.iCreate.ProcessArgs == NULL)
   {
      chkFunc.iCreate.ProcessArgs = [](Vm* vm, VMObject* object, const char* name, bool isDatablock, bool internalName, int argc, const char** argv) {
         return false;
      };
   }
   if (chkFunc.iCreate.AddObject == NULL)
   {
      chkFunc.iCreate.AddObject = [](Vm* vm, VMObject* object, bool placeAtRoot, U32 groupAddId) {
         return false;
      };
   }
   if (chkFunc.iCreate.GetId == NULL)
   {
      chkFunc.iCreate.GetId = [](VMObject* object) {
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
   mInternal->createHeapRef(size);
}

void Vm::releaseHeapRef(ConsoleHeapAllocRef value)
{
   mInternal->releaseHeapRef(value);
}

ConsoleHeapAllocRef VmInternal::createHeapRef(U32 size)
{
   ConsoleHeapAlloc* ref = (ConsoleHeapAlloc*)mConfig.mallocFn(sizeof(ConsoleHeapAllocRef) + size, mConfig.allocUser);
   ref->size = size;

   if (mHeapAllocs)
   {
      ref->prev = NULL;
      ref->next = mHeapAllocs;
      mHeapAllocs->prev = ref;
      mHeapAllocs = ref;
   }
   else
   {
      ref->prev = NULL;
      ref->next = NULL;
      mHeapAllocs = ref;
   }

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

ConsoleValue Vm::getStringReturnBuffer(U32 size)
{
    return mInternal->getStringReturnBuffer(size);
}

ConsoleValue Vm::getStringArgBuffer(U32 size)
{
    return mInternal->getStringArgBuffer(size);
}

ConsoleValue Vm::getTypeReturn(TypeId typeId)
{
    return mInternal->getTypeReturn(typeId);
}

ConsoleValue Vm::getTypeArg(TypeId typeId)
{
    return mInternal->getTypeArg(typeId);
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
   if (zone == ConsoleValue::ZoneArg)
      return getStringArgBuffer(size);
   else if (zone == ConsoleValue::ZoneReturn)
      return getStringReturnBuffer(size);
   else
      return ConsoleValue();
}

ConsoleValue VmInternal::getTypeInZone(U16 zone, TypeId typeId)
{
   if (zone == ConsoleValue::ZoneArg)
      return getTypeArg(typeId);
   else if (zone == ConsoleValue::ZoneReturn)
      return getTypeReturn(typeId);
   else
      return ConsoleValue();
}

ConsoleValue VmInternal::getStringReturnBuffer(U32 size)
{
   char* data = mSTR.getReturnBuffer(size);
   return ConsoleValue::makeString((char*)(data - mSTR.mBuffer), ConsoleValue::ZoneReturn);
}

ConsoleValue VmInternal::getStringArgBuffer(U32 size)
{
   char* data = mSTR.getArgBuffer(size);
   return ConsoleValue::makeString((char*)(data - mSTR.mArgBuffer), ConsoleValue::ZoneArg);
}

ConsoleValue VmInternal::getTypeReturn(TypeId typeId)
{
   U32 size = mTypes[typeId].size;
   char* data = mSTR.getReturnBuffer(size);
   return ConsoleValue::makeTyped((char*)(data - mSTR.mArgBuffer), typeId, ConsoleValue::ZoneReturn);
}

ConsoleValue VmInternal::getTypeArg(TypeId typeId)
{
   U32 size = mTypes[typeId].size;
   char* data = mSTR.getArgBuffer(size);
   return ConsoleValue::makeTyped((char*)(data - mSTR.mArgBuffer), typeId, ConsoleValue::ZoneArg);
}



void Vm::pushValueFrame()
{
   return mInternal->mSTR.pushFrame();
}

void Vm::popValueFrame()
{
   return mInternal->mSTR.popFrame();
}

// Public
VMObject* Vm::constructObject(ClassId klassId, const char* name, int argc, const char** argv)
{
   ClassInfo* ci = &mInternal->mClassList[klassId];
   VMObject* object = new VMObject();
   
   if (ci->iCreate.CreateClassFn)
   {
      object->klass = ci;
      object->ns = NULL;
      object->userPtr = ci->iCreate.CreateClassFn(ci->userPtr, this, object);
      if (object->userPtr)
      {
         if (!ci->iCreate.ProcessArgs(this, object, name, false, false, argc, argv))
         {
            ci->iCreate.DestroyClassFn(ci->userPtr, this, object);
         }
         else
         {
            return object;
         }
      }
   }
   
   delete object;
   return NULL;
}

VMObject* Vm::setObjectNamespace(VMObject* object, NamespaceId nsId)
{
   object->ns = (Namespace*)nsId;
}

// Internal
VMObject* Vm::createVMObject(ClassId klassId, void* klassPtr)
{
   VMObject* object = new VMObject();
   object->klass = &mInternal->mClassList[klassId];
   object->ns = NULL;
   object->userPtr = klassPtr;
    return object;
}

void Vm::destroyVMObject(VMObject* object)
{
   delete object;
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

bool Vm::isNamespaceFunction(NamespaceId nsId, StringTableEntry name)
{
   Namespace* ns = (Namespace*)nsId;
   return ns->lookup(name) != NULL;
}

bool Vm::compileCodeBlock(const char* code, const char* filename, U32* outCodeSize, U32** outCode)
{
   CodeBlock* block = new CodeBlock(mInternal);
   
   U32* buffer = new U32[1024 * 1024];
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
   
   const char* ret = block->exec(0, filename, NULL, 0, 0, noCalls, NULL, setFrame);
   return ConsoleValue();
}

ConsoleValue Vm::evalCode(const char* code, const char* filename)
{
    CodeBlock *newCodeBlock = new CodeBlock(mInternal);
    const char* result = newCodeBlock->compileExec(filename, code, false, filename ? -1 : 0); // TODO: should this be 0 or -1?
    return ConsoleValue();
}

ConsoleValue Vm::call(int argc, const char** argv)
{
   ConsoleValue retValue = ConsoleValue();
   callNamespaceFunction(getGlobalNamespace(), StringTable->insert(argv[1]), argc, argv, retValue);
   return retValue;
}

ConsoleValue Vm::callObject(VMObject* h, int argc, const char** argv)
{
   ConsoleValue retValue = ConsoleValue();
   callObjectFunction(h, StringTable->insert(argv[1]), argc, argv, retValue);
   return retValue;
}

bool Vm::callObjectFunction(VMObject* self, StringTableEntry funcName, int argc, const char** argv, ConsoleValue& retValue)
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
      mInternal->printf(0, " Vm::callObjectFunction - %d has no namespace: %s", self->klass->iCreate.GetId(self), argv[0]);
      return false;
   }

   Namespace::Entry *ent = self->ns->lookup(funcName);

   if(ent == NULL)
   {
      mInternal->printf(0, "%s: undefined for object id %d", funcName, self->klass->iCreate.GetId(self));

      // Clean up arg buffers, if any.
      mInternal->mSTR.clearFunctionOffset();
      return "";
   }

   // Twiddle %this argument
   const char *oldArg1 = argv[1];
   SimObjectId cv = self->klass->iCreate.GetId(self);
   dSprintf(idBuf, sizeof(idBuf), "%u", cv);
   argv[1] = idBuf;

   if (ent->mType == Namespace::Entry::ScriptFunctionType)
   {
      // TODO: need a better way of dealing with this
      // TOFIX
      //object->pushScriptCallbackGuard();
   }

   KorkApi::VMObject* save = mInternal->mEvalState.thisObject;
   mInternal->mEvalState.thisObject = self;
   const char *ret = ent->execute(argc, argv, &mInternal->mEvalState);
   mInternal->mEvalState.thisObject = save;
   
   retValue = ConsoleValue::makeString(ret);

   if (ent->mType == Namespace::Entry::ScriptFunctionType)
   {
      //object->popScriptCallbackGuard();
   }

   // Twiddle it back
   argv[1] = oldArg1;

   // Reset the function offset so the stack
   // doesn't continue to grow unnecessarily
   mInternal->mSTR.clearFunctionOffset();

   return true;
}

bool Vm::callNamespaceFunction(NamespaceId nsId, StringTableEntry name, int argc, const char** argv, ConsoleValue& retValue)
{
   Namespace* ns = (Namespace*)nsId;
   Namespace::Entry* ent = ns->lookup(name);

   if (!ent)
   {
      mInternal->printf(0, "%s: Unknown command.", argv[0]);
      // Clean up arg buffers, if any.
      mInternal->mSTR.clearFunctionOffset();
      return false;
   }

   const char *ret = ent->execute(argc, argv, &mInternal->mEvalState);

   // Reset the function offset so the stack
   // doesn't continue to grow unnecessarily
   mInternal->mSTR.clearFunctionOffset();

   retValue = ConsoleValue::makeString(ret);

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
   ConsoleValue foundValue = mInternal->getObjectField(object, fieldName, arrayIndex, KorkApi::TypeDirectCopy, KorkApi::ConsoleValue::ZoneReturn);
   
   return (const char*)foundValue.evaluatePtr(mInternal->mAllocBase);
}

void Vm::setGlobalVariable(StringTableEntry name, KorkApi::ConsoleValue value)
{
   mInternal->mEvalState.globalVars.setVariableValue(name, value);
}

void Vm::setLocalVariable(StringTableEntry name, KorkApi::ConsoleValue value)
{
   mInternal->mEvalState.stack.last()->setVariableValue(name, value);
}

ConsoleValue Vm::getGlobalVariable(StringTableEntry name)
{
   Dictionary::Entry* e = mInternal->mEvalState.globalVars.getVariable(name);
   
   if (!e)
   {
      return ConsoleValue();
   }
   
   return mInternal->mEvalState.globalVars.getEntryValue(e);
}

ConsoleValue Vm::getLocalVariable(StringTableEntry name)
{
   Dictionary::Entry* e = mInternal->mEvalState.stack.last()->getVariable(name);
   
   if (!e)
   {
      return ConsoleValue();
   }
   
   return mInternal->mEvalState.stack.last()->getEntryValue(e);
}


bool Vm::registerGlobalVariable(StringTableEntry name, S32 type, void *dptr, const char* usage)
{
   return mInternal->mEvalState.globalVars.addVariable( name, type, dptr, usage );
}

bool Vm::removeGlobalVariable(StringTableEntry name)
{
   return name!=0 && mInternal->mEvalState.globalVars.removeVariable(name);
}

ConsoleValue::AllocBase Vm::getAllocBase() const
{
   return mInternal->mAllocBase;
}

bool Vm::isTracing()
{
   return mInternal->mEvalState.traceOn;
}

S32 Vm::getTracingStackPos()
{
   return mInternal->mEvalState.stack.size();
}

void Vm::setTracing(bool value)
{
   mInternal->mEvalState.traceOn = value;
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

void destroyVm(Vm* vm)
{
   delete vm->mInternal;
   delete vm;
}

VmInternal::VmInternal(Vm* vm, Config* cfg) : mSTR(&mAllocBase), mEvalState(this)
{
   mVM = vm;
   mConfig = *cfg;
   mCodeBlockList = NULL;
   mCurrentCodeBlock = NULL;
   mNSState.init(this);
   mTelDebugger = new TelnetDebugger(this);
   mTelConsole = new TelnetConsole(this);
   mHeapAllocs = NULL;
   mConvIndex = 0;
   
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
}

VmInternal::~VmInternal()
{
   delete mTelDebugger;
   delete mTelConsole;
   mNSState.shutdown();

   for (ConsoleHeapAlloc* alloc = mHeapAllocs; alloc; alloc = alloc->next)
   {
      mConfig.freeFn(alloc, mConfig.allocUser);
   }
   mHeapAllocs = NULL;
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

   snprintf(mTempStringConversions[mConvIndex], MaxTempStringSize, "%f", val);
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

F64 VmInternal::valueAsFloat(ConsoleValue v)
{
   switch (v.typeId)
   {
      case KorkApi::ConsoleValue::TypeInternalInt:
      return v.getInt();
      break;
      case KorkApi::ConsoleValue::TypeInternalFloat:
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
                                               KorkApi::ConsoleValue::TypeInternalFloat,
                                               KorkApi::ConsoleValue::ZoneArg).getFloat();
         }
         break;
   }
   return 0.0f;
}

S64 VmInternal::valueAsBool(ConsoleValue v)
{
   switch (v.typeId)
   {
      case KorkApi::ConsoleValue::TypeInternalInt:
      return v.getInt();
      break;
      case KorkApi::ConsoleValue::TypeInternalFloat:
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
                                               KorkApi::ConsoleValue::TypeInternalInt,
                                               KorkApi::ConsoleValue::ZoneArg).getInt();
         }
         break;
   }
   return 0;
}


S64 VmInternal::valueAsInt(ConsoleValue v)
{
   switch (v.typeId)
   {
      case KorkApi::ConsoleValue::TypeInternalInt:
      return v.getInt();
      break;
      case KorkApi::ConsoleValue::TypeInternalFloat:
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
                                               KorkApi::ConsoleValue::TypeInternalInt,
                                               KorkApi::ConsoleValue::ZoneArg).getInt();
         }
         break;
   }
   return 0;
}

const char* VmInternal::valueAsString(ConsoleValue v)
{
   switch (v.typeId)
   {
      case KorkApi::ConsoleValue::TypeInternalInt:
      return tempIntConv(v.getInt());
      break;
      case KorkApi::ConsoleValue::TypeInternalFloat:
      return tempFloatConv(v.getFloat());
      break;
      case KorkApi::ConsoleValue::TypeInternalString:
      return (const char*)v.evaluatePtr(mAllocBase);
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
                                            KorkApi::ConsoleValue::ZoneArg).evaluatePtr(mAllocBase);
      }
      break;
   }
}

void VmInternal::printf(int level, const char* fmt, ...)
{
   if (mConfig.logFn == NULL)
      return;

   char buffer[4096];
   va_list args;
   va_start(args, fmt);
   vsnprintf(buffer, sizeof(buffer), fmt, args);
   va_end(args);
   
   mConfig.logFn(level, buffer, mConfig.logUser);
}

void VmInternal::print(int level, const char* buf)
{
   if (mConfig.logFn == NULL)
      return;
   
   mConfig.logFn(level, buf, mConfig.logUser);

}

} // namespace KorkApi
