#include "platform/platform.h"
#include "console/simpleLexer.h"
#include "console/ast.h"
#include "console/compiler.h"
#include "console/simpleParser.h"
#include "core/fileStream.h"
#include <stdio.h>
#include "embed/api.h"

/*
 Example for new API
 */


using namespace KorkApi;

void MyLogger(U32 level, const char *consoleLine, void* userPtr)
{
   printf("%s\n", consoleLine);
}


static std::unordered_map<StringTableEntry, VMObject*> gByName;
static std::unordered_map<U32, VMObject*> gById;
static KorkApi::SimObjectId gCurrentId = 1;

static VMObject* FindByName(void* userPtr, StringTableEntry name, VMObject* parent)
{
   if (!name) return NULL;
   auto it = gByName.find(name);
   return it == gByName.end() ? NULL : it->second;
}

static VMObject* FindById(void* userPtr, KorkApi::SimObjectId ident)
{
   auto it = gById.find(ident);
   return it == gById.end() ? NULL : it->second;
}

static VMObject* FindByPath(void* userPtr, const char* path)
{
   if (!path) return NULL;
   
   if (path[0] >= '0' && path[0] < '9')
   {
      auto numIt = gById.find(atoi(path));
      return numIt == gById.end() ? NULL : numIt->second;
   }
   else
   {
      auto it = gByName.find(StringTable->insert(path));
      return it == gByName.end() ? NULL : it->second;
   }
}

//
// MyPoint3F
//

struct MyPoint3F
{
   F32 x,y,z;
};

static void MyPoint3F_SetValue(void*, 
                               KorkApi::Vm* vm,
                               void* dptr,
                              S32 argc, ConsoleValue* argv,
                              const EnumTable*, 
                              BitSet32,
                              U32 typeId) {
   MyPoint3F* p = reinterpret_cast<MyPoint3F*>(dptr);
   if (!p)
      return;
   
   if (argc >= 3) 
   {
      p->x = vm->valueAsFloat(argv[0]);
      p->y = vm->valueAsFloat(argv[1]);
      p->z = vm->valueAsFloat(argv[2]);
   } 
   else if (argc == 1) 
   {
      if (argv[0].typeId == typeId)
      {
         *p = *((MyPoint3F*)(argv[0].evaluatePtr(vm->getAllocBase())));
      }
      else if (argv[0].typeId == KorkApi::ConsoleValue::TypeInternalString)
      {
         *p = {};
         const char* inputStr = (const char*)(argv[0].evaluatePtr(vm->getAllocBase()));
         sscanf(inputStr, "%f %f %f", &p->x, &p->y, &p->z);
      }
      else
      {
         *((MyPoint3F*)dptr) = {};
      }
   }
}

static ConsoleValue MyPoint3F_CopyData(void* userPtr,
                         KorkApi::Vm* vm,
                         void* sptr,
                         const EnumTable* tbl,
                         BitSet32 flag,
                         U32 requestedType,
                         U32 requestedZone) {
   static char buf[96];
   MyPoint3F* pt = reinterpret_cast<MyPoint3F*>(sptr);
   ConsoleValue cv;
   U32 realRequestedType = requestedZone & KorkApi::TypeDirectCopyMask;

   if (realRequestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      char* destPtr = buf;
      
      if (requestedZone != KorkApi::ConsoleValue::ZoneExternal)
      {
         cv = vm->getStringInZone(requestedZone, 256);
         if (cv.isNull())
         {
            return;
         }
      }
      else
      {
         cv = KorkApi::ConsoleValue::makeString(destPtr);
      }
      
      dSprintf(destPtr, 256, "%g %g %g", pt->x, pt->y, pt->z);
      return cv;
   }
   else if ((requestedType & KorkApi::TypeDirectCopy) != 0) // i.e. same type
   {
      cv = vm->getTypeInZone(requestedZone, realRequestedType);
      *((MyPoint3F*)cv.ptr()) = *pt;
      return cv;
   }
}

static const char* MyPoint3F_GetTypeClassName(void*) { return "MyPoint3F"; }

//
// MyBase
//

struct MyBase 
{
   VMObject* mVMInstance;  // will be set post-construction
   StringTableEntry mName; // keep a copy so we can unregister cleanly
   U32 mId;
};

static void* MyBase_Create(void* classUser, Vm* vm, VMObject* object)
{
   MyBase* b = new MyBase();
   b->mVMInstance = object;
   object->flags |= KorkApi::ObjectFlags::ModStaticFields;
   return b;
}

static bool MyBase_AddObject(Vm* vm, VMObject* object, bool placeAtRoot, U32 groupAddId)
{
   MyBase* b = (MyBase*)object->userPtr;
   
   b->mId = gCurrentId++;
   gByName[b->mName] = object;
   gById[b->mId] = object;
   
   return true;
}

static bool MyBase_ProcessArgs(Vm* vm, VMObject* object, const char* name, bool isDatablock, bool internalName, int argc, const char** argv)
{
    MyBase* b = (MyBase*)object->userPtr;
    b->mName = StringTable->insert(name);
    return true;
}

static SimObjectId MyBase_GetID(VMObject* object)
{
   MyBase* b = (MyBase*)object->userPtr;
   return b->mId;
}

