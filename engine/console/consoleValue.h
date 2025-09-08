#pragma once

namespace KorkApi
{

// Duck typed cvalue
#pragma pack(1)
struct ConsoleValue
{
   struct AllocBase
   {
      void* arg;
      void* ret;
   };
   
   enum TypeEnum : U16
   {
      TypeInternalString = 0,   // const char*
      TypeInternalInt    = 1,   // U64
      TypeInternalFloat  = 2,   // F64
      TypeBeginCustom    = 3    // void*
   };
   
   enum Zone : U16
   {
      ZoneExternal = 0, // externally managed pointer
      ZonePacked   = 1, // packed into CV
      ZoneVmHeap   = 2, // pointer managed by a ConsoleHeapAlloc
      ZoneArg      = 3, // allocated inside VM arg buffer
      ZoneReturn   = 4  // allocated inside VM return buffer
   };
   
   // flags
   enum FlagMask : U16
   {
      MaskVMZone = BIT(1) | BIT(2)
   };
   
   // ---- Storage ----
   U64 cvalue;
   U16 typeId;
   U16 flags;
   
   ConsoleValue() : cvalue(0), typeId(TypeInternalString), flags(0)
   {
   }
   
   inline Zone getZone() const
   {
      return (Zone)(flags & MaskVMZone);
   }
   
   inline void setZone(Zone z)
   {
      flags = static_cast<U16>((flags & ~MaskVMZone) | ((U16)(z)));
   }
   
   static ConsoleValue makeInt(U64 i)
   {
      ConsoleValue v; v.setInt(i); return v;
   }
   static ConsoleValue makeFloat(F64 d)
   {
      ConsoleValue v; v.setFloat(d); return v;
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
      ConsoleValue v; v.setTyped(p, typeId, zone); return v;
   }
   
   inline void setInt(U64 i)
   {
      typeId = TypeInternalInt;
      setZone(ZoneExternal); // zone irrelevant for immediates
      *((U64*)&cvalue) = i;
   }
   
   inline void setFloat(F64 d)
   {
      typeId = TypeInternalFloat;
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
   
   inline void setTyped(void* p, U16 customTypeId, Zone zone=ZoneExternal)
   {
      typeId = customTypeId;
      setZone(zone);
      *((U64*)&cvalue) = *((U64*)p);
   }
   
   inline U64 getInt(U64 def = 0) const
   {
      if (typeId != TypeInternalInt) return def;
      return *((U64*)&cvalue);
   }
   
   inline F64 getFloat(F64 def = 0.0) const
   {
      if (typeId != TypeInternalFloat) return def;
      return *((F64*)&cvalue);
   }

   void* ptr() const
   {
      return (void*)cvalue;
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
         case ZoneArg:
            return addOffset(base.arg, cvalue);
         case ZoneReturn:
            return addOffset(base.ret, cvalue);
         default:
            return NULL;
      }
   }
   
   static inline void* addOffset(const void* base, uint64_t off)
   {
      return (!base) ? NULL : reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(base) + static_cast<uintptr_t>(off));
   }
   
   inline bool isString() const
   {
      return typeId == TypeInternalString;
   }
   inline bool isInt()    const
   {
      return typeId == TypeInternalInt;
   }
   inline bool isFloat()  const
   {
      return typeId == TypeInternalFloat;
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
