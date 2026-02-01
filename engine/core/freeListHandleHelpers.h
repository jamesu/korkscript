#pragma once
//-----------------------------------------------------------------------------
// Copyright (c) 2025-2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

namespace FreeListHandle
{
   struct Basic32
   {
      typedef U32 ValueType;
      
      struct Parts
      {
         U32 index : 24;
         U32 generation : 7;
         U32 heavyRef : 1;
      };

      union
      {
         Parts parts;
         U32 value;
      };

      Basic32() : value(0) {;}
      explicit Basic32(U32 fullNum, bool isHeavy=false) { value = fullNum; parts.heavyRef = (U32)isHeavy; }
      explicit Basic32(U32 num, U8 gen, bool isHeavy=false) { parts.index = num; parts.generation = gen; parts.heavyRef = (U32)isHeavy; }
      Basic32(const Basic32& other)
      {
         value = other.value;
      }
      static Basic32 fromValue(U32 other)
      {
         Basic32 outv;
         outv.value = other;
         return outv;
      }
      
      inline Basic32& operator=(const Basic32& other) { value = other.value; return *this; }
      inline U32 getNum() const { return parts.index; }
      inline U8 getGen() const { return (U8)parts.generation; }
      inline U32 getIndex() const { return parts.index-1; }
      inline U32 getValue() const { return value; }
      inline bool setHeavyRef(bool value) { return parts.heavyRef = (U32)value; }
      inline bool isHeavyRef() const { return parts.heavyRef != 0; }
      
      inline static U32 makeValue(U32 num, U8 gen, bool isHeavy)
      {
         union
         {
            Parts fparts;
            U32 fvalue;
         };
         
         fparts.index = num; fparts.generation = gen; fparts.heavyRef = (U32)isHeavy;
         return fvalue;
      }
   };

   struct Basic64
   {
      typedef U64 ValueType;
      
      struct Parts
      {
         U64 index : 56;
         U64 generation : 7;
         U32 heavyRef : 1;
      };
      
      union
      {
         Parts parts;
         U64 value;
      };

      Basic64() : value(0) {;}
      explicit Basic64(U64 num, U8 gen, bool isHeavy=false) { parts.index = num; parts.generation = gen; parts.heavyRef = (U32)isHeavy; }
      explicit Basic64(U64 fullNum, bool isHeavy=false) { value = fullNum; parts.heavyRef = (U32)isHeavy; }
      Basic64(const Basic64& other) { value = other.value; }
      static Basic64 fromValue(U64 other) { Basic64 outv; outv.value = other; return outv; }
      
      inline Basic64& operator=(const Basic64& other) { value = other.value; return *this; }
      
      inline U64 getNum() const { return parts.index; }
      inline U8 getGen() const { return (U8)parts.generation; }
      inline U64 getIndex() const { return parts.index-1; }
      inline U64 getValue() const { return value; }
      inline bool setHeavyRef(bool value) { return parts.heavyRef = (U32)value; }
      inline bool isHeavyRef() const { return parts.heavyRef != 0; }
      
      inline static U64 makeValue(U64 num, U8 gen, bool isHeavy)
      {
         union
         {
            Parts fparts;
            U32 fvalue;
         };
         
         fparts.index = num; fparts.generation = gen; fparts.heavyRef = (U32)isHeavy;
         return fvalue;
      }
   };

   template<class T, class H, class Y> inline Y* resolveHandle(const T& container, const H& handle)
   {
      U64 realIndex = handle.getIndex();
      if (realIndex < container.mItems.size())
      {
         Y* item = &container.mItems[(S32)realIndex];
         return item->mGeneration == handle.parts.generation ? item : nullptr;
      }
      return nullptr;
   }

   template<class T, class H, class Y> inline Y* resolveHandlePtr(const T& container, const H& handle)
   {
      U64 realIndex = handle.getIndex();
      if (realIndex < container.mItems.size())
      {
         Y* item = container.mItems[(S32)realIndex];
         return item != nullptr && item->mGeneration == handle.parts.generation ? item : nullptr;
      }
      return nullptr;
   }

   template<class T> inline U32 getReserveSize(T& container)
   {
      U64 curBlock = container.mItems.size() / container.mChunkReserveSize;
      U64 nextBlock = curBlock + 1;
      return nextBlock * (U64)container.mChunkReserveSize;
   }
}

