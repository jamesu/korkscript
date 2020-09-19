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

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#ifndef _PLATFORM_H_
#include "platform/platform.h"
#endif
#ifndef _BITSET_H_
#include "core/bitSet.h"
#endif
#include <stdarg.h>

class ConsoleObject;
class CodeBlockWorld;
struct EnumTable;
class Namespace;

/// Indicates that warnings about undefined script variables should be displayed.
///
/// @note This is set and controlled by script.
extern bool gWarnUndefinedScriptVariables;

enum StringTableConstants
{
   StringTagPrefixByte = 0x01 ///< Magic value prefixed to tagged strings.
};

/// Represents an entry in the log.
struct ConsoleLogEntry
{
   /// This field indicates the severity of the log entry.
   ///
   /// Log entries are filtered and displayed differently based on
   /// their severity. Errors are highlighted red, while normal entries
   /// are displayed as normal text. Often times, the engine will be
   /// configured to hide all log entries except warnings or errors,
   /// or to perform a special notification when it encounters an error.
   enum Level
   {
      Normal = 0,
      Warning,
      Error,
      NUM_CLASS
   } mLevel;

   /// Used to associate a log entry with a module.
   ///
   /// Log entries can come from different sources; for instance,
   /// the scripting engine, or the network code. This allows the
   /// logging system to be aware of where different log entries
   /// originated from.
   enum Type
   {
      General = 0,
      Assert,
      Script,
      GUI,
      Network,
      NUM_TYPE
   } mType;

   /// Indicates the actual log entry.
   ///
   /// This contains a description of the event being logged.
   /// For instance, "unable to access file", or "player connected
   /// successfully", or nearly anything else you might imagine.
   ///
   /// Typically, the description should contain a concise, descriptive
   /// string describing whatever is being logged. Whenever possible,
   /// include useful details like the name of the file being accessed,
   /// or the id of the player or GuiControl, so that if a log needs
   /// to be used to locate a bug, it can be done as painlessly as
   /// possible.
   const char *mString;
};

/// Scripting engine representation of an enum.
///
/// This data structure is used by the scripting engine
/// to expose enumerations to the scripting language. It
/// acts to relate named constants to integer values, just
/// like an enum in C++.
struct EnumTable
{
   /// Number of enumerated items in the table.
   S32 size;

   /// This represents a specific item in the enumeration.
   struct Enums
   {
      S32 index;        ///< Index label maps to.
      const char *label;///< Label for this index.
   };

   Enums *table;

   /// Constructor.
   ///
   /// This sets up the EnumTable with predefined data.
   ///
   /// @param sSize  Size of the table.
   /// @param sTable Pointer to table of Enums.
   ///
   /// @see gLiquidTypeTable
   /// @see gAlignTable
   EnumTable(S32 sSize, Enums *sTable)
      { size = sSize; table = sTable; }
};

typedef const char *StringTableEntry;

/// @defgroup tsScripting TorqueScript Bindings
/// TorqueScrit bindings

/// @defgroup console_callbacks Scripting Engine Callbacks
/// @ingroup tsScripting
///
/// The scripting engine makes heavy use of callbacks to represent
/// function exposed to the scripting language. StringCallback,
/// IntCallback, FloatCallback, VoidCallback, and BoolCallback all
/// represent exposed script functions returning different types.
///
/// ConsumerCallback is used with the function Con::addConsumer; functions
/// registered with Con::addConsumer are called whenever something is outputted
/// to the console. For instance, the TelnetConsole registers itself with the
/// console so it can echo the console over the network.
///
/// @note Callbacks to the scripting language - for instance, onExit(), which is
///       a script function called when the engine is shutting down - are handled
///       using Con::executef() and kin.
/// @{

///
typedef const char * (*StringCallback)(CodeBlockWorld* con, ConsoleObject *obj, S32 argc, const char *argv[]);
typedef S32             (*IntCallback)(CodeBlockWorld* con, ConsoleObject *obj, S32 argc, const char *argv[]);
typedef F32           (*FloatCallback)(CodeBlockWorld* con, ConsoleObject *obj, S32 argc, const char *argv[]);
typedef void           (*VoidCallback)(CodeBlockWorld* con, ConsoleObject *obj, S32 argc, const char *argv[]); // We have it return a value so things don't break..
typedef bool           (*BoolCallback)(CodeBlockWorld* con, ConsoleObject *obj, S32 argc, const char *argv[]);

typedef void (*ConsumerCallback)(ConsoleLogEntry::Level level, const char *consoleLine);
/// @}

