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
#ifndef _CONSOLETYPES_H_
#include "console/consoleTypes.h"
#endif
#ifndef _DATACHUNKER_H_
#include "core/dataChunker.h"
#endif

/// @ingroup console_system Console System
/// @{

class ExprEvalState;
struct FunctionDecl;
class CodeBlock;
class AbstractClassRep;
struct EnumTable;
class StringStack;

//-----------------------------------------------------------------------------

/// A dictionary of function entries.
///
/// Namespaces are used for dispatching calls in the console system.

class Namespace
{
   public:
   StringTableEntry mName;
   StringTableEntry mPackage;

   CodeBlockWorld* mWorld;
   Namespace *mParent;
   Namespace *mNext;
   AbstractClassRep *mClassRep;
   U32 mRefCountToParent;
   const char* mUsage;
   // Script defined usage strings need to be cleaned up. This
   // field indicates whether or not the usage was set from script.
   bool mCleanUpUsage;

   struct Entry
   {
      enum {
         GroupMarker                  = -3,
         OverloadMarker               = -2,
         InvalidFunctionType          = -1,
         ScriptFunctionType,
         StringCallbackType,
         IntCallbackType,
         FloatCallbackType,
         VoidCallbackType,
         BoolCallbackType
      };

      Namespace *mNamespace;
      Entry *mNext;
      StringTableEntry mFunctionName;
      S32 mType;
      S32 mMinArgs;
      S32 mMaxArgs;
      const char *mUsage;
      StringTableEntry mPackage;

      CodeBlock *mCode;
      U32 mFunctionOffset;
      union CallbackUnion {
         StringCallback mStringCallbackFunc;
         IntCallback mIntCallbackFunc;
         VoidCallback mVoidCallbackFunc;
         FloatCallback mFloatCallbackFunc;
         BoolCallback mBoolCallbackFunc;
         const char* mGroupName;
      } cb;
      Entry();
      void clear();

      const char *execute(CodeBlockWorld* world, S32 argc, const char **argv, ExprEvalState *state);

   };
   Entry *mEntryList;

   Entry **mHashTable;
   U32 mHashSize;
   U32 mHashSequence;  ///< @note The hash sequence is used by the autodoc console facility
                     ///        as a means of testing reference asstate.

   Namespace(CodeBlockWorld* world);
   ~Namespace();
   void addFunction(StringTableEntry name, CodeBlock *cb, U32 functionOffset, const char* usage = NULL);
   void addCommand(StringTableEntry name,StringCallback, const char *usage, S32 minArgs, S32 maxArgs);
   void addCommand(StringTableEntry name,IntCallback, const char *usage, S32 minArgs, S32 maxArgs);
   void addCommand(StringTableEntry name,FloatCallback, const char *usage, S32 minArgs, S32 maxArgs);
   void addCommand(StringTableEntry name,VoidCallback, const char *usage, S32 minArgs, S32 maxArgs);
   void addCommand(StringTableEntry name,BoolCallback, const char *usage, S32 minArgs, S32 maxArgs);

   void addOverload(const char *name, const char* altUsage);

   void markGroup(const char* name, const char* usage);
   char * lastUsage;

   void getEntryList(Vector<Entry *> *);

   Entry *lookup(StringTableEntry name);
   Entry *lookupRecursive(StringTableEntry name);
   Entry *createLocalEntry(StringTableEntry name);
   void buildHashTable();
   void clearEntries();
   bool classLinkTo(Namespace *parent);
   bool unlinkClass(Namespace *parent);

   const char *tabComplete(const char *prevText, S32 baseLen, bool fForward);
};

typedef VectorPtr<Namespace::Entry *>::iterator NamespaceEntryListIterator;
extern char *typeValueEmpty;

class Dictionary
{
public:
      
   enum
   {
      TypeInternalInt = -3,
      TypeInternalFloat = -2,
      TypeInternalString = -1,
   };
   
   struct Entry
   {
      friend class Dictionary;
      
      StringTableEntry name;
      Entry *nextEntry;
      S32 type;
      
      /// Usage doc string.
      const char* mUsage;
      
      /// Whether this is a constant that cannot be assigned to.
      bool mIsConstant;
      
   protected:
      
      // NOTE: This is protected to ensure no one outside
      // of this structure is messing with it.
      
#pragma warning( push )
#pragma warning( disable : 4201 ) // warning C4201: nonstandard extension used : nameless struct/union
      
      // An variable is either a real dynamic type or
      // its one exposed from C++ using a data pointer.
      //
      // We use this nameless union and struct setup
      // to optimize the memory usage.
      union
      {
         struct
         {
            char *sval;
            U32 ival;  // doubles as strlen when type is TypeInternalString
            F32 fval;
            U32 bufferLen;
         };
         
