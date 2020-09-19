#pragma once
#include "core/dataChunker.h"
#include "console/stringStack.h"
#include "core/fileStream.h"
#include "core/tVector.h"


class CodeBlockInternal;
class StringStack;
class ExprEvalState;
class Namespace;
class CodeBlock;

// Encapsulates most of the global con state
class CodeBlockWorld
{
public:

   enum
   {
      RootGroupID = 1,
      DataBlockGroupID = 2,
   };

   /// @group Codeblock
   /// {

   CodeBlock* smCodeBlockList;
   CodeBlock* smCurrentCodeBlock;

   StringTableEntry gCurrentFile;
   StringTableEntry gCurrentRoot;

   /// }

   /// @group Codeblock eval
   /// {

   enum {
        MaxActivePackages = 512,
   };

   ExprEvalState *gEvalState;
   bool gWarnUndefinedScriptVariables;

   char gScratchBuffer[4096];

#ifdef TORQUE_MULTITHREAD
ThreadIdent gMainThreadID;
#endif


	Vector<ConsumerCallback> gConsumers;
	DataChunker consoleLogChunker;
	Vector<ConsoleLogEntry> consoleLog;
	bool consoleLogLocked;
	bool logBufferEnabled;
	S32 printLevel;
	FileStream consoleLogFile;
	const char *defLogFileName;
	S32 consoleLogMode;
	bool active;
	bool newLogFile;
	const char *logFileName;

	enum { MaxCompletionBufferSize = 4096 };
	char completionBuffer[MaxCompletionBufferSize];
	char tabBuffer[MaxCompletionBufferSize];
	//static SimObjectPtr<SimObject> tabObject;
	U32 completionBaseStart;
	U32 completionBaseLen;


// Namespace
   U32 mNumActivePackages;
   U32 mOldNumActivePackages;
   StringTableEntry mActivePackages[MaxActivePackages];

   U32 mCacheSequence;
   DataChunker mCacheAllocator;
   DataChunker mAllocator;
   Namespace *mNamespaceList;
   Namespace *mGlobalNamespace;

   /// }


   /// @group User API
   /// {

   virtual ConsoleObject* lookupObject(const char* name) = 0;
   virtual ConsoleObject* lookupObject(const char* name, ConsoleObject* parent) = 0;
   virtual ConsoleObject* lookupObject(uint32_t id) = 0;
   virtual ConsoleObject* lookupObjectST(StringTableEntry name) = 0;
   
   /// }

   /// @group Core API
   /// {

   CodeBlockWorld();
   virtual ~CodeBlockWorld();
   

   /// @name Control Functions
   ///
   /// The console must be initialized and shutdown appropriately during the
   /// lifetime of the app. These functions are used to manage this behavior.
   ///
   /// @note Torque deals with this aspect of console management, so you don't need
   ///       to call these functions in normal usage of the engine.
   /// @{

   /// Initializes the console.
   ///
   /// This performs the following steps:
   ///   - Calls Namespace::init() to initialize the scripting namespace hierarchy.
   ///   - Calls ConsoleConstructor::setup() to initialize globally defined console
   ///     methods and functions.
   ///   - Registers some basic global script variables.
   ///   - Calls AbstractClassRep::init() to initialize Torque's class database.
   ///   - Registers some basic global script functions that couldn't usefully
   ///     be defined anywhere else.
   void init();

   /// Shuts down the console.
   ///
   /// This performs the following steps:
   ///   - Closes the console log file.
   ///   - Calls Namespace::shutdown() to shut down the scripting namespace hierarchy.
   void shutdown();

   /// Is the console active at this time?
   bool isActive();

   /// @}

   /// @name Console Consumers
   ///
   /// The console distributes its output through Torque by using
   /// consumers. Every time a new line is printed to the console,
   /// all the ConsumerCallbacks registered using addConsumer are
   /// called, in order.
   ///
   /// @note The GuiConsole control, which provides the on-screen
   ///       in-game console, uses a different technique to render
   ///       the console. It calls getLockLog() to lock the Vector
   ///       of on-screen console entries, then it renders them as
   ///       needed. While the Vector is locked, the console will
   ///       not change the Vector. When the GuiConsole control is
   ///       done with the console entries, it calls unlockLog()
   ///       to tell the console that it is again safe to modify
   ///       the Vector.
   ///
   /// @see TelnetConsole
   /// @see TelnetDebugger
   /// @see WinConsole
   /// @see MacCarbConsole
   /// @see iPhoneConsole
   /// @see StdConsole
   /// @see ConsoleLogger
   ///
   /// @{
   void addConsumer(ConsumerCallback cb);
   void removeConsumer(ConsumerCallback cb);
   /// @}

