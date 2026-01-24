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
#include "core/stringTable.h"
#include "embed/api.h"
#include "embed/internalApi.h"
#include "console/consoleInternal.h"
#include "console/consoleNamespace.h"
#include "console/compiler.h"
#include "console/telnetDebugger.h"


//
// Enhanced TelnetDebugger for Torsion
// http://www.sickheadgames.com/torsion
//
//
// Debugger commands:
//
// CEVAL console line - evaluate the console line
//    output: none
//
// BRKVARSET varName passct expr - NOT IMPLEMENTED!
//    output: none
//
// BRKVARCLR varName - NOT IMPLEMENTED!
//    output: none
//
// BRKSET file line clear passct expr - set a breakpoint on the file,line
//        it must pass passct times for it to break and if clear is true, it
//        clears when hit
//    output: 
//
// BRKNEXT - stop execution at the next breakable line.
//    output: none
//
// BRKCLR file line - clear a breakpoint on the file,line
//    output: none
//
// BRKCLRALL - clear all breakpoints
//    output: none
//
// CONTINUE - continue execution
//    output: RUNNING
//
// STEPIN - run until next statement
//    output: RUNNING
//
// STEPOVER - run until next break <= current frame
//    output: RUNNING
//
// STEPOUT - run until next break <= current frame - 1
//    output: RUNNING
//
// EVAL tag frame expr - evaluate the expr in the console, on the frame'th stack frame
//    output: EVALOUT tag exprResult
//
// FILELIST - list script files loaded
//    output: FILELISTOUT file1 file2 file3 file4 ...
//
// BREAKLIST file - get a list of breakpoint-able lines in the file
//    output: BREAKLISTOUT file skipBreakPairs skiplinecount breaklinecount skiplinecount breaklinecount ...
//
//
// Other output:
//
// BREAK file1 line1 function1 file2 line2 function2 ... - Sent when the debugger hits a 
//          breakpoint.  It lists out one file/line/function triplet for each stack level.
//          The first one is the top of the stack.
//
// COUT console-output - echo of console output from engine
//
// BRKMOV file line newline - sent when a breakpoint is moved to a breakable line.
//
// BRKCLR file line - sent when a breakpoint cannot be moved to a breakable line on the client.
//

static void debuggerConsumer(U32 level, const char *line, void* userPtr)
{
   TelnetDebugger* debugger = (TelnetDebugger*)userPtr;
   if (debugger)
      debugger->processConsoleLine(line);
}

TelnetDebugger::TelnetDebugger(KorkApi::VmInternal* vm)
{
   mVMInternal->mConfig.extraConsumers[1].cbFunc = debuggerConsumer;
   mVMInternal->mConfig.extraConsumers[1].cbUser = this;
   mVMInternal = vm;
   mCurrentWatchFiber = NULL;
   
   mAcceptPort = -1;
   
   mState = NotConnected;
   mCurPos = 0;
   
   mBreakpoints = NULL;
   mBreakOnNextStatement = false;
   mProgramPaused = false;
   mWaitForClient = false;

   mValid = false;
   
   // Add the version number in a global so that
   // scripts can detect the presence of the
   // "enhanced" debugger features.
   char buf[32];
   snprintf(buf, 32, "$dbgVersion = %d;", Version );
   mVMInternal->mVM->evalCode(buf, "");
}

TelnetDebugger::Breakpoint **TelnetDebugger::findBreakpoint(StringTableEntry fileName, S32 lineNumber)
{
   Breakpoint **walk = &mBreakpoints;
   Breakpoint *cur;
   while((cur = *walk) != NULL)
   {
      // TODO: This assumes that the OS file names are case
      // insensitive... Torque needs a dFilenameCmp() function.
      if( dStricmp( cur->fileName, fileName ) == 0 && cur->lineNumber == U32(lineNumber))
         return walk;
      walk = &cur->next;
   }
   return NULL;
}


