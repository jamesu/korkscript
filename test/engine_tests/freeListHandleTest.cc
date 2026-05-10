//-----------------------------------------------------------------------------
// Copyright (c) 2023 tgemit contributors.
// Copyright (c) 2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "core/freeListHandleHelpers.h"
#include <vector>
#include <catch2/catch.hpp>

struct FLTestItem
{
   U32 mAllocNumber;
   U32 mIndex;
   U8 mGeneration : 7;

   FLTestItem() : mAllocNumber(0), mIndex(0), mGeneration(0) { ; }

   void reset()
   {
      mIndex = 0;
   }
};

typedef FreeListStruct<FLTestItem, FreeListHandle::Basic32, std::vector> TestStructPool;
typedef FreeListPtr<FLTestItem, FreeListHandle::Basic32, std::vector> TestPtrPool;

TEST_CASE("FreeListPtr should function correctly", "[FreeListPtr]") {
   TestPtrPool pool;
   pool.mChunkReserveSize = 50;

   FLTestItem* item1 = new FLTestItem();
   FLTestItem* item2 = new FLTestItem();
   FLTestItem* item3 = new FLTestItem();
   FLTestItem* item4 = new FLTestItem();

   TestStructPool::HandleType h1 = pool.allocListHandle(item1);
   TestStructPool::HandleType h2 = pool.allocListHandle(item2);
   TestStructPool::HandleType h3 = pool.allocListHandle(item3);
   TestStructPool::HandleType h4 = pool.allocListHandle(item4);

   REQUIRE(pool.mItems.size() == 4);
   REQUIRE(pool.mFreeItems.size() == 0);

   REQUIRE(item1 == pool.mItems[0]);
   REQUIRE(item2 == pool.mItems[1]);
   REQUIRE(item3 == pool.mItems[2]);
   REQUIRE(item4 == pool.mItems[3]);

   REQUIRE(item1->mAllocNumber == 1);
   REQUIRE(item2->mAllocNumber == 2);
   REQUIRE(item3->mAllocNumber == 3);
   REQUIRE(item4->mAllocNumber == 4);
   REQUIRE(item1->mGeneration == 1);
   REQUIRE(item2->mGeneration == 1);
   REQUIRE(item3->mGeneration == 1);
   REQUIRE(item4->mGeneration == 1);
   
   REQUIRE(h1.parts.index == 1);
   REQUIRE(h2.parts.index == 2);
   REQUIRE(h3.parts.index == 3);
   REQUIRE(h4.parts.index == 4);
   REQUIRE(h1.parts.generation == 1);
   REQUIRE(h2.parts.generation == 1);
   REQUIRE(h3.parts.generation == 1);
   REQUIRE(h4.parts.generation == 1);

   pool.freeListPtr(item2);

   REQUIRE(pool.mItems.size() == 4);
   REQUIRE(pool.mFreeItems.size() == 1);
   REQUIRE(pool.mItems[1] == NULL);

   pool.freeListPtr(item2);

   REQUIRE(pool.mItems.size() == 4);
   REQUIRE(pool.mFreeItems.size() == 1);
   REQUIRE(pool.mItems[1] == NULL);

   REQUIRE(item1->mAllocNumber == 1);
   REQUIRE(item2->mAllocNumber == 0);
   REQUIRE(item3->mAllocNumber == 3);
   REQUIRE(item4->mAllocNumber == 4);

   h2 = pool.allocListHandle(item2);

   REQUIRE(item2->mAllocNumber == 2);
   REQUIRE(item2->mGeneration == 2);
   REQUIRE(h2.parts.index == 2);
   REQUIRE(h2.parts.generation == 2);
   
   REQUIRE(pool.mItems.size() == 4);
   REQUIRE(pool.mFreeItems.size() == 0);
   REQUIRE(pool.mItems[1] == item2);

   // Alloc'ing same pointer again should do nothing
   h2 = pool.allocListHandle(item2);
   REQUIRE(item2->mAllocNumber == 2);

   FLTestItem* item5 = new FLTestItem();
   pool.allocListHandle(item5);

   REQUIRE(item5->mAllocNumber == 5);
   REQUIRE(pool.mItems.size() == 5);
   REQUIRE(pool.mFreeItems.size() == 0);
   REQUIRE(pool.mItems[4] == item5);

   pool.clear();

   REQUIRE(pool.mItems.size() == 0);
   REQUIRE(pool.mFreeItems.size() == 0);

   // Should still be in memory
   REQUIRE(item1->mAllocNumber == 0);
   REQUIRE(item2->mAllocNumber == 0);
   REQUIRE(item3->mAllocNumber == 0);
   REQUIRE(item4->mAllocNumber == 0);
   REQUIRE(item5->mAllocNumber == 0);
   REQUIRE(item1->mIndex == 0);
   REQUIRE(item2->mIndex == 0);
   REQUIRE(item3->mIndex == 0);
   REQUIRE(item4->mIndex == 0);
   REQUIRE(item5->mIndex == 0);
   REQUIRE(item1->mGeneration == 1);
   REQUIRE(item2->mGeneration == 2);
   REQUIRE(item3->mGeneration == 1);
   REQUIRE(item4->mGeneration == 1);
   REQUIRE(item5->mGeneration == 1);

   delete item1;
   delete item2;
   delete item3;
   delete item4;
   delete item5;
}


