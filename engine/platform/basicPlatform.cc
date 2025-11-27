#include <stdio.h>
#include <stdlib.h>
#include "platform/platformFileIO.h"
#include "platform/threads/thread.h"
#include "platform/threads/mutex.h"
#include "platform/threads/semaphore.h"
#include "core/safeDelete.h"

#include <filesystem>
namespace fs = std::filesystem;

#include <mutex>

File::File()
: currentStatus(Closed), capability(0)
{
   handle = (void *)NULL;
}

File::~File()
{
   close();
   handle = (void *)NULL;
}

File::Status File::open(const char *filename, const AccessMode openMode)
{
   AssertFatal(NULL != filename, "File::open: NULL filename");
   AssertWarn(NULL == handle, "File::open: handle already valid");
   
   // Close the file if it was already open...
   if (Closed != currentStatus)
      close();
   
   FILE* fp = NULL;
   const char* sopenMode = NULL;
   
   switch (openMode)
   {
      case Read:
         sopenMode = "rb";
         break;
      case Write:
         sopenMode = "wb";
         break;
      case ReadWrite:
         sopenMode = "wb+";
         break;
      case WriteAppend:
         sopenMode = "ab+";
         break;
      default:
         AssertFatal(false, "File::open: bad access mode");    // impossible
   }
   
   // if we are writing, make sure output path exists
   if (openMode == Write || openMode == ReadWrite || openMode == WriteAppend)
      Platform::createPath(filename);
   
   fp = fopen(filename, sopenMode);
   handle = fp;
   
   if (fp == NULL)
   {
      return setStatus();
   }
   else
   {
      // successfully created file, so set the file capabilities...
      switch (openMode)
      {
         case Read:
            capability = U32(FileRead);
            break;
         case Write:
         case WriteAppend:
            capability = U32(FileWrite);
            break;
         case ReadWrite:
            capability = U32(FileRead)  |
            U32(FileWrite);
            break;
         default:
            AssertFatal(false, "File::open: bad access mode");
      }
      return currentStatus = Ok;                                // success!
   }
}

U32 File::getPosition() const
{
   AssertFatal(Closed != currentStatus, "File::getPosition: file closed");
   AssertFatal(NULL != handle, "File::getPosition: invalid file handle");
   
   return (U32) ftell((FILE*)handle);
}

File::Status File::setPosition(S32 position, bool absolutePos)
{
   AssertFatal(Closed != currentStatus, "File::setPosition: file closed");
   AssertFatal(NULL != handle, "File::setPosition: invalid file handle");
   
   if (Ok != currentStatus && EOS != currentStatus)
      return currentStatus;
   
   U32 finalPos = 0;
   switch (absolutePos)
   {
      case true:                                                    // absolute position
         AssertFatal(0 <= position, "File::setPosition: negative absolute position");
         
         // position beyond EOS is OK
         fseek((FILE*)handle, position, SEEK_SET);
         finalPos = position;
         break;
      case false:                                                    // relative position
         AssertFatal((getPosition() >= (U32)abs(position) && 0 > position) || 0 <= position, "File::setPosition: negative relative position");
         
         // position beyond EOS is OK
         fseek((FILE*)handle, position, SEEK_CUR);
         finalPos = getPosition();
         break;
   }
   
   if (0xffffffff == finalPos)
      return setStatus();                                        // unsuccessful
   else if (finalPos >= getSize())
      return currentStatus = EOS;                                // success, at end of file
   else
      return currentStatus = Ok;                                // success!
}

U32 File::getSize() const
{
   AssertWarn(Closed != currentStatus, "File::getSize: file closed");
   AssertFatal(NULL != handle, "File::getSize: invalid file handle");
   
   if (Ok == currentStatus || EOS == currentStatus)
   {
      long currentOffset = getPosition();
      fseek((FILE*)handle, 0, SEEK_END);
      long fileSize;
      fileSize = getPosition();
      fseek((FILE*)handle, currentOffset, SEEK_SET);
      return fileSize;
   }
   else
      return 0;
}

File::Status File::flush()
{
   AssertFatal(Closed != currentStatus, "File::flush: file closed");
   AssertFatal(NULL != handle, "File::flush: invalid file handle");
   AssertFatal(true == hasCapability(FileWrite), "File::flush: cannot flush a read-only file");
   
   if (fflush((FILE*)handle))
      return currentStatus = Ok;                                // success!
   else
      return setStatus();                                       // unsuccessful
}

File::Status File::close()
{
   if (handle)
   {
      fclose((FILE*)handle);
      handle = NULL;
   }
   // Set the status to closed
   return currentStatus = Closed;
}

File::Status File::getStatus() const
{
   return currentStatus;
}

File::Status File::setStatus()
{
   return currentStatus = IOError;
}

File::Status File::setStatus(File::Status status)
{
   return currentStatus = status;
}