TelnetDebugger::~TelnetDebugger()
{
   if (mVMInternal->mConfig.extraConsumers[1].cbUser == this)
   {
      mVMInternal->mConfig.extraConsumers[1].cbFunc = NULL;
      mVMInternal->mConfig.extraConsumers[1].cbUser = NULL;
   }

   if (mValid)
   {
      mVMInternal->mConfig.iTelnet.StopListenFn(mVMInternal->mConfig.telnetUser, KorkApi::TELNET_DEBUGGER);
   }
}

void TelnetDebugger::send(const char *str)
{
   if (mDebugSocket != 0)
   {
      mVMInternal->mConfig.iTelnet.SendDataFn(mVMInternal->mConfig.telnetUser, mDebugSocket, dStrlen(str), (const unsigned char*)str);
   }
}

void TelnetDebugger::disconnect()
{
   if (mDebugSocket != 0)
   {
      mVMInternal->mConfig.iTelnet.StopSocketFn(mVMInternal->mConfig.telnetUser, mDebugSocket);
      mDebugSocket = 0;
   }
   
   removeAllBreakpoints();
   
   mState = NotConnected;
   mProgramPaused = false;
}

void TelnetDebugger::setDebugParameters(S32 port, const char *password, bool waitForClient)
{
   if (mVMInternal->mConfig.iTelnet.StartListenFn(mVMInternal->mConfig.telnetUser, KorkApi::TELNET_DEBUGGER, port))
   {
      mAcceptPort = port;
   }
   else
   {
      mAcceptPort = -1;
   }

   dStrncpy(mDebuggerPassword, password, PasswordMaxLength);
   
   mWaitForClient = waitForClient;
   if ( !mWaitForClient )
      return;
   
   // Wait for the client to fully connect.
   while ( mState != Connected  )
   {
      // TOFIX Platform::sleep(10);
      process();
   }
   
}

void TelnetDebugger::processConsoleLine(const char *consoleLine)
{
   if(mState != NotConnected)
   {
      send("COUT ");
      send(consoleLine);
      send("\r\n");
   }
}

void TelnetDebugger::process()
{
   KorkApi::Config& cfg = mVMInternal->mConfig;
   KorkApi::TelnetInterface& tel = cfg.iTelnet;

   if (!mValid)
   {
      return;
   }
   
   if (tel.CheckListenFn(cfg.telnetUser, KorkApi::TELNET_DEBUGGER))
   {
      // ok, see if we have any new connections:
      U32 newConnection = tel.CheckAcceptFn(cfg.telnetUser, KorkApi::TELNET_DEBUGGER);
      
      if(newConnection != 0 && mDebugSocket == 0)
      {
         char buffer[256];
         tel.GetSocketAddressFn(cfg.telnetUser, newConnection, buffer);
         mVMInternal->printf(0, "Debugger connection from %s", buffer);
         
         mState = PasswordTry;
         mDebugSocket = newConnection;
      }
      else if(newConnection != 0)
      {
         tel.StopSocketFn(cfg.telnetUser, newConnection);
      }
   }
   // see if we have any input to process...
   
   if (mDebugSocket == 0)
      return;
   
   checkDebugRecv();
   if (mDebugSocket == 0)
      removeAllBreakpoints();
}

