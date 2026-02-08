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

#ifndef _DYNAMIC_CONSOLETYPES_H_
#define _DYNAMIC_CONSOLETYPES_H_

#ifndef _SIMBASE_H_
#include "sim/simBase.h"
#endif

#include "embed/api.h"

class ConsoleBaseType
{
protected:
   /// This is used to generate unique IDs for each type.
   static S32 smConsoleTypeCount;

   /// We maintain a linked list of all console types; this is its head.
   static ConsoleBaseType *smListHead;

   /// Next item in the list of all console types.
   ConsoleBaseType *mListNext;

   /// Destructor is private to avoid people mucking up the list.
   ~ConsoleBaseType();

   S32      mTypeID;
   dsize_t  mTypeSize;
   dsize_t  mValueSize;
   
public:
   const char *mTypeName;
   const char *mInspectorFieldType;

public:

   /// @name cbt_list List Interface
   ///
   /// Interface for accessing/traversing the list of types.

   /// Get the head of the list.
   static ConsoleBaseType *getListHead();

   /// Get the item that follows this item in the list.
   ConsoleBaseType *getListNext() const
   {
      return mListNext;
   }

   /// Called once to initialize the console type system.
   static void initialize();
   
   static void registerWithVM(KorkApi::Vm* vm);
   
   void registerTypeWithVm(KorkApi::Vm* vm);

   /// Call me to get a pointer to a type's info.
   static ConsoleBaseType *getType(const S32 typeID);

   /// @}

   /// The constructor is responsible for linking an element into the
   /// master list, registering the type ID, etc.
   ConsoleBaseType(const U32 size, const U32 vsize, S32 *idPtr, const char *aTypeName);

   const S32 getTypeID() const { return mTypeID; }
   const dsize_t getFieldSize() const { return mTypeSize; }
   const dsize_t getValueSize() const { return mValueSize; }
   const char *getTypeName() const { return mTypeName; }

   void setInspectorFieldType(const char *type) { mInspectorFieldType = type; }
   const char *getInspectorFieldType() { return mInspectorFieldType; }

   virtual bool getData(KorkApi::Vm* vmPtr, KorkApi::TypeStorageInterface *inputStorage, KorkApi::TypeStorageInterface *outputStorage, void* fieldUserPtr, BitSet32 flag, U32 requestedType)=0;
   virtual const char *getTypeClassName()=0;
   virtual const bool isDatablock() { return false; };
   virtual const char *prepData(KorkApi::Vm* vmPtr, const char *data, char *buffer, U32 bufferLen) { return data; };
   virtual StringTableEntry getTypePrefix( void ) const { return StringTable->EmptyString; }

   virtual void exportToVm(KorkApi::Vm* vm) = 0;
   virtual KorkApi::ConsoleValue performOp(KorkApi::Vm* vm, U32 op, KorkApi::ConsoleValue lhs, KorkApi::ConsoleValue rhs);
   virtual KorkApi::ConsoleValue performOpNumeric(KorkApi::Vm* vm, U32 op, KorkApi::ConsoleValue lhs, KorkApi::ConsoleValue rhs);
   virtual KorkApi::ConsoleValue performOpUnsigned(KorkApi::Vm* vm, U32 op, KorkApi::ConsoleValue lhs, KorkApi::ConsoleValue rhs);
};

#define DefineConsoleType( type ) extern S32 type;

