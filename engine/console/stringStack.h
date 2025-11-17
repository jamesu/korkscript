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
   enum {
      MaxStackDepth = 16, // should be at least MaxStackSize
      MaxArgs = 20,
      ReturnBufferSpace = 512
   };
   char *mBuffer;
   U32   mBufferSize;
   const char *mArgVStr[MaxArgs];
   KorkApi::ConsoleValue mArgV[MaxArgs];
   U32 mFrameOffsets[MaxStackDepth]; // this is FRAME offset
   U32 mStartOffsets[MaxStackDepth]; // this is FUNCTION PARAM offset
   U8 mStartTypes[MaxStackDepth]; // this is annotated type
   U8 mType; // current type
   
   KorkApi::ConsoleValue::AllocBase* mAllocBase;
   KorkApi::TypeInfo* mTypes;

   U32 mNumFrames;

   U32 mStart;
   U32 mLen;
   U32 mStartStackSize;
   U32 mFunctionOffset;

   U32 mReturnBufferSize;
   char *mReturnBuffer;

   void validateBufferSize(U32 size)
   {
      if(size > mBufferSize)
      {
         mBufferSize = size + 2048;
         mBuffer = (char *) dRealloc(mBuffer, mBufferSize);
         if (mAllocBase) mAllocBase->func = mBuffer;
      }
   }
   void validateReturnBufferSize(U32 size)
   {
      if(size > mReturnBufferSize)
      {
         mReturnBufferSize = size + 2048;
         mReturnBuffer = (char *) dRealloc(mReturnBuffer, mReturnBufferSize);
         if (mAllocBase) mAllocBase->arg = mBuffer;
      }
   }
   
   StringStack(KorkApi::ConsoleValue::AllocBase* allocBase = NULL, KorkApi::TypeInfo* typeInfos = NULL)
   {
      mBufferSize = 0;
      mBuffer = NULL;
      mNumFrames = 0;
      mStart = 0;
      mLen = 0;
      mStartStackSize = 0;
      mFunctionOffset = 0;
      mAllocBase = allocBase;
      validateBufferSize(8192);
      validateReturnBufferSize(2048);
      mType = KorkApi::ConsoleValue::TypeInternalString;
      mTypes = typeInfos;
   }

   /// Set the top of the stack to be an integer value.
   void setUnsignedValue(U32 i)
   {
      validateBufferSize(mStart + 16);
      mLen = 8;
      *((U64*)&mBuffer[mStart]) = i;
      mType = KorkApi::ConsoleValue::TypeInternalUnsigned;
   }

   /// Set the top of the stack to be a float value.
   void setNumberValue(F64 v)
   {
      validateBufferSize(mStart + 16);
      mLen = 8;
      *((F64*)&mBuffer[mStart]) = v;
      mType = KorkApi::ConsoleValue::TypeInternalNumber;
   }

   /// Return a temporary buffer we can use to return data.
   ///
   /// @note This clobbers anything in our buffers!
   KorkApi::ConsoleValue getReturnBuffer(U16 valueType, U32 size)
   {
      KorkApi::ConsoleValue ret;
      if(size > ReturnBufferSpace)
      {
         validateReturnBufferSize(size);
         ret.setTyped((U64)0, valueType, KorkApi::ConsoleValue::ZoneReturn);
         return ret;
      }
      else
      {
         validateBufferSize(mStart + size);
         ret.setTyped((U64)(mStart), valueType, KorkApi::ConsoleValue::ZoneFunc);
         return ret;
      }
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

      validateBufferSize(mStart + mLen + 2);
      dStrcpy(mBuffer + mStart, s);

   }

   /// Set a string value on the top of the stack.
   void setConsoleValue(KorkApi::ConsoleValue v)
   {
      mType = v.typeId;
      void* valueBase = NULL;
      
      if (v.typeId == KorkApi::ConsoleValue::TypeInternalUnsigned ||
          v.typeId == KorkApi::ConsoleValue::TypeInternalNumber)
      {
         mLen = 8;
         validateBufferSize(mStart + mLen);
         *((U64*)&mBuffer[mStart]) = v.cvalue;
         return;
      }
      else
      {
         valueBase = v.evaluatePtr(*mAllocBase);
         if (v.typeId == KorkApi::ConsoleValue::TypeInternalString)
         {
            mLen = dStrlen((const char*)valueBase);
         }
         else
         {
            mLen = mTypes[v.typeId].size;
         }
      }
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
      if (mType == KorkApi::ConsoleValue::TypeInternalString || mType >= KorkApi::ConsoleValue::TypeBeginCustom)
      {
         // Strings and types are put on stack
         return KorkApi::ConsoleValue::makeRaw(&mBuffer[mStart] - &mBuffer[0], mType, KorkApi::ConsoleValue::ZoneFunc);
      }
      else
      {
         // Raw values are just placed directly on the stack
         return KorkApi::ConsoleValue::makeRaw(*((U64*)&mBuffer[mStart]), mType, KorkApi::ConsoleValue::ZoneFunc);
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
      mStart = mStartOffsets[--mStartStackSize];
      mLen = dStrlen(mBuffer + mStart);
      mType = mStartTypes[mStartStackSize];
   }

   // Terminate the current string, and pop the start stack.
   void rewindTerminate()
   {
      mBuffer[mStart] = 0;
      mStart = mStartOffsets[--mStartStackSize];
      mLen   = dStrlen(mBuffer + mStart);
      mType = mStartTypes[mStartStackSize];
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

      // Compare current and previous strings.
      U32 ret = mType == oldType ? !dStricmp(mBuffer + mStart, mBuffer + oldStart) : 0;

      // Put an empty string on the top of the stack.
      mLen = 0;
      mBuffer[mStart] = 0;

      return ret;
   }

   
   void pushFrame()
   {
      AssertFatal((mNumFrames < MaxStackDepth-1) && (mStartStackSize < MaxStackDepth-1), "Stack overflow!");
      mFrameOffsets[mNumFrames++] = mStartStackSize;
      mStartOffsets[mStartStackSize++] = mStart;
      mStart += ReturnBufferSpace;
      validateBufferSize(0);
      //Con::printf("StringStack::pushFrame");
   }

   void popFrame()
   {
      AssertFatal(mNumFrames > 0, "Stack underflow!");
      mStartStackSize = mFrameOffsets[--mNumFrames];
      mStart = mStartOffsets[mStartStackSize];
      mLen = 0;
      mType = KorkApi::ConsoleValue::TypeInternalString; // reset
      //Con::printf("StringStack::popFrame");
   }

   /// Get the arguments for a function call from the stack.
   void getArgcArgv(StringTableEntry name, U32 *argc, KorkApi::ConsoleValue **in_argv, bool popStackFrame = false);
   void convertArgv(KorkApi::VmInternal* vm, U32 numArgs, const char ***in_argv);


   static void convertArgs(KorkApi::VmInternal* vm, U32 numArgs, KorkApi::ConsoleValue* args, const char **outArgs);
   static void convertArgsReverse(KorkApi::VmInternal* vm, U32 numArgs, const char **args, KorkApi::ConsoleValue* outArgs);
};

#endif