void TelnetDebugger::checkDebugRecv()
{
   KorkApi::Config& cfg = mVMInternal->mConfig;
   KorkApi::TelnetInterface& tel = cfg.iTelnet;

   for (;;)
   {
      // Process all the complete commands in the buffer.
      while ( mCurPos > 0 )
      {
         // Remove leading whitespace.
         while ( mCurPos > 0 && ( mLineBuffer[0] == 0 || mLineBuffer[0] == '\r' || mLineBuffer[0] == '\n' ) )
         {
            mCurPos--;
            dMemmove(mLineBuffer, mLineBuffer + 1, mCurPos);
         }
         
         // Look for a complete command.
         bool gotCmd = false;
         for(S32 i = 0; i < mCurPos; i++)
         {
            if( mLineBuffer[i] == 0 )
               mLineBuffer[i] = '_';
            
            else if ( mLineBuffer[i] == '\r' || mLineBuffer[i] == '\n' )
            {
               // Send this command to be processed.
               mLineBuffer[i] = '\n';
               processLineBuffer(i+1);
               
               // Remove the command from the buffer.
               mCurPos -= i + 1;
               dMemmove(mLineBuffer, mLineBuffer + i + 1, mCurPos);
               
               gotCmd = true;
               break;
            }
         }
         
         // If we didn't find a command in this pass
         // then we have an incomplete buffer.
         if ( !gotCmd )
            break;
      }
      
      // found no <CR> or <LF>
      if(mCurPos == MaxCommandSize) // this shouldn't happen
      {
         disconnect();
         return;
      }

      U32 numBytes = 0;

      if (!tel.RecvDataFn(cfg.telnetUser, mDebugSocket, (unsigned char*)(mLineBuffer + mCurPos), MaxCommandSize - mCurPos, &numBytes))
      {
         disconnect();
         return;
      }
      
      mCurPos += numBytes;
   }
}

void TelnetDebugger::executionStopped(CodeBlock *code, U32 lineNumber)
{
   if (mProgramPaused)
      return;

   // Need to switch to whatever fiber we are on
   setWatchFiberFromVm();
   
   if (mBreakOnNextStatement)
   {
      setBreakOnNextStatement( false );
      breakProcess();
      return;
   }
   
   Breakpoint **bp = findBreakpoint(code->name, lineNumber);
   if(!bp)
      return;
   
   Breakpoint *brk = *bp;
   mProgramPaused = true;

   char buf[256];
   snprintf(buf, 256, "$Debug::result = %s;", brk->testExpression);
   mVMInternal->mVM->evalCode(buf, "");

   KorkApi::ConsoleValue cv = mVMInternal->mVM->getGlobalVariable("$Debug::result");

   if (mVMInternal->valueAsBool(cv))
   {
      brk->curCount++;
      if(brk->curCount >= brk->passCount)
      {
         brk->curCount = 0;
         if(brk->clearOnHit)
            removeBreakpoint(code->name, lineNumber);
         breakProcess();
      }
   }
   mProgramPaused = false;
}

bool TelnetDebugger::isWatchedFiber()
{
   return mCurrentWatchFiber == mVMInternal->mCurrentFiberState;
}

void TelnetDebugger::pushStackFrame()
{
   if(mState == NotConnected || !isWatchedFiber())
      return;
   
   if(mBreakOnNextStatement && 
      mCurrentWatchFiber->mStackPopBreakIndex > -1 &&
      mCurrentWatchFiber->vmFrames.size() > mCurrentWatchFiber->mStackPopBreakIndex)
   {
      setBreakOnNextStatement( false );
   }
}

void TelnetDebugger::popStackFrame()
{
   if(mState == NotConnected || !isWatchedFiber())
      return;
   
   if(mCurrentWatchFiber->mStackPopBreakIndex > -1 && 
      mCurrentWatchFiber->vmFrames.size()-1 <= mCurrentWatchFiber->mStackPopBreakIndex)
   {
      setBreakOnNextStatement( true );
   }
}

void TelnetDebugger::breakProcess()
{
   if (!mValid)
   {
      return;
   }

   // Send out a break with the full stack.
   sendBreak();
   
   mProgramPaused = true;
   while (mProgramPaused)
   {
      // TOFIX Platform::sleep(10);
      checkDebugRecv();
      if(mDebugSocket == 0)
      {
         mProgramPaused = false;
         removeAllBreakpoints();
         debugContinue();
         return;
      }
   }
}

