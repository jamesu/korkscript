#include "console/console.h"
#include "console/codeBlockWorld.h"
#include "console/consoleInternal.h"
#include "console/consoleObject.h"
#include <unordered_map>

CON_DECLARE_PARSER(CMD);

S32 CodeBlockWorld::dbgGetCurrentFrame(void)
{
   return gEvalState->stack.size() - 1;
}

const char * CodeBlockWorld::prependDollar ( const char * name )
{
   if(name[0] != '$')
   {
      S32   len = dStrlen(name);
      AssertFatal(len < sizeof(gScratchBuffer)-2, "CONSOLE: name too long");
      gScratchBuffer[0] = '$';
      dMemcpy(gScratchBuffer + 1, name, len + 1);
      name = gScratchBuffer;
   }
   return name;
}

const char * CodeBlockWorld::prependPercent ( const char * name )
{
   if(name[0] != '%')
   {
      S32   len = dStrlen(name);
      AssertFatal(len < sizeof(gScratchBuffer)-2, "CONSOLE: name too long");
      gScratchBuffer[0] = '%';
      dMemcpy(gScratchBuffer + 1, name, len + 1);
      name = gScratchBuffer;
   }
   return name;
}

static Vector<ConsumerCallback> gConsumers(__FILE__, __LINE__);
static DataChunker consoleLogChunker;
static Vector<ConsoleLogEntry> consoleLog(__FILE__, __LINE__);
static bool consoleLogLocked;
static bool logBufferEnabled=true;
static S32 printLevel = 10;
static FileStream consoleLogFile;
static const char *defLogFileName = "console.log";
static S32 consoleLogMode = 0;
static bool active = false;
static bool newLogFile;
static const char *logFileName;


CodeBlockWorld::CodeBlockWorld() :
gConsumers(__FILE__, __LINE__),
consoleLog(__FILE__, __LINE__)
{
	mGlobalNamespace = NULL;
	mNamespaceList = NULL;
	mCacheSequence = 0;
	smCodeBlockList = NULL;
	smCurrentCodeBlock = NULL;

	gCurrentFile = NULL;
	gCurrentRoot = NULL;

	printLevel = 10;
	consoleLogMode = 0;
	active = false;
	newLogFile = false;
	logFileName = NULL;
	consoleLogLocked = false;
	logBufferEnabled = true;
	defLogFileName = "console.log";

	completionBaseStart = 0;
	completionBaseLen = 0;

	#ifdef TORQUE_MULTITHREAD
	gMainThreadID = -1;
	#endif

	tabBuffer[0] = '\0';

   gEvalState = new ExprEvalState(this);

}

CodeBlockWorld::~CodeBlockWorld()
{
   if (active)
      shutdown();
}

void CodeBlockWorld::init()
{
   AssertFatal(active == false, "Con::init should only be called once.");

   // Set up general init values.
   active                        = true;
   logFileName                   = NULL;
   newLogFile                    = true;
   gWarnUndefinedScriptVariables = false;
   //sLogMutex                     = new Mutex;

#ifdef TORQUE_MULTITHREAD
   // Note the main thread ID.
   gMainThreadID = ThreadManager::getCurrentThreadId();
#endif

   // Initialize subsystems.
   mGlobalNamespace = find(NULL);
   ConsoleConstructor::setup(this);

   // Set up the parser(s)
   CON_ADD_PARSER(CMD,   "cs",   true);   // TorqueScript

   // Setup the console types.  (tgemit - needs to be here)
   ConsoleBaseType::initialize();

   // Variables
   setVariable("Con::prompt", "% ");
   addVariable("Con::logBufferEnabled", TypeBool, &logBufferEnabled);
   addVariable("Con::printLevel", TypeS32, &printLevel);
   addVariable("Con::warnUndefinedVariables", TypeBool, &gWarnUndefinedScriptVariables);

   // Current script file name and root
   addVariable( "Con::File", TypeString, &gCurrentFile );
   addVariable( "Con::Root", TypeString, &gCurrentRoot );

   // Setup the console types.
   ConsoleBaseType::initialize();

   // And finally, the ACR...
   AbstractClassRep::initialize(this);
}

//--------------------------------------

void CodeBlockWorld::shutdown()
{
   AssertFatal(active == true, "Con::shutdown should only be called once.");
   active = false;

   consoleLogFile.close();
   for(Namespace *walk = mNamespaceList; walk; walk = walk->mNext)
      walk->clearEntries();

   //SAFE_DELETE( sLogMutex );
}

bool CodeBlockWorld::isActive()
{
   return active;
}

bool CodeBlockWorld::isMainThread()
{
#ifdef TORQUE_MULTITHREAD
   return ThreadManager::isCurrentThread(gMainThreadID);
#else
   // If we're single threaded we're always in the main thread.
   return true;
#endif
}

//--------------------------------------

void CodeBlockWorld::getLockLog(ConsoleLogEntry *&log, U32 &size)  
{  
   consoleLogLocked = true;
   log = consoleLog.address();
   size = consoleLog.size();  
}

void CodeBlockWorld::unlockLog()
{
   consoleLogLocked = false;
}

