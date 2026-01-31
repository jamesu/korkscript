//-----------------------------------------------------------------------------
// Copyright (c) 2025-2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

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

#ifndef _DATACHUNKER_H_
#include "core/dataChunker.h"
#endif
#ifndef _STREAM_H_
#include "core/stream.h"
#endif

#include "console/consoleValue.h"


/// @ingroup console_system Console System
/// @{

class ExprEvalState;
struct FunctionDecl;
class CodeBlock;
class AbstractClassRep;
class EnumTable;

namespace KorkApi
{
   struct TypeStorageInterface;
}

class IFFBlock
{
public:

   U32 ident;
protected:
   U32 size;
   
public:
   IFFBlock() : ident(0), size(0) {;}
   
   inline U32 getSize() const
   {
      return (U32)dAlignSize(size, 2);
   }
   
   inline U32 getRawSize() const { return size; }
   
   void setSize(U32 val) { size = val; }
   
   inline void seekToEnd(U32 startPos, Stream &stream)
   {
      stream.setPosition(startPos + getSize() + 8);
   }

   inline bool writePad(Stream& stream)
   {
      U32 alignSize = (U32)dAlignSize(size, 2);
      if (alignSize != size)
      {
         return stream.write((U8)0);
      }
      return true;
   }

   inline bool readPad(Stream& stream)
   {
      U32 alignSize = (U32)dAlignSize(size, 2);
      if (alignSize != size)
      {
         U8 padByte = 0;
         return stream.read(&padByte);
      }
      return true;
   }
   
   bool updateSize(Stream& stream, U32 offset)
   {
      U32 newPos = stream.getPosition();
      S64 newSize = (S64)newPos - (S64)offset - 8;
      if (newSize < 0)
      {
         return false; // ???
      }
      
      stream.setPosition(offset + 4);
      size = (U32)newSize;
      stream.write(size);
      stream.setPosition(newPos);
      return true;
   }
   
   U32 getNextBlockPosition(U32 bytesInBlock)
   {
      return (U32)dAlignSize(size, 2) - bytesInBlock;
   }

   inline bool seekNext(Stream& stream, U32 bytesInBlock)
   {
      U32 eobBytes = (U32)dAlignSize(size, 2) - bytesInBlock;
      return stream.setPosition(stream.getPosition() + eobBytes);
   }
};

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
class ConsoleSerializer;

class Dictionary
{
public:
   
   struct Entry
   {
      friend class Dictionary;
      friend class ExprEvalState;
      friend struct ConsoleFrame;
      friend class ConsoleSerializer;
      
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
      
      // Whether this is a registered variable or not
      bool mIsRegistered;
      
      U16 mEnforcedType;

   public:
      
      Entry(StringTableEntry name);
      ~Entry();
      
      inline KorkApi::ConsoleValue* getCVPtr() { return &mConsoleValue; }
   };
   
   struct HashTableData
   {
      S32 size;
      S32 count;
      Entry **data;
      Dictionary* owner;
   };
   
   HashTableData* mHashTable;
   KorkApi::VmInternal* mVm;
   
   Dictionary();
   Dictionary(KorkApi::VmInternal *state, Dictionary::HashTableData* ref=NULL);
   ~Dictionary();
   
   void clearEntry(Entry* e);

   U32 getEntryUnsignedValue(Entry* e);   
   F32 getEntryNumberValue(Entry* e);
   const char *getEntryStringValue(Entry* e);
   KorkApi::ConsoleValue getEntryValue(Entry* e);
   U16 getEntryType(Entry* e);

   void setEntryUnsignedValue(Entry* e, U64 val);
   void setEntryNumberValue(Entry* e, F32 val);
   void setEntryStringValue(Entry* e, const char *value);
   void setEntryTypeValue(Entry* e, U32 typeId, KorkApi::TypeStorageInterface * storage);
   void setEntryValue(Entry* e, KorkApi::ConsoleValue value);
   void setEntryValues(Entry* e, U32 argc, KorkApi::ConsoleValue* values);
   void setEntryType(Entry* e, U16 typeId);
   
   Entry *lookup(StringTableEntry name);
   Entry* getVariable(StringTableEntry name);
   Entry *add(StringTableEntry name);
   void setState(KorkApi::VmInternal *state, Dictionary::HashTableData* ref=NULL);
   void remove(Entry *);
   void reset();
   
   void exportVariables( const char *varString, void* userPtr, KorkApi::EnumFuncCallback outFunc );
   void deleteVariables( const char *varString );
   
   void setVariable(StringTableEntry name, const char *value);
   void setVariableValue(StringTableEntry name, KorkApi::ConsoleValue value);
   
   void remapVariables(U32 oldFiberIndex, U32 newFiberIndex);
   
   void resizeHeap(Entry* e, U32 newSize, bool force);
   void getHeapPtrSize(Entry* e, U32* size, void** ptr);
   
