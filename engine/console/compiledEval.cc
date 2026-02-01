//-----------------------------------------------------------------------------
// Copyright (c) 2025-2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Copyright (c) 2013 GarageGames, LLC
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
#include "console/consoleNamespace.h"

#include "core/findMatch.h"
#include "console/consoleInternal.h"
#include "console/consoleNamespace.h"
#include "console/compiler.h"

#include "console/stringStack.h"

#include "console/telnetDebugger.h"


#include "console/ast.h"

using namespace Compiler;

struct LocalRefTrack
{
   KorkApi::VmInternal* vm;
   KorkApi::VMObject* obj;

   LocalRefTrack(KorkApi::VmInternal* _vm) : vm(_vm), obj(nullptr) {;}
   ~LocalRefTrack()
   {
      if (obj)
         vm->decVMRef(obj);
   }
   
   LocalRefTrack& operator=(const LocalRefTrack& other)
   {
      if (obj && obj != other.obj)
      {
         vm->decVMRef(obj);
      }
      obj = other.obj;
      if (obj)
      {
         vm->incVMRef(obj);
      }
      return *this;
   }

   LocalRefTrack& operator=(KorkApi::VMObject* object)
   {
      if (obj && obj != object)
      {
         vm->decVMRef(obj);
      }
      obj = object;
      if (obj)
      {
         vm->incVMRef(obj);
      }
      return *this;
   }

   KorkApi::VMObject& operator*() const { return *obj; }
   KorkApi::VMObject* operator->() const { return obj; }
   operator KorkApi::VMObject*() const { return obj; }
   explicit operator bool() const { return obj != nullptr; }
   
   bool isValid()
   {
      return obj;
   }
};

struct ConsoleFrame
{
   enum
   {
      FieldArraySize = 256,
   };

   
   // Context (16 bytes)
   Dictionary dictionary;
   ExprEvalState* evalState;
   
   // Frame state (24 bytes + 20 bytes)
   F64*            curFloatTable;
   char*           curStringTable;
   bool            noCalls;
   bool            isReference;
   bool            inNativeFunction;
   bool            inFunctionCall;
   bool            popMinDepth;
   U8              pushStringStackCount;
   U32             failJump;
   U32             stackStart; // string stack offset
   U32             callArgc;
   U32             ip;
   
   // Function state (24 bytes)
   StringTableEntry  thisFunctionName;
   const char*       curFNDocBlock;
   const char*       curNSDocBlock;

   // Generic state stuff
   
   // Variable manipulation refs (32 bytes)
   ConsoleVarRef currentVar;
   ConsoleVarRef copyVar;
   
   // Stack offsets (8 bytes)
   U16 _FLT;
   U16 _UINT;
   U16 _ITER;
   U16 _OBJ;
   U16 _TRY;
   U16 _STARTOBJ;

   // Dynamic type id (codeblock-local)
   U16 dynTypeId;

   // Last call type for restore
   U32 lastCallType;

   // Docblock processing
   U32 nsDocBlockClassOffset;
   U32 nsDocBlockOffset;
   U8 nsDocBlockClassNameLength;
   U8 nsDocBlockClassLocation;
   
   // These were from Dictionary (24 bytes)
   StringTableEntry scopeName;
   StringTableEntry scopePackage;
   Namespace *scopeNamespace; // BASE namespace (e.g. if we call Foo::abc() its Foo)
   CodeBlock *codeBlock;
   
   // References used across opcodes (40 bytes + 16 bytes)
   LocalRefTrack currentNewObject;
   LocalRefTrack thisObject;
   LocalRefTrack prevObject;
   LocalRefTrack curObject;
   LocalRefTrack saveObject;
   LocalRefTrack curIterObject; // current object for _ITER
   StringTableEntry prevField;
   StringTableEntry curField;

   // Buffers (640 bytes)
   char curFieldArray[FieldArraySize];
   char prevFieldArray[FieldArraySize];

public:
   ConsoleFrame(KorkApi::VmInternal* vm, ExprEvalState* fiber, Dictionary::HashTableData* parentVars = nullptr)
      : stackStart(0)
      , dictionary(vm, parentVars)
      , evalState(nullptr)
      , curFloatTable(nullptr)
      , curStringTable(nullptr)
      //, curStringTableLen(0)
      , thisFunctionName(nullptr)
      , pushStringStackCount(0)
      , noCalls(false)
      , isReference(false)
      , inNativeFunction(false)
      , inFunctionCall(false)
      , popMinDepth(false)
      , failJump(0)
      , scopeName( nullptr )
      , scopePackage( nullptr )
      , scopeNamespace( nullptr )
      , codeBlock( nullptr )
      , thisObject( nullptr )
      , ip( 0 )
      , dynTypeId( 0 )
      , _FLT(0)
      , _UINT(0)
      , _ITER(0)
      , _OBJ(0)
      , _STARTOBJ(0)
      , _TRY(0)
      , currentNewObject(vm)
      , prevObject(vm)
      , curObject(vm)
      , saveObject(vm)
      , curIterObject(vm)
      , prevField(nullptr)
      , curField(nullptr)
      , curFNDocBlock(nullptr)
      , curNSDocBlock(nullptr)
      , callArgc(0)
      , lastCallType(0)
      , nsDocBlockClassOffset(0)
      , nsDocBlockClassNameLength(0)
      , nsDocBlockOffset(0)
      , nsDocBlockClassLocation(0)
   {
      evalState = fiber;
      memset(curFieldArray,   0, sizeof(curFieldArray));
      memset(prevFieldArray,  0, sizeof(prevFieldArray));
   }
   
   inline void copyFrom(ConsoleFrame* other, bool includeScope);
   inline void setCurVarName(StringTableEntry name);
   inline void setCurVarNameCreate(StringTableEntry name);

   inline S32 getIntVariable();
   inline F64 getFloatVariable();

   inline const char *getStringVariable();
   inline KorkApi::ConsoleValue getConsoleVariable();
   inline void setUnsignedVariable(U32 val);
   inline void setNumberVariable(F64 val);
   inline void setStringVariable(const char *val);
   inline void setConsoleValue(KorkApi::ConsoleValue value);
   inline void setConsoleValues(U32 argc, KorkApi::ConsoleValue* values);
   inline void setCopyVariable();
};

ConsoleFrame& ExprEvalState::getCurrentFrame()
{
   return *vmFrames.back();
}



const char *ExprEvalState::getNamespaceList(Namespace *ns)
{
   U32 size = 1;
   Namespace * walk;
   for(walk = ns; walk; walk = walk->mParent)
      size += strlen(walk->mName) + 4;
   char *ret = (char*)mSTR.getFuncBuffer(KorkApi::ConsoleValue::TypeInternalString, size).evaluatePtr(vmInternal->mAllocBase);
   ret[0] = 0;
   for(walk = ns; walk; walk = walk->mParent)
   {
      strcat(ret, walk->mName);
      if(walk->mParent)
         strcat(ret, " -> ");
   }
   return ret;
}

//------------------------------------------------------------

inline void ConsoleFrame::copyFrom(ConsoleFrame* other, bool includeScope)
{
   _FLT = other->_FLT;
   _UINT = other->_UINT;
   _ITER = other->_ITER;
   _OBJ = _STARTOBJ = other->_OBJ;
   _TRY = other->_TRY;
   
   if (includeScope)
   {
      scopeName = other->scopeName;
      scopePackage = other->scopePackage;
      scopeNamespace = other->scopeNamespace;
   }
}

inline void ConsoleFrame::setCurVarName(StringTableEntry name)
{
   if(name[0] == '$')
   {
      currentVar.var = evalState->vmInternal->mGlobalVars.lookup(name);
      currentVar.dictionary = &evalState->vmInternal->mGlobalVars;
   }
   else
   {
      currentVar.var = dictionary.lookup(name);
      currentVar.dictionary = &dictionary;
   }
   if(!currentVar.var && evalState->vmInternal->mConfig.warnUndefinedScriptVariables)
   {
      evalState->vmInternal->printf(1, "Variable referenced before assignment: %s", name);
   }
}

inline void ConsoleFrame::setCurVarNameCreate(StringTableEntry name)
{
   if(name[0] == '$')
   {
      currentVar.var = evalState->vmInternal->mGlobalVars.add(name);
      currentVar.dictionary = &evalState->vmInternal->mGlobalVars;
   }
   else
   {
      currentVar.var = dictionary.add(name);
      currentVar.dictionary = &dictionary;
   }
}

//------------------------------------------------------------

inline S32 ConsoleFrame::getIntVariable()
{
   return currentVar.var ? currentVar.dictionary->getEntryUnsignedValue(currentVar.var) : 0;
}

inline F64 ConsoleFrame::getFloatVariable()
{
   return currentVar.var ? currentVar.dictionary->getEntryNumberValue(currentVar.var) : 0;
}

inline const char *ConsoleFrame::getStringVariable()
{
   return currentVar.var ? currentVar.dictionary->getEntryStringValue(currentVar.var) : 0;
}

inline KorkApi::ConsoleValue ConsoleFrame::getConsoleVariable()
{
   return currentVar.var ? currentVar.dictionary->getEntryValue(currentVar.var) : KorkApi::ConsoleValue();
}


//------------------------------------------------------------

inline void ConsoleFrame::setUnsignedVariable(U32 val)
{
   AssertFatal(currentVar.var != nullptr, "Invalid evaluator state - trying to set null variable!");
   currentVar.dictionary->setEntryUnsignedValue(currentVar.var, val);
}

inline void ConsoleFrame::setNumberVariable(F64 val)
{
   AssertFatal(currentVar.var != nullptr, "Invalid evaluator state - trying to set null variable!");
   currentVar.dictionary->setEntryNumberValue(currentVar.var, val);
}

inline void ConsoleFrame::setStringVariable(const char *val)
{
   AssertFatal(currentVar.var != nullptr, "Invalid evaluator state - trying to set null variable!");
   currentVar.dictionary->setEntryStringValue(currentVar.var, val);
}

inline void ConsoleFrame::setConsoleValue(KorkApi::ConsoleValue value)
{
   AssertFatal(currentVar.var != nullptr, "Invalid evaluator state - trying to set null variable!");
   currentVar.dictionary->setEntryValue(currentVar.var, value);
}

inline void ConsoleFrame::setConsoleValues(U32 argc, KorkApi::ConsoleValue* values)
{
   AssertFatal(currentVar.var != nullptr, "Invalid evaluator state - trying to set null variable!");
   currentVar.dictionary->setEntryValues(currentVar.var, argc, values);
}

inline void ConsoleFrame::setCopyVariable()
{
   if (copyVar.var)
   {
      switch (copyVar.var->mConsoleValue.typeId)
      {
         case KorkApi::ConsoleValue::TypeInternalUnsigned:
            currentVar.dictionary->setEntryUnsignedValue(currentVar.var, copyVar.dictionary->getEntryUnsignedValue(copyVar.var));
         break;
         case KorkApi::ConsoleValue::TypeInternalNumber:
            currentVar.dictionary->setEntryNumberValue(currentVar.var, copyVar.dictionary->getEntryNumberValue(copyVar.var));
         break;
         default:
            currentVar.dictionary->setEntryStringValue(currentVar.var, copyVar.dictionary->getEntryStringValue(copyVar.var));
         break;
      }
   }
   else if (currentVar.var)
   {
      currentVar.dictionary->setEntryStringValue(currentVar.var, ""); // needs to be set to blank if copyVariable doesn't exist
   }
}

//------------------------------------------------------------

void CodeBlock::getFunctionArgs(char buffer[1024], U32 ip)
{
   U32 fnArgc = code[ip + 5];
   buffer[0] = 0;
   for(U32 i = 0; i < fnArgc; i++)
   {
      StringTableEntry var = Compiler::CodeToSTE(nullptr, identStrings, code, ip + (i*2) + 6);
      
      // Add a comma so it looks nice!
      if(i != 0)
         strcat(buffer, ", ");
      
      strcat(buffer, "var ");
      
      // Try to capture junked parameters
      if(var[0])
         strcat(buffer, var+1);
      else
         strcat(buffer, "JUNK");
   }
}

//-----------------------------------------------------------------------------

inline void* safeObjectUserPtr(KorkApi::VMObject* obj)
{
   return obj ? obj->userPtr : nullptr;
}


void ExprEvalState::setCreatedObject(U32 index, KorkApi::VMObject* object, U32 failJump)
{
   objectCreationStack[index].newObject = object;
   objectCreationStack[index].failJump = failJump;
   
   if (object)
   {
      vmInternal->incVMRef(object);
   }
}

void ExprEvalState::clearCreatedObject(U32 index, LocalRefTrack& outTrack, U32* outJump)
{
   *outJump = objectCreationStack[index].failJump;
   KorkApi::VMObject* newObject = objectCreationStack[index].newObject;
   outTrack = newObject;
   
   if (newObject)
   {
      objectCreationStack[index].newObject = nullptr;
      vmInternal->decVMRef(newObject);
   }
}

void ExprEvalState::clearCreatedObjects(U32 start, U32 end)
{
   for (U32 i=start; i<end; i++)
   {
      if (objectCreationStack[i].newObject)
      {
         vmInternal->decVMRef(objectCreationStack[i].newObject);
         objectCreationStack[i].newObject = nullptr;
      }
   }
}

ConsoleFrame& CodeBlock::setupExecFrame(
   ExprEvalState& eval,
   U32*        code,
   U32              ip,
   const char*      packageName,
   Namespace*       thisNamespace,
   KorkApi::ConsoleValue*    argv,
   S32              argc,
   S32              setFrame,
   bool isNativeFrame)
{
   ConsoleFrame* newFrame = nullptr;
   if (isNativeFrame)
   {
      eval.pushMinStackDepth();
   }
   
   // --- Function call case (argv != nullptr) ---
   if (argv)
   {
      // assume this points into a function decl:
      U32 fnArgc = code[ip + 2 + 6];
      StringTableEntry fnName = Compiler::CodeToSTE(nullptr, identStrings, code, ip);
      S32 wantedArgc = getMin(argc - 1, fnArgc); // argv[0] is func name

      // Trace output
      if (eval.traceOn)
      {
         eval.traceBuffer[0] = 0;
         strcat(eval.traceBuffer, "Entering ");
         if (packageName)
         {
            strcat(eval.traceBuffer, "[");
            strcat(eval.traceBuffer, packageName);
            strcat(eval.traceBuffer, "]");
         }

         if (thisNamespace && thisNamespace->mName)
         {
            snprintf(
               eval.traceBuffer + strlen(eval.traceBuffer),
               ExprEvalState::TraceBufferSize - strlen(eval.traceBuffer),
               "%s::%s(", thisNamespace->mName, fnName);
         }
         else
         {
            snprintf(
               eval.traceBuffer + strlen(eval.traceBuffer),
               ExprEvalState::TraceBufferSize - strlen(eval.traceBuffer),
               "%s(", fnName);
         }

         for (U32 i = 0; i < wantedArgc; i++)
         {
            strcat(eval.traceBuffer, mVM->valueAsString(argv[i + 1]));
            if (i != wantedArgc - 1)
               strcat(eval.traceBuffer, ", ");
         }
         strcat(eval.traceBuffer, ")");
         snprintf(
            eval.traceBuffer + strlen(eval.traceBuffer),
            ExprEvalState::TraceBufferSize - strlen(eval.traceBuffer),
            " [f=%i,ss=%i,sf=%i]", eval.vmFrames.size(), eval.mSTR.mNumFrames, eval.mSTR.mStartStackSize);
         mVM->printf(0, "%s", eval.traceBuffer);
      }

      // Push a new frame for the function call and get a ref to it.
      eval.pushFrame(fnName, thisNamespace, packageName, this, ip);
      newFrame = eval.vmFrames.back();
      newFrame->thisFunctionName = fnName;
      newFrame->inFunctionCall = true;

      // Bind arguments into the new frame's locals
      for (U32 i = 0; i < (U32)wantedArgc; i++)
      {
         StringTableEntry var =
            Compiler::CodeToSTE(nullptr, identStrings, code, ip + (2 + 6 + 1) + (i * 2));
         newFrame->setCurVarNameCreate(var);
         newFrame->setConsoleValue(argv[i + 1]);
      }

      ip = ip + (fnArgc * 2) + (2 + 6 + 1);

      newFrame->curFloatTable     = functionFloats;
      newFrame->curStringTable    = functionStrings;
   }
   else
   {
      // Do we want this code to execute using a new stack frame?
      if (setFrame < 0 || eval.vmFrames.empty())
      {
         // Always push a fresh frame
         eval.pushFrame(nullptr, nullptr, nullptr, this, ip);
      }
      else
      {
         // Copy a reference to an existing stack frame onto the top of the stack.
         // Any change to locals during this new frame also affects the original frame.
         S32 stackIndex = eval.vmFrames.size() - setFrame - 1;
         eval.pushFrameRef(stackIndex, this, ip);
      }
      
      newFrame = eval.vmFrames.back();
      newFrame->curFloatTable     = globalFloats;
      newFrame->curStringTable    = globalStrings;
   }
   
   newFrame->popMinDepth = isNativeFrame;
   newFrame->ip = ip; // update ip
   return *newFrame;
}