   /// @name Miscellaneous
   /// @{

   /// Remove color marking information from a string.
   ///
   /// @note It does this in-place, so be careful! It may
   ///       potentially blast data if you're not careful.
   ///       When in doubt, make a copy of the string first.
   void stripColorChars(char* line);


   /// Convert from a relative path to an absolute path.
   ///
   /// This is used in (among other places) the exec() script function, which
   /// takes a parameter indicating a script file and executes it. Script paths
   /// can be one of:
   ///      - <b>Absolute:</b> <i>fps/foo/bar.cs</i> Paths of this sort are passed
   ///        through.
   ///      - <b>Mod-relative:</b> <i>~/foo/bar.cs</i> Paths of this sort have their
   ///        replaced with the name of the current mod.
   ///      - <b>File-relative:</b> <i>./baz/blip.cs</i> Paths of this sort are
   ///        calculated relative to the path of the current scripting file.
   ///      - <b>Expando:</b> <i>^Project/image/happy.png</I> Paths of this sort are
   ///        relative to the path defined by the expando, in this case the "Project"
   ///        expando.
   ///
   /// @param  pDstPath    Pointer to string buffer to fill with absolute path.
   /// @param  size        Size of buffer pointed to by pDstPath.
   /// @param  pSrcPath    Original, possibly relative path.
   bool expandPath( char* pDstPath, U32 size, const char* pSrcPath, const char* pWorkingDirectoryHint = NULL, const bool ensureTrailingSlash = false );
   void collapsePath( char* pDstPath, U32 size, const char* pSrcPath, const char* pWorkingDirectoryHint = NULL );
   bool isBasePath( const char* SrcPath, const char* pBasePath );
   void ensureTrailingSlash( char* pDstPath, const char* pSrcPath );
   bool stripRepeatSlashes( char* pDstPath, const char* pSrcPath, S32 dstSize );
   
   void addPathExpando( const char* pExpandoName, const char* pPath );
   void removePathExpando( const char* pExpandoName );
   bool isPathExpando( const char* pExpandoName );
   StringTableEntry getPathExpando( const char* pExpandoName );
   U32 getPathExpandoCount( void );
   StringTableEntry getPathExpandoKey( U32 expandoIndex );
   StringTableEntry getPathExpandoValue( U32 expandoIndex );

   // tgemit - this stuff is from T3D
   /// Convert from a relative script path to an absolute script path.
   ///
   /// This is used in (among other places) the exec() script function, which
   /// takes a parameter indicating a script file and executes it. Script paths
   /// can be one of:
   ///      - <b>Absolute:</b> <i>fps/foo/bar.cs</i> Paths of this sort are passed
   ///        through.
   ///      - <b>Mod-relative:</b> <i>~/foo/bar.cs</i> Paths of this sort have their
   ///        replaced with the name of the current mod.
   ///      - <b>File-relative:</b> <i>./baz/blip.cs</i> Paths of this sort are
   ///        calculated relative to the path of the current scripting file.
   ///
   /// @note This function determines paths relative to the currently executing
   ///       CodeBlock. Calling it outside of script execution will result in
   ///       it directly copying src to filename, since it won't know to what the
   ///       path is relative!
   ///
   /// @param  filename    Pointer to string buffer to fill with absolute path.
   /// @param  size        Size of buffer pointed to by filename.
   /// @param  src         Original, possibly relative script path.
   bool expandScriptFilename(char *filename, U32 size, const char *src);
   //

   StringTableEntry getModNameFromPath(const char *path);

   /// Returns true if fn is a global scripting function.
   ///
   /// This looks in the global namespace. It also checks to see if fn
   /// is in the StringTable; if not, it returns false.
   bool isFunction(const char *fn);

   /// This is the basis for tab completion in the console.
   ///
   /// @note This is an internally used function. You probably don't
   ///       care much about how this works.
   ///
   /// This function does some basic parsing to try to ascertain the namespace in which
   /// we are attempting to do tab completion, then bumps control off to the appropriate
   /// tabComplete function, either in ConsoleObject or Namespace.
   ///
   /// @param  inputBuffer     Pointer to buffer containing starting data, or last result.
   /// @param  cursorPos       Location of cursor in this buffer. This is used to indicate
   ///                         what part of the string should be kept and what part should
   ///                         be advanced to the next match if any.
   /// @param  maxResultLength Maximum amount of result data to put into inputBuffer. This
   ///                         is capped by MaxCompletionBufferSize.
   /// @param  forwardTab      Should we go forward to next match or backwards to previous
   ///                         match? True indicates forward.
   U32 tabComplete(char* inputBuffer, U32 cursorPos, U32 maxResultLength, bool forwardTab);