U32 CodeBlockWorld::tabComplete(char* inputBuffer, U32 cursorPos, U32 maxResultLength, bool forwardTab)
{
   // Check for null input.
   if (!inputBuffer[0]) 
   {
      return cursorPos;
   }

   // Cap the max result length.
   if (maxResultLength > MaxCompletionBufferSize) 
   {
      maxResultLength = MaxCompletionBufferSize;
   }

   // See if this is the same partial text as last checked.
   if (dStrcmp(tabBuffer, inputBuffer)) 
   {
      // If not...
      // Save it for checking next time.
      dStrcpy(tabBuffer, inputBuffer);
      // Scan backward from the cursor position to find the base to complete from.
      S32 p = cursorPos;
      while ((p > 0) && (inputBuffer[p - 1] != ' ') && (inputBuffer[p - 1] != '.') && (inputBuffer[p - 1] != '('))
      {
         p--;
      }
      completionBaseStart = p;
      completionBaseLen = cursorPos - p;
      // Is this function being invoked on an object?
      if (inputBuffer[p - 1] == '.') 
      {
         // If so...
         if (p <= 1) 
         {
            // Bail if no object identifier.
            return cursorPos;
         }

         // Find the object identifier.
         S32 objLast = --p;
         while ((p > 0) && (inputBuffer[p - 1] != ' ') && (inputBuffer[p - 1] != '(')) 
         {
            p--;
         }

         if (objLast == p) 
         {
            // Bail if no object identifier.
            return cursorPos;
         }

         // Look up the object identifier.
         dStrncpy(completionBuffer, inputBuffer + p, objLast - p);
         completionBuffer[objLast - p] = 0;
         // TOFIX tabObject = Sim::findObject(completionBuffer);
         if (false)//!bool(tabObject)) 
         {
            // Bail if not found.
            return cursorPos;
         }
      }
      else 
      {
         // Not invoked on an object; we'll use the global namespace.
         //tabObject = 0;
      }
   }

   // Chop off the input text at the cursor position.
   inputBuffer[cursorPos] = 0;

   // Try to find a completion in the appropriate namespace.
   const char *newText;

   if (false)//bool(tabObject))  // TOFIX
   {
      //newText = tabObject->tabComplete(inputBuffer + completionBaseStart, completionBaseLen, forwardTab);
   }
   else 
   {
      // In the global namespace, we can complete on global vars as well as functions.
      if (inputBuffer[completionBaseStart] == '$')
      {
         newText = gEvalState->globalVars.tabComplete(inputBuffer + completionBaseStart, completionBaseLen, forwardTab);
      }
      else 
      {
         newText = mGlobalNamespace->tabComplete(inputBuffer + completionBaseStart, completionBaseLen, forwardTab);
      }
   }

   if (newText) 
   {
      // If we got something, append it to the input text.
      S32 len = dStrlen(newText);
      if (len + completionBaseStart > maxResultLength)
      {
         len = maxResultLength - completionBaseStart;
      }
      dStrncpy(inputBuffer + completionBaseStart, newText, len);
      inputBuffer[completionBaseStart + len] = 0;
      // And set the cursor after it.
      cursorPos = completionBaseStart + len;
   }

   // Save the modified input buffer for checking next time.
   dStrcpy(tabBuffer, inputBuffer);

   // Return the new (maybe) cursor position.
   return cursorPos;
}

//------------------------------------------------------------------------------
void CodeBlockWorld::log(const char *string)
{
   // Lock.
   //MutexHandle mutex;
   //if( sLogMutex )
   //   mutex.lock( sLogMutex, true );

   // Bail if we ain't logging.
   if (!consoleLogMode) 
   {
      return;
   }

   // In mode 1, we open, append, close on each log write.
   if ((consoleLogMode & 0x3) == 1) 
   {
      consoleLogFile.open(defLogFileName, FileStream::ReadWrite);
   }

   // Write to the log if its status is hunky-dory.
   if ((consoleLogFile.getStatus() == Stream::Ok) || (consoleLogFile.getStatus() == Stream::EOS)) 
   {
      consoleLogFile.setPosition(consoleLogFile.getStreamSize());
      // If this is the first write...
      if (newLogFile) 
      {
         // Make a header.
         Platform::LocalTime lt;
         Platform::getLocalTime(lt);
         char buffer[128];
         dSprintf(buffer, sizeof(buffer), "//-------------------------- %d/%d/%d -- %02d:%02d:%02d -----\r\n",
               lt.month + 1,
               lt.monthday,
               lt.year + 1900,
               lt.hour,
               lt.min,
               lt.sec);
         consoleLogFile.write(dStrlen(buffer), buffer);
         newLogFile = false;
         if (consoleLogMode & 0x4) 
         {
            // Dump anything that has been printed to the console so far.
            consoleLogMode -= 0x4;
            U32 size, line;
            ConsoleLogEntry *log;
            getLockLog(log, size);
            for (line = 0; line < size; line++) 
            {
               consoleLogFile.write(dStrlen(log[line].mString), log[line].mString);
               consoleLogFile.write(2, "\r\n");
            }
            unlockLog();
         }
      }
      // Now write what we came here to write.
      consoleLogFile.write(dStrlen(string), string);
      consoleLogFile.write(2, "\r\n");
   }

   if ((consoleLogMode & 0x3) == 1) 
   {
      consoleLogFile.close();
   }
}

//------------------------------------------------------------------------------

void CodeBlockWorld::cls( void )
{
   if(consoleLogLocked)
      return;
   consoleLogChunker.freeBlocks();
   consoleLog.setSize(0);
};

//------------------------------------------------------------------------------

#if defined( _MSC_VER )  
#include <windows.h>  

static void _outputDebugString(char* pString)  
{  
    // Format string.
    char* pBuffer = pString;  
    S32 stringLength = dStrlen(pString);  
    pBuffer += stringLength;  
    *pBuffer++ = '\r';  
    *pBuffer++ = '\n';  
    *pBuffer   = '\0';  
    stringLength = strlen(pString) + 1;  
    wchar_t *wstr = new wchar_t[stringLength];  
    dMemset( wstr, 0, stringLength );

    // Convert to wide string.
    Con::MultiByteToWideChar( CP_ACP, NULL, pString, -1, wstr, stringLength );  

    // Output string.
    Con::OutputDebugStringW( wstr );
    delete [] wstr;  
}
#endif

//------------------------------------------------------------------------------

void CodeBlockWorld::_printf(ConsoleLogEntry::Level level, ConsoleLogEntry::Type type, const char* fmt)
{
   active = false; 

   char buffer[4096];
   U32 offset = 0;
   if(gEvalState->traceOn && gEvalState->stack.size())
   {
      offset = gEvalState->stack.size() * 3;
      for(U32 i = 0; i < offset; i++)
         buffer[i] = ' ';
   }
   dSprintf(buffer + offset, sizeof(buffer) - offset, "%s", fmt);

   for(U32 i = 0; i < (U32)gConsumers.size(); i++)
      gConsumers[i](level, buffer);

   Platform::cprintf(buffer);

   if(logBufferEnabled || consoleLogMode)
   {
      char *pos = buffer;
      while(*pos)
      {
         if(*pos == '\t')
            *pos = '^';
         pos++;
      }
      pos = buffer;

      for(;;)
      {
         char *eofPos = dStrchr(pos, '\n');
         if(eofPos)
            *eofPos = 0;

         log(pos);
         if(logBufferEnabled && !consoleLogLocked)
         {
            ConsoleLogEntry entry;
            entry.mLevel  = level;
            entry.mType   = type;
            entry.mString = (const char *)consoleLogChunker.alloc(dStrlen(pos) + 1);
            dStrcpy(const_cast<char*>(entry.mString), pos);
            consoleLog.push_back(entry);
         }
         if(!eofPos)
            break;
         pos = eofPos + 1;
      }
   }

   active = true;

#if defined( _MSC_VER )  
   _outputDebugString( buffer );  
#endif
}


