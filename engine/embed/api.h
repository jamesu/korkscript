#pragma once

#include "console/compiler.h"
#include "console/simpleLexer.h"
#include "console/consoleValue.h"
#include "core/bitSet.h"

class TypeValidator; // TODO: change to interface
class Namespace;
struct EnumTable;

namespace Compiler
{
    struct Resources;
}

namespace KorkApi
{

typedef const char * (*StringFuncCallback)(void *obj, void* userPtr, S32 argc, const char *argv[]);
typedef S32             (*IntFuncCallback)(void *obj, void* userPtr, S32 argc, const char *argv[]);
typedef F32           (*FloatFuncCallback)(void *obj, void* userPtr, S32 argc, const char *argv[]);
typedef void           (*VoidFuncCallback)(void *obj, void* userPtr, S32 argc, const char *argv[]); // We have it return a value so things don't break..
typedef bool           (*BoolFuncCallback)(void *obj, void* userPtr, S32 argc, const char *argv[]);
typedef ConsoleValue   (*ValueFuncCallback)(void *obj, void* userPtr, S32 argc, ConsoleValue argv[]);

template <typename C, auto ThunkFn> struct APIThunk;

template <typename C, typename R, typename... Args, R(C::*ThunkFn)(Args...)>
struct APIThunk<C, ThunkFn> {
    using type = R(*)(void*, Args...);
    static R call(void* user, Args... args) noexcept {
        return (static_cast<C*>(user)->*ThunkFn)(std::forward<Args>(args)...);
    }
};

template <typename C, typename R, typename... Args, R(C::*ThunkFn)(Args...) const>
struct APIThunk<C, ThunkFn> {
    using type = R(*)(void*, Args...);
    static R call(void* user, Args... args) noexcept {
        return (static_cast<const C*>(user)->*ThunkFn)(std::forward<Args>(args)...);
    }
};

typedef U32 SimObjectId;
typedef Namespace* NamespaceId;
typedef U32 CodeBlockId;
typedef U32 FiberId;
using VMNamespace = Namespace;

//
// Type info
//

typedef S32 TypeId;

typedef void(*SetValueFn)(void* userPtr,
                  Vm* vm,
                          void* dptr,
                          S32 argc,
                          ConsoleValue* argv,
                          const EnumTable* tbl,
                          BitSet32 flag,
                          U32 typeId);

// sptr -> [return]
// destVal = srcVal
typedef ConsoleValue(*CopyValueFn)(void* userPtr,
                  Vm* vm,
                         void* sptr,
                         const EnumTable* tbl,
                         BitSet32 flag,
                         U32 requestedType,
                         U32 requestedZone);

struct TypeInterface
{
// argv[] -> dptr
// foo = bar
// foo = b1, b2, b3
SetValueFn SetValue;

// sptr -> [return]
// destVal = srcVal
CopyValueFn CopyValue;

// TypeName
const char*(*GetTypeClassNameFn)(void* userPtr);

// For string types: compress paths
const char*(*PrepDataFn)(void* userPtr,
                  Vm* vm,
                                  const char* data,
                                  char* buffer,
                                  U32 bufferLen);

};

struct TypeInfo
{
    StringTableEntry name;
    StringTableEntry inspectorFieldType;
    void* userPtr;
    dsize_t size;
    TypeInterface iFuncs;
};

//
// Class Field Info
//

typedef bool (*WriteDataNotifyFn)( void* obj, StringTableEntry pFieldName );

struct FieldInfo {
   const char* pFieldname;
   const char* pGroupname;
   
   EnumTable *     table;
   const char*     pFieldDocs;
   TypeValidator*  validator;
   SetValueFn        ovrSetValue;
   CopyValueFn       ovrCopyValue;
   WriteDataNotifyFn writeDataFn;
   S32             elementCount;
   U32             offset;
   BitSet32        flag;
   U16             type;
   bool            groupExpand;
};

//
// VM Internal
//

struct ClassInfo;

enum ObjectFlags : U16
{
    ModStaticFields  = BIT(0),
    ModDynamicFields = BIT(1)
};

enum TypeFlags : U32
{
    TypeDirectCopy = BIT(30),
    TypeDirectCopyMask = ~TypeDirectCopy
};

struct VMObject {
   ClassInfo* klass;
   VMNamespace* ns;
   void* userPtr;
   U16 flags;
   U16 refCount; // basic ref count (for interpreter loop)

