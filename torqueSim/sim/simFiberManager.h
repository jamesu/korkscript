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
		WAIT_IGNORE=0,      // Skipped during check
		WAIT_FLAGS=1,       // Wait for global flags to be set
		WAIT_FLAGS_CLEAR=2, // Wait for global flags to be clear
		WAIT_SIMTIME=3,      // Wait for min sim time
		WAIT_NONE=4         // Dont wait just run
	};

	struct ScheduleParam
	{
		U64 flagMask;  // flags to check
		F64 minTime;   // min time to resume
	};

	struct ScheduleInfo
	{
		KorkApi::FiberId fiberId;   // fiber to resume
		WaitMode waitMode; // what we are waiting for
		ScheduleParam param;
	};

	Vector<ScheduleInfo> mFiberSchedules;
	U64 mFiberGlobalFlags;

	static void initPersistFields();

	SimFiberManager();
	~SimFiberManager();
	bool onAdd() override;
	void onRemove() override;

	KorkApi::FiberId spawnFiber(SimObject* thisObject, int argc, KorkApi::ConsoleValue* argv, ScheduleInfo initialInfo);
	void setFiberWaitMode(KorkApi::FiberId fid, WaitMode mode, ScheduleParam param);
	void execFibers();
   void cleanupFibers();
	void cleanupFiber(KorkApi::FiberId fid);

	DECLARE_CONOBJECT(SimFiberManager);
};

