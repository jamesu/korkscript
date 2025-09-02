#pragma once
#include "console/console.h"

#include "console/compiler.h"
#include "console/simpleLexer.h"
#include "console/ast.h"
#include "console/simpleParser.h"
#include "console/consoleObject.h"

namespace KorkApi
{

struct ConsoleValue {
	U32 typeId;
	union {
		F64 number;
		S64 integer;
		const char* value;
		void* ptr;
		bool flag;
	};
};

typedef U32 SimObjectId;
typedef S32 NamespaceId;
typedef S32 CodeBlockId;
using VMNamespace = Namespace;

//
// Type info
//

typedef S32 TypeId;

struct TypeInterface
{
void(*SetDataFn)(void* userPtr,
                          void* dptr,
                          S32 argc,
                          const char** argv,
                          const EnumTable* tbl,
                          BitSet32 flag);
void(*SetValueFn)(void* userPtr,
                          void* dptr,
                          S32 argc,
                          ConsoleValue* argv,
                          const EnumTable* tbl,
                          BitSet32 flag);
const char*(*GetDataFn)(void* userPtr,
                                 void* dptr,
                                 const EnumTable* tbl,
                                 BitSet32 flag);
ConsoleValue(*ExtractValueFn)(void* userPtr,
                              void* dptr,
                              const EnumTable* tbl,
                              BitSet32 flag);
const char*(*GetTypeClassNameFn)(void* userPtr);

const char*(*PrepDataFn)(void* userPtr,
                                  const char* data,
                                  char* buffer,
                                  U32 bufferLen);
StringTableEntry(*GetTypePrefixFn)(void* userPtr);
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

using SetDataNotify = AbstractClassRep::SetDataNotify;
using GetDataNotify = AbstractClassRep::GetDataNotify;
using WriteDataNotify = AbstractClassRep::WriteDataNotify;

struct FieldInfo {
	StringTableEntry name;
	U32 offset;
	TypeId typeId;
	SetDataNotify in_setDataFn;
	GetDataNotify in_getDataFn;
	WriteDataNotify in_writeDataFn;
};

//
// VM Internal
//

struct ClassInfo;

struct VMObject {
   ClassInfo* klass;
   VMNamespace* ns;
   void* userPtr;
};

//
// Class Info
//

typedef S32 ClassId;

// handles create & destroy
struct CreateOjectInterface
{
	void* (*CreateClassFn)(void* user, VMObject* object, const char* name, int argc, char** argv);
	void  (*DestroyClassFn)(void* user, void* createdPtr);
};

// handles sub object enum
struct EnumerateObjectInterface
{
	U32* (*GetSize)(VMObject* object);
	VMObject* (*GetObjectAtIndex)(VMObject* object, U32 index);
};

// handles field get & set
struct CustomFieldsInterface
{
	U32* (*GetNumFields)(VMObject* object);
	ConsoleValue (*GetField)(VMObject* user, U32 index);
	ConsoleValue (*GetFieldByName)(VMObject* object, const char* name);
	void (*SetFieldByName)(VMObject* object, const char* name, ConsoleValue value);
};

struct ClassInfo {
	StringTableEntry name;
	void* userPtr;
	ClassId parentClass;
	U32 numFields;
   FieldInfo* fields;
	CreateOjectInterface iCreate;
	EnumerateObjectInterface iEnum;
	CustomFieldsInterface iCustomFields;
};

// Sim Apis

// Finding objects
struct FindObjectsInterface
{
	VMObject* (*FindObjectByNameFn)(StringTableEntry name);
   VMObject* (*FindObjectByPathFn)(const char* path);
	VMObject* (*FindObjectByInternalNameFn)(StringTableEntry internalName);
	VMObject* (*FindObjectByIdFn)(SimObjectId objectId);
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

  ConsumerCallback callbackFn;
  void*            callbackUser;

  FindObjectsInterface iFind;
};

struct HardConsoleValueRef
{
	S32 hardIndex;
	ConsoleValue value;
};

//
// VM API
//

struct VmInternal;

class Vm
{
public:
   VmInternal* mInternal;
   
public:
   
	S32 registerNamespace(StringTableEntry name, StringTableEntry package);
	S32 getNamespace(StringTableEntry name, StringTableEntry package);
	S32 getGlobalNamespace();
	void activatePackage(StringTableEntry pkgName);
	void deactivatePackage(StringTableEntry pkgName);
   bool linkNamespace(NamespaceId parent, NamespaceId child);
   bool unlinkNamespace(NamespaceId parent, NamespaceId child);
   
	TypeId registerType(TypeInfo& info);
	ClassId registerClass(ClassInfo& info);

	// Hard refs to console values
	HardConsoleValueRef createHardRef(ConsoleValue value);
	void releaseHardRef(HardConsoleValueRef value);
   
   // Heap values (like strings)
   ConsoleValue getStringReturnBuffer(U32 size);
   ConsoleValue getTypeReturn(TypeId typeId);

	// Public
	VMObject* constructObject(ClassId klassId, const char* name, int argc, char** argv);
   VMObject* setObjectNamespace(VMObject* object, NamespaceId nsId);
	// Internal
	VMObject* createVMObject(ClassId klassId, void* klassPtr);

   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  StringCallback, const char *usage, S32 minArgs, S32 maxArgs);
   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  IntCallback, const char *usage, S32 minArgs, S32 maxArgs);
   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  FloatCallback, const char *usage, S32 minArgs, S32 maxArgs);
   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  VoidCallback, const char *usage, S32 minArgs, S32 maxArgs);
   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  BoolCallback, const char *usage, S32 minArgs, S32 maxArgs);

   bool compileCodeBlock(const char* code, const char* filename, U32* outCodeSize, U32** outCode);
   ConsoleValue execCodeBlock(U32 codeSize, U8* code, const char* filename, bool noCalls, int setFrame);

   ConsoleValue evalCode(const char* code, const char* filename);
   ConsoleValue call(int argc, const char** argv);
   ConsoleValue callObject(VMObject* h, int argc, const char** argv);

   // Helpers (should call into user funcs)
   VMObject* findObjectByName(const char* name);
   VMObject* findObjectByPath(const char* path);
   VMObject* findObjectById(SimObjectId ident);

   bool setObjectFieldNative(VMObject* object, StringTableEntry fieldName, void* nativeValue, U32* arrayIndex);
   bool setObjectFieldString(VMObject* object, StringTableEntry fieldName, const char* stringValue, U32* arrayIndex);
   bool getObjectFieldNative(VMObject* object, StringTableEntry fieldName, void* nativeValue, U32* arrayIndex);
   bool getObjectFieldString(VMObject* object, StringTableEntry fieldName, const char** stringValue, U32* arrayIndex);
};

Vm* createVM(Config* cfg);
void destroyVm(Vm* vm);

}
