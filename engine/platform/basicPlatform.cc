#include <stdio.h>
#include <stdlib.h>
#include "platform/platform.h"
#include "platform/threads/thread.h"
#include "platform/threads/mutex.h"
#include "platform/threads/semaphore.h"
#include "core/safeDelete.h"

struct PlatformThreadData
{
    ThreadRunFunction       mRunFunc;
    void*                   mRunArg;
    Thread*                 mThread;
    ThreadIdent             mThreadID;
};

//-----------------------------------------------------------------------------

Thread::Thread(ThreadRunFunction func, void* arg, bool start_thread, bool autodelete)
{
    mData = new PlatformThreadData;
    mData->mRunFunc = func;
    mData->mRunArg = arg;
    mData->mThread = this;
    mData->mThreadID = 0;
    autoDelete = autodelete;
    
    if(start_thread)
        start();
}

//-----------------------------------------------------------------------------

Thread::~Thread()
{
    stop();
    join();
    
    SAFE_DELETE(mData);
}

//-----------------------------------------------------------------------------

void Thread::start()
{
}

//-----------------------------------------------------------------------------

bool Thread::join()
{
   return false;
}

//-----------------------------------------------------------------------------

void Thread::run(void* arg)
{
    if(mData->mRunFunc)
        mData->mRunFunc(arg);
}

//-----------------------------------------------------------------------------

bool Thread::isAlive()
{
   return false;
}

//-----------------------------------------------------------------------------

ThreadIdent Thread::getId()
{
   return mData->mThreadID;
}

//-----------------------------------------------------------------------------

ThreadIdent ThreadManager::getCurrentThreadId()
{
   return 0;
}

//-----------------------------------------------------------------------------

bool ThreadManager::compare( ThreadIdent threadId_1, ThreadIdent threadId_2 )
{
   return false;
}


struct PlatformMutexData
{
    pthread_mutex_t   mMutex;
    bool              locked;
    ThreadIdent       lockedByThread;
};

//-----------------------------------------------------------------------------

Mutex::Mutex()
{
    bool ok;
    
    // Create the mutex data.
    mData = new PlatformMutexData;
    
    // Sanity!
    AssertFatal(ok == 0, "Mutex() failed: pthread_mutex_init() failed.");
    
    // Set the initial mutex state.
    mData->locked = false;
    mData->lockedByThread = 0;
}

//-----------------------------------------------------------------------------

Mutex::~Mutex()
{
    
    // Sanity!
    AssertFatal(ok == 0, "~Mutex() failed: pthread_mutex_destroy() failed.");
    
    // Delete the mutex data.
    SAFE_DELETE( mData );
}

//-----------------------------------------------------------------------------

bool Mutex::lock( bool block )
{
   return false;
}

//-----------------------------------------------------------------------------

void Mutex::unlock()
{
}



//-----------------------------------------------------------------------------

struct PlatformSemaphore
{
    S32 count;
};

//-----------------------------------------------------------------------------

Semaphore::Semaphore(S32 initialCount)
{
    bool ok;

    // Create the semaphore data.
    mData = new PlatformSemaphore;
    
    // Sanity!
    AssertFatal(ok == 0,"Create semaphore failed at creating mutex mDarkroom.");
    
    // Sanity!
    AssertFatal(ok == 0,"Create semaphore failed at creating condition mCond.");
    
    // Set the initial semaphore count.
    mData->count = initialCount;
}

//-----------------------------------------------------------------------------

Semaphore::~Semaphore()
{
    // Destroy the semaphore data.
    delete mData;
}

//-----------------------------------------------------------------------------

bool Semaphore::acquire( bool block, S32 timeoutMS )
{
   return false;
}

//-----------------------------------------------------------------------------

void Semaphore::release()
{
}
