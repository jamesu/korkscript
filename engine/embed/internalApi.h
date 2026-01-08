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

// Create ref to fixed size block
TypeStorageInterface CreateFixedTypeStorage(KorkApi::VmInternal* vmInternal, void* ptr, U16 typeId, bool isRelocatable);
// Create storage ref to block backed by ConsoleVarRef
TypeStorageInterface CreateConsoleVarTypeStorage(KorkApi::VmInternal* vmInternal, ConsoleVarRef ref, U16 typeId);
// Create storage ref to block backed by Dictionary
TypeStorageInterface CreateExprEvalTypeStorage(KorkApi::VmInternal* vmInternal, ExprEvalState& eval, U32 minSize, U16 typeId);
// Create storage ref to return buffer
TypeStorageInterface CreateExprEvalReturnTypeStorage(KorkApi::VmInternal* vmInternal, U32 minSize, U16 typeId);
// No storage, just register
TypeStorageInterface CreateRegisterStorage(KorkApi::VmInternal* vmInternal, U16 typeId);
// No storage, just registers
TypeStorageInterface CreateRegisterStorageFromArgs(KorkApi::VmInternal* vmInternal, U32 argc, KorkApi::ConsoleValue* argv);
// No storage, just registers (arg pointing to storage edition)
TypeStorageInterface CreateRegisterStorageFromArg(KorkApi::VmInternal* vmInternal, KorkApi::ConsoleValue arg);


void CopyTypeStorageValueToOutput(TypeStorageInterface* storage, KorkApi::ConsoleValue& v);

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
   CodeBlock*    mExecCodeBlockList; // temp blocks (or loaded from file)
   CodeBlock*    mCurrentCodeBlock;
   TelnetDebugger* mTelDebugger;
   TelnetConsole* mTelConsole;
   ExceptionInfo  mLastExceptionInfo;

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

   KorkApi::ConsoleValue mTempConversionValue[MaxStringConvs];
   KorkApi::ConsoleValue mReturnBufferValue;
   Vector<U8> mReturnBuffer;

   U32 mConvIndex;
   U32 mCVConvIndex;
   char mTempStringConversions[MaxStringConvs][MaxTempStringSize];

   Compiler::Resources* mCompilerResources;
   bool mOwnsResources;

   U32 mNSCounter;
   char mExecReturnBuffer[ExecReturnBufferSize];
   char mFileLineBuffer[FileLineBufferSize];

   ConsoleValue mTempValue;

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

   ConsoleValue* getTempValuePtr();

   
   // Fiber API
   void setCurrentFiberMain();
   void setCurrentFiber(FiberId fiber);
   FiberId createFiber(void* userPtr); // needs exec too
   ExprEvalState* createFiberPtr(void* userPtr); // needs exec too
   FiberId getCurrentFiber();
   void cleanupFiber(FiberId fiber);
   void suspendCurrentFiber();
   FiberRunResult resumeCurrentFiber(ConsoleValue value);
   bool getCurrentFiberFileLine(StringTableEntry* outFile, U32* outLine);
   FiberRunResult::State getCurrentFiberState();
   void clearCurrentFiberError();
   void* getCurrentFiberUserPtr();
   void throwFiber(U32 mask);
   
   void validateReturnBufferSize(U32 size);

   ConsoleHeapAllocRef createHeapRef(U32 size);
   void releaseHeapRef(ConsoleHeapAllocRef value);

   S32 lookupTypeId(StringTableEntry typeName);

   // Heap values (like strings)
   ConsoleValue getStringFuncBuffer(FiberId fiberId, U32 size);
   ConsoleValue getStringReturnBuffer(U32 size);
   ConsoleValue getTypeFunc(FiberId fiberId, TypeId typeId, U32 heapSize);
   ConsoleValue getTypeReturn(TypeId typeId, U32 heapSize);

   ConsoleValue getStringInZone(U16 zone, U32 size);
   ConsoleValue getTypeInZone(U16 zone, TypeId typeId, U32 heapSize);

   StringTableEntry getCurrentCodeBlockName();
   StringTableEntry getCurrentCodeBlockFullPath();
   StringTableEntry getCurrentCodeBlockModName();
   CodeBlock *findCodeBlock(StringTableEntry name);

   ClassInfo* getClassInfoByName(StringTableEntry name);

   const char* tempFloatConv(F64 val);
   const char* tempIntConv(U64 val);

   bool setObjectField(VMObject* object, StringTableEntry name, const char* array, ConsoleValue value);
   bool setObjectFieldTuple(VMObject* object, StringTableEntry fieldName, const char* arrayIndex, U32 argc, ConsoleValue* argv);
   ConsoleValue getObjectField(VMObject* object, StringTableEntry name, const char* array, U32 requestedType, U32 requestedZone);
   U16 getObjectFieldType(VMObject* object, StringTableEntry name, const char* array);

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
