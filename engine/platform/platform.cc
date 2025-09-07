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
#include "platform/threads/mutex.h"

//Added for the cprintf below
#include <stdarg.h>
#include <stdio.h>

S32 sgBackgroundProcessSleepTime = 200;
S32 sgTimeManagerProcessInterval = 0;


void Platform::initConsole()
{
#if TOFIX
   Con::addVariable("Pref::backgroundSleepTime", TypeS32, &sgBackgroundProcessSleepTime);
   Con::addVariable("Pref::timeManagerProcessInterval", TypeS32, &sgTimeManagerProcessInterval);
#endif
}

S32 Platform::getBackgroundSleepTime()
{
   return sgBackgroundProcessSleepTime;
}

void Platform::cprintf( const char* str )
{
    printf( "%s \n", str );
}

bool Platform::hasExtension(const char* pFilename, const char* pExtension)
{
    // Sanity!
    AssertFatal( pFilename != NULL, "Filename cannot be NULL." );
    AssertFatal( pExtension != NULL, "Extension cannot be NULL." );

    // Find filename length.
    const U32 filenameLength = dStrlen( pFilename );

    // Find extension length.
    const U32 extensionLength = dStrlen( pExtension );

    // Skip if extension is longer than filename.
    if ( extensionLength >= filenameLength )
        return false;

    // Check if extension exists.
    return dStricmp( pFilename + filenameLength - extensionLength, pExtension ) == 0;
}

/*! @defgroup PlatformFunctions Platform
 @ingroup TorqueScriptFunctions
 @{
 */

#if TOFIX
//-----------------------------------------------------------------------------

/*! Get the local time
 @return the local time formatted as: monthday/month/year hour/minute/second
 */
ConsoleFunction( getLocalTime, const char*, 1, 1, "")
{
   char* buf = Con::getReturnBuffer(128);
   
   Platform::LocalTime lt;
   Platform::getLocalTime(lt);
   
   dSprintf(buf, 128, "%d/%d/%d %02d:%02d:%02d",
            lt.monthday,
            lt.month + 1,
            lt.year + 1900,
            lt.hour,
            lt.min,
            lt.sec);
   
   return buf;
}
#endif

/*! @} */ // group PlatformFunctions