/// @defgroup console_types Scripting Engine Type Functions
/// @ingroup tsScripting
///
/// @see Con::registerType
/// @{
typedef const char* (*GetDataFunction)(CodeBlockWorld* con, void *dptr, EnumTable *tbl, BitSet32 flag);
typedef void        (*SetDataFunction)(CodeBlockWorld* con, void *dptr, S32 argc, const char **argv, EnumTable *tbl, BitSet32 flag);
/// @}

/// This namespace contains the core of the console functionality.
///
/// @section con_intro Introduction
///
/// The console is a key part of Torque's architecture. It allows direct run-time control
/// of many aspects of the engine.
///
/// @nosubgrouping
namespace Con
{
   /// Various configuration constants.
   enum Constants 
   {
      /// This is the version number associated with DSO files.
      ///
      /// If you make any changes to the way the scripting language works
      /// (such as DSO format changes, adding/removing op-codes) that would
      /// break compatibility, then you should increment this.
      ///
      /// If you make a really major change, increment it to the next multiple
      /// of ten.
      ///
      /// 12/29/04 - BJG - 33->34 Removed some opcodes, part of namespace upgrade.
      /// 12/30/04 - BJG - 34->35 Reordered some things, further general shuffling.
      /// 11/03/05 - BJG - 35->36 Integrated new debugger code.
      //  09/08/06 - THB - 36->37 New opcode for internal names
      //  09/15/06 - THB - 37->38 Added unit conversions
      //  11/23/06 - THB - 38->39 Added recursive internal name operator
      //  02/15/07 - THB - 39->40 Bumping to 40 for TGB since the console has been majorly hacked without the version number being bumped
      //  02/16/07 - THB - 40->41 newmsg operator
      //  02/16/07 - PAUP - 41->42 DSOs are read with a pointer before every string(ASTnodes changed). Namespace and HashTable revamped
      //  05/17/10 - Luma - 42-43 Adding proper sceneObject physics flags, fixes in general
      //  02/07/13 - JU   - 43->44 Expanded the width of stringtable entries to  64bits 
      //  tgemit - 77 set for now just to make it distinct
      DSOVersion = 77,
      MaxLineLength = 512,  ///< Maximum length of a line of console input.
      MaxDataTypes = 256    ///< Maximum number of registered data types.
   };

   // Con:: stuff moved to CodeBlockWorld

};

extern void expandEscape(char *dest, const char *src);
extern bool collapseEscape(char *buf);
extern U32 HashPointer(StringTableEntry ptr);

/// This is the backend for the ConsoleMethod()/ConsoleFunction() macros.
///
/// See the group ConsoleConstructor Innards for specifics on how this works.
///
/// @see @ref console_autodoc
/// @nosubgrouping
class ConsoleConstructor
{
public:
   /// @name Entry Type Fields
   ///
   /// One of these is set based on the type of entry we want
   /// inserted in the console.
   ///
   /// @ref console_autodoc
   /// @{
   StringCallback sc;   ///< A function/method that returns a string.
   IntCallback ic;      ///< A function/method that returns an int.
   FloatCallback fc;    ///< A function/method that returns a float.
   VoidCallback vc;     ///< A function/method that returns nothing.
   BoolCallback bc;     ///< A function/method that returns a bool.
   bool group;          ///< Indicates that this is a group marker.
   bool overload;       ///< Indicates that this is an overload marker.
   bool ns;             ///< Indicates that this is a namespace marker.
                        ///  @deprecated Unused.
   /// @}

   /// Minimum/maximum number of arguments for the function.
   S32 mina, maxa;
   const char *usage;         ///< Usage string.
   const char *funcName;      ///< Function name.
   const char *className;     ///< Class name.

