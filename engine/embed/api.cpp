#include "embed/api.h"
#include "core/memStream.h"
#include "console/consoleInternal.h"

namespace KorkApi
{

struct VmInternal
{
   Vector<CodeBlock*> mCodeBlocks;
   Vector<Namespace*> mNamespaces;
   Vector<TypeInfo> mTypes;
   Vector<ClassInfo> mClassList;
   Vector<ConsoleValue> mHardRefs;
   Config mConfig;
   
   VmInternal()
   {
      mNamespaces.push_back(Namespace::global());
   }
   
   ~VmInternal()
   {
   }
};

// Small helper to produce a zero-initialized ConsoleValue
static inline ConsoleValue makeDefaultValue()
{
    ConsoleValue v{};
    v.typeId = 0;
    v.integer = 0;
    return v;
}

// -------------------- Vm method stubs --------------------

S32 Vm::registerNamespace(StringTableEntry name, StringTableEntry package)
{
   Namespace* ns = Namespace::find(name, package);
   auto itr = std::find(mInternal->mNamespaces.begin(), mInternal->mNamespaces.end(), ns);
   if (itr == mInternal->mNamespaces.end())
   {
      mInternal->mNamespaces.push_back(ns);
      return mInternal->mNamespaces.size()-1;
   }
   return (itr - mInternal->mNamespaces.begin());
}

S32 Vm::getNamespace(StringTableEntry name, StringTableEntry package)
{
   auto itr = std::find_if( mInternal->mNamespaces.begin(), mInternal->mNamespaces.end(), [name, package](Namespace* ns){
      return ns->mName == name && ns->mPackage == package;
   });
    return itr == mInternal->mNamespaces.end() ? -1 : itr - mInternal->mNamespaces.begin();
}

S32 Vm::getGlobalNamespace()
{
    return 0;
}

void Vm::activatePackage(StringTableEntry pkgName)
{
   Namespace::activatePackage(pkgName);
}

void Vm::deactivatePackage(StringTableEntry pkgName)
{
   Namespace::deactivatePackage(pkgName);
}

bool Vm::linkNamespace(NamespaceId parent, NamespaceId child)
{
   Namespace *pns = mInternal->mNamespaces[parent];
   Namespace *cns = mInternal->mNamespaces[child];
   if(pns && cns)
      return cns->classLinkTo(pns);
   return false;
}

bool Vm::unlinkNamespace(NamespaceId parent, NamespaceId child)
{
   Namespace *pns = mInternal->mNamespaces[parent];
   Namespace *cns = mInternal->mNamespaces[child];
   if(pns && cns)
      return cns->unlinkClass(pns);
   return false;
}

TypeId Vm::registerType(TypeInfo& info)
{
   mInternal->mTypes.push_back(info);
   return mInternal->mTypes.size()-1;
}

ClassId Vm::registerClass(ClassInfo& info)
{
   mInternal->mClassList.push_back(info);
   return mInternal->mClassList.size()-1;
}

// Hard refs to console values
HardConsoleValueRef Vm::createHardRef(ConsoleValue value)
{
    HardConsoleValueRef r{};
    r.hardIndex = mInternal->mHardRefs.size();
    r.value = makeDefaultValue();
    mInternal->mHardRefs.push_back(value);
    return r;
}

void Vm::releaseHardRef(HardConsoleValueRef value)
{
   mInternal->mHardRefs[value.hardIndex] = ConsoleValue();
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

// Public
VMObject* Vm::constructObject(ClassId klassId, const char* name, int argc, char** argv)
{
   ClassInfo* ci = &mInternal->mClassList[klassId];
   VMObject* object = new VMObject();
   
   if (ci->iCreate.CreateClassFn)
   {
      object->klass = ci;
      object->ns = NULL;
      object->userPtr = ci->iCreate.CreateClassFn(ci->userPtr, object, name, argc, argv);
      if (object->userPtr)
      {
         return object;
      }
   }
   
   delete object;
   return NULL;
}

VMObject* Vm::setObjectNamespace(VMObject* object, NamespaceId nsId)
{
   object->ns = mInternal->mNamespaces[nsId];
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

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, StringCallback cb, const char* usage, S32 minArgs, S32 maxArgs)
{
   Namespace* ns = mInternal->mNamespaces[nsId];
   ns->addCommand(name, cb, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, IntCallback cb, const char* usage, S32 minArgs, S32 maxArgs)
{
   Namespace* ns = mInternal->mNamespaces[nsId];
   ns->addCommand(name, cb, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, FloatCallback cb, const char* usage, S32 minArgs, S32 maxArgs)
{
   Namespace* ns = mInternal->mNamespaces[nsId];
   ns->addCommand(name, cb, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, VoidCallback cb, const char* usage, S32 minArgs, S32 maxArgs)
{
   Namespace* ns = mInternal->mNamespaces[nsId];
   ns->addCommand(name, cb, usage, minArgs, maxArgs);
}

void Vm::addNamespaceFunction(NamespaceId nsId, StringTableEntry name, BoolCallback cb, const char* usage, S32 minArgs, S32 maxArgs)
{
   Namespace* ns = mInternal->mNamespaces[nsId];
   ns->addCommand(name, cb, usage, minArgs, maxArgs);
}

bool Vm::compileCodeBlock(const char* code, const char* filename, U32* outCodeSize, U32** outCode)
{
   CodeBlock* block = new CodeBlock();
   
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
   CodeBlock* block = new CodeBlock();
   
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
    CodeBlock *newCodeBlock = new CodeBlock();
    const char* result = newCodeBlock->compileExec(filename, code, false, 0);
    return makeDefaultValue();
}

ConsoleValue Vm::call(int argc, const char** argv)
{
   const char* result = Con::executef(argc, argv);
   return ConsoleValue();
}

ConsoleValue Vm::callObject(VMObject* h, int argc, const char** argv)
{
    return makeDefaultValue();
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

// -------------------- factory functions --------------------

Vm* createVM(Config* cfg)
{
    // For stubs, we don't keep state yet.
    Vm* vm = new Vm();
    vm->mInternal = new VmInternal();
    return vm;
}

void destroyVm(Vm* vm)
{
   delete vm->mInternal;
    delete vm;
}

} // namespace KorkApi
