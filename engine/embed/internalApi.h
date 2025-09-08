#pragma once

class Namespace;
class CodeBlock;
class TelnetDebugger;
class TelnetConsole;

#include "console/stringStack.h"
#include "console/consoleNamespace.h"
#include "console/consoleInternal.h"

namespace KorkApi
{

struct VmInternal
{
   enum
   {
      MaxTempStringSize = 16,
      MaxStringConvs = 16
   };

   KorkApi::Vm* mVM;
   CodeBlock*    mCodeBlockList;
   CodeBlock*    mCurrentCodeBlock;
   TelnetDebugger* mTelDebugger;
   TelnetConsole* mTelConsole;

   StringTableEntry mCurrentFile;
   StringTableEntry mCurrentRoot;
   
   // Namespace stuff
   NamespaceState mNSState;

   ExprEvalState mEvalState;
   StringStack mSTR;

   Vector<TypeInfo> mTypes;
   Vector<ClassInfo> mClassList;

   KorkApi::ConsoleHeapAlloc* mHeapAllocs;
   Config mConfig;
   ConsoleValue::AllocBase mAllocBase;

   U32 mConvIndex;
   char mTempStringConversions[MaxTempStringSize][MaxStringConvs];

   VmInternal(KorkApi::Vm* vm, Config* cfg);
   ~VmInternal();

   ConsoleHeapAllocRef createHeapRef(U32 size);
   void releaseHeapRef(ConsoleHeapAllocRef value);

   // Heap values (like strings)
   ConsoleValue getStringReturnBuffer(U32 size);
   ConsoleValue getStringArgBuffer(U32 size);
   ConsoleValue getTypeArg(TypeId typeId);
   ConsoleValue getTypeReturn(TypeId typeId);

   StringTableEntry getCurrentCodeBlockName();
   StringTableEntry getCurrentCodeBlockFullPath();
   StringTableEntry getCurrentCodeBlockModName();
   CodeBlock *findCodeBlock(StringTableEntry name);

   ClassInfo* getClassInfoByName(StringTableEntry name);

   const char* tempFloatConv(F64 val);
   const char* tempIntConv(U64 val);

   void setObjectField(StringTableEntry name, const char* array, ConsoleValue value);
   ConsoleValue getObjectField(StringTableEntry name, const char* array);

   void printf(int level, const char* fmt, ...);
   void print(int level, const char* buf);

};

}