//------------------------------------------------------------------------------
void CodeBlockWorld::printf(const char* fmt,...)
{
   va_list argptr;
   va_start(argptr, fmt);
   char buf[8192];
   dVsprintf(buf, sizeof(buf), fmt, argptr);
   //if(!isMainThread())
   //   Sim::postEvent(Sim::getRootGroup(), new ConPrinfThreadedEvent(ConsoleLogEntry::Normal, ConsoleLogEntry::General, buf), Sim::getTargetTime());
   //else
      _printf(ConsoleLogEntry::Normal, ConsoleLogEntry::General, buf);
   va_end(argptr);
}

void CodeBlockWorld::warnf(ConsoleLogEntry::Type type, const char* fmt,...)
{
   va_list argptr;
   va_start(argptr, fmt);
   char buf[8192];
   dVsprintf(buf, sizeof(buf), fmt, argptr);
   //if(!isMainThread())
   //   Sim::postEvent(Sim::getRootGroup(), new ConPrinfThreadedEvent(ConsoleLogEntry::Warning, type, buf), Sim::getTargetTime());
   //else
      _printf(ConsoleLogEntry::Warning, type, buf);
   va_end(argptr);
}

void CodeBlockWorld::errorf(ConsoleLogEntry::Type type, const char* fmt,...)
{
   va_list argptr;
   va_start(argptr, fmt);
   char buf[8192];
   dVsprintf(buf, sizeof(buf), fmt, argptr);
   //if(!isMainThread())
   //   Sim::postEvent(Sim::getRootGroup(), new ConPrinfThreadedEvent(ConsoleLogEntry::Error, type, buf), Sim::getTargetTime());
   //else
      _printf(ConsoleLogEntry::Error, type, buf);
   va_end(argptr);
}

void CodeBlockWorld::warnf(const char* fmt,...)
{
   va_list argptr;
   va_start(argptr, fmt);
   char buf[8192];
   dVsprintf(buf, sizeof(buf), fmt, argptr);
   //if(!isMainThread())
   //   Sim::postEvent(Sim::getRootGroup(), new ConPrinfThreadedEvent(ConsoleLogEntry::Warning, ConsoleLogEntry::General, buf), Sim::getTargetTime());
   //else
      _printf(ConsoleLogEntry::Warning, ConsoleLogEntry::General, buf);
   va_end(argptr);
}

void CodeBlockWorld::errorf(const char* fmt,...)
{
   va_list argptr;
   va_start(argptr, fmt);
   char buf[8192];
   dVsprintf(buf, sizeof(buf), fmt, argptr);
   //if(!isMainThread())
   //   Sim::postEvent(Sim::getRootGroup(), new ConPrinfThreadedEvent(ConsoleLogEntry::Error, ConsoleLogEntry::General, buf), Sim::getTargetTime());
   //else
      _printf(ConsoleLogEntry::Error, ConsoleLogEntry::General, buf);
   va_end(argptr);
}

//---------------------------------------------------------------------------

void CodeBlockWorld::setVariable(const char *name, const char *value)
{
   name = prependDollar(name);
   gEvalState->globalVars.setVariable(StringTable->insert(name), value);
}

void CodeBlockWorld::setLocalVariable(const char *name, const char *value)
{
   name = prependPercent(name);
   gEvalState->stack.last()->setVariable(StringTable->insert(name), value);
}

void CodeBlockWorld::setBoolVariable(const char *varName, bool value)
{
   setVariable(varName, value ? "1" : "0");
}

void CodeBlockWorld::setIntVariable(const char *varName, S32 value)
{
   char scratchBuffer[32];
   dSprintf(scratchBuffer, sizeof(scratchBuffer), "%d", value);
   setVariable(varName, scratchBuffer);
}

void CodeBlockWorld::setFloatVariable(const char *varName, F32 value)
{
   char scratchBuffer[32];
   dSprintf(scratchBuffer, sizeof(scratchBuffer), "%.9g", value);
   setVariable(varName, scratchBuffer);
}

//---------------------------------------------------------------------------
void CodeBlockWorld::addConsumer(ConsumerCallback consumer)
{
   gConsumers.push_back(consumer);
}

// dhc - found this empty -- trying what I think is a reasonable impl.
void CodeBlockWorld::removeConsumer(ConsumerCallback consumer)
{
   for(U32 i = 0; i < (U32)gConsumers.size(); i++)
      if (gConsumers[i] == consumer)
      { // remove it from the list.
         gConsumers.erase(i);
         break;
      }
}

void CodeBlockWorld::stripColorChars(char* line)
{
   char* c = line;
   char cp = *c;
   while (cp) 
   {
      if (cp < 18) 
      {
         // Could be a color control character; let's take a closer look.
         if ((cp != 8) && (cp != 9) && (cp != 10) && (cp != 13)) 
         {
            // Yep... copy following chars forward over this.
            char* cprime = c;
            char cpp;
            do 
            {
               cpp = *++cprime;
               *(cprime - 1) = cpp;
            } 
            while (cpp);
            // Back up 1 so we'll check this position again post-copy.
            c--;
         }
      }
      cp = *++c;
   }
}

const char *CodeBlockWorld::getVariable(const char *name)
{
   char scratchBuffer[4096];

   // get the field info from the object..
   if(name[0] != '$' && dStrchr(name, '.') && !isFunction(name))
   {
      S32 len = dStrlen(name);
      AssertFatal(len < sizeof(scratchBuffer)-1, "Sim::getVariable - name too long");
      dMemcpy(scratchBuffer, name, len+1);

      char * token = dStrtok(scratchBuffer, ".");
      ConsoleObject * obj = lookupObject(token);
      if(!obj)
         return("");

      token = dStrtok(0, ".\0");
      if(!token)
         return("");

      while(token != NULL)
      {
         const char * val = obj->getDataField(StringTable->insert(token), 0);
         if(!val)
            return("");

         token = dStrtok(0, ".\0");
         if(token)
         {
            obj = lookupObject(token);
            if(!obj)
               return("");
         }
         else
            return(val);
      }
   }

   name = prependDollar(name);
   return gEvalState->globalVars.getVariable(StringTable->insert(name));
}

