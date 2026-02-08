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

#include "sim/dynamicTypes.h"
#include "embed/compilerOpcodes.h"

// Init the globals.
ConsoleBaseType *ConsoleBaseType::smListHead = NULL;
S32              ConsoleBaseType::smConsoleTypeCount = KorkApi::ConsoleValue::TypeBeginCustom; // tge

// And, we also privately store the types lookup table.
std::vector<ConsoleBaseType*> gConsoleTypeTable;

ConsoleBaseType *ConsoleBaseType::getListHead()
{
   return smListHead;
}

void ConsoleBaseType::initialize()
{
   // Prep and empty the vector.
   gConsoleTypeTable.resize(smConsoleTypeCount+1);
   std::fill(gConsoleTypeTable.begin(), gConsoleTypeTable.end(), nullptr);

   // Walk the list and register each one with the console system.
   ConsoleBaseType *walk = getListHead();
   while(walk)
   {
      // Store a pointer to the type in the appropriate slot.
      const S32 id = walk->getTypeID();
      AssertFatal(gConsoleTypeTable[id]==NULL, "ConsoleBaseType::initialize - encountered a table slot that contained something!");
      gConsoleTypeTable[id] = walk;

      // Advance down the list...
      walk = walk->getListNext();
   }

   // Alright, we're all done here; we can now achieve fast lookups by ID.
}

void ConsoleBaseType::registerWithVM(KorkApi::Vm* vm)
{
   for (U32 i=0; i<smConsoleTypeCount; i++)
   {
      if (gConsoleTypeTable[i])
      {
         gConsoleTypeTable[i]->registerTypeWithVm(vm);
      }
   }
}

void ConsoleBaseType::registerTypeWithVm(KorkApi::Vm* vm)
{
   //
   KorkApi::TypeInfo info;
   info.fieldSize = mTypeSize;
   info.valueSize = mValueSize;
   info.name = vm->internString(mTypeName);
   info.inspectorFieldType = vm->internString(mInspectorFieldType);
   info.userPtr = this;
   info.iFuncs.CastValueFn = [](void* userPtr,
                              KorkApi::Vm* vm,
                                     KorkApi::TypeStorageInterface* inputStorage,
                                KorkApi::TypeStorageInterface* outputStorage,
                                     void* fieldUserPtr,
                                     BitSet32 flag,
                                     U32 requestedType){
      ConsoleBaseType* typeInfo = (ConsoleBaseType*)userPtr;
      return typeInfo->getData(vm, inputStorage, outputStorage, fieldUserPtr, flag, requestedType);
   };
   info.iFuncs.PrepDataFn = [](void* userPtr, KorkApi::Vm* vm,
                               const char* data,
                               char* buffer,
                               U32 bufferLen){
      ConsoleBaseType* typeInfo = (ConsoleBaseType*)userPtr;
      return typeInfo->prepData(vm, data, buffer, bufferLen);
   };
   info.iFuncs.GetTypeClassNameFn = [](void* userPtr){
      ConsoleBaseType* typeInfo = (ConsoleBaseType*)userPtr;
      return typeInfo->mTypeName;
   };
   info.iFuncs.PerformOpFn = [](void* userPtr, 
                                KorkApi::Vm* vm,
                                U32 op, 
                                KorkApi::ConsoleValue lhs,
                                KorkApi::ConsoleValue rhs){
      ConsoleBaseType* typeInfo = (ConsoleBaseType*)userPtr;
      return typeInfo->performOp(vm, op, lhs, rhs);
   };
   
   S32 vmTypeId = vm->registerType(info);
   AssertFatal(mTypeId != vmTypeId, "Type Id Mismatch");
}

ConsoleBaseType  *ConsoleBaseType::getType(const S32 typeID)
{
   return gConsoleTypeTable[typeID];
}
//-------------------------------------------------------------------------

ConsoleBaseType::ConsoleBaseType(const U32 size, const U32 vsize, S32 *idPtr, const char *aTypeName)
{
   // General initialization.
   mInspectorFieldType = NULL;

   // Store general info.
   mTypeSize = size;
   mValueSize = vsize;
   mTypeName = aTypeName;

   // Get our type ID and store it.
   mTypeID = smConsoleTypeCount++;
   *idPtr = mTypeID;

   // Link ourselves into the list.
   mListNext = smListHead;
   smListHead = this;

   // Alright, all done for now. Console initialization time code
   // takes us from having a list of general info and classes to
   // a fully initialized type table.
}

ConsoleBaseType::~ConsoleBaseType()
{
   // Nothing to do for now; we could unlink ourselves from the list, but why?
}

using namespace Compiler;

KorkApi::ConsoleValue ConsoleBaseType::performOp(KorkApi::Vm* vm, U32 op, KorkApi::ConsoleValue lhs, KorkApi::ConsoleValue rhs)
{
   return lhs;
}

