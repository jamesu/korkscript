#include "sim/simBase.h"
#include "sim/simFiberManager.h"
#include "console/consoleTypes.h"

IMPLEMENT_CONOBJECT(SimFiberManager);

SimFiberManager::SimFiberManager()
   : mFiberGlobalFlags(0), mNowTick(0)
{
}

SimFiberManager::~SimFiberManager()
{
}

bool SimFiberManager::onAdd()
{
   if (!Parent::onAdd())
      return false;

   mFiberSchedules.clear();
   mFiberGlobalFlags = 0;
   mWaitFiberFlags = 0;

   return true;
}

void SimFiberManager::onRemove()
{
   // Clean up all fibers we’re tracking.
   KorkApi::Vm* vm = getVM();
   if (vm)
   {
      for (U32 i = 0; i < mFiberSchedules.size(); ++i)
      {
         const ScheduleInfo &info = mFiberSchedules[i];
         if (info.fiberId != 0)
         {
            vm->cleanupFiber(info.fiberId);
         }
      }
   }

   mFiberSchedules.clear();
   Parent::onRemove();
}

void SimFiberManager::initPersistFields()
{
   Parent::initPersistFields();
   addField("flags", TypeS32, Offset(mFiberGlobalFlags, SimFiberManager));
}

KorkApi::FiberId SimFiberManager::spawnFiber(SimObject* thisObject,
                                             int argc,
                                             KorkApi::ConsoleValue* argv,
                                             SimFiberManager::ScheduleInfo initialInfo)
{
   KorkApi::Vm* vm = getVM();

   KorkApi::FiberId currentFiber = vm->getCurrentFiber();
   
   vm->setCurrentFiberMain();
   KorkApi::FiberId fid = vm->createFiber(this);
   if (fid == 0)
   {
      return 0;
   }

   vm->setCurrentFiber(fid);

   KorkApi::ConsoleValue ret;

   if (thisObject)
   {
      initialInfo.thisId = thisObject->getId();
      initialInfo.param.flagMask |= SimFiberManager::FLAG_OBJECT;
      ret = vm->callObject(thisObject->getVMObject(), argc, argv, true);
   }
   else
   {
      initialInfo.thisId = 0;
      initialInfo.param.flagMask &= ~SimFiberManager::FLAG_OBJECT;
      ret = vm->call(argc, argv, true);
   }
   
   auto fiberState = vm->getCurrentFiberState();
   
   // Fibers that don't end up being suspended here won't be added.
   if (fiberState != KorkApi::FiberRunResult::SUSPENDED)
   {
      vm->cleanupFiber(fid);
      vm->setCurrentFiber(currentFiber);
      return 0;
   }

   initialInfo.fiberId  = fid;
   mFiberSchedules.push_back(initialInfo);
   
   // Actually run fiber
   vm->resumeCurrentFiber(KorkApi::ConsoleValue());
   
   // Return control
   vm->setCurrentFiber(currentFiber);

   return fid;
}

void SimFiberManager::setFiberWaitMode(KorkApi::FiberId fid,
                                       WaitMode mode,
                                       ScheduleParam param)
{
   auto itr = std::find_if(mFiberSchedules.begin(), mFiberSchedules.end(), [fid](ScheduleInfo& info){
   	return info.fiberId == fid;
   });

   if (itr != mFiberSchedules.end())
   {
      itr->waitMode = mode;
      itr->param.flagMask = (itr->param.flagMask & STICKY_FLAGS_MASK) |
                            (param.flagMask & (~STICKY_FLAGS_MASK));
      itr->param.minTime = param.minTime;
   }
}

void SimFiberManager::cleanupFiber(KorkApi::FiberId fid)
{
   KorkApi::Vm* vm = getVM();

   auto itr = std::find_if(mFiberSchedules.begin(), mFiberSchedules.end(), [fid](ScheduleInfo& info){
   	return info.fiberId == fid;
   });
   
   if (itr != mFiberSchedules.end())
   {
      if (getVM()->getFiberState(fid) != KorkApi::FiberRunResult::RUNNING)
      {
         getVM()->cleanupFiber(fid);
         itr->fiberId = 0;
      }
      itr->waitMode = WAIT_REMOVE;
   }
}

