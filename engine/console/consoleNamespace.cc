#include "embed/api.h"
#include "embed/internalApi.h"
#include "console/consoleNamespace.h"
#include "console/consoleInternal.h"

extern U32 HashPointer(StringTableEntry ptr);

NamespaceState::NamespaceState()
{
   mCacheSequence = 0;
   mNamespaceList = NULL;
   mGlobalNamespace = NULL;
}

void NamespaceState::init(KorkApi::VmInternal* vmInternal)
{
   mNumActivePackages = 0;
   mOldNumActivePackages = 0;
   mVmInternal = vmInternal;
   mGlobalNamespace = find(NULL);
}

Namespace *NamespaceState::global()
{
   return mGlobalNamespace;
}

void NamespaceState::shutdown()
{
   for(Namespace *walk = mNamespaceList; walk; walk = walk->mNext)
      walk->clearEntries();
}

bool NamespaceState::canTabComplete(const char *prevText, const char *bestMatch,
                               const char *newText, S32 baseLen, bool fForward)
{
   // test if it matches the first baseLen chars:
   if(dStrnicmp(newText, prevText, baseLen))
      return false;
   
   if (fForward)
   {
      if(!bestMatch)
         return dStricmp(newText, prevText) > 0;
      else
         return (dStricmp(newText, prevText) > 0) &&
         (dStricmp(newText, bestMatch) < 0);
   }
   else
   {
      if (dStrlen(prevText) == (U32) baseLen)
      {
         // look for the 'worst match'
         if(!bestMatch)
            return dStricmp(newText, prevText) > 0;
         else
            return dStricmp(newText, bestMatch) > 0;
      }
      else
      {
         if (!bestMatch)
            return (dStricmp(newText, prevText)  < 0);
         else
            return (dStricmp(newText, prevText)  < 0) &&
            (dStricmp(newText, bestMatch) > 0);
      }
   }
}

bool NamespaceState::isPackage(StringTableEntry name)
{
   for(Namespace *walk = mNamespaceList; walk; walk = walk->mNext)
      if(walk->mPackage == name)
         return true;
   return false;
}

void NamespaceState::activatePackage(StringTableEntry name)
{
   if(mNumActivePackages == MaxActivePackages)
   {
      mVmInternal->printf(0, "ActivatePackage(%s) failed - Max package limit reached: %d", name, MaxActivePackages);
      return;
   }
   if(!name)
      return;

   // see if this one's already active
   for(U32 i = 0; i < mNumActivePackages; i++)
      if(mActivePackages[i] == name)
         return;

   // kill the cache
   trashCache();

   // find all the package namespaces...
   for(Namespace *walk = mNamespaceList; walk; walk = walk->mNext)
   {
      if(walk->mPackage == name)
      {
         Namespace *parent = find(walk->mName);
         // hook the parent
         walk->mParent = parent->mParent;
         parent->mParent = walk;

         // now swap the entries:
         Namespace::Entry *ew;
         for(ew = parent->mEntryList; ew; ew = ew->mNext)
            ew->mNamespace = walk;

         for(ew = walk->mEntryList; ew; ew = ew->mNext)
            ew->mNamespace = parent;

         ew = walk->mEntryList;
         walk->mEntryList = parent->mEntryList;
         parent->mEntryList = ew;
      }
   }
   mActivePackages[mNumActivePackages++] = name;
}

void NamespaceState::deactivatePackage(StringTableEntry name)
{
   S32 i, j;
   for(i = 0; i < mNumActivePackages; i++)
      if(mActivePackages[i] == name)
         break;
   if(i == mNumActivePackages)
      return;

   trashCache();

   for(j = mNumActivePackages - 1; j >= i; j--)
   {
      // gotta unlink em in reverse order...
      for(Namespace *walk = mNamespaceList; walk; walk = walk->mNext)
      {
         if(walk->mPackage == mActivePackages[j])
         {
            Namespace *parent = find(walk->mName);
            // hook the parent
            parent->mParent = walk->mParent;
            walk->mParent = NULL;

            // now swap the entries:
            Namespace::Entry *ew;
            for(ew = parent->mEntryList; ew; ew = ew->mNext)
               ew->mNamespace = walk;

            for(ew = walk->mEntryList; ew; ew = ew->mNext)
               ew->mNamespace = parent;

            ew = walk->mEntryList;
            walk->mEntryList = parent->mEntryList;
            parent->mEntryList = ew;
         }
      }
   }
   mNumActivePackages = i;
}

