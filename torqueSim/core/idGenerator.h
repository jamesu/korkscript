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

#ifndef _IDGENERATOR_H_
#define _IDGENERATOR_H_

#ifndef _PLATFORM_H_
#include "platform/platform.h"
#endif
#include <vector>

class IdGenerator
{
private:
   U32 mIdBlockBase;
   U32 mIdRangeSize;
   std::vector<U32> mPool;
   U32 mNextId;

   void reclaim();

public:
   IdGenerator(U32 base, U32 numIds)
   {
      mIdBlockBase = base;
      mIdRangeSize = numIds;
      mNextId = mIdBlockBase;
   }

   void reset()
   {
      mPool.clear();
      mNextId = mIdBlockBase;
   }

   U32 alloc()
   {
      // fist check the pool:
      if(!mPool.empty())
      {
         U32 id = mPool.back();
         mPool.pop_back();
         reclaim();
         return id;
      }
      if(mIdRangeSize && mNextId >= mIdBlockBase + mIdRangeSize)
         return 0;

      return mNextId++;
   }

   void free(U32 id)
   {
      AssertFatal(id >= mIdBlockBase, "IdGenerator::alloc: invalid id, id does not belong to this IdGenerator.")
      if(id == mNextId - 1)
      {
         mNextId--;
         reclaim();
      }
      else
         mPool.push_back(id);
   }

   U32 numIdsUsed()
   {
      return mNextId - mIdBlockBase - mPool.size();
   }
};

#endif
