#pragma once

namespace KorkApi
{

// Duck typed cvalue
#pragma pack(1)
struct ConsoleValue
{
   struct AllocBase
   {
      void** func;
      void* arg;
   };
   
   enum TypeEnum : U16
   {
      TypeInternalString = 0,   // const char*
      TypeInternalUnsigned    = 1,   // U64
      TypeInternalNumber  = 2,   // F64
      TypeBeginCustom    = 3    // void*
   };
   
   enum Zone : U16
   {
      ZoneExternal = 0, // externally managed pointer
      ZonePacked   = 1, // packed into CV
      ZoneVmHeap   = 2, // pointer managed by a ConsoleHeapAlloc
      ZoneReturn   = 3, // allocated inside thunk return buffer
      
      // Any zone beyond this is a repeat of func for each script fiber in the vm
      ZoneFunc     = 4,  // allocated inside main function buffer
      
      ZoneFiberStart = ZoneFunc
   };
   
   // ---- Storage ----
   U64 cvalue;
   U16 typeId;
   U16 zoneId;
   
   ConsoleValue() : cvalue(0), typeId(TypeInternalString), zoneId(0)
   {
   }
   
   inline Zone getZone() const
   {
      return (Zone)(zoneId);
   }
   
   inline void setZone(Zone z)
   {
      zoneId = static_cast<U16>(z);
   }
   
   static ConsoleValue makeUnsigned(U64 i)
   {
      ConsoleValue v; v.setUnsigned(i); return v;
   }
   static ConsoleValue makeNumber(F64 d)
   {
      ConsoleValue v; v.setNumber(d); return v;
   }
   static ConsoleValue makeString(const char* p, Zone zone=ZoneExternal)
   {
      ConsoleValue v; v.setString(p, zone); return v;
   }
   static ConsoleValue makeDynString(char* p, Zone zone=ZoneExternal)
   {
      ConsoleValue v; v.setDynString(p, zone); return v;
   }
   static ConsoleValue makeTyped(void* p, U16 typeId, Zone zone=ZoneExternal)
   {
      ConsoleValue v; v.setTyped((U64)p, typeId, zone); return v;
   }
   static ConsoleValue makeRaw(U64 p, U16 typeId, Zone zone=ZoneExternal)
   {
      ConsoleValue v; v.setTyped(p, typeId, zone); return v;
   }
   
   inline void setUnsigned(U64 i)
   {
      typeId = TypeInternalUnsigned;
      setZone(ZoneExternal); // zone irrelevant for immediates
      *((U64*)&cvalue) = i;
   }
   
   inline void setNumber(F64 d)
   {
      typeId = TypeInternalNumber;
      setZone(ZoneExternal); // zone irrelevant for immediates
      *((F64*)&cvalue) = d;
   }
   
   inline void setString(const char* p, Zone zone=ZoneExternal)
   {
      typeId = TypeInternalString;
      setZone(zone);
      *((const char**)&cvalue) = p;
   }
   
   inline void setDynString(char* p, Zone zone=ZoneExternal)
   {
      typeId = TypeInternalString;
      setZone(zone);
      *((char**)&cvalue) = p;
   }
   
   inline void setTyped(U64 p, U16 customTypeId, Zone zone=ZoneExternal)
   {
      typeId = customTypeId;
      setZone(zone);
      cvalue = (U64)p;
   }
   
   inline U64 getInt(U64 def = 0) const
   {
      if (typeId != TypeInternalUnsigned) return def;
      return *((U64*)&cvalue);
   }
   
   inline F64 getFloat(F64 def = 0.0) const
   {
      if (typeId != TypeInternalNumber) return def;
      return *((F64*)&cvalue);
   }

   void* ptr() const
   {
      return (void*)cvalue;
   }

   void* advancePtr(size_t bytes)
   {
      *((char**)&cvalue) += bytes;
      return *((char**)&cvalue);
   }
   
   void* evaluatePtr(AllocBase base = {}) const
   {
      if (!(typeId == TypeInternalString || typeId >= TypeBeginCustom))
      {
         return NULL;
      }
      
      switch (getZone())
      {
         case ZoneExternal:
         case ZoneVmHeap:
            return (void*)(cvalue);
         case ZonePacked:
            return (void*)&cvalue;
         case ZoneReturn:
            return addOffset(base.arg, cvalue);
         default:
            return addOffset(base.func[zoneId - ZoneFiberStart], cvalue);
      }
   }
   
   static inline void* addOffset(const void* base, U64 off)
   {
      return (!base) ? NULL : reinterpret_cast<void*>(reinterpret_cast<UINTPTR>(base) + static_cast<UINTPTR>(off));
   }
   
   inline bool isString() const
   {
      return typeId == TypeInternalString;
   }
   inline bool isInt()    const
   {
      return typeId == TypeInternalUnsigned;
   }
   inline bool isFloat()  const
   {
      return typeId == TypeInternalNumber;
   }
   inline bool isCustom() const
   {
      return typeId >= TypeBeginCustom;
   }
   inline bool isNull()
   {
      return typeId == TypeInternalString && cvalue == 0;
   }
};
#pragma pack()

}