#define ConsoleType( typeName, type, size, vsize, typePrefix ) \
   class ConsoleType##type : public ConsoleBaseType \
   { \
   public: \
      ConsoleType##type (const U32 aSize, const U32 vSize, S32 *idPtr, const char *aTypeName) : ConsoleBaseType(aSize, vSize, idPtr, aTypeName) { } \
      virtual bool getData(KorkApi::Vm* vmPtr, KorkApi::TypeStorageInterface *inputStorage, KorkApi::TypeStorageInterface *outputStorage, void* fieldUserPtr, BitSet32 flag, U32 requestedType); \
      virtual const char *getTypeClassName() { return #typeName ; } \
      virtual StringTableEntry getTypePrefix( void ) const { return StringTable->insert( typePrefix ); }\
      void exportToVm(KorkApi::Vm* vm) { exportTypeToVm(this, vm); } \
      virtual KorkApi::ConsoleValue performOp(KorkApi::Vm* vm, U32 op, KorkApi::ConsoleValue lhs, KorkApi::ConsoleValue rhs); \
   }; \
   S32 type = -1; \
   ConsoleType##type gConsoleType##type##Instance(size,vsize,&type,#type); \

#define ConsolePrepType( typeName, type, size, vsize, typePrefix ) \
   class ConsoleType##type : public ConsoleBaseType \
   { \
   public: \
      ConsoleType##type (const U32 aSize, const U32 vSize, S32 *idPtr, const char *aTypeName) : ConsoleBaseType(aSize, vSize, idPtr, aTypeName) { } \
      virtual bool getData(KorkApi::Vm* vmPtr,  KorkApi::TypeStorageInterface *inputStorage, KorkApi::TypeStorageInterface *outputStorage, void* fieldUserPtr, BitSet32 flag, U32 requestedType); \
      virtual const char *getTypeClassName() { return #typeName; }; \
      virtual const char *prepData(KorkApi::Vm* vmPtr, const char *data, char *buffer, U32 bufferLen); \
      virtual StringTableEntry getTypePrefix( void ) const { return StringTable->insert( typePrefix ); }\
      void exportToVm(KorkApi::Vm* vm) { exportTypeToVm(this, vm); } \
      virtual KorkApi::ConsoleValue performOp(KorkApi::Vm* vm, U32 op, KorkApi::ConsoleValue lhs, KorkApi::ConsoleValue rhs); \
   }; \
   S32 type = -1; \
   ConsoleType##type gConsoleType##type##Instance(size,&type,#type); \

#define ConsoleGetInputStoragePtr()\
   (inputStorage->data.storageAddress.evaluatePtr(vmPtr->getAllocBase()))

#define ConsoleGetOutputStoragePtr()\
   (outputStorage->data.storageAddress.evaluatePtr(vmPtr->getAllocBase()))

#define ConsoleGetType( type ) \
   bool ConsoleType##type::getData(KorkApi::Vm* vmPtr, KorkApi::TypeStorageInterface *inputStorage, KorkApi::TypeStorageInterface *outputStorage, void* fieldUserPtr, BitSet32 flag, U32 requestedType)

#define ConsoleTypeOp( type ) \
   KorkApi::ConsoleValue ConsoleType##type::performOp(KorkApi::Vm* vmPtr, U32 op, KorkApi::ConsoleValue lhs, KorkApi::ConsoleValue rhs)

#define ConsoleTypeOpDefault( type ) \
   KorkApi::ConsoleValue ConsoleType##type::performOp(KorkApi::Vm* vmPtr, U32 op, KorkApi::ConsoleValue lhs, KorkApi::ConsoleValue rhs) { return ConsoleBaseType::performOp(vmPtr, op, lhs, rhs); }

#define ConsoleTypeOpDefaultNumeric( type ) \
   KorkApi::ConsoleValue ConsoleType##type::performOp(KorkApi::Vm* vmPtr, U32 op, KorkApi::ConsoleValue lhs, KorkApi::ConsoleValue rhs) { return ConsoleBaseType::performOpNumeric(vmPtr, op, lhs, rhs); }

#define ConsoleTypeOpDefaultUnsigned( type ) \
   KorkApi::ConsoleValue ConsoleType##type::performOp(KorkApi::Vm* vmPtr, U32 op, KorkApi::ConsoleValue lhs, KorkApi::ConsoleValue rhs) { return ConsoleBaseType::performOpUnsigned(vmPtr, op, lhs, rhs); }

#define ConsoleCopyToOutput( value ) \
   CopyTypeStorageValueToOutput(outputStorage, value);

#define ConsolePrepData( type ) \
   const char *ConsoleType##type::prepData(KorkApi::Vm* vmPtr, const char *data, char *buffer, U32 bufferSize)

#define ConsoleTypeFieldPrefix( type, typePrefix ) \
   StringTableEntry ConsoleType##type::getTypePrefix( void ) const { return StringTable->insert( typePrefix ); }

#define DatablockConsoleType( typeName, type, size, className ) \
   class ConsoleType##type : public ConsoleBaseType \
   { \
   public: \
      ConsoleType##type (const S32 aSize, S32 *idPtr, const char *aTypeName) : ConsoleBaseType(aSize, idPtr, aTypeName) { } \
      virtual bool getData(KorkApi::Vm* vmPtr, KorkApi::TypeStorageInterface* inputStorage, void* fieldUserPtr, BitSet32 flag, U32 requestedType); \
      virtual const char *getTypeClassName() { return #className; }; \
      virtual const bool isDatablock() { return true; }; \
   }; \
   S32 type = -1; \
   ConsoleType##type gConsoleType##type##Instance(size,&type,#type); \



template <class T>
inline KorkApi::TypeInterface buildTypeInterface()
{
   KorkApi::TypeInterface ti{};

   ti.CastValueFn = &KorkApi::APIThunk<ConsoleBaseType,
      static_cast<bool(ConsoleBaseType::*)(KorkApi::Vm*,KorkApi::TypeStorageInterface*, KorkApi::TypeStorageInterface*, void*, BitSet32, U32)>
      (&ConsoleBaseType::getData)>::call;

   ti.GetTypeClassNameFn = &KorkApi::APIThunk<ConsoleBaseType,
      static_cast<const char*(ConsoleBaseType::*)()>
      (&ConsoleBaseType::getTypeClassName)>::call;

   ti.PrepDataFn = &KorkApi::APIThunk<ConsoleBaseType,
      static_cast<const char*(ConsoleBaseType::*)(KorkApi::Vm*,const char*, char*, U32)>
      (&ConsoleBaseType::prepData)>::call;

   return ti;
}

template <typename T>
inline void exportTypeToVm(T* self, KorkApi::Vm* vm)
{
   KorkApi::TypeInfo info{};
   info.name = StringTable->insert(self->getTypeName());
   info.inspectorFieldType = StringTable->insert(self->getInspectorFieldType() ? self->getInspectorFieldType() : "");
   info.userPtr = self;
   info.fieldSize = self->getFieldSize();
   info.valueSize = self->getValueSize();
   info.iFuncs = buildTypeInterface<T>();
   vm->registerType(info);
}

#endif // _DYNAMIC_CONSOLETYPES_H_