   /// @name ConsoleConstructer Innards
   ///
   /// The ConsoleConstructor class is used as the backend for the ConsoleFunction() and
   /// ConsoleMethod() macros. The way it works takes advantage of several properties of
   /// C++.
   ///
   /// The ConsoleFunction()/ConsoleMethod() macros wrap the declaration of a ConsoleConstructor.
   ///
   /// @code
   ///      // The definition of a ConsoleFunction using the macro
   ///      ConsoleFunction(ExpandPath, const char*, 2, 2, "(string filePath)")
   ///      {
   ///         argc;
   ///         char* ret = Con::getReturnBuffer( 1024 );
   ///         Con::expandPath(ret, 1024, argv[1]);
   ///         return ret;
   ///      }
   ///
   ///      // Resulting code
   ///      static const char* cExpandPath(ConsoleObject *, S32, const char **argv);
   ///      static ConsoleConstructor
   ///            gExpandPathobj(NULL,"ExpandPath", cExpandPath,
   ///            "(string filePath)", 2, 2);
   ///      static const char* cExpandPath(ConsoleObject *, S32 argc, const char **argv)
   ///      {
   ///         argc;
   ///         char* ret = Con::getReturnBuffer( 1024 );
   ///         Con::expandPath(ret, 1024, argv[1]);
   ///         return ret;
   ///      }
   ///
   ///      // A similar thing happens when you do a ConsoleMethod.
   /// @endcode
   ///
   /// As you can see, several global items are defined when you use the ConsoleFunction method.
   /// The macro constructs the name of these items from the parameters you passed it. Your
   /// implementation of the console function is is placed in a function with a name based on
   /// the actual name of the console funnction. In addition, a ConsoleConstructor is declared.
   ///
   /// Because it is defined as a global, the constructor for the ConsoleConstructor is called
   /// before execution of main() is started. The constructor is called once for each global
   /// ConsoleConstructor variable, in the order in which they were defined (this property only holds true
   /// within file scope).
   ///
   /// We have ConsoleConstructor create a linked list at constructor time, by storing a static
   /// pointer to the head of the list, and keeping a pointer to the next item in each instance
   /// of ConsoleConstructor. init() is a helper function in this process, automatically filling
   /// in commonly used fields and updating first and next as needed. In this way, a list of
   /// items to add to the console is assemble in memory, ready for use, before we start
   /// execution of the program proper.
   ///
   /// In Con::init(), ConsoleConstructor::setup() is called to process this prepared list. Each
   /// item in the list is iterated over, and the appropriate Con namespace functions (usually
   /// Con::addCommand) are invoked to register the ConsoleFunctions and ConsoleMethods in
   /// the appropriate namespaces.
   ///
   /// @see Namespace
   /// @see Con
   /// @{

   ConsoleConstructor *next;
   static ConsoleConstructor *first;

   void init(const char *cName, const char *fName, const char *usg, S32 minArgs, S32 maxArgs);
   static void setup(CodeBlockWorld* con);
   /// @}

   /// @name Basic Console Constructors
   /// @{

   ConsoleConstructor(const char *className, const char *funcName, StringCallback sfunc, const char* usage,  S32 minArgs, S32 maxArgs);
   ConsoleConstructor(const char *className, const char *funcName, IntCallback    ifunc, const char* usage,  S32 minArgs, S32 maxArgs);
   ConsoleConstructor(const char *className, const char *funcName, FloatCallback  ffunc, const char* usage,  S32 minArgs, S32 maxArgs);
   ConsoleConstructor(const char *className, const char *funcName, VoidCallback   vfunc, const char* usage,  S32 minArgs, S32 maxArgs);
   ConsoleConstructor(const char *className, const char *funcName, BoolCallback   bfunc, const char* usage,  S32 minArgs, S32 maxArgs);
   /// @}

   /// @name Magic Console Constructors
   ///
   /// These perform various pieces of "magic" related to consoleDoc functionality.
   /// @ref console_autodoc
   /// @{

   /// Indicates a group marker. (A doxygen illusion)
   ///
   /// @see Con::markCommandGroup
   /// @ref console_autodoc
   ConsoleConstructor(const char *className, const char *groupName, const char* usage);

   /// Indicates a namespace usage string.
   ConsoleConstructor(const char *className, const char *usage);

   /// @}
};

/// @name Global Console Definition Macros
///
/// @note If TORQUE_DEBUG is defined, then we gather documentation information, and
///       do some extra sanity checks.
///
/// @see ConsoleConstructor
/// @ref console_autodoc
/// @{

#define ConsoleDocClass(klass, doc)

// O hackery of hackeries
#define conmethod_return_const              return (const
#define conmethod_return_S32                return (S32
#define conmethod_return_F32                return (F32
#define conmethod_nullify(val)
#define conmethod_return_void               conmethod_nullify(void
#define conmethod_return_bool               return (bool
#define conmethod_return_ConsoleInt         conmethod_return_S32
#define conmethod_return_ConsoleFloat       conmethod_return_F32
#define conmethod_return_ConsoleVoid        conmethod_return_void
#define conmethod_return_ConsoleBool        conmethod_return_bool
#define conmethod_return_ConsoleString    conmethod_return_const char*

#if !defined(TORQUE_SHIPPING)