const char *CodeBlockWorld::getLocalVariable(const char *name)
{
   name = prependPercent(name);

   return gEvalState->stack.last()->getVariable(StringTable->insert(name));
}

bool CodeBlockWorld::getBoolVariable(const char *varName, bool def)
{
   const char *value = getVariable(varName);
   return *value ? dAtob(value) : def;
}

S32 CodeBlockWorld::getIntVariable(const char *varName, S32 def)
{
   const char *value = getVariable(varName);
   return *value ? dAtoi(value) : def;
}

F32 CodeBlockWorld::getFloatVariable(const char *varName, F32 def)
{
   const char *value = getVariable(varName);
   return *value ? dAtof(value) : def;
}

//---------------------------------------------------------------------------

bool CodeBlockWorld::addVariable(    const char *name,
                 S32 type,
                 void *dptr,
                 const char* usage )
{
   gEvalState->globalVars.addVariable( name, type, dptr, usage );
   return true;
}

bool CodeBlockWorld::removeVariable(const char *name)
{
   name = StringTable->lookup(prependDollar(name));
   return name!=0 && gEvalState->globalVars.removeVariable(name);
}

//---------------------------------------------------------------------------

void CodeBlockWorld::addCommand(const char *nsName, const char *name,StringCallback cb, const char *usage, S32 minArgs, S32 maxArgs)
{
   Namespace *ns = lookupNamespace(nsName);
   ns->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs);
}

void CodeBlockWorld::addCommand(const char *nsName, const char *name,VoidCallback cb, const char *usage, S32 minArgs, S32 maxArgs)
{
   Namespace *ns = lookupNamespace(nsName);
   ns->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs);
}

void CodeBlockWorld::addCommand(const char *nsName, const char *name,IntCallback cb, const char *usage, S32 minArgs, S32 maxArgs)
{
   Namespace *ns = lookupNamespace(nsName);
   ns->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs);
}

void CodeBlockWorld::addCommand(const char *nsName, const char *name,FloatCallback cb, const char *usage, S32 minArgs, S32 maxArgs)
{
   Namespace *ns = lookupNamespace(nsName);
   ns->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs);
}

void CodeBlockWorld::addCommand(const char *nsName, const char *name,BoolCallback cb, const char *usage, S32 minArgs, S32 maxArgs)
{
   Namespace *ns = lookupNamespace(nsName);
   ns->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs);
}

void CodeBlockWorld::markCommandGroup(const char * nsName, const char *name, const char* usage)
{
   Namespace *ns = lookupNamespace(nsName);
   ns->markGroup(name,usage);
}

void CodeBlockWorld::beginCommandGroup(const char * nsName, const char *name, const char* usage)
{
   markCommandGroup(nsName, name, usage);
}

void CodeBlockWorld::endCommandGroup(const char * nsName, const char *name)
{
   markCommandGroup(nsName, name, NULL);
}

void CodeBlockWorld::addOverload(const char * nsName, const char * name, const char * altUsage)
{
   Namespace *ns = lookupNamespace(nsName);
   ns->addOverload(name,altUsage);
}

void CodeBlockWorld::addCommand(const char *name,StringCallback cb,const char *usage, S32 minArgs, S32 maxArgs)
{
   mGlobalNamespace->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs);
}

void CodeBlockWorld::addCommand(const char *name,VoidCallback cb,const char *usage, S32 minArgs, S32 maxArgs)
{
   mGlobalNamespace->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs);
}

void CodeBlockWorld::addCommand(const char *name,IntCallback cb,const char *usage, S32 minArgs, S32 maxArgs)
{
   mGlobalNamespace->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs);
}

void CodeBlockWorld::addCommand(const char *name,FloatCallback cb,const char *usage, S32 minArgs, S32 maxArgs)
{
   mGlobalNamespace->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs);
}

void CodeBlockWorld::addCommand(const char *name,BoolCallback cb,const char *usage, S32 minArgs, S32 maxArgs)
{
   mGlobalNamespace->addCommand(StringTable->insert(name), cb, usage, minArgs, maxArgs);
}

// Known as expandOldScriptFilename in T3D
bool CodeBlockWorld::expandScriptFilename(char *filename, U32 size, const char *src)
{
   const StringTableEntry cbName = smCurrentCodeBlock ? smCurrentCodeBlock->name : NULL;

   if (!cbName)
   {
      dStrcpy(filename, src);
      return true;
   }
   
   const char *slash;
   if (dStrncmp(src, "~/", 2) == 0)
      // tilde path means load from current codeblock/mod root
      slash = dStrchr(cbName, '/');
   else if (dStrncmp(src, "./", 2) == 0)
      // dot path means load from current codeblock/mod path
      slash = dStrrchr(cbName, '/');
   else
   {
      // otherwise path must be fully specified
      if (dStrlen(src) > size)
      {
         errorf("Buffer overflow attempting to expand filename: %s", src);
         *filename = 0;
         return false;
      }
      dStrcpy(filename, src);
      return true;
   }
   
   if (slash == NULL)
   {
      errorf("Illegal CodeBlock path detected (no mod directory): %s", cbName);
      *filename = 0;
      return false;
   }
   
   U32 length = slash-cbName;
   if ((length+dStrlen(src)) > size)
   {
      errorf("Buffer overflow attempting to expand filename: %s", src);
      *filename = 0;
      return false;
   }
   
   dStrncpy(filename, cbName, length);
   dStrcpy(filename+length, src+1);
   return true;
}


const char *CodeBlockWorld::evaluate(const char* string, bool echo, const char *fileName)
{
   if (echo)
      printf("%s%s", getVariable( "$Con::Prompt" ), string);

   if(fileName)
      fileName = StringTable->insert(fileName);

   CodeBlock *newCodeBlock = new CodeBlock(this);
   return newCodeBlock->compileExec(fileName, string, false, fileName ? -1 : 0);
}