void TelnetDebugger::sendBreak()
{
   // echo out the break
   send("BREAK");
   char buffer[MaxCommandSize];
   char scope[MaxCommandSize];
   
   S32 last = 0;
   
   if (mCurrentWatchFiber)
   {
      for(S32 i = (S32) mCurrentWatchFiber->vmFrames.size() - 1; i >= last; i--)
      {
         ConsoleBasicFrame frameInfo = mCurrentWatchFiber->getBasicFrameInfo(i);
         CodeBlock *code = frameInfo.code;
         const char *file = "<none>";
         if (code && code->name && code->name[0])
            file = code->name;
         
         Namespace *ns = frameInfo.scopeNamespace;
         scope[0] = 0;
         if ( ns ) {
            
            if ( ns->mParent && ns->mParent->mPackage && ns->mParent->mPackage[0] ) {
               dStrcat( scope, ns->mParent->mPackage );
               dStrcat( scope, "::" );
            }
            if ( ns->mName && ns->mName[0] ) {
               dStrcat( scope, ns->mName );
               dStrcat( scope, "::" );
            }
         }
         
         const char *function = frameInfo.scopeName;
         if ((!function) || (!function[0]))
            function = "<none>";
         dStrcat( scope, function );
         
         U32 line=0, inst;
         U32 ip = frameInfo.ip;
         if (code)
            code->findBreakLine(ip, line, inst);
         dSprintf(buffer, MaxCommandSize, " %s %d %s", file, line, scope);
         send(buffer);
      }
   }
   
   send("\r\n");
}