static void  MyBase_Destroy(void* classUser, Vm* vm, void* instanceUser)
{
   auto* base = reinterpret_cast<MyBase*>(instanceUser);
   if (base && base->mVMInstance)
   {
      gByName.erase(base->mName);
      if (base->mId != 0)
      {
         gById.erase(base->mId);
      }
   }
   delete base;
}

//
// Player
//

struct Player : public MyBase
{
   MyPoint3F mPosition;
};

static void* Player_Create(void* classUser, Vm* vm, VMObject* object)
{
   Player* b = new Player();
   b->mPosition = {};
   b->mVMInstance = object;
   object->flags |= KorkApi::ModStaticFields;
   return b;
}

static bool Player_AddObject(Vm* vm, VMObject* object, bool placeAtRoot, U32 groupAddId)
{
   if (MyBase_AddObject(vm, object, placeAtRoot, groupAddId))
   {
      vm->setObjectNamespace(object, vm->findNamespace(StringTable->insert("Player")));
      return true;
   }
   return false;
}

static void  Player_Destroy(void* classUser, Vm* vm, void* instanceUser)
{
   auto* p = reinterpret_cast<Player*>(instanceUser);
   if (p && p->mVMInstance) 
   {
      gByName.erase(p->mName);
   }
   delete p;
}

void cPlayerJump(Player* object, int argc, const char** argv)
{
   object->mPosition.z += 10;
}

void cEcho(void* object, void* userPtr, int argc, const char** argv)
{
   for (int i=1; i<argc; i++)
   {
      printf("%s", argv[i]);
   }
   printf("\n");
}

int testScript(char* script, const char* filename)
{
   Config cfg{};
   cfg.mallocFn = [](size_t sz, void* user) {
      return (void*)malloc(sz);
   };
   cfg.freeFn = [](void* ptr, void* user){
      free(ptr);
   };
   cfg.logFn = MyLogger;
   cfg.iFind = { &FindByName, &FindByPath, NULL, &FindById };
   Vm* vm = createVM(&cfg);
   if (!vm)
   {
      return -1;
   }
   
   TypeInfo tInfo{};
   tInfo.name = "MyPoint3F";
   tInfo.userPtr = NULL;
   tInfo.size = sizeof(MyPoint3F);
   tInfo.iFuncs = {
      &MyPoint3F_SetValue,
      &MyPoint3F_CopyData,
      &MyPoint3F_GetTypeClassName,
      NULL
   };
   TypeId typeMyPoint3F = vm->registerType(tInfo);
   
   // 3) Register MyBase
   ClassInfo myBase{};
   myBase.name = StringTable->insert("MyBase");
   myBase.userPtr = NULL;
   myBase.numFields = 0;
   myBase.fields = NULL;
   myBase.iCreate = { &MyBase_Create, &MyBase_Destroy, &MyBase_ProcessArgs, &MyBase_AddObject, &MyBase_GetID };
   
   myBase.iCustomFields   = {};
   
   ClassId myBaseId = vm->registerClass(myBase);
   
   //ConsoleBaseType::registerWithVM(vm);
   
   //AbstractClassRep::registerWithVM(vm);
   
   // 4) Register Player (derived from MyBase) with a MyPoint3F field
   static FieldInfo playerFields[1];
   playerFields[0] = {};
   playerFields[0].pFieldname = StringTable->insert("position");
   playerFields[0].offset = Offset(mPosition, Player);
   playerFields[0].type = typeMyPoint3F;
   
   ClassInfo player{};
   player.name        = StringTable->insert("Player");
   player.userPtr     = NULL;
   player.numFields   = 1;
   player.fields      = playerFields;
   player.iCreate     = { &Player_Create, &Player_Destroy, &MyBase_ProcessArgs, &Player_AddObject, &MyBase_GetID };
   player.iCustomFields = {};
   
   ClassId playerId = vm->registerClass(player);
   
   // 5) Register a basic echo in the global namespace so the script can print
   NamespaceId globalNS = vm->getGlobalNamespace(); // or obtain root namespace
   NamespaceId playerNS = vm->findNamespace(StringTable->insert("Player"), NULL);
   
   vm->addNamespaceFunction(vm->getGlobalNamespace(), StringTable->insert("echo"), cEcho, "", 1, 32);
   vm->addNamespaceFunction(playerNS, StringTable->insert("jump"), (KorkApi::VoidFuncCallback)cPlayerJump, "()", 2, 2);
   vm->evalCode(script, filename);
   
   VMObject* found = cfg.iFind.FindObjectByNameFn(cfg.findUser, "player1", NULL);
   AssertFatal(found, "player1 should be registered in iFind");
   
   destroyVm(vm);
   return 0;
}


int procMain(int argc, char **argv)
{
   if (argc < 2)
   {
      printf("Not enough args\n");
      return 1;
   }
   
   for (int i=2; i<argc; i++)
   {
   }
   
   FileStream fs;
   if (!fs.open(argv[1], FileStream::Read))
   {
      printf("Error loading file %s\n", argv[1]);
      return 1;
   }
   
   char* data = new char[fs.getStreamSize()+1];
   fs.read(fs.getStreamSize(), data);
   data[fs.getStreamSize()] = '\0';
   
   int ret = testScript(data, argv[1]) ? 0 : 1;
   delete[] data;
   return ret;
}

int main(int argc, char **argv)
{
   int ret = procMain(argc, argv);
   
   return ret;
}