void NamespaceState::unlinkPackages()
{
   mOldNumActivePackages = mNumActivePackages;
   if(!mNumActivePackages)
      return;
   deactivatePackage(mActivePackages[0]);
}

void NamespaceState::relinkPackages()
{
   if(!mOldNumActivePackages)
      return;
   for(U32 i = 0; i < mOldNumActivePackages; i++)
      activatePackage(mActivePackages[i]);
}

void NamespaceState::trashCache()
{
   mCacheSequence++;
   mCacheAllocator.freeBlocks();
}

Namespace::Entry::Entry()
{
   mCode = NULL;
   mUserPtr = NULL;
   mType = InvalidFunctionType;
}

void Namespace::Entry::clear()
{
   if(mCode)
   {
      mCode->decRefCount();
      mCode = NULL;
   }
   mUserPtr = NULL;
}

Namespace::Namespace()
{
   mPackage = NULL;
   mName = NULL;
   mParent = NULL;
   mNext = NULL;
   mEntryList = NULL;
   mHashSize = 0;
   mHashTable = 0;
   mHashSequence = 0;
   mRefCountToParent = 0;
   //mClassRep = 0;
   mVmInternal = NULL;
}

void Namespace::initVM(KorkApi::VmInternal* vm)
{
   mVmInternal = vm;
}

void Namespace::clearEntries()
{
   for(Entry *walk = mEntryList; walk; walk = walk->mNext)
      walk->clear();
}

Namespace *NamespaceState::find(StringTableEntry name, StringTableEntry package)
{
   for(Namespace *walk = mNamespaceList; walk; walk = walk->mNext)
      if(walk->mName == name && walk->mPackage == package)
         return walk;

   Namespace *ret = (Namespace *) mAllocator.alloc(sizeof(Namespace));
   constructInPlace(ret);
   ret->mVmInternal = mVmInternal;
   ret->mPackage = package;
   ret->mName = name;
   ret->mNext = mNamespaceList;
   mNamespaceList = ret;
   return ret;
}

bool Namespace::unlinkClass(Namespace *parent)
{
   Namespace *walk = this;
   while(walk->mParent && walk->mParent->mName == mName)
      walk = walk->mParent;

   if(walk->mParent && walk->mParent != parent)
   {
      mVmInternal->printf(0, "Error, cannot unlink namespace parent linkage for %s for %s.",
         walk->mName, walk->mParent->mName);
      return false;
   }

   mRefCountToParent--;
   AssertFatal(mRefCountToParent >= 0, "Namespace::unlinkClass: reference count to parent is less than 0");

   if(mRefCountToParent == 0)
      walk->mParent = NULL;

   return true;
}


bool Namespace::classLinkTo(Namespace *parent)
{
   Namespace *walk = this;
   while(walk->mParent && walk->mParent->mName == mName)
      walk = walk->mParent;

   if(walk->mParent && walk->mParent != parent)
   {
      mVmInternal->printf(0, "Error: cannot change namespace parent linkage for %s from %s to %s.",
         walk->mName, walk->mParent->mName, parent->mName);
      return false;
   }
   mRefCountToParent++;
   walk->mParent = parent;
   return true;
}

void Namespace::buildHashTable()
{
   if(mHashSequence == mVmInternal->mNSState.mCacheSequence)
      return;

   if(!mEntryList && mParent)
   {
      mParent->buildHashTable();
      mHashTable = mParent->mHashTable;
      mHashSize = mParent->mHashSize;
      mHashSequence = mVmInternal->mNSState.mCacheSequence;
      return;
   }

   U32 entryCount = 0;
   Namespace * ns;
   for(ns = this; ns; ns = ns->mParent)
      for(Entry *walk = ns->mEntryList; walk; walk = walk->mNext)
         if(lookupRecursive(walk->mFunctionName) == walk)
            entryCount++;

   mHashSize = entryCount + (entryCount >> 1) + 1;

   if(!(mHashSize & 1))
      mHashSize++;

   mHashTable = (Namespace::Entry **) mVmInternal->mNSState.mCacheAllocator.alloc(sizeof(Namespace::Entry *) * mHashSize);
   for(U32 i = 0; i < mHashSize; i++)
      mHashTable[i] = NULL;

   for(ns = this; ns; ns = ns->mParent)
   {
      for(Entry *walk = ns->mEntryList; walk; walk = walk->mNext)
      {
         U32 index = HashPointer(walk->mFunctionName) % mHashSize;
         while(mHashTable[index] && mHashTable[index]->mFunctionName != walk->mFunctionName)
         {
            index++;
            if(index >= mHashSize)
               index = 0;
         }

         if(!mHashTable[index])
            mHashTable[index] = walk;
      }
   }

   mHashSequence = mVmInternal->mNSState.mCacheSequence;
}

