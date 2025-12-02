//-----------------------------------------------------------------------------
// Copyright (c) 2012 GarageGames, LLC
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

#ifndef _CONSOLEINTERNAL_H_
#define _CONSOLEINTERNAL_H_

#ifndef _STRINGTABLE_H_
#include "core/stringTable.h"
#endif
#ifndef _TVECTOR_H_
#include "core/tVector.h"
#endif
#ifndef _DATACHUNKER_H_
#include "core/dataChunker.h"
#endif

#include "console/consoleValue.h"


/// @ingroup console_system Console System
/// @{

class ExprEvalState;
struct FunctionDecl;
class CodeBlock;
class AbstractClassRep;
class EnumTable;

//-----------------------------------------------------------------------------

/// A dictionary of function entries.
///
/// Namespaces are used for dispatching calls in the console system.

extern char *typeValueEmpty;
class ExprEvalState;

enum EvalConstants {
   ObjectCreationStackSize = 32,
   MaxExpectedFunctionDepth = 10,
   // NOTE: code gen uses 2 stack elememts per op, but if you do "func() + val" it needs to
   // keep a stack position around while the function is running, so we use this
   // logic to give us a reasonable maximum.
   MaxStackSize = 2 + (MaxExpectedFunctionDepth),
   MaxIterStackSize = 64,
   MaxTryStackSize = MaxStackSize * 2,
   MaxVmStackSize = MaxStackSize,
   MethodOnComponent = -2
};

struct ConsoleFrame;

class Dictionary
{
public:
   
   struct Entry
   {
      friend class Dictionary;
      friend class ExprEvalState;
      friend struct ConsoleFrame;
      
      StringTableEntry name;
      Entry *nextEntry;
      
      /// Usage doc string.
      const char* mUsage;
      
   protected:
      
      // NOTE: This is protected to ensure no one outside
      // of this structure is messing with it.
      
      // NOTE: currently, values are copied whenever 
      // they are used in functions, so throughput with 
      // large strings in function calls can be a problem.
      KorkApi::ConsoleValue mConsoleValue;
      KorkApi::ConsoleHeapAllocRef mHeapAlloc;

   protected:
      
      /// Whether this is a constant that cannot be assigned to.
      bool mIsConstant;

   public:
      
      Entry(StringTableEntry name);
      ~Entry();
   };
   
   struct HashTableData
   {
      Dictionary* owner;
      S32 size;
      S32 count;
      Entry **data;
   };
   
   HashTableData *hashTable;
   KorkApi::VmInternal* vm;
   
   Dictionary();
   Dictionary(KorkApi::VmInternal *state, Dictionary* ref=NULL);
   ~Dictionary();
   
   void clearEntry(Entry* e);

   U32 getEntryUnsignedValue(Entry* e);   
   F32 getEntryNumberValue(Entry* e);
   const char *getEntryStringValue(Entry* e);
   KorkApi::ConsoleValue getEntryValue(Entry* e);

   void setEntryUnsignedValue(Entry* e, U64 val);
   void setEntryNumberValue(Entry* e, F32 val);
   void setEntryStringValue(Entry* e, const char *value);
   void setEntryTypeValue(Entry* e, U32 typeId, void * value);
   void setEntryValue(Entry* e, KorkApi::ConsoleValue value);
   
   Entry *lookup(StringTableEntry name);
   Entry* getVariable(StringTableEntry name);
   Entry *add(StringTableEntry name);
   void setState(KorkApi::VmInternal *state, Dictionary* ref=NULL);
   void remove(Entry *);
   void reset();
   
   void exportVariables( const char *varString, const char *fileName, bool append );
   void deleteVariables( const char *varString );
   
   void setVariable(StringTableEntry name, const char *value);
   void setVariableValue(StringTableEntry name, KorkApi::ConsoleValue value);
   
   U32 getCount() const
   {
      return hashTable->count;
   }
   
   bool isOwner() const
   {
      return hashTable->owner == this;
   }
   
   /// @see Con::addVariable
   Entry* addVariable(    const char *name,
                      S32 type,
                      void *dataPtr,
                      const char* usage = NULL );
   
   /// @see Con::removeVariable
   bool removeVariable(StringTableEntry name);
   
   /// Return the best tab completion for prevText, with the length
   /// of the pre-tab string in baseLen.
   const char *tabComplete(const char *prevText, S32 baseLen, bool);
   
   /// Run integrity checks for debugging.
   void validate();
};


/// Frame data for a foreach/foreach$ loop.
struct IterStackRecord
{
   /// If true, this is a foreach$ loop; if not, it's a foreach loop.
   bool mIsStringIter;
   