KorkApi::ConsoleValue CodeBlock::exec(U32 ip, const char *functionName, Namespace *thisNamespace, U32 argc,  KorkApi::ConsoleValue* argv, bool noCalls, bool isNativeFrame, StringTableEntry packageName, S32 setFrame, bool startSuspended)
{
   ExprEvalState& evalState = *mVM->mCurrentFiberState;
   evalState.mSTR.clearFunctionOffset();
   
   if (evalState.mState == KorkApi::FiberRunResult::SUSPENDED)
   {
      mVM->printf(0, "Fiber suspended");
      return KorkApi::ConsoleValue();
   }
   
   // Setup frame state
   ConsoleFrame* frame = beginExec(evalState,
                                        ip,
                                        functionName,
                                        thisNamespace,
                                        argc,
                                        argv,
                                        noCalls,
                                        isNativeFrame,
                                        packageName,
                                        setFrame);
   
   evalState.mSTR.setStringValue(""); // this should be cleared before exec (otherwise ops can get garbage values)
   
   if (frame && !startSuspended)
   {
      KorkApi::FiberRunResult result = evalState.runVM();
      return result.value;
   }
   
   return KorkApi::ConsoleValue();
}

ConsoleFrame* CodeBlock::beginExec(ExprEvalState& evalState, U32 ip, const char *functionName, Namespace *thisNamespace, U32 argc,  KorkApi::ConsoleValue* argv, bool noCalls,  bool isNativeFrame, StringTableEntry packageName, S32 setFrame)
{
   evalState.mSTR.clearFunctionOffset();
   evalState.mState = KorkApi::FiberRunResult::RUNNING;
   
   // Setup frame state
   ConsoleFrame& frame = setupExecFrame(evalState,
                                        code,
                                        ip,
                                        packageName,
                                        thisNamespace,
                                        argv,
                                        argc,
                                        setFrame,
                                        isNativeFrame);
   
   frame.stackStart = evalState.mSTR.mStartStackSize;
   frame.noCalls = noCalls;
   
   // TODO: this needs to push frame info too
   // Grab the state of the telenet debugger here once
   // so that the push and pop frames are always balanced.
   const bool telDebuggerOn = mVM->mTelDebugger && mVM->mTelDebugger->isConnected();
   if ( telDebuggerOn && !frame.isReference )
      mVM->mTelDebugger->pushStackFrame();
   
   // NOTE: should be handled in pushFrame
   //incRefCount();
   
   return &frame;
}

#define FIBERS_START while (mState == KorkApi::FiberRunResult::RUNNING) {
#define FIBER_STATE(thestate) mState = thestate;
#define FIBERS_END }