const char *Namespace::tabComplete(const char *prevText, S32 baseLen, bool fForward)
{
   if(mHashSequence != mVmInternal->mNSState.mCacheSequence)
      buildHashTable();

   const char *bestMatch = NULL;
   for(U32 i = 0; i < mHashSize; i++)
      if(mHashTable[i] && mVmInternal->mNSState.canTabComplete(prevText, bestMatch, mHashTable[i]->mFunctionName, baseLen, fForward))
         bestMatch = mHashTable[i]->mFunctionName;
   return bestMatch;
}

Namespace::Entry *Namespace::lookupRecursive(StringTableEntry name)
{
   for(Namespace *ns = this; ns; ns = ns->mParent)
      for(Entry *walk = ns->mEntryList; walk; walk = walk->mNext)
         if(walk->mFunctionName == name)
            return walk;

   return NULL;
}

Namespace::Entry *Namespace::lookup(StringTableEntry name)
{
   if(mHashSequence != mVmInternal->mNSState.mCacheSequence)
      buildHashTable();

   if (mHashSize == 0)
      return NULL;

   U32 index = HashPointer(name) % mHashSize;
   while(mHashTable[index] && mHashTable[index]->mFunctionName != name)
   {
      index++;
      if(index >= mHashSize)
         index = 0;
   }
   return mHashTable[index];
}

bool compareEntries(const Namespace::Entry* a, const Namespace::Entry* b)
{
    return dStricmp(a->mFunctionName, b->mFunctionName) < 0;
}

void Namespace::getEntryList(Vector<Entry *> *vec)
{
   if(mHashSequence != mVmInternal->mNSState.mCacheSequence)
      buildHashTable();

   for(U32 i = 0; i < mHashSize; i++)
      if(mHashTable[i])
         vec->push_back(mHashTable[i]);

   std::sort(vec->begin(),vec->end(),compareEntries);
}

Namespace::Entry *Namespace::createLocalEntry(StringTableEntry name)
{
   for(Entry *walk = mEntryList; walk; walk = walk->mNext)
   {
      if(walk->mFunctionName == name)
      {
         walk->clear();
         return walk;
      }
   }

   Namespace::Entry *ent = (Namespace::Entry *) mVmInternal->mNSState.mAllocator.alloc(sizeof(Entry));
   constructInPlace(ent);

   ent->mNamespace = this;
   ent->mFunctionName = name;
   ent->mNext = mEntryList;
   ent->mPackage = mPackage;
   mEntryList = ent;
   return ent;
}

void Namespace::addFunction(StringTableEntry name, CodeBlock *cb, U32 functionOffset, const char *usage)
{
   Entry *ent = createLocalEntry(name);
   mVmInternal->mNSState.trashCache();

   ent->mUsage = NULL;
   ent->mCode = cb;
   ent->mFunctionOffset = functionOffset;
   ent->mCode->incRefCount();
   ent->mType = Entry::ScriptFunctionType;
}

void Namespace::addCommand(StringTableEntry name, KorkApi::StringFuncCallback cb, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs)
{
   Entry *ent = createLocalEntry(name);
   mVmInternal->mNSState.trashCache();

   ent->mUsage = usage;
   ent->mMinArgs = minArgs;
   ent->mMaxArgs = maxArgs;
   ent->mUserPtr = userPtr;

   ent->mType = Entry::StringCallbackType;
   ent->cb.mStringCallbackFunc = cb;
}

void Namespace::addCommand(StringTableEntry name, KorkApi::IntFuncCallback cb, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs)
{
   Entry *ent = createLocalEntry(name);
   mVmInternal->mNSState.trashCache();

   ent->mUsage = usage;
   ent->mMinArgs = minArgs;
   ent->mMaxArgs = maxArgs;
   ent->mUserPtr = userPtr;

   ent->mType = Entry::IntCallbackType;
   ent->cb.mIntCallbackFunc = cb;
}

void Namespace::addCommand(StringTableEntry name, KorkApi::VoidFuncCallback cb, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs)
{
   Entry *ent = createLocalEntry(name);
   mVmInternal->mNSState.trashCache();

   ent->mUsage = usage;
   ent->mMinArgs = minArgs;
   ent->mMaxArgs = maxArgs;
   ent->mUserPtr = userPtr;

   ent->mType = Entry::VoidCallbackType;
   ent->cb.mVoidCallbackFunc = cb;
}