//------------------------------------------------------------------------------
const char *CodeBlockWorld::evaluatef(const char* string, ...)
{
   const char * result = NULL;
   char * buffer = new char[4096];
   if (buffer != NULL)
   {
      va_list args;
      va_start(args, string);
      dVsprintf(buffer, 4096, string, args);
      va_end (args);

      CodeBlock *newCodeBlock = new CodeBlock(this);
      result = newCodeBlock->compileExec(NULL, buffer, false, 0);

      delete [] buffer;
      buffer = NULL;
   }

   return result;
}

const char *CodeBlockWorld::execute(S32 argc, const char *argv[])
{
#ifdef TORQUE_MULTITHREAD
   if(isMainThread())
   {
#endif
      Namespace::Entry *ent;
      StringTableEntry funcName = StringTable->insert(argv[0]);
      ent = mGlobalNamespace->lookup(funcName);

      if(!ent)
      {
         warnf(ConsoleLogEntry::Script, "%s: Unknown command.", argv[0]);

         // Clean up arg buffers, if any.
         gEvalState->STR.clearFunctionOffset();
         return "";
      }

      const char *ret = ent->execute(this, argc, argv, gEvalState);

      // Reset the function offset so the stack
      // doesn't continue to grow unnecessarily
      gEvalState->STR.clearFunctionOffset();

      return ret;

#ifdef TORQUE_MULTITHREAD
   }
   else
   {
      SimConsoleThreadExecCallback cb;
      SimConsoleThreadExecEvent *evt = new SimConsoleThreadExecEvent(argc, argv, false, &cb);
      Sim::postEvent(Sim::getRootGroup(), evt, Sim::getCurrentTime());
      
      return cb.waitForResult();
   }
#endif
}

//------------------------------------------------------------------------------
const char *CodeBlockWorld::execute(ConsoleObject *object, S32 argc, const char *argv[],bool thisCallOnly)
{
   static char idBuf[16];
   if(argc < 2)
      return "";
   
   if(object->getNamespace())
   {
      StringTableEntry funcName = StringTable->insert(argv[0]);
      Namespace::Entry *ent = object->getNamespace()->lookup(funcName);

      if(ent == NULL)
      {
         //warnf(ConsoleLogEntry::Script, "%s: undefined for object '%s' - id %d", funcName, object->getName(), object->getId());

         // Clean up arg buffers, if any.
         gEvalState->STR.clearFunctionOffset();
         return "";
      }

      // Twiddle %this argument
      const char *oldArg1 = argv[1];
      dSprintf(idBuf, sizeof(idBuf), "%d", object->getId());
      argv[1] = idBuf;

      if (ent->mType == Namespace::Entry::ScriptFunctionType)
      {
         // TODO: need a better way of dealing with this
         object->pushScriptCallbackGuard();
      }

      ConsoleObject *save = gEvalState->thisObject;
      gEvalState->thisObject = object;
      const char *ret = ent->execute(this, argc, argv, gEvalState);
      gEvalState->thisObject = save;

      if (ent->mType == Namespace::Entry::ScriptFunctionType)
      {
         object->popScriptCallbackGuard();
      }

      // Twiddle it back
      argv[1] = oldArg1;

      // Reset the function offset so the stack
      // doesn't continue to grow unnecessarily
      gEvalState->STR.clearFunctionOffset();

      return ret;
   }
   warnf(ConsoleLogEntry::Script, "Con::execute - %d has no namespace: %s", object->getId(), argv[0]);
   return "";
}
const char *CodeBlockWorld::executef(ConsoleObject *object, S32 argc, ...)
{
   const char *argv[128];

   va_list args;
   va_start(args, argc);
   for(S32 i = 0; i < argc; i++)
      argv[i+1] = va_arg(args, const char *);
   va_end(args);
   argv[0] = argv[1];
   argc++;

   return execute(object, argc, argv);
}

//------------------------------------------------------------------------------
const char *CodeBlockWorld::executef(S32 argc, ...)
{
   const char *argv[128];

   va_list args;
   va_start(args, argc);
   for(S32 i = 0; i < argc; i++)
      argv[i] = va_arg(args, const char *);
   va_end(args);
   return execute(argc, argv);
}

//------------------------------------------------------------------------------
bool CodeBlockWorld::isFunction(const char *fn)
{
   const char *string = StringTable->lookup(fn);
   if(!string)
      return false;
   else
      return mGlobalNamespace->lookup(string) != NULL;
}

//------------------------------------------------------------------------------

void CodeBlockWorld::setLogMode(S32 newMode)
{
   if ((newMode & 0x3) != (consoleLogMode & 0x3))
   {
      if (newMode && !consoleLogMode)
      {
         // Enabling logging when it was previously disabled.
         newLogFile = true;
      }
      if ((consoleLogMode & 0x3) == 2)
      {
         // Changing away from mode 2, must close logfile.
         consoleLogFile.close();
      }
      else if ((newMode & 0x3) == 2)
      {
         // Starting mode 2, must open logfile.
         consoleLogFile.open(defLogFileName, FileStream::Write);
      }
      consoleLogMode = newMode;
   }
}

Namespace *CodeBlockWorld::lookupNamespace(const char *ns)
{
   if(!ns)
      return mGlobalNamespace;
   return find(StringTable->insert(ns));
}

bool CodeBlockWorld::linkNamespaces(const char *parent, const char *child)
{
   Namespace *pns = lookupNamespace(parent);
   Namespace *cns = lookupNamespace(child);
   if(pns && cns)
      return cns->classLinkTo(pns);
   return false;
}

bool CodeBlockWorld::unlinkNamespaces(const char *parent, const char *child)
{
   Namespace *pns = lookupNamespace(parent);
   Namespace *cns = lookupNamespace(child);
   if(pns && cns)
      return cns->unlinkClass(pns);
   return false;
}

bool CodeBlockWorld::classLinkNamespaces(Namespace *parent, Namespace *child)
{
   if(parent && child)
      return child->classLinkTo(parent);
   return false;
}

void CodeBlockWorld::setData(S32 type, void *dptr, S32 index, S32 argc, const char **argv, const EnumTable *tbl, BitSet32 flag)
{
   ConsoleBaseType *cbt = ConsoleBaseType::getType(type);
   AssertFatal(cbt, "Con::setData - could not resolve type ID!");
   cbt->setData(this, (void *) (((const char *)dptr) + index * cbt->getTypeSize()),argc, argv, tbl, flag);
}

