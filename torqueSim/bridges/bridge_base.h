#pragma once

/*

Base class for KorkScript module-based bridge binder.

Module exports get exposed as namespace functions in the modules namespace.
Host exports are namespace functions which can be imported by loaded runtime code.

This basically works as an explicit dynamic import/export mechanism between the 
namespace system and the target runtime, as opposed to the bridge being hardcoded in C++. 

The module can also target other namespaces provided you use the "Namespace::function" form.

Example usage:

new <BridgeClass>(MyModule)
{
	// Funcs in runtime
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

class BaseBridgeObject : public SimObject
{
typedef SimObject Parent;

protected:

	enum
	{
		MAX_FUNCS = 16
	};

	template<class T> struct FuncUserInfo
	{
		T* module;
		U32 funcIdx;
	};

	struct ScratchAlloc
	{
		U8* ptr;
		U32 vmOffset;
	};

public:

	template<class A> ScratchAlloc allocScratch(A talloc, U32 bytes)
	{
		ScratchAlloc alloc = {};

		// Don't return if we cant possibly fit it in
		if (bytes > mScratchSize)
		{
			return alloc;
		}

		U32 memSize = 0;
		alloc.vmOffset = mScratchAllocPtr;
		mScratchAllocPtr += bytes;

		// Reset if overflow
		if (alloc.vmOffset > mScratchSize)
		{
			alloc.vmOffset = 0;
			mScratchAllocPtr = 0;
		}

		alloc.vmOffset += mScratchOffset;
		alloc.ptr = talloc.getPtr(alloc.vmOffset);
		return alloc;
	}

	void resetScratch()
	{
		mScratchAllocPtr = 0;
	}

	static void convertSig(char* sig)
	{
		while (*sig != '\0')
		{
			if (*sig == 's')
				*sig = 'i';
			sig++;
		}
	}

	static bool isSigValid(const char* sig)
	{
		const char* startSig = sig;
		while (*sig != '\0' && *sig != '(')
			sig++;
		if (*sig == '\0' || (sig - startSig != 1))
			return false;
		while (*sig != '\0' && *sig != ')')
			sig++;
		if (*sig != ')')
			return false;
		if (sig - startSig > 31)
			return false;
		return true;
	}

	static U32 getSigParamCount(const char* sig)
	{
		const char* startSig = sig;
		while (*sig != '\0' && *sig != '(')
			sig++;
		if (*sig == '\0')
			return 0;
		startSig = sig = sig+1;
		while (*sig != '\0' && *sig != ')')
			sig++;
		if (*sig != ')')
			return 0;
		return (U32)(sig - startSig);
	}

	static void initPersistFields()
	{
		Parent::initPersistFields();

		addField("memSize", TypeS32, Offset(mMemSize, BaseBridgeObject));
		addField("moduleFile", TypeString, Offset(mModuleFile, BaseBridgeObject));

		addField("funcName", TypeString, Offset(mFuncNames[2], BaseBridgeObject), MAX_FUNCS-2);
		addField("funcSig", TypeString, Offset(mFuncSignatures[2], BaseBridgeObject), MAX_FUNCS-2);
		addField("hostFuncName", TypeString, Offset(mHostFuncs, BaseBridgeObject), MAX_FUNCS);
		addField("hostFuncSig", TypeString, Offset(mHostFuncSignatures, BaseBridgeObject), MAX_FUNCS);

		// KorkScript-specific
		addField("className", TypeString, Offset(mClassName, BaseBridgeObject));
	}

	BaseBridgeObject()
	{
		mMemSize = 128 * 1024;
		mScratchSize = 256;
		mModuleFile = NULL;
		mClassName = NULL;
		memset(mFuncNames, '\0', sizeof(mFuncNames));
		memset(mFuncSignatures, '\0', sizeof(mFuncSignatures));
		memset(mHostFuncs, '\0', sizeof(mHostFuncs));
		memset(mHostFuncSignatures, '\0', sizeof(mHostFuncSignatures));
		mScratchOffset = 0;
		mScratchAllocPtr = 0;
	}

	~BaseBridgeObject()
	{
		cleanup();
	}

	virtual void initScratch() = 0;
	virtual bool initRuntime() = 0;
	virtual void cleanup() = 0;
	virtual bool load(Stream& s) = 0;
	virtual bool linkFuncs() = 0;

	bool onAdd() override
	{
		mNSLinkMask = LinkClassName;

		if (!Parent::onAdd())
			return false;

		FileStream fs;

		// Use malloc and free for base allocators
		mFuncNames[0] = StringTable->insert("malloc");
		mFuncNames[1] = StringTable->insert("free");
		mFuncSignatures[0] = StringTable->insert("i(i)");
		mFuncSignatures[1] = StringTable->insert("v(i)");

		if (!initRuntime() || !fs.open(mModuleFile, FileStream::Read))
		{
			cleanup();
			return false;
		}

		if (!load(fs) || !linkFuncs())
		{
			cleanup();
			return false;
		}

		initScratch();
		return true;
	}

public:
	U32 mMemSize;
	U32 mScratchSize;
	StringTableEntry mModuleFile;

	// These get called inside WASM
	StringTableEntry mFuncNames[MAX_FUNCS];
	StringTableEntry mFuncSignatures[MAX_FUNCS];

	// These get called from WASM thunk
	StringTableEntry mHostFuncs[MAX_FUNCS];
	StringTableEntry mHostFuncSignatures[MAX_FUNCS];

	//StringTableEntry mClassName;
	U32 mScratchOffset;
	U32 mScratchAllocPtr;
};

