#include "platform/platform.h"
#include "console/simpleLexer.h"
#include "console/ast.h"
#include "console/compiler.h"
#include "console/simpleParser.h"
#include "core/fileStream.h"
#include <stdio.h>
#include "embed/api.h"
#include "embed/internalApi.h" // debug
#include <emscripten/bind.h>

using namespace KorkApi;
using JsVal = emscripten::val;

struct JsEnv
{
   JsVal logFn = JsVal::undefined();
   
   // iFind functions (optional; may be undefined)
   JsVal findByName         = JsVal::undefined();
   JsVal findByPath         = JsVal::undefined();
   JsVal findByInternalName = JsVal::undefined();
   JsVal findById           = JsVal::undefined();
};

struct WrappedConsoleValue
{
   KorkApi::ConsoleValue cv;
   std::string hold; // owns bytes if cv is a string
};

//
// Helpers
//

inline bool js_is_nullish(const JsVal& v)
{
   return v.isNull() || v.isUndefined();
}

inline bool js_is_array(const JsVal& v)
{
   static JsVal Array = JsVal::global("Array");
   return Array.call<bool>("isArray", v);
}

inline bool js_is_function(const JsVal& v)
{
   if (js_is_nullish(v)) return false;
   static JsVal Fn = JsVal::global("Function");
   return v.instanceof(Fn);
}

inline bool js_has_own(const JsVal& obj, const char* prop)
{
   if (js_is_nullish(obj)) return false;
   static JsVal hasOwn = JsVal::global("Object")["prototype"]["hasOwnProperty"];
   return hasOwn.call<bool>("call", obj, JsVal(prop));
}

// Wrapper for generic JS values
struct JsRef
{
   JsVal v;
   JsRef(JsVal vv) : v(std::move(vv)) {}
};

// Wrapper for Console Namespace callbacks.
// Signature for wrapper is: (objectInfo, vm, argv[])
struct NSFuncCtx
{
   Vm* vm; // Useful reference to vm
   JsVal cb; // handler
};

// Wrapper for JS TypeInfo values
// Converted from input objects in VmJS::registerType
struct TypeBinding
{
   Vm* vm = nullptr;
   
   // stable C-string storage
   std::string nameBuf;
   std::string inspectorBuf;
   std::string prepScratch;
   
   dsize_t size = 0;
   
   // JS callbacks
   JsVal cb_setValue;
   JsVal cb_copyValue;
   JsVal cb_getTypeName;
   JsVal cb_prepData;
   
   TypeBinding() : 
   cb_setValue(JsVal::undefined()),
   cb_copyValue(JsVal::undefined()),
   cb_getTypeName(JsVal::undefined()),
   cb_prepData(JsVal::undefined())
   {
      
   }
};

// Wrapper for JS ClassInfo values
// Converted from input objects in VmJS::registerClass
struct ClassBinding 
{
   Vm* vm;
   VMObject* vmObject;
   
   // stable storage for C-string fields
   std::string nameBuf;
   
   // JS callbacks
   JsVal cb_create;
   JsVal cb_destroy;
   JsVal cb_processArgs;
   JsVal cb_addObject;
   JsVal cb_removeObject;
   JsVal cb_getId;
   
   // enumeration
   JsVal cb_enum_getSize;
   JsVal cb_enum_getAtIndex;
   
   // custom fields
   JsVal cb_cf_getByName;
   JsVal cb_cf_setByName;
   
   ClassBinding() : 
   vm(NULL),
   vmObject(NULL),
   cb_create(JsVal::undefined()),
   cb_destroy(JsVal::undefined()),
   cb_processArgs(JsVal::undefined()),
   cb_addObject(JsVal::undefined()),
   cb_removeObject(JsVal::undefined()),
   cb_getId(JsVal::undefined()),
   cb_enum_getSize(JsVal::undefined()),
   cb_enum_getAtIndex(JsVal::undefined()),
   cb_cf_getByName(JsVal::undefined()),
   cb_cf_setByName(JsVal::undefined())
   {
   }
};

// Per-object binding stored in VMObject::userPtr
struct ObjBinding
{
   ClassBinding* klass;  // backlink to class binding
   JsVal         peer; // JS peer returned by create()
   
   ObjBinding() : klass(nullptr), peer(JsVal::undefined())
   {
      
   }
};

// Attach a JS object to VMObject::userPtr (replaces existing if present)
inline void attachJsUser(VMObject* o, JsVal js)
{
   if (!o) 
   {
      return;
   }
   
   // free any previous ref
   if (o->userPtr) 
   {
      delete static_cast<JsRef*>(o->userPtr);
      o->userPtr = nullptr;
   }
   
   // store new rooted handle
   o->userPtr = new JsRef(std::move(js));
}

// Get the JS object back (undefined if none)
inline JsVal getJsUser(VMObject* o)
{
   if (!o || !o->userPtr) return JsVal::undefined();
   return static_cast<JsRef*>(o->userPtr)->v;
}

// Clear and free
inline void clearJsUser(VMObject* o)
{
   if (!o || !o->userPtr) return;
   delete static_cast<JsRef*>(o->userPtr);
   o->userPtr = nullptr;
}

// Convert JS -> wrapped CV
static void CVFromJS(const JsVal& v, WrappedConsoleValue& outV)
{
   outV.cv = ConsoleValue();
   
   if (v.isString())
   {
      outV.hold = v.as<std::string>();
      outV.cv.setString(outV.hold.c_str(), ConsoleValue::ZoneExternal);
   }
   else if (v.isNumber()) 
   {
      double d = v.as<double>();
      outV.cv.setFloat(d);
   }
}