void TelnetDebugger::processLineBuffer(S32 cmdLen)
{
   KorkApi::Config& cfg = mVMInternal->mConfig;
   KorkApi::TelnetInterface& tel = cfg.iTelnet;

   if (mState == PasswordTry)
   {
      if(dStrncmp(mLineBuffer, mDebuggerPassword, cmdLen-1))
      {
         // failed password:
         send("PASS WrongPassword.\r\n");
         disconnect();
      }
      else
      {
         send("PASS Connected.\r\n");
         mState = mWaitForClient ? Initialize : Connected;
      }
      
      mCurrentWatchFiber = NULL;
      setWatchFiberFromVm();
      return;
   }
   else
   {
      char evalBuffer[MaxCommandSize];
      char varBuffer[MaxCommandSize];
      char fileBuffer[MaxCommandSize];
      char clear[MaxCommandSize];
      U32 setFiberId = 0;
      S32 passCount, line, frame;
      ExprEvalState* existingEvalState = mVMInternal->mCurrentFiberState;
      
      if(dSscanf(mLineBuffer, "CEVAL %[^\n]", evalBuffer) == 1)
      {
         mVMInternal->mCurrentFiberState = mCurrentWatchFiber;
         if (cfg.iTelnet.QueueEvaluateFn)
         {
            tel.QueueEvaluateFn(cfg.telnetUser, evalBuffer);
         }
      }
      else if(dSscanf(mLineBuffer, "BRKVARSET %s %d %[^\n]", varBuffer, &passCount, evalBuffer) == 3)
      {
         mVMInternal->mCurrentFiberState = mCurrentWatchFiber;
         addVariableBreakpoint(varBuffer, passCount, evalBuffer);
      }
      else if(dSscanf(mLineBuffer, "BRKVARCLR %s", varBuffer) == 1)
      {
         mVMInternal->mCurrentFiberState = mCurrentWatchFiber;
         removeVariableBreakpoint(varBuffer);
      }
      else if(dSscanf(mLineBuffer, "BRKSET %s %d %s %d %[^\n]", fileBuffer,&line,&clear,&passCount,evalBuffer) == 5)
      {
         mVMInternal->mCurrentFiberState = mCurrentWatchFiber;
         addBreakpoint(fileBuffer, line, dAtob(clear), passCount, evalBuffer);
      }
      else if(dSscanf(mLineBuffer, "BRKCLR %s %d", fileBuffer, &line) == 2)
      {
         mVMInternal->mCurrentFiberState = mCurrentWatchFiber;
         removeBreakpoint(fileBuffer, line);
      }
      else if(!dStrncmp(mLineBuffer, "BRKCLRALL\n", cmdLen))
      {
         mVMInternal->mCurrentFiberState = mCurrentWatchFiber;
         removeAllBreakpoints();
      }
      else if(!dStrncmp(mLineBuffer, "BRKNEXT\n", cmdLen))
      {
         mVMInternal->mCurrentFiberState = mCurrentWatchFiber;
         debugBreakNext();
      }
      else if(!dStrncmp(mLineBuffer, "CONTINUE\n", cmdLen))
      {
         mVMInternal->mCurrentFiberState = mCurrentWatchFiber;
         debugContinue();
      }
      else if(!dStrncmp(mLineBuffer, "STEPIN\n", cmdLen))
      {
         mVMInternal->mCurrentFiberState = mCurrentWatchFiber;
         debugStepIn();
      }
      else if(!dStrncmp(mLineBuffer, "STEPOVER\n", cmdLen))
      {
         mVMInternal->mCurrentFiberState = mCurrentWatchFiber;
         debugStepOver();
      }
      else if(!dStrncmp(mLineBuffer, "STEPOUT\n", cmdLen))
      {
         mVMInternal->mCurrentFiberState = mCurrentWatchFiber;
         debugStepOut();
      }
      else if(dSscanf(mLineBuffer, "EVAL %s %d %[^\n]", varBuffer, &frame, evalBuffer) == 3)
      {
         mVMInternal->mCurrentFiberState = mCurrentWatchFiber;
         evaluateExpression(varBuffer, frame, evalBuffer);
      }
      else if(!dStrncmp(mLineBuffer, "FILELIST\n", cmdLen))
      {
         mVMInternal->mCurrentFiberState = mCurrentWatchFiber;
         dumpFileList();
      }
      else if(dSscanf(mLineBuffer, "BREAKLIST %s", fileBuffer) == 1)
      {
         mVMInternal->mCurrentFiberState = mCurrentWatchFiber;
         dumpBreakableList(fileBuffer);
      }
      else if(dSscanf(mLineBuffer, "SETFIBER %u", setFiberId) == 1)
      {
         mVMInternal->setCurrentFiber(setFiberId);
         setWatchFiberFromVm();
      }
      else
      {
         S32 errorLen = dStrlen(mLineBuffer) + 32; // ~25 in error message, plus buffer
         KorkApi::Vector<char> errorBuffer(errorLen);
         char* usageStr = errorBuffer.data();
         
         dSprintf( errorBuffer.data(), errorLen, "DBGERR Invalid command(%s)!\r\n", mLineBuffer );
         // invalid stuff.
         send( errorBuffer.data() );
      }

      if (mVMInternal->mCurrentFiberState != existingEvalState)
      {
         mVMInternal->mCurrentFiberState = existingEvalState;
      }
   }
}

void TelnetDebugger::addVariableBreakpoint(const char*, S32, const char*)
{
   send("addVariableBreakpoint\r\n");
}

void TelnetDebugger::removeVariableBreakpoint(const char*)
{
   send("removeVariableBreakpoint\r\n");
}

