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

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#ifndef _TORQUECONFIG_H_
#include "core/torqueConfig.h"
#endif

#ifndef _TORQUE_TYPES_H_
#include "platform/types.h"
#endif

#ifndef _PLATFORMASSERT_H_
#include "platform/platformAssert.h"
#endif

#ifndef _PLATFORM_ENDIAN_H_
#include "platform/platformEndian.h"
#endif

#ifndef _PLATFORM_STRING_H_
#include "platform/platformString.h"
#endif

#ifndef _PLATFORM_MEMORY_H_
#include "platform/platformMemory.h"
#endif

#define PROFILE_START(a)
#define PROFILE_END()
#define PROFILE_SCOPE(a)


inline U32 getMin(U32 p1, U32 p2)
{
    return p1 < p2 ? p1 : p2;
}

inline U32 getMax(U32 p1, U32 p2)
{
    return p1 > p2 ? p1 : p2;
}

inline U32 getNextPow2(U32 io_num);

/// Determines if the given U32 is some 2^n
/// @returns true if in_num is a power of two, otherwise false
inline bool isPow2(const U32 in_num)
{
   return (in_num == getNextPow2(in_num));
}

// note: impl from T2D
inline U32 getNextPow2(U32 io_num)
{
   S32 oneCount   = 0;
   S32 shiftCount = -1;
   while (io_num) {
      if(io_num & 1)
         oneCount++;
      shiftCount++;
      io_num >>= 1;
   }
   if(oneCount > 1)
      shiftCount++;
   
   return U32(1 << shiftCount);
}

// note: impl from T2D
inline U32 getBinLog2(U32 io_num)
{
   //AssertFatal(io_num != 0 && isPow2(io_num) == true,
   //            "Error, this only works on powers of 2 > 0");
   
   S32 shiftCount = 0;
   while (io_num) {
      shiftCount++;
      io_num >>= 1;
   }
   
   return U32(shiftCount - 1);
}

//------------------------------------------------------------------------------

template <class T> class Vector;

//------------------------------------------------------------------------------

struct Platform
{
    struct LocalTime
    {
        U8  sec;        // seconds after minute (0-59)
        U8  min;        // Minutes after hour (0-59)
        U8  hour;       // Hours after midnight (0-23)
        U8  month;      // Month (0-11; 0=january)
        U8  monthday;   // Day of the month (1-31)
        U8  weekday;    // Day of the week (0-6, 6=sunday)
        U16 year;       // current year minus 1900
        U16 yearday;    // Day of year (0-365)
        bool isdst;     // true if daylight savings time is active
    };

    struct FileInfo
    {
        const char* pFullPath;
        const char* pFileName;
        U32 fileSize;

        bool equal( const FileInfo& fileInfo )
        {
            return
                fileInfo.pFullPath == pFullPath &&
                fileInfo.pFileName == pFileName &&
                fileInfo.fileSize == fileSize;
        }
    };

    struct VolumeInformation
    {
        StringTableEntry  RootPath;
        StringTableEntry  Name;
        StringTableEntry  FileSystem;
        U32               SerialNumber;
        U32               Type;
        bool              ReadOnly;
    };

    typedef void* FILE_HANDLE;
    enum DFILE_STATUS
    {
        DFILE_OK = 1
    };


    /// Application.
    static void init();
    static void initConsole();
    static void process();
    static void shutdown();
    static void sleep(U32 ms);
    static void restartInstance();
    static void postQuitMessage(const U32 in_quitVal);
    static void forceShutdown(S32 returnValue);

    /// User.
    static StringTableEntry getUserHomeDirectory();
    static StringTableEntry getUserDataDirectory();

    /// Date & Time.
    static U32 getTime( void );
    static U32 getVirtualMilliseconds( void );
    static U32 getRealMilliseconds( void );
    static void advanceTime(U32 delta);
    static S32 getBackgroundSleepTime();
    static void getLocalTime(LocalTime &);
    static S32 compareFileTimes(const FileTime &a, const FileTime &b);

    /// Math.
    static float getRandom();

    /// Debug.
    static void debugBreak();
    static void outputDebugString(const char *string);
    static void cprintf(const char* str);

    /// File IO.
    static StringTableEntry getCurrentDirectory();
    static bool setCurrentDirectory(StringTableEntry newDir);
    static StringTableEntry getTemporaryDirectory();
    static StringTableEntry getTemporaryFileName();
    static StringTableEntry getExecutableName();
    static StringTableEntry getExecutablePath(); 
    static void setMainDotCsDir(const char *dir);
    static StringTableEntry getMainDotCsDir();
    static StringTableEntry getPrefsPath(const char *file = NULL);
    static char *makeFullPathName(const char *path, char *buffer, U32 size, const char *cwd = NULL);
    static StringTableEntry stripBasePath(const char *path);
    static bool isFullPath(const char *path);
    static StringTableEntry makeRelativePathName(const char *path, const char *to);
    static bool dumpPath(const char *in_pBasePath, Vector<FileInfo>& out_rFileVector, S32 recurseDepth = -1);
    static bool dumpDirectories( const char *path, Vector<StringTableEntry> &directoryVector, S32 depth = 0, bool noBasePath = false );
    static bool hasSubDirectory( const char *pPath );
    static bool getFileTimes(const char *filePath, FileTime *createTime, FileTime *modifyTime);
    static bool isFile(const char *pFilePath);
    static S32  getFileSize(const char *pFilePath);
    static bool hasExtension(const char* pFilename, const char* pExtension);
    static bool isDirectory(const char *pDirPath);
    static bool isSubDirectory(const char *pParent, const char *pDir);
    static void addExcludedDirectory(const char *pDir);
    static void clearExcludedDirectories();
    static bool isExcludedDirectory(const char *pDir);
    static bool createPath(const char *path);
    static bool deleteDirectory( const char* pPath );
    static bool fileDelete(const char *name);
    static bool fileRename(const char *oldName, const char *newName);
    static bool fileTouch(const char *name);
    static bool pathCopy(const char *fromName, const char *toName, bool nooverwrite = true);
    static StringTableEntry osGetTemporaryDirectory();
};

#endif // _PLATFORM_H_