// Convert JS -> wrapped CV (using return buffer)
static void CVFromJSReturn(Vm* vm, const JsVal& v, ConsoleValue& outV)
{
   outV = ConsoleValue();
   
   if (v.isString())
   {
      std::string copyStr = v.as<std::string>();
      outV = vm->getStringReturnBuffer(copyStr.size());
      memcpy(outV.ptr(), copyStr.c_str(), copyStr.size());
   }
   else if (v.isNumber()) 
   {
      double d = v.as<double>();
      outV.setFloat(d);
   }
}

// Convert CV -> JS value
static JsVal JSFromCV(Vm* vm, const ConsoleValue& v)
{
   if (v.isInt())    return JsVal(static_cast<double>(vm->valueAsInt(v)));
   if (v.isFloat())  return JsVal(vm->valueAsFloat(v));
   const char* s = vm->valueAsString(v);
   // TODO: type'd values
   return JsVal(s ? std::string(s) : std::string());
}

static void* CAlloc(size_t sz, void*) { return std::malloc(sz); }
static void  CFree (void* p,     void*) { std::free(p); }

// Logger
static void LogThunk(U32 level, const char* line, void* user) 
{
   auto* env = static_cast<JsEnv*>(user);
   if (!env || env->logFn.isUndefined() || env->logFn.isNull()) return;
   env->logFn(JsVal(level), JsVal(line ? line : ""));
}

//
// iFind
//

static VMObject* FindByNameThunk(void* user, StringTableEntry name, VMObject* parent);
static VMObject* FindByPathThunk(void* user, const char* path);
static VMObject* FindByInternalNameThunk(void* user, StringTableEntry internalName, bool recursive, VMObject* parent);
static VMObject* FindByIdThunk(void* user, SimObjectId objectId);

//
// iCreate
//

static void Class_CreateThunk(void* user, Vm* vm, CreateClassReturn* outP);
static void Class_DestroyThunk(void* user, Vm* vm, void* createdPtr);
static bool Class_ProcessArgsThunk(Vm* vm, void* createdPtr, const char* name, bool isDatablock, bool internalName, int argc, const char** argv);
static bool Class_AddObjectThunk(Vm* vm, VMObject* object, bool placeAtRoot, U32 groupAddId);
static SimObjectId Class_GetIdThunk(VMObject* object);

//
// iEnum
//

static U32 Enum_GetSizeThunk(VMObject* object);
static VMObject* Enum_GetAtIndexThunk(VMObject* object, U32 index);

//
// iCustomFields
//

static ConsoleValue CF_GetFieldByNameThunk(Vm* vm, VMObject* object, const char* name);
static void CF_SetFieldByNameThunk(Vm* vm, VMObject* object, const char* name, ConsoleValue value);

//
// TypeInfo Thunks
//

static void Type_SetValueThunk(
                               void* userPtr,
                               Vm* vm,
                               void* dptr,
                               S32 argc,
                               ConsoleValue* argv,
                               const EnumTable* tbl,
                               BitSet32 flag,
                               U32 typeId);

static ConsoleValue Type_CopyValueThunk(
                                        void* userPtr,
                                        Vm* vm,
                                        void* sptr,
                                        const EnumTable* tbl,
                                        BitSet32 flag,
                                        U32 requestedType,
                                        U32 requestedZone);

static const char* Type_GetTypeClassNameThunk(void* userPtr);

static const char* Type_PrepDataThunk(
                                      void* userPtr,
                                      Vm* vm,
                                      const char* data,
                                      char* buffer,
                                      U32 bufferLen);

// Builds a JS argv array from (argc, argv); currently assumes strings as input.
static JsVal make_js_argv(S32 argc, const char* const* argv)
{
   JsVal arr = JsVal::array();
   for (S32 i = 0; i < argc; ++i) {
      const char* s = (argv && argv[i]) ? argv[i] : "";
      arr.set(i, JsVal(std::string(s)));
   }
   return arr;
}

// Console Namespace Thunks
// Callback wrapper is passed through as userPtr, object wrapper is obj.
// See NSFuncCtx for the actual callback wrapper

static const char* NS_StringThunk(void* obj, void* userPtr, S32 argc, const char* argv[]);
static S32 NS_IntThunk(void* obj, void* userPtr, S32 argc, const char* argv[]);
static F32 NS_FloatThunk(void* obj, void* userPtr, S32 argc, const char* argv[]);
static void NS_VoidThunk(void* obj, void* userPtr, S32 argc, const char* argv[]);
static bool NS_BoolThunk(void* obj, void* userPtr, S32 argc, const char* argv[]);

//
// Vm Wrapper
//

class VmJS 
{
public:
   Vm* mVm;
   std::unique_ptr<JsEnv> mEnv;
   std::vector<std::unique_ptr<ClassBinding>> mClassBindings;
   std::vector<std::unique_ptr<TypeBinding>> mTypeBindings;
   std::vector<std::unique_ptr<NSFuncCtx>> mNSFuncBindings;
   
public:
   
