//-----------------------------------------------------------------------------
// Copyright (c) 2025-2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

#include "console/console.h"
#include "console/consoleTypes.h"
#include "sim/simBase.h"
#include "core/fileStream.h"
#include "bridges/bridge_base.h"

#include "wasm3.h"

/*

KorkScript WASM module binder.

Module exports get exposed as namespace functions.
Host exports are namespace functions which can be imported by wasm modules.

Example usage:

new Wasm3ModuleObject(MyModule)
{
	// Funcs in wasm
	funcName[0] = "add";
	funcSig[0] = "i(ii)";
	funcName[0] = "sub";
	funcSig[1] = "i(ii)";

	// Host funcs
	hostFuncName[0] = "print";
	hostFuncSig[1] = "v(s)";

	moduleFile = "test.wasm";
};

function MyModule::print(%this, %msg)
{
	echo("Module Print: " @ %msg);
}

echo(MyModule.add(1,2));


*/
class Wasm3ModuleObject : public BaseBridgeObject
{
typedef BaseBridgeObject Parent;

	struct ScratchResolver
	{
		Wasm3ModuleObject* ptr;

      U8* getPtr(uintptr_t allocPos)
		{
			U32 memSize = 0;
			U8* ret = (U8*)(m3_GetMemory(ptr->mRuntime, &memSize, 0));
			return allocPos < memSize ? ret + allocPos : nullptr;
		}
	};

public:
	DECLARE_CONOBJECT(Wasm3ModuleObject);

	IM3Runtime mRuntime;
	IM3Environment mEnv;
	IM3Module mModule;

	// These get called inside WASM
	IM3Function mFuncs[MAX_FUNCS];
	FuncUserInfo<Wasm3ModuleObject> mInfos[MAX_FUNCS];

	// These get called from WASM thunk
	FuncUserInfo<Wasm3ModuleObject> mHostInfos[MAX_FUNCS];

	static void initPersistFields()
	{
		Parent::initPersistFields();
	}

	Wasm3ModuleObject()
	{
		mRuntime = nullptr;
		mEnv = nullptr;
		mModule = nullptr;
		memset(mFuncs, '\0', sizeof(mFuncs));
		memset(mInfos, '\0', sizeof(mInfos));
		memset(mHostInfos, '\0', sizeof(mHostInfos));
	}

	void initScratch() override
	{
		// Make sure scratch space is allocated
		if (mScratchOffset == 0)
		{
			if (mFuncs[0])
			{
				// Call allocator
            void* wasmArgv[1];
            U64 wasmArgData[1] = {};
            
            wasmArgv[0] = &wasmArgData[0];
            *((U32*)wasmArgData) = mScratchSize;

            // Call the wasm function
            M3Result r = m3_Call(mFuncs[0], 1, (const void**)wasmArgv);
            if (r)
            {
               return;
            }
            
            if (m3_GetRetCount(mFuncs[0]) == 0)
            {
               return;
            }

            void* retPtr[1] = {};
            retPtr[0] = &wasmArgData[0];
            r = m3_GetResults(mFuncs[0], 1, (const void**)&retPtr);
            
            if (r)
            {
               return;
            }
            
            mScratchOffset = *((U32*)&wasmArgData[0]);
				mScratchAllocPtr = 0;
			}
		}
	}

	bool initRuntime() override
   {
      mEnv = m3_NewEnvironment();
      mRuntime = m3_NewRuntime(mEnv, mMemSize, nullptr);
      return mRuntime != nullptr;
	}

	void cleanup() override
	{
		if (!mRuntime)
		{
			return;
		}

		m3_FreeRuntime(mRuntime);
		m3_FreeEnvironment(mEnv);
		mRuntime = nullptr;
		mEnv = nullptr;
	}

	bool load(Stream& s) override
	{
		U32 sz = s.getStreamSize();
		U8* bytes = new U8[sz];
      s.read(sz, bytes);
      
      M3Result res = m3_ParseModule(mEnv, &mModule, bytes, sz);
      
      if (res == nullptr)
      {
         res = m3_LoadModule(mRuntime, mModule);
      }
      
      return res == nullptr;
	}

