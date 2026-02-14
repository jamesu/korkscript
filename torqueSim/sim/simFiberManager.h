#pragma once

/*

Class for managing a collection of script fibers/coroutines.

Example script:

function coroutine1(%param)
{
	%value = %param;
	fiberYield(%value);
	%value += 1;
	fiberYield(%value);
}

new SimFiberManager(mgr)
{
};

%fiberId = %mgr.spawnFiber(coroutine1, 2);
%yield1 = %mgr.resumeFiber(%fiberId);
%yield2 = %mgr.resumeFiber(%fiberId);
echo("Fiber status now ==" @ %mgr.getFiberStatus(%fiberId));
%mgr.cleanupFiber(%fiberId);

*/

class SimFiberManager : public SimObject
{
typedef SimObject Parent;

protected:

public:

	enum WaitMode
	{
		WAIT_IGNORE=0,        // Skipped during check
		WAIT_FLAGS=1,         // Wait for global flags to be set
		WAIT_FLAGS_CLEAR=2,   // Wait for global flags to be clear
		WAIT_LOCAL_CLEAR=3,   // Wait for local wait flags to be clear
		WAIT_SIMTIME=4,       // Wait for min sim time
		WAIT_TICK=5,          // Wait for a ticker value
		WAIT_NONE=6           // Dont wait just run
	};

	struct ScheduleParam
	{
		U64 flagMask;  // flags to check (or local flags to wait on)
		U64 minTime;   // min time value to resume 
	};

	struct ScheduleInfo
	{
		KorkApi::FiberId fiberId;   // fiber to resume
		WaitMode waitMode; // what we are waiting for
		ScheduleParam param;
	};

	std::vector<ScheduleInfo> mFiberSchedules;
	U64 mFiberGlobalFlags;
	U64 mNowTick;

	static void initPersistFields();

	SimFiberManager();
	~SimFiberManager();
	bool onAdd() override;
	void onRemove() override;

	KorkApi::FiberId spawnFiber(SimObject* thisObject, int argc, KorkApi::ConsoleValue* argv, ScheduleInfo initialInfo);
	void setFiberWaitMode(KorkApi::FiberId fid, WaitMode mode, ScheduleParam param);
	void execFibers(U64 tickAdvance);
	void cleanupFibers();
	void cleanupFiber(KorkApi::FiberId fid);
   
   inline U64 getCurrentTick() { return mNowTick; }

	DECLARE_CONOBJECT(SimFiberManager);
};