const char *CodeBlockWorld::getData(S32 type, void *dptr, S32 index, const EnumTable *tbl, BitSet32 flag)
{
   ConsoleBaseType *cbt = ConsoleBaseType::getType(type);
   AssertFatal(cbt, "Con::getData - could not resolve type ID!");
   return cbt->getData(this, (void *) (((const char *)dptr) + index * cbt->getTypeSize()), tbl, flag);
}
   
//------------------------------------------------------------------------------

StringTableEntry CodeBlockWorld::getModNameFromPath(const char *path)
{
   if(path == NULL || *path == 0)
      return NULL;

   char buf[1024];
   buf[0] = 0;

   if(path[0] == '/' || path[1] == ':')
   {
      // It's an absolute path
      const StringTableEntry exePath = Platform::getMainDotCsDir();
      U32 len = dStrlen(exePath);
      if(dStrnicmp(exePath, path, len) == 0)
      {
         const char *ptr = path + len + 1;
         const char *slash = dStrchr(ptr, '/');
         if(slash)
         {
            dStrncpy(buf, ptr, slash - ptr);
            buf[slash - ptr] = 0;
         }
         else
            return NULL;
      }
      else
         return NULL;
   }
   else
   {
      const char *slash = dStrchr(path, '/');
      if(slash)
      {
         dStrncpy(buf, path, slash - path);
         buf[slash - path] = 0;
      }
      else
         return NULL;
   }

   return StringTable->insert(buf);
}

//-----------------------------------------------------------------------------

typedef std::unordered_map<StringTableEntry, StringTableEntry> typePathExpandoMap;
static typePathExpandoMap PathExpandos;

//-----------------------------------------------------------------------------

void CodeBlockWorld::addPathExpando( const char* pExpandoName, const char* pPath )
{
   // Sanity!
   AssertFatal( pExpandoName != NULL, "Expando name cannot be NULL." );
   AssertFatal( pPath != NULL, "Expando path cannot be NULL." );
   
   // Fetch expando name.
   StringTableEntry expandoName = StringTable->insert( pExpandoName );
   
   // Fetch the length of the path.
   S32 pathLength = dStrlen(pPath);
   
   char pathBuffer[1024];
   
   // Sanity!
   if ( pathLength == 0 || pathLength >= sizeof(pathBuffer) )
   {
      warnf( "Cannot add path expando '%s' with path '%s' as the path is an invalid length.", pExpandoName, pPath );
      return;
   }
   
   // Strip repeat slashes.
   if ( !stripRepeatSlashes(pathBuffer, pPath, sizeof(pathBuffer) ) )
   {
      warnf( "Cannot add path expando '%s' with path '%s' as the path is an invalid length.", pExpandoName, pPath );
      return;
   }
   
   // Fetch new path length.
   pathLength = dStrlen(pathBuffer);
   
   // Sanity!
   if ( pathLength == 0 )
   {
      warnf( "Cannot add path expando '%s' with path '%s' as the path is an invalid length.", pExpandoName, pPath );
      return;
   }
   
   // Remove any terminating slash.
   if (pathBuffer[pathLength-1] == '/')
      pathBuffer[pathLength-1] = 0;
   
   // Fetch expanded path.
   StringTableEntry expandedPath = StringTable->insert( pathBuffer );
   
   // Info.
#if defined(TORQUE_DEBUG)
   printf("Adding path expando of '%s' as '%s'.", expandoName, expandedPath );
#endif
   
   // Find any existing path expando.
   typePathExpandoMap::iterator expandoItr = PathExpandos.find( pExpandoName );
   
   // Does the expando exist?
   if( expandoItr != PathExpandos.end() )
   {
      // Yes, so modify the path.
      expandoItr->second = expandedPath;
      return;
   }
   
   // Insert expando.
   PathExpandos[expandoName] = expandedPath;
}

//-----------------------------------------------------------------------------

StringTableEntry CodeBlockWorld::getPathExpando( const char* pExpandoName )
{
   // Sanity!
   AssertFatal( pExpandoName != NULL, "Expando name cannot be NULL." );
   
   // Fetch expando name.
   StringTableEntry expandoName = StringTable->insert( pExpandoName );
   
   // Find any existing path expando.
   typePathExpandoMap::iterator expandoItr = PathExpandos.find( expandoName );
   
   // Does the expando exist?
   if( expandoItr != PathExpandos.end() )
   {
      // Yes, so return it.
      return expandoItr->second;
   }
   
   // Not found.
   return NULL;
}

//-----------------------------------------------------------------------------

void CodeBlockWorld::removePathExpando( const char* pExpandoName )
{
   // Sanity!
   AssertFatal( pExpandoName != NULL, "Expando name cannot be NULL." );
   
   // Fetch expando name.
   StringTableEntry expandoName = StringTable->insert( pExpandoName );
   
   // Find any existing path expando.
   typePathExpandoMap::iterator expandoItr = PathExpandos.find( expandoName );
   
   // Does the expando exist?
   if ( expandoItr == PathExpandos.end() )
   {
      // No, so warn.
#if defined(TORQUE_DEBUG)
      warnf("Removing path expando of '%s' but it does not exist.", expandoName );
#endif
      return;
   }
   
   // Info.
#if defined(TORQUE_DEBUG)
   printf("Removing path expando of '%s' as '%s'.", expandoName, expandoItr->second );
#endif
   // Remove expando.
   PathExpandos.erase( expandoItr );
}

//-----------------------------------------------------------------------------

bool CodeBlockWorld::isPathExpando( const char* pExpandoName )
{
   // Sanity!
   AssertFatal( pExpandoName != NULL, "Expando name cannot be NULL." );
   
   // Fetch expando name.
   StringTableEntry expandoName = StringTable->insert( pExpandoName );
   
   auto itr = PathExpandos.find(expandoName);
   return itr != PathExpandos.end();
}

//-----------------------------------------------------------------------------

U32 CodeBlockWorld::getPathExpandoCount( void )
{
   return PathExpandos.size();
}

//-----------------------------------------------------------------------------

StringTableEntry CodeBlockWorld::getPathExpandoKey( U32 expandoIndex )
{
   // Finish if index is out of range.
   if ( expandoIndex >= PathExpandos.size() )
      return NULL;
   
   // Find indexed iterator.
   typePathExpandoMap::iterator expandoItr = PathExpandos.begin();
   while( expandoIndex > 0 ) { ++expandoItr; --expandoIndex; }
   
   return expandoItr->first;
}

