#pragma once

#include "console/compiler.h"
#include "console/simpleLexer.h"
#include "console/consoleValue.h"
#include "core/bitSet.h"

class TypeValidator; // TODO: change to interface
class Namespace;
struct EnumTable;

namespace KorkApi
{

typedef const char * (*StringFuncCallback)(void *obj, void* userPtr, S32 argc, const char *argv[]);
typedef S32             (*IntFuncCallback)(void *obj, void* userPtr, S32 argc, const char *argv[]);
typedef F32           (*FloatFuncCallback)(void *obj, void* userPtr, S32 argc, const char *argv[]);
typedef void           (*VoidFuncCallback)(void *obj, void* userPtr, S32 argc, const char *argv[]); // We have it return a value so things don't break..
typedef bool           (*BoolFuncCallback)(void *obj, void* userPtr, S32 argc, const char *argv[]);

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
typedef S32 CodeBlockId;
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

enum ObjectFlags : U32
{
    Deleted   = BIT(0),
    Removed   = BIT(1),
    Added     = BIT(3),
    Selected  = BIT(4),
    Expanded  = BIT(5),
    ModStaticFields  = BIT(6),
    ModDynamicFields = BIT(7)
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
   U32 flags;
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


struct Vm;

// handles create & destroy
struct CreateObjectInterface
{
    // Create object
	void* (*CreateClassFn)(void* user, Vm* vm, VMObject* object);
    // Destroy
    void (*DestroyClassFn)(void* user, Vm* vm, void* createdPtr);
    // Process args (happens next; usually: name set, then args processed)
    bool (*ProcessArgs)(Vm* vm, VMObject* object, const char* name, bool isDatablock, bool internalName, int argc, const char** argv);
    // i.e. OP_ADD_OBJECT
    bool (*AddObject)(Vm* vm, VMObject* object, bool placeAtRoot, U32 groupAddId);
    // Get identifier (used for return value)
    SimObjectId (*GetId)(VMObject* object);
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
};


//
// VM Config
//

typedef void* (*MallocFn)(size_t, void* user);
typedef void  (*FreeFn)(void*, void* user);

struct Config {
  MallocFn mallocFn;
  FreeFn   freeFn;
  void*    allocUser;

  ConsumerCallback logFn;
  void*            logUser;

  FindObjectsInterface iFind;
   void* findUser;

   void* vmUser;
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
   ConsoleValue getStringReturnBuffer(U32 size);
   ConsoleValue getStringArgBuffer(U32 size);
   ConsoleValue getTypeArg(TypeId typeId);
   ConsoleValue getTypeReturn(TypeId typeId);
   ConsoleValue getStringInZone(U16 zone, U32 size);
   ConsoleValue getTypeInZone(U16 zone, TypeId typeId);

   void pushValueFrame();
   void popValueFrame();

	// Public
	VMObject* constructObject(ClassId klassId, const char* name, int argc, const char** argv);
   VMObject* setObjectNamespace(VMObject* object, NamespaceId nsId);
   NamespaceId getObjectNamespace(VMObject* object);
	// Internal
	VMObject* createVMObject(ClassId klassId, void* klassPtr);
    void destroyVMObject(VMObject* object);

   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  StringFuncCallback, void* userPtr, const char *usage, S32 minArgs, S32 maxArgs);
   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  IntFuncCallback, void* userPtr, const char *usage, S32 minArgs, S32 maxArgs);
   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  FloatFuncCallback, void* userPtr, const char *usage, S32 minArgs, S32 maxArgs);
   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  VoidFuncCallback, void* userPtr, const char *usage, S32 minArgs, S32 maxArgs);
   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  BoolFuncCallback, void* userPtr, const char *usage, S32 minArgs, S32 maxArgs);
   bool isNamespaceFunction(NamespaceId nsId, StringTableEntry name);
   
   bool compileCodeBlock(const char* code, const char* filename, U32* outCodeSize, U32** outCode);
   ConsoleValue execCodeBlock(U32 codeSize, U8* code, const char* filename, bool noCalls, int setFrame);

   ConsoleValue evalCode(const char* code, const char* filename);
   ConsoleValue call(int argc, const char** argv);
   ConsoleValue callObject(VMObject* h, int argc, const char** argv);

   bool callNamespaceFunction(NamespaceId nsId, StringTableEntry name, int argc, const char** argv, ConsoleValue& retValue);
    bool callObjectFunction(VMObject* self, StringTableEntry name, int argc, const char** argv, ConsoleValue& retValue);

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
};

Vm* createVM(Config* cfg);
void destroyVm(Vm* vm);

}
