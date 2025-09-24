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


const char *ExprEvalState::getNamespaceList(Namespace *ns)
{
   U32 size = 1;
   Namespace * walk;
   for(walk = ns; walk; walk = walk->mParent)
      size += dStrlen(walk->mName) + 4;
   char *ret = (char*)vmInternal->getStringFuncBuffer(size).ptr();
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

inline void ExprEvalState::setCurVarName(StringTableEntry name)
{
   if(name[0] == '$')
   {
      currentVariable = globalVars.lookup(name);
      currentDictionary = &globalVars;
   }
   else if(stack.size())
   {
      currentVariable = stack.last()->lookup(name);
      currentDictionary = stack.last();
   }
   if(!currentVariable && vmInternal->mConfig.warnUndefinedScriptVariables)
   {
      vmInternal->printf(1, "Variable referenced before assignment: %s", name);
   }
}

inline void ExprEvalState::setCurVarNameCreate(StringTableEntry name)
{
   if(name[0] == '$')
   {
      currentVariable = globalVars.add(name);
      currentDictionary = &globalVars;
   }
   else if(stack.size())
   {
      currentVariable = stack.last()->add(name);
      currentDictionary = stack.last();
   }
   else
   {
      currentVariable = NULL;
      vmInternal->printf(1, "Accessing local variable in global scope... failed: %s", name);
   }
}

//------------------------------------------------------------

inline S32 ExprEvalState::getIntVariable()
{
   return currentVariable ? currentDictionary->getEntryIntValue(currentVariable) : 0;
}

inline F64 ExprEvalState::getFloatVariable()
{
   return currentVariable ? currentDictionary->getEntryFloatValue(currentVariable) : 0;
}

inline const char *ExprEvalState::getStringVariable()
{
   return currentVariable ? currentDictionary->getEntryStringValue(currentVariable) : 0;
}

//------------------------------------------------------------

inline void ExprEvalState::setIntVariable(S32 val)
{
   AssertFatal(currentVariable != NULL, "Invalid evaluator state - trying to set null variable!");
   currentDictionary->setEntryIntValue(currentVariable, val);
}

inline void ExprEvalState::setFloatVariable(F64 val)
{
   AssertFatal(currentVariable != NULL, "Invalid evaluator state - trying to set null variable!");
   currentDictionary->setEntryFloatValue(currentVariable, val);
}

inline void ExprEvalState::setStringVariable(const char *val)
{
   AssertFatal(currentVariable != NULL, "Invalid evaluator state - trying to set null variable!");
   currentDictionary->setEntryStringValue(currentVariable, val);
}

inline void ExprEvalState::setCopyVariable()
{
   if (copyVariable)
   {
      switch (copyVariable->mConsoleValue.typeId)
      {
         case KorkApi::ConsoleValue::TypeInternalInt:
            currentDictionary->setEntryIntValue(currentVariable, copyDictionary->getEntryIntValue(copyVariable));
         break;
         case KorkApi::ConsoleValue::TypeInternalFloat:
            currentDictionary->setEntryIntValue(currentVariable, copyDictionary->getEntryFloatValue(copyVariable));
         break;
         default:
            currentDictionary->setEntryStringValue(currentVariable, copyDictionary->getEntryStringValue(copyVariable));
         break;
      }
   }
   else if (currentVariable)
   {
      currentDictionary->setEntryStringValue(currentVariable, ""); // needs to be set to blank if copyVariable doesn't exist
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
      case KorkApi::ConsoleValue::TypeInternalInt:
         return (U32)retValue.getInt();
      case KorkApi::ConsoleValue::TypeInternalFloat:
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
      case KorkApi::ConsoleValue::TypeInternalInt:
         return (U32)retValue.getInt();
      case KorkApi::ConsoleValue::TypeInternalFloat:
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

struct ObjectCreationStack
{
   struct Item
   {
      KorkApi::VMObject* newObject;
      U32 failJump;
   };

   static const U32 objectCreationStackSize = 32;
   U32 index;
   Item stack[objectCreationStackSize];

   ObjectCreationStack() : index(0) {;}

   void push(KorkApi::VmInternal* vm, KorkApi::VMObject* object, U32 failJump)
   {
      stack[index].newObject = object;
      stack[index++].failJump = failJump;
      if (object)
      {
         vm->incVMRef(object);
      }
   }

   void pop(KorkApi::VmInternal* vm, LocalRefTrack& outTrack, U32* outJump)
   {
      if (index == 0)
      {
         return;
      }

      U32 realIndex = index-1;
      *outJump = stack[realIndex].failJump;
      KorkApi::VMObject* newObject = stack[realIndex].newObject;
      outTrack = newObject;
      
      if (newObject)
      {
         stack[realIndex].newObject = NULL;
         vm->decVMRef(newObject);
      }
   }

   void clear(KorkApi::VmInternal* vm)
   {
      for (U32 i=0; i<index; i++)
      {
         if (stack[i].newObject)
         {
            vm->decVMRef(stack[i].newObject);
            stack[i].newObject = NULL;
         }
      }
   }
};


U32 gExecCount = 0;
const char *CodeBlock::exec(U32 ip, const char *functionName, Namespace *thisNamespace, U32 argc, const char **argv, bool noCalls, StringTableEntry packageName, S32 setFrame)
{
   
#ifdef TORQUE_DEBUG
   U32 stackStart = mVM->mSTR.mStartStackSize;
   gExecCount++;
#endif
   
   static char traceBuffer[1024];
   U32 i;
   
   U32 iterDepth = 0;
   
   incRefCount();
   F64 *curFloatTable;
   char *curStringTable;
   S32 curStringTableLen = 0; //clint to ensure we dont overwrite it
   mVM->mSTR.clearFunctionOffset();
   StringTableEntry thisFunctionName = NULL;
   bool popFrame = false;
   if(argv)
   {
      // assume this points into a function decl:
      U32 fnArgc = code[ip + 2 + 6];
      thisFunctionName = Compiler::CodeToSTE(NULL, code, ip);
      S32 wantedArgc = getMin(argc-1, fnArgc); // argv[0] is func name
      if(mVM->mEvalState.traceOn)
      {
         traceBuffer[0] = 0;
         dStrcat(traceBuffer, "Entering ");
         if(packageName)
         {
            dStrcat(traceBuffer, "[");
            dStrcat(traceBuffer, packageName);
            dStrcat(traceBuffer, "]");
         }
         if(thisNamespace && thisNamespace->mName)
         {
            dSprintf(traceBuffer + dStrlen(traceBuffer), sizeof(traceBuffer) - dStrlen(traceBuffer),
                     "%s::%s(", thisNamespace->mName, thisFunctionName);
         }
         else
         {
            dSprintf(traceBuffer + dStrlen(traceBuffer), sizeof(traceBuffer) - dStrlen(traceBuffer),
                     "%s(", thisFunctionName);
         }
         for(i = 0; i < wantedArgc; i++)
         {
            dStrcat(traceBuffer, argv[i+1]);
            if(i != wantedArgc - 1)
               dStrcat(traceBuffer, ", ");
         }
         dStrcat(traceBuffer, ")");
         mVM->printf(0, "%s", traceBuffer);
      }
      mVM->mEvalState.pushFrame(thisFunctionName, thisNamespace);
      popFrame = true;
      for(i = 0; i < wantedArgc; i++)
      {
         StringTableEntry var = Compiler::CodeToSTE(NULL, code, ip + (2 + 6 + 1) + (i * 2));
         mVM->mEvalState.setCurVarNameCreate(var);
         mVM->mEvalState.setStringVariable(argv[i+1]);
      }
      ip = ip + (fnArgc * 2) + (2 + 6 + 1);
      curFloatTable = functionFloats;
      curStringTable = functionStrings;
      curStringTableLen = functionStringsMaxLen;
   }
   else
   {
      curFloatTable = globalFloats;
      curStringTable = globalStrings;
      curStringTableLen = globalStringsMaxLen;
      
      // Do we want this code to execute using a new stack frame?
      if (setFrame < 0)
      {
         mVM->mEvalState.pushFrame(NULL, NULL);
         popFrame = true;
      }
      else if (!mVM->mEvalState.stack.empty())
      {
         // We want to copy a reference to an existing stack frame
         // on to the top of the stack.  Any change that occurs to
         // the locals during this new frame will also occur in the
         // original frame.
         S32 stackIndex = mVM->mEvalState.stack.size() - setFrame - 1;
         mVM->mEvalState.pushFrameRef( stackIndex );
         popFrame = true;
      }
   }
   
   // Grab the state of the telenet debugger here once
   // so that the push and pop frames are always balanced.
   const bool telDebuggerOn = mVM->mTelDebugger && mVM->mTelDebugger->isConnected();
   if ( telDebuggerOn && setFrame < 0 )
      mVM->mTelDebugger->pushStackFrame();
   
   StringTableEntry var, objParent;
   U32 failJump;
   StringTableEntry fnName;
   StringTableEntry fnNamespace, fnPackage;
   
   ObjectCreationStack creationStack;
   
   LocalRefTrack currentNewObject(mVM);
   StringTableEntry prevField = NULL;
   StringTableEntry curField = NULL;
   LocalRefTrack prevObject(mVM);
   LocalRefTrack curObject(mVM);
   LocalRefTrack saveObject(mVM);
   Namespace::Entry *nsEntry;
   Namespace *ns;
   const char* curFNDocBlock = NULL;
   const char* curNSDocBlock = NULL;
   const S32 nsDocLength = 128;
   char nsDocBlockClass[nsDocLength];
   
   U32 callArgc;
   const char **callArgv;
   
   static char curFieldArray[256];
   static char prevFieldArray[256];
   
   CodeBlock *saveCodeBlock = mVM->mCurrentCodeBlock;
   mVM->mCurrentCodeBlock = this;
   if(this->name)
   {
      mVM->mCurrentFile = this->name;
      mVM->mCurrentRoot = mRoot;
   }
   const char * val;
   
   // The frame temp is used by the variable accessor ops (OP_SAVEFIELD_* and
   // OP_LOADFIELD_*) to store temporary values for the fields.
   //static S32 VAL_BUFFER_SIZE = 1024;
   //TempAlloc<char> valBuffer( VAL_BUFFER_SIZE );
   
   for(;;)
   {
      U32 instruction = code[ip++];
   breakContinue:
      switch(instruction)
      {
         case OP_FUNC_DECL:
            if(!noCalls)
            {
               fnName       = Compiler::CodeToSTE(NULL, code, ip);
               fnNamespace  = Compiler::CodeToSTE(NULL, code, ip+2);
               fnPackage    = Compiler::CodeToSTE(NULL, code, ip+4);
               bool hasBody = ( code[ ip + 6 ] & 0x01 ) != 0;
               U32 lineNumber = code[ ip + 6 ] >> 1;
               
               mVM->mNSState.unlinkPackages();
               ns = mVM->mNSState.find(fnNamespace, fnPackage);
               ns->addFunction(fnName, this, hasBody ? ip : 0 );// if no body, set the IP to 0
               if( curNSDocBlock )
               {
                  if( fnNamespace == StringTable->lookup( nsDocBlockClass ) )
                  {
                     char *usageStr = dStrdup( curNSDocBlock );
                     usageStr[dStrlen(usageStr)] = '\0';
                     ns->mUsage = usageStr;
                     ns->mCleanUpUsage = true;
                     curNSDocBlock = NULL;
                  }
               }
               mVM->mNSState.relinkPackages();
               
               // If we had a docblock, it's definitely not valid anymore, so clear it out.
               curFNDocBlock = NULL;
               
               //Con::printf("Adding function %s::%s (%d)", fnNamespace, fnName, ip);
            }
            ip = code[ip + 7];
            break;
            
         case OP_CREATE_OBJECT:
         {
            // Read some useful info.
            objParent        = Compiler::CodeToSTE(NULL, code, ip);
            bool isDataBlock =          code[ip + 2];
            bool isInternal  =          code[ip + 3];
            bool isSingleton =          code[ip + 4];
            U32  lineNumber  =          code[ip + 5];
            failJump         =          code[ip + 6];
            
            // If we don't allow calls, we certainly don't allow creating objects!
            // Moved this to after failJump is set. Engine was crashing when
            // noCalls = true and an object was being created at the beginning of
            // a file. ADL.
            if(noCalls)
            {
               ip = failJump;
               break;
            }
            
            // Push the old info to the stack
            //Assert( objectCreationStackIndex < objectCreationStackSize );

            creationStack.push(mVM, currentNewObject, failJump);
            
            // Get the constructor information off the stack.
            mVM->mSTR.getArgcArgv(NULL, &callArgc, &callArgv);
            const char *objectName = callArgv[ 2 ];
            
            // Con::printf("Creating object...");
            
            // objectName = argv[1]...
            currentNewObject = NULL;
            
            // Are we creating a datablock? If so, deal with case where we override
            // an old one.
            if(isDataBlock)
            {
               // Con::printf("  - is a datablock");
               
               // Find the old one if any.
               KorkApi::VMObject *db = mVM->mConfig.iFind.FindDatablockGroup(mVM->mConfig.findUser);
               
               // Make sure we're not changing types on ourselves...
               if(db && dStricmp(db->klass->name, callArgv[1]))
               {
                  mVM->printf(0, "Cannot re-declare data block %s with a different class.", callArgv[2]);
                  ip = failJump;
                  break;
               }
               
               // If there was one, set the currentNewObject and move on.
               if(db)
               {
                  currentNewObject = db;
               }
            }

            // For singletons, delete the old object if it exists
            if ( isSingleton )
            {
               KorkApi::VMObject *oldObject = mVMPublic->findObjectByName( objectName );
               if (oldObject)
               {                  
                  // Prevent stack value corruption
                  mVM->mSTR.pushFrame();
                  // --
                  
                  oldObject->klass->iCreate.RemoveObjectFn(oldObject->klass->userPtr, mVMPublic, oldObject);
                  oldObject->klass->iCreate.DestroyClassFn(oldObject->klass->userPtr, mVMPublic, oldObject->userPtr);
                  
                  oldObject = NULL;

                  // Prevent stack value corruption
                  mVM->mSTR.popFrame();
               }
            }
            
            mVM->mSTR.popFrame();
            
            if(!currentNewObject.isValid())
            {
               // Well, looks like we have to create a new object.
               KorkApi::ClassInfo* klassInfo = mVM->getClassInfoByName(StringTable->insert(callArgv[1]));
               KorkApi::VMObject *object = NULL;

               if (klassInfo)
               {
                  object = new KorkApi::VMObject();
                  object->klass = klassInfo;
                  object->ns = NULL;
                  object->userPtr = klassInfo->iCreate.CreateClassFn(klassInfo->userPtr, mVMPublic, object);  

                  if (object->userPtr == NULL)
                  {
                     delete object;
                     object = NULL;
                  }
               }
               
               // Deal with failure!
               if(!object)
               {
                  mVM->printf(0, "%s: Unable to instantiate non-conobject class %s.", getFileLine(ip-1), callArgv[1]);
                  ip = failJump;
                  break;
               }
               
               // Finally, set currentNewObject to point to the new one.
               currentNewObject = object;
               
               // Deal with the case of a non-SimObject.
               if(!currentNewObject.isValid())
               {
                  mVM->printf(0, "%s: Unable to instantiate non-SimObject class %s.", getFileLine(ip-1), callArgv[1]);
                  delete object;
                  ip = failJump;
                  break;
               }

               if(*objParent)
               {
                  // Find it!
                  KorkApi::VMObject *parent = mVM->mConfig.iFind.FindObjectByNameFn(mVM->mConfig.findUser, objParent, NULL);
                  if (parent)
                  {
                     // Con::printf(" - Parent object found: %s", parent->getClassName());
                     
                     // TOFIX currentNewObject->assignFieldsFrom(parent);
                  }
                  else
                  {
                     mVM->printf(0, "%s: Unable to find parent object %s for %s.", getFileLine(ip-1), objParent, callArgv[1]);
                  }
               }

               if (!klassInfo->iCreate.ProcessArgsFn(mVMPublic, currentNewObject, objectName, isDataBlock, isInternal, callArgc-3, callArgv+3))
               {
                  currentNewObject = NULL;
                  ip = failJump;
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
            curFNDocBlock = NULL;
            curNSDocBlock = NULL;
            
            // Do we place this object at the root?
            bool placeAtRoot = code[ip++];
            
            // Con::printf("Adding object %s", currentNewObject->getName());
            
            // Make sure it wasn't already added, then add it.
            if (!currentNewObject.isValid())
            {
               break;
            }
            
            U32 groupAddId = (U32)mVM->mEvalState.intStack[mVM->mEvalState._UINT];
            if(!currentNewObject->klass->iCreate.AddObjectFn(mVMPublic, currentNewObject, placeAtRoot, groupAddId))
            {
               // This error is usually caused by failing to call Parent::initPersistFields in the class' initPersistFields().
               /* TOFIX mVM->printf(0, "%s: Register object failed for object %s of class %s.", getFileLine(ip-2), currentNewObject->getName(), currentNewObject->getClassName());*/

               // NOTE: AddObject may have "unregistered" the object, but since we refcount our objects this is still safe.
               currentNewObject->klass->iCreate.DestroyClassFn(currentNewObject->klass->userPtr, mVMPublic, currentNewObject->userPtr);
               currentNewObject = NULL;
               ip = failJump;
               break;
            }
            
            // store the new object's ID on the stack (overwriting the group/set
            // id, if one was given, otherwise getting pushed)
            if(placeAtRoot)
               mVM->mEvalState.intStack[mVM->mEvalState._UINT] = currentNewObject->klass->iCreate.GetIdFn(currentNewObject);
            else
               mVM->mEvalState.intStack[++mVM->mEvalState._UINT] = currentNewObject->klass->iCreate.GetIdFn(currentNewObject);
            
            break;
         }
            
         case OP_END_OBJECT:
         {
            // If we're not to be placed at the root, make sure we clean up
            // our group reference.
            bool placeAtRoot = code[ip++];
            if(!placeAtRoot)
               mVM->mEvalState._UINT--;
            break;
         }
            
         case OP_FINISH_OBJECT:
         {
            creationStack.pop(mVM, currentNewObject, &failJump);
            break;
         }
            
         case OP_JMPIFFNOT:
            if(mVM->mEvalState.floatStack[mVM->mEvalState._FLT--])
            {
               ip++;
               break;
            }
            ip = code[ip];
            break;
         case OP_JMPIFNOT:
            if(mVM->mEvalState.intStack[mVM->mEvalState._UINT--])
            {
               ip++;
               break;
            }
            ip = code[ip];
            break;
         case OP_JMPIFF:
            if(!mVM->mEvalState.floatStack[mVM->mEvalState._FLT--])
            {
               ip++;
               break;
            }
            ip = code[ip];
            break;
         case OP_JMPIF:
            if(!mVM->mEvalState.intStack[mVM->mEvalState._UINT--])
            {
               ip ++;
               break;
            }
            ip = code[ip];
            break;
         case OP_JMPIFNOT_NP:
            if(mVM->mEvalState.intStack[mVM->mEvalState._UINT])
            {
               mVM->mEvalState._UINT--;
               ip++;
               break;
            }
            ip = code[ip];
            break;
         case OP_JMPIF_NP:
            if(!mVM->mEvalState.intStack[mVM->mEvalState._UINT])
            {
               mVM->mEvalState._UINT--;
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
            mVM->mSTR.setStringValue("");
            // We're falling thru here on purpose.
            
         case OP_RETURN:

            creationStack.clear(mVM);

            if( iterDepth > 0 )
            {
               // Clear iterator state.
               while( iterDepth > 0 )
               {
                  IterStackRecord& iter = mVM->mEvalState.iterStack[ -- mVM->mEvalState._ITER ];
                  if (iter.mData.mObj.mSet)
                  {
                     mVM->decVMRef(iter.mData.mObj.mSet);
                     iter.mData.mObj.mSet = NULL;
                  }
                  iter.mIsStringIter = false;
                  -- iterDepth;
               }
               
               const char* returnValue = mVM->mSTR.getStringValue();
               mVM->mSTR.rewind();
               mVM->mSTR.setStringValue( returnValue ); // Not nice but works.
            }
               
            goto execFinished;

         case OP_RETURN_FLT:

            creationStack.clear(mVM);
         
            if( iterDepth > 0 )
            {
               // Clear iterator state.
               while( iterDepth > 0 )
               {
                  IterStackRecord& iter = mVM->mEvalState.iterStack[ -- mVM->mEvalState._ITER ];
                  if (iter.mData.mObj.mSet)
                  {
                     mVM->decVMRef(iter.mData.mObj.mSet);
                     iter.mData.mObj.mSet = NULL;
                  }
                  iter.mIsStringIter = false;
                  -- iterDepth;
               }
               
            }

            mVM->mSTR.setFloatValue( mVM->mEvalState.floatStack[mVM->mEvalState._FLT] );
            mVM->mEvalState._FLT--;
               
            goto execFinished;

         case OP_RETURN_UINT:

            creationStack.clear(mVM);
         
            if( iterDepth > 0 )
            {
               // Clear iterator state.
               while( iterDepth > 0 )
               {
                  IterStackRecord& iter = mVM->mEvalState.iterStack[ -- mVM->mEvalState._ITER ];
                  if (iter.mData.mObj.mSet)
                  {
                     mVM->decVMRef(iter.mData.mObj.mSet);
                     iter.mData.mObj.mSet = NULL;
                  }
                  iter.mIsStringIter = false;
                  -- iterDepth;
               }
            }

            mVM->mSTR.setIntValue( mVM->mEvalState.intStack[mVM->mEvalState._UINT] );
            mVM->mEvalState._UINT--;
               
            goto execFinished;

         case OP_CMPEQ:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT+1] = bool(mVM->mEvalState.floatStack[mVM->mEvalState._FLT] == mVM->mEvalState.floatStack[mVM->mEvalState._FLT-1]);
            mVM->mEvalState._UINT++;
            mVM->mEvalState._FLT -= 2;
            break;
            
         case OP_CMPGR:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT+1] = bool(mVM->mEvalState.floatStack[mVM->mEvalState._FLT] > mVM->mEvalState.floatStack[mVM->mEvalState._FLT-1]);
            mVM->mEvalState._UINT++;
            mVM->mEvalState._FLT -= 2;
            break;
            
         case OP_CMPGE:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT+1] = bool(mVM->mEvalState.floatStack[mVM->mEvalState._FLT] >= mVM->mEvalState.floatStack[mVM->mEvalState._FLT-1]);
            mVM->mEvalState._UINT++;
            mVM->mEvalState._FLT -= 2;
            break;
            
         case OP_CMPLT:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT+1] = bool(mVM->mEvalState.floatStack[mVM->mEvalState._FLT] < mVM->mEvalState.floatStack[mVM->mEvalState._FLT-1]);
            mVM->mEvalState._UINT++;
            mVM->mEvalState._FLT -= 2;
            break;
            
         case OP_CMPLE:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT+1] = bool(mVM->mEvalState.floatStack[mVM->mEvalState._FLT] <= mVM->mEvalState.floatStack[mVM->mEvalState._FLT-1]);
            mVM->mEvalState._UINT++;
            mVM->mEvalState._FLT -= 2;
            break;
            
         case OP_CMPNE:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT+1] = bool(mVM->mEvalState.floatStack[mVM->mEvalState._FLT] != mVM->mEvalState.floatStack[mVM->mEvalState._FLT-1]);
            mVM->mEvalState._UINT++;
            mVM->mEvalState._FLT -= 2;
            break;
            
         case OP_XOR:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT-1] = mVM->mEvalState.intStack[mVM->mEvalState._UINT] ^ mVM->mEvalState.intStack[mVM->mEvalState._UINT-1];
            mVM->mEvalState._UINT--;
            break;
            
         case OP_MOD:
            if(  mVM->mEvalState.intStack[mVM->mEvalState._UINT-1] != 0 )
               mVM->mEvalState.intStack[mVM->mEvalState._UINT-1] = mVM->mEvalState.intStack[mVM->mEvalState._UINT] % mVM->mEvalState.intStack[mVM->mEvalState._UINT-1];
            else
               mVM->mEvalState.intStack[mVM->mEvalState._UINT-1] = 0;
            mVM->mEvalState._UINT--;
            break;
            
         case OP_BITAND:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT-1] = mVM->mEvalState.intStack[mVM->mEvalState._UINT] & mVM->mEvalState.intStack[mVM->mEvalState._UINT-1];
            mVM->mEvalState._UINT--;
            break;
            
         case OP_BITOR:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT-1] = mVM->mEvalState.intStack[mVM->mEvalState._UINT] | mVM->mEvalState.intStack[mVM->mEvalState._UINT-1];
            mVM->mEvalState._UINT--;
            break;
            
         case OP_NOT:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT] = !mVM->mEvalState.intStack[mVM->mEvalState._UINT];
            break;
            
         case OP_NOTF:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT+1] = !mVM->mEvalState.floatStack[mVM->mEvalState._FLT];
            mVM->mEvalState._FLT--;
            mVM->mEvalState._UINT++;
            break;
            
         case OP_ONESCOMPLEMENT:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT] = ~mVM->mEvalState.intStack[mVM->mEvalState._UINT];
            break;
            
         case OP_SHR:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT-1] = mVM->mEvalState.intStack[mVM->mEvalState._UINT] >> mVM->mEvalState.intStack[mVM->mEvalState._UINT-1];
            mVM->mEvalState._UINT--;
            break;
            
         case OP_SHL:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT-1] = mVM->mEvalState.intStack[mVM->mEvalState._UINT] << mVM->mEvalState.intStack[mVM->mEvalState._UINT-1];
            mVM->mEvalState._UINT--;
            break;
            
         case OP_AND:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT-1] = mVM->mEvalState.intStack[mVM->mEvalState._UINT] && mVM->mEvalState.intStack[mVM->mEvalState._UINT-1];
            mVM->mEvalState._UINT--;
            break;
            
         case OP_OR:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT-1] = mVM->mEvalState.intStack[mVM->mEvalState._UINT] || mVM->mEvalState.intStack[mVM->mEvalState._UINT-1];
            mVM->mEvalState._UINT--;
            break;
            
         case OP_ADD:
            mVM->mEvalState.floatStack[mVM->mEvalState._FLT-1] = mVM->mEvalState.floatStack[mVM->mEvalState._FLT] + mVM->mEvalState.floatStack[mVM->mEvalState._FLT-1];
            mVM->mEvalState._FLT--;
            break;
            
         case OP_SUB:
            mVM->mEvalState.floatStack[mVM->mEvalState._FLT-1] = mVM->mEvalState.floatStack[mVM->mEvalState._FLT] - mVM->mEvalState.floatStack[mVM->mEvalState._FLT-1];
            mVM->mEvalState._FLT--;
            break;
            
         case OP_MUL:
            mVM->mEvalState.floatStack[mVM->mEvalState._FLT-1] = mVM->mEvalState.floatStack[mVM->mEvalState._FLT] * mVM->mEvalState.floatStack[mVM->mEvalState._FLT-1];
            mVM->mEvalState._FLT--;
            break;
         case OP_DIV:
            mVM->mEvalState.floatStack[mVM->mEvalState._FLT-1] = mVM->mEvalState.floatStack[mVM->mEvalState._FLT] / mVM->mEvalState.floatStack[mVM->mEvalState._FLT-1];
            mVM->mEvalState._FLT--;
            break;
         case OP_NEG:
            mVM->mEvalState.floatStack[mVM->mEvalState._FLT] = -mVM->mEvalState.floatStack[mVM->mEvalState._FLT];
            break;
            
         case OP_SETCURVAR:
            var = Compiler::CodeToSTE(NULL, code, ip);
            ip += 2;
            
            // If a variable is set, then these must be NULL. It is necessary
            // to set this here so that the vector parser can appropriately
            // identify whether it's dealing with a vector.
            prevField = NULL;
            prevObject = NULL;
            curObject = NULL;
            
            mVM->mEvalState.setCurVarName(var);
            
            // In order to let docblocks work properly with variables, we have
            // clear the current docblock when we do an assign. This way it
            // won't inappropriately carry forward to following function decls.
            curFNDocBlock = NULL;
            curNSDocBlock = NULL;
            break;
            
         case OP_SETCURVAR_CREATE:
            var = Compiler::CodeToSTE(NULL, code, ip);
            ip += 2;
            
            // See OP_SETCURVAR
            prevField = NULL;
            prevObject = NULL;
            curObject = NULL;
            
            mVM->mEvalState.setCurVarNameCreate(var);
            
            // See OP_SETCURVAR for why we do this.
            curFNDocBlock = NULL;
            curNSDocBlock = NULL;
            break;
            
         case OP_SETCURVAR_ARRAY:
            var = mVM->mSTR.getSTValue();
            
            // See OP_SETCURVAR
            prevField = NULL;
            prevObject = NULL;
            curObject = NULL;
            
            mVM->mEvalState.setCurVarName(var);
            
            // See OP_SETCURVAR for why we do this.
            curFNDocBlock = NULL;
            curNSDocBlock = NULL;
            break;
            
         case OP_SETCURVAR_ARRAY_CREATE:
            var = mVM->mSTR.getSTValue();
            
            // See OP_SETCURVAR
            prevField = NULL;
            prevObject = NULL;
            curObject = NULL;
            
            mVM->mEvalState.setCurVarNameCreate(var);
            
            // See OP_SETCURVAR for why we do this.
            curFNDocBlock = NULL;
            curNSDocBlock = NULL;
            break;
            
         case OP_LOADVAR_UINT:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT+1] = mVM->mEvalState.getIntVariable();
            mVM->mEvalState._UINT++;
            break;
            
         case OP_LOADVAR_FLT:
            mVM->mEvalState.floatStack[mVM->mEvalState._FLT+1] = mVM->mEvalState.getFloatVariable();
            mVM->mEvalState._FLT++;
            break;
            
         case OP_LOADVAR_STR:
            val = mVM->mEvalState.getStringVariable();
            mVM->mSTR.setStringValue(val);
            break;
            
         case OP_LOADVAR_VAR:
            // Sets current source of OP_SAVEVAR_VAR
            mVM->mEvalState.copyVariable = mVM->mEvalState.currentVariable;
            break;
            
         case OP_SAVEVAR_UINT:
            mVM->mEvalState.setIntVariable((S32)mVM->mEvalState.intStack[mVM->mEvalState._UINT]);
            break;
            
         case OP_SAVEVAR_FLT:
            mVM->mEvalState.setFloatVariable(mVM->mEvalState.floatStack[mVM->mEvalState._FLT]);
            break;
            
         case OP_SAVEVAR_STR:
            mVM->mEvalState.setStringVariable(mVM->mSTR.getStringValue());
            break;
            
         case OP_SAVEVAR_VAR:
            // this basically handles %var1 = %var2
            mVM->mEvalState.setCopyVariable();
            break;
            
         case OP_SETCUROBJECT:
            // Save the previous object for parsing vector fields.
            prevObject = curObject;
            val = mVM->mSTR.getStringValue();
            
            // Sim::findObject will sometimes find valid objects from
            // multi-component strings. This makes sure that doesn't
            // happen.
            for( const char* check = val; *check; check++ )
            {
               if( *check == ' ' )
               {
                  val = "";
                  break;
               }
            }
            curObject = mVM->mConfig.iFind.FindObjectByPathFn(mVM->mConfig.findUser, val);
            break;
            
         case OP_SETCUROBJECT_INTERNAL:
            ++ip; // To skip the recurse flag if the object wasnt found
            if(curObject)
            {
               StringTableEntry intName = StringTable->insert(mVM->mSTR.getStringValue());
               bool recurse = code[ip-1];
               KorkApi::VMObject* obj = mVM->mConfig.iFind.FindObjectByInternalNameFn(mVM->mConfig.findUser, intName, recurse, curObject);
               mVM->mEvalState.intStack[mVM->mEvalState._UINT+1] = obj ? obj->klass->iCreate.GetIdFn(obj) : 0;
            }
            break;
            
         case OP_SETCUROBJECT_NEW:
            curObject = currentNewObject;
            break;
            
         case OP_SETCURFIELD:
            // Save the previous field for parsing vector fields.
            prevField = curField;
            dStrcpy( prevFieldArray, curFieldArray );
            curField = Compiler::CodeToSTE(NULL, code, ip);
            curFieldArray[0] = 0;
            ip += 2;
            break;
            
         case OP_SETCURFIELD_ARRAY:
            dStrcpy(curFieldArray, mVM->mSTR.getStringValue());
            break;

         case OP_SETCURFIELD_TYPE:
            //if(curObject)
            //   curObject->setDataFieldType(code[ip], curField, curFieldArray);
            ip++;
            break;
            
         case OP_LOADFIELD_UINT:
            if(curObject)
            {
               KorkApi::ConsoleValue retValue = mVM->getObjectField(curObject, curField, curFieldArray, KorkApi::ConsoleValue::TypeInternalInt, KorkApi::ConsoleValue::ZoneExternal);
               mVM->mEvalState.intStack[mVM->mEvalState._UINT+1] = castValueToU32(retValue, mVM->mAllocBase);
            }
            else
            {
               // The field is not being retrieved from an object. Maybe it's
               // a special accessor?
               
               //getFieldComponent( prevObject, prevField, prevFieldArray, curField, valBuffer, VAL_BUFFER_SIZE );
               mVM->mEvalState.intStack[mVM->mEvalState._UINT+1] = 0;//dAtoi( valBuffer );
            }
            mVM->mEvalState._UINT++;
            break;
            
         case OP_LOADFIELD_FLT:
            if(curObject)
            {
               KorkApi::ConsoleValue retValue =  mVM->getObjectField(curObject, curField, curFieldArray, KorkApi::ConsoleValue::TypeInternalFloat, KorkApi::ConsoleValue::ZoneExternal);
               mVM->mEvalState.floatStack[mVM->mEvalState._FLT+1] = castValueToF32(retValue, mVM->mAllocBase);
            }
            else
            {
               // The field is not being retrieved from an object. Maybe it's
               // a special accessor?
               //getFieldComponent( prevObject, prevField, prevFieldArray, curField, valBuffer, VAL_BUFFER_SIZE );
               mVM->mEvalState.floatStack[mVM->mEvalState._FLT+1] = 0.0f;//dAtof( valBuffer );
            }
            mVM->mEvalState._FLT++;
            break;
            
         case OP_LOADFIELD_STR:
            if(curObject)
            {
               KorkApi::ConsoleValue retValue =  mVM->getObjectField(curObject, curField, curFieldArray, KorkApi::ConsoleValue::TypeInternalString, KorkApi::ConsoleValue::ZoneExternal);
               mVM->mSTR.setStringValue(retValue);
            }
            else
            {
               // The field is not being retrieved from an object. Maybe it's
               // a special accessor?
               //getFieldComponent( prevObject, prevField, prevFieldArray, curField, valBuffer, VAL_BUFFER_SIZE );
               mVM->mSTR.setStringValue( ""); //valBuffer );
            }
            break;
            
         case OP_SAVEFIELD_UINT:
            mVM->mSTR.setIntValue((U32)mVM->mEvalState.intStack[mVM->mEvalState._UINT]);
            if(curObject)
            {
               KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeString(mVM->mSTR.getStringValue());
               mVM->setObjectField(curObject, curField, curFieldArray, cv);
            }
            else
            {
               // The field is not being set on an object. Maybe it's
               // a special accessor?
               //setFieldComponent( prevObject, prevField, prevFieldArray, curField );
               prevObject = NULL;
            }
            break;
            
         case OP_SAVEFIELD_FLT:
            mVM->mSTR.setFloatValue(mVM->mEvalState.floatStack[mVM->mEvalState._FLT]);
            if(curObject)
            {
               KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeString(mVM->mSTR.getStringValue());
               mVM->setObjectField(curObject, curField, curFieldArray, cv);
            }
            else
            {
               // The field is not being set on an object. Maybe it's
               // a special accessor?
               //setFieldComponent( prevObject, prevField, prevFieldArray, curField );
               prevObject = NULL;
            }
            break;
            
         case OP_SAVEFIELD_STR:
            if(curObject)
            {
               KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeString(mVM->mSTR.getStringValue());
               mVM->setObjectField(curObject, curField, curFieldArray, cv);
            }
            else
            {
               // The field is not being set on an object. Maybe it's
               // a special accessor?
               //setFieldComponent( prevObject, prevField, prevFieldArray, curField );
               prevObject = NULL;
            }
            break;
            
         case OP_STR_TO_UINT:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT+1] = mVM->mSTR.getIntValue();
            mVM->mEvalState._UINT++;
            break;
            
         case OP_STR_TO_FLT:
            mVM->mEvalState.floatStack[mVM->mEvalState._FLT+1] = mVM->mSTR.getFloatValue();
            mVM->mEvalState._FLT++;
            break;
            
         case OP_STR_TO_NONE:
            // This exists simply to deal with certain typecast situations.
            break;
            
         case OP_FLT_TO_UINT:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT+1] = (S64)mVM->mEvalState.floatStack[mVM->mEvalState._FLT];
            mVM->mEvalState._FLT--;
            mVM->mEvalState._UINT++;
            break;
            
         case OP_FLT_TO_STR:
            mVM->mSTR.setFloatValue(mVM->mEvalState.floatStack[mVM->mEvalState._FLT]);
            mVM->mEvalState._FLT--;
            break;
            
         case OP_FLT_TO_NONE:
            mVM->mEvalState._FLT--;
            break;
            
         case OP_UINT_TO_FLT:
            mVM->mEvalState.floatStack[mVM->mEvalState._FLT+1] = (F64)mVM->mEvalState.intStack[mVM->mEvalState._UINT];
            mVM->mEvalState._UINT--;
            mVM->mEvalState._FLT++;
            break;
            
         case OP_UINT_TO_STR:
            mVM->mSTR.setIntValue((U32)mVM->mEvalState.intStack[mVM->mEvalState._UINT]);
            mVM->mEvalState._UINT--;
            break;
            
         case OP_UINT_TO_NONE:
            mVM->mEvalState._UINT--;
            break;
         
         case OP_COPYVAR_TO_NONE:
            mVM->mEvalState.copyVariable = NULL;
            break;
            
         case OP_LOADIMMED_UINT:
            mVM->mEvalState.intStack[mVM->mEvalState._UINT+1] = code[ip++];
            mVM->mEvalState._UINT++;
            break;
            
         case OP_LOADIMMED_FLT:
            mVM->mEvalState.floatStack[mVM->mEvalState._FLT+1] = curFloatTable[code[ip]];
            ip++;
            mVM->mEvalState._FLT++;
            break;
         case OP_TAG_TO_STR:
            code[ip-1] = OP_LOADIMMED_STR;
            // it's possible the string has already been converted
            if(U8(curStringTable[code[ip]]) != KorkApi::StringTagPrefixByte)
            {
               U32 id = 0;// TOFIX GameAddTaggedString(curStringTable + code[ip]);
               dSprintf(curStringTable + code[ip] + 1, 7, "%d", id);
               *(curStringTable + code[ip]) = KorkApi::StringTagPrefixByte;
            }
         case OP_LOADIMMED_STR:
            mVM->mSTR.setStringValue(curStringTable + code[ip++]);
            break;
            
         case OP_DOCBLOCK_STR:
         {
            // If the first word of the doc is '\class' or '@class', then this
            // is a namespace doc block, otherwise it is a function doc block.
            const char* docblock = curStringTable + code[ip++];
            
            const char* sansClass = dStrstr( docblock, "@class" );
            if( !sansClass )
               sansClass = dStrstr( docblock, "\\class" );
            
            if( sansClass )
            {
               // Don't save the class declaration. Scan past the 'class'
               // keyword and up to the first whitespace.
               sansClass += 7;
               S32 index = 0;
               while( ( *sansClass != ' ' ) && ( *sansClass != '\n' ) && *sansClass && ( index < ( nsDocLength - 1 ) ) )
               {
                  nsDocBlockClass[index++] = *sansClass;
                  sansClass++;
               }
               nsDocBlockClass[index] = '\0';
               
               curNSDocBlock = sansClass + 1;
            }
            else
               curFNDocBlock = docblock;
         }
            
            break;
            
         case OP_LOADIMMED_IDENT:
            mVM->mSTR.setStringValue(Compiler::CodeToSTE(NULL, code, ip));
            ip += 2;
            break;
            
         case OP_CALLFUNC_RESOLVE:
            // This deals with a function that is potentially living in a namespace.
            fnNamespace = Compiler::CodeToSTE(NULL, code, ip+2);
            fnName      = Compiler::CodeToSTE(NULL, code, ip);
            
            // Try to look it up.
            ns = mVM->mNSState.find(fnNamespace);
            nsEntry = ns->lookup(fnName);
            if(!nsEntry)
            {
               ip+= 5;
               mVM->printf(0,
                          "%s: Unable to find function %s%s%s",
                          getFileLine(ip-4), fnNamespace ? fnNamespace : "",
                          fnNamespace ? "::" : "", fnName);
               mVM->mSTR.popFrame();
               break;
            }
            // Now, rewrite our code a bit (ie, avoid future lookups) and fall
            // through to OP_CALLFUNC