   bool linkFuncs() override
	{
		KorkApi::Vm* theVm = getVM();
		KorkApi::NamespaceId nsId = getNamespace();

		// See if we have host funcs; register these with the VM (needs to be done FIRST)

		for (U32 i=0; i<MAX_FUNCS; i++)
		{
			if (mHostFuncs[i] != nullptr && mHostFuncs[i] != StringTable->EmptyString && 
				mHostFuncSignatures[i] != nullptr && mHostFuncSignatures[i] != StringTable->EmptyString &&
				isSigValid(mHostFuncSignatures[i]))
			{
				char realSig[32];
				auto* hostInfo = &mHostInfos[i];
				strcpy(realSig, mHostFuncSignatures[i]);
				convertSig(realSig);

				hostInfo->module = this;
				hostInfo->funcIdx = i;

				M3Result res = m3_LinkRawFunctionEx(mModule, 
					                                "env", 
					                                mHostFuncs[i], 
					                                realSig, 
					                                thunkHostCall, 
					                                hostInfo);
            
            if (res != nullptr)
            {
               Con::warnf("Function %s %s not bound (%s)", mHostFuncs[i], realSig, res);
            }
			}
		}
      
      // Bind all funcs in nsId

      for (U32 i=0; i<MAX_FUNCS; i++)
      {
         if (mFuncNames[i] != nullptr && mFuncNames[i] != StringTable->EmptyString &&
            mFuncSignatures[i] != nullptr && mFuncSignatures[i] != StringTable->EmptyString &&
            isSigValid(mFuncSignatures[i]))
         {
            auto* info = &mInfos[i];
            info->module = this;
            info->funcIdx = i;
            U32 paramCount = getSigParamCount(mFuncSignatures[i]);
            
            M3Result result = m3_FindFunction(&mFuncs[i], mRuntime, mFuncNames[i]);
            if (result != nullptr)
            {
               Con::warnf("Can't find function %s %s (%s)", mFuncNames[i], mFuncSignatures[i], result);
            }
            else
            {
               theVm->addNamespaceFunction(nsId, mFuncNames[i], (KorkApi::ValueFuncCallback)thunkCall, info, mFuncSignatures[i], paramCount+2, paramCount+2);
            }
         }
      }

      return true;
	}