   VmJS(JsVal jsCfg)
   {
      mEnv = std::make_unique<JsEnv>();
      
      if (!jsCfg.isUndefined() && !jsCfg.isNull())
      {
         if (jsCfg.hasOwnProperty("logFn") && js_is_function(jsCfg["logFn"]))
         {
            mEnv->logFn = jsCfg["logFn"];
         }
         
         if (jsCfg.hasOwnProperty("iFind") && !js_is_nullish(jsCfg["iFind"]))
         {
            auto f = jsCfg["iFind"];
            if (f.hasOwnProperty("byName") && js_is_function(f["byName"]))                 mEnv->findByName = f["byName"];
            if (f.hasOwnProperty("byPath") && js_is_function(f["byPath"]))                 mEnv->findByPath = f["byPath"];
            if (f.hasOwnProperty("byInternalName") && js_is_function(f["byInternalName"])) mEnv->findByInternalName = f["byInternalName"];
            if (f.hasOwnProperty("byId") && js_is_function(f["byId"]))                     mEnv->findById = f["byId"];
         }
      }
      
      Config cfg = {};
      cfg.mallocFn = &CAlloc;
      cfg.freeFn   = &CFree;
      cfg.allocUser = nullptr;
      
      cfg.logFn   = &LogThunk;
      cfg.logUser = mEnv.get();
      
      cfg.iFind.FindObjectByNameFn         = &FindByNameThunk;
      cfg.iFind.FindObjectByPathFn         = &FindByPathThunk;
      cfg.iFind.FindObjectByInternalNameFn = &FindByInternalNameThunk;
      cfg.iFind.FindObjectByIdFn           = &FindByIdThunk;
      cfg.findUser = mEnv.get();
      
      cfg.vmUser = this;
      
      mVm = createVM(&cfg);
   }
   
   ~VmJS()
   {
      if (mVm)
      {
         destroyVM(mVm);
      }
   }
   
   // evalCode(code, filename) -> string|number
   JsVal evalCode(const std::string& code, const std::string& filename)
   {
      ConsoleValue v = mVm->evalCode(code.c_str(), filename.c_str());
      return JSFromCV(mVm, v);
   }
   
   // call(argv: (string|number)[]) -> string|number
   JsVal call(JsVal jsArgv)
   {
      if (!jsArgv.isArray()) return JsVal::null();
      
      const int argc = jsArgv["length"].as<int>();
      // argv must be const char** for the VM's call(); convert all args to strings
      std::vector<void*> scratch;
      std::vector<std::string> strBuf(argc);
      std::vector<const char*> argv(argc, nullptr);
      
      for (int i = 0; i < argc; ++i)
      {
         JsVal v = jsArgv[i];
         if (v.isString()) 
         {
            strBuf[i] = v.as<std::string>();
         } 
         else if (v.isNumber()) 
         {
            // Keep textual semantics for VM argv
            double d = v.as<double>();
            if (std::isfinite(d) && std::floor(d) == d) 
            {
               // integer print
               char tmp[32];
               std::snprintf(tmp, sizeof(tmp), "%lld", static_cast<long long>(d));
               strBuf[i] = tmp;
            } 
            else 
            {
               char tmp[64];
               std::snprintf(tmp, sizeof(tmp), "%.17g", d);
               strBuf[i] = tmp;
            }
         } 
         else 
         {
            strBuf[i] = ""; // unsupported becomes empty
         }
         argv[i] = strBuf[i].c_str();
      }
      
      ConsoleValue v = mVm->call(argc, argv.data());
      return JSFromCV(mVm, v);
   }
   
   // setGlobal(name, value:string|number)
   void setGlobal(const std::string& name, JsVal jsVal) 
   {
      WrappedConsoleValue wcv;
      CVFromJS(jsVal, wcv);
      mVm->setGlobalVariable(StringTable->insert(name.c_str()), wcv.cv);
   }
   
   // getGlobal(name) -> string|number
   JsVal getGlobal(const std::string& name) 
   {
      ConsoleValue v = mVm->getGlobalVariable(StringTable->insert(name.c_str()));
      return JSFromCV(mVm, v);
   }
   
   int registerClass(JsVal jsSpec)
   {
      // Build & own a ClassBinding for this registration
      auto kb = std::make_unique<ClassBinding>();
      kb->vm = mVm;
      
      // name (required)
      if (!js_has_own(jsSpec, "name")) 
      {
         return -1;
      }
      kb->nameBuf = jsSpec["name"].as<std::string>();
      
      // iCreate
      if (js_has_own(jsSpec, "iCreate"))
      {
         JsVal c = jsSpec["iCreate"];
         if (js_is_function(c["create"]))        kb->cb_create       = c["create"];
         if (js_is_function(c["destroy"]))       kb->cb_destroy      = c["destroy"];
         if (js_is_function(c["processArgs"]))   kb->cb_processArgs  = c["processArgs"];
         if (js_is_function(c["addObject"]))     kb->cb_addObject    = c["addObject"];
         if (js_is_function(c["removeObject"]))  kb->cb_removeObject    = c["removeObject"];
         if (js_is_function(c["getId"]))         kb->cb_getId        = c["getId"];
      }
      
      // iEnum
      if (js_has_own(jsSpec, "iEnum"))
      {
         JsVal e = jsSpec["iEnum"];
         if (js_is_function(e["getSize"]))       kb->cb_enum_getSize    = e["getSize"];
         if (js_is_function(e["getObjectAtIndex"])) kb->cb_enum_getAtIndex = e["getObjectAtIndex"];
      }
      
      // iCustomFields
      if (js_has_own(jsSpec, "iCustomFields"))
      {
         JsVal f = jsSpec["iCustomFields"];
         if (js_is_function(f["getFieldByName"])) kb->cb_cf_getByName = f["getFieldByName"];
         if (js_is_function(f["setFieldByName"])) kb->cb_cf_setByName = f["setFieldByName"];
      }
      
      // Fill ClassInfo
      ClassInfo info{};
      info.name     =  StringTable->insert(kb->nameBuf.c_str());
      info.userPtr  = kb.get();        // engine will pass this as 'user' to iCreate
      info.numFields = 0;
      info.fields    = nullptr;
      
      // iCreate (always populate; JS handlers may be undefined)
      info.iCreate.CreateClassFn = &Class_CreateThunk;
      info.iCreate.DestroyClassFn = &Class_DestroyThunk;
      info.iCreate.ProcessArgsFn = &Class_ProcessArgsThunk;
      info.iCreate.AddObjectFn   = &Class_AddObjectThunk;
      info.iCreate.GetIdFn       = &Class_GetIdThunk;
      
      // iEnum
      info.iEnum.GetSize          = &Enum_GetSizeThunk;
      info.iEnum.GetObjectAtIndex = &Enum_GetAtIndexThunk;
      
      // iCustomFields (only the simple ones)
      info.iCustomFields.IterateFields      = nullptr;
      info.iCustomFields.GetFieldByIterator = nullptr;
      info.iCustomFields.GetFieldByName     = &CF_GetFieldByNameThunk;
      info.iCustomFields.SetFieldByName     = &CF_SetFieldByNameThunk;
      
      // Register
      ClassId cid = mVm->registerClass(info);
      
      // Keep kb alive for the VM lifetime
      mClassBindings.push_back(std::move(kb));
      
      return cid;
   }
   
