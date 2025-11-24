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
#include "console/simpleLexer.h"
#include "console/ast.h"
#include "console/consoleNamespace.h"

#include "core/findMatch.h"
#include "console/consoleInternal.h"
#include "console/consoleNamespace.h"
#include "core/fileStream.h"
#include "console/compiler.h"

// TOFIX #include "sim/netStringTable.h"
#include "console/stringStack.h"

#include "console/telnetDebugger.h"

using namespace Compiler;

struct LocalRefTrack
{
   KorkApi::VmInternal* vm;
   KorkApi::VMObject* obj;

   LocalRefTrack(KorkApi::VmInternal* _vm) : vm(_vm), obj(NULL) {;}
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

struct ConsoleVarRef
{
   Dictionary* dictionary;
   Dictionary::Entry *var;
   
   ConsoleVarRef() : dictionary(NULL), var(NULL)
   {
      
   }
};

struct ConsoleFrame
{
   enum
   {
      NSDocLength = 128,
      FieldArraySize = 256,
   };

   
   // Context (16 bytes)
   
   Dictionary* dictionary;
   ExprEvalState* evalState;
   
   // Frame state (24 bytes + 20 bytes)
   F64*            curFloatTable;
   char*           curStringTable;
   //S32             curStringTableLen;
   bool            popStringStack;
   bool            noCalls;
   bool            isReference;
   bool            inNativeFunction;
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
   
   U16 _STARTOBJ;
   
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
   StringTableEntry prevField;
   StringTableEntry curField;
   
   // These are used when calling functions but we might need them until exec ends (16 bytes)
   Namespace::Entry* nsEntry;
   Namespace*        ns;  // ACTIVE namespace (e.g. for parentcall)

   // Args (16 bytes)
   KorkApi::ConsoleValue* callArgv;
   const char**           callArgvS;
   
   U32 lastCallType;