	// TS -> WASM Thunk
	static KorkApi::ConsoleValue thunkCall(Wasm3ModuleObject* obj, void* userPtr, int argc, KorkApi::ConsoleValue* argv)
	{
	    FuncUserInfo<Wasm3ModuleObject>* userInfo = (FuncUserInfo<Wasm3ModuleObject>*)userPtr;
	    Wasm3ModuleObject* userModule = userInfo->module;
	    KorkApi::Vm* vm = userModule->getVM();
	    StringTableEntry sig = userModule->mFuncSignatures[userInfo->funcIdx];
	    StringTableEntry fname = userModule->mFuncNames[userInfo->funcIdx];
	    IM3Function func = userModule->mFuncs[userInfo->funcIdx];

	    if (!sig || !fname || !func)
	    {
	        return KorkApi::ConsoleValue::makeString("bad_sig_or_name");
	    }

	    // Parse signature: <ret>(<params>)
	    const char* s = sig;
	    char retCh = 'v';
	    if (*s && *s != '(') { retCh = *s; }
	    while (*s && *s != '(') ++s;
	    if (*s == '(') ++s;

	    // Prepare scratch buffer for strings
	    userModule->resetScratch();

	    void* wasmArgv[16];
	    U64 wasmArgData[16];

	    U32 paramIdx = 0;
	    argc -= 2;
	    argv += 2;

	    if (argc < 0 || argc != m3_GetArgCount(func) || argc > 16)
	    {
	       KorkApi::ConsoleValue::makeString("bad_argc");
	    }

	    ScratchResolver scratch;

	    for (U32 i=0; i<argc; i++)
	    {
	        char t = s[i];
	        wasmArgv[i] = &wasmArgData[i];
	        void* data = (void*)&wasmArgData[i];

	        if (t == 's')
	        {
	            // copy argv[paramIdx] into module memory, get offset, pass as decimal string
	            const char* src = vm->valueAsString(argv[i]);
	            U32 len = (U32)strlen(src);

	            ScratchAlloc alloc = userModule->allocScratch(scratch, len + 1);
	            if (!alloc.ptr)
            	{
            		return KorkApi::ConsoleValue::makeString("scratch_oom");
            	}

	            memcpy(alloc.ptr, src, len);
	            alloc.ptr[len] = '\0';
	            ((U32*)wasmArgData)[i] = alloc.vmOffset;
	        }
	        else if (t == 'i')
	        {
              *(S32*)(&wasmArgData[i]) = vm->valueAsFloat(argv[i]);
	        }
           else if (t == 'I')
           {
              *(S64*)(&wasmArgData[i]) = vm->valueAsFloat(argv[i]);
           }
	        else if (t == 'f')
	        {
              *(F32*)(&wasmArgData[i]) = vm->valueAsFloat(argv[i]);
	        }
	        else if (t == 'F')
	        {
              *(F64*)(&wasmArgData[i]) = vm->valueAsFloat(argv[i]);
	        }
	        else
	        {
	            ((U32*)wasmArgData)[i] = 0;
	        }
	    }

	    // Call the wasm function
	    M3Result r = m3_Call(func, argc, (const void**)wasmArgv);
	    if (r) 
	    {
	    	return KorkApi::ConsoleValue::makeString(r);
	    }

	    // Prepare a buffer for returning a string to TorqueScript
	    KorkApi::ConsoleValue outBufV = vm->getStringFuncBuffer(1024);
	    char* outBuf = (char*)outBufV.evaluatePtr(vm->getAllocBase());
	    if (!outBuf) 
	    {
	       return KorkApi::ConsoleValue::makeString("no_vm_buffer");
	    }

	    // No return?
	    if (retCh == 'v' || m3_GetRetCount(func) == 0)
	    {
	        outBuf[0] = '\0';
	        return KorkApi::ConsoleValue::makeString(nullptr);
	    }

        void* retPtr[1] = {};
      
	    if (retCh == 's') // string
	    {
           uint32_t off = 0;
           retPtr[0] = &off;
           r = m3_GetResults(func, 1, (const void**)&retPtr);
           if (r)
           {
              return KorkApi::ConsoleValue::makeString(r);
           }
          
	        U32 memSize = 0;
	        U8* mem = (U8*)m3_GetMemory(userModule->mRuntime, &memSize, 0);
	        if (!mem || off >= memSize)
	        {
	            outBuf[0] = '\0';
		         return KorkApi::ConsoleValue::makeString(nullptr);
	        }

	        const char* strInMem = (const char*)(mem + off);
	        return KorkApi::ConsoleValue::makeString(strInMem);
	    }
	    else if (retCh == 'i')
	    {
          S32 value = 0;
          retPtr[0] = &value;
          r = m3_GetResults(func, 1, (const void**)&retPtr);
          if (r)
          {
              return KorkApi::ConsoleValue::makeString(r);
          }

          return KorkApi::ConsoleValue::makeNumber(value);
	    }
       else if (retCh == 'I')
       {
          S64 value = 0;
          retPtr[0] = &value;
          r = m3_GetResults(func, 1, (const void**)&retPtr);
          if (r)
          {
              return KorkApi::ConsoleValue::makeString(r);
          }

          return KorkApi::ConsoleValue::makeNumber(value);
       }
	    else if (retCh == 'f')
	    {
          F32 value = 0;
          retPtr[0] = &value;
          r = m3_GetResults(func, 1, (const void**)&retPtr);
          if (r)
          {
              return KorkApi::ConsoleValue::makeString(r);
          }

          return KorkApi::ConsoleValue::makeNumber(value);
	    }
	    else if (retCh == 'F')
	    {
          F64 value = 0;
          retPtr[0] = &value;
          r = m3_GetResults(func, 1, (const void**)&retPtr);
          if (r)
          {
              return KorkApi::ConsoleValue::makeString(r);
          }

          return KorkApi::ConsoleValue::makeNumber(value);
	    }
	    else
	    {
	        return KorkApi::ConsoleValue::makeString(nullptr);
	    }
	}

