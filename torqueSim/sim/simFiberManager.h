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
   
   enum WaitMode : U8
   {
      WAIT_IGNORE=0,        // Skipped during check
      WAIT_FLAGS=1,         // Wait for global flags to be set
      WAIT_FLAGS_CLEAR=2,   // Wait for global flags to be clear
      WAIT_LOCAL_CLEAR=3,   // Wait for local wait flags to be clear
      WAIT_SIMTIME=4,       // Wait for min sim time
      WAIT_TICK=5,          // Wait for a ticker value
      WAIT_FIBER=6,         // Waiting for fiber to stop
      WAIT_NONE=7,          // Dont wait just run
      WAIT_REMOVE=8         // Waiting to be removed (used in case current is active)
   };
   
   enum BaseFlags : U8
   {
      // Flag for when time based waits are visited
      FLAG_VISITED = BIT(0),
      // Flag to mark object call
      FLAG_OBJECT = BIT(1),
      // These flags can never be set from user code
      STICKY_FLAGS_MASK = BIT(1) | BIT(2) | BIT(3) | BIT(4)
   };
   
   enum
   {
      WaitFlagStackSize = 4,
      WaitFlagFiberStackSize = 4
   };
   
   struct ScheduleParam
   {
      U64 flagMask;  // flags to check (or local flags to wait on)
      U64 minTime;   // min time value to resume
   };
   
   struct ScheduleInfo
   {
      KorkApi::FiberId fiberId;   // fiber to resume
      ScheduleParam param; // schedule criteria
      SimObjectId thisId; // who spawned us
      WaitMode waitMode; // what we are waiting for
      S8 waitSlot; //
   };
   
   struct WaitFlagStack
   {
      U64 flag;
      U64 flagStack[WaitFlagFiberStackSize];
      KorkApi::FiberId fiberId;
      U8 stackPos;
      U8 overflow;
      bool used;
   };

	std::vector<ScheduleInfo> mFiberSchedules;
	U64 mFiberGlobalFlags;
   U64 mWaitFiberFlags;
   U64 mUserWaitFiberFlags;
	U64 mNowTick;
   WaitFlagStack mWaitFlagStack[WaitFlagStackSize];

	static void initPersistFields();

	SimFiberManager();
	~SimFiberManager();
	bool onAdd() override;
	void onRemove() override;

	KorkApi::FiberId spawnFiber(SimObject* thisObject, int argc, KorkApi::ConsoleValue* argv, ScheduleInfo initialInfo);
	void setFiberWaitMode(KorkApi::FiberId fid, WaitMode mode, ScheduleParam param);
	void execFibers(U64 tickAdvance);
   
   void setSuspendMode(U64 flags);
   void recalculateSuspendFlags();
   void pushFiberSuspendFlags(U64 flags);
   void popFiberSuspendFlags(U64 flags);
   void cleanupFiberSuspendFlags(KorkApi::FiberId fid);
   
   void cleanupWithFlags(U64 flags);
   void cleanupWithObjectId(SimObjectId objectId);
   
	void cleanupFibers();
	void cleanupFiber(KorkApi::FiberId fid);
   
   void throwWithMask(U64 fiberMask, U32 catchMask);
   void throwWithObject(SimObjectId objectId, U32 catchMask);
   
   inline U64 getCurrentTick() { return mNowTick; }

	DECLARE_CONOBJECT(SimFiberManager);
};

