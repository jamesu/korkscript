#pragma once

class Namespace;
class CodeBlock;
class TelnetDebugger;
class TelnetConsole;

#include "console/stringStack.h"
#include "console/consoleNamespace.h"
#include "console/consoleInternal.h"
#include "core/freeListHandleHelpers.h"

namespace Compiler
{
   struct Resources;
}

namespace KorkApi
{

typedef FreeListPtr<ExprEvalState, FreeListHandle::Basic32> InternalFiberList;

struct VmInternal
{
   enum
   {
      MaxTempStringSize = 16,
      MaxStringConvs = 32,
      ExecReturnBufferSize = 32,
      FileLineBufferSize = 512
   };

   KorkApi::Vm* mVM;
   CodeBlock*    mCodeBlockList;
   CodeBlock*    mCurrentCodeBlock;
   TelnetDebugger* mTelDebugger;
   TelnetConsole* mTelConsole;

   Dictionary mGlobalVars;
   
   // Namespace stuff
   NamespaceState mNSState;

   ExprEvalState* mCurrentFiberState;
   InternalFiberList mFiberStates;
   ClassChunker<ExprEvalState> mFiberAllocator;

   Vector<TypeInfo> mTypes;
   Vector<ClassInfo> mClassList;

   KorkApi::ConsoleHeapAlloc* mHeapAllocs;
   Config mConfig;
   ConsoleValue::AllocBase mAllocBase;

   Vector<U8> mReturnBuffer;

   U32 mConvIndex;
   char mTempStringConversions[MaxStringConvs][MaxTempStringSize];

   Compiler::Resources* mCompilerResources;
   bool mOwnsResources;

   U32 mNSCounter;
   char mExecReturnBuffer[ExecReturnBufferSize];
   char mFileLineBuffer[FileLineBufferSize];

   VmInternal(KorkApi::Vm* vm, Config* cfg);
   ~VmInternal();

   inline void incVMRef(VMObject* object)
   {
      object->refCount++;
   }
   
   inline void decVMRef(VMObject* object)
   {
      object->refCount--;
      if (object->refCount == 0)
      {
         AssertFatal(object->userPtr, "Userptr still present with no refs, check refs!");
         delete object;
      }
   }

   
   // Fiber API
   void setCurrentFiberMain();
   void setCurrentFiber(FiberId fiber);
   FiberId createFiber(void* userPtr); // needs exec too
   FiberId getCurrentFiber();
   void cleanupFiber(FiberId fiber);
   void suspendCurrentFiber();
   FiberRunResult resumeCurrentFiber(ConsoleValue value);
   FiberRunResult::State getCurrentFiberState();
   void* getCurrentFiberUserPtr();
   void throwFiber(U32 mask);
   
   void validateReturnBufferSize(U32 size);

   ConsoleHeapAllocRef createHeapRef(U32 size);
   void releaseHeapRef(ConsoleHeapAllocRef value);

   // Heap values (like strings)
   ConsoleValue getStringFuncBuffer(FiberId fiberId, U32 size);
   ConsoleValue getStringReturnBuffer(U32 size);
   ConsoleValue getTypeFunc(FiberId fiberId, TypeId typeId);
   ConsoleValue getTypeReturn(TypeId typeId);

   ConsoleValue getStringInZone(U16 zone, U32 size);
   ConsoleValue getTypeInZone(U16 zone, TypeId typeId);

   StringTableEntry getCurrentCodeBlockName();
   StringTableEntry getCurrentCodeBlockFullPath();
   StringTableEntry getCurrentCodeBlockModName();
   CodeBlock *findCodeBlock(StringTableEntry name);

   ClassInfo* getClassInfoByName(StringTableEntry name);

   const char* tempFloatConv(F64 val);
   const char* tempIntConv(U64 val);

   bool setObjectField(VMObject* object, StringTableEntry name, const char* array, ConsoleValue value);
   ConsoleValue getObjectField(VMObject* object, StringTableEntry name, const char* array, U32 requestedType, U32 requestedZone);

   void printf(int level, const char* fmt, ...);
   void print(int level, const char* buf);

   // Conversion helpers
   F64 valueAsFloat(ConsoleValue v);
   S64 valueAsInt(ConsoleValue v);
   S64 valueAsBool(ConsoleValue v);
   const char* valueAsString(ConsoleValue v);

   void assignFieldsFromTo(VMObject* from, VMObject* to);
};

}