//-----------------------------------------------------------------------------

StringTableEntry CodeBlockWorld::getPathExpandoValue( U32 expandoIndex )
{
   // Finish if index is out of range.
   if ( expandoIndex >= PathExpandos.size() )
      return NULL;
   
   // Find indexed iterator.
   typePathExpandoMap::iterator expandoItr = PathExpandos.begin();
   while( expandoIndex > 0 ) { ++expandoItr; --expandoIndex; }
   
   return expandoItr->second;
}

//-----------------------------------------------------------------------------

bool CodeBlockWorld::expandPath( char* pDstPath, U32 size, const char* pSrcPath, const char* pWorkingDirectoryHint, const bool _ensureTrailingSlash )
{
   char pathBuffer[2048];
   const char* pSrc = pSrcPath;
   char* pSlash;
   
   // Fetch leading character.
   const char leadingToken = *pSrc;
   
   // Fetch following token.
   const char followingToken = leadingToken != 0 ? pSrc[1] : 0;
   
   // Expando.
   if ( leadingToken == '^' )
   {
      // Initial prefix search.
      const char* pPrefixSrc = pSrc+1;
      char* pPrefixDst = pathBuffer;
      
      // Search for end of expando.
      while( *pPrefixSrc != '/' && *pPrefixSrc != 0 )
      {
         // Copy prefix character.
         *pPrefixDst++ = *pPrefixSrc++;
      }
      
      // Yes, so terminate the expando string.
      *pPrefixDst = 0;
      
      // Fetch the expando path.
      StringTableEntry expandoPath = getPathExpando(pathBuffer);
      
      // Does the expando exist?
      if( expandoPath == NULL )
      {
         // No, so error.
         errorf("expandPath() : Could not find path expando '%s' for path '%s'.", pathBuffer, pSrcPath );
         
         // Are we ensuring the trailing slash?
         if ( _ensureTrailingSlash )
         {
            // Yes, so ensure it.
            ensureTrailingSlash( pDstPath, pSrcPath );
         }
         else
         {
            // No, so just use the source path.
            dStrcpy( pDstPath, pSrcPath );
         }
         
         return false;
      }
      
      // Skip the expando and the following slash.
      pSrc += dStrlen(pathBuffer) + 1;
      
      // Format the output path.
      dSprintf( pathBuffer, sizeof(pathBuffer), "%s/%s", expandoPath, pSrc );
      
      // Are we ensuring the trailing slash?
      if ( _ensureTrailingSlash )
      {
         // Yes, so ensure it.
         ensureTrailingSlash( pathBuffer, pathBuffer );
      }
      
      // Strip repeat slashes.
      stripRepeatSlashes( pDstPath, pathBuffer, size );
      
      return true;
   }
   
   // Script-Relative.
   if ( leadingToken == '.' )
   {
      // Fetch the code-block file-path
      const StringTableEntry codeblockFullPath = smCurrentCodeBlock ? smCurrentCodeBlock->fullPath : NULL;
      
      // Do we have a code block full path?
      if( codeblockFullPath == NULL )
      {
         // No, so error.
         errorf("expandPath() : Could not find relative path from code-block for path '%s'.", pSrcPath );
         
         // Are we ensuring the trailing slash?
         if ( _ensureTrailingSlash )
         {
            // Yes, so ensure it.
            ensureTrailingSlash( pDstPath, pSrcPath );
         }
         else
         {
            // No, so just use the source path.
            dStrcpy( pDstPath, pSrcPath );
         }
         
         return false;
      }
      
      // Yes, so use it as the prefix.
      dStrncpy(pathBuffer, codeblockFullPath, sizeof(pathBuffer) - 1);
      
      // Find the final slash in the code-block.
      pSlash = dStrrchr(pathBuffer, '/');
      
      // Is this a parent directory token?
      if ( followingToken == '.' )
      {
         // Yes, so terminate after the slash so we include it.
         pSlash[1] = 0;
      }
      else
      {
         // No, it's a current directory token so terminate at the slash so we don't include it.
         pSlash[0] = 0;
         
         // Skip the current directory token.
         pSrc++;
      }
      
      // Format the output path.
      dStrncat(pathBuffer, "/", sizeof(pathBuffer) - 1 - dStrlen(pathBuffer));
      dStrncat(pathBuffer, pSrc, sizeof(pathBuffer) - 1 - dStrlen(pathBuffer));
      
      // Are we ensuring the trailing slash?
      if ( _ensureTrailingSlash )
      {
         // Yes, so ensure it.
         ensureTrailingSlash( pathBuffer, pathBuffer );
      }
      
      // Strip repeat slashes.
      stripRepeatSlashes( pDstPath, pathBuffer, size );
      
      return true;
   }
   
   // All else.
   
   //Using a special case here because the code below barfs on trying to build a full path for apk reading
#ifdef TORQUE_OS_ANDROID
   if (leadingToken == '/' || strstr(pSrcPath, "/") == NULL)
      Platform::makeFullPathName( pSrcPath, pathBuffer, sizeof(pathBuffer), pWorkingDirectoryHint );
   else
      dSprintf(pathBuffer, sizeof(pathBuffer), "/%s", pSrcPath);
#else
   Platform::makeFullPathName( pSrcPath, pathBuffer, sizeof(pathBuffer), pWorkingDirectoryHint );
#endif
   
   // Are we ensuring the trailing slash?
   if ( _ensureTrailingSlash )
   {
      // Yes, so ensure it.
      ensureTrailingSlash( pathBuffer, pathBuffer );
   }
   
   // Strip repeat slashes.
   stripRepeatSlashes( pDstPath, pathBuffer, size );
   
   return true;
}

//-----------------------------------------------------------------------------

bool CodeBlockWorld::isBasePath( const char* SrcPath, const char* pBasePath )
{
   char expandBuffer[1024];
   expandPath( expandBuffer, sizeof(expandBuffer), SrcPath );
   return dStrnicmp(pBasePath, expandBuffer, dStrlen( pBasePath ) ) == 0;
}

//-----------------------------------------------------------------------------