void TelnetDebugger::addAllBreakpoints(CodeBlock *code)
{
   if(mState == NotConnected)
      return;
   
   // Find the breakpoints for this code block and attach them.
   Breakpoint *cur = mBreakpoints;
   while( cur != NULL )
   {
      // TODO: This assumes that the OS file names are case
      // insensitive... Torque needs a dFilenameCmp() function.
      if( dStricmp( cur->fileName, code->name ) == 0 )
      {
         cur->code = code;
         
         // Find the fist breakline starting from and
         // including the requested breakline.
         S32 newLine = code->findFirstBreakLine(cur->lineNumber);
         if (newLine <= 0)
         {
            char buffer[MaxCommandSize];
            dSprintf(buffer, MaxCommandSize, "BRKCLR %s %d\r\n", cur->fileName, cur->lineNumber);
            send(buffer);
            
            Breakpoint *next = cur->next;
            removeBreakpoint(cur->fileName, cur->lineNumber);
            cur = next;
            
            continue;
         }
         
         // If the requested breakline does not match
         // the actual break line we need to inform
         // the client.
         if (newLine != cur->lineNumber)
         {
            char buffer[MaxCommandSize];
            
            // If we already have a line at this breapoint then
            // tell the client to clear the breakpoint.
            if ( findBreakpoint(cur->fileName, newLine) ) {
               
               dSprintf(buffer, MaxCommandSize, "BRKCLR %s %d\r\n", cur->fileName, cur->lineNumber);
               send(buffer);
               
               Breakpoint *next = cur->next;
               removeBreakpoint(cur->fileName, cur->lineNumber);
               cur = next;
               
               continue;
            }
            
            // We're moving the breakpoint to new line... inform the
            // client so it can update it's view.
            dSprintf(buffer, MaxCommandSize, "BRKMOV %s %d %d\r\n", cur->fileName, cur->lineNumber, newLine);
            send(buffer);
            cur->lineNumber = newLine;
         }
         
         code->setBreakpoint(cur->lineNumber);
      }
      
      cur = cur->next;
   }
   
   // Enable all breaks if a break next was set.
   if (mBreakOnNextStatement)
      code->setAllBreaks();
}

void TelnetDebugger::addBreakpoint(const char *fileName, S32 line, bool clear, S32 passCount, const char *evalString)
{
   fileName = StringTable->insert(fileName);
   Breakpoint **bp = findBreakpoint(fileName, line);
   
   if(bp)
   {
      // trying to add the same breakpoint...
      Breakpoint *brk = *bp;
      dFree(brk->testExpression);
      brk->testExpression = dStrdup(evalString);
      brk->passCount = passCount;
      brk->clearOnHit = clear;
      brk->curCount = 0;
   }
   else
   {
      // Note that if the code block is not already
      // loaded it is handled by addAllBreakpoints.
      CodeBlock* code = mVMInternal->findCodeBlock(fileName);
      if (code)
      {
         // Find the fist breakline starting from and
         // including the requested breakline.
         S32 newLine = code->findFirstBreakLine(line);
         if (newLine <= 0)
         {
            char buffer[MaxCommandSize];
            dSprintf(buffer, MaxCommandSize, "BRKCLR %s %d\r\n", fileName, line);
            send(buffer);
            return;
         }
         
         // If the requested breakline does not match
         // the actual break line we need to inform
         // the client.
         if (newLine != line)
         {
            char buffer[MaxCommandSize];
            
            // If we already have a line at this breapoint then
            // tell the client to clear the breakpoint.
            if ( findBreakpoint(fileName, newLine) ) {
               dSprintf(buffer, MaxCommandSize, "BRKCLR %s %d\r\n", fileName, line);
               send(buffer);
               return;
            }
            
            // We're moving the breakpoint to new line... inform the client.
            dSprintf(buffer, MaxCommandSize, "BRKMOV %s %d %d\r\n", fileName, line, newLine);
            send(buffer);
            line = newLine;
         }
         
         code->setBreakpoint(line);
      }
      
      Breakpoint *brk = mVMInternal->New<Breakpoint>();
      brk->code = code;
      brk->fileName = fileName;
      brk->lineNumber = line;
      brk->passCount = passCount;
      brk->clearOnHit = clear;
      brk->curCount = 0;
      brk->testExpression = dStrdup(evalString);
      brk->next = mBreakpoints;
      mBreakpoints = brk;
   }
}

void TelnetDebugger::removeBreakpointsFromCode(CodeBlock *code)
{
   Breakpoint **walk = &mBreakpoints;
   Breakpoint *cur;
   while((cur = *walk) != NULL)
   {
      if(cur->code == code)
      {
         dFree(cur->testExpression);
         *walk = cur->next;
         delete walk;
      }
      else
         walk = &cur->next;
   }
}

