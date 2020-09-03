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
class EnumTable;

//-----------------------------------------------------------------------------

/// A dictionary of function entries.
///
/// Namespaces are used for dispatching calls in the console system.

class Namespace
{
   enum {
        MaxActivePackages = 512,
   };

   static U32 mNumActivePackages;
   static U32 mOldNumActivePackages;
   static StringTableEntry mActivePackages[MaxActivePackages];
   public:
   StringTableEntry mName;
   StringTableEntry mPackage;

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

      const char *execute(S32 argc, const char **argv, ExprEvalState *state);

   };
   Entry *mEntryList;

   Entry **mHashTable;
   U32 mHashSize;
   U32 mHashSequence;  ///< @note The hash sequence is used by the autodoc console facility
                     ///        as a means of testing reference asstate.

   Namespace();
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

   static U32 mCacheSequence;
   static DataChunker mCacheAllocator;
   static DataChunker mAllocator;
   static void trashCache();
   static Namespace *mNamespaceList;
   static Namespace *mGlobalNamespace;

   static void init();
   static void shutdown();
   static Namespace *global();

   static Namespace *find(StringTableEntry name, StringTableEntry package=NULL);

   static bool canTabComplete(const char *prevText, const char *bestMatch, const char *newText, S32 baseLen, bool fForward);

   static void activatePackage(StringTableEntry name);
   static void deactivatePackage(StringTableEntry name);
   static void dumpClasses( bool dumpScript = true, bool dumpEngine = true );
   static void dumpFunctions( bool dumpScript = true, bool dumpEngine = true );
   static void printNamespaceEntries(Namespace * g, bool dumpScript = true, bool dumpEngine = true);
   static void unlinkPackages();
   static void relinkPackages();
   static bool isPackage(StringTableEntry name);
};

typedef VectorPtr<Namespace::Entry *>::iterator NamespaceEntryListIterator;
extern char *typeValueEmpty;

class Dictionary
{
public:
   
   struct Entry
   {
      friend class Dictionary;
      
      enum
      {
         TypeInternalInt = -3,
         TypeInternalFloat = -2,
         TypeInternalString = -1,
      };
      
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
      
      U32 getIntValue()
      {
         if(type <= TypeInternalString)
            return ival;
         else
            return dAtoi(Con::getData(type, dataPtr, 0, enumTable));
      }
      
      F32 getFloatValue()
      {
         if(type <= TypeInternalString)
            return fval;
         else
            return dAtof(Con::getData(type, dataPtr, 0, enumTable));
      }
      
      const char *getStringValue()
      {
         if(type == TypeInternalString)
            return sval;
         if(type == TypeInternalFloat)
            return Con::getData(TypeF32, &fval, 0);
         else if(type == TypeInternalInt)
            return Con::getData(TypeS32, &ival, 0);
         else
            return Con::getData(type, dataPtr, 0, enumTable);
      }
      
      void setIntValue(U32 val)
      {
         if( mIsConstant )
         {
            Con::errorf( "Cannot assign value to constant '%s'.", name );
            return;
         }
         
         if(type <= TypeInternalString)
         {
            fval = (F32)val;
            ival = val;
            if(sval != typeValueEmpty)
            {
               dFree(sval);
               sval = typeValueEmpty;
            }
            type = TypeInternalInt;
         }
         else
         {
            const char *dptr = Con::getData(TypeS32, &val, 0);
            Con::setData(type, dataPtr, 0, 1, &dptr, enumTable);
         }
      }
      
      void setFloatValue(F32 val)
      {
         if( mIsConstant )
         {
            Con::errorf( "Cannot assign value to constant '%s'.", name );
            return;
         }
         
         if(type <= TypeInternalString)
         {
            fval = val;
            ival = static_cast<U32>(val);
            if(sval != typeValueEmpty)
            {
               dFree(sval);
               sval = typeValueEmpty;
            }
            type = TypeInternalFloat;
         }
         else
         {
            const char *dptr = Con::getData(TypeF32, &val, 0);
            Con::setData(type, dataPtr, 0, 1, &dptr, enumTable);
         }
      }
      
      void setStringValue(const char *value);
   };
   
   struct HashTableData
   {
      Dictionary* owner;
      S32 size;
      S32 count;
      Entry **data;
   };
   
   HashTableData *hashTable;
   ExprEvalState *exprState;
   
   StringTableEntry scopeName;
   Namespace *scopeNamespace;
   CodeBlock *code;
   U32 ip;
   
   Dictionary();
   Dictionary(ExprEvalState *state, Dictionary* ref=NULL);
   ~Dictionary();
   
   Entry *lookup(StringTableEntry name);
   Entry *add(StringTableEntry name);
   void setState(ExprEvalState *state, Dictionary* ref=NULL);
   void remove(Entry *);
   void reset();
   
   void exportVariables( const char *varString, const char *fileName, bool append );
   void deleteVariables( const char *varString );
   
   void setVariable(StringTableEntry name, const char *value);
   const char *getVariable(StringTableEntry name, bool *valid = NULL);
   
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
   SimObject *thisObject;
   Dictionary::Entry *currentVariable;
   Dictionary::Entry *copyVariable;
   bool traceOn;
   
   U32 mStackDepth;
   
   ExprEvalState();
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