   // Buffers (640 bytes)
   char nsDocBlockClass[NSDocLength];
   char curFieldArray[FieldArraySize];
   char prevFieldArray[FieldArraySize];

public:
   ConsoleFrame(KorkApi::VmInternal* vm, ExprEvalState* fiber)
      : stackStart(0)
      , dictionary(nullptr)
      , evalState(nullptr)
      , curFloatTable(nullptr)
      , curStringTable(nullptr)
      //, curStringTableLen(0)
      , thisFunctionName(nullptr)
      , popStringStack(false)
      , noCalls(false)
      , isReference(false)
      , inNativeFunction(false)
      , failJump(0)
      , scopeName( NULL )
      , scopePackage( NULL )
      , scopeNamespace( NULL )
      , codeBlock( NULL )
      , thisObject( NULL )
      , ip( 0 )
      , _FLT(0)
      , _UINT(0)
      , _ITER(0)
      , _OBJ(0)
      , _STARTOBJ(0)
      , currentNewObject(vm)
      , prevObject(vm)
      , curObject(vm)
      , saveObject(vm)
      , prevField(nullptr)
      , curField(nullptr)
      , nsEntry(nullptr)
      , ns(nullptr)
      , curFNDocBlock(nullptr)
      , curNSDocBlock(nullptr)
      , callArgc(0)
      , callArgv(nullptr)
      , callArgvS(nullptr)
      , lastCallType(0)
   {
      evalState = fiber;
      memset(nsDocBlockClass, 0, sizeof(nsDocBlockClass));
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
   inline void setCopyVariable();
};

ConsoleFrame& ExprEvalState::getCurrentFrame()
{
   return *( vmFrames[ mStackDepth - 1 ] );
}



const char *ExprEvalState::getNamespaceList(Namespace *ns)
{
   U32 size = 1;
   Namespace * walk;
   for(walk = ns; walk; walk = walk->mParent)
      size += dStrlen(walk->mName) + 4;
   char *ret = (char*)mSTR.getFuncBuffer(KorkApi::ConsoleValue::TypeInternalString, size).evaluatePtr(vmInternal->mAllocBase);
   ret[0] = 0;
   for(walk = ns; walk; walk = walk->mParent)
   {
      dStrcat(ret, walk->mName);
      if(walk->mParent)
         dStrcat(ret, " -> ");
   }
   return ret;
}


//------------------------------------------------------------

F64 consoleStringToNumber(const char *str, StringTableEntry file, U32 line)
{
   F64 val = dAtof(str);
   if(val != 0)
      return val;
   else if(!dStricmp(str, "true"))
      return 1;
   else if(!dStricmp(str, "false"))
      return 0;
   else if(file)
   {
      // TOFIX mVM->printf(0, "%s (%d): string always evaluates to 0.", file, line);
      return 0;
   }
   return 0;
}

//------------------------------------------------------------

inline void ConsoleFrame::copyFrom(ConsoleFrame* other, bool includeScope)
{
   _FLT = other->_FLT;
   _UINT = other->_UINT;
   _ITER = other->_ITER;
   _OBJ = _STARTOBJ = other->_OBJ;
   
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
   else if (dictionary)
   {
      currentVar.var = dictionary->lookup(name);
      currentVar.dictionary = dictionary;
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
   else if(dictionary)
   {
      currentVar.var = dictionary->add(name);
      currentVar.dictionary = dictionary;
   }
   else
   {
      currentVar.var = NULL;
      evalState->vmInternal->printf(1, "Accessing local variable in global scope... failed: %s", name);
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
   AssertFatal(currentVar.var != NULL, "Invalid evaluator state - trying to set null variable!");
   currentVar.dictionary->setEntryUnsignedValue(currentVar.var, val);
}

inline void ConsoleFrame::setNumberVariable(F64 val)
{
   AssertFatal(currentVar.var != NULL, "Invalid evaluator state - trying to set null variable!");
   currentVar.dictionary->setEntryNumberValue(currentVar.var, val);
}

inline void ConsoleFrame::setStringVariable(const char *val)
{
   AssertFatal(currentVar.var != NULL, "Invalid evaluator state - trying to set null variable!");
   currentVar.dictionary->setEntryStringValue(currentVar.var, val);
}

inline void ConsoleFrame::setConsoleValue(KorkApi::ConsoleValue value)
{
   AssertFatal(currentVar.var != NULL, "Invalid evaluator state - trying to set null variable!");
   currentVar.dictionary->setEntryValue(currentVar.var, value);
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
      StringTableEntry var = Compiler::CodeToSTE(NULL, code, ip + (i*2) + 6);
      
      // Add a comma so it looks nice!
      if(i != 0)
         dStrcat(buffer, ", ");
      
      dStrcat(buffer, "var ");
      
      // Try to capture junked parameters
      if(var[0])
         dStrcat(buffer, var+1);
      else
         dStrcat(buffer, "JUNK");
   }
}

//-----------------------------------------------------------------------------


static U32 castValueToU32(KorkApi::ConsoleValue retValue, KorkApi::ConsoleValue::AllocBase& allocBase)
{
   switch (retValue.typeId)
   {
      case KorkApi::ConsoleValue::TypeInternalUnsigned:
         return (U32)retValue.getInt();
      case KorkApi::ConsoleValue::TypeInternalNumber:
         return (F32)retValue.getFloat();
      case KorkApi::ConsoleValue::TypeInternalString:
         return (U32)atoll((const char*)retValue.evaluatePtr(allocBase));
      default:
         return 0; // TOFIX: use type api
   }
}

static F32 castValueToF32(KorkApi::ConsoleValue retValue, KorkApi::ConsoleValue::AllocBase& allocBase)
{
   switch (retValue.typeId)
   {
      case KorkApi::ConsoleValue::TypeInternalUnsigned:
         return (U32)retValue.getInt();
      case KorkApi::ConsoleValue::TypeInternalNumber:
         return (F32)retValue.getFloat();
      case KorkApi::ConsoleValue::TypeInternalString:
         return atoll((const char*)retValue.evaluatePtr(allocBase));
      default:
         return 0.0f; // TOFIX: use type api
   }
}

inline void* safeObjectUserPtr(KorkApi::VMObject* obj)
{
   return obj ? obj->userPtr : NULL;
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
      objectCreationStack[index].newObject = NULL;
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
         objectCreationStack[i].newObject = NULL;
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
   S32              setFrame)
{
   ConsoleFrame* newFrame = NULL;

   // --- Function call case (argv != nullptr) ---
   if (argv)
   {
      // assume this points into a function decl:
      U32 fnArgc = code[ip + 2 + 6];
      StringTableEntry fnName = Compiler::CodeToSTE(NULL, code, ip);
      S32 wantedArgc = getMin(argc - 1, fnArgc); // argv[0] is func name

      // Trace output
      if (eval.traceOn)
      {
         eval.traceBuffer[0] = 0;
         dStrcat(eval.traceBuffer, "Entering ");
         if (packageName)
         {
            dStrcat(eval.traceBuffer, "[");
            dStrcat(eval.traceBuffer, packageName);
            dStrcat(eval.traceBuffer, "]");
         }

         if (thisNamespace && thisNamespace->mName)
         {
            dSprintf(
               eval.traceBuffer + dStrlen(eval.traceBuffer),
               ExprEvalState::TraceBufferSize - dStrlen(eval.traceBuffer),
               "%s::%s(", thisNamespace->mName, fnName);
         }
         else
         {
            dSprintf(
               eval.traceBuffer + dStrlen(eval.traceBuffer),
               ExprEvalState::TraceBufferSize - dStrlen(eval.traceBuffer),
               "%s(", fnName);
         }

         for (U32 i = 0; i < wantedArgc; i++)
         {
            dStrcat(eval.traceBuffer, mVM->valueAsString(argv[i + 1]));
            if (i != wantedArgc - 1)
               dStrcat(eval.traceBuffer, ", ");
         }
         dStrcat(eval.traceBuffer, ")");
         mVM->printf(0, "%s", eval.traceBuffer);
      }

      // Push a new frame for the function call and get a ref to it.
      eval.pushFrame(fnName, thisNamespace, packageName, this, ip);
      newFrame = eval.vmFrames.last();
      newFrame->thisFunctionName = fnName;

      // Bind arguments into the new frame's locals
      for (U32 i = 0; i < (U32)wantedArgc; i++)
      {
         StringTableEntry var =
            Compiler::CodeToSTE(NULL, code, ip + (2 + 6 + 1) + (i * 2));
         newFrame->setCurVarNameCreate(var);
         newFrame->setConsoleValue(argv[i + 1]);
      }

      ip = ip + (fnArgc * 2) + (2 + 6 + 1);

      newFrame->curFloatTable     = functionFloats;
      newFrame->curStringTable    = functionStrings;
      //newFrame->curStringTableLen = functionStringsMaxLen;
   }
   else
   {
      // Do we want this code to execute using a new stack frame?
      if (setFrame < 0 || eval.vmFrames.empty())
      {
         // Always push a fresh frame
         eval.pushFrame(NULL, NULL, NULL, this, ip);
      }
      else
      {
         // Copy a reference to an existing stack frame onto the top of the stack.
         // Any change to locals during this new frame also affects the original frame.
         S32 stackIndex = eval.vmFrames.size() - setFrame - 1;
         eval.pushFrameRef(stackIndex);
      }
      
      newFrame = eval.vmFrames.last();
      newFrame->curFloatTable     = globalFloats;
      newFrame->curStringTable    = globalStrings;
      //newFrame->curStringTableLen = globalStringsMaxLen;
   }
   
   newFrame->ip = ip; // update ip
   return *newFrame;
}

U32 gExecCount = 0;
KorkApi::ConsoleValue CodeBlock::exec(U32 ip, const char *functionName, Namespace *thisNamespace, U32 argc,  KorkApi::ConsoleValue* argv, bool noCalls, StringTableEntry packageName, S32 setFrame, bool startSuspended)
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
                                        packageName,
                                        setFrame);
   
   if (frame && !startSuspended)
   {
      KorkApi::FiberRunResult result = evalState.runVM();
      return result.yieldValue;
   }
   
   return KorkApi::ConsoleValue();
}

ConsoleFrame* CodeBlock::beginExec(ExprEvalState& evalState, U32 ip, const char *functionName, Namespace *thisNamespace, U32 argc,  KorkApi::ConsoleValue* argv, bool noCalls, StringTableEntry packageName, S32 setFrame)
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
                                        setFrame);
   
   frame.stackStart = evalState.mSTR.mStartStackSize;
   frame.noCalls = noCalls;
   
   // TOFIX: this needs to push frame info too
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
   
#ifdef TORQUE_DEBUG
   gExecCount++;
