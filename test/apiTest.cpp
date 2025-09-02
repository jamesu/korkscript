#include "embed/api.h"
#include "platform/platform.h"
#include "console/console.h"
#include "console/simpleLexer.h"
#include "console/ast.h"
#include "console/compiler.h"
#include "console/simpleParser.h"
#include "core/fileStream.h"
#include <stdio.h>
#include "embed/api.h"
#include "console/dynamicTypes.h"

/*
 Example for new API
 */


using namespace KorkApi;

void MyLogger(ConsoleLogEntry::Level level, const char *consoleLine, void* userPtr)
{
   printf("%s\n", consoleLine);
}


static std::unordered_map<StringTableEntry, VMObject*> gByName;

static VMObject* FindByName(StringTableEntry name)
{
   if (!name) return NULL;
   auto it = gByName.find(name);
   return it == gByName.end() ? NULL : it->second;
}

//
// MyPoint3F
//

struct MyPoint3F
{
   F32 x,y,z;
};

static void MyPoint3F_SetData(void*, void* dptr,
                              S32 argc, const char** argv,
                              const EnumTable*, BitSet32) {
   MyPoint3F* p = reinterpret_cast<MyPoint3F*>(dptr);
   if (!p)
      return;
   
   if (argc >= 3) {
      p->x = (float)std::atof(argv[0]);
      p->y = (float)std::atof(argv[1]);
      p->z = (float)std::atof(argv[2]);
   } else if (argc == 1) {
      *p = {};
      sscanf(argv[0], "%f %f %f", &p->x, &p->y, &p->z);
   }
}

static void MyPoint3F_SetDataCV(void*, void* dptr,
                                S32 argc, ConsoleValue* argv,
                                const EnumTable*, BitSet32) {
}

static const char* MyPoint3F_GetData(void*, void* dptr,
                                     const EnumTable*, BitSet32) {
   static char buf[96];
   MyPoint3F* p = reinterpret_cast<MyPoint3F*>(dptr);
   std::snprintf(buf, sizeof(buf), "%g %g %g", p->x, p->y, p->z);
   return buf;
}
static ConsoleValue MyPoint3F_GetDataCV(void*, void* dptr,
                                        const EnumTable*, BitSet32) {
   return ConsoleValue();
}
static const char* MyPoint3F_GetTypeClassName(void*) { return "MyPoint3F"; }
static const char* MyPoint3F_Prep(void*, const char* in, char* out, U32 len) {
   if (!in || !out || !len) return in;
   std::strncpy(out, in, len);
   out[len-1] = '\0';
   return out;
}
static StringTableEntry MyPoint3F_Prefix(void*) { return "p3f"; }

//
// MyBase
//

struct MyBase 
{
   VMObject* mVMInstance;  // will be set post-construction
   StringTableEntry mName; // keep a copy so we can unregister cleanly
};

static void* MyBase_Create(void* classUser, VMObject* object, const char* name, int argc, char** argv)
{
   MyBase* b = new MyBase(); 
   b->mName = StringTable->insert(name);
   return b;
}

static void  MyBase_Destroy(void* classUser, void* instanceUser)
{
   auto* base = reinterpret_cast<MyBase*>(instanceUser);
   if (base && base->mVMInstance)
   {
      gByName.erase(base->mName);
   }
   delete base;
}

//
// Player
//

struct Player : public MyBase
{
   VMObject* mVMObject;
   MyPoint3F mPosition;
};

static void* Player_Create(void* classUser, VMObject* object, const char* name, int argc, char** argv)
{
   Player* b = new Player();
   b->mPosition = {};
   b->mName = StringTable->insert(name);
   b->mVMObject = object;
   gByName[b->mName] = object;
   return b;
}

static void  Player_Destroy(void* classUser, void* instanceUser)
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

int testScript(char* script, const char* filename)
{
   // 1) Build config with working iFind
   Config cfg{};
   cfg.iFind = { &FindByName, NULL, NULL };
   Vm* vm = createVM(&cfg);
   if (!vm)
   {
      return -1;
   }
   
   // 2) Register TypeMyPoint3F
   TypeInfo tInfo{};
   tInfo.name = "MyPoint3F";
   tInfo.userPtr = NULL;
   tInfo.size = sizeof(MyPoint3F);
   tInfo.iFuncs = {
      &MyPoint3F_SetData,
      &MyPoint3F_SetDataCV,
      &MyPoint3F_GetData,
      &MyPoint3F_GetDataCV,
      &MyPoint3F_GetTypeClassName,
      &MyPoint3F_Prep,
      &MyPoint3F_Prefix
   };
   TypeId typeMyPoint3F = vm->registerType(tInfo);
   
   // 3) Register MyBase
   ClassInfo myBase{};
   myBase.name = "MyBase";
   myBase.userPtr = NULL;
   myBase.parentClass = -1;
   myBase.numFields = 0;
   myBase.fields = NULL;
   myBase.iCreate = { &MyBase_Create, &MyBase_Destroy };
   
   myBase.iCustomFields   = {};
   
   ClassId myBaseId = vm->registerClass(myBase);
   
   //ConsoleBaseType::registerWithVM(vm);
   
   // 4) Register Player (derived from MyBase) with a MyPoint3F field
   static FieldInfo playerFields[] = {
      { "position",
         Offset(mPosition, Player),
         (TypeId)typeMyPoint3F,
         NULL, NULL, NULL
      }
   };
   
   ClassInfo player{};
   player.name        = "Player";
   player.userPtr     = NULL;
   player.parentClass = myBaseId;
   player.numFields   = 1;
   player.fields      = playerFields;
   player.iCreate     = { &Player_Create, &Player_Destroy };
   player.iCustomFields = {};
   
   ClassId playerId = vm->registerClass(player);
   
   // 5) Register a basic echo in the global namespace so the script can print
   S32 globalNS = vm->getGlobalNamespace(); // or obtain root namespace
   S32 playerNS = vm->registerNamespace(StringTable->insert("Player"), NULL);
   
   vm->addNamespaceFunction(playerNS, StringTable->insert("jump"), (VoidCallback)cPlayerJump, "()", 2, 2);
   vm->evalCode(script, filename);
   
   // Optionally, prove C++ side can Find it as well:
   VMObject* found = cfg.iFind.FindObjectByNameFn("player1");
   AssertFatal(found, "player1 should be registered in iFind");
   
   destroyVm(vm);
   return 0;
}


int procMain(int argc, char **argv)
{
   if (argc < 2)
   {
      Con::printf("Not enough args");
      return 1;
   }
   
   for (int i=2; i<argc; i++)
   {
   }
   
   FileStream fs;
   if (!fs.open(argv[1], FileStream::Read))
   {
      Con::printf("Error loading file %s\n", argv[1]);
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
   Con::init();
   Con::addConsumer(MyLogger);
   
   int ret = procMain(argc, argv);
   
   Con::shutdown();
   
   return ret;
}