   int registerType(JsVal jsSpec)
   {
      // Build & keep a TypeBinding alive for this type
      auto tb = std::make_unique<TypeBinding>();
      tb->vm = mVm;
      
      // name (required)
      if (!js_has_own(jsSpec, "name")) return -1;
      tb->nameBuf = jsSpec["name"].as<std::string>();
      
      // optional inspector field type
      if (js_has_own(jsSpec, "inspectorFieldType"))
      {
         tb->inspectorBuf = jsSpec["inspectorFieldType"].as<std::string>();
      }
      
      // optional size
      if (js_has_own(jsSpec, "size"))
      {
         tb->size = static_cast<dsize_t>(jsSpec["size"].as<unsigned>());
      }
      
      // callbacks
      if (js_has_own(jsSpec, "setValue") && js_is_function(jsSpec["setValue"]))           tb->cb_setValue    = jsSpec["setValue"];
      if (js_has_own(jsSpec, "copyValue") && js_is_function(jsSpec["copyValue"]))         tb->cb_copyValue   = jsSpec["copyValue"];
      if (js_has_own(jsSpec, "getTypeClassName") && js_is_function(jsSpec["getTypeClassName"]))
      {
         tb->cb_getTypeName = jsSpec["getTypeClassName"];
      }
      if (js_has_own(jsSpec, "prepData") && js_is_function(jsSpec["prepData"]))           tb->cb_prepData    = jsSpec["prepData"];
      
      // Fill TypeInfo
      TypeInfo info{};
      info.name                =  StringTable->insert(tb->nameBuf.c_str());
      info.inspectorFieldType  = tb->inspectorBuf.empty() ? nullptr :  StringTable->insert((StringTableEntry)tb->inspectorBuf.c_str());
      info.userPtr             = tb.get();
      info.size                = tb->size;
      
      info.iFuncs.SetValue             = &Type_SetValueThunk;
      info.iFuncs.CopyValue            = &Type_CopyValueThunk;
      info.iFuncs.GetTypeClassNameFn   = &Type_GetTypeClassNameThunk;
      info.iFuncs.PrepDataFn           = &Type_PrepDataThunk;
      
      TypeId tid = mVm->registerType(info);
      
      mTypeBindings.push_back(std::move(tb)); // keep alive for VM lifetime
      return tid;
   }
   
   // --- Namespace lookups / links ---
   uintptr_t findNamespace(const std::string& name, const std::string& package = std::string())
   {
      auto ns = mVm->findNamespace(StringTable->insert(name.c_str()),
                                   package.empty() ? (StringTableEntry)nullptr
                                   : StringTable->insert(package.c_str()));
      return (uintptr_t)ns;
   }
   
   uintptr_t getGlobalNamespace()
   {
      return (uintptr_t)mVm->getGlobalNamespace();
   }
   
   void setNamespaceUsage(uintptr_t nsPtr, const std::string& usage)
   {
      mVm->setNamespaceUsage((VMNamespace*)nsPtr, usage.c_str());
   }
   
   void activatePackage(const std::string& pkg)    
   { 
      mVm->activatePackage(StringTable->insert(pkg.c_str())); 
   }
   
   void deactivatePackage(const std::string& pkg) 
   { 
      mVm->deactivatePackage( StringTable->insert(pkg.c_str())); 
   }
   
   bool linkNamespace(const std::string& parent, const std::string& child)   
   {
      return mVm->linkNamespace((StringTableEntry)parent.c_str(), StringTable->insert(child.c_str()));
   }
   
   bool unlinkNamespace(const std::string& parent, const std::string& child) 
   {
      return mVm->unlinkNamespace((StringTableEntry)parent.c_str(),  StringTable->insert(child.c_str()));
   }
   
   bool linkNamespaceById(uintptr_t parentPtr, uintptr_t childPtr)   
   { 
      return mVm->linkNamespaceById((VMNamespace*)parentPtr,(VMNamespace*)childPtr); 
   }
   
   bool unlinkNamespaceById(uintptr_t parentPtr, uintptr_t childPtr) 
   { 
      return mVm->unlinkNamespaceById((VMNamespace*)parentPtr,(VMNamespace*)childPtr); 
   }
   