	// WASM -> TS Thunk
	// Converts input to const char* 
	static const void* thunkHostCall(IM3Runtime rt,
                               IM3ImportContext ctx,
                               uint64_t* sp,    // value stack (args in 64-bit slots)
                               void* mem)
	{
	    const  FuncUserInfo<Wasm3ModuleObject>* userInfo = (const  FuncUserInfo<Wasm3ModuleObject>*)ctx->userdata;
	    Wasm3ModuleObject* userModule = userInfo->module;
	    StringTableEntry sig = userModule->mHostFuncSignatures[userInfo->funcIdx];
	    KorkApi::Vm* vm = userModule->getVM();

	    bool returnsString = *sig == 's';
	    bool returnsValue = *sig != 'v';
	    ScratchResolver scratch;

	    while (*sig != '\0' && *sig != '(')
	    	sig++;
	    if (*sig == '(')
	    	sig++;

	    // discover which import this is and its type
	    uint32_t    argc      = m3_GetArgCount(ctx->function);

	    KorkApi::ConsoleValue argv_local[16];
       KorkApi::ConsoleValue* thunk_argv = &argv_local[2];
	    KorkApi::ConsoleValue bufspaceV = vm->getStringFuncBuffer(1024);
	    char* bufspace = (char*)bufspaceV.evaluatePtr(vm->getAllocBase());
	    size_t ofs = 0;
	    const size_t capacity = 1024-1;
      
       argv_local[0] = KorkApi::ConsoleValue();
       argv_local[1] = KorkApi::ConsoleValue();

	    for (uint32_t i = 0; i < argc; ++i) 
	    {
	        M3ValueType t = m3_GetArgType(ctx->function, i);
	        bool isStr = sig[i] == 's';
	        const char* s = "";
           char* bufStart = &bufspace[ofs];

	        if (isStr && t == c_m3Type_i32)
	        {
	        	// Resolve string pointer
	        	U32 memSize = 0;
				U8* mem = ((U8*) m3_GetMemory(userModule->mRuntime, &memSize, 0));
				U32 offset = (U32)sp[i];
				if (offset < memSize)
				{
					s = (const char*)mem + offset;
				}

				ofs += snprintf(bufStart, capacity-ofs, "%s",  s);
				bufspace[ofs++] = '\0';
				thunk_argv[i] = KorkApi::ConsoleValue::makeString(bufStart);
	        }
	        else if (t == c_m3Type_i32)  { int32_t v = (int32_t) sp[i];                  thunk_argv[i] = KorkApi::ConsoleValue::makeNumber(v); }
	        else if (t == c_m3Type_i64) { int64_t v = (int64_t) sp[i];                   thunk_argv[i] = KorkApi::ConsoleValue::makeNumber(v); }
	        else if (t == c_m3Type_f32) { float    v = *(float*) (&sp[i]);               thunk_argv[i] = KorkApi::ConsoleValue::makeNumber(v); }
	        else if (t == c_m3Type_f64) { double   v = *(double*)(&sp[i]);               thunk_argv[i] = KorkApi::ConsoleValue::makeNumber(v); }
	    }

	    KorkApi::ConsoleValue retV;
	    vm->callObjectFunction(userModule->getVMObject(), 
	    	                   userModule->mHostFuncs[userInfo->funcIdx], 
	    	                   argc+2, &argv_local[0], retV);

	    // marshal result(s) back to wasm
	    // If callee expects a value, set it on the stack tail according to return type.
	    if (m3_GetRetCount(ctx->function) == 0) 
	    {
	    	return m3Err_none;
	    }

	    M3ValueType rt0 = m3_GetRetType(ctx->function, 0);
	    if (rt0 != c_m3Type_i32)
	    {
	    	returnsString = false;
	    }

	    if (returnsString) 
	    {
		    // Prepare scratch buffer for strings
		    userModule->resetScratch();

	    	// Convert to string
	    	const char* strValue = vm->valueAsString(retV);
	    	U32 size = strlen(strValue);
	    	ScratchAlloc alloc = userModule->allocScratch(scratch, size);
	    	if (alloc.ptr)
	    	{
	    		memcpy(alloc.ptr, strValue, size);
	    		alloc.ptr[size] = '\0';
	    	}
	    	*(uint32_t*)sp = alloc.vmOffset;
	    } 
	    else
	    {
		    if (rt0 == c_m3Type_i32) {
		        *(int32_t*)sp = vm->valueAsInt(retV);
		    } else if (rt0 == c_m3Type_i64) {
		        *(int64_t*)sp = vm->valueAsInt(retV);
		    } else if (rt0 == c_m3Type_f32) {
		        *(F32*)sp = vm->valueAsFloat(retV);
		    } else if (rt0 == c_m3Type_f64) {
		        *(F64*)sp = vm->valueAsFloat(retV);
		    } else {
		        return "m3Err_trapReturnType";
		    }
		}

	    return m3Err_none;
	}
};
IMPLEMENT_CONOBJECT(Wasm3ModuleObject);


