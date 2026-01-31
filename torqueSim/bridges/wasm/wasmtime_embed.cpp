#include "console/console.h"
#include "console/consoleTypes.h"
#include "sim/simBase.h"
#include "core/fileStream.h"
#include "bridges/bridge_base.h"

#include <wasm.h>        // upstream wasm-c-api (engine/config/types)
#include <wasmtime.h>    // Wasmtime C API (store/linker/func/memory/…)
#include <vector>
#include <cstring>

class WasmTimeModuleObject : public BaseBridgeObject
{
   typedef BaseBridgeObject Parent;

   struct ScratchResolver
   {
      WasmTimeModuleObject* ptr;
      U8* getPtr(uintptr_t allocPos)
      {
         if (!ptr || !ptr->mStore || !ptr->mHasMemory)
         {
            return NULL;
         }
         
         wasmtime_context_t* ctx = wasmtime_store_context(ptr->mStore);
         uint8_t* base = wasmtime_memory_data(ctx, &ptr->mMemory);
         size_t   sz   = wasmtime_memory_data_size(ctx, &ptr->mMemory);
         return allocPos < sz ? base + allocPos : NULL;
      }
   };

public:
   DECLARE_CONOBJECT(WasmTimeModuleObject);

   // Wasmtime state
   wasm_engine_t*        mEngine;
   wasmtime_store_t*     mStore;
   wasmtime_linker_t*    mLinker;
   wasmtime_module_t*    mModule;
   wasmtime_instance_t   mInstance;
   wasmtime_memory_t     mMemory;
   
   bool                  mHasInstance;
   bool                  mHasMemory;

   // Exported funcs (TS -> WASM)
   wasmtime_func_t       mFuncs[MAX_FUNCS];
   FuncUserInfo<WasmTimeModuleObject> mInfos[MAX_FUNCS];

   // Host funcs (WASM -> TS)
   FuncUserInfo<WasmTimeModuleObject> mHostInfos[MAX_FUNCS];

   static void initPersistFields()
   {
      Parent::initPersistFields();
   }

   WasmTimeModuleObject()
   {
      mEngine      = NULL;
      mStore       = NULL;
      mLinker      = NULL;
      mModule      = NULL;
      mInstance = {};
      mMemory = {};
      mHasInstance = false;
      mHasMemory   = false;
      memset(mFuncs,     0, sizeof(mFuncs));
      memset(mInfos,     0, sizeof(mInfos));
      memset(mHostInfos, 0, sizeof(mHostInfos));
   }

   void initScratch() override
   {
      if (mScratchOffset != 0 || !mHasInstance || !funcIsValid(mFuncs[0]))
      {
         return;
      }

      wasmtime_context_t* ctx = wasmtime_store_context(mStore);
      wasmtime_val_t args[1];
      wasmtime_val_t results[1] = {};
      args[0].kind = WASMTIME_I32; args[0].of.i32 = (int32_t)mScratchSize;
      
      wasm_trap_t* trap = NULL;
      wasmtime_error_t* err = wasmtime_func_call(ctx, &mFuncs[0], args, 1, results, 1, &trap);
      if (trap)
      {
         wasm_trap_delete(trap);
         return;
      }
      if (err)
      {
         wasmtime_error_delete(err);
         return;
      }
      if (results[0].kind != WASMTIME_I32)
      {
         return;
      }
      
      mScratchOffset   = (U32)results[0].of.i32;
      mScratchAllocPtr = 0;
   }

   bool initRuntime() override
   {
      cleanup();

      wasm_config_t* cfg = wasm_config_new();
      if (!cfg)
      {
         return false;
      }

      mEngine = wasm_engine_new_with_config(cfg);
      if (!mEngine)
      {
         return false;
      }
      
      mStore  = wasmtime_store_new(mEngine, NULL, NULL);
      if (!mStore)
      {
         return false;
      }
      
      mLinker = wasmtime_linker_new(mEngine);
      return mLinker != NULL;
   }