   bool isNamespaceFunction(uintptr_t nsPtr, const std::string& name) 
   {
      return mVm->isNamespaceFunction((VMNamespace*)nsPtr,  StringTable->insert(name.c_str()));
   }
   
   // call into a namespace function and return string|number
   JsVal callNamespace(uintptr_t nsPtr, const std::string& name, JsVal jsArgv)
   {
      if (!js_is_array(jsArgv))
      {
         return JsVal::null();
      }
      
      int argc = jsArgv["length"].as<int>();
      std::vector<std::string> buf(argc);
      std::vector<const char*> argv(argc);
      
      for (int i=0;i<argc;++i) 
      {
         auto v = jsArgv[i];
         if (v.isString()) 
         {
            buf[i] = v.as<std::string>();
         }
         else if (v.isNumber()) 
         {
            double d = v.as<double>();
            char tmp[64];
            if (std::isfinite(d) && std::floor(d)==d)
            {
               std::snprintf(tmp,sizeof(tmp), "%lld", (long long)d);
            }
            else
            {
               std::snprintf(tmp,sizeof(tmp), "%.17g", d);
            }
            buf[i] = tmp;
         } else buf[i] = "";
         argv[i] = buf[i].c_str();
      }
      
      ConsoleValue ret{};
      bool ok = mVm->callNamespaceFunction((VMNamespace*)nsPtr, StringTable->insert(name.c_str()), argc, argv.data(), ret);
      if (!ok) 
      {
         return JsVal::null();
      }
      return JSFromCV(mVm, ret);
   }
   
   void addNamespaceFunction(uintptr_t nsPtr,
                             const std::string& name,
                             const std::string& usage,
                             int minArgs,
                             int maxArgs,
                             const std::string& kind,
                             JsVal cb)
   {
      mVm->mInternal->printf(0, "addNamespaceFunction start ptr=%u", nsPtr);
      
      if (cb.isUndefined() || cb.isNull()) return;
      
      // Keep the context alive for the VM lifetime
      auto ctx = std::make_unique<NSFuncCtx>();
      ctx->vm = mVm;
      ctx->cb = cb;
      NSFuncCtx* raw = ctx.get();
      mNSFuncBindings.push_back(std::move(ctx));
      
      mVm->mInternal->printf(0, "Adding as %s uptr=%p", kind.c_str(), raw);
      
      if (kind == "string") 
      {
         mVm->addNamespaceFunction(
                                   (VMNamespace*)nsPtr, StringTable->insert(name.c_str()),
                                   &NS_StringThunk, raw, usage.c_str(), (S32)minArgs, (S32)maxArgs
                                   );
      } 
      else if (kind == "int") 
      {
         mVm->addNamespaceFunction(
                                   (VMNamespace*)nsPtr, StringTable->insert(name.c_str()),
                                   &NS_IntThunk, raw, usage.c_str(), (S32)minArgs, (S32)maxArgs
                                   );
      } 
      else if (kind == "float") 
      {
         mVm->addNamespaceFunction(
                                   (VMNamespace*)nsPtr, StringTable->insert(name.c_str()),
                                   &NS_FloatThunk, raw, usage.c_str(), (S32)minArgs, (S32)maxArgs
                                   );
      } 
      else if (kind == "bool") 
      {
         mVm->addNamespaceFunction(
                                   (VMNamespace*)nsPtr, StringTable->insert(name.c_str()),
                                   &NS_BoolThunk, raw, usage.c_str(), (S32)minArgs, (S32)maxArgs
                                   );
      } 
      else if (kind == "void") 
      {
         mVm->addNamespaceFunction(
                                   (VMNamespace*)nsPtr, StringTable->insert(name.c_str()),
                                   &NS_VoidThunk, raw, usage.c_str(), (S32)minArgs, (S32)maxArgs
                                   );
      }
   }
};


// Bindings

EMSCRIPTEN_BINDINGS(kork_mVmmodule) {
   using namespace emscripten;
   
   class_<VmJS>("Vm")
      .constructor<JsVal>()
      .function("evalCode", &VmJS::evalCode)
      .function("call",     &VmJS::call)
      .function("setGlobal",&VmJS::setGlobal)
      .function("getGlobal",&VmJS::getGlobal)
      .function("registerClass",&VmJS::registerClass)
      .function("registerType",  &VmJS::registerType)
      .function("findNamespace",        &VmJS::findNamespace)
      .function("getGlobalNamespace",   &VmJS::getGlobalNamespace)
      .function("setNamespaceUsage",    &VmJS::setNamespaceUsage)
      .function("activatePackage",      &VmJS::activatePackage)
      .function("deactivatePackage",    &VmJS::deactivatePackage)
      .function("linkNamespace",        &VmJS::linkNamespace)
      .function("unlinkNamespace",      &VmJS::unlinkNamespace)
      .function("linkNamespaceById",    &VmJS::linkNamespaceById)
      .function("unlinkNamespaceById",  &VmJS::unlinkNamespaceById)
      .function("isNamespaceFunction",  &VmJS::isNamespaceFunction)
      .function("callNamespace",        &VmJS::callNamespace)
      .function("addNamespaceFunction", &VmJS::addNamespaceFunction)
   ;
}

// Thunks need to go here


// Console Namespace Thunks
// Callback wrapper is passed through as userPtr, object wrapper is obj.
// See NSFuncCtx for the actual callback wrapper

