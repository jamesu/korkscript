#pragma once

class Namespace;
class CodeBlock;
class TelnetDebugger;
class TelnetConsole;

#include "console/stringStack.h"

namespace KorkApi
{

struct VmInternal
{
   CodeBlock*    mCodeBlockList;
   CodeBlock*    mCurrentCodeBlock;
   TelnetDebugger* mTelDebugger;
   TelnetConsole* mTelConsole;
   
   // Namespace stuff
   NamespaceState mNSState;

   ExprEvalState mEvalState;
   StringStack STR;

   Vector<TypeInfo> mTypes;
   Vector<ClassInfo> mClassList;
   Vector<ConsoleValue> mHardRefs;
   Config mConfig;
   ConsoleValue::AllocBase mAllocBase;

   VmInternal();
   ~VmInternal();

   StringTableEntry getCurrentCodeBlockName();
   StringTableEntry getCurrentCodeBlockFullPath();
   StringTableEntry getCurrentCodeBlockModName();
   CodeBlock *findCodeBlock(StringTableEntry name);

};

}