KorkApi::FiberRunResult ExprEvalState::runVM()
{
   //printf("Frame size=%u exprSize=%u dbSize=%u\n", sizeof(ConsoleFrame), sizeof(ExprEvalState), sizeof(CodeBlock));
   
   if (vmFrames.size() == 0)
   {
      return KorkApi::FiberRunResult();
   }

   auto cleanupIterator = [](ExprEvalState& evalState, ConsoleFrame& frame){
      frame.curIterObject = nullptr;

      // Clear iterator state.
      while ( frame._ITER > 0 )
      {
         IterStackRecord& iter = evalState.iterStack[ -- frame._ITER ];
         if (iter.mIsStringIter)
         {
            evalState.mSTR.rewind();
         }
         iter.mIsStringIter = false;
      }
   };
   
   lastThrow = 0;

   bool checkExtraStack = true;
   KorkApi::ConsoleValue tmpVal = KorkApi::ConsoleValue();
   KorkApi::FiberRunResult result;
   result.state = mState;
   result.value = KorkApi::ConsoleValue();
   
   FIBERS_START
   
   S32 lastTypeId = -1;
   
   // NOTE: these are only used temporarily inside opcode cases
   StringTableEntry tmpVar = nullptr;
   StringTableEntry tmpFnName = nullptr;
   StringTableEntry tmpFnNamespace = nullptr;
   StringTableEntry tmpFnPackage = nullptr;
   // These used for lookup but this all happens within a single step
   Namespace::Entry* tmpNsEntry = nullptr;
   Namespace*        tmpNs = nullptr;  // ACTIVE namespace (e.g. for parentcall)
   // Tmp args
   U32 callArgc = 0;
   KorkApi::ConsoleValue* callArgv = nullptr;
   const char**           callArgvS = nullptr;
   
   AssertFatal(!vmFrames.empty(), "no frames");
   
   ConsoleFrame& frame = *vmFrames.back();
   ExprEvalState& evalState = *this;
   U32 ip = frame.ip;
   const U32* code = frame.codeBlock->code;
   KorkApi::Vm* vmPublic = vmInternal->mVM;
   bool loopFrameSetup = false;
   StringTableEntry* identStrings = frame.codeBlock->identStrings;
   
   // Ensure we are using correct codeblock (NOTE: refcount is handled by frame push)
   vmInternal->mCurrentCodeBlock = frame.codeBlock;
   if(frame.codeBlock->name)
   {
      evalState.mCurrentFile = frame.codeBlock->name;
      evalState.mCurrentRoot = frame.codeBlock->mRoot;
   }
   
   // If we came from a native function, process it
   if (frame.inNativeFunction)
   {
      // NOTE: result of the function should be in ExprEvalState mLastFiberValue
      // This gets set either in the control functions OR if a suspend didn't occur this
      // will be set directly after the call.
      if (frame.pushStringStackCount > 0)
      {
         evalState.mSTR.popFrame();
         frame.pushStringStackCount--;
      }
      
      if ((frame.lastCallType == Namespace::Entry::VoidCallbackType) && (code[ip] != OP_STR_TO_NONE))
      {
         vmInternal->printf(0, "%s: Call to %s in %s uses result of void function call.", frame.codeBlock->getFileLine(ip-4), tmpFnName, frame.scopeName);
      }
      
      if(code[ip] == OP_STR_TO_UINT)
      {
         ip++;
         evalState.intStack[++frame._UINT] = vmInternal->valueAsInt(mLastFiberValue);
      }
      else if(code[ip] == OP_STR_TO_FLT)
      {
         ip++;
         evalState.floatStack[++frame._FLT] = vmInternal->valueAsFloat(mLastFiberValue);
      }
      else
      {
         if (code[ip] == OP_STR_TO_NONE)
         {
            ip++;
         }
         
         // NOTE: can't assume this since concat may occur after this;
         // ideally we need something like OP_STR_TO_VALUE
         //evalState.mSTR.setConsoleValue(result);
         evalState.mSTR.setStringValue(vmInternal->valueAsString(mLastFiberValue));
      }
      
      if(frame.lastCallType == FuncCallExprNode::MethodCall)
         frame.thisObject = frame.saveObject;
      
      frame.inNativeFunction = false;
      frame.ip = ip;
   }
   
   for(;;)
   {
#ifdef LINE_DEBUG
      printf("LINE %s\n", frame.codeBlock->getFileLine(ip));
#endif
      
      const U32 instruction = code[ip++];
      
   breakContinue:
      switch(instruction)
      {
         case OP_FUNC_DECL:
            if(!frame.noCalls)
            {
               tmpFnName       = Compiler::CodeToSTE(nullptr, identStrings, code, ip);
               tmpFnNamespace  = Compiler::CodeToSTE(nullptr, identStrings, code, ip+2);
               tmpFnPackage    = Compiler::CodeToSTE(nullptr, identStrings, code, ip+4);
               bool hasBody = ( code[ ip + 6 ] & 0x01 ) != 0;
               U32 lineNumber = code[ ip + 6 ] >> 1;
               
               vmInternal->mNSState.unlinkPackages();
               tmpNs = vmInternal->mNSState.find(tmpFnNamespace, tmpFnPackage);
               tmpNs->addFunction(tmpFnName, frame.codeBlock, hasBody ? ip : 0 );// if no body, set the IP to 0
               
               if (frame.nsDocBlockClassLocation != 0 && frame.nsDocBlockClassNameLength > 0)
               {
                  const char* baseDocStr = frame.nsDocBlockClassLocation == 1 ? frame.codeBlock->globalStrings : frame.codeBlock->functionStrings;
                  const char* classText = baseDocStr + frame.nsDocBlockClassOffset;

                  if (tmpFnNamespace == vmInternal->lookupStringN(classText, frame.nsDocBlockClassNameLength, false))
                  {
                     const char *usageStr = baseDocStr + frame.nsDocBlockOffset;
                     tmpNs->mUsage = nullptr;
                     tmpNs->mDynamicUsage = usageStr;
                     frame.nsDocBlockClassLocation = 0;
                  }
               }
               vmInternal->mNSState.relinkPackages();
               
               // If we had a docblock, it's definitely not valid anymore, so clear it out.
               frame.curFNDocBlock = nullptr;
               
               //Con::printf("Adding function %s::%s (%d)", fnNamespace, fnName, ip);
            }
            ip = code[ip + 7];
            break;
            
         case OP_CREATE_OBJECT:
         {
            // Read some useful info.
            tmpVar        = Compiler::CodeToSTE(nullptr, identStrings, code, ip); // objParent
            bool isDataBlock =          code[ip + 2];
            bool isInternal  =          code[ip + 3];
            bool isSingleton =          code[ip + 4];
            U32  lineNumber  =          code[ip + 5];
            frame.failJump         =          code[ip + 6];
            
            // If we don't allow calls, we certainly don't allow creating objects!
            // Moved this to after frame.failJump is set. Engine was crashing when
            // noCalls = true and an object was being created at the beginning of
            // a file. ADL.
            if(frame.noCalls)
            {
               ip = frame.failJump;
               break;
            }
            
            // Push the old info to the stack
            //Assert( objectCreationStackIndex < objectCreationStackSize );

            evalState.setCreatedObject(frame._OBJ++, frame.currentNewObject, frame.failJump);
            
            // Get the constructor information off the stack.
            evalState.mSTR.getArgcArgv(nullptr, &callArgc, &callArgv);
            evalState.mSTR.convertArgv(vmInternal, callArgc, &callArgvS);
            const char *objectName = callArgvS[ 2 ];
            
            // Con::printf("Creating object...");
            
            // objectName = argv[1]...
            frame.currentNewObject = nullptr;
            
            // Are we creating a datablock? If so, deal with case where we override
            // an old one.
            if(isDataBlock)
            {
               // Con::printf("  - is a datablock");
               
               // Find the old one if any.
               KorkApi::VMObject *db = vmInternal->mConfig.iFind.FindDatablockGroup(vmInternal->mConfig.findUser);
               
               // Make sure we're not changing types on ourselves...
               if(db && strcasecmp(db->klass->name, callArgvS[1]))
               {
                  vmInternal->printf(0, "Cannot re-declare data block %s with a different class.", callArgv[2]);
                  ip = frame.failJump;
                  break;
               }
               
               // If there was one, set the currentNewObject and move on.
               if(db)
               {
                  frame.currentNewObject = db;
               }
            }

            // For singletons, delete the old object if it exists
            if ( isSingleton )
            {
               KorkApi::VMObject *oldObject = vmPublic->findObjectByName( objectName );
               if (oldObject)
               {
                  // Prevent stack value corruption
                  evalState.mSTR.pushFrame();
                  frame.pushStringStackCount++;
                  // --
                  
                  oldObject->klass->iCreate.RemoveObjectFn(oldObject->klass->userPtr, vmPublic, oldObject);
                  oldObject->klass->iCreate.DestroyClassFn(oldObject->klass->userPtr, vmPublic, oldObject->userPtr);
                  
                  oldObject = nullptr;

                  // Prevent stack value corruption
                  evalState.mSTR.popFrame();
                  frame.pushStringStackCount--;
               }
            }
            
            evalState.mSTR.popFrame();
            frame.pushStringStackCount--;
            
            if(!frame.currentNewObject.isValid())
            {
               // Well, looks like we have to create a new object.
               KorkApi::ClassInfo* klassInfo = vmInternal->getClassInfoByName(vmInternal->internString(callArgvS[1], false));
               KorkApi::VMObject *object = nullptr;

               if (klassInfo)
               {
                  object = vmInternal->New<KorkApi::VMObject>();
                  object->klass = klassInfo;
                  object->ns = nullptr;

                  KorkApi::CreateClassReturn ret = {};
                  klassInfo->iCreate.CreateClassFn(klassInfo->userPtr,  vmPublic, &ret);
                  object->userPtr = ret.userPtr;
                  object->flags = ret.initialFlags;

                  if (object->userPtr == nullptr)
                  {
                     delete object;
                     object = nullptr;
                  }
               }
               
               // Deal with failure!
               if(!object)
               {
                  vmInternal->printf(0, "%s: Unable to instantiate non-conobject class %s.", frame.codeBlock->getFileLine(ip-1), callArgvS[1]);
                  ip = frame.failJump;
                  break;
               }
               
               // Finally, set currentNewObject to point to the new one.
               frame.currentNewObject = object;
               
               // Deal with the case of a non-SimObject.
               if(!frame.currentNewObject.isValid())
               {
                  vmInternal->printf(0, "%s: Unable to instantiate non-SimObject class %s.", frame.codeBlock->getFileLine(ip-1), callArgvS[1]);
                  delete object;
                  ip = frame.failJump;
                  break;
               }

               if (*tmpVar)
               {
                  // Find it!
                  KorkApi::VMObject *parent = vmInternal->mConfig.iFind.FindObjectByNameFn(vmInternal->mConfig.findUser, tmpVar, nullptr);
                  if (parent)
                  {
                     // Con::printf(" - Parent object found: %s", parent->getClassName());
                     
                     vmInternal->assignFieldsFromTo(parent, frame.currentNewObject);
                  }
                  else
                  {
                     vmInternal->printf(0, "%s: Unable to find parent object %s for %s.", frame.codeBlock->getFileLine(ip-1), tmpVar, callArgvS[1]);
                  }
               }

               if (!klassInfo->iCreate.ProcessArgsFn(vmPublic, frame.currentNewObject->userPtr, objectName, isDataBlock, isInternal, callArgc-3, callArgvS+3))
               {
                  frame.currentNewObject = nullptr;
                  ip = frame.failJump;
                  break;
               }
            }
            
            // Advance the IP past the create info...
            ip += 7;
            break;
         }
            
         case OP_ADD_OBJECT:
         {
            // See OP_SETCURVAR for why we do this.
            frame.nsDocBlockClassLocation = 0;
            
            // Do we place this object at the root?
            bool placeAtRoot = code[ip++];
            
            // Con::printf("Adding object %s", currentNewObject->getName());
            
            // Make sure it wasn't already added, then add it.
            if (!frame.currentNewObject.isValid())
            {
               break;
            }
            
            U32 groupAddId = (U32)evalState.intStack[frame._UINT];
            if(!frame.currentNewObject->klass->iCreate.AddObjectFn(vmPublic, frame.currentNewObject, placeAtRoot, groupAddId))
            {
               // This error is usually caused by failing to call Parent::initPersistFields in the class' initPersistFields().
               vmInternal->printf(0, "%s: Register object failed for object %s of class %s.", frame.codeBlock->getFileLine(ip-2),
                                  frame.currentNewObject->klass->iCreate.GetNameFn(frame.currentNewObject),
                                  frame.currentNewObject->klass->name);

               // NOTE: AddObject may have "unregistered" the object, but since we refcount our objects this is still safe.
               frame.currentNewObject->klass->iCreate.DestroyClassFn(frame.currentNewObject->klass->userPtr, vmPublic, frame.currentNewObject->userPtr);
               frame.currentNewObject = nullptr;
               ip = frame.failJump;
               break;
            }
            
            // store the new object's ID on the stack (overwriting the group/set
            // id, if one was given, otherwise getting pushed)
            if(placeAtRoot)
               evalState.intStack[frame._UINT] = frame.currentNewObject->klass->iCreate.GetIdFn(frame.currentNewObject);
            else
               evalState.intStack[++frame._UINT] = frame.currentNewObject->klass->iCreate.GetIdFn(frame.currentNewObject);
            
            break;
         }
            
         case OP_END_OBJECT:
         {
            // If we're not to be placed at the root, make sure we clean up
            // our group reference.
            bool placeAtRoot = code[ip++];
            if(!placeAtRoot)
               frame._UINT--;
            break;
         }
            
         case OP_FINISH_OBJECT:
         {
            frame._OBJ--;
            evalState.clearCreatedObject(frame._OBJ, frame.currentNewObject, &frame.failJump);
            break;
         }
            
         case OP_JMPIFFNOT:
            if(evalState.floatStack[frame._FLT--])
            {
               ip++;
               break;
            }
            ip = code[ip];
            break;
         case OP_JMPIFNOT:
            if(evalState.intStack[frame._UINT--])
            {
               ip++;
               break;
            }
            ip = code[ip];
            break;
         case OP_JMPIFF:
            if(!evalState.floatStack[frame._FLT--])
            {
               ip++;
               break;
            }
            ip = code[ip];
            break;
         case OP_JMPIF:
            if(!evalState.intStack[frame._UINT--])
            {
               ip ++;
               break;
            }
            ip = code[ip];
            break;
         case OP_JMPIFNOT_NP:
            if(evalState.intStack[frame._UINT])
            {
               frame._UINT--;
               ip++;
               break;
            }
            ip = code[ip];
            break;
         case OP_JMPIF_NP:
            if(!evalState.intStack[frame._UINT])
            {
               frame._UINT--;
               ip++;
               break;
            }
            ip = code[ip];
            break;
         case OP_JMP:
            ip = code[ip];
            break;
            
         // This fixes a bug when not explicitly returning a value.
         case OP_RETURN_VOID:
            evalState.mSTR.setStringValue("");
            // We're falling thru here on purpose.
            
         case OP_RETURN:
         {
            
            if ( frame._ITER > 0 )
            {
               // Get last value
               KorkApi::ConsoleValue returnValueCV = evalState.mSTR.getConsoleValue();

               // Clear iterator state.
               cleanupIterator(evalState, frame);

               // Restore to top of stack
               evalState.mSTR.setConsoleValue(vmInternal, returnValueCV);
            }
            
            goto execFinished;
         }

         case OP_RETURN_FLT:
         
            if( frame._ITER > 0 )
            {
               // Clear iterator state.
               cleanupIterator(evalState, frame);
            }

            evalState.mSTR.setNumberValue( evalState.floatStack[frame._FLT] );
            frame._FLT--;
               
            goto execFinished;

         case OP_RETURN_UINT:
         
            if( frame._ITER > 0 )
            {
               // Clear iterator state.
               cleanupIterator(evalState, frame);
            }

            evalState.mSTR.setUnsignedValue( evalState.intStack[frame._UINT] );
            frame._UINT--;
               
            goto execFinished;

         case OP_CMPEQ:
            evalState.intStack[frame._UINT+1] = bool(evalState.floatStack[frame._FLT] == evalState.floatStack[frame._FLT-1]);
            frame._UINT++;
            frame._FLT -= 2;
            break;
            
         case OP_CMPGR:
            evalState.intStack[frame._UINT+1] = bool(evalState.floatStack[frame._FLT] > evalState.floatStack[frame._FLT-1]);
            frame._UINT++;
            frame._FLT -= 2;
            break;
            
         case OP_CMPGE:
            evalState.intStack[frame._UINT+1] = bool(evalState.floatStack[frame._FLT] >= evalState.floatStack[frame._FLT-1]);
            frame._UINT++;
            frame._FLT -= 2;
            break;
            
         case OP_CMPLT:
            evalState.intStack[frame._UINT+1] = bool(evalState.floatStack[frame._FLT] < evalState.floatStack[frame._FLT-1]);
            frame._UINT++;
            frame._FLT -= 2;
            break;
            
         case OP_CMPLE:
            evalState.intStack[frame._UINT+1] = bool(evalState.floatStack[frame._FLT] <= evalState.floatStack[frame._FLT-1]);
            frame._UINT++;
            frame._FLT -= 2;
            break;
            
         case OP_CMPNE:
            evalState.intStack[frame._UINT+1] = bool(evalState.floatStack[frame._FLT] != evalState.floatStack[frame._FLT-1]);
            frame._UINT++;
            frame._FLT -= 2;
            break;
            
         case OP_XOR:
            evalState.intStack[frame._UINT-1] = evalState.intStack[frame._UINT] ^ evalState.intStack[frame._UINT-1];
            frame._UINT--;
            break;
            
         case OP_MOD:
            if(  evalState.intStack[frame._UINT-1] != 0 )
               evalState.intStack[frame._UINT-1] = evalState.intStack[frame._UINT] % evalState.intStack[frame._UINT-1];
            else
               evalState.intStack[frame._UINT-1] = 0;
            frame._UINT--;
            break;
            
         case OP_BITAND:
            evalState.intStack[frame._UINT-1] = evalState.intStack[frame._UINT] & evalState.intStack[frame._UINT-1];
            frame._UINT--;
            break;
            
         case OP_BITOR:
            evalState.intStack[frame._UINT-1] = evalState.intStack[frame._UINT] | evalState.intStack[frame._UINT-1];
            frame._UINT--;
            break;
            
         case OP_NOT:
            evalState.intStack[frame._UINT] = !evalState.intStack[frame._UINT];
            break;
            
         case OP_NOTF:
            evalState.intStack[frame._UINT+1] = !evalState.floatStack[frame._FLT];
            frame._FLT--;
            frame._UINT++;
            break;
            
         case OP_ONESCOMPLEMENT:
            evalState.intStack[frame._UINT] = ~evalState.intStack[frame._UINT];
            break;
            
         case OP_SHR:
            evalState.intStack[frame._UINT-1] = evalState.intStack[frame._UINT] >> evalState.intStack[frame._UINT-1];
            frame._UINT--;
            break;
            
         case OP_SHL:
            evalState.intStack[frame._UINT-1] = evalState.intStack[frame._UINT] << evalState.intStack[frame._UINT-1];
            frame._UINT--;
            break;
            
         case OP_AND:
            evalState.intStack[frame._UINT-1] = evalState.intStack[frame._UINT] && evalState.intStack[frame._UINT-1];
            frame._UINT--;
            break;
            
         case OP_OR:
            evalState.intStack[frame._UINT-1] = evalState.intStack[frame._UINT] || evalState.intStack[frame._UINT-1];
            frame._UINT--;
            break;
            
         case OP_ADD:
            evalState.floatStack[frame._FLT-1] = evalState.floatStack[frame._FLT] + evalState.floatStack[frame._FLT-1];
            frame._FLT--;
            break;
            
         case OP_SUB:
            evalState.floatStack[frame._FLT-1] = evalState.floatStack[frame._FLT] - evalState.floatStack[frame._FLT-1];
            frame._FLT--;
            break;
            
         case OP_MUL:
            evalState.floatStack[frame._FLT-1] = evalState.floatStack[frame._FLT] * evalState.floatStack[frame._FLT-1];
            frame._FLT--;
            break;
         case OP_DIV:
            evalState.floatStack[frame._FLT-1] = evalState.floatStack[frame._FLT] / evalState.floatStack[frame._FLT-1];
            frame._FLT--;
            break;
         case OP_NEG:
            evalState.floatStack[frame._FLT] = -evalState.floatStack[frame._FLT];
            break;
            
         case OP_SETCURVAR:
            tmpVar = Compiler::CodeToSTE(nullptr, identStrings, code, ip);
            ip += 2;
            
            // If a variable is set, then these must be nullptr. It is necessary
            // to set this here so that the vector parser can appropriately
            // identify whether it's dealing with a vector.
            frame.prevField = nullptr;
            frame.prevObject = nullptr;
            frame.curObject = nullptr;
            
            frame.setCurVarName(tmpVar);
            
            // In order to let docblocks work properly with variables, we have
            // clear the current docblock when we do an assign. This way it
            // won't inappropriately carry forward to following function decls.
            frame.nsDocBlockClassLocation = 0;
            break;
            
         case OP_SETCURVAR_CREATE:
            tmpVar = Compiler::CodeToSTE(nullptr, identStrings, code, ip);
            ip += 2;
            
            // See OP_SETCURVAR
            frame.prevField = nullptr;
            frame.prevObject = nullptr;
            frame.curObject = nullptr;
            
            frame.setCurVarNameCreate(tmpVar);
            
            // See OP_SETCURVAR for why we do this.
            frame.nsDocBlockClassLocation = 0;
            break;
            
         case OP_SETCURVAR_ARRAY:
            tmpVar = vmInternal->internString(evalState.mSTR.getStringValue(), false);
            
            // See OP_SETCURVAR
            frame.prevField = nullptr;
            frame.prevObject = nullptr;
            frame.curObject = nullptr;
            
            frame.setCurVarName(tmpVar);
            
            // See OP_SETCURVAR for why we do this.
            frame.nsDocBlockClassLocation = 0;
            break;
            
         case OP_SETCURVAR_ARRAY_CREATE:
            tmpVar = vmInternal->internString(evalState.mSTR.getStringValue(), false);
            
            // See OP_SETCURVAR
            frame.prevField = nullptr;
            frame.prevObject = nullptr;
            frame.curObject = nullptr;
            
            frame.setCurVarNameCreate(tmpVar);
            
            // See OP_SETCURVAR for why we do this.
            frame.nsDocBlockClassLocation = 0;
            break;
            
         case OP_LOADVAR_UINT:
            evalState.intStack[frame._UINT+1] = frame.getIntVariable();
            frame._UINT++;
            break;
            
         case OP_LOADVAR_FLT:
            evalState.floatStack[frame._FLT+1] = frame.getFloatVariable();
            frame._FLT++;
            break;
            
         case OP_LOADVAR_STR:
            tmpVal = frame.getConsoleVariable();
            evalState.mSTR.setStringValue(vmInternal->valueAsString(tmpVal));
            break;
            
         case OP_LOADVAR_VAR:
            // Sets current source of OP_SAVEVAR_VAR
            frame.copyVar = frame.currentVar;
            break;
            
         case OP_SAVEVAR_UINT:
            frame.setUnsignedVariable((S32)evalState.intStack[frame._UINT]);
            break;
            
         case OP_SAVEVAR_FLT:
            frame.setNumberVariable(evalState.floatStack[frame._FLT]);
            break;
            
         case OP_SAVEVAR_STR:
            frame.setStringVariable(evalState.mSTR.getStringValue());
            break;
            
         case OP_SAVEVAR_VAR:
            // this basically handles %var1 = %var2
            frame.setCopyVariable();
            break;
            
         case OP_SETCUROBJECT:
            // Save the previous object for parsing vector fields.
            frame.prevObject = frame.curObject;
            tmpVal = evalState.mSTR.getConsoleValue();
            {
               const char* findPath = vmInternal->valueAsString(tmpVal);

               // Sim::findObject will sometimes find valid objects from
               // multi-component strings. This makes sure that doesn't
               // happen.
               if (tmpVal.isString())
               {
                  const char* chkValue = findPath;
                  for( const char* check = chkValue; *check; check++ )
                  {
                     if( *check == ' ' )
                     {
                        findPath = "";
                        break;
                     }
                  }
               }

               frame.curObject = vmInternal->mConfig.iFind.FindObjectByPathFn(vmInternal->mConfig.findUser, findPath);
            }
            break;
            
         case OP_SETCUROBJECT_INTERNAL:
            ++ip; // To skip the recurse flag if the object wasnt found
            if (frame.curObject)
            {
               StringTableEntry intName = vmInternal->internString(evalState.mSTR.getStringValue(), false);
               bool recurse = code[ip-1];
               KorkApi::VMObject* obj = vmInternal->mConfig.iFind.FindObjectByInternalNameFn(vmInternal->mConfig.findUser, intName, recurse, frame.curObject);
               evalState.intStack[frame._UINT+1] = obj ? obj->klass->iCreate.GetIdFn(obj) : 0;
            }
            else
            {
               intStack[frame._UINT+1] = 0;
            }
            frame._UINT++;
            break;
            
         case OP_SETCUROBJECT_NEW:
            frame.curObject = frame.currentNewObject;
            break;
            
         case OP_SETCURFIELD:
            // Save the previous field for parsing vector fields.
            frame.prevField = frame.curField;
            strcpy( frame.prevFieldArray, frame.curFieldArray );
            frame.curField = Compiler::CodeToSTE(nullptr, identStrings, code, ip);
            frame.curFieldArray[0] = 0;
            ip += 2;
            break;
            
         case OP_SETCURFIELD_ARRAY:
            strcpy(frame.curFieldArray, evalState.mSTR.getStringValue());
            break;

         case OP_SETCURFIELD_TYPE:
         {
            U32 typeId = code[ip++];
            
            if(frame.curObject)
            {
               typeId = frame.codeBlock->getRealTypeID((U16)typeId);
               frame.curObject->klass->iCustomFields.SetCustomFieldType(vmPublic, frame.curObject, frame.curField, frame.curFieldArray, typeId);
            }
            
            break;
         }
            
         case OP_LOADFIELD_UINT:
            if (frame.curObject)
            {
               KorkApi::ConsoleValue retValue = vmInternal->getObjectField(frame.curObject, frame.curField, frame.curFieldArray, KorkApi::ConsoleValue::TypeInternalUnsigned, KorkApi::ConsoleValue::ZoneExternal);
               evalState.intStack[frame._UINT+1] = vmInternal->valueAsInt(retValue);
            }
            else
            {
               // The field is not being retrieved from an object. Maybe it's
               // a special accessor?
               
               //getFieldComponent( prevObject, prevField, prevFieldArray, curField, valBuffer, VAL_BUFFER_SIZE );
               evalState.intStack[frame._UINT+1] = 0;//atoi( valBuffer );
            }
            frame._UINT++;
            break;
            
         case OP_LOADFIELD_FLT:
            if (frame.curObject)
            {
               KorkApi::ConsoleValue retValue =  vmInternal->getObjectField(frame.curObject, frame.curField, frame.curFieldArray, KorkApi::ConsoleValue::TypeInternalNumber, KorkApi::ConsoleValue::ZoneExternal);
               evalState.floatStack[frame._FLT+1] = vmInternal->valueAsFloat(retValue);
            }
            else
            {
               // The field is not being retrieved from an object. Maybe it's
               // a special accessor?
               //getFieldComponent( prevObject, prevField, prevFieldArray, curField, valBuffer, VAL_BUFFER_SIZE );
               evalState.floatStack[frame._FLT+1] = 0.0f;//atof( valBuffer );
            }
            frame._FLT++;
            break;
            
         case OP_LOADFIELD_STR:
            if (frame.curObject)
            {
               KorkApi::ConsoleValue retValue =  vmInternal->getObjectField(frame.curObject, frame.curField, frame.curFieldArray, KorkApi::ConsoleValue::TypeInternalString, KorkApi::ConsoleValue::ZoneExternal);
               evalState.mSTR.setStringValue(vmInternal->valueAsString(retValue));
            }
            else
            {
               // The field is not being retrieved from an object. Maybe it's
               // a special accessor?
               //getFieldComponent( prevObject, prevField, prevFieldArray, curField, valBuffer, VAL_BUFFER_SIZE );
               evalState.mSTR.setStringValue( ""); //valBuffer );
            }
            break;
            
         case OP_SAVEFIELD_UINT:
            evalState.mSTR.setUnsignedValue((U32)evalState.intStack[frame._UINT]);
            if (frame.curObject)
            {
               KorkApi::ConsoleValue cv = evalState.mSTR.getConsoleValue();
               vmInternal->setObjectField(frame.curObject, frame.curField, frame.curFieldArray, cv);
            }
            else
            {
               // The field is not being set on an object. Maybe it's
               // a special accessor?
               //setFieldComponent( prevObject, prevField, prevFieldArray, curField );
               frame.prevObject = nullptr;
            }
            break;
            
         case OP_SAVEFIELD_FLT:
            evalState.mSTR.setNumberValue(evalState.floatStack[frame._FLT]);
            if (frame.curObject)
            {
               KorkApi::ConsoleValue cv = evalState.mSTR.getConsoleValue();
               vmInternal->setObjectField(frame.curObject, frame.curField, frame.curFieldArray, cv);
            }
            else
            {
               // The field is not being set on an object. Maybe it's
               // a special accessor?
               //setFieldComponent( prevObject, prevField, prevFieldArray, curField );
               frame.prevObject = nullptr;
            }
            break;
            
         case OP_SAVEFIELD_STR:
            if (frame.curObject)
            {
               KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeString(evalState.mSTR.getStringValue());
               vmInternal->setObjectField(frame.curObject, frame.curField, frame.curFieldArray, cv);
            }
            else
            {
               // The field is not being set on an object. Maybe it's
               // a special accessor?
               //setFieldComponent( prevObject, prevField, prevFieldArray, curField );
               frame.prevObject = nullptr;
            }
            break;
            
         case OP_STR_TO_UINT:
            evalState.intStack[frame._UINT+1] = evalState.mSTR.getIntValue();
            frame._UINT++;
            break;
            
         case OP_STR_TO_FLT:
            evalState.floatStack[frame._FLT+1] = evalState.mSTR.getFloatValue();
            frame._FLT++;
            break;
            
         case OP_STR_TO_NONE:
            // This exists simply to deal with certain typecast situations.
            break;
            
         case OP_FLT_TO_UINT:
            evalState.intStack[frame._UINT+1] = (S64)evalState.floatStack[frame._FLT];
            frame._FLT--;
            frame._UINT++;
            break;
            
         case OP_FLT_TO_STR:
            evalState.mSTR.setStringFloatValue(evalState.floatStack[frame._FLT]);
            frame._FLT--;
            break;
            
         case OP_FLT_TO_NONE:
            frame._FLT--;
            break;
            
         case OP_UINT_TO_FLT:
            evalState.floatStack[frame._FLT+1] = (F64)evalState.intStack[frame._UINT];
            frame._UINT--;
            frame._FLT++;
            break;
            
         case OP_UINT_TO_STR:
            evalState.mSTR.setStringIntValue((U32)evalState.intStack[frame._UINT]);
            frame._UINT--;
            break;
            
         case OP_UINT_TO_NONE:
            frame._UINT--;
            break;
         
         case OP_COPYVAR_TO_NONE:
            frame.copyVar.var = nullptr;
            break;
            
         case OP_LOADIMMED_UINT:
            evalState.intStack[frame._UINT+1] = code[ip++];
            frame._UINT++;
            break;
            
         case OP_LOADIMMED_FLT:
            evalState.floatStack[frame._FLT+1] = frame.curFloatTable[code[ip]];
            ip++;
            frame._FLT++;
            break;
         case OP_TAG_TO_STR:
         {
            // NOTE: before the string in the codeblock was modified. Instead we pay the lookup cost again and change it to
            // a typed value.
            
            S32 typeId = vmInternal->lookupTypeId(vmInternal->internString("TypeTaggedString", false));
            if (typeId != -1)
            {
               U32 id = vmInternal->mConfig.addTagFn(frame.curStringTable + code[ip], vmInternal->mConfig.addTagUser);
               KorkApi::ConsoleValue values[2];
               values[0] = KorkApi::ConsoleValue::makeUnsigned(id);
               values[1] = KorkApi::ConsoleValue::makeString(frame.curStringTable + code[ip]);
               
               KorkApi::TypeStorageInterface inputStorage = KorkApi::CreateRegisterStorageFromArgs(vmInternal, 2, values);
               KorkApi::TypeStorageInterface outputStorage = KorkApi::CreateExprStringStackStorage(vmInternal, mSTR, strlen(frame.curStringTable + code[ip])+9, typeId);
               
               vmInternal->mTypes[typeId].iFuncs.CastValueFn(vmInternal->mTypes[typeId].userPtr,
                                                             vmPublic,
                                                             &inputStorage,
                                                             &outputStorage,
                                                             nullptr,
                                                             0,
                                                             typeId);
            }
            else
            {
               evalState.mSTR.setStringValue(frame.curStringTable + code[ip]);
            }
         }
            break;
         case OP_LOADIMMED_STR:
            evalState.mSTR.setStringValue(frame.curStringTable + code[ip++]);
            break;
            
         case OP_DOCBLOCK_STR:
         {
            // If the first word of the doc is '\class' or '@class', then this
            // is a namespace doc block, otherwise it is a function doc block.
            const char* docblock = frame.curStringTable + code[ip++];
            
            const char* sansClass = strstr( docblock, "@class" );
            if (!sansClass)
               sansClass = strstr( docblock, "\\class" );
            
            if (sansClass)
            {
               // Don't save the class declaration. Scan past the 'class'
               // keyword and up to the first whitespace.
               sansClass += 7;

               // Mark start of class name.
               const char* classStart = sansClass;
               U32 classLen = 0;

               // Read up to first space, newline, or string end.
               while ((*sansClass != ' ') &&
                      (*sansClass != '\n') &&
                      *sansClass)
               {
                  ++classLen;
                  ++sansClass;
               }

               // Store pointer and length instead of copying into a buffer.
               frame.nsDocBlockClassOffset = (U32)(classStart - frame.curStringTable);
               frame.nsDocBlockClassNameLength   = classLen;

               ++sansClass;
               
               frame.nsDocBlockOffset = (U32)(sansClass - frame.curStringTable);
            }
            else
            {
               frame.nsDocBlockClassOffset = 0;
               frame.nsDocBlockClassNameLength = 0;
               frame.nsDocBlockOffset = (U32)(docblock - frame.curStringTable);
            }

            if (frame.curStringTable == frame.codeBlock->functionStrings)
            {
               frame.nsDocBlockClassLocation = 2;
            }
            else
            {
               frame.nsDocBlockClassLocation = 1;
            }
         }
            
            break;
            
         case OP_LOADIMMED_IDENT:
            evalState.mSTR.setStringValue(Compiler::CodeToSTE(nullptr, identStrings, code, ip));
            ip += 2;
            break;
            
         case OP_CALLFUNC_RESOLVE:
         {
            // This deals with a function that is potentially living in a namespace.
            
            U32 funcSlotIndex = (code[ip+4] >> 16) & 0xFFFF;
            tmpNsEntry = (Namespace::Entry*)frame.codeBlock->getNSEntry(funcSlotIndex);
            
            if (tmpNsEntry == nullptr || funcSlotIndex == 0)
            {
               tmpFnNamespace = Compiler::CodeToSTE(nullptr, identStrings, code, ip+2);
               tmpFnName      = Compiler::CodeToSTE(nullptr, identStrings, code, ip);
               
               tmpNs = vmInternal->mNSState.find(tmpFnNamespace);
               tmpNsEntry = tmpNs->lookup(tmpFnName);
               
               frame.codeBlock->setNSEntry(funcSlotIndex, tmpNsEntry);
            }
         }
         
         case OP_CALLFUNC:
         {
            // This routingId is set when we query the object as to whether
            // it handles this method.  It is set to an enum from the table
            // above indicating whether it handles it on a component it owns
            // or just on the object.
            S32 routingId = 0;
            
            tmpFnName = Compiler::CodeToSTE(nullptr, identStrings, code, ip);
            
            //if this is called from inside a function, append the ip and codeptr
            if (!evalState.vmFrames.empty())
            {
               evalState.vmFrames.back()->codeBlock = frame.codeBlock;
               evalState.vmFrames.back()->ip = ip - 1;
            }
            
            frame.lastCallType = code[ip+4] & 0xFFFF;
            
            ip += 5;
            frame.ip = ip; // sync ip with frame
            evalState.mSTR.getArgcArgv(tmpFnName, &callArgc, &callArgv);
            
            if(frame.lastCallType == FuncCallExprNode::FunctionCall)
            {
               // Lookup function
               U32 funcSlotIndex = (code[ip-5+4] >> 16) & 0xFFFF;
               tmpNsEntry = (Namespace::Entry*)frame.codeBlock->getNSEntry(funcSlotIndex);
               tmpNs = nullptr;
            }
            else if(frame.lastCallType == FuncCallExprNode::MethodCall)
            {
               frame.saveObject = frame.thisObject;
               const char* objName = vmInternal->valueAsString(callArgv[1]);
               frame.thisObject = vmInternal->mConfig.iFind.FindObjectByPathFn(vmInternal->mConfig.findUser, objName);
               
               if(!frame.thisObject)
               {
                  frame.thisObject = 0;
                  vmInternal->printf(0,"%s: Unable to find object: '%s' attempting to call function '%s'", frame.codeBlock->getFileLine(ip-6), objName, tmpFnName);
                  evalState.mSTR.popFrame();
                  frame.pushStringStackCount--;
                  evalState.mSTR.setStringValue("");
                  break;
               }
               
               tmpNs = frame.thisObject->ns;
               if(tmpNs)
                  tmpNsEntry = tmpNs->lookup(tmpFnName);
               else
                  tmpNsEntry = nullptr;
            }
            else // it's a ParentCall
            {
               if(frame.scopeNamespace)
               {
                  tmpNs = frame.scopeNamespace->mParent;
                  if(tmpNs)
                     tmpNsEntry = tmpNs->lookup(tmpFnName);
                  else
                     tmpNsEntry = nullptr;
               }
               else
               {
                  tmpNs = nullptr;
                  tmpNsEntry = nullptr;
               }
            }
            
            if(!tmpNsEntry || frame.noCalls)
            {
               if(!frame.noCalls)
               {
                  vmInternal->printf(0,"%s: Unknown command %s.", frame.codeBlock->getFileLine(ip-4), tmpFnName);
                  if(frame.lastCallType == FuncCallExprNode::MethodCall)
                  {
                     const char* objName = frame.thisObject.obj->klass->iCreate.GetNameFn(frame.thisObject.obj);
                     KorkApi::SimObjectId ident = frame.thisObject.obj->klass->iCreate.GetIdFn(frame.thisObject.obj);
                     vmInternal->printf(0, "  Object %s(%d) %s",
                                        objName ? objName : "",
                                        ident, getNamespaceList(frame.thisObject->ns) );
                  }
               }
               evalState.mSTR.popFrame();
               frame.pushStringStackCount--;
               evalState.mSTR.setStringValue("");
               evalState.mSTR.setStringValue("");
               break;
            }
            
            AssertFatal(frame.pushStringStackCount != 0, "No PUSH_FRAME before function call");
            
            if(tmpNsEntry->mType == Namespace::Entry::ScriptFunctionType)
            {
               KorkApi::ConsoleValue ret = KorkApi::ConsoleValue();
               
               // NOTE: script opcodes should have pushed a frame thus the need to pop it either
               // here or in execFinished
               
               if(tmpNsEntry->mFunctionOffset)
               {
                  frame.pushStringStackCount--;
                  ConsoleFrame* newFrame = tmpNsEntry->mCode->beginExec(evalState,
                                                                           tmpNsEntry->mFunctionOffset,
                                                                           tmpFnName,
                                                                           tmpNsEntry->mNamespace,
                                                                           callArgc,
                                                                           callArgv,
                                                                           false,
                                                                           false,
                                                                           tmpNsEntry->mPackage);
                  
                  // NOTE: "frame" is now invalidated
                  if (newFrame)
                  {
                     // OK: what we want to do here is transfer the pushed script frame to the new frame
                     newFrame->pushStringStackCount++;
                     loopFrameSetup = true;
                     goto execFinished;
                  }
               }
               
               // No body fallthrough case
               evalState.mSTR.popFrame();
               frame.pushStringStackCount--;
               evalState.mSTR.setStringValue("");
            }
            else
            {
               if((tmpNsEntry->mMinArgs && S32(callArgc) < tmpNsEntry->mMinArgs) || (tmpNsEntry->mMaxArgs && S32(callArgc) > tmpNsEntry->mMaxArgs))
               {
                  const char* nsName = tmpNs? tmpNs->mName: "";
                  vmInternal->printf(0, "%s: %s::%s - wrong number of arguments.", frame.codeBlock->getFileLine(ip-4), nsName, tmpFnName);
                  vmInternal->printf(0, "%s: usage: %s", frame.codeBlock->getFileLine(ip-4), tmpNsEntry->getUsage());
                  evalState.mSTR.popFrame();
                  frame.pushStringStackCount--;
                  evalState.mSTR.setStringValue("");
               }
               else
               {
                  if (tmpNsEntry->mType != Namespace::Entry::ValueCallbackType)
                  {
                     // Need to convert to strings for old callbacks
                     //printf("Converting %i argv calling %s\n", callArgc, fnName);
                     evalState.mSTR.convertArgv(vmInternal, callArgc, &callArgvS);
                  }
                  
                  // Handle calling
                  // NOTE regarding yielding:
                  //   Yielded value should match the type of the function in this case.
                  
                  switch(tmpNsEntry->mType)
                  {
                     case Namespace::Entry::StringCallbackType:
                     {
                        frame.inNativeFunction = true;
                        const char *ret = tmpNsEntry->cb.mStringCallbackFunc(safeObjectUserPtr(frame.thisObject), tmpNsEntry->mUserPtr, callArgc, callArgvS);
                        if (mState != KorkApi::FiberRunResult::RUNNING)
                        {
                           vmInternal->printf(0,"String function yielded, ignoring result");
                           mLastFiberValue = KorkApi::ConsoleValue();
                        }
                        else
                        {
                           mLastFiberValue = KorkApi::ConsoleValue::makeString(ret); // NOTE: none of these should yield
                        }
                        
                        loopFrameSetup = true;
                        goto execFinished;
                     }
                     case Namespace::Entry::IntCallbackType:
                     {
                        frame.inNativeFunction = true;
                        S32 result = tmpNsEntry->cb.mIntCallbackFunc(safeObjectUserPtr(frame.thisObject), tmpNsEntry->mUserPtr, callArgc, callArgvS);
                        mLastFiberValue = KorkApi::ConsoleValue::makeNumber(result);
                        loopFrameSetup = true;
                        goto execFinished;
                     }
                     case Namespace::Entry::FloatCallbackType:
                     {
                        frame.inNativeFunction = true;
                        F64 result = tmpNsEntry->cb.mFloatCallbackFunc(safeObjectUserPtr(frame.thisObject), tmpNsEntry->mUserPtr, callArgc, callArgvS);
                        mLastFiberValue = KorkApi::ConsoleValue::makeNumber(result);
                        loopFrameSetup = true;
                        goto execFinished;
                     }
                     case Namespace::Entry::VoidCallbackType:
                     {
                        frame.inNativeFunction = true;
                        tmpNsEntry->cb.mVoidCallbackFunc(safeObjectUserPtr(frame.thisObject), tmpNsEntry->mUserPtr, callArgc, callArgvS);
                        mLastFiberValue = KorkApi::ConsoleValue();
                        loopFrameSetup = true;
                        goto execFinished;
                     }
                     case Namespace::Entry::BoolCallbackType:
                     {
                        frame.inNativeFunction = true;
                        bool result = tmpNsEntry->cb.mBoolCallbackFunc(safeObjectUserPtr(frame.thisObject), tmpNsEntry->mUserPtr, callArgc, callArgvS);
                        mLastFiberValue = KorkApi::ConsoleValue::makeUnsigned(result);
                        loopFrameSetup = true;
                        goto execFinished;
                     }
                     case Namespace::Entry::ValueCallbackType:
                     {
                        frame.inNativeFunction = true;
                        mLastFiberValue = tmpNsEntry->cb.mValueCallbackFunc(safeObjectUserPtr(frame.thisObject), tmpNsEntry->mUserPtr, callArgc, callArgv);
                        loopFrameSetup = true;
                        goto execFinished;
                     }
                  }
                  
                  // NOTE: Code previously set the stack based on the next opcode and type of the function; instead
                  // we mark the frame as being in a native function, then when the loop restarts the opcode checks are handled there.
                  // We do this even if the function didn't suspend the fiber to keep behavior consistent
                  
                  if(frame.lastCallType == FuncCallExprNode::MethodCall) // NOTE: this is permissible here since nothing checks thisObject from user code.
                     frame.thisObject = frame.saveObject;

               }
            }
            
            if(frame.lastCallType == FuncCallExprNode::MethodCall)
               frame.thisObject = frame.saveObject;
            break;
         }
         case OP_ADVANCE_STR:
            evalState.mSTR.advance();
            break;
         case OP_ADVANCE_STR_APPENDCHAR:
            evalState.mSTR.advanceChar(code[ip++]);
            break;
            
         case OP_ADVANCE_STR_COMMA:
            evalState.mSTR.advanceChar('_');
            break;
            
         case OP_ADVANCE_STR_NUL:
            evalState.mSTR.advanceChar(0);
            break;
            
         case OP_REWIND_STR:
            evalState.mSTR.rewind();
            break;
            
         case OP_TERMINATE_REWIND_STR:
            evalState.mSTR.rewindTerminate();
            break;
            
         case OP_COMPARE_STR:
            evalState.intStack[++frame._UINT] = evalState.mSTR.compare();
            break;
         case OP_PUSH:
            evalState.mSTR.push();
            break;
            
         case OP_PUSH_UINT:
            // OPframe._UINT_TO_STR, OP_PUSH
            evalState.mSTR.setUnsignedValue((U32)evalState.intStack[frame._UINT]);
            frame._UINT--;
            evalState.mSTR.push();
            break;
         case OP_PUSH_FLT:
            // OPframe._FLT_TO_STR, OP_PUSH
            evalState.mSTR.setNumberValue(evalState.floatStack[frame._FLT]);
            frame._FLT--;
            evalState.mSTR.push();
            break;
         case OP_PUSH_VAR:
            // OP_LOADVAR_STR, OP_PUSH
            tmpVal = frame.getConsoleVariable();
            evalState.mSTR.setConsoleValue(vmInternal, tmpVal);
            evalState.mSTR.push();
            break;

         case OP_PUSH_FRAME:
            evalState.mSTR.pushFrame();
            frame.pushStringStackCount++;
            break;

         case OP_ASSERT:
         {
            if( !evalState.intStack[frame._UINT--] )
            {
               const char *message = frame.curStringTable + code[ip];

               U32 breakLine, inst;
               frame.codeBlock->findBreakLine( ip - 1, breakLine, inst );

               if ( PlatformAssert::processAssert( PlatformAssert::Fatal,
                                                   frame.codeBlock->name ? frame.codeBlock->name : "eval",
                                                   breakLine,
                                                   message ) )
               {
                  if ( vmInternal->mTelDebugger && vmInternal->mTelDebugger->isConnected() && breakLine > 0 )
                  {
                     vmInternal->mTelDebugger->breakProcess();
                  }
                  else
                     Platform::debugBreak();
               }
            }

            ip++;
            break;
         }

         case OP_BREAK:
         {
            //append the ip and codeptr before managing the breakpoint!
            AssertFatal( !evalState.vmFrames.empty(), "Empty eval stack on break!");
            evalState.vmFrames.back()->codeBlock = frame.codeBlock;
            evalState.vmFrames.back()->ip = ip - 1;
            
            U32 breakLine;
            U32 inst = 0;
            frame.codeBlock->findBreakLine(ip-1, breakLine, inst);
            if(!breakLine)
               goto breakContinue;
            vmInternal->mTelDebugger->executionStopped(frame.codeBlock, breakLine);
            
            goto breakContinue;
         }
         
         case OP_ITER_BEGIN_STR:
         {
            evalState.iterStack[ frame._ITER ].mIsStringIter = true;
            /* fallthrough */
         }
         
         case OP_ITER_BEGIN:
         {
            StringTableEntry varName = Compiler::CodeToSTE(nullptr, identStrings, code, ip);
            U32 failIp = code[ ip + 2 ];
            
            IterStackRecord& iter = evalState.iterStack[ frame._ITER ];
            
            iter.mVariable = evalState.getCurrentFrame().dictionary.add( varName );
            
            if (iter.mIsStringIter)
            {
               iter.mData = evalState.mSTR.getConsoleValue();
               iter.mIndex = 0;
               frame.curIterObject = nullptr;
            }
            else
            {
               // Look up the object.
               iter.mData = evalState.mSTR.getConsoleValue();
               frame.curIterObject = vmInternal->mConfig.iFind.FindObjectByPathFn(vmInternal->mConfig.findUser, vmInternal->valueAsString(iter.mData));
               
               if (!frame.curIterObject.isValid())
               {
                  iter.mData = KorkApi::ConsoleValue();
                  vmInternal->printf(0, "No SimSet object '%s'", evalState.mSTR.getStringValue());
                  vmInternal->printf(0, "Did you mean to use 'foreach$' instead of 'foreach'?");
                  ip = failIp;
                  continue;
               }
               
               // Set up.

               AssertFatal(iter.mData.cvalue == 0, "Should be nullptr");

               iter.mIndex = 0;
            }
            
            frame._ITER ++;
            
            evalState.mSTR.push();
            
            ip += 3;
            break;
         }
         
         case OP_ITER:
         {
            U32 breakIp = code[ ip ];
            IterStackRecord& iter = evalState.iterStack[ frame._ITER - 1 ];
            
            if (iter.mIsStringIter && 
               iter.mData.isString() &&
                iter.mData.cvalue)
            {
               const char* str = (const char*)iter.mData.evaluatePtr(vmInternal->mAllocBase);
                              
               U32 startIndex = iter.mIndex;
               U32 endIndex = startIndex;
               
               // Break if at end.

               if( !str[ startIndex ] )
               {
                  ip = breakIp;
                  continue;
               }

               // Find right end of current component.
               
               if( !isspace( str[ endIndex ] ) )
                  do ++ endIndex;
                  while( str[ endIndex ] && !isspace( str[ endIndex ] ) );
                  
               // Extract component.
                  
               if( endIndex != startIndex )
               {
                  char savedChar = str[ endIndex ];
                  const_cast< char* >( str )[ endIndex ] = '\0'; // We are on the string stack so this is okay.
                  frame.dictionary.setEntryStringValue(iter.mVariable, &str[ startIndex ] );
                  const_cast< char* >( str )[ endIndex ] = savedChar;
               }
               else
               {
                  frame.dictionary.setEntryStringValue( iter.mVariable, "" );
               }
               
               // Skip separator.
               if( str[ endIndex ] != '\0' )
                  ++ endIndex;
               
               iter.mIndex = endIndex;
            }
            else if (!iter.mIsStringIter && 
               frame.curIterObject.isValid())
            {
               U32 index = iter.mIndex;
               KorkApi::VMObject* set = frame.curIterObject;
               
               if( index >= set->klass->iEnum.GetSize(set) )
               {
                  if (set)
                  {
                     frame.curIterObject = nullptr;
                  }
                  ip = breakIp;
                  continue;
               }
               
               KorkApi::VMObject* atObject = set->klass->iEnum.GetObjectAtIndex(set, index);
               frame.dictionary.setEntryUnsignedValue(iter.mVariable, atObject ? atObject->klass->iCreate.GetIdFn(atObject) : 0);
               iter.mIndex = index + 1;
            }
            else
            {
               // Problem: break out
               iter.mData = KorkApi::ConsoleValue();
               frame.curIterObject = nullptr;
               ip = breakIp;
            }
            
            ++ ip;
            break;
         }
         
         case OP_ITER_END:
         {
            -- frame._ITER;
            IterStackRecord& iter = evalState.iterStack[frame._ITER]; // iter we are ending
            frame.curIterObject = nullptr;

            if (iter.mIsStringIter)
            {
               evalState.mSTR.rewind();
               iter.mIsStringIter = false;
            }

            // Restore prev iter if valid
            if (frame._ITER > 0)
            {
               IterStackRecord& prevIter = evalState.iterStack[frame._ITER];
               if (!prevIter.mIsStringIter)
               {
                  frame.curIterObject = vmInternal->mConfig.iFind.FindObjectByPathFn(vmInternal->mConfig.findUser, vmInternal->valueAsString(iter.mData));
               }
            }

            break;
         }
         
         case OP_PUSH_TRY:
         case OP_PUSH_TRY_STACK:
            {
               // !! IMPORTANT: should be only 1 of these invoked per try case. If multiple cases are needed,
               // emit a bunch of conditional checks to the jump address to go to the correct catch block.
               // Main block should always end with OP_POP_TRY; the vm will pop the actual try stack to the correct
               // place when handling the main catch case.
               TryItem item;
               item.mask = instruction == OP_PUSH_TRY_STACK ? (U32)evalState.intStack[frame._UINT--] : code[ip++];
               item.frameDepth = vmFrames.size()-1;
               item.ip = code[ip++];
               evalState.tryStack[ frame._TRY++ ] = item;
            }
            break;
            
         case OP_POP_TRY:
            if (frame._TRY > 0)
            {
               frame._TRY--;
            }
            break;
         
         case OP_THROW:
         {
            // NOTE: in order to handle native function throws, we tack onto the loop setup
            U32 throwMask = code[ip++];
            frame.ip = ip;
            lastThrow = throwMask;
            loopFrameSetup = true;
            goto execFinished;
         }

         // NOTE: this opcode is just so we avoid using vars in try blocks
         case OP_DUP_UINT:
         {
            evalState.intStack[frame._UINT+1] = evalState.intStack[frame._UINT];
            frame._UINT++;
         }
         break;

         case OP_SAVEVAR_MULTIPLE:
         {
            // This is like OP_CALLFUNC
            evalState.mSTR.getArgcArgv(nullptr, &callArgc, &callArgv);

            frame.setConsoleValues(callArgc-1, callArgv+1);
            
            evalState.mSTR.popFrame();
            frame.pushStringStackCount--;

            lastTypeId = -1;
         }
            break;
         
         case OP_SETCURFIELD_NONE:
            frame.prevField = frame.curField;
            strcpy( frame.prevFieldArray, frame.curFieldArray );
            frame.curField = vmInternal->mEmptyString;
            frame.curFieldArray[0] = 0;
            break;
            
         case OP_SETVAR_FROM_COPY:
         {
            frame.currentVar = frame.copyVar;
            break;
         }
         
         case OP_SAVEFIELD_MULTIPLE:
         {
            // This is like OP_CALLFUNC
            evalState.mSTR.getArgcArgv(nullptr, &callArgc, &callArgv);
            vmInternal->setObjectFieldTuple(frame.curObject, frame.curField, frame.curFieldArray, callArgc-1, callArgv+1);
            
            evalState.mSTR.popFrame();
            evalState.mSTR.setStringValue(""); // clear stack in case original value is garbage
            frame.pushStringStackCount--;
         }
            break;
            
         case OP_PUSH_TYPED:
            evalState.mSTR.push();
            break;
            
         case OP_TYPED_TO_STR:
            evalState.mSTR.setStringValue(vmInternal->valueAsString(evalState.mSTR.getConsoleValue()));
            break;
            
         case OP_TYPED_TO_FLT:
            evalState.floatStack[++frame._FLT] = vmInternal->valueAsFloat(evalState.mSTR.getConsoleValue());
            break;
            
         case OP_TYPED_TO_UINT:
            evalState.intStack[++frame._UINT] = vmInternal->valueAsInt(evalState.mSTR.getConsoleValue());
            break;
         
         case OP_TYPED_TO_NONE:
            // This exists simply to deal with certain typecast situations.
            break;
         
         case OP_TYPED_OP:
            evalState.mSTR.performOp(code[ip++], vmPublic, &vmInternal->mTypes[0]);
            break;
            
         case OP_TYPED_OP_REVERSE:
            evalState.mSTR.performOpReverse(code[ip++], vmPublic, &vmInternal->mTypes[0]);
            break;
            
         case OP_TYPED_UNARY_OP:
            evalState.mSTR.performUnaryOp(code[ip++], vmPublic, &vmInternal->mTypes[0]);
            break;
            
         case OP_LOADFIELD_VAR:
            // field -> var
            tmpVal = vmInternal->getObjectField(frame.curObject, frame.curField, frame.curFieldArray, KorkApi::ConsoleValue::TypeInternalUnsigned, KorkApi::ConsoleValue::ZoneFunc);
            frame.setConsoleValue(tmpVal);
            break;
         case OP_SAVEFIELD_VAR:
            // var -> field
            
            if (frame.curObject)
            {
               KorkApi::ConsoleValue cv = frame.getConsoleVariable();
               vmInternal->setObjectField(frame.curObject, frame.curField, frame.curFieldArray, cv);
            }
            else
            {
               // The field is not being set on an object. Maybe it's
               // a special accessor?
               //setFieldComponent( prevObject, prevField, prevFieldArray, curField );
               frame.prevObject = nullptr;
            }
            
            break;
         case OP_LOADVAR_TYPED:
            tmpVal = frame.getConsoleVariable();
            evalState.mSTR.setConsoleValue(vmInternal, tmpVal);
            break;
         case OP_LOADVAR_TYPED_REF:
            // TODO: copy ref?
            tmpVal = frame.getConsoleVariable();
            evalState.mSTR.setConsoleValue(vmInternal, tmpVal);
            break;
         case OP_LOADFIELD_TYPED:
            // field -> typed
            tmpVal = vmInternal->getObjectField(frame.curObject, frame.curField, frame.curFieldArray, KorkApi::ConsoleValue::TypeInternalUnsigned, KorkApi::ConsoleValue::ZoneFunc);
            evalState.mSTR.setConsoleValue(vmInternal, tmpVal);
            break;
         case OP_SAVEVAR_TYPED:
            // typed -> var
            // (use OP_SETCURVAR_TYPE to set the type here)
            frame.setConsoleValue(evalState.mSTR.getConsoleValue());
            break;
         case OP_SAVEFIELD_TYPED:
            // typed -> field
            // (use OP_SETCURFIELD_TYPE to set the field type if dynamic)

            if (frame.curObject)
            {
   
               KorkApi::ConsoleValue cv = evalState.mSTR.getConsoleValue();
               vmInternal->setObjectField(frame.curObject, frame.curField, frame.curFieldArray, cv);
            }
            else
            {
               // The field is not being set on an object. Maybe it's
               // a special accessor?
               //setFieldComponent( prevObject, prevField, prevFieldArray, curField );
               frame.prevObject = nullptr;
            }

            break;
         case OP_STR_TO_TYPED:
            if (frame.dynTypeId != 0)
            {
               KorkApi::ConsoleValue cv = evalState.mSTR.getConsoleValue();

               KorkApi::TypeStorageInterface outputStorage = KorkApi::CreateExprStringStackStorage(vmInternal,
                                                                                       mSTR,
                                                                                       0,
                                                                                       frame.dynTypeId);

               KorkApi::TypeStorageInterface inputStorage = KorkApi::CreateRegisterStorageFromArg(vmInternal, cv);
               
               // NOTE: types should set head of stack to value if data pointer is nullptr in this case
               vmInternal->mTypes[frame.dynTypeId].iFuncs.CastValueFn(vmInternal->mTypes[frame.dynTypeId].userPtr,
                                                                   vmPublic,
                                                                   &inputStorage,
                                                                   &outputStorage,
                                                                   nullptr,
                                                                   0,
                                                                   frame.dynTypeId);
            }
            break;
         case OP_FLT_TO_TYPED:
            if (frame.dynTypeId != 0)
            {
               KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeNumber(evalState.floatStack[frame._FLT--]);

               KorkApi::TypeStorageInterface outputStorage = KorkApi::CreateExprStringStackStorage(vmInternal,
                                                                                       mSTR,
                                                                                       0,
                                                                                       frame.dynTypeId);

               KorkApi::TypeStorageInterface inputStorage = KorkApi::CreateRegisterStorageFromArg(vmInternal, cv);
               
               // NOTE: types should set head of stack to value if data pointer is nullptr in this case
               vmInternal->mTypes[frame.dynTypeId].iFuncs.CastValueFn(vmInternal->mTypes[frame.dynTypeId].userPtr,
                                                                   vmPublic,
                                                                   &inputStorage,
                                                                   &outputStorage,
                                                                   nullptr,
                                                                   0,
                                                                   frame.dynTypeId);
            }
            else
            {
               KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeNumber(evalState.floatStack[frame._FLT--]);
               evalState.mSTR.setConsoleValue(vmInternal, cv);
            }
            break;
         case OP_UINT_TO_TYPED:
            if (frame.dynTypeId != 0)
            {
               KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeUnsigned(evalState.intStack[frame._UINT--]);

               KorkApi::TypeStorageInterface outputStorage = KorkApi::CreateExprStringStackStorage(vmInternal,
                                                                                       mSTR,
                                                                                       0,
                                                                                       frame.dynTypeId);

               KorkApi::TypeStorageInterface inputStorage = KorkApi::CreateRegisterStorageFromArg(vmInternal, cv);
               
               // NOTE: types should set head of stack to value if data pointer is nullptr in this case
               vmInternal->mTypes[frame.dynTypeId].iFuncs.CastValueFn(vmInternal->mTypes[frame.dynTypeId].userPtr,
                                                                   vmPublic,
                                                                   &inputStorage,
                                                                   &outputStorage,
                                                                   nullptr,
                                                                   0,
                                                                   frame.dynTypeId);
            }
            else
            {
               KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeUnsigned(evalState.floatStack[frame._UINT--]);
               evalState.mSTR.setConsoleValue(vmInternal, cv);
            }
            break;
            
         case OP_SET_DYNAMIC_TYPE_FROM_VAR:
         {
            frame.dynTypeId = frame.currentVar.var ? frame.currentVar.dictionary->getEntryValue(frame.currentVar.var).typeId : 0;
            break;
         }

         case OP_SET_DYNAMIC_TYPE_FROM_FIELD:
         {
            frame.dynTypeId = frame.curObject ? vmInternal->getObjectFieldType(frame.curObject, frame.curField, frame.curFieldArray) : 0;
            break;
         }
         
         case OP_SET_DYNAMIC_TYPE_FROM_ID:
         {
            frame.dynTypeId = frame.codeBlock->getRealTypeID(code[ip++]);
            break;
         }
            
         case OP_SET_DYNAMIC_TYPE_TO_NULL:
         {
            frame.dynTypeId = 0;
            break;
         }
            
         case OP_SETCURVAR_TYPE:
         {
            U32 typeId = code[ip++];
            
            if (frame.currentVar.var)
            {
               typeId = frame.codeBlock->getRealTypeID((U16)typeId);
               frame.currentVar.dictionary->setEntryType(frame.currentVar.var, typeId);
            }
            
            break;
         }
            
         default:
            // error!
            goto execFinished;
      }
   }
      
execFinished:
   
   // If user has opted to suspend thread, bail out here; mLastFiberValue should be returned from
   // a relevant control function
   if (mState == KorkApi::FiberRunResult::SUSPENDED)
   {
      frame.ip = ip;
      result.state = mState;
      result.value = mLastFiberValue;
      return result;
   }
   
   // Handle exceptions
   if (lastThrow != 0)
   {
      frame.ip = ip;
      
      for (S32 i=frame._TRY-1; i>=0; i--)
      {
         TryItem& item = evalState.tryStack[i];
         
         // If item is before the frame we started at, ignore it
         // (i.e. if we go down to that depth, will we still be in a frame pushed by the current native exec?)
         if ((S32)item.frameDepth <= getMinStackDepth())
         {
            break;
         }
         
         if ((lastThrow & item.mask) != 0)
         {
            // Acceptable handler
            if (handleThrow(i, &item, getMinStackDepth()))
            {
               lastThrow = 0;
               AssertFatal(vmFrames.size() > 0, "Too many frames popped!");
               loopFrameSetup = true;
               goto execFinished;
            }
         }
      }
      
      // If nothing handled error, this is bad. Error the entire fiber
      // UNLESS last bit 31 is set in which case continue execution.
      if ((lastThrow & BIT(31)) == 0)
      {
         // Set error state
         FIBER_STATE(KorkApi::FiberRunResult::ERROR);
         vmInternal->mLastExceptionInfo.ip = ip;
         vmInternal->mLastExceptionInfo.code = lastThrow;
         vmInternal->mLastExceptionInfo.cb = frame.codeBlock;

         // Unwind stack as much as we can and return info
         handleThrow(-1, nullptr, getMinStackDepth());
         result.state = mState;
         result.value = KorkApi::ConsoleValue::makeNumber(lastThrow);
         result.exceptionInfo = &vmInternal->mLastExceptionInfo;
         
         AssertFatal(vmFrames.size() == getStackMinDepth()+1, "This is bad");
         return result;
      }
      lastThrow = 0;
   }
   
   // Do we need to setup a new frame? if so skip back to start
   // NOTE: we dont need this when resuming a yielded thread since we start in the
   // correct place in that case.
   if (loopFrameSetup)
   {
      continue;
   }
   
   // Update ip in the event we need to know where we jumped ship
   frame.ip = ip;
   
   // Update the yield value (which is now basically the return value i.e. head of stack)
   result.state = mState;
   result.value = evalState.mSTR.getConsoleValue();
   
   // Stuff which follows is normally done when function exits...
   
   // NOTE: previously happened in the ScriptFunctionType conditional, now tied to frame lifetime
   bool didClear = clearStringStack(frame, false);
   if (didClear)
   {
      // We assume here this is a function call; result should be moved to the new head stack pointer
      evalState.mSTR.setStringValue(vmInternal->valueAsString(result.value));
   }
   else
   {
      AssertFatal(getMinStackSize() == 0, "Function call occured but no stack pop?");
   }
   
   S32 oldMinSize = getMinStackSize();
   
   // ALWAYS pop the frame in this case we are done with it
   evalState.popFrame();
   checkExtraStack = false;
   
   // Basically: If we are still inside a script frame, keep looping
   
   if (evalState.vmFrames.size() == oldMinSize) // we always start at +1 in this case so it needs to go below
   {
      FIBER_STATE(KorkApi::FiberRunResult::FINISHED);
   }
   else if (evalState.vmFrames.size() < oldMinSize)
   {
      AssertFatal(false, "Invalid case");
      FIBER_STATE(KorkApi::FiberRunResult::ERROR);
      break;
   }
   else
   {
      FIBER_STATE(KorkApi::FiberRunResult::RUNNING);
   }
   
   
   // NOTE: we rely on the inc count stuff to clear strings and such
   
execCheck:
   
   continue;
   
   FIBERS_END
   
   // If we still have frames left and didn't error, put back in the running state
   if (mState == KorkApi::FiberRunResult::FINISHED)
   {
      if (vmFrames.size() > 0)
      {
         FIBER_STATE(KorkApi::FiberRunResult::RUNNING);
      }
   }
   
   // If we exited out with an error (say, from a native function),
   // make sure stack is cleared up
   if (checkExtraStack)
   {
      while ((S32)vmFrames.size() > getMinStackSize())
      {
         ConsoleFrame* prevFrame = vmFrames.back();
         popFrame();
      }
   }

   return result;
}