static const char* NS_StringThunk(void* obj, void* userPtr, S32 argc, const char* argv[])
{
   auto* ctx = static_cast<NSFuncCtx*>(userPtr);
   if (!ctx || ctx->cb.isUndefined() || ctx->cb.isNull()) return "";
   
   VmJS* vmPeer = static_cast<VmJS*>(ctx->vm->getUserPtr());
   ObjBinding* vmObjRef = static_cast<ObjBinding*>(obj);
   
   JsVal r = ctx->cb(
                     vmObjRef ? vmObjRef->peer : JsVal::undefined(),
                     vmPeer,
                     make_js_argv(argc, argv)
                     );
   
   if (r.isUndefined() || r.isNull())
   {
      return "";
   }
   
   // Convert to string and place into VM return buffer so the pointer stays valid.
   std::string s = r.as<std::string>();
   const uint32_t len = static_cast<uint32_t>(s.size());
   ConsoleValue buf = ctx->vm->getStringReturnBuffer(len);
   char* dst = static_cast<char*>(buf.evaluatePtr(ctx->vm->getAllocBase()));
   if (!dst) 
   {
      return "";
   }
   std::memcpy(dst, s.c_str(), len + 1);
   return dst;
}

static S32 NS_IntThunk(void* obj, void* userPtr, S32 argc, const char* argv[])
{
   auto* ctx = static_cast<NSFuncCtx*>(userPtr);
   if (!ctx || ctx->cb.isUndefined() || ctx->cb.isNull())
   {
      return 0;
   }
   
   VmJS* vmPeer = static_cast<VmJS*>(ctx->vm->getUserPtr());
   ObjBinding* vmObjRef = static_cast<ObjBinding*>(obj);
   
   JsVal r = ctx->cb(
                     vmObjRef ? vmObjRef->peer : JsVal::undefined(),
                     vmPeer,
                     make_js_argv(argc, argv)
                     );
   
   return r.isNumber() ? (S32)r.as<int32_t>() : 0;
}

static F32 NS_FloatThunk(void* obj, void* userPtr, S32 argc, const char* argv[])
{
   auto* ctx = static_cast<NSFuncCtx*>(userPtr);
   if (!ctx || ctx->cb.isUndefined() || ctx->cb.isNull())
   {
      return 0.0f;
   }
   
   VmJS* vmPeer = static_cast<VmJS*>(ctx->vm->getUserPtr());
   ObjBinding* vmObjRef = static_cast<ObjBinding*>(obj);
   
   JsVal r = ctx->cb(
                     vmObjRef ? vmObjRef->peer : JsVal::undefined(),
                     vmPeer,
                     make_js_argv(argc, argv)
                     );
   
   return r.isNumber() ? (F32)r.as<double>() : 0.0f;
}

static void NS_VoidThunk(void* obj, void* userPtr, S32 argc, const char* argv[])
{
   auto* ctx = static_cast<NSFuncCtx*>(userPtr);
   if (!ctx || ctx->cb.isUndefined() || ctx->cb.isNull())
   {
      return;
   }
   
   VmJS* vmPeer = static_cast<VmJS*>(ctx->vm->getUserPtr());
   ObjBinding* vmObjRef = static_cast<ObjBinding*>(obj);
   
   ctx->cb(
           vmObjRef ? vmObjRef->peer : JsVal::undefined(),
           vmPeer,
           make_js_argv(argc, argv)
           );
}

static bool NS_BoolThunk(void* obj, void* userPtr, S32 argc, const char* argv[])
{
   auto* ctx = static_cast<NSFuncCtx*>(userPtr);
   if (!ctx || ctx->cb.isUndefined() || ctx->cb.isNull())
   {
      return false;
   }
   
   VmJS* vmPeer = static_cast<VmJS*>(ctx->vm->getUserPtr());
   ObjBinding* vmObjRef = static_cast<ObjBinding*>(obj);
   
   JsVal r = ctx->cb(
                     vmObjRef ? vmObjRef->peer : JsVal::undefined(),
                     vmPeer,
                     make_js_argv(argc, argv)
                     );
   
   if (r.isNumber()) return r.as<double>() != 0.0;
   if (r.isString()) return !r.as<std::string>().empty();
   if (r.isTrue())   return true;
   return false;
}

//
// iFind
//

static VMObject* FindByNameThunk(void* user, StringTableEntry name, VMObject* parent)
{
   auto* env = static_cast<JsEnv*>(user);
   if (!env || env->findByName.isUndefined() || env->findByName.isNull()) 
   {
      return nullptr;
   }
   JsVal ret = env->findByName(JsVal(name ? name : ""),
                               JsVal(reinterpret_cast<uintptr_t>(parent)));
   // JS must return a pointer (number) or 0/null
   uintptr_t p = 0;
   if (!ret.isNull() && !ret.isUndefined()) 
   {
      p = ret.as<uintptr_t>();
   }
   return reinterpret_cast<VMObject*>(p);
}

static VMObject* FindByPathThunk(void* user, const char* path)
{
   auto* env = static_cast<JsEnv*>(user);
   if (!env || env->findByPath.isUndefined() || env->findByPath.isNull()) 
   {
      return nullptr;
   }
   
   JsVal ret = env->findByPath(JsVal(path ? path : ""));
   uintptr_t p = 0;
   if (!ret.isNull() && !ret.isUndefined()) 
   {
      p = ret.as<uintptr_t>();
   }
   return reinterpret_cast<VMObject*>(p);
}