   void cleanup() override
   {
      if (mModule)
      {
         wasmtime_module_delete(mModule);
         mModule = NULL;
      }
      if (mLinker)
      {
         wasmtime_linker_delete(mLinker);
         mLinker = NULL;
      }
      if (mStore)
      {
         wasmtime_store_delete(mStore);
         mStore = NULL;
      }
      if (mEngine)
      {
         wasm_engine_delete(mEngine);
         mEngine = NULL;
      }

      mInstance = {};
      mMemory = {};
      mHasInstance = false;
      mHasMemory   = false;
      memset(mFuncs,     0, sizeof(mFuncs));
      memset(mInfos,     0, sizeof(mInfos));
      memset(mHostInfos, 0, sizeof(mHostInfos));
   }

   bool load(Stream& s) override
   {
      U32 sz = s.getStreamSize();
      std::vector<uint8_t> bytes;
      bytes.resize(sz);
      s.read(sz, &bytes[0]);

      wasmtime_error_t* err = wasmtime_module_new(mEngine, &bytes[0], bytes.size(), &mModule);
      if (err)
      {
         wasm_message_t msg;
         wasmtime_error_message(err, &msg);
         Con::warnf("Wasmtime compile failed: %.*s", (int)msg.size, msg.data);
         wasm_byte_vec_delete(&msg);
         wasmtime_error_delete(err);
         return false;
      }
      return true;
   }

   bool linkFuncs() override
   {
      if (!mModule || !mStore || !mLinker)
      {
         return false;
      }
      
      // 1) Define host funcs (env.*)
      for (U32 i = 0; i < MAX_FUNCS; i++)
      {
         if (mHostFuncs[i] && mHostFuncs[i] != StringTable->EmptyString &&
             mHostFuncSignatures[i] && mHostFuncSignatures[i] != StringTable->EmptyString &&
             isSigValid(mHostFuncSignatures[i]))
         {

            wasm_functype_t* fty = buildFuncTypeFromSig(mHostFuncSignatures[i]);

            auto* info   = &mHostInfos[i];
            info->module = this;
            info->funcIdx= i;

            // Create host func in the linker
            wasmtime_error_t* err = wasmtime_linker_define_func(
              mLinker,
              "env", strlen("env"),
              mHostFuncs[i], strlen(mHostFuncs[i]),
              fty,
              &hostThunkBridge, // wasmtime_func_callback_t
              info,             // env
              NULL              // finalizer
            );
            wasm_functype_delete(fty);

            if (err)
            {
               wasm_message_t msg;
               wasmtime_error_message(err, &msg);
               Con::warnf("Host bind failed %s: %.*s", mHostFuncs[i], (int)msg.size, msg.data);
               wasm_byte_vec_delete(&msg);
               wasmtime_error_delete(err);
            }
         }
      }

      // 2) Instantiate via linker
      wasmtime_context_t* ctx = wasmtime_store_context(mStore);
      wasm_trap_t* trap = NULL;
      wasmtime_error_t* err = wasmtime_linker_instantiate(mLinker, ctx, mModule, &mInstance, &trap);
      if (trap)
      {
         wasm_message_t msg;
         wasm_trap_message(trap, &msg);
         Con::warnf("Instantiate trap: %.*s", (int)msg.size, msg.data);
         wasm_byte_vec_delete(&msg);
         wasm_trap_delete(trap);
         return false;
      }
      if (err)
      {
         wasm_message_t msg;
         wasmtime_error_message(err, &msg);
         Con::warnf("Instantiate error: %.*s", (int)msg.size, msg.data);
         wasm_byte_vec_delete(&msg);
         wasmtime_error_delete(err);
         return false;
      }
      mHasInstance = true;

      mHasMemory = false;
      {
         wasmtime_extern_t ext;
         if (wasmtime_instance_export_get(ctx, &mInstance, "memory", strlen("memory"), &ext)
             && ext.kind == WASMTIME_EXTERN_MEMORY)
         {
            mMemory = ext.of.memory; mHasMemory = true;
         }
         else
         {
            // scan all exports
            size_t i = 0;
            while (true)
            {
               char* name = NULL;
               size_t name_len = 0;
               bool ok = wasmtime_instance_export_nth(ctx, &mInstance, i++, &name, &name_len, &ext);
               if (!ok)
               {
                  break;
               }
               
               if (ext.kind == WASMTIME_EXTERN_MEMORY)
               {
                  mMemory = ext.of.memory;
                  mHasMemory = true;
                  break;
               }
            }
         }
      }

      // 4) Bind guest funcs into VM namespace
      KorkApi::Vm* theVm = getVM();
      KorkApi::NamespaceId nsId = getNamespace();

      for (U32 i = 0; i < MAX_FUNCS; i++)
      {
         if (mFuncNames[i] && mFuncNames[i] != StringTable->EmptyString &&
             mFuncSignatures[i] && mFuncSignatures[i] != StringTable->EmptyString &&
             isSigValid(mFuncSignatures[i]))
         {

            wasmtime_extern_t ext;
            bool ok = wasmtime_instance_export_get(ctx, &mInstance, mFuncNames[i], strlen(mFuncNames[i]), &ext);
            if (!ok || ext.kind != WASMTIME_EXTERN_FUNC)
            {
               Con::warnf("Can't find function %s %s", mFuncNames[i], mFuncSignatures[i]);
               continue;
            }

            mFuncs[i] = ext.of.func;

            auto* info = &mInfos[i];
            info->module  = this;
            info->funcIdx = i;

            U32 paramCount = getSigParamCount(mFuncSignatures[i]);
            theVm->addNamespaceFunction(nsId, mFuncNames[i],
               (KorkApi::ValueFuncCallback)thunkCall, info,
               mFuncSignatures[i], paramCount + 2, paramCount + 2);
         }
      }

      return true;
   }