#ifdef TORQUE_64
            *((U64*)(code+ip+2)) = ((U64)nsEntry);
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
            
            fnName = Compiler::CodeToSTE(NULL, code, ip);
            
            //if this is called from inside a function, append the ip and codeptr
            if (!mVM->mEvalState.stack.empty())
            {
               mVM->mEvalState.stack.last()->code = this;
               mVM->mEvalState.stack.last()->ip = ip - 1;
            }
            
            U32 callType = code[ip+4];
            
            ip += 5;
            mVM->mSTR.getArgcArgv(fnName, &callArgc, &callArgv);
            
            if(callType == FuncCallExprNode::FunctionCall)
            {
#ifdef TORQUE_64
               nsEntry = ((Namespace::Entry *) *((U64*)(code+ip-3)));
#else
               nsEntry = ((Namespace::Entry *) *(code+ip-3));
#endif
               ns = NULL;
            }
            else if(callType == FuncCallExprNode::MethodCall)
            {
               saveObject = mVM->mEvalState.thisObject;
               mVM->mEvalState.thisObject = mVM->mConfig.iFind.FindObjectByPathFn(mVM->mConfig.findUser, callArgv[1]);
               
               if(!mVM->mEvalState.thisObject)
               {
                  mVM->mEvalState.thisObject = 0;
                  mVM->printf(0,"%s: Unable to find object: '%s' attempting to call function '%s'", getFileLine(ip-6), callArgv[1], fnName);
                  mVM->mSTR.popFrame();
                  mVM->mSTR.setStringValue("");
                  break;
               }
               
               ns = mVM->mEvalState.thisObject->ns;
               if(ns)
                  nsEntry = ns->lookup(fnName);
               else
                  nsEntry = NULL;
            }
            else // it's a ParentCall
            {
               if(thisNamespace)
               {
                  ns = thisNamespace->mParent;
                  if(ns)
                     nsEntry = ns->lookup(fnName);
                  else
                     nsEntry = NULL;
               }
               else
               {
                  ns = NULL;
                  nsEntry = NULL;
               }
            }
            
            if(!nsEntry || noCalls)
            {
               if(!noCalls)
               {
                  mVM->printf(0,"%s: Unknown command %s.", getFileLine(ip-4), fnName);
                  if(callType == FuncCallExprNode::MethodCall)
                  {
                     /* TOFIXmVM->printf(0, "  Object %s(%d) %s",
                                mVM->mEvalState.thisObject->getName() ? mVM->mEvalState.thisObject->getName() : "",
                                mVM->mEvalState.thisObject->getId(), getNamespaceList(ns) ); */
                  }
               }
               mVM->mSTR.popFrame();
               mVM->mSTR.setStringValue("");
               mVM->mSTR.setStringValue("");
               break;
            }
            if(nsEntry->mType == Namespace::Entry::ScriptFunctionType)
            {
               const char *ret = "";
               if(nsEntry->mFunctionOffset)
                  ret = nsEntry->mCode->exec(nsEntry->mFunctionOffset, fnName, nsEntry->mNamespace, callArgc, callArgv, false, nsEntry->mPackage);
               else // no body
                  mVM->mSTR.setStringValue("");
               
               mVM->mSTR.popFrame();
               mVM->mSTR.setStringValue(ret);
            }
            else
            {
               if((nsEntry->mMinArgs && S32(callArgc) < nsEntry->mMinArgs) || (nsEntry->mMaxArgs && S32(callArgc) > nsEntry->mMaxArgs))
               {
                  const char* nsName = ns? ns->mName: "";
                  mVM->printf(0, "%s: %s::%s - wrong number of arguments.", getFileLine(ip-4), nsName, fnName);
                  mVM->printf(0, "%s: usage: %s", getFileLine(ip-4), nsEntry->mUsage);
                  mVM->mSTR.popFrame();
                  mVM->mSTR.setStringValue("");
               }
               else
               {
                  switch(nsEntry->mType)
                  {
                     case Namespace::Entry::StringCallbackType:
                     {
                        const char *ret = nsEntry->cb.mStringCallbackFunc(safeObjectUserPtr(mVM->mEvalState.thisObject), nsEntry->mUserPtr, callArgc, callArgv);
                        mVM->mSTR.popFrame();
                        if(ret != mVM->mSTR.getStringValue())
                           mVM->mSTR.setStringValue(ret);
                        else
                           mVM->mSTR.setLen(dStrlen(ret));
                        break;
                     }
                     case Namespace::Entry::IntCallbackType:
                     {
                        S32 result = nsEntry->cb.mIntCallbackFunc(safeObjectUserPtr(mVM->mEvalState.thisObject), nsEntry->mUserPtr, callArgc, callArgv);
                        mVM->mSTR.popFrame();
                        if(code[ip] == OP_STR_TO_UINT)
                        {
                           ip++;
                           mVM->mEvalState.intStack[++mVM->mEvalState._UINT] = result;
                           break;
                        }
                        else if(code[ip] == OP_STR_TO_FLT)
                        {
                           ip++;
                           mVM->mEvalState.floatStack[++mVM->mEvalState._FLT] = result;
                           break;
                        }
                        else if(code[ip] == OP_STR_TO_NONE)
                           ip++;
                        else
                           mVM->mSTR.setIntValue(result);
                        break;
                     }
                     case Namespace::Entry::FloatCallbackType:
                     {
                        F64 result = nsEntry->cb.mFloatCallbackFunc(safeObjectUserPtr(mVM->mEvalState.thisObject), nsEntry->mUserPtr, callArgc, callArgv);
                        mVM->mSTR.popFrame();
                        if(code[ip] == OP_STR_TO_UINT)
                        {
                           ip++;
                           mVM->mEvalState.intStack[++mVM->mEvalState._UINT] = (S64)result;
                           break;
                        }
                        else if(code[ip] == OP_STR_TO_FLT)
                        {
                           ip++;
                           mVM->mEvalState.floatStack[++mVM->mEvalState._FLT] = result;
                           break;
                        }
                        else if(code[ip] == OP_STR_TO_NONE)
                           ip++;
                        else
                           mVM->mSTR.setFloatValue(result);
                        break;
                     }
                     case Namespace::Entry::VoidCallbackType:
                        nsEntry->cb.mVoidCallbackFunc(safeObjectUserPtr(mVM->mEvalState.thisObject), nsEntry->mUserPtr, callArgc, callArgv);
                        if(code[ip] != OP_STR_TO_NONE)
                        {
                           mVM->printf(0, "%s: Call to %s in %s uses result of void function call.", getFileLine(ip-4), fnName, functionName);
                        }
                        mVM->mSTR.popFrame();
                        mVM->mSTR.setStringValue("");
                        break;
                     case Namespace::Entry::BoolCallbackType:
                     {
                        bool result = nsEntry->cb.mBoolCallbackFunc(safeObjectUserPtr(mVM->mEvalState.thisObject), nsEntry->mUserPtr, callArgc, callArgv);
                        mVM->mSTR.popFrame();
                        if(code[ip] == OP_STR_TO_UINT)
                        {
                           ip++;
                           mVM->mEvalState.intStack[++mVM->mEvalState._UINT] = result;
                           break;
                        }
                        else if(code[ip] == OP_STR_TO_FLT)
                        {
                           ip++;
                           mVM->mEvalState.floatStack[++mVM->mEvalState._FLT] = result;
                           break;
                        }
                        else if(code[ip] == OP_STR_TO_NONE)
                           ip++;
                        else
                           mVM->mSTR.setIntValue(result);
                        break;
                     }
                  }
               }
            }
            
            if(callType == FuncCallExprNode::MethodCall)
               mVM->mEvalState.thisObject = saveObject;
            break;
         }
         case OP_ADVANCE_STR:
            mVM->mSTR.advance();
            break;
         case OP_ADVANCE_STR_APPENDCHAR:
            mVM->mSTR.advanceChar(code[ip++]);
            break;
            
         case OP_ADVANCE_STR_COMMA:
            mVM->mSTR.advanceChar('_');
            break;
            
         case OP_ADVANCE_STR_NUL:
            mVM->mSTR.advanceChar(0);
            break;
            
         case OP_REWIND_STR:
            mVM->mSTR.rewind();
            break;
            
         case OP_TERMINATE_REWIND_STR:
            mVM->mSTR.rewindTerminate();
            break;
            
         case OP_COMPARE_STR:
            mVM->mEvalState.intStack[++mVM->mEvalState._UINT] = mVM->mSTR.compare();
            break;
         case OP_PUSH:
            mVM->mSTR.push();
            break;
            
         case OP_PUSH_UINT:
            // OPmVM->mEvalState._UINT_TO_STR, OP_PUSH
            mVM->mSTR.setIntValue((U32)mVM->mEvalState.intStack[mVM->mEvalState._UINT]);
            mVM->mEvalState._UINT--;
            mVM->mSTR.push();
            break;
         case OP_PUSH_FLT:
            // OPmVM->mEvalState._FLT_TO_STR, OP_PUSH
            mVM->mSTR.setFloatValue(mVM->mEvalState.floatStack[mVM->mEvalState._FLT]);
            mVM->mEvalState._FLT--;
            mVM->mSTR.push();
            break;
         case OP_PUSH_VAR:
            // OP_LOADVAR_STR, OP_PUSH
            val = mVM->mEvalState.getStringVariable();
            mVM->mSTR.setStringValue(val);
            mVM->mSTR.push();
            break;

         case OP_PUSH_FRAME:
            mVM->mSTR.pushFrame();
            break;

         case OP_ASSERT:
         {
            if( !mVM->mEvalState.intStack[mVM->mEvalState._UINT--] )
            {
               const char *message = curStringTable + code[ip];

               U32 breakLine, inst;
               findBreakLine( ip - 1, breakLine, inst );

               if ( PlatformAssert::processAssert( PlatformAssert::Fatal, 
                                                   name ? name : "eval", 
                                                   breakLine,  
                                                   message ) )
               {
                  if ( mVM->mTelDebugger && mVM->mTelDebugger->isConnected() && breakLine > 0 )
                  {
                     mVM->mTelDebugger->breakProcess();
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
            AssertFatal( !mVM->mEvalState.stack.empty(), "Empty eval stack on break!");
            mVM->mEvalState.stack.last()->code = this;
            mVM->mEvalState.stack.last()->ip = ip - 1;
            
            U32 breakLine;
            findBreakLine(ip-1, breakLine, instruction);
            if(!breakLine)
               goto breakContinue;
            mVM->mTelDebugger->executionStopped(this, breakLine);
            
            goto breakContinue;
         }
         
         case OP_ITER_BEGIN_STR:
         {
            mVM->mEvalState.iterStack[ mVM->mEvalState._ITER ].mIsStringIter = true;
            /* fallthrough */
         }
         
         case OP_ITER_BEGIN:
         {
            StringTableEntry varName = Compiler::CodeToSTE(NULL, code, ip);
            U32 failIp = code[ ip + 2 ];
            
            IterStackRecord& iter = mVM->mEvalState.iterStack[ mVM->mEvalState._ITER ];
            
            iter.mVariable = mVM->mEvalState.getCurrentFrame().add( varName );
            iter.mDictionary = &mVM->mEvalState.getCurrentFrame();
            
            if( iter.mIsStringIter )
            {
               iter.mData.mStr.mString = mVM->mSTR.getStringValue();
               iter.mData.mStr.mIndex = 0;
            }
            else
            {
               // Look up the object.
               KorkApi::VMObject* set = mVM->mConfig.iFind.FindObjectByPathFn(mVM->mConfig.findUser, mVM->mSTR.getStringValue());
               
               if( !set )
               {
                  mVM->printf(0, "No SimSet object '%s'", mVM->mSTR.getStringValue());
                  mVM->printf(0, "Did you mean to use 'foreach$' instead of 'foreach'?");
                  ip = failIp;
                  continue;
               }
               
               // Set up.

               AssertFatal(iter.mData.mObj.mSet == NULL, "Should be NULL");

               mVM->incVMRef(set);
               iter.mData.mObj.mSet = set;
               iter.mData.mObj.mIndex = 0;
            }
            
            mVM->mEvalState._ITER ++;
            iterDepth ++;
            
            mVM->mSTR.push();
            
            ip += 3;
            break;
         }
         
         case OP_ITER:
         {
            U32 breakIp = code[ ip ];
            IterStackRecord& iter = mVM->mEvalState.iterStack[ mVM->mEvalState._ITER - 1 ];
            
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
                     mVM->decVMRef(set);
                     iter.mData.mObj.mSet = NULL;
                  }
                  ip = breakIp;
                  continue;
               }
               
               KorkApi::VMObject* atObject = set->klass->iEnum.GetObjectAtIndex(set, index);
               iter.mDictionary->setEntryIntValue(iter.mVariable, atObject ? atObject->klass->iCreate.GetIdFn(atObject) : 0);
               iter.mData.mObj.mIndex = index + 1;
            }
            
            ++ ip;
            break;
         }
         
         case OP_ITER_END:
         {
            -- mVM->mEvalState._ITER;
            -- iterDepth;
            IterStackRecord& iter = mVM->mEvalState.iterStack[mVM->mEvalState._ITER];

            if (iter.mData.mObj.mSet)
            {
               mVM->decVMRef(iter.mData.mObj.mSet);
               iter.mData.mObj.mSet = NULL;
            }
            

            mVM->mSTR.rewind();
            
            mVM->mEvalState.iterStack[ mVM->mEvalState._ITER ].mIsStringIter = false;
            break;
         }

         case OP_INVALID:
            
         default:
            // error!
            goto execFinished;
      }
   }