TEST_CASE("FreeListStruct should function correctly", "[StaticPoolIDResourceList]") {
   TestStructPool pool;
   pool.mChunkReserveSize = 50;

   FLTestItem* ptr1 = NULL;
   FLTestItem* ptr2 = NULL;
   FLTestItem* ptr3 = NULL;
   FLTestItem* ptr4 = NULL;
   
   TestStructPool::HandleType h1 = pool.allocItem(&ptr1);
   TestStructPool::HandleType h2 = pool.allocItem(&ptr2);
   TestStructPool::HandleType h3 = pool.allocItem(&ptr3);
   TestStructPool::HandleType h4 = pool.allocItem(&ptr4);
   
   ptr2->mIndex = 0x1010;
   pool.getItem(h3.value)->mIndex = 0x3030;
   
   REQUIRE(ptr1->mAllocNumber == 1);
   REQUIRE(ptr2->mAllocNumber == 2);
   REQUIRE(ptr3->mAllocNumber == 3);
   REQUIRE(ptr4->mAllocNumber == 4);

   REQUIRE(pool.mItems.size() == 4);
   REQUIRE(pool.mFreeItems.size() == 0);

   REQUIRE(h1.parts.index == pool.mItems[0].mAllocNumber);
   REQUIRE(h2.parts.index == pool.mItems[1].mAllocNumber);
   REQUIRE(h3.parts.index == pool.mItems[2].mAllocNumber);
   REQUIRE(h4.parts.index == pool.mItems[3].mAllocNumber);
   REQUIRE(h1.parts.generation == 1);
   REQUIRE(h2.parts.generation == 1);
   REQUIRE(h3.parts.generation == 1);
   REQUIRE(h4.parts.generation == 1);
   REQUIRE(pool.mItems[0].mIndex == 0);
   REQUIRE(pool.mItems[1].mIndex == 0x1010);
   REQUIRE(pool.mItems[2].mIndex == 0x3030);
   REQUIRE(pool.mItems[3].mIndex == 0);

   pool.freeItemPtr(ptr2);

   REQUIRE(pool.mItems.size() == 4);
   REQUIRE(pool.mFreeItems.size() == 1);
   REQUIRE(pool.mItems[1].mAllocNumber == 0);
   REQUIRE(pool.mItems[0].mIndex == 0);
   REQUIRE(pool.mItems[1].mIndex == 0);
   REQUIRE(pool.mItems[2].mIndex == 0x3030);
   REQUIRE(pool.mItems[3].mIndex == 0);

   pool.freeItemPtr(ptr2);

   REQUIRE(pool.mItems.size() == 4);
   REQUIRE(pool.mFreeItems.size() == 1);
   REQUIRE(pool.mItems[1].mAllocNumber == 0);
   REQUIRE(pool.mItems[0].mIndex == 0);
   REQUIRE(pool.mItems[1].mIndex == 0);
   REQUIRE(pool.mItems[2].mIndex == 0x3030);
   REQUIRE(pool.mItems[3].mIndex == 0);

   // Alloc again should restore old alloc number
   // but increase generation
   h2 = pool.allocItem(&ptr2);

   REQUIRE(h2.parts.index == 2);
   REQUIRE(h2.parts.generation == 2);
   REQUIRE(ptr2->mAllocNumber == 2);
   REQUIRE(ptr2->mGeneration == 2);
   
   REQUIRE(pool.mItems.size() == 4);
   REQUIRE(pool.mFreeItems.size() == 0);
   REQUIRE(pool.mItems[0].mAllocNumber == 1);
   REQUIRE(pool.mItems[1].mAllocNumber == 2);
   REQUIRE(pool.mItems[2].mAllocNumber == 3);
   REQUIRE(pool.mItems[3].mAllocNumber == 4);
   //
   REQUIRE(pool.mItems[0].mGeneration == 1);
   REQUIRE(pool.mItems[1].mGeneration == 2);
   REQUIRE(pool.mItems[2].mGeneration == 1);
   REQUIRE(pool.mItems[3].mGeneration == 1);

   // Alloc'ing again of course gives new alloc number
   h2 = pool.allocItem(&ptr2);
   REQUIRE(ptr2->mAllocNumber == 5);

   FLTestItem* ptr5;
   TestStructPool::HandleType h5 = pool.allocItem(&ptr5);

   REQUIRE(h5.parts.index == 6);
   REQUIRE(pool.mItems.size() == 6);
   REQUIRE(pool.mFreeItems.size() == 0);
   REQUIRE(pool.mItems[0].mAllocNumber == 1);
   REQUIRE(pool.mItems[1].mAllocNumber == 2);
   REQUIRE(pool.mItems[2].mAllocNumber == 3);
   REQUIRE(pool.mItems[3].mAllocNumber == 4);
   REQUIRE(pool.mItems[4].mAllocNumber == 5);
   REQUIRE(pool.mItems[5].mAllocNumber == 6);

   pool.clear();

   REQUIRE(pool.mItems.size() == 0);
   REQUIRE(pool.mFreeItems.size() == 0);
}