   /// @}


   /// @name Variable Management
   /// @{

   /// Add a console variable that references the value of a variable in C++ code.
   ///
   /// If a value is assigned to the console variable the C++ variable is updated,
   /// and vice-versa.
   ///
   /// @param name    Global console variable name to create
   /// @param type    The type of the C++ variable; see the ConsoleDynamicTypes enum for a complete list.
   /// @param pointer Pointer to the variable.
   /// @see ConsoleDynamicTypes
   bool addVariable(const char *name, S32 type, void *pointer, const char* desc = NULL);

   /// Remove a console variable.
   ///
   /// @param name   Global console variable name to remove
   /// @return       true if variable existed before removal.
   bool removeVariable(const char *name);

   /// Assign a string value to a locally scoped console variable
   ///
   /// @note The context of the variable is determined by gEvalState; that is,
   ///       by the currently executing code.
   ///
   /// @param name   Local console variable name to set
   /// @param value  String value to assign to name
   void setLocalVariable(const char *name, const char *value);

   /// Retrieve the string value to a locally scoped console variable
   ///
   /// @note The context of the variable is determined by gEvalState; that is,
   ///       by the currently executing code.
   ///
   /// @param name   Local console variable name to get
   const char* getLocalVariable(const char* name);

   /// @}

   /// @name Global Variable Accessors
   /// @{
   /// Assign a string value to a global console variable
   /// @param name   Global console variable name to set
   /// @param value  String value to assign to this variable.
   void setVariable(const char *name, const char *value);

   /// Retrieve the string value of a global console variable
   /// @param name   Global Console variable name to query
   /// @return       The string value of the variable or "" if the variable does not exist.
   const char* getVariable(const char* name);

   /// Same as setVariable(), but for bools.
   void setBoolVariable (const char* name,bool var);

   /// Same as getVariable(), but for bools.
   ///
   /// @param  name  Name of the variable.
   /// @param  def   Default value to supply if no matching variable is found.
   bool getBoolVariable (const char* name,bool def = false);

   /// Same as setVariable(), but for ints.
   void setIntVariable  (const char* name,S32 var);

   /// Same as getVariable(), but for ints.
   ///
   /// @param  name  Name of the variable.
   /// @param  def   Default value to supply if no matching variable is found.
   S32  getIntVariable  (const char* name,S32 def = 0);

   /// Same as setVariable(), but for floats.
   void setFloatVariable(const char* name,F32 var);

   /// Same as getVariable(), but for floats.
   ///
   /// @param  name  Name of the variable.
   /// @param  def   Default value to supply if no matching variable is found.
   F32  getFloatVariable(const char* name,F32 def = .0f);

   /// @}

   /// @name Global Function Registration
   /// @{

   /// Register a C++ function with the console making it a global function callable from the scripting engine.
   ///
   /// @param name      Name of the new function.
   /// @param cb        Pointer to the function implementing the scripting call; a console callback function returning a specific type value.
   /// @param usage     Documentation for this function. @ref console_autodoc
   /// @param minArgs   Minimum number of arguments this function accepts
   /// @param maxArgs   Maximum number of arguments this function accepts
   void addCommand(const char *name, StringCallback cb, const char *usage, S32 minArgs, S32 maxArgs);

   void addCommand(const char *name, IntCallback    cb,    const char *usage, S32 minArgs, S32 maxArg); ///< @copydoc addCommand(const char *, StringCallback, const char *, S32, S32)
   void addCommand(const char *name, FloatCallback  cb,  const char *usage, S32 minArgs, S32 maxArgs); ///< @copydoc addCommand(const char *, StringCallback, const char *, S32, S32)
   void addCommand(const char *name, VoidCallback   cb,   const char *usage, S32 minArgs, S32 maxArgs); ///< @copydoc addCommand(const char *, StringCallback, const char *, S32, S32)
   void addCommand(const char *name, BoolCallback   cb,   const char *usage, S32 minArgs, S32 maxArgs); ///< @copydoc addCommand(const char *, StringCallback, const char *, S32, S32)
   /// @}