// Console function return types
#define ConsoleString   const char*
#define ConsoleInt      S32
#define ConsoleFloat F32
#define ConsoleVoid     void
#define ConsoleBool     bool

class CodeBlockWorld;

// Console function macros
#  define ConsoleFunctionGroupBegin(groupName, usage) \
      static ConsoleConstructor gConsoleFunctionGroup##groupName##__GroupBegin(NULL,#groupName,usage);

#  define ConsoleFunction(name,returnType,minArgs,maxArgs,usage1)                         \
      static returnType c##name(CodeBlockWorld*, ConsoleObject *, S32, const char **argv);                     \
      static ConsoleConstructor g##name##obj(NULL,#name,c##name,usage1,minArgs,maxArgs);  \
      static returnType c##name(CodeBlockWorld* con, ConsoleObject *, S32 argc, const char **argv)

#  define ConsoleFunctionWithDocs(name,returnType,minArgs,maxArgs,argString)              \
      static returnType c##name(CodeBlockWorld*, ConsoleObject *, S32, const char **argv);                     \
     static ConsoleConstructor g##name##obj(NULL,#name,c##name,#argString,minArgs,maxArgs);      \
      static returnType c##name(CodeBlockWorld* con, ConsoleObject *, S32 argc, const char **argv)

#  define ConsoleFunctionGroupEnd(groupName) \
      static ConsoleConstructor gConsoleFunctionGroup##groupName##__GroupEnd(NULL,#groupName,NULL);

// Console method macros
#  define ConsoleNamespace(className, usage) \
      static ConsoleConstructor className##__Namespace(#className, usage);

#  define ConsoleMethodGroupBegin(className, groupName, usage) \
      static ConsoleConstructor className##groupName##__GroupBegin(#className,#groupName,usage);

// note: we would want to expand the following macro into (Doxygen) comments!
// we can not do that with a macro.  these are here just as a reminder until completion
#  define ConsoleMethodRootGroupBeginWithDocs(className)
#  define ConsoleMethodGroupBeginWithDocs(className, superclassName)

#  define ConsoleMethod(className,name,returnType,minArgs,maxArgs,usage1)                                                 \
      static inline returnType c##className##name(CodeBlockWorld*, className *, S32, const char **argv);                                   \
      static returnType c##className##name##caster(CodeBlockWorld* con, ConsoleObject *object, S32 argc, const char **argv) {                      \
         AssertFatal( dynamic_cast<className*>( object ), "Object passed to " #name " is not a " #className "!" );        \
         conmethod_return_##returnType ) c##className##name(con, static_cast<className*>(object),argc,argv);                   \
      };                                                                                                                  \
      static ConsoleConstructor className##name##obj(#className,#name,c##className##name##caster,usage1,minArgs,maxArgs); \
      static inline returnType c##className##name(CodeBlockWorld* con, className *object, S32 argc, const char **argv)

#  define ConsoleMethodWithDoc(className,name,returnType,minArgs,maxArgs,usage1,desc)                                                 \
      static inline returnType c##className##name(CodeBlockWorld*, className *, S32, const char **argv);                                   \
      static returnType c##className##name##caster(CodeBlockWorld* con, ConsoleObject *object, S32 argc, const char **argv) {                      \
         AssertFatal( dynamic_cast<className*>( object ), "Object passed to " #name " is not a " #className "!" );        \
         conmethod_return_##returnType ) c##className##name(con, static_cast<className*>(object),argc,argv);                   \
      };                                                                                                                  \
      static ConsoleConstructor className##name##obj(#className,#name,c##className##name##caster,usage1,minArgs,maxArgs); \
      static inline returnType c##className##name(CodeBlockWorld* con, className *object, S32 argc, const char **argv)

#  define ConsoleMethodWithDocs(className,name,returnType,minArgs,maxArgs,argString)                                  \
      static inline returnType c##className##name(CodeBlockWorld*, className *, S32, const char **argv);                               \
      static returnType c##className##name##caster(CodeBlockWorld* con, ConsoleObject *object, S32 argc, const char **argv) {                  \
         AssertFatal( dynamic_cast<className*>( object ), "Object passed to " #name " is not a " #className "!" );    \
         conmethod_return_##returnType ) c##className##name(con, static_cast<className*>(object),argc,argv);               \
      };                                                                                                              \
     static ConsoleConstructor className##name##obj(#className,#name,c##className##name##caster,#argString,minArgs,maxArgs); \
      static inline returnType c##className##name(CodeBlockWorld* con, className *object, S32 argc, const char **argv)