void ExprEvalState::setLocalFrameVariable(StringTableEntry name, KorkApi::ConsoleValue value)
{
   vmFrames.back()->dictionary.setVariableValue(name, value);
}

KorkApi::ConsoleValue ExprEvalState::getLocalFrameVariable(StringTableEntry name)
{
   Dictionary::Entry* e = vmFrames.empty() ? nullptr : vmFrames.back()->dictionary.getVariable(name);
   
   if (!e)
   {
      return KorkApi::ConsoleValue();
   }
   
   return vmFrames.back()->dictionary.getEntryValue(e);
}


void ExprEvalState::pushFrame(StringTableEntry frameName, Namespace *ns, StringTableEntry packageName, CodeBlock* block, U32 ip)
{
   ConsoleFrame *newFrame = vmInternal->New<ConsoleFrame>(vmInternal, this);
   if (vmFrames.size() > 0)
   {
      newFrame->copyFrom(vmFrames.back(), false);
   }
   
   newFrame->scopeName = frameName;
   newFrame->scopePackage = packageName;
   newFrame->scopeNamespace = ns;
   
   newFrame->codeBlock = block;
   block->incRefCount();
   newFrame->ip = ip;
   vmFrames.push_back(newFrame);
}

bool ExprEvalState::clearStringStack(ConsoleFrame& frame, bool clearValue)
{
   bool ret = false;
   for (U32 i=0; i<frame.pushStringStackCount; i++)
   {
      if (clearValue)
      {
         mSTR.setStringValue("");
      }
      mSTR.popFrame();
      ret = true;
   }
   frame.pushStringStackCount = 0;
   return ret;
}