   /// @name Namespace Function Registration
   /// @{

   /// Register a C++ function with the console making it callable
   /// as a method of the given namespace from the scripting engine.
   ///
   /// @param nameSpace Name of the namespace to associate the new function with; this is usually the name of a class.
   /// @param name      Name of the new function.
   /// @param cb        Pointer to the function implementing the scripting call; a console callback function returning a specific type value.
   /// @param usage     Documentation for this function. @ref console_autodoc
   /// @param minArgs   Minimum number of arguments this function accepts
   /// @param maxArgs   Maximum number of arguments this function accepts
   void addCommand(const char *nameSpace, const char *name,StringCallback cb, const char *usage, S32 minArgs, S32 maxArgs);
   void addCommand(const char *nameSpace, const char *name,IntCallback cb,    const char *usage, S32 minArgs, S32 maxArgs); ///< @copydoc addCommand(const char*, const char *, StringCallback, const char *, S32, S32)
   void addCommand(const char *nameSpace, const char *name,FloatCallback cb,  const char *usage, S32 minArgs, S32 maxArgs); ///< @copydoc addCommand(const char*, const char *, StringCallback, const char *, S32, S32)
   void addCommand(const char *nameSpace, const char *name,VoidCallback cb,   const char *usage, S32 minArgs, S32 maxArgs); ///< @copydoc addCommand(const char*, const char *, StringCallback, const char *, S32, S32)
   void addCommand(const char *nameSpace, const char *name,BoolCallback cb,   const char *usage, S32 minArgs, S32 maxArgs); ///< @copydoc addCommand(const char*, const char *, StringCallback, const char *, S32, S32)
   /// @}

   /// @name Special Purpose Registration
   ///
   /// These are special-purpose functions that exist to allow commands to be grouped, so
   /// that when we generate console docs, they can be more meaningfully presented.
   ///
   /// @ref console_autodoc "Click here for more information about console docs and grouping."
   ///
   /// @{

   void markCommandGroup (const char * nsName, const char *name, const char* usage=NULL);
   void beginCommandGroup(const char * nsName, const char *name, const char* usage);
   void endCommandGroup  (const char * nsName, const char *name);

   /// @deprecated
   void addOverload      (const char * nsName, const char *name, const char *altUsage);

   /// @}

   /// @name Console Output
   ///
   /// These functions process the formatted string and pass it to all the ConsumerCallbacks that are
   /// currently registered. The console log file and the console window callbacks are installed by default.
   ///
   /// @see addConsumer()
   /// @see removeConsumer()
   /// @{

   void log(const char *string);

   /// @param _format   A stdlib printf style formatted out put string
   /// @param ...       Variables to be written
   void printf(const char *_format, ...);

   /// @note The console window colors warning text as LIGHT GRAY.
   /// @param _format   A stdlib printf style formatted out put string
   /// @param ...       Variables to be written
   void warnf(const char *_format, ...);

   /// @note The console window colors warning text as RED.
   /// @param _format   A stdlib printf style formatted out put string
   /// @param ...       Variables to be written
   void errorf(const char *_format, ...);

   /// @note The console window colors warning text as LIGHT GRAY.
   /// @param type      Allows you to associate the warning message with an internal module.
   /// @param _format   A stdlib printf style formatted out put string
   /// @param ...       Variables to be written
   /// @see Con::warnf()
   void warnf(ConsoleLogEntry::Type type, const char *_format, ...);

   /// @note The console window colors warning text as RED.
   /// @param type      Allows you to associate the warning message with an internal module.
   /// @param _format   A stdlib printf style formatted out put string
   /// @param ...       Variables to be written
   /// @see Con::errorf()
   void errorf(ConsoleLogEntry::Type type, const char *_format, ...);

   /// clear the console log
   void cls( void );

   /// Prints a separator to the console.
   inline void printSeparator( void ) { printf("--------------------------------------------------------------------------------"); }

   /// Prints a separator to the console.
   inline void printBlankLine( void ) { printf(""); }

   /// @}

   /// Returns true when called from the main thread, false otherwise
   bool isMainThread();


   /// @name Console Execution
   ///
   /// These are functions relating to the execution of script code.
   ///
   /// @{

   /// Call a script function from C/C++ code.
   ///
   /// @param argc      Number of elements in the argv parameter
   /// @param argv      A character string array containing the name of the function
   ///                  to call followed by the arguments to that function.
   /// @code
   /// // Call a Torque script function called mAbs, having one parameter.
   /// char* argv[] = {"abs", "-9"};
   /// char* result = execute(2, argv);
   /// @endcode
   const char *execute(S32 argc, const char* argv[]);