static VMObject* FindByInternalNameThunk(void* user, StringTableEntry internalName, bool recursive, VMObject* parent)
{
   auto* env = static_cast<JsEnv*>(user);
   if (!env || env->findByInternalName.isUndefined() || env->findByInternalName.isNull()) 
   {
      return nullptr;
   }
   
   JsVal ret = env->findByInternalName(JsVal(internalName ? internalName : ""),
                                       JsVal(recursive),
                                       JsVal(reinterpret_cast<uintptr_t>(parent)));
   uintptr_t p = 0;
   if (!ret.isNull() && !ret.isUndefined()) 
   {
      p = ret.as<uintptr_t>();
   }
   return reinterpret_cast<VMObject*>(p);
}

static VMObject* FindByIdThunk(void* user, SimObjectId objectId)
{
   auto* env = static_cast<JsEnv*>(user);
   if (!env || env->findById.isUndefined() || env->findById.isNull()) 
   {
      return nullptr;
   }
   
   JsVal ret = env->findById(JsVal(objectId));
   uintptr_t p = 0;
   if (!ret.isNull() && !ret.isUndefined()) 
   {
      p = ret.as<uintptr_t>();
   }
   return reinterpret_cast<VMObject*>(p);
}

//
// iCreate
//

static void Class_CreateThunk(void* user, Vm* vm, CreateClassReturn* outP)
{
   auto* kb = static_cast<ClassBinding*>(user);
   auto* ob = new ObjBinding();
   ob->klass = kb;

   if (!js_is_nullish(kb->cb_create))
   {
      ob->peer = kb->cb_create();
   }
   else
   {
      delete ob;
      ob = NULL;
   }

   outP->userPtr = ob;
   outP->initialFlags = 0;
}

static void Class_DestroyThunk(void* user, Vm* vm, void* createdPtr)
{
   auto* kb = static_cast<ClassBinding*>(user);
   auto* ob = static_cast<ObjBinding*>(createdPtr);
   if (ob && !js_is_nullish(kb->cb_destroy))
   {
      kb->cb_destroy(ob->peer);
   }
   delete ob;
}

static bool Class_ProcessArgsThunk(Vm* vm, void* createdPtr, const char* name, bool isDatablock, bool internalName, int argc, const char** argv)
{
   auto* ob = static_cast<ObjBinding*>(createdPtr);
   auto* kb = ob ? ob->klass : nullptr;
   if (!kb || js_is_nullish(kb->cb_processArgs)) 
   {
      return true; // default ok
   }
   std::vector<JsVal> jsArgv; jsArgv.reserve(argc);
   for (int i=0;i<argc;++i) jsArgv.emplace_back(JsVal(argv[i] ? std::string(argv[i]) : std::string()));
   JsVal jsArray = JsVal::array(jsArgv);
   return kb->cb_processArgs(JsVal((uintptr_t)vm), JsVal((uintptr_t)0),
                             JsVal(name ? name : ""), JsVal(isDatablock), JsVal(internalName), jsArray).as<bool>();
}

static bool Class_AddObjectThunk(Vm* vm, VMObject* object, bool placeAtRoot, U32 groupAddId)
{
   auto* ob = static_cast<ObjBinding*>(object ? object->userPtr : nullptr);
   auto* kb = ob ? ob->klass : nullptr;

   if (!kb || js_is_nullish(kb->cb_addObject)) 
   {
      return false;
   }
   
   bool ret = kb->cb_addObject(JsVal((uintptr_t)vm), JsVal((uintptr_t)object),
                           JsVal(placeAtRoot), JsVal(groupAddId)).as<bool>();

   if (ret)
   {
      kb->vmObject = object;
      vm->incVMRef(object);
   }

   return ret;
}


static void Class_RemoveObjectThunk(void* user, Vm* vm, VMObject* object)
{
   auto* ob = static_cast<ObjBinding*>(object ? object->userPtr : nullptr);
   auto* kb = ob ? ob->klass : nullptr;

   bool ret;

   if (kb && !js_is_nullish(kb->cb_removeObject)) 
   {
      kb->cb_removeObject(JsVal((uintptr_t)object)).as<bool>();
   }

   kb->vmObject = NULL;
   vm->decVMRef(object);
}

static SimObjectId Class_GetIdThunk(VMObject* object)
{
   ConsoleValue cv;
   auto* ob = static_cast<ObjBinding*>(object ? object->userPtr : nullptr);
   auto* kb = ob ? ob->klass : nullptr;
   if (!kb || js_is_nullish(kb->cb_getId)) 
   {
      return 0;
   }
   
   JsVal r = kb->cb_getId(ob->peer);
   return r.as<uint32_t>();
}

//
// iEnum
//

static U32 Enum_GetSizeThunk(VMObject* object)
{
   auto* ob = static_cast<ObjBinding*>(object ? object->userPtr : nullptr);
   auto* kb = ob ? ob->klass : nullptr;
   if (!kb || js_is_nullish(kb->cb_enum_getSize)) 
   {
      return 0;
   }
   
   return kb->cb_enum_getSize(ob->peer).as<U32>();
}

static VMObject* Enum_GetAtIndexThunk(VMObject* object, U32 index)
{
   auto* ob = static_cast<ObjBinding*>(object ? object->userPtr : nullptr);
   auto* kb = ob ? ob->klass : nullptr;
   if (!kb || js_is_nullish(kb->cb_enum_getAtIndex)) 
   {
      return nullptr;
   }
   
   uintptr_t p = kb->cb_enum_getAtIndex(ob->peer, JsVal(index)).as<uintptr_t>();
   return reinterpret_cast<VMObject*>(p);
}

//
// iCustomFields
//