void ExprEvalState::popFrame()
{
   ConsoleFrame *last = vmFrames.back();
   vmFrames.pop_back();
   
   // Make sure string stack is in correct state
   bool didClear = clearStringStack(*last, true); // just in case trace is logging
   AssertFatal(didClear == false, "stack not cleaned up properly");

   // Clear any objects created in frame
   clearCreatedObjects(last->_STARTOBJ, last->_OBJ);
   last->_OBJ = last->_STARTOBJ;
   
   // Handle trace log here
   if (last->inFunctionCall)
   {
      last->inFunctionCall = false;
      
      if(traceOn)
      {
         traceBuffer[0] = 0;
         strcat(traceBuffer, "Leaving ");
         
         if(last->scopePackage)
         {
            strcat(traceBuffer, "[");
            strcat(traceBuffer, last->scopePackage);
            strcat(traceBuffer, "]");
         }
         if(last->scopeNamespace && last->scopeNamespace->mName)
         {
            snprintf(traceBuffer + strlen(traceBuffer), ExprEvalState::TraceBufferSize - strlen(traceBuffer),
                     "%s::%s() - return %s", last->scopeNamespace->mName, last->thisFunctionName, mSTR.getStringValue());
         }
         else
         {
            snprintf(traceBuffer + strlen(traceBuffer), ExprEvalState::TraceBufferSize - strlen(traceBuffer),
                     "%s() - return %s", last->thisFunctionName, mSTR.getStringValue());
         }
         vmInternal->printf(0, "%s [f=%i,ss=%i,sf=%i]", traceBuffer, vmFrames.size(), mSTR.mNumFrames, mSTR.mStartStackSize);
      }
   }
   
   const bool telDebuggerOn = vmInternal->mTelDebugger && vmInternal->mTelDebugger->isConnected();
   if ( telDebuggerOn && !last->isReference )
      vmInternal->mTelDebugger->popStackFrame();
   
   if (last->codeBlock)
   {
      last->codeBlock->decRefCount();
   }
   
   if (last->popMinDepth)
   {
      _VM--;
   }
   
   if (vmFrames.size() > 0)
   {
      ConsoleFrame* prevFrame = vmFrames.back();
      AssertFatal(prevFrame->_FLT == last->_FLT && prevFrame->_UINT == last->_UINT && prevFrame->_ITER == last->_ITER, "Stack mismatch");
   }
   
   delete last;
}

