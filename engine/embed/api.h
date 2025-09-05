#pragma once
#include "console/console.h"

#include "console/compiler.h"
#include "console/simpleLexer.h"
#include "console/ast.h"
#include "console/simpleParser.h"
#include "console/consoleValue.h"

class TypeValidator; // TODO: change to interface
class Namespace;

namespace KorkApi
{

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

/// This is a function pointer typedef to support get/set callbacks for fields
typedef bool (*SetDataNotify)( void *obj, const char *data );
typedef const char *(*GetDataNotify)( void *obj, const char *data );

/// This is a function pointer typedef to support optional writing for fields.
typedef bool (*WriteDataNotify)( void* obj, const char* pFieldName );

struct FieldInfo {
   const char* pFieldname;    ///< Name of the field.
   const char* pGroupname;    ///< Optionally filled field containing the group name.
   ///
   ///  This is filled when type is StartField or EndField
   EnumTable *     table;         ///< If this is an enum, this points to the table defining it.
   const char*     pFieldDocs;    ///< Documentation about this field; see consoleDoc.cc.
   TypeValidator*  validator;     ///< Validator, if any.
   SetDataNotify   setDataFn;     ///< Set data notify Fn
   GetDataNotify   getDataFn;     ///< Get data notify Fn
   WriteDataNotify writeDataFn;   ///< Function to determine whether data should be written or not.
   S32             elementCount;  ///< Number of elements, if this is an array.
   U32             offset;        ///< Memory offset from beginning of class for this field.
   BitSet32        flag;          ///< Stores various flags
   U16             type;          ///< A type ID. @see ACRFieldTypes
   bool            groupExpand;   ///< Flag to track expanded/not state of this group in the editor.
};

//
// VM Internal
//

struct ClassInfo;

enum ObjectFlags : U32
{
    Deleted   = BIT(0),   ///< This object is marked for deletion.
    Removed   = BIT(1),   ///< This object has been unregistered from the object system.
    Added     = BIT(3),   ///< This object has been registered with the object system.
    Selected  = BIT(4),   ///< This object has been marked as selected. (in editor)
    Expanded  = BIT(5),   ///< This object has been marked as expanded. (in editor)
    ModStaticFields  = BIT(6),    ///< The object allows you to read/modify static fields
    ModDynamicFields = BIT(7)     ///< The object allows you to read/modify dynamic fields
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

struct Vm;

// handles create & destroy
struct CreateObjectInterface
{
    // Create object
	void* (*CreateClassFn)(void* user, VMObject* object);
    // Destroy
    void (*DestroyClassFn)(void* user, void* createdPtr);
    // Process args (happens next; usually: name set, then args processed)
    bool (*ProcessArgs)(Vm* vm, VMObject* object, const char* name, bool isDatablock, bool internalName, int argc, const char** argv);
    // Get identifier (used for return value)
    ConsoleValue (*GetId)(VMObject* object);
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
    bool (*IterateFields)(KorkApi::VMObject* object, VMIterator& state, StringTableEntry* name);
	ConsoleValue (*GetFieldByIterator)(VMObject* object, VMIterator& state);
	ConsoleValue (*GetFieldByName)(VMObject* object, const char* name);
	void (*SetFieldByName)(VMObject* object, const char* name, ConsoleValue value);
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

  ConsumerCallback logFn;
  void*            logUser;

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
    TypeInfo* getTypeInfo(TypeId ident);

	// Hard refs to console values
	HardConsoleValueRef createHardRef(ConsoleValue value);
	void releaseHardRef(HardConsoleValueRef value);
   
   // Heap values (like strings)
   ConsoleValue getStringReturnBuffer(U32 size);
   ConsoleValue getTypeReturn(TypeId typeId);

   void pushValueFrame();
   void popValueFrame();

	// Public
	VMObject* constructObject(ClassId klassId, const char* name, int argc, const char** argv);
   VMObject* setObjectNamespace(VMObject* object, NamespaceId nsId);
   NamespaceId getObjectNamespace(VMObject* object);
	// Internal
	VMObject* createVMObject(ClassId klassId, void* klassPtr);
    void destroyVMObject(VMObject* object);

   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  StringCallback, const char *usage, S32 minArgs, S32 maxArgs);
   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  IntCallback, const char *usage, S32 minArgs, S32 maxArgs);
   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  FloatCallback, const char *usage, S32 minArgs, S32 maxArgs);
   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  VoidCallback, const char *usage, S32 minArgs, S32 maxArgs);
   void addNamespaceFunction(NamespaceId nsId, StringTableEntry name,  BoolCallback, const char *usage, S32 minArgs, S32 maxArgs);
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

   bool setObjectFieldNative(VMObject* object, StringTableEntry fieldName, void* nativeValue, U32* arrayIndex);
   bool setObjectFieldString(VMObject* object, StringTableEntry fieldName, const char* stringValue, U32* arrayIndex);
   bool getObjectFieldNative(VMObject* object, StringTableEntry fieldName, void* nativeValue, U32* arrayIndex);
   bool getObjectFieldString(VMObject* object, StringTableEntry fieldName, const char** stringValue, U32* arrayIndex);

   void setGlobalVariable(StringTableEntry name, const char* value);
   void setLocalVariable(StringTableEntry name, const char* value);
   ConsoleValue getGlobalVariable(StringTableEntry name);
   ConsoleValue getLocalVariable(StringTableEntry name);

   bool registerGlobalVariable(StringTableEntry name, S32 type, void *dptr, const char* usage);
   bool removeGlobalVariable(StringTableEntry name);

   bool isTracing();
   S32 getTracingStackPos();
   void setTracing(bool value);
   
   ConsoleValue::AllocBase getAllocBase() const;
};

Vm* createVM(Config* cfg);
void destroyVm(Vm* vm);

}
