#pragma once
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
   //AbstractClassRep *mClassRep;
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

      KorkApi::ConsoleValue execute(S32 argc, KorkApi::ConsoleValue* argv, ExprEvalState *state);
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


struct NamespaceState
{
   enum {
        MaxActivePackages = 512,
   };

   KorkApi::VmInternal* mVmInternal;
   Namespace *mNamespaceList;
   Namespace *mGlobalNamespace;
   DataChunker mCacheAllocator;
   DataChunker mAllocator;
   U32 mCacheSequence;
   U32 mNumActivePackages;
   U32 mOldNumActivePackages;
   StringTableEntry mActivePackages[MaxActivePackages];

   NamespaceState();
   void trashCache();
   
   Namespace *find(StringTableEntry name, StringTableEntry package=NULL);
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

