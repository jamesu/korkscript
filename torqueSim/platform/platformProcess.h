#pragma once

#include <vector>

//------------------------------------------------------------------------------

namespace Platform
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
    void init();
    void process();
    void shutdown();
    void sleep(U32 ms);
    void restartInstance();
    void postQuitMessage(const U32 in_quitVal);
    void forceShutdown(S32 returnValue);

    /// User.
    StringTableEntry getUserHomeDirectory();
    StringTableEntry getUserDataDirectory();

    /// Date & Time.
    U32 getTime( void );
    U32 getVirtualMilliseconds( void );
    U32 getRealMilliseconds( void );
    void advanceTime(U32 delta);
    S32 getBackgroundSleepTime();
    void getLocalTime(LocalTime &);
    S32 compareFileTimes(const FileTime &a, const FileTime &b);

    /// Math.
    float getRandom();

    /// Debug.
    void outputDebugString(const char *string);
    void cprintf(const char* str);

    /// File IO.
    StringTableEntry getCurrentDirectory();
    bool setCurrentDirectory(StringTableEntry newDir);
    StringTableEntry getTemporaryDirectory();
    StringTableEntry getTemporaryFileName();
    StringTableEntry getExecutableName();
    StringTableEntry getExecutablePath(); 
    void setMainDotCsDir(const char *dir);
    StringTableEntry getMainDotCsDir();
    StringTableEntry getPrefsPath(const char *file = NULL);
    char *makeFullPathName(const char *path, char *buffer, U32 size, const char *cwd = NULL);
    StringTableEntry stripBasePath(const char *path);
    bool isFullPath(const char *path);
    StringTableEntry makeRelativePathName(const char *path, const char *to);
    bool dumpPath(const char *in_pBasePath, std::vector<FileInfo>& out_rFileVector, S32 recurseDepth = -1);
    bool dumpDirectories( const char *path, std::vector<StringTableEntry> &directoryVector, S32 depth = 0, bool noBasePath = false );
    bool hasSubDirectory( const char *pPath );
    bool getFileTimes(const char *filePath, FileTime *createTime, FileTime *modifyTime);
    bool isFile(const char *pFilePath);
    S32  getFileSize(const char *pFilePath);
    bool hasExtension(const char* pFilename, const char* pExtension);
    bool isDirectory(const char *pDirPath);
    bool isSubDirectory(const char *pParent, const char *pDir);
    void addExcludedDirectory(const char *pDir);
    void clearExcludedDirectories();
    bool isExcludedDirectory(const char *pDir);
    bool createPath(const char *path);
    bool deleteDirectory( const char* pPath );
    bool fileDelete(const char *name);
    bool fileRename(const char *oldName, const char *newName);
    bool fileTouch(const char *name);
    bool pathCopy(const char *fromName, const char *toName, bool nooverwrite = true);
    StringTableEntry osGetTemporaryDirectory();
};