   VMObject() : klass(NULL), ns(NULL), userPtr(NULL), flags(0), refCount(0) {;}
};

struct VMIterator {
   void *userObject;
   void *internalEntry;
   S32 count;
};

//
// Class Info
//

typedef S32 ClassId;

typedef void (*ConsumerCallback)(U32 level, const char *consoleLine, void* userPtr);
typedef U32 (*AddTaggedStringCallback)(const char* vmString, void* userPtr);


struct Vm;

struct CreateClassReturn
{
    void* userPtr;
    U32 initialFlags;
};

// handles create & destroy
struct CreateObjectInterface
{
    // Create object
	void (*CreateClassFn)(void* user, Vm* vm, CreateClassReturn* ret);
    // Destroys createdPtr
    void (*DestroyClassFn)(void* user, Vm* vm, void* createdPtr);
    // Process args (happens next; usually: name set, then args processed)
    bool (*ProcessArgsFn)(Vm* vm, void* createdPtr, const char* name, bool isDatablock, bool internalName, int argc, const char** argv);
    // i.e. OP_ADD_OBJECT
    // Should perform any registration of the object (unless it has already been performed)
    bool (*AddObjectFn)(Vm* vm, VMObject* object, bool placeAtRoot, U32 groupAddId);
    // Should perform any removal of the object, including any extra vmobject deregistrations
    void (*RemoveObjectFn)(void* user, Vm* vm, VMObject* object);
    // Get identifier (used for return value)
    SimObjectId (*GetIdFn)(VMObject* object);
    // Get Name
    StringTableEntry (*GetNameFn)(VMObject* object);
};

// handles sub object enum
struct EnumerateObjectInterface
{
	U32 (*GetSize)(VMObject* object);
	VMObject* (*GetObjectAtIndex)(VMObject* object, U32 index);
};

// handles field get & set
struct CustomFieldsInterface
{
    bool (*IterateFields)(Vm* vm, VMObject* object, VMIterator& state, StringTableEntry* name);
	ConsoleValue (*GetFieldByIterator)(Vm* vm, VMObject* object, VMIterator& state);
	ConsoleValue (*GetFieldByName)(Vm* vm, VMObject* object, const char* name);
	void (*SetFieldByName)(Vm* vm, VMObject* object, const char* name, ConsoleValue value);
};

struct ClassInfo {
	StringTableEntry name;
	void* userPtr;
	U32 numFields;
   FieldInfo* fields;
   CreateObjectInterface iCreate;
	EnumerateObjectInterface iEnum;
	CustomFieldsInterface iCustomFields;
};

// Sim Apis

// Finding objects
struct FindObjectsInterface
{
	VMObject* (*FindObjectByNameFn)(void* userPtr, StringTableEntry name, VMObject* parent);
   VMObject* (*FindObjectByPathFn)(void* userPtr, const char* path);
	VMObject* (*FindObjectByInternalNameFn)(void* userPtr, StringTableEntry internalName, bool recursive, VMObject* parent);
	VMObject* (*FindObjectByIdFn)(void* userPtr, SimObjectId objectId);
   VMObject* (*FindDatablockGroup)(void* userPtr);
};


//
// VM Config
//

typedef void* (*MallocFn)(size_t, void* user);
typedef void  (*FreeFn)(void*, void* user);


enum TelnetSocket
{
    TELNET_DEBUGGER=1,
    TELNET_CONSOLE=2
};

struct TelnetInterface
{
    bool (*StartListenFn)(void* user, TelnetSocket kind, int port); // callback to start listening
    bool (*StopListenFn)(void* user, TelnetSocket kind); // callback to stop listening

    bool (*CheckSocketActiveFn)(void* user, U32 socket); // callback to check if a socket is active
    U32 (*CheckAcceptFn)(void* user, TelnetSocket kind); // callback for if a connection should be accepted
    bool (*CheckListenFn)(void* user, TelnetSocket kind); // callback for if listener is still active
    bool (*StopSocketFn)(void* user, U32 socket); // callback for if a socket needs disconnecting