void Namespace::addCommand(StringTableEntry name, KorkApi::FloatFuncCallback cb, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs)
{
   Entry *ent = createLocalEntry(name);
   mVmInternal->mNSState.trashCache();

   ent->mUsage = usage;
   ent->mMinArgs = minArgs;
   ent->mMaxArgs = maxArgs;
   ent->mUserPtr = userPtr;

   ent->mType = Entry::FloatCallbackType;
   ent->cb.mFloatCallbackFunc = cb;
}

void Namespace::addCommand(StringTableEntry name, KorkApi::BoolFuncCallback cb, void* userPtr, const char* usage, S32 minArgs, S32 maxArgs)
{
   Entry *ent = createLocalEntry(name);
   mVmInternal->mNSState.trashCache();

   ent->mUsage = usage;
   ent->mMinArgs = minArgs;
   ent->mMaxArgs = maxArgs;
   ent->mUserPtr = userPtr;

   ent->mType = Entry::BoolCallbackType;
   ent->cb.mBoolCallbackFunc = cb;
}

void Namespace::addOverload(const char * name, const char *altUsage)
{
   char buffer[1024];
   char lilBuffer[32];
   dStrcpy(buffer, name);
   dSprintf(lilBuffer, 32, "_%d", mVmInternal->mNSCounter++);
   dStrcat(buffer, lilBuffer);

   Entry *ent = createLocalEntry(StringTable->insert( buffer ));
   mVmInternal->mNSState.trashCache();

   ent->mUsage = altUsage;
   ent->mMinArgs = -1;
   ent->mMaxArgs = -2;

   ent->mType = Entry::OverloadMarker;
   ent->cb.mGroupName = name;
}

void Namespace::markGroup(const char* name, const char* usage)
{
   char buffer[1024];
   char lilBuffer[32];
   dStrcpy(buffer, name);
   dSprintf(lilBuffer, 32, "_%d", mVmInternal->mNSCounter++);
   dStrcat(buffer, lilBuffer);

   Entry *ent = createLocalEntry(StringTable->insert( buffer ));
   mVmInternal->mNSState.trashCache();

   if(usage != NULL)
      lastUsage = (char*)(ent->mUsage = usage);
   else
      ent->mUsage = lastUsage;

   ent->mMinArgs = -1; // Make sure it explodes if somehow we run this entry.
   ent->mMaxArgs = -2;

   ent->mType = Namespace::Entry::GroupMarker;
   ent->cb.mGroupName = name;
}

extern S32 executeBlock(StmtNode *block, ExprEvalState *state);

const char *Namespace::Entry::execute(S32 argc, const char **argv, ExprEvalState *state)
{
   if(mType == ScriptFunctionType)
   {
      if(mFunctionOffset)
         return mCode->exec(mFunctionOffset, argv[0], mNamespace, argc, argv, false, mPackage);
      else
         return "";
   }

   if((mMinArgs && argc < mMinArgs) || (mMaxArgs && argc > mMaxArgs))
   {
      state->vmInternal->printf(0, "%s::%s - wrong number of arguments.", mNamespace->mName, mFunctionName);
      state->vmInternal->printf(0, "usage: %s", mUsage);
      return "";
   }

   char* returnBuffer = state->vmInternal->mExecReturnBuffer;
   switch(mType)
   {
      case StringCallbackType:
         return cb.mStringCallbackFunc((SimObject*)state->thisObject->userPtr, mUserPtr, argc, argv);
      case IntCallbackType:
         dSprintf(returnBuffer, KorkApi::VmInternal::ExecReturnBufferSize, "%d",
            cb.mIntCallbackFunc((SimObject*)state->thisObject->userPtr, mUserPtr, argc, argv));
         return returnBuffer;
      case FloatCallbackType:
         dSprintf(returnBuffer, KorkApi::VmInternal::ExecReturnBufferSize, "%g",
            cb.mFloatCallbackFunc((SimObject*)state->thisObject->userPtr, mUserPtr, argc, argv));
         return returnBuffer;
      case VoidCallbackType:
         cb.mVoidCallbackFunc((SimObject*)state->thisObject->userPtr, mUserPtr, argc, argv);
         return "";
      case BoolCallbackType:
         dSprintf(returnBuffer, KorkApi::VmInternal::ExecReturnBufferSize, "%d",
            (U32)cb.mBoolCallbackFunc((SimObject*)state->thisObject->userPtr, mUserPtr, argc, argv));
         return returnBuffer;
   }

   return "";
}
