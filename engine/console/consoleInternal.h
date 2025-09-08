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

class Dictionary
{
public:
   
   struct Entry
   {
      friend class Dictionary;
      friend class ExprEvalState;
      
      StringTableEntry name;
      Entry *nextEntry;
      
      /// Usage doc string.
      const char* mUsage;
      
      /// Whether this is a constant that cannot be assigned to.
      bool mIsConstant;
      
   protected:
      
      // NOTE: This is protected to ensure no one outside
      // of this structure is messing with it.
      
      // NOTE: currently, values are copied whenever 
      // they are used in functions, so throughput with 
      // large strings in function calls can be a problem.
      KorkApi::ConsoleValue mConsoleValue;
      KorkApi::ConsoleHeapAllocRef mHeapAlloc;

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
   ExprEvalState *exprState;
   KorkApi::VmInternal* vm;
   
   StringTableEntry scopeName;
   Namespace *scopeNamespace;
   CodeBlock *code;
   U32 ip;
   
   Dictionary();
   Dictionary(ExprEvalState *state, Dictionary* ref=NULL);
   ~Dictionary();
   
   void clearEntry(Entry* e);

   U32 getEntryIntValue(Entry* e);   
   F32 getEntryFloatValue(Entry* e);
   const char *getEntryStringValue(Entry* e);
   void setEntryIntValue(Entry* e, U32 val);
   void setEntryFloatValue(Entry* e, F32 val);
   void setEntryStringValue(Entry* e, const char *value);
   
   Entry *lookup(StringTableEntry name);
   Entry* getVariable(StringTableEntry name);
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
   KorkApi::VmInternal* vmInternal;
   KorkApi::VMObject *thisObject;
   Dictionary* currentDictionary;
   Dictionary::Entry *currentVariable;
   Dictionary* copyDictionary;
   Dictionary::Entry *copyVariable;
   bool traceOn;
   
   U32 mStackDepth;
   
   ExprEvalState(KorkApi::VmInternal* vm);
   ~ExprEvalState();
   
   
   const char *getNamespaceList(Namespace *ns);
   
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
