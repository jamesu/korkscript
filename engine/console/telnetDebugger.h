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

#ifndef _TELNETDEBUGGER_H_
#define _TELNETDEBUGGER_H_

class CodeBlock;

#include "console/consoleNamespace.h"
#include "embed/api.h"
#include "embed/internalApi.h"

/// Telnet debug service implementation.
///
/// This is the C++ side of the built-in Torque debugger.
///
/// To use the debugger, use dbgSetParameters(port, password); in the console
/// of the server to enable debugger connections.  Then on some other system,
/// start up the app (you don't have to start a game or connect to the
/// server) and exec("common/debugger/debugger.cs"); in the console.  Then use
/// the debugger GUI to connect to the server with the right port and password.
///
/// @see http://www.planettribes.com/tribes2/editing.shtml for more thorough discussion.
class TelnetDebugger
{
   enum {
      
      // We should only change this is we truely 
      // break the protocol in a future version.
      Version = 2,

      PasswordMaxLength = 32,
      MaxCommandSize = 2048
   };

   char mDebuggerPassword[PasswordMaxLength+1];
   enum State
   {
      NotConnected,
      PasswordTry,
      Initialize,
      Connected
   };
   S32 mState;
   S32 mAcceptPort;
   char mLineBuffer[MaxCommandSize];
   S32 mCurPos;
   U32 mDebugSocket;
   bool mValid;
   bool mWaitForClient;

   KorkApi::VmInternal* mVMInternal;

public:
   TelnetDebugger(KorkApi::VmInternal* vm);
   ~TelnetDebugger();

   struct Breakpoint
   {
      StringTableEntry fileName;
      CodeBlock *code;
      U32 lineNumber;
      S32 passCount;
      S32 curCount;
      KorkApi::String testExpression;
      bool clearOnHit;
      Breakpoint *next;
   };
   Breakpoint *mBreakpoints;

   Breakpoint **findBreakpoint(StringTableEntry fileName, S32 lineNumber);

   bool mProgramPaused;
   bool mBreakOnNextStatement;
   ExprEvalState* mCurrentWatchFiber;

   bool isWatchedFiber();

   void addVariableBreakpoint(const char *varName, S32 passCount, const char *evalString);
   void removeVariableBreakpoint(const char *varName);
   void addBreakpoint(const char *fileName, S32 line, bool clear, S32 passCount, const char *evalString);
   void removeBreakpoint(const char *fileName, S32 line);
   void removeAllBreakpoints();

   void debugBreakNext();
   void debugContinue();
   void debugStepIn();
   void debugStepOver();
   void debugStepOut();
   void evaluateExpression(const char *tag, S32 frame, const char *evalBuffer);
   void dumpFileList();
   void dumpBreakableList(const char *fileName);
   void removeBreakpointsFromCode(CodeBlock *code);

   void checkDebugRecv();
   void processLineBuffer(S32);
   void sendBreak();
   void setBreakOnNextStatement( bool enabled );

   void setWatchFiberFromVm();
   void enumerateFibers();
   void onFiberChanged();
public:

   void disconnect();
   bool isConnected() const { return mState == Connected; }

   void process();
   void popStackFrame();
   void pushStackFrame();
   void addAllBreakpoints(CodeBlock *code);

   void clearCodeBlockPointers(CodeBlock *code);

   void breakProcess();

   void executionStopped(CodeBlock *code, U32 lineNumber);
   void send(const char *s);
   void setDebugParameters(S32 port, const char *password, bool waitForClient);
   void processConsoleLine(const char *consoleLine);
};

#endif