bool ExprEvalState::handleThrow(S32 throwIdx, TryItem* info, S32 minStackPos)
{
   if (vmFrames.size() == 0)
   {
      return false;
   }
   
   ConsoleFrame* curFrame = vmFrames.back();
   S32 minFrame = std::max<S32>(minStackPos, info ? info->frameDepth : -1);
   
   // Exit native function state so we dont get weird errors in loop
   if (curFrame->inNativeFunction)
   {
      if (curFrame->pushStringStackCount > 0)
      {
         mSTR.popFrame();
         curFrame->pushStringStackCount--;
      }
      
      if(curFrame->lastCallType == FuncCallExprNode::MethodCall)
         curFrame->thisObject = curFrame->saveObject;
      
      curFrame->inNativeFunction = false;
   }
   
   ConsoleFrame* frame = minFrame < 0 ? nullptr : vmFrames[minFrame];
   
   // Pop down to correct frame
   for (U32 i=vmFrames.size()-1; i>minFrame; i--)
   {
      ConsoleFrame* prevFrame = vmFrames[i];
      clearStringStack(*prevFrame, true); // just in case trace is logging
      popFrame();
   }
   mSTR.setStringValue("");
   
   // If we have a frame left, try to restore it (UNLESS we are not on the right frame)
   if (vmFrames.size() > 0 &&
       info &&
       info->frameDepth == vmFrames.size()-1)
   {
      curFrame = vmFrames.back();
      curFrame->_TRY = throwIdx > 0 ? throwIdx-1 : 0; // pop try
      curFrame->ip = info->ip;
   }
   else
   {
      return false; // ran out of frames in this case
   }
   
   return true;
}

void ExprEvalState::throwMask(U32 mask)
{
   lastThrow = mask; // should get caught by loop setup
   
   if (traceOn)
   {
      traceBuffer[0] = 0;
      vmInternal->printf(0, "  Throwing exception %u in fiber %u [f=%i,ss=%i,sf=%i]", mask, mAllocNumber, vmFrames.size(), mSTR.mNumFrames, mSTR.mStartStackSize);
   }
}

void ExprEvalState::pushFrameRef(S32 stackIndex, CodeBlock* codeBlock, U32 ip)
{
   AssertFatal( stackIndex >= 0 && stackIndex < stack.size(), "You must be asking for a valid frame!" );
   ConsoleFrame *newFrame = vmInternal->New<ConsoleFrame>(vmInternal, this, vmFrames[stackIndex]->dictionary.mHashTable);
   vmFrames.push_back(newFrame);
   
   ConsoleFrame* oldFrame = vmFrames[stackIndex];
   newFrame->copyFrom(oldFrame, true);
   newFrame->ip = ip;
   newFrame->codeBlock = codeBlock;
   newFrame->codeBlock->incRefCount();
   newFrame->isReference = true;
}

void ExprEvalState::validate()
{
   for( U32 i = 0; i < vmFrames.size(); ++ i )
      vmFrames[ i ]->dictionary.validate();
}

ConsoleBasicFrame ExprEvalState::getBasicFrameInfo(U32 idx)
{
   if (idx >= vmFrames.size())
   {
      ConsoleBasicFrame outF = {};
      return outF;
   }
   
   ConsoleFrame& frame = *vmFrames[idx];
   ConsoleBasicFrame outF;
   outF.code = frame.codeBlock;
   outF.scopeNamespace = frame.scopeNamespace;
   outF.scopeName = frame.scopeName;
   outF.ip = frame.ip;
   return outF;
}

void ExprEvalState::suspend()
{
   if (mState == KorkApi::FiberRunResult::RUNNING)
   {
      mState = KorkApi::FiberRunResult::SUSPENDED;
      mLastFiberValue = KorkApi::ConsoleValue(); // to handle void()
   }
}

KorkApi::FiberRunResult ExprEvalState::resume(KorkApi::ConsoleValue value)
{
   if (mState == KorkApi::FiberRunResult::SUSPENDED)
   {
      mState = KorkApi::FiberRunResult::RUNNING;
      mLastFiberValue = value;
      KorkApi::FiberRunResult result = runVM();
      return result;
   }
   
   return KorkApi::FiberRunResult();
}

ConsoleSerializer::ConsoleSerializer(KorkApi::VmInternal* target, void* userPtr, bool allowId, Stream* s)
{
   mTarget = target;
   mUserPtr = userPtr;
   mAllowId = allowId;
   mStream = s;
}

ConsoleSerializer::~ConsoleSerializer()
{
}

bool ConsoleSerializer::isOk()
{
   return mStream && mStream->getStatus() == Stream::Ok;
}

S32 ConsoleSerializer::addReferencedCodeblock(CodeBlock* block)
{
   if (block == nullptr)
   {
      return -1;
   }

   auto itr = std::find(mCodeBlocks.begin(), mCodeBlocks.end(), block);
   if (itr != mCodeBlocks.end())
   {
      return (S32)(itr - mCodeBlocks.begin());
   }
   else
   {
      mCodeBlocks.push_back(block);
      block->incRefCount();
      return (S32)mCodeBlocks.size()-1;
   }
}

S32 ConsoleSerializer::addReferencedDictionary(Dictionary::HashTableData* dict)
{
   if (dict == nullptr)
   {
      return -1;
   }

   auto itr = std::find(mDictionaryTables.begin(), mDictionaryTables.end(), dict);
   if (itr != mDictionaryTables.end())
   {
      return (S32)(itr - mDictionaryTables.begin());
   }
   else
   {
      mDictionaryTables.push_back(dict);
      return (S32)mDictionaryTables.size()-1;
   }
}

S32 ConsoleSerializer::addReferencedFiber(ExprEvalState* fiber)
{
   if (fiber == nullptr)
   {
      return -1;
   }

   auto itr = std::find(mFibers.begin(), mFibers.end(), fiber);
   if (itr != mFibers.end())
   {
      return (S32)(itr - mFibers.begin());
   }
   else
   {
      mFibers.push_back(fiber);
      return (S32)mFibers.size()-1;
   }
}

