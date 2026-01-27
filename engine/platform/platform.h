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

#ifndef _PLATFORM_MEMORY_H_
#include "platform/platformMemory.h"
#endif

#include <stdio.h>
#include <stdarg.h>

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

inline char dToupper(const char c) { if (c >= char('a') && c <= char('z')) return char(c + 'A' - 'a'); else return c; }
inline char dTolower(const char c) { if (c >= char('A') && c <= char('Z')) return char(c - 'A' + 'a'); else return c; }
inline bool dAtob(const char *str){ return !strcasecmp(str, "true") || atof(str); } 

#endif // _PLATFORM_H_
