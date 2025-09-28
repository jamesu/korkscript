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

#ifndef INCLUDED_TYPES_VISUALC_H
#define INCLUDED_TYPES_VISUALC_H

#include <stdarg.h> // for va_list

// For more information on VisualC++ predefined macros
// http://support.microsoft.com/default.aspx?scid=kb;EN-US;q65472

//--------------------------------------
// Types
typedef signed _int64   S64;
typedef unsigned _int64 U64;


//--------------------------------------
// Compiler Version
#define TORQUE_COMPILER_VISUALC _MSC_VER

//--------------------------------------
// Identify the compiler string
#if _MSC_VER < 1600
   // No support for old compilers
#  error "VC: Minimum Visual Studio 2010 or newer required"
#endif

//--------------------------------------
// Identify the CPU
#if defined(_M_IX86)
#  define TORQUE_CPU_STRING "x86"
#  define TORQUE_CPU_X86
#  define TORQUE_LITTLE_ENDIAN
#elif defined(_M_AMD64)
#  define TORQUE_CPU_STRING "x86_64"
#  define TORQUE_64
#  define TORQUE_LITTLE_ENDIAN
#elif defined(_M_ARM64)
#  define TORQUE_CPU_STRING "arm64"
#  define TORQUE_64
#  define TORQUE_LITTLE_ENDIAN
#  else
#  error "VC: Unsupported Target CPU"
#endif

#endif


#define FN_CDECL __cdecl            ///< Calling convention

// disable warning caused by memory layer
// see msdn.microsoft.com "Compiler Warning (level 1) C4291" for more details
#pragma warning(disable: 4291) 


#endif // INCLUDED_TYPES_VISUALC_H