static bool shouldRunFiber(const SimFiberManager::ScheduleInfo &info,
                           U64 suspendFlags,
                           U64 globalFlags,
                           U64 nowTime,
                           U64 nowTick)
{
   using WaitMode = SimFiberManager::WaitMode;
   
   // override: don't schedule if this flag set
   if ((info.param.flagMask & suspendFlags) != 0)
   {
      return false;
   }

   switch (info.waitMode)
   {
      case SimFiberManager::WAIT_IGNORE:
         // Not considered this tick.
         return false;

      case SimFiberManager::WAIT_NONE:
         // Always runnable.
         return true;

      case SimFiberManager::WAIT_FLAGS:
         // Wait until *all* bits in flagMask are set in globalFlags.
         return (globalFlags & info.param.flagMask) == info.param.flagMask;

      case SimFiberManager::WAIT_FLAGS_CLEAR:
         // Wait until *no* bits in flagMask are set in globalFlags.
         return (globalFlags & info.param.flagMask) == 0;

      case SimFiberManager::WAIT_LOCAL_CLEAR:
         return (info.param.flagMask == 0);

      case SimFiberManager::WAIT_SIMTIME:
         // Wait until current sim time >= minTime.
         return ((info.param.flagMask & SimFiberManager::FLAG_VISITED) == 0) && nowTime >= info.param.minTime;

      case SimFiberManager::WAIT_TICK:
         return ((info.param.flagMask & SimFiberManager::FLAG_VISITED) == 0) && nowTick >= info.param.minTime;

      default:
         break;
   }

   return false;
}

void SimFiberManager::execFibers(U64 tickAdvance)
{
   KorkApi::Vm* vm = getVM();
   U64 nowTime = Sim::getCurrentTime();
   mNowTick += tickAdvance;

   for (ScheduleInfo &info : mFiberSchedules)
   {
      if (!shouldRunFiber(info, mWaitFiberFlags, mFiberGlobalFlags, nowTime, mNowTick))
      {
         continue;
      }
      
      // Mark as used
      if (info.waitMode == SimFiberManager::WAIT_SIMTIME ||
          info.waitMode == SimFiberManager::WAIT_TICK)
      {
         info.param.flagMask |= FLAG_VISITED;
      }

      // Ready to run.
      vm->setCurrentFiber(info.fiberId);

      KorkApi::ConsoleValue inValue = KorkApi::ConsoleValue();
      KorkApi::FiberRunResult result = vm->resumeCurrentFiber(inValue);
      
      // NOTE: technically we should only get suspended here; RUNNING is only possible if
      // we call execFibers from a fiber which isn't possible.
      if (result.state != KorkApi::FiberRunResult::SUSPENDED)
      {
         vm->cleanupFiber(info.fiberId);
         info.fiberId = 0;
         info.waitMode = WAIT_REMOVE;
      }
   }
   
   vm->setCurrentFiberMain();
   cleanupFibers();
}

void SimFiberManager::setSuspendMode(U64 flags)
{
   mWaitFiberFlags = flags;
}

void SimFiberManager::cleanupWithFlags(U64 flags)
{
   for (S32 i = (S32)mFiberSchedules.size() - 1; i >= 0; --i)
   {
      ScheduleInfo &info = mFiberSchedules[i];
      
      if (info.fiberId != 0 &&
          (info.param.flagMask & flags) != 0)
      {
         if (getVM()->getFiberState(info.fiberId) != KorkApi::FiberRunResult::RUNNING)
         {
            getVM()->cleanupFiber(info.fiberId);
            info.fiberId = 0;
         }
         info.waitMode = WAIT_REMOVE;
      }
   }
}

void SimFiberManager::cleanupWithObjectId(SimObjectId objectId)
{
   for (S32 i = (S32)mFiberSchedules.size() - 1; i >= 0; --i)
   {
      ScheduleInfo &info = mFiberSchedules[i];
      
      if (info.fiberId != 0 &&
          info.thisId == objectId)
      {
         if (getVM()->getFiberState(info.fiberId) != KorkApi::FiberRunResult::RUNNING)
         {
            getVM()->cleanupFiber(info.fiberId);
            info.fiberId = 0;
         }
         info.waitMode = WAIT_REMOVE;
      }
   }
}

void SimFiberManager::cleanupFibers()
{
   for (S32 i = (S32)mFiberSchedules.size() - 1; i >= 0; --i)
   {
      ScheduleInfo &info = mFiberSchedules[i];
      
      if (info.fiberId == 0 ||
          info.waitMode == WAIT_REMOVE)
      {
         if (info.fiberId != 0)
         {
            getVM()->cleanupFiber(info.fiberId);
         }
         mFiberSchedules.erase(mFiberSchedules.begin() + i);
         continue;
      }
   }
}

