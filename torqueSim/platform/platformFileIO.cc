//-----------------------------------------------------------------------------
// Copyright (c) 2013 GarageGames, LLC
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "platform/platformProcess.h"
#include "platform/platformFileIO.h"
#include "core/stringTable.h"

#include <vector>

//-----------------------------------------------------------------------------

namespace Platform
{

StringTableEntry osGetTemporaryDirectory()
{
   return NULL;
}

StringTableEntry getTemporaryDirectory()
{
   StringTableEntry path = osGetTemporaryDirectory();
   
   if(! Platform::isDirectory(path))
      path = Platform::getCurrentDirectory();
   
   return path;
}

StringTableEntry getTemporaryFileName()
{
   char buf[512];
   StringTableEntry path = Platform::getTemporaryDirectory();
   
   dSprintf(buf, sizeof(buf), "%s/tgb.%08x.%02x.tmp", path, Platform::getRealMilliseconds(), U32(Platform::getRandom() * 255));
   
   // [tom, 9/7/2006] This shouldn't be needed, but just in case
   if(Platform::isFile(buf))
      return Platform::getTemporaryFileName();
   
   return StringTable->insert(buf);
}

//-----------------------------------------------------------------------------
static char filePathBuffer[1024];
static bool deleteDirectoryRecusrive( const char* pPath )
{
   // Sanity!
   AssertFatal( pPath != NULL, "Cannot delete directory that is NULL." );
   
   // Find directories.
   std::vector<StringTableEntry> directories;
   if ( !Platform::dumpDirectories( pPath, directories, 0 ) )
   {
      // Warn.
      return false;
   }
   
   // Iterate directories.
   for( std::vector<StringTableEntry>::iterator basePathItr = directories.begin(); basePathItr != directories.end(); ++basePathItr )
   {
      // Fetch base path.
      StringTableEntry basePath = *basePathItr;
      
      // Skip if the base path.
      if ( basePathItr == directories.begin() && dStrcmp( pPath, basePath ) == 0 )
         continue;
      
      // Delete any directories recursively.
      if ( !deleteDirectoryRecusrive( basePath ) )
         return false;
   }
   
   // Find files.
   std::vector<Platform::FileInfo> files;
   if ( !Platform::dumpPath( pPath, files, 0 ) )
   {
      return false;
   }
   
   // Iterate files.
   for ( std::vector<Platform::FileInfo>::iterator fileItr = files.begin(); fileItr != files.end(); ++fileItr )
   {
      // Format file.
      dSprintf( filePathBuffer, sizeof(filePathBuffer), "%s/%s", fileItr->pFullPath, fileItr->pFileName );
      
      // Delete file.
      if ( !Platform::fileDelete( filePathBuffer ) )
      {
         return false;
      }
   }
   
   // Delete the directory.
   if ( !Platform::fileDelete( pPath ) )
   {
      return false;
   }
   
   return true;
}

//-----------------------------------------------------------------------------

bool deleteDirectory( const char* pPath )
{
   // Sanity!
   AssertFatal( pPath != NULL, "Cannot delete directory that is NULL." );
   
   // Is the path a file?
   if ( Platform::isFile( pPath ) )
   {
      return false;
   }
   // Delete directory recursively.
   
   return deleteDirectoryRecusrive( pPath );
}

//-----------------------------------------------------------------------------

static StringTableEntry sgMainCSDir = NULL;

StringTableEntry getMainDotCsDir()
{
   if(sgMainCSDir == NULL)
      sgMainCSDir = Platform::getExecutablePath();
   
   return sgMainCSDir;
}

void setMainDotCsDir(const char *dir)
{
   sgMainCSDir = StringTable->insert(dir);
}

//-----------------------------------------------------------------------------

typedef std::vector<char*> CharVector;
static CharVector gPlatformDirectoryExcludeList;

void addExcludedDirectory(const char *pDir)
{
   gPlatformDirectoryExcludeList.push_back(strdup(pDir));
}

void clearExcludedDirectories()
{
   while(gPlatformDirectoryExcludeList.size())
   {
      free(gPlatformDirectoryExcludeList.back());
      gPlatformDirectoryExcludeList.pop_back();
   }
}

bool isExcludedDirectory(const char *pDir)
{
   for(CharVector::iterator i=gPlatformDirectoryExcludeList.begin(); i!=gPlatformDirectoryExcludeList.end(); i++)
      if(!dStricmp(pDir, *i))
         return true;
   
   return false;
}

//-----------------------------------------------------------------------------

inline void catPath(char *dst, const char *src, U32 len)
{
   if (dStrlen(dst) == 0)
   {
      dStrncpy(dst, src, len);
      dst[len - 1] = 0;
      return;
   }
   
   if(*dst != '/')
   {
      ++dst; --len;
      *dst = '/';
   }
   
   ++dst; --len;
   
   dStrncpy(dst, src, len);
   dst[len - 1] = 0;
}

// converts the posix root path "/" to "c:/" for win32
// FIXME: this is not ideal. the c: drive is not guaranteed to exist.
#if defined(TORQUE_OS_WIN32)
static inline void _resolveLeadingSlash(char* buf, U32 size)
{
   if(buf[0] != '/')
      return;
   
   AssertFatal(dStrlen(buf) + 2 < size, "Expanded path would be too long");
   memmove(buf + 2, buf, dStrlen(buf));
   buf[0] = 'c';
   buf[1] = ':';
}
#endif

char * makeFullPathName(const char *path, char *buffer, U32 size, const char *cwd /* = NULL */)
{
   char bspath[1024];
   dStrncpy(bspath, path, sizeof(bspath));
   bspath[sizeof(bspath)-1] = 0;
   
   for(U32 i = 0;i < dStrlen(bspath);++i)
   {
      if(bspath[i] == '\\')
         bspath[i] = '/';
   }
   
   if(Platform::isFullPath(bspath))
   {
      // Already a full path
#if defined(TORQUE_OS_WIN32)
      _resolveLeadingSlash(bspath, sizeof(bspath));
#endif
      dStrncpy(buffer, bspath, size);
      buffer[size-1] = 0;
      return buffer;
   }
   
   if(cwd == NULL)
      cwd = Platform::getCurrentDirectory();
   
   dStrncpy(buffer, cwd, size);
   buffer[size-1] = 0;
   
   char *ptr = bspath;
   char *slash = NULL;
   char *endptr = buffer + dStrlen(buffer) - 1;
   
   do
   {
      slash = dStrchr(ptr, '/');
      if(slash)
      {
         *slash = 0;
         
         // Directory
         
         if(dStrcmp(ptr, "..") == 0)
         {
            // Parent
            endptr = dStrrchr(buffer, '/');
         }
         else if(dStrcmp(ptr, ".") == 0)
         {
            // Current dir
         }
         else if(endptr)
         {
            catPath(endptr, ptr, (U32)(size - (endptr - buffer)));
            endptr += dStrlen(endptr) - 1;
         }
         
         ptr = slash + 1;
      }
      else if(endptr)
      {
         // File
         
         catPath(endptr, ptr, (U32)(size - (endptr - buffer)));
         endptr += dStrlen(endptr) - 1;
      }
      
   } while(slash);
   
   return buffer;
}

bool isFullPath(const char *path)
{
   // Quick way out
   if(path[0] == '/' || path[1] == ':')
      return true;
   
   return false;
}

//-----------------------------------------------------------------------------

StringTableEntry makeRelativePathName(const char *path, const char *to)
{
   char buffer[1024];
   
   if(path[0] != '/' && path[1] != ':')
   {
      // It's already relative, bail
      return StringTable->insert(path);
   }
   
   // [tom, 12/13/2006] We need a trailing / for this to work, so add one if needed
   if(*(to + dStrlen(to) - 1) != '/')
   {
      dSprintf(buffer, sizeof(buffer), "%s/", to);
      to = StringTable->insert(buffer);
   }
   
   const char *pathPtr, *toPtr, *branch = path;
   char *bufPtr = buffer;
   
   // Find common part of path
   for(pathPtr = path, toPtr = to;*pathPtr && *toPtr && dTolower(*pathPtr) == dTolower(*toPtr);++pathPtr, ++toPtr)
   {
      if(*pathPtr == '/')
         branch = pathPtr;
   }
   
   if((*pathPtr == 0 || (*pathPtr == '/' && *(pathPtr + 1) == 0)) &&
      (*toPtr == 0 || (*toPtr == '/' && *(toPtr + 1) == 0)))
   {
      *bufPtr++ = '.';
      
      if(*pathPtr == '/' || *(pathPtr - 1) == '/')
         *bufPtr++ = '/';
      
      *bufPtr = 0;
      return StringTable->insert(buffer);
   }
   
   if((*pathPtr == 0 && *toPtr == '/') || (*toPtr == '/' && *pathPtr == 0))
      branch = pathPtr;
   
   // Figure out parent dirs
   for(toPtr = to + (branch - path);*toPtr;++toPtr)
   {
      if(*toPtr == '/' && *(toPtr + 1) != 0)
      {
         *bufPtr++ = '.';
         *bufPtr++ = '.';
         *bufPtr++ = '/';
      }
   }
   *bufPtr = 0;
   
   // Copy the rest
   if(*branch)
      dStrcpy(bufPtr, branch + 1);
   else
      *--bufPtr = 0;
   
   return StringTable->insert(buffer);
}

//-----------------------------------------------------------------------------

static StringTableEntry tryStripBasePath(const char *path, const char *base)
{
   U32 len = dStrlen(base);
   if(dStrnicmp(path, base, len) == 0)
   {
      if(*(path + len) == '/') ++len;
      return StringTable->insert(path + len, true);
   }
   return NULL;
}

StringTableEntry stripBasePath(const char *path)
{
   StringTableEntry str = NULL;
   
   str = tryStripBasePath( path, Platform::getMainDotCsDir() );
   
   if ( str )
      return str;
   
   str = tryStripBasePath( path, Platform::getCurrentDirectory() );
   
   if ( str )
      return str;
   
   str = tryStripBasePath( path, Platform::getPrefsPath() );
   
   if ( str )
      return str;
   
   
   return path;
}

//-----------------------------------------------------------------------------

StringTableEntry getPrefsPath(const char *file /* = NULL */)
{
   return "";
}

//-----------------------------------------------------------------------------

}



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


