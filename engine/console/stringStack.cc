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

#include "embed/api.h"
#include "embed/internalApi.h"
#include "stringStack.h"

void StringStack::getArgcArgv(StringTableEntry name, U32 *argc, KorkApi::ConsoleValue **in_argv, bool popStackFrame /* = false */)
{
   AssertFatal(mNumFrames != 0, "Stack underflow!");
   U32 startStack = mFrameOffsets[mNumFrames-1] + 1;
   U32 argCount   = getMin(mStartStackSize - startStack, (U32)MaxArgs-1);
   
   AssertFatal(argCount != MaxArgs-1, "WTF");
   
   *in_argv = mArgV;
   mArgV[0] = KorkApi::ConsoleValue::makeString(name);
   
   for(U32 i = 0; i < argCount; i++)
   {
      mArgV[i+1] = getStackConsoleValue(startStack + i);
   }
   argCount++;
   
   *argc = argCount;
   
   if(popStackFrame)
      popFrame();
}

void StringStack::convertArgv(KorkApi::VmInternal* vm, U32 argc, const char*** in_argv)
{
   *in_argv = mArgVStr;

   for(U32 i = 0; i < argc; i++)
   {
      if (mArgV[i].isString())
      {
         mArgVStr[i] = (const char*)mArgV[i].evaluatePtr(*mAllocBase);
      }
      else
      {
         mArgVStr[i] = vm->valueAsString(mArgV[i]);
      }
   }
}

void StringStack::convertArgs(KorkApi::VmInternal* vm, U32 numArgs, KorkApi::ConsoleValue* args, const char **outArgs)
{
   for(U32 i = 0; i < numArgs; i++)
   {
      if (!args[i].isString())
      {
         outArgs[i] = (const char*)args[i].evaluatePtr(vm->mAllocBase);
      }
      else
      {
         outArgs[i] = vm->valueAsString(args[i]);
      }
   }

}

void StringStack::convertArgsReverse(KorkApi::VmInternal* vm, U32 numArgs, const char **args, KorkApi::ConsoleValue* outArgs)
{
   for(U32 i = 0; i < numArgs; i++)
   {
      outArgs[i] = KorkApi::ConsoleValue::makeString(args[i]);
   }
}


void StringStack::performOp(U32 op, KorkApi::Vm* vm, KorkApi::TypeInfo* typeInfo)
{
   KorkApi::ConsoleValue rhs = getStackConsoleValue(mStartStackSize-1);
   KorkApi::ConsoleValue lhs = getConsoleValue();
   
   KorkApi::TypeInfo& info = typeInfo[rhs.typeId];
   
   rewind(); // only rhs is on other side
   
   KorkApi::ConsoleValue result = info.iFuncs.PerformOpFn(info.userPtr, vm, op, lhs, rhs);
   setConsoleValue(vm->mInternal, result);
}

void StringStack::performOpReverse(U32 op, KorkApi::Vm* vm, KorkApi::TypeInfo* typeInfo)
{
   KorkApi::ConsoleValue rhs = getStackConsoleValue(mStartStackSize-1);
   KorkApi::ConsoleValue lhs = getConsoleValue();
   
   KorkApi::TypeInfo& info = typeInfo[lhs.typeId];
   
   rewind(); // only lhs is on other side
   
   KorkApi::ConsoleValue result = info.iFuncs.PerformOpFn(info.userPtr, vm, op, lhs, rhs);
   setConsoleValue(vm->mInternal, result);
}

void StringStack::performUnaryOp(U32 op, KorkApi::Vm* vm, KorkApi::TypeInfo* typeInfo)
{
   KorkApi::ConsoleValue lhs = getConsoleValue();
   KorkApi::TypeInfo& info = typeInfo[lhs.typeId];
   KorkApi::ConsoleValue result = info.iFuncs.PerformOpFn(info.userPtr, vm, op, lhs, lhs);
   setConsoleValue(vm->mInternal, result);
}

void StringStack::copyStoredValueToStack(KorkApi::VmInternal* vm, KorkApi::ConsoleValue v, void* ptr)
{
   if (ptr)
   {
      KorkApi::TypeStorageInterface outputStorage = KorkApi::CreateExprStringStackStorage(vm,
                                                                              *this,
                                                                              0,
                                                                              v.typeId);

      KorkApi::TypeStorageInterface inputStorage = KorkApi::CreateRegisterStorageFromArg(vm, v);
      
      // NOTE: types should set head of stack to value if data pointer is NULL in this case
      vm->mTypes[v.typeId].iFuncs.CastValueFn(vm->mTypes[v.typeId].userPtr,
                                                          vm->mVM,
                                                          &inputStorage,
                                                          &outputStorage,
                                                          NULL,
                                                          0,
                                                          v.typeId);
   }
   else
   {
      mValue = 0;
      mType = 0;
      mLen = 0;
   }
}

void StringStack::setConsoleValue(KorkApi::VmInternal* vmInternal, KorkApi::ConsoleValue v)
{
   void* valueBase = NULL;
   
   if (v.typeId != KorkApi::ConsoleValue::TypeInternalString && 
       v.typeId < KorkApi::ConsoleValue::TypeBeginCustom)
   {
      mType = v.typeId;
      mLen = 0;
      validateBufferSize(mStart + mLen);
      *((U64*)&mValue) = v.cvalue;
      return;
   }
   else
   {
      // TOFIX: needs to use storage api (mLen issues)
      valueBase = v.evaluatePtr(*mAllocBase);
      if (valueBase != (mBuffer + mStart)) // account for setting same head
      {
         if (v.typeId == KorkApi::ConsoleValue::TypeInternalString)
         {
            setStringValue((const char*)valueBase);
         }
         else if (valueBase)
         {
            KorkApi::TypeInfo info = (*mTypes)[v.typeId];
            
            if (info.valueSize != UINT_MAX)
            {
               mLen = (U32)info.valueSize;
               memmove(mBuffer + mStart, valueBase, mLen);
               mValue = mStart;
            }
            else
            {
               copyStoredValueToStack(vmInternal, v, v.evaluatePtr(*mAllocBase));
               return;
            }
         }
         else
         {
            mValue = v.cvalue;
         }
      }
      mType = v.typeId;
   }
}