void TelnetDebugger::removeBreakpoint(const char *fileName, S32 line)
{
   fileName = StringTable->insert(fileName);
   Breakpoint **bp = findBreakpoint(fileName, line);
   if(bp)
   {
      Breakpoint *brk = *bp;
      *bp = brk->next;
      if ( brk->code )
         brk->code->clearBreakpoint(brk->lineNumber);
      dFree(brk->testExpression);
      delete brk;
   }
}

void TelnetDebugger::removeAllBreakpoints()
{
   Breakpoint *walk = mBreakpoints;
   while(walk)
   {
      Breakpoint *temp = walk->next;
      if ( walk->code )
         walk->code->clearBreakpoint(walk->lineNumber);
      dFree(walk->testExpression);
      delete walk;
      walk = temp;
   }
   mBreakpoints = NULL;
}

void TelnetDebugger::debugContinue()
{
   if (mState == Initialize) {
      mState = Connected;
      return;
   }
   
   setBreakOnNextStatement( false );
   if (mCurrentWatchFiber)
   {
      mCurrentWatchFiber->mStackPopBreakIndex = -1;
   }
   mProgramPaused = false;
   send("RUNNING\r\n");
}

void TelnetDebugger::setBreakOnNextStatement( bool enabled )
{
   if ( enabled )
   {
      // Apply breaks on all the code blocks.
      for(CodeBlock *walk = mVMInternal->mCodeBlockList; walk; walk = walk->nextFile)
         walk->setAllBreaks();
      mBreakOnNextStatement = true;
   }
   else if ( !enabled )
   {
      // Clear all the breaks on the codeblocks
      // then go reapply the breakpoints.
      for(CodeBlock *walk = mVMInternal->mCodeBlockList; walk; walk = walk->nextFile)
         walk->clearAllBreaks();
      for(Breakpoint *w = mBreakpoints; w; w = w->next)
      {
         if ( w->code )
            w->code->setBreakpoint(w->lineNumber);
      }
      mBreakOnNextStatement = false;
   }
}

void TelnetDebugger::debugBreakNext()
{
   if (mState != Connected)
      return;
   
   if ( !mProgramPaused )
      setBreakOnNextStatement( true );
}

void TelnetDebugger::debugStepIn()
{
   // Note that step in is allowed during
   // the initialize state, so that we can
   // break on the first script line executed.
   
   setBreakOnNextStatement( true );
   if (mCurrentWatchFiber)
   {
      mCurrentWatchFiber->mStackPopBreakIndex = -1;
   }
   mProgramPaused = false;
   
   // Don't bother sending this to the client
   // if it's in the initialize state.  It will
   // just be ignored as the client knows it
   // is in a running state when it connects.
   if (mState != Initialize)
      send("RUNNING\r\n");
   else
      mState = Connected;
}

void TelnetDebugger::debugStepOver()
{
   if (mState != Connected)
      return;
   
   setBreakOnNextStatement( true );
   if (mCurrentWatchFiber)
   {
      mCurrentWatchFiber->mStackPopBreakIndex = mCurrentWatchFiber->vmFrames.size();
   }
   mProgramPaused = false;
   send("RUNNING\r\n");
}

void TelnetDebugger::debugStepOut()
{
   if (mState != Connected)
      return;
   
   setBreakOnNextStatement( false );
   if (mCurrentWatchFiber)
   {
      mCurrentWatchFiber->mStackPopBreakIndex = mCurrentWatchFiber->vmFrames.size() - 1;
      if ( mCurrentWatchFiber->mStackPopBreakIndex == 0 )
         mCurrentWatchFiber->mStackPopBreakIndex = -1;
   }
   mProgramPaused = false;
   send("RUNNING\r\n");
}