#endif
   
   if (vmFrames.size() == 0)
   {
      return KorkApi::FiberRunResult();
   }
   
   KorkApi::ConsoleValue val = KorkApi::ConsoleValue();
   U32 startFrameSize = vmFrames.size();
   
   KorkApi::FiberRunResult result;
   result.state = mState;
   result.yieldValue = KorkApi::ConsoleValue();
   
   FIBERS_START
   
   // NOTE: these are only used temporarily inside opcode cases
   StringTableEntry tmpVar = NULL;
   StringTableEntry tmpObjParent = NULL;
   StringTableEntry tmpFnName = NULL;
   StringTableEntry tmpFnNamespace = NULL;
   StringTableEntry tmpFnPackage = NULL;
   
   ConsoleFrame& frame = *vmFrames.last();
   ExprEvalState& evalState = *this;
   U32 ip = frame.ip;
   U32* code = frame.codeBlock->code;
   KorkApi::Vm* vmPublic = vmInternal->mVM;
   const bool noCalls = frame.noCalls;
   Namespace* thisNamespace = frame.scopeNamespace;
   StringTableEntry packageName = frame.scopePackage;
   bool loopFrameSetup = false;
   
   // Ensure we are using correct codeblock (NOTE: refcount is handled by frame push)
   vmInternal->mCurrentCodeBlock = frame.codeBlock;
   if(frame.codeBlock->name)
   {
      // TOFIX
      //evalState.mCurrentFile = frame.codeBlock->name;
      //evalState.mCurrentRoot = vmInternal->mRoot;
   }
   
   // If we came from a native function, process it
   if (frame.inNativeFunction)
   {
      // NOTE: result of the function should be in ExprEvalState mLastYield
      evalState.mSTR.popFrame();
      
      if ((frame.nsEntry->mType == Namespace::Entry::VoidCallbackType) && (code[ip] != OP_STR_TO_NONE))
      {
         vmInternal->printf(0, "%s: Call to %s in %s uses result of void function call.", frame.codeBlock->getFileLine(ip-4), tmpFnName, frame.scopeName);
      }
      
      if(code[ip] == OP_STR_TO_UINT)
      {
         ip++;
         evalState.intStack[++frame._UINT] = vmInternal->valueAsInt(mLastYield);
      }
      else if(code[ip] == OP_STR_TO_FLT)
      {
         ip++;
         evalState.floatStack[++frame._FLT] = vmInternal->valueAsFloat(mLastYield);
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
         evalState.mSTR.setStringValue(vmInternal->valueAsString(mLastYield));
      }
      
      if(frame.lastCallType == FuncCallExprNode::MethodCall)
         frame.thisObject = frame.saveObject;
      
      frame.inNativeFunction = false;
      frame.ip = ip;
   }
   
   for(;;)
   {
      U32 instruction = code[ip++];
      
   breakContinue:
      switch(instruction)
      {
         case OP_FUNC_DECL:
            if(!noCalls)
            {
               tmpFnName       = Compiler::CodeToSTE(NULL, code, ip);
               tmpFnNamespace  = Compiler::CodeToSTE(NULL, code, ip+2);
               tmpFnPackage    = Compiler::CodeToSTE(NULL, code, ip+4);
               bool hasBody = ( code[ ip + 6 ] & 0x01 ) != 0;
               U32 lineNumber = code[ ip + 6 ] >> 1;
               
               vmInternal->mNSState.unlinkPackages();
               frame.ns = vmInternal->mNSState.find(tmpFnNamespace, tmpFnPackage);
               frame.ns->addFunction(tmpFnName, frame.codeBlock, hasBody ? ip : 0 );// if no body, set the IP to 0
               if( frame.curNSDocBlock )
               {
                  if( tmpFnNamespace == StringTable->lookup( frame.nsDocBlockClass ) )
                  {
                     char *usageStr = dStrdup( frame.curNSDocBlock );
                     usageStr[dStrlen(usageStr)] = '\0';
                     frame.ns->mUsage = usageStr;
                     frame.ns->mCleanUpUsage = true;
                     frame.curNSDocBlock = NULL;
                  }
               }
               vmInternal->mNSState.relinkPackages();
               
               // If we had a docblock, it's definitely not valid anymore, so clear it out.
               frame.curFNDocBlock = NULL;
               
               //Con::printf("Adding function %s::%s (%d)", fnNamespace, fnName, ip);
            }
            ip = code[ip + 7];
            break;
            
         case OP_CREATE_OBJECT:
         {
            // Read some useful info.
            tmpObjParent        = Compiler::CodeToSTE(NULL, code, ip);
            bool isDataBlock =          code[ip + 2];
            bool isInternal  =          code[ip + 3];
            bool isSingleton =          code[ip + 4];
            U32  lineNumber  =          code[ip + 5];
            frame.failJump         =          code[ip + 6];
            
            // If we don't allow calls, we certainly don't allow creating objects!
            // Moved this to after frame.failJump is set. Engine was crashing when
            // noCalls = true and an object was being created at the beginning of
            // a file. ADL.
            if(noCalls)
            {
               ip = frame.failJump;
               break;
            }
            
            // Push the old info to the stack
            //Assert( objectCreationStackIndex < objectCreationStackSize );

            evalState.setCreatedObject(frame._OBJ++, frame.currentNewObject, frame.failJump);
            
            // Get the constructor information off the stack.
            evalState.mSTR.getArgcArgv(NULL, &frame.callArgc, &frame.callArgv);
            evalState.mSTR.convertArgv(vmInternal, frame.callArgc, &frame.callArgvS);
            const char *objectName = frame.callArgvS[ 2 ];
            
            // Con::printf("Creating object...");
            
            // objectName = argv[1]...
            frame.currentNewObject = NULL;
            
            // Are we creating a datablock? If so, deal with case where we override
            // an old one.
            if(isDataBlock)
            {
               // Con::printf("  - is a datablock");
               
               // Find the old one if any.
               KorkApi::VMObject *db = vmInternal->mConfig.iFind.FindDatablockGroup(vmInternal->mConfig.findUser);
               
               // Make sure we're not changing types on ourselves...
               if(db && dStricmp(db->klass->name, frame.callArgvS[1]))
               {
                  vmInternal->printf(0, "Cannot re-declare data block %s with a different class.", frame.callArgv[2]);
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
                  // --
                  
                  oldObject->klass->iCreate.RemoveObjectFn(oldObject->klass->userPtr, vmPublic, oldObject);
                  oldObject->klass->iCreate.DestroyClassFn(oldObject->klass->userPtr, vmPublic, oldObject->userPtr);
                  
                  oldObject = NULL;

                  // Prevent stack value corruption
                  evalState.mSTR.popFrame();
               }
            }
            
            evalState.mSTR.popFrame();
            
            if(!frame.currentNewObject.isValid())
            {
               // Well, looks like we have to create a new object.
               KorkApi::ClassInfo* klassInfo = vmInternal->getClassInfoByName(StringTable->insert(frame.callArgvS[1]));
               KorkApi::VMObject *object = NULL;

               if (klassInfo)
               {
                  object = new KorkApi::VMObject();
                  object->klass = klassInfo;
                  object->ns = NULL;

                  KorkApi::CreateClassReturn ret = {};
                  klassInfo->iCreate.CreateClassFn(klassInfo->userPtr,  vmPublic, &ret);
                  object->userPtr = ret.userPtr;
                  object->flags = ret.initialFlags;

                  if (object->userPtr == NULL)
                  {
                     delete object;
                     object = NULL;
                  }
               }
               
               // Deal with failure!
               if(!object)
               {
                  vmInternal->printf(0, "%s: Unable to instantiate non-conobject class %s.", frame.codeBlock->getFileLine(ip-1), frame.callArgvS[1]);
                  ip = frame.failJump;
                  break;
               }
               
               // Finally, set currentNewObject to point to the new one.
               frame.currentNewObject = object;
               
               // Deal with the case of a non-SimObject.
               if(!frame.currentNewObject.isValid())
               {
                  vmInternal->printf(0, "%s: Unable to instantiate non-SimObject class %s.", frame.codeBlock->getFileLine(ip-1), frame.callArgvS[1]);
                  delete object;
                  ip = frame.failJump;
                  break;
               }

               if (*tmpObjParent)
               {
                  // Find it!
                  KorkApi::VMObject *parent = vmInternal->mConfig.iFind.FindObjectByNameFn(vmInternal->mConfig.findUser, tmpObjParent, NULL);
                  if (parent)
                  {
                     // Con::printf(" - Parent object found: %s", parent->getClassName());
                     
                     vmInternal->assignFieldsFromTo(parent, frame.currentNewObject);
                  }
                  else
                  {
                     vmInternal->printf(0, "%s: Unable to find parent object %s for %s.", frame.codeBlock->getFileLine(ip-1), tmpObjParent, frame.callArgvS[1]);
                  }
               }

               if (!klassInfo->iCreate.ProcessArgsFn(vmPublic, frame.currentNewObject->userPtr, objectName, isDataBlock, isInternal, frame.callArgc-3, frame.callArgvS+3))
               {
                  frame.currentNewObject = NULL;
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
            frame.curFNDocBlock = NULL;
            frame.curNSDocBlock = NULL;
            
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
               /* TOFIX vmInternal->printf(0, "%s: Register object failed for object %s of class %s.", getFileLine(ip-2), currentNewObject->getName(), currentNewObject->getClassName());*/

               // NOTE: AddObject may have "unregistered" the object, but since we refcount our objects this is still safe.
               frame.currentNewObject->klass->iCreate.DestroyClassFn(frame.currentNewObject->klass->userPtr, vmPublic, frame.currentNewObject->userPtr);
               frame.currentNewObject = NULL;
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

            evalState.clearCreatedObjects(frame._STARTOBJ, frame._OBJ);

            if ( frame._ITER > 0 )
            {
               // Clear iterator state.
               while ( frame._ITER > 0 )
               {
                  IterStackRecord& iter = evalState.iterStack[ -- frame._ITER ];
                  if (iter.mData.mObj.mSet)
                  {
                     vmInternal->decVMRef(iter.mData.mObj.mSet);
                     iter.mData.mObj.mSet = NULL;
                  }
                  iter.mIsStringIter = false;
               }
               
               const char* returnValue = evalState.mSTR.getStringValue();
               evalState.mSTR.rewind();
               evalState.mSTR.setStringValue( returnValue ); // Not nice but works.
            }
               
            goto execFinished;

         case OP_RETURN_FLT:

            evalState.clearCreatedObjects(frame._STARTOBJ, frame._OBJ);
         
            if( frame._ITER > 0 )
            {
               // Clear iterator state.
               while( frame._ITER > 0 )
               {
                  IterStackRecord& iter = evalState.iterStack[ -- frame._ITER ];
                  if (iter.mData.mObj.mSet)
                  {
                     vmInternal->decVMRef(iter.mData.mObj.mSet);
                     iter.mData.mObj.mSet = NULL;
                  }
                  iter.mIsStringIter = false;
               }
               
            }

            evalState.mSTR.setNumberValue( evalState.floatStack[frame._FLT] );
            frame._FLT--;
               
            goto execFinished;

         case OP_RETURN_UINT:

            evalState.clearCreatedObjects(frame._STARTOBJ, frame._OBJ);
         
            if( frame._ITER > 0 )
            {
               // Clear iterator state.
               while( frame._ITER > 0 )
               {
                  IterStackRecord& iter = evalState.iterStack[ -- frame._ITER ];
                  if (iter.mData.mObj.mSet)
                  {
                     vmInternal->decVMRef(iter.mData.mObj.mSet);
                     iter.mData.mObj.mSet = NULL;
                  }
                  iter.mIsStringIter = false;
               }
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
            tmpVar = Compiler::CodeToSTE(NULL, code, ip);
            ip += 2;
            
            // If a variable is set, then these must be NULL. It is necessary
            // to set this here so that the vector parser can appropriately
            // identify whether it's dealing with a vector.
            frame.prevField = NULL;
            frame.prevObject = NULL;
            frame.curObject = NULL;
            
            frame.setCurVarName(tmpVar);
            
            // In order to let docblocks work properly with variables, we have
            // clear the current docblock when we do an assign. This way it
            // won't inappropriately carry forward to following function decls.
            frame.curFNDocBlock = NULL;
            frame.curNSDocBlock = NULL;
            break;
            
         case OP_SETCURVAR_CREATE:
            tmpVar = Compiler::CodeToSTE(NULL, code, ip);
            ip += 2;
            
            // See OP_SETCURVAR
            frame.prevField = NULL;
            frame.prevObject = NULL;
            frame.curObject = NULL;
            
            frame.setCurVarNameCreate(tmpVar);
            
            // See OP_SETCURVAR for why we do this.
            frame.curFNDocBlock = NULL;
            frame.curNSDocBlock = NULL;
            break;
            
         case OP_SETCURVAR_ARRAY:
            tmpVar = evalState.mSTR.getSTValue();
            
            // See OP_SETCURVAR
            frame.prevField = NULL;
            frame.prevObject = NULL;
            frame.curObject = NULL;
            
            frame.setCurVarName(tmpVar);
            
            // See OP_SETCURVAR for why we do this.
            frame.curFNDocBlock = NULL;
            frame.curNSDocBlock = NULL;
            break;
            
         case OP_SETCURVAR_ARRAY_CREATE:
            tmpVar = evalState.mSTR.getSTValue();
            
            // See OP_SETCURVAR
            frame.prevField = NULL;
            frame.prevObject = NULL;
            frame.curObject = NULL;
            
            frame.setCurVarNameCreate(tmpVar);
            
            // See OP_SETCURVAR for why we do this.
            frame.curFNDocBlock = NULL;
            frame.curNSDocBlock = NULL;
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
            val = frame.getConsoleVariable();
            evalState.mSTR.setStringValue(vmInternal->valueAsString(val));
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
            val = evalState.mSTR.getConsoleValue();
            {
               const char* findPath = vmInternal->valueAsString(val);

               // Sim::findObject will sometimes find valid objects from
               // multi-component strings. This makes sure that doesn't
               // happen.
               if (val.isString())
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
               StringTableEntry intName = StringTable->insert(evalState.mSTR.getStringValue());
               bool recurse = code[ip-1];
               KorkApi::VMObject* obj = vmInternal->mConfig.iFind.FindObjectByInternalNameFn(vmInternal->mConfig.findUser, intName, recurse, frame.curObject);
               evalState.intStack[frame._UINT+1] = obj ? obj->klass->iCreate.GetIdFn(obj) : 0;
            }
            break;
            
         case OP_SETCUROBJECT_NEW:
            frame.curObject = frame.currentNewObject;
            break;
            
         case OP_SETCURFIELD:
            // Save the previous field for parsing vector fields.
            frame.prevField = frame.curField;
            dStrcpy( frame.prevFieldArray, frame.curFieldArray );
            frame.curField = Compiler::CodeToSTE(NULL, code, ip);
            frame.curFieldArray[0] = 0;
            ip += 2;
            break;
            
         case OP_SETCURFIELD_ARRAY:
            dStrcpy(frame.curFieldArray, evalState.mSTR.getStringValue());
            break;

         case OP_SETCURFIELD_TYPE:
            //if(curObject)
            //   frame.curObject->setDataFieldType(code[ip], curField, curFieldArray);
            ip++;
            break;
            
         case OP_LOADFIELD_UINT:
            if (frame.curObject)
            {
               KorkApi::ConsoleValue retValue = vmInternal->getObjectField(frame.curObject, frame.curField, frame.curFieldArray, KorkApi::ConsoleValue::TypeInternalUnsigned, KorkApi::ConsoleValue::ZoneExternal);
               evalState.intStack[frame._UINT+1] = castValueToU32(retValue, vmInternal->mAllocBase);
            }
            else
            {
               // The field is not being retrieved from an object. Maybe it's
               // a special accessor?
               
               //getFieldComponent( prevObject, prevField, prevFieldArray, curField, valBuffer, VAL_BUFFER_SIZE );
               evalState.intStack[frame._UINT+1] = 0;//dAtoi( valBuffer );
            }
            frame._UINT++;
            break;
            
         case OP_LOADFIELD_FLT:
            if (frame.curObject)
            {
               KorkApi::ConsoleValue retValue =  vmInternal->getObjectField(frame.curObject, frame.curField, frame.curFieldArray, KorkApi::ConsoleValue::TypeInternalNumber, KorkApi::ConsoleValue::ZoneExternal);
               evalState.floatStack[frame._FLT+1] = castValueToF32(retValue, vmInternal->mAllocBase);
            }
            else
            {
               // The field is not being retrieved from an object. Maybe it's
               // a special accessor?
               //getFieldComponent( prevObject, prevField, prevFieldArray, curField, valBuffer, VAL_BUFFER_SIZE );
               evalState.floatStack[frame._FLT+1] = 0.0f;//dAtof( valBuffer );
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
               frame.prevObject = NULL;
            }
            break;
            
         case OP_SAVEFIELD_FLT:
            evalState.mSTR.setNumberValue(evalState.floatStack[frame._FLT]);
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
               frame.prevObject = NULL;
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
               frame.prevObject = NULL;
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
            frame.copyVar.var = NULL;
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
            code[ip-1] = OP_LOADIMMED_STR;
            // it's possible the string has already been converted
            if(U8(frame.curStringTable[code[ip]]) != KorkApi::StringTagPrefixByte)
            {
               U32 id = vmInternal->mConfig.addTagFn(frame.curStringTable + code[ip], vmInternal->mConfig.addTagUser);
               dSprintf(frame.curStringTable + code[ip] + 1, 7, "%d", id);
               *(frame.curStringTable + code[ip]) = KorkApi::StringTagPrefixByte;
            }
         case OP_LOADIMMED_STR:
            evalState.mSTR.setStringValue(frame.curStringTable + code[ip++]);
            break;
            
         case OP_DOCBLOCK_STR:
         {
            // If the first word of the doc is '\class' or '@class', then this
            // is a namespace doc block, otherwise it is a function doc block.
            const char* docblock = frame.curStringTable + code[ip++];
            
            const char* sansClass = dStrstr( docblock, "@class" );
            if( !sansClass )
               sansClass = dStrstr( docblock, "\\class" );
            
            if( sansClass )
            {
               // Don't save the class declaration. Scan past the 'class'
               // keyword and up to the first whitespace.
               sansClass += 7;
               S32 index = 0;
               while( ( *sansClass != ' ' ) && ( *sansClass != '\n' ) && *sansClass && ( index < ( ConsoleFrame::NSDocLength - 1 ) ) )
               {
                  frame.nsDocBlockClass[index++] = *sansClass;
                  sansClass++;
               }
               frame.nsDocBlockClass[index] = '\0';
               
               frame.curNSDocBlock = sansClass + 1;
            }
            else
               frame.curFNDocBlock = docblock;
         }
            
            break;
            
         case OP_LOADIMMED_IDENT:
            evalState.mSTR.setStringValue(Compiler::CodeToSTE(NULL, code, ip));
            ip += 2;
            break;
            
         case OP_CALLFUNC_RESOLVE:
            // This deals with a function that is potentially living in a namespace.
            tmpFnNamespace = Compiler::CodeToSTE(NULL, code, ip+2);
            tmpFnName      = Compiler::CodeToSTE(NULL, code, ip);
            
            // Try to look it up.
            frame.ns = vmInternal->mNSState.find(tmpFnNamespace);
            frame.nsEntry = frame.ns->lookup(tmpFnName);
            if(!frame.nsEntry)
            {
               ip+= 5;
               vmInternal->printf(0,
                          "%s: Unable to find function %s%s%s",
                          frame.codeBlock->getFileLine(ip-4), tmpFnNamespace ? tmpFnNamespace : "",
                           tmpFnNamespace ? "::" : "", tmpFnName);
               evalState.mSTR.popFrame();
               break;
            }
            // Now, rewrite our code a bit (ie, avoid future lookups) and fall
            // through to OP_CALLFUNC
#ifdef TORQUE_64
            *((U64*)(code+ip+2)) = ((U64)frame.nsEntry);
#else
            code[ip+2] = ((U32)nsEntry);
#endif
            code[ip-1] = OP_CALLFUNC;
            
         case OP_CALLFUNC:
         {
            // This routingId is set when we query the object as to whether
            // it handles this method.  It is set to an enum from the table
            // above indicating whether it handles it on a component it owns
            // or just on the object.
            S32 routingId = 0;
            
            tmpFnName = Compiler::CodeToSTE(NULL, code, ip);
            
            //if this is called from inside a function, append the ip and codeptr
            if (!evalState.vmFrames.empty())
            {
               evalState.vmFrames.last()->codeBlock = frame.codeBlock;
               evalState.vmFrames.last()->ip = ip - 1;
            }
            
            frame.lastCallType = code[ip+4];
            
            ip += 5;
            frame.ip = ip; // sync ip with frame
            evalState.mSTR.getArgcArgv(tmpFnName, &frame.callArgc, &frame.callArgv);
            
            if(frame.lastCallType == FuncCallExprNode::FunctionCall)
            {
#ifdef TORQUE_64
               frame.nsEntry = ((Namespace::Entry *) *((U64*)(code+ip-3)));
#else
               frame.nsEntry = ((Namespace::Entry *) *(code+ip-3));
#endif
               frame.ns = NULL;
            }
            else if(frame.lastCallType == FuncCallExprNode::MethodCall)
            {
               frame.saveObject = frame.thisObject;
               const char* objName = vmInternal->valueAsString(frame.callArgv[1]);
               frame.thisObject = vmInternal->mConfig.iFind.FindObjectByPathFn(vmInternal->mConfig.findUser, objName);
               
               if(!frame.thisObject)
               {
                  frame.thisObject = 0;
                  vmInternal->printf(0,"%s: Unable to find object: '%s' attempting to call function '%s'", frame.codeBlock->getFileLine(ip-6), objName, tmpFnName);
                  evalState.mSTR.popFrame();
                  evalState.mSTR.setStringValue("");
                  break;
               }
               
               frame.ns = frame.thisObject->ns;
               if(frame.ns)
                  frame.nsEntry = frame.ns->lookup(tmpFnName);
               else
                  frame.nsEntry = NULL;
            }
            else // it's a ParentCall
            {
               if(thisNamespace)
               {
                  frame.ns = thisNamespace->mParent;
                  if(frame.ns)
                     frame.nsEntry = frame.ns->lookup(tmpFnName);
                  else
                     frame.nsEntry = NULL;
               }
               else
               {
                  frame.ns = NULL;
                  frame.nsEntry = NULL;
               }
            }
            
            if(!frame.nsEntry || noCalls)
            {
               if(!noCalls)
               {
                  vmInternal->printf(0,"%s: Unknown command %s.", frame.codeBlock->getFileLine(ip-4), tmpFnName);
                  if(frame.lastCallType == FuncCallExprNode::MethodCall)
                  {
                     /* TOFIXvmInternal->printf(0, "  Object %s(%d) %s",
                                frame.thisObject->getName() ? frame.thisObject->getName() : "",
                                frame.thisObject->getId(), getNamespaceList(ns) ); */
                  }
               }
               evalState.mSTR.popFrame();
               evalState.mSTR.setStringValue("");
               evalState.mSTR.setStringValue("");
               break;
            }
            
            if(frame.nsEntry->mType == Namespace::Entry::ScriptFunctionType)
            {
               KorkApi::ConsoleValue ret = KorkApi::ConsoleValue();
               
               // NOTE: script opcodes should have pushed a frame thus the need to pop it either
               // here or in execFinished
               
               if(frame.nsEntry->mFunctionOffset)
               {
                  ConsoleFrame* newFrame = frame.nsEntry->mCode->beginExec(evalState,
                                                                           frame.nsEntry->mFunctionOffset,
                                                                           tmpFnName,
                                                                           frame.nsEntry->mNamespace,
                                                                           frame.callArgc,
                                                                           frame.callArgv,
                                                                           false,
                                                                           frame.nsEntry->mPackage);
                  
                  // NOTE: "frame" is now invalidated
                  if (newFrame)
                  {
                     newFrame->popStringStack = true; // i.e. we need to do this after frame pops
                     loopFrameSetup = true;
                     goto execFinished;
                  }
               }
               
               // No body fallthrough case
               evalState.mSTR.popFrame();
               evalState.mSTR.setStringValue("");
            }
            else
            {
               if((frame.nsEntry->mMinArgs && S32(frame.callArgc) < frame.nsEntry->mMinArgs) || (frame.nsEntry->mMaxArgs && S32(frame.callArgc) > frame.nsEntry->mMaxArgs))
               {
                  const char* nsName = frame.ns? frame.ns->mName: "";
                  vmInternal->printf(0, "%s: %s::%s - wrong number of arguments.", frame.codeBlock->getFileLine(ip-4), nsName, tmpFnName);
                  vmInternal->printf(0, "%s: usage: %s", frame.codeBlock->getFileLine(ip-4), frame.nsEntry->mUsage);
                  evalState.mSTR.popFrame();
                  evalState.mSTR.setStringValue("");
               }
               else
               {
                  if (frame.nsEntry->mType != Namespace::Entry::ValueCallbackType)
                  {
                     // Need to convert to strings for old callbacks
                     //printf("Converting %i argv calling %s\n", callArgc, fnName);
                     evalState.mSTR.convertArgv(vmInternal, frame.callArgc, &frame.callArgvS);
                  }
                  
                  // Handle calling
                  
                  switch(frame.nsEntry->mType)
                  {
                     case Namespace::Entry::StringCallbackType:
                     {
                        const char *ret = frame.nsEntry->cb.mStringCallbackFunc(safeObjectUserPtr(frame.thisObject), frame.nsEntry->mUserPtr, frame.callArgc, frame.callArgvS);
                        if (mState != KorkApi::FiberRunResult::RUNNING)
                        {
                           vmInternal->printf(0,"String function yielded, ignoring result");
                           mLastYield = KorkApi::ConsoleValue();
                        }
                        else
                        {
                           mLastYield = KorkApi::ConsoleValue::makeString(ret); // NOTE: none of these should yield
                        }
                        goto execFinished;
                     }
                     case Namespace::Entry::IntCallbackType:
                     {
                        S32 result = frame.nsEntry->cb.mIntCallbackFunc(safeObjectUserPtr(frame.thisObject), frame.nsEntry->mUserPtr, frame.callArgc, frame.callArgvS);
                        mLastYield = KorkApi::ConsoleValue::makeNumber(result);
                        frame.inNativeFunction = true;
                        loopFrameSetup = true;
                        goto execFinished;
                     }
                     case Namespace::Entry::FloatCallbackType:
                     {
                        F64 result = frame.nsEntry->cb.mFloatCallbackFunc(safeObjectUserPtr(frame.thisObject), frame.nsEntry->mUserPtr, frame.callArgc, frame.callArgvS);
                        mLastYield = KorkApi::ConsoleValue::makeNumber(result);
                        frame.inNativeFunction = true;
                        loopFrameSetup = true;
                        goto execFinished;
                     }
                     case Namespace::Entry::VoidCallbackType:
                     {
                        frame.nsEntry->cb.mVoidCallbackFunc(safeObjectUserPtr(frame.thisObject), frame.nsEntry->mUserPtr, frame.callArgc, frame.callArgvS);
                        mLastYield = KorkApi::ConsoleValue();
                        frame.inNativeFunction = true;
                        loopFrameSetup = true;
                        goto execFinished;
                     }
                     case Namespace::Entry::BoolCallbackType:
                     {
                        bool result = frame.nsEntry->cb.mBoolCallbackFunc(safeObjectUserPtr(frame.thisObject), frame.nsEntry->mUserPtr, frame.callArgc, frame.callArgvS);
                        mLastYield = KorkApi::ConsoleValue::makeUnsigned(result);
                        frame.inNativeFunction = true;
                        loopFrameSetup = true;
                        goto execFinished;
                     }
                     case Namespace::Entry::ValueCallbackType:
                     {
                        mLastYield = frame.nsEntry->cb.mValueCallbackFunc(safeObjectUserPtr(frame.thisObject), frame.nsEntry->mUserPtr, frame.callArgc, frame.callArgv);
                        frame.inNativeFunction = true;
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
            val = frame.getConsoleVariable();
            evalState.mSTR.setConsoleValue(val);
            evalState.mSTR.push();
            break;

         case OP_PUSH_FRAME:
            evalState.mSTR.pushFrame();
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
            AssertFatal( !frame.stack.empty(), "Empty eval stack on break!");
            evalState.vmFrames.last()->codeBlock = frame.codeBlock;
            evalState.vmFrames.last()->ip = ip - 1;
            
            U32 breakLine;
            frame.codeBlock->findBreakLine(ip-1, breakLine, instruction);
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
            StringTableEntry varName = Compiler::CodeToSTE(NULL, code, ip);
            U32 failIp = code[ ip + 2 ];
            
            IterStackRecord& iter = evalState.iterStack[ frame._ITER ];
            
            iter.mVariable = evalState.getCurrentFrame().dictionary->add( varName );
            iter.mDictionary = evalState.getCurrentFrame().dictionary;
            
            if( iter.mIsStringIter )
            {
               iter.mData.mStr.mString = evalState.mSTR.getStringValue();
               iter.mData.mStr.mIndex = 0;
            }
            else
            {
               // Look up the object.
               KorkApi::VMObject* set = vmInternal->mConfig.iFind.FindObjectByPathFn(vmInternal->mConfig.findUser, evalState.mSTR.getStringValue());
               
               if( !set )
               {
                  vmInternal->printf(0, "No SimSet object '%s'", evalState.mSTR.getStringValue());
                  vmInternal->printf(0, "Did you mean to use 'foreach$' instead of 'foreach'?");
                  ip = failIp;
                  continue;
               }
               
               // Set up.

               AssertFatal(iter.mData.mObj.mSet == NULL, "Should be NULL");

               vmInternal->incVMRef(set);
               iter.mData.mObj.mSet = set;
               iter.mData.mObj.mIndex = 0;
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
            
            if( iter.mIsStringIter )
            {
               const char* str = iter.mData.mStr.mString;
                              
               U32 startIndex = iter.mData.mStr.mIndex;
               U32 endIndex = startIndex;
               
               // Break if at end.

               if( !str[ startIndex ] )
               {
                  ip = breakIp;
                  continue;
               }

               // Find right end of current component.
               
               if( !dIsspace( str[ endIndex ] ) )
                  do ++ endIndex;
                  while( str[ endIndex ] && !dIsspace( str[ endIndex ] ) );
                  
               // Extract component.
                  
               if( endIndex != startIndex )
               {
                  char savedChar = str[ endIndex ];
                  const_cast< char* >( str )[ endIndex ] = '\0'; // We are on the string stack so this is okay.
                  iter.mDictionary->setEntryStringValue(iter.mVariable, &str[ startIndex ] );
                  const_cast< char* >( str )[ endIndex ] = savedChar;
               }
               else
                  iter.mDictionary->setEntryStringValue( iter.mVariable, "" );

               // Skip separator.
               if( str[ endIndex ] != '\0' )
                  ++ endIndex;
               
               iter.mData.mStr.mIndex = endIndex;
            }
            else
            {
               U32 index = iter.mData.mObj.mIndex;
               KorkApi::VMObject* set = iter.mData.mObj.mSet;
               
               if( index >= set->klass->iEnum.GetSize(set) )
               {
                  if (set)
                  {
                     vmInternal->decVMRef(set);
                     iter.mData.mObj.mSet = NULL;
                  }
                  ip = breakIp;
                  continue;
               }
               
               KorkApi::VMObject* atObject = set->klass->iEnum.GetObjectAtIndex(set, index);
               iter.mDictionary->setEntryUnsignedValue(iter.mVariable, atObject ? atObject->klass->iCreate.GetIdFn(atObject) : 0);
               iter.mData.mObj.mIndex = index + 1;
            }
            
            ++ ip;
            break;
         }
         
         case OP_ITER_END:
         {
            -- frame._ITER;
            IterStackRecord& iter = evalState.iterStack[frame._ITER];

            if (iter.mData.mObj.mSet)
            {
               vmInternal->decVMRef(iter.mData.mObj.mSet);
               iter.mData.mObj.mSet = NULL;
            }
            

            evalState.mSTR.rewind();
            
            evalState.iterStack[ frame._ITER ].mIsStringIter = false;
            break;
         }

         case OP_INVALID:
            
         default:
            // error!
            goto execFinished;
      }
   }
   
   // If user has opted to suspend thread, bail out here
   if (mState == KorkApi::FiberRunResult::SUSPENDED)
   {
      frame.ip = ip;
      result.state = mState;
      // TOFIX result.yieldValue = evalState.mSTR.getConsoleValue();
      return result;
   }
      
execFinished:
   
   // Do we need to setup a new frame? if so skip back to start
   if (loopFrameSetup)
   {
      continue;
   }
   
   // Update ip in the event we need to know where we jumped ship
   frame.ip = ip;
   
   // Update the yield value (which is now basically the return value i.e. head of stack)
   result.state = mState;
   result.yieldValue = evalState.mSTR.getConsoleValue();
   
   // Stuff which follows is normally done when function exits...
   
   evalState.clearCreatedObjects(frame._STARTOBJ, frame._OBJ);
   
   const bool telDebuggerOn = vmInternal->mTelDebugger && vmInternal->mTelDebugger->isConnected();
   if ( telDebuggerOn && !frame.isReference )
      vmInternal->mTelDebugger->popStackFrame();
   
   // NOTE: previously happened in the ScriptFunctionType conditional, now tied to frame lifetime
   if (frame.popStringStack)
   {
      evalState.mSTR.popFrame();
      // We assume here this is a function call; result should be moved to the new head stack pointer
      evalState.mSTR.setStringValue(vmInternal->valueAsString(result.yieldValue));
   }
   
   // ALWAYS pop the frame in this case we are done with it
   evalState.popFrame();
   
   // Basically: If we are still inside a script frame, keep looping
   
   if (evalState.vmFrames.size() == startFrameSize-1) // we always start at +1 in this case so it needs to go below
   {
      FIBER_STATE(KorkApi::FiberRunResult::FINISHED);
   }
   else if (evalState.vmFrames.size() < startFrameSize)
   {
      AssertFatal(false, "Invalid case");
      FIBER_STATE(KorkApi::FiberRunResult::ERROR);
      break;
   }
   else
   {
      FIBER_STATE(KorkApi::FiberRunResult::RUNNING);
   }
   
   if (frame.callArgv)
   {
      if(evalState.traceOn)
      {
         evalState.traceBuffer[0] = 0;
         dStrcat(evalState.traceBuffer, "Leaving ");
         
         if(packageName)
         {
            dStrcat(evalState.traceBuffer, "[");
            dStrcat(evalState.traceBuffer, packageName);
            dStrcat(evalState.traceBuffer, "]");
         }
         if(thisNamespace && thisNamespace->mName)
         {
            dSprintf(evalState.traceBuffer + dStrlen(evalState.traceBuffer), ExprEvalState::TraceBufferSize - dStrlen(evalState.traceBuffer),
                     "%s::%s() - return %s", thisNamespace->mName, frame.thisFunctionName, evalState.mSTR.getStringValue());
         }
         else
         {
            dSprintf(evalState.traceBuffer + dStrlen(evalState.traceBuffer), ExprEvalState::TraceBufferSize - dStrlen(evalState.traceBuffer),
                     "%s() - return %s", frame.thisFunctionName, evalState.mSTR.getStringValue());
         }
         vmInternal->printf(0, "%s", evalState.traceBuffer);
      }
   }
   // NOTE: we rely on the inc count stuff to clear strings and such
   
execCheck:
   
   FIBERS_END

#ifdef TORQUE_DEBUG
   // TOFIX
   //AssertFatal(!(evalState.mSTR.mStartStackSize > stackStart), "String stack not popped enough in script exec");
   //AssertFatal(!(evalState.mSTR.mStartStackSize < stackStart), "String stack popped too much in script exec");
#endif
   
   return result;
}

void ExprEvalState::setLocalFrameVariable(StringTableEntry name, KorkApi::ConsoleValue value)
{
   vmFrames.last()->dictionary->setVariableValue(name, value);
}

KorkApi::ConsoleValue ExprEvalState::getLocalFrameVariable(StringTableEntry name)
{
   Dictionary::Entry* e = vmFrames.last()->dictionary->getVariable(name);
   
   if (!e)
   {
      return KorkApi::ConsoleValue();
   }
   
   return vmFrames.last()->dictionary->getEntryValue(e);
}


void ExprEvalState::pushFrame(StringTableEntry frameName, Namespace *ns, StringTableEntry packageName, CodeBlock* block, U32 ip)
{
   ConsoleFrame *newFrame = new ConsoleFrame(vmInternal, this);
   newFrame->dictionary = new Dictionary(vmInternal);
   if (vmFrames.size() > 0)
   {
      newFrame->copyFrom(vmFrames.last(), false);
   }
   
   newFrame->scopeName = frameName;
   newFrame->scopePackage = packageName;
   newFrame->scopeNamespace = ns;
   
   newFrame->codeBlock = block;
   block->incRefCount();
   newFrame->ip = ip;
   vmFrames.push_back(newFrame);
   mStackDepth ++;
   //Con::printf("ExprEvalState::pushFrame");
}

void ExprEvalState::popFrame()
{
   ConsoleFrame *last = vmFrames.last();
   vmFrames.pop_back();
   
   if (last->codeBlock)
   {
      last->codeBlock->decRefCount();
   }
   
   if (vmFrames.size() > 0)
   {
      ConsoleFrame* prevFrame = vmFrames.last();
      AssertFatal(prevFrame->_FLT == last->_FLT && prevFrame->_UINT == last->_UINT && prevFrame->_ITER == last->_ITER, "Stack mismatch");
   }
   delete last->dictionary;
   delete last;
   mStackDepth --;
   //Con::printf("ExprEvalState::popFrame");
}

void ExprEvalState::pushFrameRef(S32 stackIndex)
{
   AssertFatal( stackIndex >= 0 && stackIndex < stack.size(), "You must be asking for a valid frame!" );
   ConsoleFrame *newFrame = new ConsoleFrame(vmInternal, this);
   newFrame->dictionary = new Dictionary(vmInternal, vmFrames[stackIndex]->dictionary);
   vmFrames.push_back(newFrame);
   newFrame->copyFrom(vmFrames[stackIndex], true);
   newFrame->codeBlock->incRefCount();
   newFrame->isReference = true;
   mStackDepth ++;
   //Con::printf("ExprEvalState::pushFrameRef");
}

void ExprEvalState::validate()
{
   AssertFatal( mStackDepth <= vmFrames.size(),
               "ExprEvalState::validate() - Stack depth pointing beyond last stack frame!" );
   
   for( U32 i = 0; i < vmFrames.size(); ++ i )
      vmFrames[ i ]->dictionary->validate();
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
   outF.scopeNamespace = frame.ns;
   outF.scopeName = frame.scopeName;
   outF.ip = frame.ip;
   return outF;
}

void ExprEvalState::suspend()
{
   if (mState == KorkApi::FiberRunResult::RUNNING)
   {
      mState = KorkApi::FiberRunResult::SUSPENDED;
      mLastYield = KorkApi::ConsoleValue();
   }
}

KorkApi::FiberRunResult ExprEvalState::resume(KorkApi::ConsoleValue value)
{
   if (mState == KorkApi::FiberRunResult::SUSPENDED)
   {
      mState = KorkApi::FiberRunResult::RUNNING;
      mLastYield = value;
      KorkApi::FiberRunResult result = runVM();
      return result;
   }
   
   return KorkApi::FiberRunResult();
}

//------------------------------------------------------------