   // ---------------- TS -> WASM ----------------
   static KorkApi::ConsoleValue thunkCall(WasmTimeModuleObject* obj, void* userPtr, int argc, KorkApi::ConsoleValue* argv)
   {
      auto* userInfo = (FuncUserInfo<WasmTimeModuleObject>*)userPtr;
      WasmTimeModuleObject* userModule = userInfo->module;
      KorkApi::Vm* vm = userModule->getVM();
      StringTableEntry sig   = userModule->mFuncSignatures[userInfo->funcIdx];
      StringTableEntry fname = userModule->mFuncNames[userInfo->funcIdx];
      wasmtime_func_t* func  = &userModule->mFuncs[userInfo->funcIdx];

      if (!sig || !fname || !funcIsValid(*func))
      {
         return KorkApi::ConsoleValue::makeString("bad_sig_or_name");
      }
      
      const char* s = sig;
      char retCh = 'v';
      if (*s && *s != '(')
      {
         retCh = *s;
      }
      while (*s && *s != '(')
      {
         ++s;
      }
      if (*s == '(')
      {
         ++s;
      }

      userModule->resetScratch();

      argc -= 2; argv += 2;
      if (argc < 0 || argc > 16)
      {
         return KorkApi::ConsoleValue::makeString("bad_argc");
      }
      
      wasmtime_val_t args[16] = {};
      wasmtime_val_t results[1] = {};

      ScratchResolver scratch; scratch.ptr = userModule;

      for (int i = 0; i < argc; ++i)
      {
         char t = s[i];
         if (t == 's')
         {
            const char* src = vm->valueAsString(argv[i]);
            U32 len = (U32)strlen(src);
            ScratchAlloc alloc = userModule->allocScratch(scratch, len + 1);
            if (!alloc.ptr)
            {
               return KorkApi::ConsoleValue::makeString("scratch_oom");
            }
            memcpy(alloc.ptr, src, len + 1);
            args[i].kind = WASMTIME_I32;
            args[i].of.i32 = (int32_t)alloc.vmOffset;
         }
         else if (t == 'i') { args[i].kind = WASMTIME_I32; args[i].of.i32 = (int32_t)vm->valueAsFloat(argv[i]); }
         else if (t == 'I') { args[i].kind = WASMTIME_I64; args[i].of.i64 = (int64_t)vm->valueAsFloat(argv[i]); }
         else if (t == 'f') { args[i].kind = WASMTIME_F32; args[i].of.f32 = (float)  vm->valueAsFloat(argv[i]); }
         else if (t == 'F') { args[i].kind = WASMTIME_F64; args[i].of.f64 = (double) vm->valueAsFloat(argv[i]); }
         else               { args[i].kind = WASMTIME_I32; args[i].of.i32 = 0; }
      }

      wasmtime_context_t* ctx = wasmtime_store_context(userModule->mStore);
      wasm_trap_t* trap = NULL;
      wasmtime_error_t* err = wasmtime_func_call(ctx, func, args, (size_t)argc,
                                                 results, (retCh=='v'?0:1), &trap);
      if (trap)
      {
         wasm_message_t msg;
         wasm_trap_message(trap, &msg);
         auto v = KorkApi::ConsoleValue::makeString(StringTable->insert((const char*)msg.data));
         wasm_byte_vec_delete(&msg);
         wasm_trap_delete(trap);
         return v;
      }
      if (err)
      {
         wasm_message_t msg;
         wasmtime_error_message(err, &msg);
         auto v = KorkApi::ConsoleValue::makeString(StringTable->insert((const char*)msg.data));
         wasm_byte_vec_delete(&msg);
         wasmtime_error_delete(err);
         return v;
      }

      // return conversion
      if (retCh == 'v')
      {
         return KorkApi::ConsoleValue::makeString(NULL);
      }
      
      if (retCh == 's')
      {
         if (!userModule->mHasMemory || results[0].kind != WASMTIME_I32)
         {
            return KorkApi::ConsoleValue::makeString(NULL);
         }
         
         int32_t off = results[0].of.i32;
         uint8_t* base = wasmtime_memory_data(ctx, &userModule->mMemory);
         size_t   sz   = wasmtime_memory_data_size(ctx, &userModule->mMemory);
         return KorkApi::ConsoleValue::makeString(((size_t)off >= sz) ? NULL : (const char*)(base + off));
      }
      if (retCh == 'i') return KorkApi::ConsoleValue::makeNumber((S32)(results[0].kind==WASMTIME_I32?results[0].of.i32:0));
      if (retCh == 'I') return KorkApi::ConsoleValue::makeNumber((S64)(results[0].kind==WASMTIME_I64?results[0].of.i64:0));
      if (retCh == 'f') return KorkApi::ConsoleValue::makeNumber((F32)(results[0].kind==WASMTIME_F32?results[0].of.f32:0));
      if (retCh == 'F') return KorkApi::ConsoleValue::makeNumber((F64)(results[0].kind==WASMTIME_F64?results[0].of.f64:0));
      return KorkApi::ConsoleValue::makeString(NULL);
   }