static ConsoleValue CF_GetFieldByNameThunk(Vm* vm, VMObject* object, const char* name)
{
   ConsoleValue cv;
   auto* ob = static_cast<ObjBinding*>(object ? object->userPtr : nullptr);
   auto* kb = ob ? ob->klass : nullptr;
   if (!kb || js_is_nullish(kb->cb_cf_getByName))
   {
      cv.setString(nullptr, ConsoleValue::ZoneExternal);
      return cv;
   }
   
   JsVal r = kb->cb_cf_getByName(ob->peer, JsVal(name ? name : ""));
   CVFromJSReturn(vm, r, cv);
   return cv;
}

static void CF_SetFieldByNameThunk(Vm* vm, VMObject* object, const char* name, ConsoleValue value)
{
   auto* ob = static_cast<ObjBinding*>(object ? object->userPtr : nullptr);
   auto* kb = ob ? ob->klass : nullptr;
   if (!kb || js_is_nullish(kb->cb_cf_setByName)) return;
   
   // CV -> JS (number or string). Use VM for string conversion when needed.
   JsVal jsV;
   if (value.isInt()) 
   {
      jsV = JsVal((double)value.getInt());
   }
   else if (value.isFloat()) 
   {
      jsV = JsVal(value.getFloat());
   }
   else 
   {
      const char* s = kb->vm ? kb->vm->valueAsString(value) : "";
      jsV = JsVal(s ? std::string(s) : std::string());
   }
   kb->cb_cf_setByName(ob->peer, JsVal(name ? name : ""), jsV);
}

static void Type_SetValueThunk(
                               void* userPtr,
                               Vm* vm,
                               void* dptr,
                               S32 argc,
                               ConsoleValue* argv,
                               const EnumTable* tbl,
                               BitSet32 flag,
                               U32 typeId)
{
   auto* tb = static_cast<TypeBinding*>(userPtr);
   if (!tb || js_is_nullish(tb->cb_setValue))
   {
      return;
   }
   
   std::vector<JsVal> jsArgs;
   jsArgs.reserve(argc);
   
   for (S32 i=0;i<argc;++i)
   {
      const ConsoleValue& cv = argv[i];
      if (cv.isInt())
      {
         jsArgs.emplace_back(JsVal(static_cast<double>(vm->valueAsInt(cv))));
      }
      else if (cv.isFloat())
      {
         jsArgs.emplace_back(JsVal(vm->valueAsFloat(cv)));
      }
      else 
      {
         const char* s = vm->valueAsString(cv);
         jsArgs.emplace_back(JsVal(s ? std::string(s) : std::string()));
      }
   }
   
   JsVal jsArgv = JsVal::array(jsArgs);
   VmJS* vmPeer = static_cast<VmJS*>(vm->getUserPtr());
   
   tb->cb_setValue(
                   vmPeer,
                   JsVal((uintptr_t)dptr),
                   jsArgv,
                   JsVal((uintptr_t)tbl),
                   JsVal((uint32_t)flag),
                   JsVal(typeId)
                   );
}

static ConsoleValue Type_CopyValueThunk(
                                        void* userPtr,
                                        Vm* vm,
                                        void* sptr,
                                        const EnumTable* tbl,
                                        BitSet32 flag,
                                        U32 requestedType,
                                        U32 requestedZone)
{
   auto* tb = static_cast<TypeBinding*>(userPtr);
   if (!tb || js_is_nullish(tb->cb_copyValue))
   {
      ConsoleValue def; def.setString(nullptr, ConsoleValue::ZoneExternal);
      return def;
   }
   
   VmJS* vmPeer = static_cast<VmJS*>(vm->getUserPtr());
   
   JsVal r = tb->cb_copyValue(
                              vmPeer,
                              JsVal((uintptr_t)sptr),
                              JsVal((uintptr_t)tbl),
                              JsVal((uint32_t)flag),
                              JsVal(requestedType),
                              JsVal(requestedZone)
                              );
   ConsoleValue cv;
   CVFromJSReturn(vm, r, cv);
   return cv;
}

static const char* Type_GetTypeClassNameThunk(void* userPtr) {
   auto* tb = static_cast<TypeBinding*>(userPtr);
   if (!tb || js_is_nullish(tb->cb_getTypeName)) 
   {
      // fallback to the registered type name
      return StringTable->insert(tb ? tb->nameBuf.c_str() : "");
   }
   
   JsVal r = tb->cb_getTypeName();
   if (r.isString())
   {
      return StringTable->insert(r.as<std::string>().c_str());
   }
   
   return StringTable->insert(tb->nameBuf.c_str());
}

static const char* Type_PrepDataThunk(
                                      void* userPtr,
                                      Vm* vm,
                                      const char* data,
                                      char* buffer,
                                      U32 bufferLen)
{
   auto* tb = static_cast<TypeBinding*>(userPtr);
   if (!tb || js_is_nullish(tb->cb_prepData))
   {
      return data;
   }
   
   VmJS* vmPeer = static_cast<VmJS*>(vm->getUserPtr());
   
   JsVal r = tb->cb_prepData(
                             vmPeer,
                             JsVal(data ? std::string(data) : std::string()),
                             JsVal((uintptr_t)buffer),
                             JsVal(bufferLen)
                             );
   
   if (r.isNumber())
   {
      uintptr_t p = r.as<uintptr_t>();
      return reinterpret_cast<const char*>(p);
   }
   
   if (r.isString()) 
   {
      tb->prepScratch = r.as<std::string>();
      return tb->prepScratch.c_str();
   }
   
   return buffer ? buffer : data;
}


