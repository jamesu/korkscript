#pragma once
//-----------------------------------------------------------------------------
// Copyright (c) 2025-2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

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


#include "console/consoleInternal.h"
#include "embed/api.h"
#include "embed/internalApi.h"

namespace KorkApi
{
class VmInternal;
}

class Namespace
{
   public:
   StringTableEntry mName;
   StringTableEntry mPackage;

   KorkApi::VmInternal* mVmInternal;

   Namespace *mParent;
   Namespace *mNext;
   void* mUserPtr;
   U32 mRefCountToParent;
   
   const char* mUsage;
   KorkApi::String mDynamicUsage;

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
         BoolCallbackType,
         ValueCallbackType
      };

      Namespace *mNamespace;
      Entry *mNext;
      StringTableEntry mFunctionName;
      S32 mType;
      S32 mMinArgs;
      S32 mMaxArgs;
      const char *mUsage;
      KorkApi::String mDynamicUsage;
      StringTableEntry mPackage;
      void* mUserPtr;

      CodeBlock *mCode;
      U32 mFunctionOffset;
      union CallbackUnion {
         KorkApi::StringFuncCallback mStringCallbackFunc;
         KorkApi::IntFuncCallback mIntCallbackFunc;
         KorkApi::VoidFuncCallback mVoidCallbackFunc;
         KorkApi::FloatFuncCallback mFloatCallbackFunc;
         KorkApi::BoolFuncCallback mBoolCallbackFunc;
         KorkApi::ValueFuncCallback mValueCallbackFunc;
         const char* mGroupName;
      } cb;
      Entry();
      void clear();

      KorkApi::ConsoleValue execute(S32 argc, KorkApi::ConsoleValue* argv, ExprEvalState *state, KorkApi::VMObject* resolvedThis, bool startSuspended=false);
      
      const char* getUsage()
      {
         return mUsage ? mUsage : mDynamicUsage.c_str();
      }
   };
   Entry *mEntryList;

   Entry **mHashTable;
   U32 mHashSize;
   U32 mHashSequence;  ///< @note The hash sequence is used by the autodoc console facility
                     ///        as a means of testing reference asstate.

   Namespace();
   ~Namespace();

   void initVM(KorkApi::VmInternal* vm);
   void addFunction(StringTableEntry name, CodeBlock *cb, U32 functionOffset, const char* usage = NULL);
   void addCommand(StringTableEntry name, KorkApi::StringFuncCallback, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs);
   void addCommand(StringTableEntry name, KorkApi::IntFuncCallback, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs);
   void addCommand(StringTableEntry name, KorkApi::FloatFuncCallback, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs);
   void addCommand(StringTableEntry name, KorkApi::VoidFuncCallback, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs);
   void addCommand(StringTableEntry name, KorkApi::BoolFuncCallback, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs);
   void addCommand(StringTableEntry name, KorkApi::ValueFuncCallback, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs);

   void markGroup(const char* name, const char* usage);
   char * lastUsage;

   void getEntryList(KorkApi::Vector<Entry *> *);
   
   const char* getUsage()
   {
      return mUsage ? mUsage : mDynamicUsage.c_str();
   }
   
   Entry *lookup(StringTableEntry name);
   Entry *lookupRecursive(StringTableEntry name);
   Entry *createLocalEntry(StringTableEntry name);
   void buildHashTable();
   void clearEntries();
   bool classLinkTo(Namespace *parent);
   bool unlinkClass(Namespace *parent);

   const char *tabComplete(const char *prevText, S32 baseLen, bool fForward);
};


struct NamespaceState
{
   enum {
        MaxActivePackages = 512,
   };

   KorkApi::VmInternal* mVmInternal;
   Namespace *mNamespaceList;
   Namespace *mGlobalNamespace;
   KorkApi::VMChunker mCacheAllocator;
   KorkApi::VMChunker mAllocator;
   U32 mCacheSequence;
   U32 mNumActivePackages;
   U32 mOldNumActivePackages;
   StringTableEntry mActivePackages[MaxActivePackages];

   NamespaceState();
   void trashCache();
   
   Namespace *find(StringTableEntry name, StringTableEntry package=NULL);
   Namespace *lookup(StringTableEntry name, StringTableEntry package=NULL);
   bool canTabComplete(const char *prevText, const char *bestMatch, const char *newText, S32 baseLen, bool fForward);

   // Packages
   void activatePackage(StringTableEntry name);
   void deactivatePackage(StringTableEntry name);
   void unlinkPackages();
   void relinkPackages();
   bool isPackage(StringTableEntry name);
   
   // ConsoleDoc
   void dumpClasses( bool dumpScript = true, bool dumpEngine = true );
   void dumpFunctions( bool dumpScript = true, bool dumpEngine = true );
   void printNamespaceEntries(Namespace * g, bool dumpScript = true, bool dumpEngine = true);

   Namespace* global();
   void init(KorkApi::VmInternal* vmInternal);
   void shutdown();
};