   // WASM -> TS (host import)
   static wasm_trap_t* hostThunkBridge(void* env,
                                       wasmtime_caller_t* caller,
                                       const wasmtime_val_t* args, size_t nargs,
                                       wasmtime_val_t* results, size_t nresults)
   {
      const auto* userInfo = (const FuncUserInfo<WasmTimeModuleObject>*)env;
      WasmTimeModuleObject* userModule = userInfo->module;
      KorkApi::Vm* vm = userModule->getVM();
      StringTableEntry sig = userModule->mHostFuncSignatures[userInfo->funcIdx];

      while (*sig && *sig != '(')
      {
         ++sig;
      }
      if (*sig == '(')
      {
         ++sig;
      }

      KorkApi::ConsoleValue argv_local[16];
      KorkApi::ConsoleValue* thunk_argv = &argv_local[2];
      KorkApi::ConsoleValue bufspaceV = vm->getStringFuncBuffer(1024);
      char* bufspace = (char*)bufspaceV.evaluatePtr(vm->getAllocBase());
      size_t ofs = 0, cap = 1024 - 1;

      argv_local[0] = KorkApi::ConsoleValue();
      argv_local[1] = KorkApi::ConsoleValue();

      // memory for string offsets
      wasmtime_context_t* ctx = wasmtime_caller_context(caller);
      uint8_t* memBase = NULL; size_t memSz = 0;
      if (userModule->mHasMemory)
      {
         memBase = wasmtime_memory_data(ctx, &userModule->mMemory);
         memSz   = wasmtime_memory_data_size(ctx, &userModule->mMemory);
      }

      for (size_t i = 0; i < nargs; ++i)
      {
         char t = sig[i];
         const wasmtime_val_t& v = args[i];
         if (t == 's' && v.kind == WASMTIME_I32 && memBase)
         {
            U32 off = (U32)v.of.i32;
            const char* s = (off < memSz) ? (const char*)(memBase + off) : "";
            char* dst = &bufspace[ofs];
            ofs += (size_t)snprintf(dst, cap - ofs, "%s", s);
            bufspace[ofs++] = '\0';
            thunk_argv[i] = KorkApi::ConsoleValue::makeString(dst);
         }
         else if (v.kind == WASMTIME_I32) { thunk_argv[i] = KorkApi::ConsoleValue::makeNumber((S32)v.of.i32); }
         else if (v.kind == WASMTIME_I64) { thunk_argv[i] = KorkApi::ConsoleValue::makeNumber((S64)v.of.i64); }
         else if (v.kind == WASMTIME_F32) { thunk_argv[i] = KorkApi::ConsoleValue::makeNumber((F32)v.of.f32); }
         else if (v.kind == WASMTIME_F64) { thunk_argv[i] = KorkApi::ConsoleValue::makeNumber((F64)v.of.f64); }
         else { thunk_argv[i] = KorkApi::ConsoleValue::makeNumber(0); }
      }

      KorkApi::ConsoleValue retV;
      vm->callObjectFunction(userModule->getVMObject(),
                             userModule->mHostFuncs[userInfo->funcIdx],
                             (U32)nargs + 2, &argv_local[0], retV);

      if (nresults == 0)
      {
         return NULL;
      }
      
      char retCh = userModule->mHostFuncSignatures[userInfo->funcIdx][0];
      if (retCh == 's' && userModule->mHasMemory)
      {
         userModule->resetScratch();
         const char* out = vm->valueAsString(retV);
         U32 len = (U32)strlen(out);
         ScratchResolver scratch{userModule};
         ScratchAlloc alloc = userModule->allocScratch(scratch, len);
         if (alloc.ptr)
         {
            memcpy(alloc.ptr, out, len);
            alloc.ptr[len] = '\0';
            results[0].kind = WASMTIME_I32;
            results[0].of.i32 = (int32_t)alloc.vmOffset;
         }
         else
         {
            results[0].kind = WASMTIME_I32;
            results[0].of.i32 = 0;
         }
         return NULL;
      }

      if (retCh == 'i') { results[0].kind = WASMTIME_I32; results[0].of.i32 = (int32_t)vm->valueAsInt(retV);  return NULL; }
      if (retCh == 'I') { results[0].kind = WASMTIME_I64; results[0].of.i64 = (int64_t)vm->valueAsInt(retV);  return NULL; }
      if (retCh == 'f') { results[0].kind = WASMTIME_F32; results[0].of.f32 = (float)  vm->valueAsFloat(retV);return NULL; }
      if (retCh == 'F') { results[0].kind = WASMTIME_F64; results[0].of.f64 = (double) vm->valueAsFloat(retV);return NULL; }
      return NULL; // void
   }

private:
   static bool funcIsValid(const wasmtime_func_t& f)
   {
      static wasmtime_func_t empty = {};
      return memcmp(&f, &empty, sizeof(wasmtime_func_t)) != 0;
   }