         struct
         {
            /// The real data pointer.
            void *dataPtr;
            
            /// The enum lookup table for enumerated types.
            const EnumTable *enumTable;
         };
      };
      
#pragma warning( pop ) // C4201
      
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

   struct LookupEntry
   {
      Dictionary* dictionary;
      Dictionary::Entry* entry;

      LookupEntry() : dictionary(NULL), entry(NULL) {;}

      inline void setNull() { dictionary = NULL; entry = NULL; }
   };
   
   HashTableData *hashTable;
   ExprEvalState *exprState;
   
   StringTableEntry scopeName;
   Namespace *scopeNamespace;
   CodeBlock *code;
   CodeBlockWorld* world;
   U32 ip;
   
   Dictionary(CodeBlockWorld* world);
   Dictionary(ExprEvalState *state, Dictionary* ref=NULL);
   ~Dictionary();
   
   Entry *lookup(StringTableEntry name);
   Entry *add(StringTableEntry name);

   inline LookupEntry lookupDE(StringTableEntry name)
   {
      LookupEntry ent;
      ent.dictionary = this;
      ent.entry = lookup(name);
   }

   inline LookupEntry addDE(StringTableEntry name)
   {
      LookupEntry ent;
      ent.dictionary = this;
      ent.entry = add(name);
   }

   void setState(ExprEvalState *state, Dictionary* ref=NULL);
   void remove(Entry *);
   void reset();
   
   void exportVariables( const char *varString, const char *fileName, bool append );
   void deleteVariables( const char *varString );
   
   void setVariable(StringTableEntry name, const char *value);
   const char *getVariable(StringTableEntry name, bool *valid = NULL);


   // Moved here to simplify world referencing

   U32 getIntValue(Entry* entry);
   
   F32 getFloatValue(Entry* entry);
   
   const char *getStringValue(Entry* entry);
   
   void setIntValue(Entry* entry, U32 val);
   
   void setFloatValue(Entry* entry, F32 val);
   
   void setStringValue(Entry* entry, const char *value);
   
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


class ExprEvalState
{
public:
   /// @name Expression Evaluation
   /// @{
   
   ///
   ConsoleObject *thisObject;
   Dictionary::LookupEntry currentVariable;
   Dictionary::LookupEntry copyVariable;
   bool traceOn;
   CodeBlockWorld* mWorld;
   
   U32 mStackDepth;

   enum EvalConstants {
      MaxStackSize = 1024,
      MethodOnComponent = -2
   };


   /// Frame data for a foreach/foreach$ loop.
   struct IterStackRecord
   {
      /// If true, this is a foreach$ loop; if not, it's a foreach loop.
      bool mIsStringIter;
      
      /// The iterator variable.
      Dictionary::LookupEntry mVariable;
      
      /// Information for an object iterator loop.
      struct ObjectPos
      {
         /// The set being iterated over.
         ConsoleObject* mSet;

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

   IterStackRecord iterStack[ MaxStackSize ];

   F64 floatStack[MaxStackSize];
   S64 intStack[MaxStackSize];

   StringStack STR;

   U32 _FLT;
   U32 _UINT;
   U32 _ITER;    ///< Stack pointer for iterStack.

   
   ExprEvalState(CodeBlockWorld* world);
   ~ExprEvalState();
   
   /// @}
   
   /// @name Stack Management
   /// @{
   
   /// The stack of callframes.  The extra redirection is necessary since Dictionary holds
   /// an interior pointer that will become invalid when the object changes address.
   Vector< Dictionary* > stack;
   
   ///
   Dictionary globalVars;
   
   void setCurVarName(StringTableEntry name);
   void setCurVarNameCreate(StringTableEntry name);
   
   S32 getIntVariable();
   F64 getFloatVariable();
   const char *getStringVariable();
   void setIntVariable(S32 val);
   void setFloatVariable(F64 val);
   void setStringVariable(const char *str);
   void setCopyVariable();
   
   void pushFrame(StringTableEntry frameName, Namespace *ns);
   void popFrame();
   
   /// Puts a reference to an existing stack frame
   /// on the top of the stack.
   void pushFrameRef(S32 stackIndex);
   
   U32 getStackDepth() const
   {
      return mStackDepth;
   }
   
   Dictionary& getCurrentFrame()
   {
      return *( stack[ mStackDepth - 1 ] );
   }
   
   /// @}
   
   /// Run integrity checks for debugging.
   void validate();
};

#endif