    void (*SendDataFn)(void* user, U32 socketNum, U32 bytes, const void* data); // callback for sending data
    bool (*RecvDataFn)(void* user, U32 socketNum, void* data, U32 bufferBytes, U32* outBytes); // callback for receiving data

    void (*GetSocketAddressFn)(void* user, U32 socket, char* buffer); // callback to get socket address; 256 byte buffer

    void (*QueueEvaluateFn)(void* user, const char* evaluateStr); // callback to eval command next frame
};

struct LogConfig
{
  ConsumerCallback cbFunc;
  void*            cbUser;
};

struct Config {
  MallocFn mallocFn;
  FreeFn   freeFn;
  void*    allocUser;

  ConsumerCallback logFn;
  void*            logUser;

  LogConfig extraConsumers[2]; // for telnet
  TelnetInterface iTelnet;
  void* telnetUser;

  FindObjectsInterface iFind;
   void* findUser;

   AddTaggedStringCallback addTagFn;
   void* addTagUser;

   void* vmUser;

   Compiler::Resources* userResources;

   bool warnUndefinedScriptVariables;
   bool enableExceptions;
   bool initTelnet;
   
   U16 maxFibers;
};

struct ConsoleHeapAlloc
{
    ConsoleHeapAlloc* prev;
    ConsoleHeapAlloc* next;
    U32 size;

    void* ptr()
    {
        return this+1;
    }
};

typedef ConsoleHeapAlloc* ConsoleHeapAllocRef;

//
// VM API
//

struct VmInternal;

struct FiberRunResult
{
   enum State : U8
   {
      INACTIVE,
      RUNNING,
      SUSPENDED,
      ERROR, // some form of error occured (otherwise same as inactive)
      FINISHED
   };
   
   KorkApi::ConsoleValue value;
   State state;

   static const char* stateAsString(State inState);
};

enum Constants 
{
  DSOVersion = 77,
  MaxLineLength = 512,
  MaxDataTypes = 256
};

enum
{
StringTagPrefixByte = 0x01
};

class Vm
{
public:
   VmInternal* mInternal;
   
public:
   
	NamespaceId findNamespace(StringTableEntry name, StringTableEntry package = NULL);
   NamespaceId getGlobalNamespace();
   void setNamespaceUsage(NamespaceId ns, const char* usage);
	void activatePackage(StringTableEntry pkgName);
	void deactivatePackage(StringTableEntry pkgName);
   bool linkNamespace(StringTableEntry parent, StringTableEntry child);
   bool unlinkNamespace(StringTableEntry parent, StringTableEntry child);
   bool linkNamespaceById(NamespaceId parent, NamespaceId child);
   bool unlinkNamespaceById(NamespaceId parent, NamespaceId child);
   

    const char* tabCompleteNamespace(NamespaceId nsId, const char *prevText, S32 baseLen, bool fForward);
    const char* tabCompleteVariable(const char *prevText, S32 baseLen, bool fForward);

	TypeId registerType(TypeInfo& info);
	ClassId registerClass(ClassInfo& info);
    ClassId getClassId(const char* name);
    TypeInfo* getTypeInfo(TypeId ident);

	// Hard refs to console values
	ConsoleHeapAllocRef createHeapRef(U32 size);
	void releaseHeapRef(ConsoleHeapAllocRef value);
   
   // Heap values (like strings)
   ConsoleValue getStringFuncBuffer(U32 size);
   ConsoleValue getStringReturnBuffer(U32 size);
   ConsoleValue getTypeFunc(TypeId typeId);
   ConsoleValue getTypeReturn(TypeId typeId);
   
   
   // Gets a string in a specific zone
   ConsoleValue getStringInZone(U16 zone, U32 size);
   ConsoleValue getTypeInZone(U16 zone, TypeId typeId);

   void pushValueFrame();
   void popValueFrame();

	// Public
	VMObject* constructObject(ClassId klassId, const char* name, int argc, const char** argv);
    void setObjectNamespace(VMObject* object, NamespaceId nsId);
   NamespaceId getObjectNamespace(VMObject* object);
	// Internal
	VMObject* createVMObject(ClassId klassId, void* klassPtr);
   