   static wasm_valtype_t* mapCh(char c)
   {
      switch (c)
      {
        case 'i': return wasm_valtype_new_i32();
        case 'I': return wasm_valtype_new_i64();
        case 'f': return wasm_valtype_new_f32();
        case 'F': return wasm_valtype_new_f64();
        case 's': return wasm_valtype_new_i32();
        default:  return wasm_valtype_new_i32();
      }
   }

   static wasm_functype_t* buildFuncTypeFromSig(const char* sig)
   {
      // "<ret>(params...)"
      const char* p = sig;
      char ret = 'v';
      if (*p && *p != '(')
      {
         ret = *p;
         while (*p && *p != '(')
         {
            ++p;
         }
      }
      if (*p == '(')
      {
         ++p;
      }

      std::vector<wasm_valtype_t*> params;
      while (*p && *p != ')')
      {
         params.push_back(mapCh(*p++));
      }
      
      wasm_valtype_vec_t pv, rv;
      wasm_valtype_vec_new_uninitialized(&pv, params.size());
      for (U32 i = 0; i < params.size(); ++i)
      {
         pv.data[i] = params[i];
      }

      if (ret == 'v')
      {
         wasm_valtype_vec_new_empty(&rv);
      }
      else
      {
         wasm_valtype_vec_new_uninitialized(&rv, 1);
         rv.data[0] = mapCh(ret);
      }

      return wasm_functype_new(&pv, &rv);
   }
};

IMPLEMENT_CONOBJECT(WasmTimeModuleObject);
