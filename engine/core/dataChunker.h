//-----------------------------------------------------------------------------
// Copyright (c) 2023-2026 tgemit contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

#pragma once
#define _DATACHUNKER_H_

#ifndef _PLATFORM_H_
#  include "platform/platform.h"
#endif
#ifndef _PLATFORMASSERT_H_
#  include "platform/platformAssert.h"
#endif

#include <algorithm>
#include <cstddef>
#include <memory>
#include <stdint.h>
#include "core/alignedBufferAllocator.h"

// NOTE: this variant of the code uses std::allocator to manage memory.

/// Implements a chunked data allocator.
///
/// This memory allocator allocates data in chunks of bytes, 
/// the default size being ChunkSize.
/// Bytes are sourced from the current head chunk until expended, 
/// in which case a new chunk of bytes will be allocated from 
/// the system memory allocator.
///
template<class T, class Alloc = std::allocator<std::byte>> class BaseDataChunker
{
public:
   enum
   {
      ChunkSize = 16384
   };

   struct alignas(uintptr_t) DataBlock : public AlignedBufferAllocator<T>
   {
      DataBlock* mNext;
      size_t mCapacityBytes;
      
      inline DataBlock* getEnd()
      {
         return this+1;
      }

      inline size_t getCapacityBytes()
      {
         return mCapacityBytes;
      }
   };

protected:
   using ByteAlloc   = typename std::allocator_traits<Alloc>::template rebind_alloc<std::byte>;
   using ByteTraits  = std::allocator_traits<ByteAlloc>;

   size_t mChunkSize;
   DataBlock* mChunkHead;
   ByteAlloc mAlloc;

public:
   
   BaseDataChunker(U32 chunkSize=BaseDataChunker<T>::ChunkSize, 
      const Alloc& alloc = Alloc{}) : mChunkSize(chunkSize), mChunkHead(nullptr)
   {
   }
   
   virtual ~BaseDataChunker()
   {
      freeBlocks(false);
   }

   const ByteAlloc& get_allocator() const noexcept { return mAlloc; }
   
   DataBlock* allocChunk(size_t chunkSize)
   {
      const size_t headerBytes = sizeof(DataBlock);
      const size_t totalBytes  = headerBytes + chunkSize;

      std::byte* raw = ByteTraits::allocate(mAlloc, totalBytes);

      DataBlock* newChunk = reinterpret_cast<DataBlock*>(raw);
      constructInPlace(newChunk);

      newChunk->initWithBytes(
         reinterpret_cast<T*>(reinterpret_cast<std::byte*>(newChunk->getEnd())),
         chunkSize
      );

      // push front
      newChunk->mNext = mChunkHead;
      mChunkHead = newChunk;
      return newChunk;
   }

   void* alloc(size_t numBytes)
   {
      void* theAlloc = mChunkHead ? mChunkHead->allocBytes(numBytes) : nullptr;
      if (theAlloc == nullptr)
      {
         size_t actualSize = std::max<size_t>(mChunkSize, numBytes);
         allocChunk(actualSize);
         theAlloc = mChunkHead->allocBytes(numBytes);
         AssertFatal(theAlloc != nullptr, "Something really odd going on here");
      }
      return theAlloc;
   }

   void freeBlocks(bool keepOne = false)
   {
      DataBlock* itr = mChunkHead;
      while (itr)
      {
         DataBlock* nextItr = itr->mNext;
         if (nextItr == nullptr && keepOne)
         {
            itr->setPosition(0);
            break;
         }

         const size_t totalBytes = sizeof(DataBlock) + itr->getCapacityBytes();

         destructInPlace(itr);
         ByteTraits::deallocate(mAlloc, reinterpret_cast<std::byte*>(itr), totalBytes);


         itr = nextItr;
      }
      mChunkHead = itr;
   }

   U32 countUsedBlocks()
   {
      U32 count = 0;
      for (DataBlock* itr = mChunkHead; itr; itr = itr->mNext)
      {
         count++;
      }
      return count;
   }
   
   size_t countUsedBytes()
   {
      size_t count = 0;
      for (DataBlock* itr = mChunkHead; itr; itr = itr->mNext)
      {
         count += itr->getPositionBytes();
      }
      return count;
   }

   void setChunkSize(size_t size)
   {
      AssertFatal(mChunkHead == nullptr, "Tried setting AFTER init");
      mChunkSize = size;
   }
};