File::Status File::read(U32 size, char *dst, U32 *bytesRead)
{
   AssertFatal(Closed != currentStatus, "File::read: file closed");
   AssertFatal(NULL != handle, "File::read: invalid file handle");
   AssertFatal(NULL != dst, "File::read: NULL destination pointer");
   AssertFatal(true == hasCapability(FileRead), "File::read: file lacks capability");
   AssertWarn(0 != size, "File::read: size of zero");
   
   if (Ok != currentStatus || 0 == size)
      return currentStatus;
   else
   {
      U32 lastBytes;
      U32 *bytes = (NULL == bytesRead) ? &lastBytes : (U32 *)bytesRead;
      *bytes = (U32)fread(dst, 1, size, (FILE*)handle);
      if (*bytes == 0)
      {
         if (feof((FILE*)handle))
         {
            return currentStatus = EOS;
         }
         else
         {
            setStatus();
         }
      }
      return currentStatus = Ok;                        // end of stream
   }
}

File::Status File::write(U32 size, const char *src, U32 *bytesWritten)
{
   // JMQ: despite the U32 parameters, the maximum filesize supported by this
   // function is probably the max value of S32, due to the unix syscall
   // api.
   AssertFatal(Closed != currentStatus, "File::write: file closed");
   AssertFatal(NULL != handle, "File::write: invalid file handle");
   AssertFatal(NULL != src, "File::write: NULL source pointer");
   AssertFatal(true == hasCapability(FileWrite), "File::write: file lacks capability");
   AssertWarn(0 != size, "File::write: size of zero");
   
   if ((Ok != currentStatus && EOS != currentStatus) || 0 == size)
      return currentStatus;
   else
   {
      S32 numWritten = fwrite(src, 1, size, (FILE*)handle);
      if (numWritten < 0)
         return setStatus();
      
      if (bytesWritten)
         *bytesWritten = static_cast<U32>(numWritten);
      return currentStatus = Ok;
   }
}

bool File::hasCapability(Capability cap) const
{
   return (0 != (U32(cap) & capability));
}


void Platform::init()
{

}

void Platform::process()
{

}

void Platform::shutdown()
{

}

void Platform::sleep(U32 ms)
{

}

void Platform::restartInstance()
{

}

void Platform::postQuitMessage(const U32 in_quitVal)
{

}

void Platform::forceShutdown(S32 returnValue)
{

}

StringTableEntry Platform::getUserHomeDirectory()
{
   return NULL;
}

StringTableEntry Platform::getUserDataDirectory()
{
   return NULL;
}


U32 Platform::getTime( void )
{
   return 0;
}

U32 Platform::getVirtualMilliseconds( void )
{
   return 0;
}

U32 Platform::getRealMilliseconds( void )
{
   return 0;
}

void Platform::advanceTime(U32 delta)
{

}

void Platform::getLocalTime(LocalTime &)
{

}

S32 Platform::compareFileTimes(const FileTime &a, const FileTime &b)
{
   return 0;
}

/// Math.
float Platform::getRandom()
{
   return 3;
}

/// Debug.
void Platform::debugBreak()
{
   
}

void Platform::outputDebugString(const char *string)
{
   
}

/// File IO.
StringTableEntry Platform::getCurrentDirectory()
{
   return NULL;
}

bool Platform::setCurrentDirectory(StringTableEntry newDir)
{
   return false;
}

StringTableEntry Platform::getExecutableName()
{
   return NULL;
}

StringTableEntry Platform::getExecutablePath()
{
   return NULL;
}

static char *makeFullPathName(const char *path, char *buffer, U32 size, const char *cwd)
{
   return NULL;
}

bool Platform::dumpPath(const char *in_pBasePath, Vector<FileInfo>& out_rFileVector, S32 recurseDepth)
{
   return false;
}

bool Platform::dumpDirectories( const char *path, Vector<StringTableEntry> &directoryVector, S32 depth, bool noBasePath )
{
   return false;
}

bool Platform::hasSubDirectory( const char *pPath )
{
   return false;
}

bool Platform::getFileTimes(const char *filePath, FileTime *createTime, FileTime *modifyTime)
{
   return false;
}

bool Platform::isFile(const char *pFilePath)
{
   return false;
}

S32  Platform::getFileSize(const char *pFilePath)
{
   return 0;
}

bool Platform::isDirectory(const char *pDirPath)
{
   return false;
}

bool Platform::isSubDirectory(const char *pParent, const char *pDir)
{
   return false;
}

bool Platform::createPath(const char *path)
{
   return false;
}

bool Platform::fileDelete(const char *name)
{
   return false;
}

bool Platform::fileRename(const char *oldName, const char *newName)
{
   return false;
}

bool Platform::fileTouch(const char *name)
{
   return false;
}

bool Platform::pathCopy(const char *fromName, const char *toName, bool nooverwrite)
{
   return false;
}

StringTableEntry Platform::osGetTemporaryDirectory()
{
   return NULL;
}



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