void CodeBlockWorld::collapsePath( char* pDstPath, U32 size, const char* pSrcPath, const char* pWorkingDirectoryHint )
{
   // Check path against expandos.  If there are multiple matches, choose the
   // expando that produces the shortest relative path.
   
   char pathBuffer[2048];
   
   // Fetch expando count.
   const U32 expandoCount = getPathExpandoCount();
   
   // Iterate expandos.
   U32 expandoRelativePathLength = U32_MAX;
   for( U32 expandoIndex = 0; expandoIndex < expandoCount; ++expandoIndex )
   {
      // Fetch expando value (path).
      StringTableEntry expandoValue = getPathExpandoValue( expandoIndex );
      
      // Skip if not the base path.
      if ( !isBasePath( pSrcPath, expandoValue ) )
         continue;
      
      // Fetch path relative to expando path.
      StringTableEntry relativePath = Platform::makeRelativePathName( pSrcPath, expandoValue );
      
      // If the relative path is simply a period
      if ( relativePath[0] == '.' )
         relativePath++;
      
      if ( dStrlen(relativePath) > expandoRelativePathLength )
      {
         // This expando covers less of the path than any previous one found.
         // We will keep the previous one.
         continue;
      }
      
      // Keep track of the relative path length
      expandoRelativePathLength = dStrlen(relativePath);
      
      // Fetch expando key (name).
      StringTableEntry expandoName = getPathExpandoKey( expandoIndex );
      
      // Format against expando.
      dSprintf( pathBuffer, sizeof(pathBuffer), "^%s/%s", expandoName, relativePath );
   }
   
   // Check if we've found a suitable expando
   if ( expandoRelativePathLength != U32_MAX )
   {
      // Strip repeat slashes.
      stripRepeatSlashes( pDstPath, pathBuffer, size );
      
      return;
   }
   
   // Fetch the working directory.
   StringTableEntry workingDirectory = pWorkingDirectoryHint != NULL ? pWorkingDirectoryHint : Platform::getCurrentDirectory();
   
   // Fetch path relative to current directory.
   StringTableEntry relativePath = Platform::makeRelativePathName( pSrcPath, workingDirectory );
   
   // If the relative path is simply a period
   if ( relativePath[0] == '.'  && relativePath[1] != '.' )
      relativePath++;
   
   // Format against expando.
   dSprintf( pathBuffer, sizeof(pathBuffer), "%s/%s", workingDirectory, relativePath );
   
   // Strip repeat slashes.
   stripRepeatSlashes( pDstPath, pathBuffer, size );
}

//-----------------------------------------------------------------------------

void CodeBlockWorld::ensureTrailingSlash( char* pDstPath, const char* pSrcPath )
{
   // Copy to target.
   dStrcpy( pDstPath, pSrcPath );
   
   // Find trailing character index.
   S32 trailIndex = dStrlen(pDstPath);
   
   // Ignore if empty string.
   if ( trailIndex == 0 )
      return;
   
   // Finish if the trailing slash already exists.
   if ( pDstPath[trailIndex-1] == '/' )
      return;
   
   // Add trailing slash.
   pDstPath[trailIndex++] = '/';
   pDstPath[trailIndex] = 0;
}

//-----------------------------------------------------------------------------

bool CodeBlockWorld::stripRepeatSlashes( char* pDstPath, const char* pSrcPath, S32 dstSize )
{
   // Note original destination.
   char* pOriginalDst = pDstPath;
   
   // Reset last source character.
   char lastSrcChar = 0;
   
   // Search source...
   while ( dstSize > 0 )
   {
      // Fetch characters.
      const char srcChar = *pSrcPath++;
      
      // Do we have a repeat slash?
      if ( srcChar == '/' && lastSrcChar == '/' )
      {
         // Yes, so skip it.
         continue;
      }
      
      // No, so copy character.
      *pDstPath++ = srcChar;
      
      // Finish if end of source.
      if ( srcChar == 0 )
         return true;
      
      // Reduce room left in destination.
      dstSize--;
      
      // Set last character.
      lastSrcChar = srcChar;
   }
   
   // Terminate the destination string as we ran out of room.
   *pOriginalDst = 0;
   
   // Fail!
   return false;
}



char *CodeBlockWorld::getReturnBuffer(U32 bufferSize)
{
   return gEvalState->STR.getReturnBuffer(bufferSize);
}

char *CodeBlockWorld::getReturnBuffer( const char *stringToCopy )
{
   char *ret = gEvalState->STR.getReturnBuffer( dStrlen( stringToCopy ) + 1 );
   dStrcpy( ret, stringToCopy );
   ret[dStrlen( stringToCopy )] = '\0';
   return ret;
}

char *CodeBlockWorld::getArgBuffer(U32 bufferSize)
{
   return gEvalState->STR.getArgBuffer(bufferSize);
}

char *CodeBlockWorld::getFloatArg(F64 arg)
{
   char *ret = gEvalState->STR.getArgBuffer(32);
   dSprintf(ret, 32, "%g", arg);
   return ret;
}

char *CodeBlockWorld::getIntArg(S32 arg)
{
   char *ret = gEvalState->STR.getArgBuffer(32);
   dSprintf(ret, 32, "%d", arg);
   return ret;
}

char* CodeBlockWorld::getBoolArg(bool arg)
{
   char *ret = gEvalState->STR.getArgBuffer(32);
   dSprintf(ret, 32, "%d", arg);
   return ret;
}

char *CodeBlockWorld::getStringArg( const char *arg )
{
   U32 len = dStrlen( arg ) + 1;
   char *ret = gEvalState->STR.getArgBuffer( len );
   dMemcpy( ret, arg, len );
   return ret;
}


const char *CodeBlockWorld::getNamespaceList(Namespace *ns)
{
   U32 size = 1;
   Namespace * walk;
   for(walk = ns; walk; walk = walk->mParent)
      size += dStrlen(walk->mName) + 4;
   char *ret = getReturnBuffer(size);
   ret[0] = 0;
   for(walk = ns; walk; walk = walk->mParent)
   {
      dStrcat(ret, walk->mName);
      if(walk->mParent)
         dStrcat(ret, " -> ");
   }
   return ret;
}



CodeBlock *CodeBlockWorld::findCodeBlock(StringTableEntry name)
{
   for(CodeBlock *walk = smCodeBlockList; walk; walk = walk->nextFile)
      if(walk->name == name)
         return walk;
   return NULL;
}
