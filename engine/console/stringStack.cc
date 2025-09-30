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
      U16 typeId = mStartTypes[startStack + i];
      uintptr_t startData = mStartOffsets[startStack + i];
      if (typeId == KorkApi::ConsoleValue::TypeInternalUnsigned ||
          typeId == KorkApi::ConsoleValue::TypeInternalNumber)
      {
         // Copy value straight from buffer
         startData += (uintptr_t)mBuffer;
         mArgV[i+1] = KorkApi::ConsoleValue::makeRaw(((U64*)startData)[0], typeId);
      }
      else
      {
         mArgV[i+1] = KorkApi::ConsoleValue::makeTyped((void*)startData,
                                                       mStartTypes[startStack + i],
                                                       KorkApi::ConsoleValue::ZoneFunc);
      }
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