   /// @see execute(S32 argc, const char* argv[])
   const char *executef(S32 argc, ...);

   /// Call a Torque Script member function of a ConsoleObject from C/C++ code.
   /// @param object    Object on which to execute the method call.
   /// @param argc      Number of elements in the argv parameter (must be >2, see argv)
   /// @param argv      A character string array containing the name of the member function
   ///                  to call followed by an empty parameter (gets filled with object ID)
   ///                  followed by arguments to that function.
   /// @code
   /// // Call the method setMode() on an object, passing it one parameter.
   ///
   /// char* argv[] = {"setMode", "", "2"};
   /// char* result = execute(myConsoleObject, 3, argv);
   /// @endcode
   // [neo, 5/10/2007 - #3010]
   // Added flag thisCallOnly to bypass dynamic method calls
   const char *execute(ConsoleObject *object, S32 argc, const char *argv[], bool thisCallOnly = false);

   /// @see execute(ConsoleObject *, S32 argc, const char *argv[])
   const char *executef(ConsoleObject *, S32 argc, ...);

   /// Evaluate an arbitrary chunk of code.
   ///
   /// @param  string   Buffer containing code to execute.
   /// @param  echo     Should we echo the string to the console?
   /// @param  fileName Indicate what file this code is coming from; used in error reporting and such.
   const char *evaluate(const char* string, bool echo = false, const char *fileName = NULL);

   /// Evaluate an arbitrary line of script.
   ///
   /// This wraps dVsprintf(), so you can substitute parameters into the code being executed.
   const char *evaluatef(const char* string, ...);

   /// @}

   /// @name Console Function Implementation Helpers
   ///
   /// The functions Con::getIntArg, Con::getFloatArg and Con::getArgBuffer(size) are used to
   /// allocate on the console stack string variables that will be passed into the next console
   //  function called.  This allows the console to avoid copying some data.
   ///
   /// getReturnBuffer lets you allocate stack space to return data in.
   /// @{

   ///
   char *getReturnBuffer(U32 bufferSize);
   char *getReturnBuffer(const char *stringToCopy);

   char *getArgBuffer(U32 bufferSize);
   char *getFloatArg(F64 arg);
   char *getIntArg  (S32 arg);
   char* getBoolArg(bool arg);
   char *getStringArg( const char *arg );
   /// @}

   void _printf(ConsoleLogEntry::Level level, ConsoleLogEntry::Type type, const char* fmt);

   /// @name Namespaces
   /// @{

   void trashNSCache();

   Namespace* find(StringTableEntry name, StringTableEntry package=NULL);
   bool canTabComplete(const char *prevText, const char *bestMatch, const char *newText, S32 baseLen, bool fForward);

   void activatePackage(StringTableEntry name);
   void deactivatePackage(StringTableEntry name);
   void dumpClasses( bool dumpScript = true, bool dumpEngine = true );
   void dumpFunctions( bool dumpScript = true, bool dumpEngine = true );
   void printNamespaceEntries(Namespace * g, bool dumpScript = true, bool dumpEngine = true);
   void unlinkPackages();
   void relinkPackages();
   bool isPackage(StringTableEntry name);

   Namespace *lookupNamespace(const char *nsName);
   bool linkNamespaces(const char *parentName, const char *childName);
   bool unlinkNamespaces(const char *parentName, const char *childName);

   /// @note This should only be called from consoleObject.h
   bool classLinkNamespaces(Namespace *parent, Namespace *child);

   const char *getNamespaceList(Namespace *ns);

   CodeBlock* findCodeBlock(StringTableEntry name);
   
   /// @}

   /// @name Logging
   /// @{

   void getLockLog(ConsoleLogEntry * &log, U32 &size);
   void unlockLog(void);
   void setLogMode(S32 mode);

   /// @}

   /// @name Dynamic Type System
   /// @{

   void setData(S32 type, void *dptr, S32 index, S32 argc, const char **argv, const EnumTable *tbl = NULL, BitSet32 flag = 0);
   const char *getData(S32 type, void *dptr, S32 index, const EnumTable *tbl = NULL, BitSet32 flag = 0);
   /// @}

   S32 dbgGetCurrentFrame(void);

const char * prependDollar ( const char * name );
const char * prependPercent ( const char * name );

   /// }

};