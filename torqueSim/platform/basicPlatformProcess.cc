#include <stdio.h>
#include <stdlib.h>
#include "platform/platform.h"
#include "platform/platformProcess.h"
#include "platform/platformFileIO.h"
#include "platform/threads/thread.h"
#include "platform/threads/mutex.h"
#include "platform/threads/semaphore.h"
#include "core/safeDelete.h"

#include <filesystem>
namespace fs = std::filesystem;

#include <mutex>

namespace Platform
{

void init()
{
   
}

void process()
{
   
}

void shutdown()
{
   
}

void sleep(U32 ms)
{
   
}

void restartInstance()
{
   
}

void postQuitMessage(const U32 in_quitVal)
{
   
}

void forceShutdown(S32 returnValue)
{
   
}

StringTableEntry getUserHomeDirectory()
{
   return NULL;
}

StringTableEntry getUserDataDirectory()
{
   return NULL;
}


U32 getTime( void )
{
   return 0;
}

U32 getVirtualMilliseconds( void )
{
   return 0;
}

U32 getRealMilliseconds( void )
{
   return 0;
}

void advanceTime(U32 delta)
{
   
}

void getLocalTime(LocalTime &)
{
   
}

S32 compareFileTimes(const FileTime &a, const FileTime &b)
{
   return 0;
}

/// Math.
float getRandom()
{
   return 3;
}

void outputDebugString(const char *string)
{
   
}

/// File IO.
StringTableEntry getCurrentDirectory()
{
   return NULL;
}

bool setCurrentDirectory(StringTableEntry newDir)
{
   return false;
}

StringTableEntry getExecutableName()
{
   return NULL;
}

StringTableEntry getExecutablePath()
{
   return NULL;
}

bool dumpPath(const char *in_pBasePath, std::vector<FileInfo>& out_rFileVector, S32 recurseDepth)
{
   return false;
}

bool dumpDirectories( const char *path, std::vector<StringTableEntry> &directoryVector, S32 depth, bool noBasePath )
{
   return false;
}

bool hasSubDirectory( const char *pPath )
{
   return false;
}

bool getFileTimes(const char *filePath, FileTime *createTime, FileTime *modifyTime)
{
   return false;
}

bool isFile(const char *pFilePath)
{
   return false;
}

S32  getFileSize(const char *pFilePath)
{
   return 0;
}

bool isDirectory(const char *pDirPath)
{
   return false;
}

bool isSubDirectory(const char *pParent, const char *pDir)
{
   return false;
}

bool createPath(const char *path)
{
   return false;
}

bool fileDelete(const char *name)
{
   return false;
}

bool fileRename(const char *oldName, const char *newName)
{
   return false;
}

bool fileTouch(const char *name)
{
   return false;
}

bool pathCopy(const char *fromName, const char *toName, bool nooverwrite)
{
   return false;
}

}