   void incVMRef(VMObject* object);
   void decVMRef(VMObject* object);

   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  StringFuncCallback, void* userPtr, const char *usage, S32 minArgs, S32 maxArgs);
   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  IntFuncCallback, void* userPtr, const char *usage, S32 minArgs, S32 maxArgs);
   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  FloatFuncCallback, void* userPtr, const char *usage, S32 minArgs, S32 maxArgs);
   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  VoidFuncCallback, void* userPtr, const char *usage, S32 minArgs, S32 maxArgs);
   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  BoolFuncCallback, void* userPtr, const char *usage, S32 minArgs, S32 maxArgs);
   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  ValueFuncCallback, void* userPtr, const char *usage, S32 minArgs, S32 maxArgs);
   bool isNamespaceFunction(NamespaceId nsId, StringTableEntry name);
   
   bool compileCodeBlock(const char* code, const char* filename, U32* outCodeSize, U8** outCode);
   ConsoleValue execCodeBlock(U32 codeSize, U8* code, const char* filename, bool noCalls, int setFrame);

   ConsoleValue evalCode(const char* code, const char* filename, S32 setFrame=-1);
   ConsoleValue call(int argc, ConsoleValue* argv, bool startSuspended=false);
   ConsoleValue callObject(VMObject* h, int argc, ConsoleValue* argv, bool startSuspended=false);

   bool callNamespaceFunction(NamespaceId nsId, StringTableEntry name, int argc, ConsoleValue* argv, ConsoleValue& retValue, bool startSuspended=false);
   bool callObjectFunction(VMObject* self, StringTableEntry name, int argc, ConsoleValue* argv, ConsoleValue& retValue, bool startSuspended=false);

   // Helpers (should call into user funcs)
   VMObject* findObjectByName(const char* name);
   VMObject* findObjectByPath(const char* path);
   VMObject* findObjectById(SimObjectId ident);

   bool setObjectField(VMObject* object, StringTableEntry fieldName, ConsoleValue nativeValue, const char* arrayIndex);
   bool setObjectFieldString(VMObject* object, StringTableEntry fieldName, const char* stringValue, const char* arrayIndex);
   ConsoleValue getObjectField(VMObject* object, StringTableEntry fieldName, ConsoleValue nativeValue, const char* arrayIndex);
   const char* getObjectFieldString(VMObject* object, StringTableEntry fieldName, const char** stringValue, const char* arrayIndex);

   void setGlobalVariable(StringTableEntry name, ConsoleValue value);
   void setLocalVariable(StringTableEntry name, ConsoleValue value);
   ConsoleValue getGlobalVariable(StringTableEntry name);
   ConsoleValue getLocalVariable(StringTableEntry name);

   bool registerGlobalVariable(StringTableEntry name, S32 type, void *dptr, const char* usage);
   bool removeGlobalVariable(StringTableEntry name);

   bool isTracing();
   S32 getTracingStackPos();
   void setTracing(bool value);
   
   ConsoleValue::AllocBase getAllocBase() const;

   // Conversion helpers
   F64 valueAsFloat(ConsoleValue v);
   S64 valueAsInt(ConsoleValue v);
   const char* valueAsString(ConsoleValue v);
   bool valueAsBool(ConsoleValue v);

   void* getUserPtr() const;

   // These print to the logger
   void dumpNamespaceClasses( bool dumpScript, bool dumpEngine );
   void dumpNamespaceFunctions( bool dumpScript, bool dumpEngine );

   void dbgSetParameters(int port, const char* password, bool waitForClient);
   bool dbgIsConnected();
   void dbgDisconnect();
   void telnetSetParameters(int port, const char* consolePass, const char* listenPass, bool remoteEcho);
   void telnetDisconnect();

   void processTelnet();

   // Fiber API
   void setCurrentFiberMain();
   void setCurrentFiber(FiberId fiber);
   FiberId createFiber(void* userPtr = NULL); // needs exec too
   FiberId getCurrentFiber();
   FiberRunResult::State getCurrentFiberState();
   void clearCurrentFiberError();
   void* getCurrentFiberUserPtr();
   void cleanupFiber(FiberId fiber);
   void suspendCurrentFiber();
   void throwFiber(U32 mask);
   FiberRunResult resumeCurrentFiber(ConsoleValue value);
};

Vm* createVM(Config* cfg);
void destroyVM(Vm* vm);

}
