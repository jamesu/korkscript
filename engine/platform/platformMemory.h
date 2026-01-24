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

#ifndef _PLATFORM_MEMORY_H_
#define _PLATFORM_MEMORY_H_

#include <new>
#include <string.h>

//------------------------------------------------------------------------------

template <class T, class... Args>
inline T* constructInPlace(T* p, Args&&... args)
{
    return new (p) T(std::forward<Args>(args)...);
}

//------------------------------------------------------------------------------

template <class T> inline T* constructInPlace(T* p, const T* copy)
{
   return new(p) T(*copy);
}


template< class T >
inline T* constructArrayInPlace( T* p, U32 num )
{
    return new ( p ) T[ num ];
}

//------------------------------------------------------------------------------

template <class T> inline void destructInPlace(T* p)
{
   p->~T();
}

#endif // _PLATFORM_MEMORY_H_