#  define ConsoleStaticMethod(className,name,returnType,minArgs,maxArgs,usage1)                       \
      static inline returnType c##className##name(CodeBlockWorld*, S32, const char **);                                \
      static returnType c##className##name##caster(CodeBlockWorld* con, ConsoleObject *object, S32 argc, const char **argv) {  \
         conmethod_return_##returnType ) c##className##name(con, argc,argv);                               \
      };                                                                                              \
      static ConsoleConstructor                                                                       \
         className##name##obj(#className,#name,c##className##name##caster,usage1,minArgs,maxArgs);    \
      static inline returnType c##className##name(CodeBlockWorld* con, S32 argc, const char **argv)

#  define ConsoleStaticMethodWithDocs(className,name,returnType,minArgs,maxArgs,argString)            \
      static inline returnType c##className##name(CodeBlockWorld*, S32, const char **);                                \
      static returnType c##className##name##caster(CodeBlockWorld* con, ConsoleObject *object, S32 argc, const char **argv) {  \
         conmethod_return_##returnType ) c##className##name(con, argc,argv);                               \
      };                                                                                              \
      static ConsoleConstructor                                                                       \
     className##name##obj(#className,#name,c##className##name##caster,#argString,minArgs,maxArgs);        \
      static inline returnType c##className##name(CodeBlockWorld* con, S32 argc, const char **argv)

#  define ConsoleMethodGroupEnd(className, groupName) \
      static ConsoleConstructor className##groupName##__GroupEnd(#className,#groupName,NULL);

#  define ConsoleMethodRootGroupEndWithDocs(className)
#  define ConsoleMethodGroupEndWithDocs(className)

#else

// These do nothing if we don't want doc information.
#  define ConsoleFunctionGroupBegin(groupName, usage)
#  define ConsoleFunctionGroupEnd(groupName)
#  define ConsoleNamespace(className, usage)
#  define ConsoleMethodGroupBegin(className, groupName, usage)
#  define ConsoleMethodGroupEnd(className, groupName)

// These are identical to what's above, we just want to null out the usage strings.
#  define ConsoleFunction(name,returnType,minArgs,maxArgs,usage1)                   \
      static returnType c##name(ConsoleObject *, S32, const char **);                   \
      static ConsoleConstructor g##name##obj(NULL,#name,c##name,"",minArgs,maxArgs);\
      static returnType c##name(CodeBlockWorld* con, ConsoleObject *, S32 argc, const char **argv)

#  define ConsoleMethod(className,name,returnType,minArgs,maxArgs,usage1)                             \
      static inline returnType c##className##name(className *, S32, const char **argv);               \
      static returnType c##className##name##caster(CodeBlockWorld* con, ConsoleObject *object, S32 argc, const char **argv) {  \
         conmethod_return_##returnType ) c##className##name(con, static_cast<className*>(object),argc,argv);              \
      };                                                                                              \
      static ConsoleConstructor                                                                       \
         className##name##obj(#className,#name,c##className##name##caster,"",minArgs,maxArgs);        \
      static inline returnType c##className##name(CodeBlockWorld* con, className *object, S32 argc, const char **argv)

#  define ConsoleMethodWithDoc(className,name,returnType,minArgs,maxArgs,usage1,doc)                      \
static inline returnType c##className##name(className *, S32, const char **argv);               \
static returnType c##className##name##caster(CodeBlockWorld* con, ConsoleObject *object, S32 argc, const char **argv) {  \
conmethod_return_##returnType ) c##className##name(con, static_cast<className*>(object),argc,argv);              \
};                                                                                              \
static ConsoleConstructor                                                                       \
className##name##obj(#className,#name,c##className##name##caster,"",minArgs,maxArgs);        \
static inline returnType c##className##name(CodeBlockWorld* con, className *object, S32 argc, const char **argv)

#  define ConsoleStaticMethod(className,name,returnType,minArgs,maxArgs,usage1)                       \
      static inline returnType c##className##name(S32, const char **);                                \
      static returnType c##className##name##caster(CodeBlockWorld* con, ConsoleObject *object, S32 argc, const char **argv) {  \
         conmethod_return_##returnType ) c##className##name(con, argc,argv);                                                        \
      };                                                                                              \
      static ConsoleConstructor                                                                       \
         className##name##obj(#className,#name,c##className##name##caster,"",minArgs,maxArgs);        \
      static inline returnType c##className##name(CodeBlockWorld* con, S32 argc, const char **argv)


#endif

/// @}

#include "console/codeBlockWorld.h"

#endif