void TelnetDebugger::evaluateExpression(const char *tag, S32 frame, const char *evalBuffer)
{
   if (!mCurrentWatchFiber)
      return;

   // Make sure we're passing a valid frame to the eval.
   if ( frame > mCurrentWatchFiber->vmFrames.size() )
      frame = mCurrentWatchFiber->vmFrames.size() - 1;
   if ( frame < 0 )
      frame = 0;
   
   // Build a buffer just big enough for this eval.
   const char* format = "return %s;";
   dsize_t len = dStrlen( format ) + dStrlen( evalBuffer );
   char* buffer = mVMInternal->NewArray<char>(len);
   dSprintf( buffer, len, format, evalBuffer );
   
   // Execute the eval.
   KorkApi::ConsoleValue res = mVMInternal->mVM->evalCode(evalBuffer, NULL, frame);
   const char* result = mVMInternal->valueAsString(res);
   delete [] buffer;
   
   // Create a new buffer that fits the result.
   format = "EVALOUT %s %s\r\n";
   len = dStrlen( format ) + dStrlen( tag ) + dStrlen( result );
   buffer = mVMInternal->NewArray<char>(len);
   dSprintf( buffer, len, format, tag, result[0] ? result : "\"\"" );
   
   send( buffer );
   delete [] buffer;
}

void TelnetDebugger::setWatchFiberFromVm()
{
   if (mCurrentWatchFiber != mVMInternal->mCurrentFiberState)
   {
      mCurrentWatchFiber = mVMInternal->mCurrentFiberState;
      onFiberChanged();
   }
}

void TelnetDebugger::enumerateFibers()
{
   send("FIBERLIST ");
   mVMInternal->mFiberStates.forEach([this](ExprEvalState* state){
      char buffer[MaxCommandSize];
      U32 fiberId = mVMInternal->mFiberStates.getHandleValue(state);
      dSprintf(buffer, sizeof(buffer), "F %u %s", fiberId, KorkApi::FiberRunResult::stateAsString(state->mState));
      send(buffer);
   });
   // TODO
}

void TelnetDebugger::dumpFileList()
{
   send("FILELISTOUT ");
   for(CodeBlock *walk = mVMInternal->mCodeBlockList; walk; walk = walk->nextFile)
   {
      send(walk->name);
      if(walk->nextFile)
         send(" ");
   }
   send("\r\n");
}

void TelnetDebugger::dumpBreakableList(const char *fileName)
{
   fileName = StringTable->insert(fileName);
   CodeBlock *file = mVMInternal->findCodeBlock(fileName);
   char buffer[MaxCommandSize];
   if(file)
   {
      dSprintf(buffer, MaxCommandSize, "BREAKLISTOUT %s %d", fileName, file->breakListSize >> 1);
      send(buffer);
      for(U32 i = 0; i < file->breakListSize; i += 2)
      {
         dSprintf(buffer, MaxCommandSize, " %d %d", file->breakList[i], file->breakList[i+1]);
         send(buffer);
      }
      send("\r\n");
   }
   else
      send("DBGERR No such file!\r\n");
}


void TelnetDebugger::clearCodeBlockPointers(CodeBlock *code)
{
   Breakpoint **walk = &mBreakpoints;
   Breakpoint *cur;
   while((cur = *walk) != NULL)
   {
      if(cur->code == code)
         cur->code = NULL;
      
      walk = &cur->next;
   }
}

void TelnetDebugger::onFiberChanged()
{
   if (!mCurrentWatchFiber)
   {
      send("FIBER 0");
      return;
   }
   else
   {
      char buffer[MaxCommandSize];
      mCurrentWatchFiber->mStackPopBreakIndex = -1; // reset this
      U32 fiberId = mVMInternal->mFiberStates.getHandleValue(mCurrentWatchFiber);
      dSprintf(buffer, MaxCommandSize, "FIBER %u", fiberId);
      send(buffer);
   }
}
