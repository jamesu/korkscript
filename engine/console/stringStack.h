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

#ifndef _STRINGSTACK_H_
#define _STRINGSTACK_H_

#include "platform/platform.h"
#include "core/stringTable.h"
#include "console/consoleValue.h"

/// Core stack for interpreter operations.
///
/// This class provides some powerful semantics for working with strings, and is
/// used heavily by the console interpreter.
struct StringStack
{
/*
   NOTE: scenarios here

   1. Native -> Native (only mFunctionOffset used)
   2. Native -> Script (only mFunctionOffset, setStringValue, getStringValue used)
   3. Script -> Native (pushFrame, setStringValue, push, popFrame used)
   4. Script -> Script (pushFrame, setStringValue, push, popFrame used)

   The return value buffer is now in VmInternal. This is primarily used 
   when returning values from native functions.
   EVERYTHING ends up either as a value in the stack OR a heap allocated variable.
*/

   enum {
      // NOTE: MaxStackDepth should be at least MaxStackSize; the other consideration 
      // here is if you have a function call which calls functions for parameters, 
      // it needs to factor in MaxArgs since for example if you have MaxArgs params and 
      // the last parameter calls a function with more than one param, you need 
      // at least MaxArgs space available on the stack.
      // MaxFrameDepth also needs to be at least MaxStackSize
      MaxStackDepth = 16,
      MaxFrameDepth = 16,
      MaxArgs = 20,
      ReturnBufferSpace = 512
   };

   char *mBuffer;
   U32   mBufferSize;
   const char *mArgVStr[MaxArgs];
   KorkApi::ConsoleValue mArgV[MaxArgs];
   U32 mFrameOffsets[MaxFrameDepth]; // this is FRAME offset
   U32 mStartOffsets[MaxStackDepth]; // this is FUNCTION PARAM offset
   U16 mStartTypes[MaxStackDepth];   // this is annotated type
   U64 mStartValues[MaxStackDepth];  // this is the cv value
   U64 mValue; // current cv value
   U16 mType;  // current type
   
   U16 mFuncId;
   U32 mNumFrames;

   U32 mStart;
   U32 mLen;
   U32 mStartStackSize;
   U32 mFunctionOffset;
   
   KorkApi::ConsoleValue::AllocBase* mAllocBase;
   KorkApi::TypeInfo** mTypes;


   void reset()
   {
      mStart = 0;
      mLen = 0;
      mNumFrames = 0;
      mStartStackSize = 0;
      mFunctionOffset = 0;
   }

   void validateBufferSize(U32 size)
   {
      if(size > mBufferSize)
      {
         mBufferSize = size + 2048;
         mBuffer = (char *) dRealloc(mBuffer, mBufferSize);
         if (mAllocBase)
         {
            mAllocBase->func[mFuncId] = mBuffer;
         }
      }
   }

   StringStack(KorkApi::ConsoleValue::AllocBase* allocBase = NULL, KorkApi::TypeInfo** typeInfos = NULL)
   {
      mBufferSize = 0;
      mBuffer = NULL;
      mFuncId = 0;
      mNumFrames = 0;
      mStart = 0;
      mLen = 0;
      mStartStackSize = 0;
      mFunctionOffset = 0;
      mAllocBase = allocBase;
      mType = KorkApi::ConsoleValue::TypeInternalString;
      mTypes = typeInfos;
   }

   ~StringStack()
   {
      if (mBuffer)
      {
         dFree(mBuffer);
      }
   }

   void initForFiber(U32 fiberId)
   {
      mFuncId = fiberId;
      validateBufferSize(8192);
   }

   /// Set the top of the stack to be an integer value.
   void setUnsignedValue(U32 i)
   {
      validateBufferSize(mStart + 16);
      mLen = 0;
      *((U64*)&mValue) = i;
      mType = KorkApi::ConsoleValue::TypeInternalUnsigned;
   }

   /// Set the top of the stack to be a float value.
   void setNumberValue(F64 v)
   {
      validateBufferSize(mStart + 16);
      mLen = 0;
      *((F64*)&mValue) = v;
      mType = KorkApi::ConsoleValue::TypeInternalNumber;
   }

   /// Return a temporary buffer we can use to return data.
   ///
   /// @note This clobbers anything in our buffers!
   KorkApi::ConsoleValue getFrameBuffer(U16 valueType, U32 size)
   {
      KorkApi::ConsoleValue ret;
      validateBufferSize(mStart + size);
      ret.setTyped((U64)(mStart), valueType, (KorkApi::ConsoleValue::Zone)((U32)KorkApi::ConsoleValue::ZoneFunc + mFuncId));
      return ret;
   }

