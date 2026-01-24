#pragma once

template<typename T> class AlignedBufferAllocator
{
protected:
   T* mBuffer;
   U32 mHighWaterMark;
   U32 mWaterMark;
   
public:
   
   typedef T ValueType;
   
   AlignedBufferAllocator() : mBuffer(NULL), mHighWaterMark(0), mWaterMark(0)
   {
   }
   
   inline void initWithElements(T* ptr, U32 numElements)
   {
      mBuffer = ptr;
      mHighWaterMark = numElements;
      mWaterMark = 0;
   }
   
   inline void initWithBytes(T* ptr, dsize_t bytes)
   {
      mBuffer = ptr;
      mHighWaterMark = (U32)(calcMaxElementSize(bytes));
      mWaterMark = 0;
   }
   
   inline T* allocBytes(const size_t numBytes)
   {
      T* ptr = &mBuffer[mWaterMark];
      size_t numElements = calcRequiredElementSize(numBytes);
      if (((size_t)mWaterMark + (size_t)numElements) > (size_t)mHighWaterMark) // safety check
      {
#ifdef TORQUE_MEM_DEBUG
         AssertFatal(false, "Overflow");
#endif
         return NULL;
      }
      mWaterMark += (U32)numElements;
      return ptr;
   }
   
   inline T* allocElements(const U32 numElements)
   {
      T* ptr = &mBuffer[mWaterMark];
      if (((size_t)mWaterMark + (size_t)numElements) > (size_t)mHighWaterMark) // safety check
      {
#ifdef TORQUE_MEM_DEBUG
         AssertFatal(false, "Overflow");
#endif
         return NULL;
      }
      mWaterMark += numElements;
      return ptr;
   }
   
   inline void setPosition(const U32 waterMark)
   {
      AssertFatal(waterMark <= mHighWaterMark, "Error, invalid waterMark");
      mWaterMark = waterMark;
   }
   
   /// Calculates maximum elements required to store numBytes bytes (may overshoot)
   static inline U32 calcRequiredElementSize(const dsize_t numBytes)
   {
      return (U32)((numBytes + (sizeof(T)-1)) / sizeof(T));
   }
   
   /// Calculates maximum elements required to store numBytes bytes
   static inline U32 calcMaxElementSize(const dsize_t numBytes)
   {
      return (U32)(numBytes / sizeof(T));
   }
   
   inline T* getAlignedBuffer() const
   {
      return mBuffer;
   }
   
   inline U32 getPosition() const
   {
      return mWaterMark;
   }

   inline U32 getSize() const
   {
      return mHighWaterMark;
   }
   
   inline U32 getElementsLeft() const
   {
      return mHighWaterMark - mWaterMark;
   }
   
   inline dsize_t getPositionBytes() const
   {
      return mWaterMark * sizeof(T);
   }
   
   inline dsize_t getSizeBytes() const
   {
      return mHighWaterMark * sizeof(T);
   }
};