CodeBlock* ConsoleSerializer::getReferencedCodeblock(S32 blockId)
{
   return blockId < 0 ? nullptr : mCodeBlocks[blockId];
}

Dictionary::HashTableData* ConsoleSerializer::getReferencedDictionary(S32 dictId)
{
   return dictId < 0 ? nullptr : mDictionaryTables[dictId];
}

ExprEvalState* ConsoleSerializer::getReferencedFiber(S32 fiberId)
{
   return fiberId < 0 ? nullptr : mFibers[fiberId];
}

ExprEvalState* ConsoleSerializer::loadEvalState()
{
   Remap theRemap;
   U32 version = 0;
   bool ownsDict = false;
   
   if (!mStream->read(&version) ||
       version != CEOB_VERSION)
   {
      return nullptr;
   }

   if (!mStream->read(&theRemap.oldIndex))
   {
      return nullptr;
   }

   ExprEvalState* state = mTarget->createFiberPtr(mUserPtr);
   if (state == nullptr)
   {
      return nullptr;
   }

   theRemap.newIndex = state->mAllocNumber-1;
   mFiberRemap.push_back(theRemap);

   // Stacks
   for (U32 i=0; i<MaxIterStackSize; i++)
   {
      readIterStackRecord(state->iterStack[i]);
   }
   mStream->read(sizeof(state->floatStack), state->floatStack);
   mStream->read(sizeof(state->intStack),   state->intStack);
   for (U32 i=0; i<ObjectCreationStackSize; i++)
   {
      auto& item = state->objectCreationStack[i];
      item.newObject = loadObject();
      mStream->read(&item.failJump);
   }
   mStream->read(sizeof(state->tryStack), state->tryStack);
   mStream->read(sizeof(state->vmStack), state->vmStack);

   // Offsets
   mStream->read(&state->_VM);

   // String stack
   readStringStack(state->mSTR);

   // Names
   state->mCurrentFile = readSTString(mStream);
   state->mCurrentRoot = readSTString(mStream);
   
   // State
   U8 val = 0;
   mStream->read(&val);
   mStream->read(&ownsDict);
   state->mState = (KorkApi::FiberRunResult::State)val;
   readHeapData(state->mLastFiberHeapData);
   readConsoleValue(state->mLastFiberValue, state->mLastFiberHeapData);
   
   // Reset alignment
   U8 padBytes = 0;
   U8 pad = 0x0;
   mStream->read(&padBytes);
   for (U32 i=0; i<padBytes; i++)
   {
      mStream->read(&pad);
   }
   
   // Dictionary
   S32 dictionaryId = addReferencedDictionary(state->globalVars.mHashTable);
   mStream->read(&dictionaryId);

   // Frames
   U32 numFrames = 0;
   mStream->read(&numFrames);
   for (U32 i=0; i<numFrames; i++)
   {
      IFFBlock frameBlock;
      
      if (!mStream->read(sizeof(IFFBlock), &frameBlock) ||
          frameBlock.ident != CFFB_MAGIC)
      {
         delete state;
         return nullptr;
      }
      
      U32 startBlock = mStream->getPosition();
      
      ConsoleFrame* frame = loadFrame(state);
      if (frame == nullptr)
      {
         return nullptr;
      }
      
      state->vmFrames.push_back(frame);
      
      frameBlock.seekNext(*mStream, mStream->getPosition()-startBlock);
   }
   
   // Set dictionary owner
   if (dictionaryId < 0)
   {
      state->globalVars.setState(mTarget, mTarget->mGlobalVars.mHashTable);
   }
   else
   {
      if (ownsDict)
      {
         mDictionaryTables[dictionaryId]->owner = &state->globalVars;
      }
      state->globalVars.setState(mTarget, mDictionaryTables[dictionaryId]);
   }
   
   return state;
}

bool ConsoleSerializer::saveEvalState(ExprEvalState* state)
{
   U32 startWrite = mStream->getPosition();
   U32 oldIndex = state->mAllocNumber-1;
   mStream->write(CEOB_VERSION);
   mStream->write(oldIndex);

   // Stacks
   for (U32 i=0; i<MaxIterStackSize; i++)
   {
      writeIterStackRecord(state->iterStack[i]);
   }
   mStream->write(sizeof(state->floatStack), state->floatStack);
   mStream->write(sizeof(state->intStack), state->intStack);
   for (U32 i=0; i<ObjectCreationStackSize; i++)
   {
      auto& item = state->objectCreationStack[i];
      writeObject(item.newObject);
      mStream->write(item.failJump);
   }
   mStream->write(sizeof(state->tryStack), state->tryStack);
   mStream->write(sizeof(state->vmStack), state->vmStack);

   // Offsets
   mStream->write(state->_VM);

   // String stack
   writeStringStack(state->mSTR);

   // Names
   mStream->writeString(state->mCurrentFile);
   mStream->writeString(state->mCurrentRoot);

   // State
   mStream->write((U8)state->mState);
   bool ownsDict = state->globalVars.mHashTable && state->globalVars.mHashTable->owner == &state->globalVars;
   mStream->write(ownsDict);
   writeHeapData(state->mLastFiberHeapData);
   writeConsoleValue(state->mLastFiberValue, state->mLastFiberHeapData);
   
   // Reset alignment
   U8 padBytes = 0;
   U32 alignPos = mStream->getPosition() - startWrite;
   if (((alignPos+1) & BIT(0)) != 0)
   {
      padBytes = 1;
   }
   
   mStream->write(padBytes);
   for (U32 i=0; i<padBytes; i++)
   {
      mStream->write((U8)0xFF);
   }
   
   // Dictionary
   S32 dictionaryId = addReferencedDictionary(ownsDict ? state->globalVars.mHashTable : nullptr); // fiber must own globals for them to be written
   mStream->write(dictionaryId);

   // Frames
   mStream->write((U32)state->vmFrames.size());
   for (ConsoleFrame* frame : state->vmFrames)
   {
      IFFBlock frameBlock;
      frameBlock.ident = CFFB_MAGIC;
      U32 startBlock = mStream->getPosition();
      
      if (!mStream->write(sizeof(IFFBlock), &frameBlock))
      {
         return false;
      }
      
      if (!writeFrame(*frame))
      {
         return false;
      }
      
      frameBlock.updateSize(*mStream, startBlock);
      if (!frameBlock.writePad(*mStream))
      {
         return false;
      }
   }
   
   return true;
}

bool ConsoleSerializer::readConsoleValue(KorkApi::ConsoleValue& value, KorkApi::ConsoleHeapAllocRef dataRef)
{
   if (!(mStream->read(&value.cvalue) &&
        mStream->read(&value.typeId) &&
        mStream->read(&value.zoneId)))
   {
      return false;
   }
   
   // Load external data
   if (dataRef && value.getZone() == KorkApi::ConsoleValue::ZoneVmHeap)
   {
      value.cvalue = (U64)dataRef->ptr();
   }
   else if (!(value.isUnsigned() || value.isFloat()) &&
            (
            value.getZone() == KorkApi::ConsoleValue::ZoneExternal ||
            value.getZone() == KorkApi::ConsoleValue::ZoneReturn ||
            value.getZone() == KorkApi::ConsoleValue::ZoneVmHeap))
   {
      value.zoneId = KorkApi::ConsoleValue::ZoneExternal;
      value.cvalue = 0;
   }
   
   return true;
}

bool ConsoleSerializer::writeConsoleValue(KorkApi::ConsoleValue& value, KorkApi::ConsoleHeapAllocRef dataRef)
{
   return mStream->write(value.cvalue) &&
          mStream->write(value.typeId) &&
          mStream->write(value.zoneId);
}

bool ConsoleSerializer::readHeapData(KorkApi::ConsoleHeapAllocRef& ref)
{
   U32 size = 0;
   if (!mStream->read(&size))
   {
      return false;
   }
   
   if (size > 0)
   {
      ref = mTarget->createHeapRef(size);
      return mStream->read(size, ref->ptr());
   }
   else
   {
      ref = nullptr;
   }
   
   return true;
}

bool ConsoleSerializer::writeHeapData(KorkApi::ConsoleHeapAllocRef ref)
{
   if (!ref)
   {
      return mStream->write((U32)0);
   }
   else
   {
      return mStream->write((U32)ref->size) &&
             mStream->write((U32)ref->size, ref->ptr());
   }
}

bool ConsoleSerializer::readIterStackRecord(IterStackRecord& ref)
{
   return mStream->read(&ref.mIsStringIter) &&
          mStream->read(&ref.mIndex) &&
          readHeapData(ref.mHeapData) &&
          readConsoleValue(ref.mData, ref.mHeapData);
}

bool ConsoleSerializer::writeIterStackRecord(IterStackRecord& ref)
{
   return mStream->write(ref.mIsStringIter) &&
          mStream->write(ref.mIndex) &&
          writeHeapData(ref.mHeapData) &&
          writeConsoleValue(ref.mData, ref.mHeapData);
}

bool ConsoleSerializer::readVarRef(ConsoleVarRef& ref)
{
   S32 dictId = -1;
   if (!mStream->read(&dictId))
   {
      return false;
   }
   
   StringTableEntry steName = readSTString(mStream);
   
   Dictionary::HashTableData* data = getReferencedDictionary(dictId);
   if (!data || data->owner == nullptr)
   {
      ref.dictionary = nullptr;
      ref.var = nullptr;
   }
   else
   {
      ref.dictionary = data->owner;
      ref.var = ref.dictionary->lookup(steName);
   }
   
   return mStream->getStatus() == Stream::Ok;
}

bool ConsoleSerializer::writeVarRef(ConsoleVarRef& ref)
{
   S32 dictId = addReferencedDictionary(ref.dictionary ? ref.dictionary->mHashTable : nullptr);
   mStream->write(dictId);
   mStream->writeString(ref.var ? ref.var->name : "");
   return mStream->getStatus() == Stream::Ok;
}

KorkApi::VMObject* ConsoleSerializer::loadObject()
{
   U8 refType = 0;
   KorkApi::VMObject *foundObject = nullptr;
   mStream->read(&refType);

   // By name?
   if ((refType & BIT(1)) != 0)
   {
      StringTableEntry steName = readSTString(mStream);
      foundObject = mTarget->mVM->findObjectByName(steName);
   }

   // By id?
   if (mAllowId && foundObject == nullptr && (refType & BIT(0)) != 0)
   {
      KorkApi::SimObjectId value = 0;
      mStream->read(&value);
      foundObject = mTarget->mVM->findObjectById(value);
   }

   return foundObject;
}

void ConsoleSerializer::writeObject(KorkApi::VMObject* obj)
{
   if (obj != nullptr)
   {
      StringTableEntry objName = obj->klass->iCreate.GetNameFn(obj);
      U8 objFlags = 0;
      
      if (objName)
      {
         objFlags |= BIT(1);
         mStream->write(objFlags);
         mStream->writeString(objName);
         return;
      }
      else if (mAllowId)
      {
         KorkApi::SimObjectId objId = obj->klass->iCreate.GetIdFn(obj);
         objFlags |= BIT(0);
         mStream->write(objFlags);
         mStream->write(objId);
         return;
      }
   }
   
   mStream->write((U8)0);
}

void ConsoleSerializer::readObjectRef(LocalRefTrack& track)
{
   KorkApi::VMObject *foundObject = loadObject();
   track = foundObject;
}

void ConsoleSerializer::writeObjectRef(LocalRefTrack& track)
{
   writeObject(track.obj);
}

bool ConsoleSerializer::readStringStack(StringStack& stack)
{
   stack.reset();
   
   U32 bufferSize = 0;
   if (!mStream->read(&bufferSize))
   {
      return false;
   }
   
   stack.mBuffer.resize(bufferSize);
   
   return mStream->read(bufferSize, stack.mBuffer.data()) &&
     mStream->read(sizeof(stack.mFrameOffsets), stack.mFrameOffsets) &&
     mStream->read(sizeof(stack.mStartOffsets), stack.mStartOffsets) &&
     mStream->read(sizeof(stack.mStartTypes), stack.mStartTypes) &&
     mStream->read(sizeof(stack.mStartValues), stack.mStartValues) &&
     mStream->read(&stack.mValue) &&
     mStream->read(&stack.mType) &&
     mStream->read(&stack.mFuncId) &&
     mStream->read(&stack.mNumFrames) &&
     mStream->read(&stack.mStart) &&
     mStream->read(&stack.mLen) &&
     mStream->read(&stack.mStartStackSize) &&
     mStream->read(&stack.mFunctionOffset);
}

bool ConsoleSerializer::writeStringStack(StringStack& stack)
{
   return mStream->write((U32)stack.mBuffer.size()) &&
     mStream->write((U32)stack.mBuffer.size(), stack.mBuffer.data()) &&
     mStream->write(sizeof(stack.mFrameOffsets), stack.mFrameOffsets) &&
     mStream->write(sizeof(stack.mStartOffsets), stack.mStartOffsets) &&
     mStream->write(sizeof(stack.mStartTypes), stack.mStartTypes) &&
     mStream->write(sizeof(stack.mStartValues), stack.mStartValues) &&
     mStream->write(stack.mValue) &&
     mStream->write(stack.mType) &&
     mStream->write(stack.mFuncId) &&
     mStream->write(stack.mNumFrames) &&
     mStream->write(stack.mStart) &&
     mStream->write(stack.mLen) &&
     mStream->write(stack.mStartStackSize) &&
     mStream->write(stack.mFunctionOffset)
   ;
}

ConsoleFrame* ConsoleSerializer::loadFrame(ExprEvalState* state)
{
   // Global refs
   S32 blockId = -1;
   S32 dictionaryId = -1;
   bool ownsDict = false;

   mStream->read(&blockId);
   mStream->read(&dictionaryId);
   mStream->read(&ownsDict);

   CodeBlock* block = getReferencedCodeblock(blockId);
   Dictionary::HashTableData* dict = getReferencedDictionary(dictionaryId);
   
   state->globalVars.setState(mTarget, mTarget->mGlobalVars.mHashTable);
   
   if (block == nullptr)
   {
      return nullptr;
   }

   ConsoleFrame* frame = state->vmInternal->New<ConsoleFrame>(mTarget, state, dict);
   
   if (ownsDict)
   {
      dict->owner = &frame->dictionary;
   }

   // Flags
   bool inFunction = false;
   mStream->read(&inFunction);
   mStream->read(&frame->noCalls);
   mStream->read(&frame->isReference);
   mStream->read(&frame->inNativeFunction);
   mStream->read(&frame->popMinDepth);
   //
   mStream->read(&frame->pushStringStackCount);

   // Stack
   mStream->read(&frame->_FLT);
   mStream->read(&frame->_UINT);
   mStream->read(&frame->_ITER);
   mStream->read(&frame->_OBJ);
   mStream->read(&frame->_TRY);
   mStream->read(&frame->_STARTOBJ);

   // Offsets
   mStream->read(&frame->failJump);
   mStream->read(&frame->stackStart);
   mStream->read(&frame->callArgc);
   mStream->read(&frame->ip);
   mStream->read(&frame->lastCallType);

   // Dictionary state
   frame->scopeName = readSTString(mStream);
   frame->scopePackage = readSTString(mStream);
   StringTableEntry nsName = readSTString(mStream);
   frame->scopeNamespace = mTarget->mNSState.find(nsName, frame->scopePackage); // TODO: is this correct?
   
   // References
   readObjectRef(frame->currentNewObject);
   readObjectRef(frame->thisObject);
   readObjectRef(frame->prevObject);
   readObjectRef(frame->curObject);
   readObjectRef(frame->saveObject);
   readObjectRef(frame->curIterObject);
   
   readVarRef(frame->currentVar);
   readVarRef(frame->copyVar);
   
   // Docblock
   mStream->read(&frame->nsDocBlockClassOffset);
   mStream->read(&frame->nsDocBlockOffset);
   mStream->read(&frame->nsDocBlockClassNameLength);
   mStream->read(&frame->nsDocBlockClassLocation);

   // Buffers
   mStream->read(ConsoleFrame::FieldArraySize, frame->curFieldArray);
   mStream->read(ConsoleFrame::FieldArraySize, frame->prevFieldArray);

   // Restore everything remaining

   frame->codeBlock = block;
   block->incRefCount();
   
   if (inFunction)
   {
      frame->curFloatTable = block->functionFloats;
      frame->curStringTable = block->functionStrings;
   }
   else
   {
      frame->curFloatTable = block->globalFloats;
      frame->curStringTable = block->globalStrings;
   }
   
   return frame;
}