   Dictionary* mDictionary;
   
   /// The iterator variable.
   Dictionary::Entry* mVariable;
   
   /// Information for an object iterator loop.
   struct ObjectPos
   {
      /// The set being iterated over.
      KorkApi::VMObject* mSet;

      /// Current index in the set.
      U32 mIndex;
   };
   
   /// Information for a string iterator loop.
   struct StringPos
   {
      /// The raw string data on the string stack.
      const char* mString;
      
      /// Current parsing position.
      U32 mIndex;
   };
   union
   {
      ObjectPos mObj;
      StringPos mStr;
   } mData;
};

struct ConsoleFrame;
struct LocalRefTrack;

struct ConsoleBasicFrame
{
   CodeBlock* code;
   Namespace* scopeNamespace;
   const char* scopeName;
   U32 ip;
};

class ExprEvalState
{
public:

   enum
   {
      TraceBufferSize = 1024
   };
   
   struct ObjectStackItem
   {
      KorkApi::VMObject* newObject;
      U32 failJump;
   };
   
   struct TryItem
   {
      U32 ip;
      U32 mask;
      U16 frameDepth;
   };
   
   U32 mAllocNumber : 24;
   U32 mGeneration : 7;
   
   /// @name Expression Evaluation
   /// @{
   
   ///
   KorkApi::VmInternal* vmInternal;
   Dictionary* globalVars;
   void* mUserPtr;
   
   IterStackRecord iterStack[MaxIterStackSize];
   F64 floatStack[MaxStackSize];
   S64 intStack[MaxStackSize];
   ObjectStackItem objectCreationStack[ObjectCreationStackSize];
   TryItem tryStack[MaxTryStackSize];
   S32 vmStack[MaxVmStackSize];
   U16 _VM;
   bool traceOn;
   U32 lastThrow;

   U32 mStackPopBreakIndex; // for telnet debugger
   
   StringStack mSTR;
   
   StringTableEntry mCurrentFile;
   StringTableEntry mCurrentRoot;

   /// The stack of callframes.  The extra redirection is necessary since Dictionary holds
   /// an interior pointer that will become invalid when the object changes address.
   Vector< ConsoleFrame* > vmFrames;
   
   KorkApi::FiberRunResult::State mState;
   KorkApi::ConsoleValue mLastFiberValue; ///< Value yielded from function or returned to fiber

   char traceBuffer[TraceBufferSize];
   
   ExprEvalState(KorkApi::VmInternal* vm);
   ~ExprEvalState();
   
   void reset();
   
   void setCreatedObject(U32 index, KorkApi::VMObject* object, U32 failJump);
   void clearCreatedObject(U32 index, LocalRefTrack& outTrack, U32* outJump);
   void clearCreatedObjects(U32 start, U32 end);
   
   
   const char *getNamespaceList(Namespace *ns);
   
   /// @}
   
   /// @name Stack Management
   /// @{
   

   void setLocalFrameVariable(StringTableEntry name, KorkApi::ConsoleValue value);
   KorkApi::ConsoleValue getLocalFrameVariable(StringTableEntry name);
   ConsoleBasicFrame getBasicFrameInfo(U32 idx);
   
   void pushFrame(StringTableEntry frameName, Namespace *ns, StringTableEntry packageName, CodeBlock* block, U32 ip);
   void popFrame();

   bool clearStringStack(ConsoleFrame& frame, bool clearValue);
   
   /// Puts a reference to an existing stack frame
   /// on the top of the stack.
   void pushFrameRef(S32 stackIndex, CodeBlock* codeBlock, U32 ip);

   void pushMinStackDepth()
   {
      vmStack[_VM++] = vmFrames.size()-1;
   }

   void popMinStackDepth()
   {
      _VM--;
   }

   S32 getMinStackDepth() const
   {
      return _VM == 0 ? -1 : vmStack[_VM-1];
   }
   
   S32 getMinStackSize() const
   {
      return _VM == 0 ? 0 : vmStack[_VM-1]+1;
   }

   void pushVmStack(U16 size);
   void popVmStack();
   
   // Fiber API
   KorkApi::FiberRunResult runVM(); // Runs VM
   void suspend(); // Suspends fiber; NOTE: use return value from function to set the fiber value.
   KorkApi::FiberRunResult resume(KorkApi::ConsoleValue value); // Resumes fiber
   bool handleThrow(S32 throwIdx, TryItem* info, S32 minStackPos);
   void throwMask(U32 mask);
   
   ConsoleFrame& getCurrentFrame();
   
   /// @}
   
   /// Run integrity checks for debugging.
   void validate();
};

#endif
