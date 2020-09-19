#include "platform/platform.h"
#include "console/console.h"
#include "console/consoleObject.h"
#include <stdio.h>
#include <unordered_map>
#include <vector>

void MyLogger(ConsoleLogEntry::Level level, const char *consoleLine)
{
	printf("%s\n", consoleLine);
}

U32 gObjectID = 3;
std::unordered_map<U32, ConsoleObject*> gObjectIdLookup;
std::unordered_map<StringTableEntry, ConsoleObject*> gObjectNameLookup;

class MyConsoleObject : public ConsoleObject
{
public:
   typedef ConsoleObject Parent;

   StringTableEntry mName;
   StringTableEntry mInternalName;

   U32 mId;
   U32 mScriptCallbackGuard;

   ConsoleObject* mGroup;
   std::vector<ConsoleObject*> mObjects;
   std::vector<MyConsoleObject*> mObjectsListening;

   bool mIsGroup;

   MyConsoleObject() :
      mName(NULL),
      mInternalName(NULL),
      mId(0),
      mScriptCallbackGuard(0),
      mGroup(NULL),
      mIsGroup(false)
   {

   }

   virtual ~MyConsoleObject()
   {

   }

   virtual bool registerObject()
   {
      if (!registerObjectById(gObjectID++))
      {
         gObjectID--;
         return false;
      }

      return true;
   }

   virtual bool registerObjectById(uint32_t id)
   {
      mId = id;
      gObjectIdLookup[id] = this;
      return true;
   }

   virtual bool isProperlyAdded()
   {
      return true;
   }

   virtual void deleteObject()
   {
      for (auto obj : mObjectsListening)
      {
         obj->onListenDestroy(this);
      }

      clearObjects();
      gObjectIdLookup.erase(mId);
   }

   virtual void onListenDestroy(ConsoleObject* obj)
   {
      auto itr = std::find(mObjects.begin(), mObjects.end(), obj);
      if (itr != mObjects.end())
      {
         mObjects.erase(itr);
      }

      if (obj == mGroup)
      {
         deleteObject();
      }
   }

   virtual void assignFieldsFrom(ConsoleObject* other)
   {

   }

   virtual void assignName(StringTableEntry name)
   {
      if (mName != NULL)
      {
         gObjectNameLookup.erase(mName);
      }

      mName = name;
      gObjectNameLookup[name] = this;
   }

   virtual void setInternalName(StringTableEntry name)
   {
      mInternalName = name;
   }

   virtual bool processArguments(int argc, const char **argv)
   {
      return true;
   }

   virtual void setModStaticFields(bool value)
   {

   }

   virtual void setModDynamicFields(bool value)
   {

   }
   
   void clearObjects()
   {
      if (mIsGroup)
      {
         mObjects.clear();
      }
      else
      {
         while (mObjects.size() != 0)
         {
            mObjects[0]->deleteObject();
         }
      }
   }

   virtual void addObject(ConsoleObject* child)
   {
      MyConsoleObject* myChild = (MyConsoleObject*)child;

      mObjects.push_back(child);
      if (mIsGroup)
      {
         myChild->mGroup = this;
      }

      myChild->mObjectsListening.push_back(this);
   }
   
   virtual ConsoleObject* getObject(int index)
   {
      return mObjects[index];
   }
   
   virtual U32 getChildObjectCount()
   {
      return mObjects.size();
   }
   
   virtual bool isGroup()
   {
      return true;
   }
   
   virtual ConsoleObject* getGroup()
   {
      return mGroup;
   }
   
   virtual U32 getId()
   {
      return mId;
   }
   
   virtual const char *getDataField(StringTableEntry slotName, const char *array)
   {
      return "";
   }
   
   virtual void setDataField(StringTableEntry slotName, const char *array, const char *value)
   {

   }
   
   virtual Namespace* getNamespace() { return getClassRep()->getNamespace(); }
   virtual StringTableEntry getName()
   {
      return mName;
   }
   
   ConsoleObject* findObjectByInternalName(StringTableEntry name, bool recurse)
   {
      return NULL;
   }
   
   virtual void pushScriptCallbackGuard()
   {
      mScriptCallbackGuard++;
   }

   virtual void popScriptCallbackGuard()
   {
      mScriptCallbackGuard--;
   }

   DECLARE_CONOBJECT(MyConsoleObject);
};
IMPLEMENT_CONOBJECT(MyConsoleObject);


class MyCodeBlockWorld : public CodeBlockWorld
{
public:
   virtual ConsoleObject* lookupObject(const char* name)
   {
      if (name[0] > '0' && name[0] <= '9')
      {
         return gObjectIdLookup[atoi(name)];
      }
      else
      {
         return gObjectNameLookup[StringTable->insert(name)];
      }
   	return NULL;
   }
   
   virtual ConsoleObject* lookupObject(const char* name, ConsoleObject* parent)
   {
   	return NULL;
   }
   
   virtual ConsoleObject* lookupObject(uint32_t id)
   {
      return gObjectIdLookup[id];
   }

   virtual ConsoleObject* lookupObjectST(StringTableEntry entry)
   {
         return gObjectNameLookup[entry];
   }
};

ConsoleMethod(MyConsoleObject, testFunction, void, 3, 3, "")
{
   printf("Test function called with: %s\n", argv[2]);
}

int main(int argc, char **argv)
{
	MyCodeBlockWorld world;

	world.init();


   MyConsoleObject* rootGroup = new MyConsoleObject();
   rootGroup->mIsGroup = true;
   rootGroup->registerObjectById(CodeBlockWorld::RootGroupID);

   MyConsoleObject* dbGroup = new MyConsoleObject();
   dbGroup->mIsGroup = true;
   dbGroup->registerObjectById(CodeBlockWorld::DataBlockGroupID);

	world.evaluatef("echo(\"Hello world\" SPC TorqueScript SPC is SPC amazing);");

   MyConsoleObject* obj = new MyConsoleObject();
   if (obj->registerObject())
   {
      world.evaluatef("%u.testFunction(test);", obj->getId());
   }


   world.evaluatef("$f = new MyConsoleObject() {};");
   //" $f.testFunction(test2);", obj->getId());

   obj->deleteObject();
   dbGroup->deleteObject();
   rootGroup->deleteObject();

	world.shutdown();

	return 1;
}