execFinished:

   creationStack.clear(mVM);
   
   if ( telDebuggerOn && setFrame < 0 )
      mVM->mTelDebugger->popStackFrame();
   
   if ( popFrame )
      mVM->mEvalState.popFrame();
   
   if(argv)
   {
      if(mVM->mEvalState.traceOn)
      {
         traceBuffer[0] = 0;
         dStrcat(traceBuffer, "Leaving ");
         
         if(packageName)
         {
            dStrcat(traceBuffer, "[");
            dStrcat(traceBuffer, packageName);
            dStrcat(traceBuffer, "]");
         }
         if(thisNamespace && thisNamespace->mName)
         {
            dSprintf(traceBuffer + dStrlen(traceBuffer), sizeof(traceBuffer) - dStrlen(traceBuffer),
                     "%s::%s() - return %s", thisNamespace->mName, thisFunctionName, mVM->mSTR.getStringValue());
         }
         else
         {
            dSprintf(traceBuffer + dStrlen(traceBuffer), sizeof(traceBuffer) - dStrlen(traceBuffer),
                     "%s() - return %s", thisFunctionName, mVM->mSTR.getStringValue());
         }
         mVM->printf(0, "%s", traceBuffer);
      }
   }
   else
   {
      delete[] const_cast<char*>(globalStrings);
      delete[] globalFloats;
      globalStrings = NULL;
      globalFloats = NULL;
   }
   
   mVM->mCurrentCodeBlock = saveCodeBlock;
   if(saveCodeBlock && saveCodeBlock->name)
   {
      mVM->mCurrentFile = saveCodeBlock->name;
      mVM->mCurrentRoot = saveCodeBlock->mRoot;
   }
   
   const char* retValue = mVM->mSTR.getStringValue();
   
#ifdef TORQUE_DEBUG
   AssertFatal(!(mVM->mSTR.mStartStackSize > stackStart), "String stack not popped enough in script exec");
   AssertFatal(!(mVM->mSTR.mStartStackSize < stackStart), "String stack popped too much in script exec");
#endif
   
   decRefCount();
   return retValue;
}

//------------------------------------------------------------

