#include "embed/api.h"
#include "core/memStream.h"
#include "console/consoleInternal.h"
#include "console/consoleNamespace.h"
#include "console/telnetDebugger.h"
#include "console/telnetConsole.h"
#include "embed/internalApi.h"

namespace KorkApi
{

// Small helper to produce a zero-initialized ConsoleValue
static inline ConsoleValue makeDefaultValue()
{
   ConsoleValue v;
    return v;
}

// -------------------- Vm method stubs --------------------

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
   return mInternal->mTypes.size()-1;
}

TypeInfo* Vm::getTypeInfo(TypeId ident)
{
   return &mInternal->mTypes[ident];
}

ClassId Vm::registerClass(ClassInfo& info)
{
   mInternal->mClassList.push_back(info);
   return mInternal->mClassList.size()-1;
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

// Heap values (like strings)
ConsoleValue Vm::getStringReturnBuffer(U32 size)
{
    return makeDefaultValue();
}

ConsoleValue Vm::getTypeReturn(TypeId typeId)
{
    return makeDefaultValue();
}

void Vm::pushValueFrame()
{
   return mInternal->STR.pushFrame();
}

void Vm::popValueFrame()
{
   return mInternal->STR.popFrame();
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
      object->userPtr = ci->iCreate.CreateClassFn(ci->userPtr, object);
      if (object->userPtr)
      {
         if (!ci->iCreate.ProcessArgs(this, object, name, false, false, argc, argv))
         {
            ci->iCreate.DestroyClassFn(ci->userPtr, object);
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

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, StringCallback cb, const char* usage, S32 minArgs, S32 maxArgs)
{
   Namespace* ns = (Namespace*)nsId;
   ns->addCommand(name, cb, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, IntCallback cb, const char* usage, S32 minArgs, S32 maxArgs)
{
   Namespace* ns = (Namespace*)nsId;
   ns->addCommand(name, cb, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, FloatCallback cb, const char* usage, S32 minArgs, S32 maxArgs)
{
   Namespace* ns = (Namespace*)nsId;
   ns->addCommand(name, cb, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, VoidCallback cb, const char* usage, S32 minArgs, S32 maxArgs)
{
   Namespace* ns = (Namespace*)nsId;
   ns->addCommand(name, cb, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, BoolCallback cb, const char* usage, S32 minArgs, S32 maxArgs)
{
   Namespace* ns = (Namespace*)nsId;
   ns->addCommand(name, cb, usage, minArgs, maxArgs);
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
   
   if (!block->read(filename, stream))
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
    return makeDefaultValue();
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
      //warnf(ConsoleLogEntry::Script, "Con::execute - %d has no namespace: %s", object->getId(), argv[0]);
      return false;
   }

   Namespace::Entry *ent = self->ns->lookup(funcName);

   if(ent == NULL)
   {
      //warnf(ConsoleLogEntry::Script, "%s: undefined for object '%s' - id %d", funcName, object->getName(), object->getId());

      // Clean up arg buffers, if any.
      mInternal->STR.clearFunctionOffset();
      return "";
   }

   // Twiddle %this argument
   const char *oldArg1 = argv[1];
   ConsoleValue cv = self->klass->iCreate.GetId(self);
   switch (cv.typeId)
   {
      case ConsoleValue::TypeInternalFloat:
         dSprintf(idBuf, sizeof(idBuf), "%g", cv.getFloat());
         break;
      case ConsoleValue::TypeInternalInt:
         dSprintf(idBuf, sizeof(idBuf), "%i", cv.getInt());
         break;
      case ConsoleValue::TypeInternalString:
         dSprintf(idBuf, sizeof(idBuf), "%s", (const char*)cv.evaluatePtr(mInternal->mAllocBase));
         break;
      default:
         idBuf[0] = '\0';
         break;
   }
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
   mInternal->STR.clearFunctionOffset();

   return true;
}

bool Vm::callNamespaceFunction(NamespaceId nsId, StringTableEntry name, int argc, const char** argv, ConsoleValue& retValue)
{
   Namespace* ns = (Namespace*)nsId;
   Namespace::Entry* ent = ns->lookup(name);

   if (!ent)
   {
      // warnf(ConsoleLogEntry::Script, "%s: Unknown command.", argv[0]);
      // Clean up arg buffers, if any.
      mInternal->STR.clearFunctionOffset();
      return false;
   }

   const char *ret = ent->execute(argc, argv, &mInternal->mEvalState);

   // Reset the function offset so the stack
   // doesn't continue to grow unnecessarily
   mInternal->STR.clearFunctionOffset();

   retValue = ConsoleValue::makeString(ret);

   return true;
}

// Helpers (should call into user funcs)
VMObject* Vm::findObjectByName(const char* name)
{
    return mInternal->mConfig.iFind.FindObjectByNameFn(name);
}

VMObject* Vm::findObjectByPath(const char* path)
{
    return mInternal->mConfig.iFind.FindObjectByPathFn(path);
}

VMObject* Vm::findObjectById(SimObjectId ident)
{
   return mInternal->mConfig.iFind.FindObjectByIdFn(ident);
}

bool Vm::setObjectFieldNative(VMObject* object, StringTableEntry fieldName, void* nativeValue, U32* arrayIndex)
{
   // try normal fields
   // try custom fields
}

bool Vm::setObjectFieldString(VMObject* object, StringTableEntry fieldName, const char* stringValue, U32* arrayIndex)
{
    // no-op
}

bool Vm::getObjectFieldNative(VMObject* object, StringTableEntry fieldName, void* nativeValue, U32* arrayIndex)
{
    return false;
}

bool Vm::getObjectFieldString(VMObject* object, StringTableEntry fieldName, const char** stringValue, U32* arrayIndex)
{
    return false;
}

void Vm::setGlobalVariable(StringTableEntry name, const char* value)
{
   mInternal->mEvalState.globalVars.setVariable(name, value);
}

void Vm::setLocalVariable(StringTableEntry name, const char* value)
{
   mInternal->mEvalState.stack.last()->setVariable(name, value);
}

ConsoleValue Vm::getGlobalVariable(StringTableEntry name)
{
   return ConsoleValue();
}

ConsoleValue Vm::getLocalVariable(StringTableEntry name)
{
   return ConsoleValue();
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
    // For stubs, we don't keep state yet.
    Vm* vm = new Vm();
    vm->mInternal = new VmInternal(cfg);
    return vm;
}

void destroyVm(Vm* vm)
{
   delete vm->mInternal;
   delete vm;
}

VmInternal::VmInternal(Config* cfg) : STR(&mAllocBase)
{
   mConfig = *cfg;
   mCodeBlockList = NULL;
   mCurrentCodeBlock = NULL;
   mNSState.init();
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


const char* VmInternal::tempFloatConv(F64 val)
{
   if (mConvIndex == MaxStringConvs)
      mConvIndex = 0;

   printf(mTempStringConversions[mConvIndex], MaxTempStringSize, "%f", val);
   return mTempStringConversions[mConvIndex++];
}

const char* VmInternal::tempIntConv(U64 val)
{
   if (mConvIndex == MaxStringConvs)
      mConvIndex = 0;

   printf(mTempStringConversions[mConvIndex], MaxTempStringSize, "%llu", val);
   return mTempStringConversions[mConvIndex++];
}

} // namespace KorkApi
