#include "platform/platform.h"
#include <stdio.h>
#include <unordered_map>
#include "embed/api.h"

/*
 Example for new API
 */


using namespace KorkApi;

void MyLogger(U32 level, const char *consoleLine, void* userPtr)
{
   printf("%s\n", consoleLine);
}

Vm* gVM = NULL;
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
      auto it = gByName.find(gVM->internString(path));
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

// Hack until we define these properly in the API
namespace KorkApi
{
TypeStorageInterface CreateRegisterStorageFromArgs(KorkApi::VmInternal* vmInternal, U32 argc, KorkApi::ConsoleValue* argv);
}

static bool MyPoint3F_CastValue(void*,
                               KorkApi::Vm* vm,
                               KorkApi::TypeStorageInterface* inputStorage,
                               KorkApi::TypeStorageInterface* outputStorage,
                              const EnumTable* tbl,
                              BitSet32 flag,
                              U32 typeId)
{
   const KorkApi::ConsoleValue* argv = nullptr;
   U32 argc = inputStorage ? inputStorage->data.argc : 0;
   bool directLoad = false;

   if (argc > 0 && inputStorage->data.storageRegister)
   {
      argv = inputStorage->data.storageRegister;
   }
   else
   {
      argc = 1;
      argv = &inputStorage->data.storageAddress;
      directLoad = true;
   }

   MyPoint3F v = {0, 0, 0};

   if (inputStorage->isField && directLoad)
   {
      const MyPoint3F* src = (const MyPoint3F*)inputStorage->data.storageAddress.evaluatePtr(vm->getAllocBase());
      if (!src) return false;
      v = *src;
   }
   else
   {
      if (argc == 3)
      {
         v.x = (F32)argv[0].getFloat((F64)argv[0].getInt(0));
         v.y = (F32)argv[1].getFloat((F64)argv[1].getInt(0));
         v.z = (F32)argv[2].getFloat((F64)argv[2].getInt(0));
      }
      else if (argc == 1)
      {
         const char* s = vm->valueAsString(argv[0]);
         if (!s) s = "";

         sscanf(s, "%g %g %g", &v.x, &v.y, &v.z);
      }
      else
      {
         // Not supported
         return false;
      }
   }

   // -> output

   if (typeId == 3) // TOFIX
   {
      MyPoint3F* dstPtr = (MyPoint3F*)outputStorage->data.storageAddress.evaluatePtr(vm->getAllocBase());
      if (!dstPtr)
      {
         return false;
      }

      *dstPtr = v;

      if (outputStorage->data.storageRegister)
         *outputStorage->data.storageRegister = outputStorage->data.storageAddress;

      return true;
   }
   else if (typeId == KorkApi::ConsoleValue::TypeInternalString)
   {
      const U32 bufLen = 96;

      outputStorage->FinalizeStorage(outputStorage, bufLen);

      char* out = (char*)outputStorage->data.storageAddress.evaluatePtr(vm->getAllocBase());
      if (!out) return false;

      snprintf(out, bufLen, "%.9g %.9g %.9g", v.x, v.y, v.z);

      if (outputStorage->data.storageRegister)
         *outputStorage->data.storageRegister = outputStorage->data.storageAddress;

      return true;
   }
   else
   {
      KorkApi::ConsoleValue vals[3];
      vals[0] = KorkApi::ConsoleValue::makeNumber(v.x);
      vals[1] = KorkApi::ConsoleValue::makeNumber(v.y);
      vals[2] = KorkApi::ConsoleValue::makeNumber(v.z);

      KorkApi::TypeStorageInterface castInput =
         KorkApi::CreateRegisterStorageFromArgs(vm->mInternal, 3, vals);

      return vm->castValue(typeId, &castInput, outputStorage, tbl, flag);
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

static void MyBase_Create(void* classUser, Vm* vm, CreateClassReturn* outP)
{
   MyBase* b = new MyBase();
   b->mVMInstance = NULL;
   outP->userPtr = b;
   outP->initialFlags |= KorkApi::ObjectFlags::ModStaticFields;
}

static void MyBase_RemoveObject(void* user, Vm* vm, VMObject* object)
{
   MyBase* b = (MyBase*)object->userPtr;
   if (b->mVMInstance)
   {
      vm->decVMRef(b->mVMInstance);
      b->mVMInstance = NULL;
   }
}

static bool MyBase_AddObject(Vm* vm, VMObject* object, bool placeAtRoot, U32 groupAddId)
{
   MyBase* b = (MyBase*)object->userPtr;
   
   if (b->mVMInstance != NULL && b->mVMInstance != object)
   {
      vm->decVMRef(b->mVMInstance);
   }
   
   b->mVMInstance = object;
   vm->incVMRef(object);
   
   b->mId = gCurrentId++;
   gByName[b->mName] = object;
   gById[b->mId] = object;
   
   return true;
}

static bool MyBase_ProcessArgs(Vm* vm, void* createdPtr, const char* name, bool isDatablock, bool internalName, int argc, const char** argv)
{
    MyBase* b = (MyBase*)createdPtr;
    b->mName = vm->internString(name);
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

static void Player_Create(void* classUser, Vm* vm, CreateClassReturn* outP)
{
   Player* b = new Player();
   b->mPosition = {};
   b->mVMInstance = NULL;
   outP->userPtr = b;
   outP->initialFlags = KorkApi::ModStaticFields;
}

static bool Player_AddObject(Vm* vm, VMObject* object, bool placeAtRoot, U32 groupAddId)
{
   if (MyBase_AddObject(vm, object, placeAtRoot, groupAddId))
   {
      vm->setObjectNamespace(object, vm->findNamespace(vm->internString("Player")));
      return true;
   }
   
   return false;
}

static void Player_RemoveObject(void* user, Vm* vm, VMObject* object)
{
   auto* p = reinterpret_cast<Player*>(user);
   if (p && p->mVMInstance)
   {
      gByName.erase(p->mName);
   }
   MyBase_RemoveObject(user, vm, object);
}

static void  Player_Destroy(void* classUser, Vm* vm, void* instanceUser)
{
   auto* p = reinterpret_cast<Player*>(instanceUser);
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
   gVM = vm;
   if (!vm)
   {
      return -1;
   }
   
   TypeInfo tInfo{};
   tInfo.name = "MyPoint3F";
   tInfo.userPtr = NULL;
   tInfo.fieldsize = sizeof(MyPoint3F);
   tInfo.valueSize = sizeof(MyPoint3F);
   tInfo.iFuncs = {
      &MyPoint3F_CastValue,
      &MyPoint3F_GetTypeClassName,
      NULL
   };
   TypeId typeMyPoint3F = vm->registerType(tInfo);
   
   // 3) Register MyBase
   ClassInfo myBase{};
   myBase.name = vm->internString("MyBase");
   myBase.userPtr = NULL;
   myBase.numFields = 0;
   myBase.fields = NULL;
   myBase.iCreate = { &MyBase_Create, &MyBase_Destroy, &MyBase_ProcessArgs, &MyBase_AddObject, &MyBase_RemoveObject, &MyBase_GetID };
   
   myBase.iCustomFields   = {};
   
   ClassId myBaseId = vm->registerClass(myBase);
   
   //ConsoleBaseType::registerWithVM(vm);
   
   //AbstractClassRep::registerWithVM(vm);
   
   // 4) Register Player (derived from MyBase) with a MyPoint3F field
   static FieldInfo playerFields[1];
   playerFields[0] = {};
   playerFields[0].pFieldname = vm->internString("position");
   playerFields[0].offset = Offset(mPosition, Player);
   playerFields[0].type = typeMyPoint3F;
   
   ClassInfo player{};
   player.name        = vm->internString("Player");
   player.userPtr     = NULL;
   player.numFields   = 1;
   player.fields      = playerFields;
   player.iCreate     = { &Player_Create, &Player_Destroy, &MyBase_ProcessArgs, &Player_AddObject, &Player_RemoveObject, &MyBase_GetID };
   player.iCustomFields = {};
   
   ClassId playerId = vm->registerClass(player);
   
   // 5) Register a basic echo in the global namespace so the script can print
   NamespaceId globalNS = vm->getGlobalNamespace(); // or obtain root namespace
   NamespaceId playerNS = vm->findNamespace(vm->internString("Player"), NULL);
   
   vm->addNamespaceFunction(vm->getGlobalNamespace(), vm->internString("echo"), cEcho, NULL, "", 1, 32);
   vm->addNamespaceFunction(playerNS, vm->internString("jump"), (KorkApi::VoidFuncCallback)cPlayerJump, NULL, "()", 2, 2);
   vm->evalCode(script, filename);
   
   VMObject* found = cfg.iFind.FindObjectByNameFn(cfg.findUser, "player1", NULL);
   AssertFatal(found, "player1 should be registered in iFind");
   
   destroyVM(vm);
   gVM = NULL;
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
   

   FILE* fp = fopen(argv[1], "r");
   if (!fp)
   {
      printf("Error loading file %s\n", argv[1]);
      return 1;
   }
   
   fseek(fp, 0, SEEK_END);
   size_t fend = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   
   char* data = new char[fend+1];
   fread(data, 1, fend, fp);
   data[fend] = '\0';
   fclose(fp);
   
   int ret = testScript(data, argv[1]) ? 0 : 1;
   delete[] data;
   return ret;
}

int main(int argc, char **argv)
{
   int ret = procMain(argc, argv);
   
   return ret;
}
