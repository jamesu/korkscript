//-----------------------------------------------------------------------------
// Copyright (c) 2012 GarageGames, LLC
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
#include "console/console.h"
#include "console/consoleInternal.h"
#include "console/consoleObject.h"
#include "core/fileStream.h"

#include "console/ast.h"
#include "core/tAlgorithm.h"
#include "console/consoleTypes.h"
#include "console/telnetDebugger.h"
#include "console/compiler.h"
#include "console/stringStack.h"

#include "core/safeDelete.h"
#include <stdarg.h>
#include <unordered_map>


// NOTE: these globals are currently independent of codeblock world

//static Mutex* sLogMutex;

StmtNode *gStatementList;
ConsoleConstructor *ConsoleConstructor::first = NULL;


//--------------------------------------
void ConsoleConstructor::init(const char *cName, const char *fName, const char *usg, S32 minArgs, S32 maxArgs)
{
   mina = minArgs;
   maxa = maxArgs;
   funcName = fName;
   usage = usg;
   className = cName;
   sc = 0; fc = 0; vc = 0; bc = 0; ic = 0;
   group = false;
   next = first;
   ns = false;
   first = this;
}

void ConsoleConstructor::setup(CodeBlockWorld* con)
{
   for(ConsoleConstructor *walk = first; walk; walk = walk->next)
   {
      if(walk->sc)
         con->addCommand(walk->className, walk->funcName, walk->sc, walk->usage, walk->mina, walk->maxa);
      else if(walk->ic)
         con->addCommand(walk->className, walk->funcName, walk->ic, walk->usage, walk->mina, walk->maxa);
      else if(walk->fc)
         con->addCommand(walk->className, walk->funcName, walk->fc, walk->usage, walk->mina, walk->maxa);
      else if(walk->vc)
         con->addCommand(walk->className, walk->funcName, walk->vc, walk->usage, walk->mina, walk->maxa);
      else if(walk->bc)
         con->addCommand(walk->className, walk->funcName, walk->bc, walk->usage, walk->mina, walk->maxa);
      else if(walk->group)
         con->markCommandGroup(walk->className, walk->funcName, walk->usage);
      else if(walk->overload)
         con->addOverload(walk->className, walk->funcName, walk->usage);
      else if(walk->ns)
      {
         Namespace* ns = con->find(StringTable->insert(walk->className));
         if( ns )
            ns->mUsage = walk->usage;
      }
      else
         AssertFatal(false, "Found a ConsoleConstructor with an indeterminate type!");
   }
}

ConsoleConstructor::ConsoleConstructor(const char *className, const char *funcName, StringCallback sfunc, const char *usage, S32 minArgs, S32 maxArgs)
{
   init(className, funcName, usage, minArgs, maxArgs);
   sc = sfunc;
}

ConsoleConstructor::ConsoleConstructor(const char *className, const char *funcName, IntCallback ifunc, const char *usage, S32 minArgs, S32 maxArgs)
{
   init(className, funcName, usage, minArgs, maxArgs);
   ic = ifunc;
}

ConsoleConstructor::ConsoleConstructor(const char *className, const char *funcName, FloatCallback ffunc, const char *usage, S32 minArgs, S32 maxArgs)
{
   init(className, funcName, usage, minArgs, maxArgs);
   fc = ffunc;
}

ConsoleConstructor::ConsoleConstructor(const char *className, const char *funcName, VoidCallback vfunc, const char *usage, S32 minArgs, S32 maxArgs)
{
   init(className, funcName, usage, minArgs, maxArgs);
   vc = vfunc;
}

ConsoleConstructor::ConsoleConstructor(const char *className, const char *funcName, BoolCallback bfunc, const char *usage, S32 minArgs, S32 maxArgs)
{
   init(className, funcName, usage, minArgs, maxArgs);
   bc = bfunc;
}

ConsoleConstructor::ConsoleConstructor(const char* className, const char* groupName, const char* aUsage)
{
   init(className, groupName, usage, -1, -2);

   group = true;

   // Somewhere, the entry list is getting flipped, partially.
   // so we have to do tricks to deal with making sure usage
   // is properly populated.

   // This is probably redundant.
   static char * lastUsage = NULL;
   if(aUsage)
      lastUsage = (char *)aUsage;

   usage = lastUsage;
}

ConsoleConstructor::ConsoleConstructor(const char* className, const char* usage)
{
   init(className, NULL, usage, -1, -2);
   ns = true;
}



ConsoleFunctionGroupBegin( Clipboard, "Miscellaneous functions to control the clipboard and clear the console.");

/*! Use the cls function to clear the console output.
 @return No return value
 */
ConsoleFunction( cls, ConsoleVoid, 1, 1, "")
{
   con->cls();
};

ConsoleFunctionGroupEnd( Clipboard );