   U32 getCount() const
   {
      return mHashTable->count;
   }
   
   bool isOwner() const
   {
      return mHashTable->owner == this;
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
   
   /// The iterator variable.
   Dictionary::Entry* mVariable;

   U32 mIndex;

   KorkApi::ConsoleValue mData;
   KorkApi::ConsoleHeapAllocRef mHeapData; // mainly for serialization
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
   Dictionary globalVars;
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
   KorkApi::Vector< ConsoleFrame* > vmFrames;
   
   KorkApi::FiberRunResult::State mState;
   KorkApi::ConsoleValue mLastFiberValue; ///< Value yielded from function or returned to fiber
   KorkApi::ConsoleHeapAllocRef mLastFiberHeapData; // mainly for serialization

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

struct ConsoleVarRef;

/// Class which serializes console execution state. Accepts a list of
/// fibers to serialize; any referenced values will get re-mapped.
struct ConsoleSerializer
{
   struct Remap
   {
      U32 oldIndex;
      U32 newIndex;
   };

   // Main block
   static const U32 CSOB_VERSION = 1;
   static const U32 CSOB_MAGIC = makeFourCCTag('C','S','O','B');
   // Fiber
   static const U32 CEOB_VERSION = 1;
   static const U32 CEOB_MAGIC = makeFourCCTag('C','E','O','B');
   // Frame
   static const U32 CFFB_MAGIC = makeFourCCTag('C','F','F','B');
   // Dictionary
   static const U32 DICT_VERSION = 1;
   static const U32 DICT_MAGIC = makeFourCCTag('D','I','C','T');
   // DSO copy
   static const U32 DSOB_MAGIC = makeFourCCTag('D','S','O','B');
   // End of list
   static const U32 EOLB_MAGIC = makeFourCCTag('E','O','L','B');
   
   KorkApi::VmInternal* mTarget;
   Stream* mStream;
   void* mUserPtr;
   KorkApi::Vector<CodeBlock*> mCodeBlocks;
   KorkApi::Vector<Dictionary::HashTableData*> mDictionaryTables;
   KorkApi::Vector<ExprEvalState*> mFibers;
   KorkApi::Vector<Remap> mFiberRemap;
   bool mAllowId;

   ConsoleSerializer(KorkApi::VmInternal* target, void* userPtr, bool allowId, Stream* s);
   ~ConsoleSerializer();

   S32 addReferencedCodeblock(CodeBlock* block);
   S32 addReferencedDictionary(Dictionary::HashTableData* dict);
   S32 addReferencedFiber(ExprEvalState* state);
   
   CodeBlock* getReferencedCodeblock(S32 blockId);
   Dictionary::HashTableData* getReferencedDictionary(S32 dictId);
   ExprEvalState* getReferencedFiber(S32 fiberId);
   
   ExprEvalState* loadEvalState();
   bool saveEvalState(ExprEvalState* evalState);

   ConsoleFrame* loadFrame(ExprEvalState* state);
   bool writeFrame(ConsoleFrame& frame);

   KorkApi::VMObject* loadObject();
   void writeObject(KorkApi::VMObject* obj);
   
   void readObjectRef(LocalRefTrack& track);
   void writeObjectRef(LocalRefTrack& track);

   bool readVarRef(ConsoleVarRef& ref);
   bool writeVarRef(ConsoleVarRef& ref);
   
   bool readConsoleValue(KorkApi::ConsoleValue& value, KorkApi::ConsoleHeapAllocRef ref);
   bool writeConsoleValue(KorkApi::ConsoleValue& value, KorkApi::ConsoleHeapAllocRef ref);
   
   bool readHeapData(KorkApi::ConsoleHeapAllocRef& ref);
   bool writeHeapData(KorkApi::ConsoleHeapAllocRef ref);

   bool readIterStackRecord(IterStackRecord& ref);
   bool writeIterStackRecord(IterStackRecord& ref);

   bool readStringStack(StringStack& stack);
   bool writeStringStack(StringStack& stack);
   
   bool loadRelatedObjects();
   bool saveRelatedObjects();
   
   bool loadFibers();
   bool saveFibers();
   
   const char *readSTString(Stream* s, bool casesens=false);
   
   void fixupConsoleValues();

   void reset(bool ownObjects);
   
   bool isOk();
   
   Dictionary::HashTableData* loadHashTable();
   bool writeHashTable(const Dictionary::HashTableData* ht);
   
   bool read(KorkApi::Vector<ExprEvalState*> &fibers);
   bool write(KorkApi::Vector<ExprEvalState*> &fibers);
};

struct ConsoleVarRef
{
   Dictionary* dictionary;
   Dictionary::Entry *var;
   
   ConsoleVarRef() : dictionary(NULL), var(NULL)
   {
      
   }
};

#endif
