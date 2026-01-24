#include "sim/simBase.h"
#include "sim/simFiberManager.h"
#include "console/consoleTypes.h"

IMPLEMENT_CONOBJECT(SimFiberManager);

SimFiberManager::SimFiberManager()
   : mFiberGlobalFlags(0)
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
      ret = vm->callObject((KorkApi::VMObject*)thisObject, argc, argv, true);
   }
   else
   {
      ret = vm->call(argc, argv, true);
   }

   ScheduleInfo info;
   initialInfo.fiberId  = fid;
   mFiberSchedules.push_back(info);

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
      itr->param    = param;
   }
}

void SimFiberManager::cleanupFiber(KorkApi::FiberId fid)
{
   KorkApi::Vm* vm = getVM();
   vm->cleanupFiber(fid);

   auto itr = std::find_if(mFiberSchedules.begin(), mFiberSchedules.end(), [fid](ScheduleInfo& info){
   	return info.fiberId == fid;
   });
   
   if (itr != mFiberSchedules.end())
   {
     vm->cleanupFiber(fid);
     itr->fiberId = 0;
   }
}

static bool shouldRunFiber(const SimFiberManager::ScheduleInfo &info,
                           U64 globalFlags,
                           F64 nowTime)
{
   using WaitMode = SimFiberManager::WaitMode;

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

      case SimFiberManager::WAIT_SIMTIME:
         // Wait until current sim time >= minTime.
         return nowTime >= info.param.minTime;

      default:
         break;
   }

   return false;
}

void SimFiberManager::execFibers()
{
   KorkApi::Vm* vm = getVM();
   F64 nowTime = Sim::getCurrentTime();

   for (ScheduleInfo &info : mFiberSchedules)
   {
      if (!shouldRunFiber(info, mFiberGlobalFlags, nowTime))
      {
         continue;
      }

      // Ready to run.
      vm->setCurrentFiber(info.fiberId);

      KorkApi::ConsoleValue inValue = KorkApi::ConsoleValue();
      KorkApi::FiberRunResult result = vm->resumeCurrentFiber(inValue);
      
      if (result.state == KorkApi::FiberRunResult::FINISHED)
      {
         vm->cleanupFiber(info.fiberId);
         info.fiberId = 0;
      }
   }
   
   vm->setCurrentFiberMain();
   cleanupFibers();
}

void SimFiberManager::cleanupFibers()
{
   for (S32 i = (S32)mFiberSchedules.size() - 1; i >= 0; --i)
   {
      ScheduleInfo &info = mFiberSchedules[i];
      
      if (info.fiberId == 0)
      {
         mFiberSchedules.erase(mFiberSchedules.begin() + i);
         continue;
      }
   }
}