/// Free structure list. T must have "mAllocNumber" to designate it is allocated and 
/// what index it is. Also "mGeneration" should be present.
/// T should also implement initFromHandle, makeHandle, isValidHandle and extractHandle.
template<class T, class B,  template<class...> class VEC> struct FreeListStruct
{
   typedef T ValueType;
   typedef B HandleType;
   
   VEC<T> mItems;
   VEC<typename B::ValueType> mFreeItems;
   U32 mChunkReserveSize;
   
   FreeListStruct() : mChunkReserveSize(4096)
   {
      
   }

   T* getItem(typename B::ValueType handleNum)
   {
      B handle = B::fromValue(handleNum);
      T* itemPtr = FreeListHandle::resolveHandle<decltype(*this), decltype(handle), T>(*this, handle);
      return itemPtr;
   }

   B allocItem(T** itemPtr)
   {
      typename B::ValueType index = 0;
      T* item;
      if (mFreeItems.size() > 0)
      {
         index = mFreeItems.last();
         mFreeItems.pop_back();
         item = &mItems[index];
         item->mAllocNumber = index+1;
      }
      else
      {
         mItems.push_back(T());
         mItems.reserve(FreeListHandle::getReserveSize(*this));
         item = &mItems.last();
         item->mAllocNumber = mItems.size();
      }
      item->mGeneration++;
      if (itemPtr) *itemPtr = item;
      return B(item->mAllocNumber, item->mGeneration);
   }

   void freeItemPtr(T* itemPtr)
   {
      if (itemPtr == nullptr || itemPtr->mAllocNumber == 0)
         return;

      mFreeItems.push_back(itemPtr->mAllocNumber-1);
      itemPtr->reset();
      itemPtr->mAllocNumber = 0;
   }

   void freeItem(typename B::ValueType handleNum)
   {
      B handle(handleNum);
      T* itemPtr = FreeListHandle::resolveHandle<decltype(*this), decltype(handle), T>(*this, handle);
      freeItemPtr(itemPtr);
   }

   void clear()
   {
      mFreeItems.clear();

      for (T& item : mItems)
      {
         item.reset();
         item.mAllocNumber = 0;
      }
      mItems.clear();
   }
   
   inline typename B::ValueType getHandleValue(T* itemPtr, bool isHeavy=false) const
   {
      return B::makeValue(itemPtr->mAllocNumber, itemPtr->mGeneration, isHeavy);
   }

   template<class F> void forEach(F&& func)
   {
      for (auto itr = mItems.begin(); itr != mItems.end(); itr++)
      {
         if (itr->mAllocNumber == 0)
            continue;
         func(*itr);
      }
   }
   
   template<class F, class L> void mapToIndexIf(F&& func, L& outList)
   {
      for (auto itr = mItems.begin(); itr != mItems.end(); itr++)
      {
         if (itr->mAllocNumber == 0)
            continue;
         if (func(*itr))
         {
            outList.push_back(itr - mItems.begin());
         }
      }
   }
};

/// Free pointer list. T must have "mAllocNumber" to designate it is allocated and
/// what index it is. Also "mGeneration" should be present.
/// T should also implement initFromHandle, makeHandle, isValidHandle and extractHandle.
/// Underlying memory management of item pointers should be
/// handled by another class.
/// Reference counting should be handled by the handle class.
template<class T, class B, template<class...> class VEC> struct FreeListPtr
{
   VEC<T*> mItems;
   VEC<typename B::ValueType> mFreeItems;
   U32 mChunkReserveSize;
   
   typedef B HandleType;
   typedef T ValueType;
   
   FreeListPtr() : mChunkReserveSize(4096)
   {
      
   }

   T* getItem(typename B::ValueType handleNum)
   {
      B handle(handleNum, false);
      T* itemPtr = FreeListHandle::resolveHandlePtr<decltype(*this), decltype(handle), T>(*this, handle);
      return itemPtr;
   }

   B allocListHandle(T* itemPtr, bool isStrong=true)
   {
      typename B::ValueType index = 0;
      
      if (itemPtr->mAllocNumber != 0)
      {
         return B(itemPtr->mAllocNumber, itemPtr->mGeneration, isStrong);
      }
      else if (mFreeItems.size() > 0)
      {
         index = mFreeItems.back();
         mFreeItems.pop_back();
         mItems[index] = itemPtr;
         itemPtr->mAllocNumber = index+1;
         itemPtr->mGeneration++;
         return B(itemPtr->mAllocNumber, itemPtr->mGeneration, isStrong);
      }
      else
      {
         mItems.push_back(itemPtr);
         mItems.reserve(FreeListHandle::getReserveSize(*this));
         itemPtr->mAllocNumber = mItems.size();
         itemPtr->mGeneration++;
         return B(itemPtr->mAllocNumber, itemPtr->mGeneration, isStrong);
      }
   }

   void freeListPtr(T* itemPtr)
   {
      if (itemPtr == nullptr || itemPtr->mAllocNumber == 0)
         return;

      mItems[itemPtr->mAllocNumber-1] = nullptr;
      mFreeItems.push_back(itemPtr->mAllocNumber-1);
      itemPtr->reset();
      itemPtr->mAllocNumber = 0;
   }

   void freeListHandle(typename B::ValueType handleNum)
   {
      B handle(handleNum, false);
      T* itemPtr = FreeListHandle::resolveHandlePtr<T*>(*this, handle);
      freeListPtr(itemPtr);
   }

   void clear()
   {
      mFreeItems.clear();

      for (T* item : mItems)
      {
         if (item && item->mAllocNumber > 0)
         {
            item->reset();
            item->mAllocNumber = 0;
         }
      }
      
      mItems.clear();
   }
   
   inline typename B::ValueType getHandleValue(T* itemPtr, bool isHeavy=false) const
   {
      return B::makeValue(itemPtr->mAllocNumber, itemPtr->mGeneration, isHeavy);
   }

   template<class F> void forEach(F&& func)
   {
      for (auto itr = mItems.begin(); itr != mItems.end(); itr++)
      {
         T* item = *itr;
         if (item == nullptr || (item && item->mAllocNumber == 0))
            continue;
         func(item);
      }
   }
   
   template<typename F, class L> void mapToIndexIf(F&& func, L& outList) const
   {
      for (auto itr = mItems.begin(); itr != mItems.end(); itr++)
      {
         T* item = *itr;
         if (item == nullptr || (item && item->mAllocNumber == 0))
            continue;
         if (func(item))
         {
            outList.push_back(itr - mItems.begin());
         }
      }
   }
};