   /// Return a buffer we can use for arguments.
   ///
   /// This updates the function offset.
   KorkApi::ConsoleValue getFuncBuffer(U16 valueType, U32 size)
   {
      KorkApi::ConsoleValue ret;
      validateBufferSize(mStart + mFunctionOffset + size);
      ret.setTyped((U64)(mStart + mFunctionOffset), valueType, KorkApi::ConsoleValue::ZoneFunc);
      mFunctionOffset += size;
      return ret;
   }

   /// Clear the function offset.
   void clearFunctionOffset()
   {
      mFunctionOffset = 0;
   }

   /// Set a string value on the top of the stack.
   void setStringValue(const char *s)
   {
      if(!s)
      {
         mLen = 0;
         mBuffer[mStart] = 0;
         return;
      }
      mLen = dStrlen(s);
      mType = KorkApi::ConsoleValue::TypeInternalString;
      
      if ((mBuffer + mStart) != s)
      {
         validateBufferSize(mStart + mLen + 2);
         dStrcpy(mBuffer + mStart, s);
      }
   }

   /// Set a string value on the top of the stack.
   void setConsoleValue(KorkApi::ConsoleValue v)
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
         // TOFIX: needs to use storage api
         valueBase = v.evaluatePtr(*mAllocBase);
         if (valueBase != (mBuffer + mStart)) // account for setting same head
         {
            if (v.typeId == KorkApi::ConsoleValue::TypeInternalString)
            {
               setStringValue((const char*)valueBase);
            }
            else
            {
               mLen = (U32)((*mTypes)[v.typeId].valueSize);
               memcpy(mBuffer + mStart, valueBase, mLen);
            }
         }
         mType = v.typeId;
      }
   }
   
   void setConsoleValueSize(U32 size)
   {
      mLen = size;
   }

   void setConsoleValueValue(U64 value)
   {
      mValue = value;
   }

   void setStringIntValue(U32 value)
   {
      char shortBuf[16];
      dSprintf(shortBuf, sizeof(shortBuf), "%i", value);
      setStringValue(shortBuf);
   }

   void setStringFloatValue(F64 value)
   {
      char shortBuf[16];
      dSprintf(shortBuf, sizeof(shortBuf), "%g", value);
      setStringValue(shortBuf);
   }
   
   /// Get the top of the stack, as a StringTableEntry.
   ///
   /// @note Don't free this memory!
   inline StringTableEntry getSTValue()
   {
      return StringTable->insert(mBuffer + mStart);
   }

   /// Get an integer representation of the top of the stack.
   inline U32 getIntValue()
   {
      return dAtoi(mBuffer + mStart);
   }

   /// Get a float representation of the top of the stack.
   inline F64 getFloatValue()
   {
      return dAtof(mBuffer + mStart);
   }

   /// Get a string representation of the top of the stack.
   ///
   /// @note This returns a pointer to the actual top of the stack, be careful!
   inline const char *getStringValue()
   {
      return mBuffer + mStart;
   }

   inline KorkApi::ConsoleValue getConsoleValue()
   {
      if (mType == KorkApi::ConsoleValue::TypeInternalString || 
         mType >= KorkApi::ConsoleValue::TypeBeginCustom)
      {
         // Strings and types are put on stack
         return KorkApi::ConsoleValue::makeRaw(&mBuffer[mStart] - &mBuffer[0], mType, (KorkApi::ConsoleValue::Zone)(KorkApi::ConsoleValue::ZoneFunc + mFuncId));
      }
      else
      {
         // Raw values are just placed directly on the stack
         return KorkApi::ConsoleValue::makeRaw(*((U64*)&mBuffer[mStart]), mType, (KorkApi::ConsoleValue::Zone)(KorkApi::ConsoleValue::ZoneFunc + mFuncId));
      }
   }
   
   inline KorkApi::ConsoleValue getStackConsoleValue(U32 offset)
   {
      U16 typeId = mStartTypes[offset];
      U64 typeValue = mStartValues[offset];
   
      if (typeId == KorkApi::ConsoleValue::TypeInternalUnsigned ||
          typeId == KorkApi::ConsoleValue::TypeInternalNumber)
      {
         // Copy value straight from stack
         return KorkApi::ConsoleValue::makeRaw(typeValue, typeId, KorkApi::ConsoleValue::ZonePacked);
      }
      else
      {
         UINTPTR startData = mStartOffsets[offset];
         return KorkApi::ConsoleValue::makeTyped((void*)startData,
                                                       mStartTypes[offset],
                                                       (KorkApi::ConsoleValue::Zone)(KorkApi::ConsoleValue::ZoneFunc + mFuncId));
      }
   }

   /// Advance the start stack, placing a zero length string on the top.
   ///
   /// @note You should use StringStack::push, not this, if you want to
   ///       properly push the stack.
   void advance()
   {
      AssertFatal(mStartStackSize < MaxStackDepth-1, "Stack overflow!");
      mStartTypes[mStartStackSize] = mType;
      mStartValues[mStartStackSize] = mValue;
      mStartOffsets[mStartStackSize++] = mStart;
      mStart += mLen;
      mLen = 0;
      mType = KorkApi::ConsoleValue::TypeInternalString; // reset
   }

   /// Advance the start stack, placing a single character, null-terminated strong
   /// on the top.
   ///
   /// @note You should use StringStack::push, not this, if you want to
   ///       properly push the stack.
   void advanceChar(char c)
   {
      AssertFatal(mStartStackSize < MaxStackDepth-1, "Stack overflow!");
      mStartTypes[mStartStackSize] = mType;
      mStartValues[mStartStackSize] = mValue;
      mStartOffsets[mStartStackSize++] = mStart;
      mStart += mLen;
      mBuffer[mStart] = c;
      mBuffer[mStart+1] = 0;
      mStart += 1;
      mLen = 0;
      mType = KorkApi::ConsoleValue::TypeInternalString; // reset
   }

   /// Push the stack, placing a zero-length string on the top.
   void push()
   {
      advanceChar(0);
   }

   inline void setTypedLen(U8 typeId, U32 newlen)
   {
      mType = typeId;
      mLen = newlen;
   }

   /// Pop the start stack.
   void rewind()
   {
      if (mType != KorkApi::ConsoleValue::TypeInternalString) // termimate regardless if incorrect type
      {
         mBuffer[mStart] = 0;
      }

      mStart = mStartOffsets[--mStartStackSize];
      mType = mStartTypes[mStartStackSize];
      mValue = mStartValues[mStartStackSize];
      mLen = getHeadLength();
   }

   // Terminate the current string, and pop the start stack.
   void rewindTerminate()
   {
      mBuffer[mStart] = 0;
      mStart = mStartOffsets[--mStartStackSize];
      mType = mStartTypes[mStartStackSize];
      mValue = mStartValues[mStartStackSize];
      mLen = getHeadLength();
   }

   U32 getHeadLength() const
   {
      if (mType == KorkApi::ConsoleValue::TypeInternalString)
      {
         return dStrlen(mBuffer + mStart);
      }
      else if (mType < KorkApi::ConsoleValue::TypeBeginCustom)
      {
         return 0;
      }
      else
      {
         return (U32)((*mTypes)[mType].valueSize);
      }
   }

   /// Compare 1st and 2nd items on stack, consuming them in the process,
   /// and returning true if they matched, false if they didn't.
   U32 compare()
   {
      // Figure out the 1st and 2nd item offsets.
      U32 oldStart = mStart;
      U8 oldType = mType;
      mStart = mStartOffsets[--mStartStackSize];
      mType = mStartTypes[mStartStackSize];
      mValue = mStartValues[mStartStackSize];

      // Compare current and previous strings.
      U32 ret = mType == oldType ? !dStricmp(mBuffer + mStart, mBuffer + oldStart) : 0;

      // Put an empty string on the top of the stack.
      mLen = 0;
      mBuffer[mStart] = 0;

      return ret;
   }

   
   void pushFrame()
   {
      AssertFatal((mNumFrames < MaxFrameDepth-1) && (mStartStackSize < MaxStackDepth-1), "Stack overflow!");
      mFrameOffsets[mNumFrames++] = mStartStackSize;
      mStartTypes[mStartStackSize] = mType;
      mStartValues[mStartStackSize] = mValue;
      mStartOffsets[mStartStackSize++] = mStart;
      mStart += ReturnBufferSpace;
      validateBufferSize(mStart+1);
      // terminate start just in case we get an early exit
      mBuffer[mStart] = '\0';
      //Con::printf("StringStack::pushFrame");
   }

   void popFrame()
   {
      AssertFatal(mNumFrames > 0, "Stack underflow!");
      mStartStackSize = mFrameOffsets[--mNumFrames];
      mStart = mStartOffsets[mStartStackSize];
      mLen = 0;
      mType = mStartTypes[mStartStackSize]; // reset
      mValue = mStartValues[mStartStackSize];
      //Con::printf("StringStack::popFrame");
   }

   /// Get the arguments for a function call from the stack.
   void getArgcArgv(StringTableEntry name, U32 *argc, KorkApi::ConsoleValue **in_argv, bool popStackFrame = false);
   void convertArgv(KorkApi::VmInternal* vm, U32 numArgs, const char ***in_argv);
   
   void performOp(U32 op, KorkApi::Vm* vm, KorkApi::TypeInfo* typeInfo);
   void performOpReverse(U32 op, KorkApi::Vm* vm, KorkApi::TypeInfo* typeInfo);
   void performUnaryOp(U32 op, KorkApi::Vm* vm, KorkApi::TypeInfo* typeInfo);


   static void convertArgs(KorkApi::VmInternal* vm, U32 numArgs, KorkApi::ConsoleValue* args, const char **outArgs);
   static void convertArgsReverse(KorkApi::VmInternal* vm, U32 numArgs, const char **args, KorkApi::ConsoleValue* outArgs);
};

#endif