bool ConsoleSerializer::writeFrame(ConsoleFrame& frame)
{
   // Global refs
   S32 blockId = addReferencedCodeblock(frame.codeBlock);
   S32 dictionaryId = addReferencedDictionary(frame.dictionary.mHashTable);
   bool ownsDict = frame.dictionary.mHashTable->owner == &frame.dictionary;
   
   if (blockId < 0 ||
       dictionaryId < 0)
   {
      return false;
   }
   
   mStream->write(blockId);
   mStream->write(dictionaryId);
   mStream->write(ownsDict);

   bool inFunction = frame.curFloatTable != frame.codeBlock->globalFloats;

   // Flags
   mStream->write(inFunction);
   mStream->write(frame.noCalls);
   mStream->write(frame.isReference);
   mStream->write(frame.inNativeFunction);
   mStream->write(frame.popMinDepth);
   //
   mStream->write(frame.pushStringStackCount);

   // Stack
   mStream->write(frame._FLT);
   mStream->write(frame._UINT);
   mStream->write(frame._ITER);
   mStream->write(frame._OBJ);
   mStream->write(frame._TRY);
   mStream->write(frame._STARTOBJ);

   // Offsets
   mStream->write(frame.failJump);
   mStream->write(frame.stackStart);
   mStream->write(frame.callArgc);
   mStream->write(frame.ip);
   mStream->write(frame.lastCallType);

   // Dictionary state
   mStream->writeString(frame.scopeName);
   mStream->writeString(frame.scopePackage);
   mStream->writeString(frame.scopeNamespace ? frame.scopeNamespace->mName : "");

   // References
   writeObjectRef(frame.currentNewObject);
   writeObjectRef(frame.thisObject);
   writeObjectRef(frame.prevObject);
   writeObjectRef(frame.curObject);
   writeObjectRef(frame.saveObject);
   writeObjectRef(frame.curIterObject);
   
   writeVarRef(frame.currentVar);
   writeVarRef(frame.copyVar);

   // Docblock
   mStream->write(frame.nsDocBlockClassOffset);
   mStream->write(frame.nsDocBlockOffset);
   mStream->write(frame.nsDocBlockClassNameLength);
   mStream->write(frame.nsDocBlockClassLocation);

   // Buffers
   mStream->write(ConsoleFrame::FieldArraySize, frame.curFieldArray);
   mStream->write(ConsoleFrame::FieldArraySize, frame.prevFieldArray);
   
   return true;
}

void ConsoleSerializer::fixupConsoleValues()
{
   auto remapCV = [this](KorkApi::ConsoleValue& v){
      U32 oldZone = v.getZone() - KorkApi::ConsoleValue::ZoneFiberStart;
      
      for (Remap& r : mFiberRemap)
      {
         if (r.oldIndex == oldZone)
         {
            v.setZone((KorkApi::ConsoleValue::Zone)(KorkApi::ConsoleValue::ZoneFiberStart + r.newIndex));
            break;
         }
      }
   };
   
   // Dictionary variables
   for (Dictionary::HashTableData* ht : mDictionaryTables)
   {
      for (U32 i=0; i<ht->size; i++)
      {
         for (Dictionary::Entry* entry = ht->data[i]; entry; entry = entry->nextEntry)
         {
            KorkApi::ConsoleValue& v = entry->mConsoleValue;
            if (v.getZone() >= KorkApi::ConsoleValue::ZoneFiberStart)
            {
               remapCV(v);
            }
         }
      }
   }
   
   // Fiber values
   for (ExprEvalState* state : mFibers)
   {
      if (state->mLastFiberValue.getZone() >= KorkApi::ConsoleValue::ZoneFiberStart)
      {
         remapCV(state->mLastFiberValue);
      }
      
      for (U32 i=0; i<MaxIterStackSize; i++)
      {
         if (state->iterStack[i].mData.getZone() >= KorkApi::ConsoleValue::ZoneFiberStart)
         {
            remapCV(state->iterStack[i].mData);
         }
      }
   }
}

void ConsoleSerializer::reset(bool ownObjects)
{
   if (ownObjects && mTarget)
   {
      for (ExprEvalState* state : mFibers)
      {
         mTarget->cleanupFiber(mTarget->mFiberStates.getHandleValue(state));
      }
   }
   
   for (CodeBlock* block : mCodeBlocks)
   {
      block->decRefCount();
   }
   
   for (Dictionary::HashTableData* ht : mDictionaryTables)
   {
      if (ht->owner == nullptr)
      {
         delete ht;
      }
   }
   
   mCodeBlocks.clear();
   mDictionaryTables.clear();
   mFibers.clear();
   mFiberRemap.clear();
}

bool ConsoleSerializer::read(KorkApi::Vector<ExprEvalState*> &fibers)
{
   IFFBlock block;
   if (!mStream->read(sizeof(IFFBlock), &block))
   {
      reset(true);
      return false;
   }
   
   if (block.ident != CSOB_MAGIC || block.getRawSize() < 12)
   {
      reset(true);
      return false;
   }
   
   U32 version = 0;
   U32 objectOffset = 0;
   U32 mainOffset = 0;
   mStream->read(&version);
   mStream->read(&objectOffset);
   mStream->read(&mainOffset);
   
   if (version != CSOB_VERSION)
   {
      reset(true);
      return false;
   }
   
   mStream->setPosition(objectOffset);
   
   if (!loadRelatedObjects())
   {
      reset(true);
      return false;
   }
   
   mStream->setPosition(mainOffset);
   
   if (!loadFibers())
   {
      reset(true);
      return false;
   }
   
   fibers = mFibers;
   
   return true;
}

bool ConsoleSerializer::write(KorkApi::Vector<ExprEvalState*> &fibers)
{
   // This is:
   // CSOB
   //   version
   //   <offsets>
   //   <fiber list>
   //   <object list>
   
   U32 startPos = mStream->getPosition();

   IFFBlock block;
   block.ident = CSOB_MAGIC;
   if (!mStream->write(sizeof(IFFBlock), &block))
   {
      reset(false);
      return false;
   }

   mStream->write(CSOB_VERSION);
   U32 objectOffset = mStream->getPosition(); mStream->write((U32)0);
   U32 mainOffset = mStream->getPosition(); mStream->write((mStream->getPosition() - startPos) + 4);

   mFibers = fibers;
   if (!saveFibers())
   {
      reset(false);
      return false;
   }

   U32 startObjects = mStream->getPosition() - startPos;
   mStream->setPosition(objectOffset);
   mStream->write(startObjects);
   mStream->setPosition(startObjects);
   
   if (!saveRelatedObjects())
   {
      reset(false);
      return false;
   }
   
   block.updateSize(*mStream, startPos);
   if (!block.writePad(*mStream))
   {
      reset(false);
      return false;
   }
   
   reset(false);
   return true;
}

Dictionary::HashTableData* ConsoleSerializer::loadHashTable()
{
   Dictionary tempDict(mTarget);
   tempDict.reset();

   U32 entryCount = 0;
   if (!mStream->read(&entryCount))
   {
      return nullptr;
   }
   
   for (U32 i = 0; i < entryCount; ++i)
   {
      bool isConst = false;
      if (!mStream->read(&isConst))
      {
         return nullptr;
      }
      
      StringTableEntry name = readSTString(mStream);
      if (name == nullptr)
      {
         return nullptr;
      }
      
      Dictionary::Entry* entry = tempDict.add(name);
      if (!entry)
      {
         return nullptr;
      }
      
      U32 valueSize = 0;
      mStream->read(&valueSize);
      
      // Set heap value
      if (valueSize != 0)
      {
         entry->mHeapAlloc = mTarget->createHeapRef(valueSize);
         if (!mStream->read(valueSize, entry->mHeapAlloc->ptr()))
         {
            return nullptr;
         }
      }
      
      if (!readConsoleValue(entry->mConsoleValue, entry->mHeapAlloc))
      {
         return nullptr;
      }
      
      entry->mIsConstant = isConst;
   }
   
   Dictionary::HashTableData* ht = tempDict.mHashTable;
   ht->owner = nullptr;
   tempDict.setState(mTarget, nullptr);
   return ht;
}

bool ConsoleSerializer::writeHashTable(const Dictionary::HashTableData* ht)
{
   U32 entryCount = (U32)ht->count;

   if (!mStream->write(entryCount))
      return false;

   if (ht->size <= 0 ||
       entryCount == 0)
   {
      return true;
   }
   
   for (S32 i = 0; i < ht->size; ++i)
   {
      for (Dictionary::Entry* entry = ht->data[i]; entry; entry = entry->nextEntry)
      {
         mStream->write((bool)entry->mIsConstant);
         mStream->writeString(entry->name);
         
         if (mStream->getStatus() != Stream::Ok)
         {
            return false;
         }
         
         if (entry->mHeapAlloc)
         {
            mStream->write((U32)entry->mHeapAlloc->size);
            mStream->write(entry->mHeapAlloc->size, entry->mHeapAlloc->ptr());
         }
         else
         {
            mStream->write((U32)0);
         }
         
         if (!writeConsoleValue(entry->mConsoleValue, entry->mHeapAlloc))
         {
            return false;
         }
      }
   }

   return true;
}

bool ConsoleSerializer::loadRelatedObjects()
{
   IFFBlock scanBlock;
   while (mStream->read(sizeof(IFFBlock), &scanBlock))
   {
      U32 startBlockPos = mStream->getPosition();

      switch (scanBlock.ident)
      {
      case DICT_MAGIC:
         // Serialized dictionary
         {
            if (scanBlock.getRawSize() < 4)
            {
               scanBlock.seekNext(*mStream, 0);
               continue;
            }
            
            Dictionary::HashTableData* ht = loadHashTable();
            
            if (ht == nullptr)
            {
               return false;
            }
            
            mDictionaryTables.push_back(ht);
            scanBlock.seekNext(*mStream, mStream->getPosition() - startBlockPos);
         }
         break;
      case DSOB_MAGIC:
         // Serialized codeblock
         {
            if (scanBlock.getRawSize() < 4)
            {
               scanBlock.seekNext(*mStream, 0);
               continue;
            }
            
            StringTableEntry steFilename = readSTString(mStream);
            StringTableEntry steModPath = readSTString(mStream);
            CodeBlock* block = mTarget->New<CodeBlock>(mTarget, true);
            if (!block->read(steFilename[0] == '\0' ? nullptr : steFilename,
                             steModPath[0] == '\0' ? nullptr : steModPath,
                             *mStream, 0))
            {
               delete block;
               return false;
            }
            block->incRefCount();
            mCodeBlocks.push_back(block);
            scanBlock.seekNext(*mStream, mStream->getPosition() - startBlockPos);
         }
         break;
      case EOLB_MAGIC:
         // End of list block
         scanBlock.seekNext(*mStream, 0);
         return true;
      default:
            AssertFatal(false, "Unknown block");
            return false;
         break;
      }
   }
   
   return false;
}

bool ConsoleSerializer::saveRelatedObjects()
{
   IFFBlock writeBlock;
   writeBlock.ident = DICT_MAGIC;
   
   for (Dictionary::HashTableData* data : mDictionaryTables)
   {
      U32 startPos = mStream->getPosition();
      if (!mStream->write(sizeof(IFFBlock), &writeBlock))
      {
         return false;
      }
      
      writeHashTable(data);
      writeBlock.updateSize(*mStream, startPos);
      writeBlock.writePad(*mStream);
   }
   
   writeBlock.ident = DSOB_MAGIC;
   for (CodeBlock* block : mCodeBlocks)
   {
      U32 startPos = mStream->getPosition();
      if (!mStream->write(sizeof(IFFBlock), &writeBlock))
      {
         return false;
      }
      
      mStream->writeString(block->name);
      mStream->writeString(block->modPath);
      if (!block->write(*mStream))
      {
         return false;
      }
      
      writeBlock.updateSize(*mStream, startPos);
      writeBlock.writePad(*mStream);
   }
   
   // End of list
   writeBlock.ident = EOLB_MAGIC;
   writeBlock.setSize(0);
   return mStream->write(sizeof(IFFBlock), &writeBlock);
}

bool ConsoleSerializer::loadFibers()
{
   IFFBlock scanBlock;
   while (mStream->read(sizeof(IFFBlock), &scanBlock))
   {
      U32 startInBlock = mStream->getPosition();
      
      ExprEvalState* evalState = nullptr;

      switch (scanBlock.ident)
      {
      case CEOB_MAGIC:
         if (scanBlock.getRawSize() < 16)
         {
            scanBlock.seekNext(*mStream, 0);
            continue;
         }
         
         // Serialized fiber
         evalState = loadEvalState();
         if (!evalState)
         {
            return false;
         }
         mFibers.push_back(evalState);
         break;
      case EOLB_MAGIC:
         // End of list block
         return true;
      default:
            AssertFatal(false, "Unknown block");
            return false;
         break;
      }
      
      // Go to next block
      scanBlock.seekNext(*mStream, mStream->getPosition()-startInBlock);
   }
   
   return false; // invalid list
}

bool ConsoleSerializer::saveFibers()
{
   IFFBlock writeBlock;
   writeBlock.ident = CEOB_MAGIC;
   for (ExprEvalState* state : mFibers)
   {
      U32 startPos = mStream->getPosition();
      if (!mStream->write(sizeof(IFFBlock), &writeBlock))
      {
         return false;
      }
      
      if (!saveEvalState(state))
      {
         return false;
      }
      
      writeBlock.updateSize(*mStream, startPos);
   }
   
   writeBlock.ident = EOLB_MAGIC;
   writeBlock.setSize(0);
   return mStream->write(sizeof(IFFBlock), &writeBlock);
}

const char* ConsoleSerializer::readSTString(Stream* s, bool casesens)
{
   char buf[256];
   s->readString(buf);
   return mTarget->internString(buf, casesens);
}

bool KorkApi::VmInternal::getCurrentFiberFileLine(StringTableEntry* outFile, U32* outLine)
{
   if (!mCurrentFiberState || mCurrentFiberState->vmFrames.size() == 0)
   {
      return false;
   }
   
   ConsoleFrame* frame = mCurrentFiberState->vmFrames.back();
   CodeBlock* block = frame->codeBlock;
   
   U32 line = 0;
   U32 inst = 0;
   block->findBreakLine(frame->ip, line, inst);
   
   *outFile = block->name;
   *outLine = line;
}

KorkApi::FiberFrameInfo KorkApi::Vm::getCurrentFiberFrameInfo(S32 frameId)
{
   KorkApi::FiberFrameInfo outInfo = {};
   
   if (!mInternal->mCurrentFiberState)
   {
      return outInfo;
   }
   
   if (frameId < 0)
   {
      frameId = (S32)(mInternal->mCurrentFiberState->vmFrames.size() + frameId);
   }
   
   if (!mInternal->mCurrentFiberState ||
       frameId < 0 ||
       frameId >= mInternal->mCurrentFiberState->vmFrames.size())
   {
      return outInfo;
   }
   
   ConsoleFrame* curFrame = mInternal->mCurrentFiberState->vmFrames[frameId];
   outInfo.fullPath = curFrame->codeBlock->fullPath;
   outInfo.scopeName = curFrame->scopeName;
   outInfo.scopeNamespace = curFrame->scopeNamespace ? curFrame->scopeNamespace->mName : mInternal->internString("", false);
   
   return outInfo;
}



void KorkApi::Vm::enumGlobals(const char* expr, void* userPtr, EnumFuncCallback callback)
{
   mInternal->mGlobalVars.exportVariables(expr, userPtr, callback);
}

bool KorkApi::Vm::enumLocals(void* userPtr, EnumFuncCallback callback, S32 frame)
{
   U32 frameIdx = frame < 0 ? (U32)(mInternal->mCurrentFiberState->vmFrames.size()-1) : (U32)frame;
   if (frameIdx >= mInternal->mCurrentFiberState->vmFrames.size())
   {
      return false;
   }

   mInternal->mCurrentFiberState->vmFrames[frameIdx]->dictionary.exportVariables(nullptr, userPtr, callback);
   return true;
}

//------------------------------------------------------------