template<class Alloc = std::allocator<std::byte>>
class DataChunker : public BaseDataChunker<uintptr_t, Alloc>
{
   using Base = BaseDataChunker<uintptr_t, Alloc>;
public:
   DataChunker(const Alloc& a = Alloc{}) : Base(BaseDataChunker<uintptr_t>::ChunkSize, a) {;}
   explicit DataChunker(size_t size, const Alloc& a = Alloc{})
      : Base((U32)size, a) {}
};

/// Implements a derivative of BaseDataChunker designed for 
/// allocating structs of type T without initialization.
template<class T, class Alloc = std::allocator<std::byte>>
class Chunker : private BaseDataChunker<T, Alloc>
{
   using Base = BaseDataChunker<T, Alloc>;
public:
   Chunker(size_t size = Base::ChunkSize, const Alloc& a = Alloc{})
      : Base((U32)std::max<size_t>(sizeof(T), size), a) 
   {
   }

   T* alloc()
   {
      return (T*)BaseDataChunker<T>::alloc(sizeof(T));
   }

   void clear()
   {
      BaseDataChunker<T>::freeBlocks();
   }
};

/// Implements a simple linked list for ClassChunker and FreeListChunker.
template<class T> struct ChunkerFreeClassList
{
   ChunkerFreeClassList<T>* mNextList;

   ChunkerFreeClassList() : mNextList(nullptr)
   {
   }

   void reset()
   {
      mNextList = nullptr;
   }

   bool isEmpty()
   {
      return mNextList == nullptr;
   }

   T* pop()
   {
      ChunkerFreeClassList<T>* oldNext = mNextList;
      mNextList = mNextList ? mNextList->mNextList : nullptr;
      return (T*)oldNext;
   }

   void push(ChunkerFreeClassList<T>* other)
   {
      other->mNextList = mNextList;
      mNextList = other;
   }
};

/// Implements a derivative of BaseDataChunker designed for 
/// allocating structs or classes of type T with initialization.
template<class T, class Alloc = std::allocator<std::byte>> class ClassChunker : private BaseDataChunker<T, Alloc>
{
   using Base = BaseDataChunker<T, Alloc>;

protected:
   ChunkerFreeClassList<T> mFreeListHead;

public:
   ClassChunker(size_t size = BaseDataChunker<T>::ChunkSize, const Alloc& alloc = Alloc{}) : Base(size, alloc)
   {
      
   }
   
   T* alloc()
   {
      if (mFreeListHead.isEmpty())
      {
         return constructInPlace((T*)BaseDataChunker<T>::alloc(sizeof(T)));
      }
      else
      {
         return constructInPlace(mFreeListHead.pop());
      }
   }
   
   void free(T* item)
   {
      destructInPlace(item);
      mFreeListHead.push(reinterpret_cast< ChunkerFreeClassList<T>* >(item));
   }
   
   void freeBlocks(bool keepOne=false)
   {
      BaseDataChunker<T>::freeBlocks(keepOne);
   }
};

/// Implements a chunker which uses the data of another BaseDataChunker 
/// as underlying storage.
template<class T, class Alloc = std::allocator<std::byte>>
class FreeListChunker
{
protected:
   BaseDataChunker<T, Alloc>* mChunker;
   bool mOwnsChunker;
   ChunkerFreeClassList<T> mFreeListHead;

public:
   FreeListChunker(BaseDataChunker<T>* otherChunker) :
   mChunker(otherChunker),
   mOwnsChunker(false)
   {
   }

   FreeListChunker(size_t size = BaseDataChunker<T>::ChunkSize)
   {
      mChunker = new BaseDataChunker<T, Alloc>(size);
      mOwnsChunker = true;
   }
   
   BaseDataChunker<T>* getChunker()
   {
      return mChunker;
   }

   T* alloc()
   {
      if (mFreeListHead.isEmpty())
      {
         return constructInPlace((T*)mChunker->alloc(sizeof(T)));
      }
      else
      {
         return constructInPlace(mFreeListHead.pop());
      }
   }

   void free(T* item)
   {
      destructInPlace(item);
      mFreeListHead.push(reinterpret_cast< ChunkerFreeClassList<T>* >(item));
   }

   void freeBlocks(bool keepOne)
   {
      BaseDataChunker<T>::freeBlocks(keepOne);
   }
};