KorkApi::ConsoleValue ConsoleBaseType::performOpNumeric(KorkApi::Vm* vm, U32 op, KorkApi::ConsoleValue lhs, KorkApi::ConsoleValue rhs)
{
   F64 valueL = vm->valueAsFloat(lhs);
   F64 valueR = vm->valueAsFloat(rhs);
   
   switch (op)
   {
      // unary
      case OP_NOT:
         valueL = !((U64)valueL);
         break;
      case OP_NOTF:
         valueL = !valueL;
         break;
      case OP_ONESCOMPLEMENT:
         valueL = ~((U64)valueL);
         break;
      case OP_NEG:
         valueL = -valueL;
         break;
         
      // comparisons (return 0/1)
      case OP_CMPEQ: valueL = (valueL == valueR) ? 1.0f : 0.0f; break;
      case OP_CMPNE: valueL = (valueL != valueR) ? 1.0f : 0.0f; break;
      case OP_CMPGR: valueL = (valueL >  valueR) ? 1.0f : 0.0f; break;
      case OP_CMPGE: valueL = (valueL >= valueR) ? 1.0f : 0.0f; break;
      case OP_CMPLT: valueL = (valueL <  valueR) ? 1.0f : 0.0f; break;
      case OP_CMPLE: valueL = (valueL <= valueR) ? 1.0f : 0.0f; break;
      
      // bitwise (operate on integer views)
      case OP_XOR:
         valueL = (F32)(((U64)valueL) ^ ((U64)valueR));
         break;
         
      case OP_BITAND:
         valueL = (F32)(((U64)valueL) & ((U64)valueR));
         break;
         
      case OP_BITOR:
         valueL = (F32)(((U64)valueL) | ((U64)valueR));
         break;
         
      case OP_SHR:
      {
         const U64 a = (U64)valueL;
         const U64 b = (U64)valueR;
         valueL = (F32)(a >> b);
         break;
      }
         
      case OP_SHL:
      {
         const U64 a = (U64)valueL;
         const U64 b = (U64)valueR;
         valueL = (F32)(a << b);
         break;
      }
         
      // logical (return 0/1)
      case OP_AND:
         valueL = (valueL != 0.0f && valueR != 0.0f) ? 1.0f : 0.0f;
         break;
         
      case OP_OR:
         valueL = (valueL != 0.0f || valueR != 0.0f) ? 1.0f : 0.0f;
         break;
         
      // arithmetic
      case OP_ADD: valueL = valueL + valueR; break;
      case OP_SUB: valueL = valueL - valueR; break;
      case OP_MUL: valueL = valueL * valueR; break;
         
      case OP_DIV:
         valueL = (valueR == 0.0f) ? 0.0f : (valueL / valueR);
         break;
         
      case OP_MOD:
      {
         const U64 a = (U64)valueL;
         const U64 b = (U64)valueR;
         valueL = (b == 0u) ? 0.0f : (F32)(a % b);
         break;
      }
         
      default:
         break;
   }
   
   return KorkApi::ConsoleValue::makeNumber(valueL);
}

KorkApi::ConsoleValue ConsoleBaseType::performOpUnsigned(KorkApi::Vm* vm, U32 op, KorkApi::ConsoleValue lhs, KorkApi::ConsoleValue rhs)
{
   U64 valueL = vm->valueAsInt(lhs);
   U64 valueR = vm->valueAsInt(rhs);
   
   switch (op)
   {
      // unary
      case OP_NOT:
         valueL = !((U64)valueL);
         break;
      case OP_NOTF:
         valueL = !(F64)valueL;
         break;
      case OP_ONESCOMPLEMENT:
         valueL = ~((U64)valueL);
         break;
      case OP_NEG:
         valueL = -valueL;
         break;
         
      // comparisons (return 0/1)
      case OP_CMPEQ: valueL = (valueL == valueR) ? 1 : 0; break;
      case OP_CMPNE: valueL = (valueL != valueR) ? 1 : 0; break;
      case OP_CMPGR: valueL = (valueL >  valueR) ? 1 : 0; break;
      case OP_CMPGE: valueL = (valueL >= valueR) ? 1 : 0; break;
      case OP_CMPLT: valueL = (valueL <  valueR) ? 1 : 0; break;
      case OP_CMPLE: valueL = (valueL <= valueR) ? 1 : 0; break;
         
         
      // bitwise (operate on integer views)
      case OP_XOR:
         valueL = (U64)(((U64)valueL) ^ ((U64)valueR));
         break;
         
      case OP_BITAND:
         valueL = (U64)(((U64)valueL) & ((U64)valueR));
         break;
         
      case OP_BITOR:
         valueL = (U64)(((U64)valueL) | ((U64)valueR));
         break;
         
      case OP_SHR:
      {
         const U64 a = (U64)valueL;
         const U64 b = (U64)valueR;
         valueL = (F32)(a >> b);
         break;
      }
         
      case OP_SHL:
      {
         const U64 a = (U64)valueL;
         const U64 b = (U64)valueR;
         valueL = (F32)(a << b);
         break;
      }
         
      // logical (return 0/1)
      case OP_AND:
         valueL = (valueL != 0 && valueR != 0) ? 1 : 0;
         break;
         
      case OP_OR:
         valueL = (valueL != 0 || valueR != 0) ? 1 : 0;
         break;
         
         // arithmetic
      case OP_ADD: valueL = valueL + valueR; break;
      case OP_SUB: valueL = valueL - valueR; break;
      case OP_MUL: valueL = valueL * valueR; break;
         
      case OP_DIV:
         valueL = (valueR == 0) ? 0 : (valueL / valueR);
         break;
         
      case OP_MOD:
      {
         const U64 a = (U64)valueL;
         const U64 b = (U64)valueR;
         valueL = (b == 0u) ? 0 : (F32)(a % b);
         break;
      }
         
      default:
         break;
   }
   
   return KorkApi::ConsoleValue::makeUnsigned(valueL);
}
