//-----------------------------------------------------------------------------
// Copyright (c) 2025-2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "platform/platformString.h"
#include "console/console.h"
#include <stdio.h>
#include "sim/simBase.h"
#include "sim/dynamicTypes.h"
#include "core/fileStream.h"
#include "core/stringUnit.h"

S32 gReturnCode = 0;
U32 gNumPasses = 0;
U32 gNumFails = 0;


// Hack until we define these properly in the API
namespace KorkApi
{
TypeStorageInterface CreateRegisterStorageFromArgs(KorkApi::VmInternal* vmInternal, U32 argc, KorkApi::ConsoleValue* argv);
}

struct MyPoint3F
{
   F32 x,y,z;
};

ConsoleType( MyPoint3F, TypeMyPoint3F, sizeof(MyPoint3F), sizeof(MyPoint3F), "" )

ConsoleGetType( TypeMyPoint3F )
{
   const KorkApi::ConsoleValue* argv = nullptr;
   U32 argc = inputStorage ? inputStorage->data.argc : 0;
   bool directLoad = false;

   if (argc > 0 && inputStorage->data.storageRegister)
   {
      argv = inputStorage->data.storageRegister;
   }
   else
   {
      argc = 1;
      argv = &inputStorage->data.storageAddress;
      directLoad = true;
   }

   MyPoint3F v = {0, 0, 0};

   if (inputStorage->isField && directLoad)
   {
      const MyPoint3F* src = (const MyPoint3F*)inputStorage->data.storageAddress.evaluatePtr(vmPtr->getAllocBase());
      if (!src) return false;
      v = *src;
   }
   else
   {
      if (argc == 3)
      {
         v.x = (F32)argv[0].getFloat((F64)argv[0].getInt(0));
         v.y = (F32)argv[1].getFloat((F64)argv[1].getInt(0));
         v.z = (F32)argv[2].getFloat((F64)argv[2].getInt(0));
      }
      else if (argc == 1)
      {
         if (argv[0].typeId == TypeMyPoint3F)
         {
            // NOTE: This needs to use evaluatePtr as custom storage can be used
            const MyPoint3F* src = (MyPoint3F*)argv[0].evaluatePtr(vmPtr->getAllocBase());
            if (src)
            {
               v = *src;
            }
         }
         else
         {
            const char* s = vmPtr->valueAsString(argv[0]);
            if (!s) s = "";
            
            dSscanf(s, "%g %g %g", &v.x, &v.y, &v.z);
         }
      }
      else
      {
         // Not supported
         return false;
      }
   }

   // -> output

   if (requestedType == TypeMyPoint3F)
   {
      MyPoint3F* dstPtr = (MyPoint3F*)outputStorage->data.storageAddress.evaluatePtr(vmPtr->getAllocBase());
      if (!dstPtr)
      {
         return false;
      }

      *dstPtr = v;

      if (outputStorage->data.storageRegister)
         *outputStorage->data.storageRegister = outputStorage->data.storageAddress;

      return true;
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      const U32 bufLen = 96;

      outputStorage->FinalizeStorage(outputStorage, bufLen);

      char* out = (char*)outputStorage->data.storageAddress.evaluatePtr(vmPtr->getAllocBase());
      if (!out) return false;

      dSprintf(out, bufLen, "%.9g %.9g %.9g", v.x, v.y, v.z);

      if (outputStorage->data.storageRegister)
         *outputStorage->data.storageRegister = outputStorage->data.storageAddress;

      return true;
   }
   else
   {
      KorkApi::ConsoleValue vals[3];
      vals[0] = KorkApi::ConsoleValue::makeNumber(v.x);
      vals[1] = KorkApi::ConsoleValue::makeNumber(v.y);
      vals[2] = KorkApi::ConsoleValue::makeNumber(v.z);

      KorkApi::TypeStorageInterface castInput =
         KorkApi::CreateRegisterStorageFromArgs(vmPtr->mInternal, 3, vals);

      return vmPtr->castValue(requestedType, &castInput, outputStorage, fieldUserPtr, flag);
   }
}

ConsoleTypeOp( TypeMyPoint3F )
{
   MyPoint3F* firstPoint = lhs.typeId == TypeMyPoint3F ? (MyPoint3F*)lhs.evaluatePtr(vmPtr->getAllocBase()) : nullptr;
   MyPoint3F* secondPoint = rhs.typeId == TypeMyPoint3F ? (MyPoint3F*)rhs.evaluatePtr(vmPtr->getAllocBase()) : nullptr;
   MyPoint3F* outPoint = nullptr;
   F32 otherScalar = 1.0f;
   
   MyPoint3F first;
   MyPoint3F second;
   
   if (firstPoint == nullptr)
   {
      otherScalar = vmPtr->valueAsFloat(lhs);
      first.x = first.y = first.z = otherScalar;
      second = *secondPoint;
      outPoint = secondPoint;
   }
   else if (secondPoint == nullptr)
   {
      otherScalar = vmPtr->valueAsFloat(rhs);
      second.x = second.y = second.z = otherScalar;
      first = *firstPoint;
      outPoint = firstPoint;
   }
   else
   {
      first = *firstPoint;
      second = *secondPoint;
      outPoint = firstPoint;
   }
      
   
   MyPoint3F otherPointValue;
   using namespace Compiler;
   
   switch (op)
   {
      case OP_ADD:
         outPoint->x = first.x + second.x;
         outPoint->y = first.y + second.y;
         outPoint->z = first.z + second.z;
         break;
      case OP_SUB:
         outPoint->x = first.x - second.x;
         outPoint->y = first.y - second.y;
         outPoint->z = first.z - second.z;
         break;
      case OP_MUL:
         outPoint->x = first.x * second.x;
         outPoint->y = first.y * second.y;
         outPoint->z = first.z * second.z;
         break;
      case OP_DIV:
         outPoint->x = second.x == 0 ? 0 : first.x / second.x;
         outPoint->y = second.y == 0 ? 0 : first.y / second.y;
         outPoint->z = second.z == 0 ? 0 : first.z / second.z;
         break;
      case OP_NEG:
         outPoint->x = -first.x;
         outPoint->y = -first.y;
         outPoint->z = -first.z;
         break;
      default:
         break;
   }
   
   return outPoint == firstPoint ? lhs : rhs;
}

ConsoleFunction(testAssert, void, 3, 3, "msg, cond")
{
   if (!dAtob(argv[2]))
   {
      Con::errorf("Failed: %s\n", argv[1]);
      gReturnCode = 1;
      gNumFails++;
   }
   else
   {
      gNumPasses++;
   }
}

ConsoleFunction(testInt, void, 4, 4, "msg, value, expected")
{
   if (dAtoi(argv[2]) != dAtoi(argv[3]))
   {
      Con::errorf("Failed: %s (got %s)\n", argv[1], argv[2]);
      gReturnCode = 1;
      gNumFails++;
   }
   else
   {
      gNumPasses++;
   }
}

ConsoleFunction(testNumber, void, 4, 4, "msg, value, expected")
{
   if (dAtof(argv[2]) != dAtof(argv[3]))
   {
      Con::errorf("Failed: %s (got %s)\n", argv[1], argv[2]);
      gReturnCode = 1;
      gNumFails++;
   }
   else
   {
      gNumPasses++;
   }
}

ConsoleFunction(testString, void, 4, 4, "msg, value, expected")
{
   if (strcmp(argv[2], argv[3]) != 0)
   {
      Con::errorf("Failed: %s (got %s)\n", argv[1], argv[2]);
      gReturnCode = 1;
      gNumFails++;
   }
   else
   {
      gNumPasses++;
   }
}

ConsoleFunction(yieldFiber, S32, 2, 2, "value")
{
   vmPtr->suspendCurrentFiber();
   return dAtoi(argv[1]); // NOTE: this will be set as yield value
}

ConsoleFunction(throwFiber, void, 3, 3, "value, soft")
{
   vmPtr->throwFiber(((U32)dAtoi(argv[1])) | (dAtob(argv[2]) ? BIT(31) : 0));
}

ConsoleFunction(saveFibers, bool, 3, 3, "fiberIdList, fileName")
{
   const char* list = argv[1];
   
   FileStream fs;
   bool didWrite = false;
   U32 blobSize = 0;
   U8* blob = nullptr;
   
   const S32 count = StringUnit::getUnitCount(list, " \t\n");
   if (count <= 0)
      return false;

   KorkApi::FiberId* fibers = (KorkApi::FiberId*)malloc(sizeof(KorkApi::FiberId) * count);

   for (S32 i = 0; i < count; ++i)
   {
      const char* unit = StringUnit::getUnit(list, i, " \t\n");
      fibers[i] = (KorkApi::FiberId)dAtoi(unit);
   }

   bool ok = vmPtr->dumpFiberStateToBlob((U32)count, fibers, &blobSize, &blob);
   free(fibers);

   if (!ok)
   {
      return false;
   }

   if (fs.open(argv[2], FileStream::Write))
   {
      fs.write(blobSize, blob);
      didWrite = true;
   }

   free(blob);
   return didWrite;
}

ConsoleFunction(restoreFibers, const char*, 2, 2, "fileName")
{
   FileStream fs;
   
   if (fs.open(argv[1], FileStream::Read))
   {
      U32 blobSize = fs.getStreamSize();
      U8* blob = (U8*)malloc(blobSize);
      fs.read(blobSize, blob);
      
      KorkApi::FiberId* outFibers = nullptr;
      U32 outNumFibers = 0;
      
      const char* result = "";

      if (vmPtr->restoreFiberStateFromBlob(&outNumFibers, &outFibers, blobSize, blob))
      {
         KorkApi::ConsoleValue cbuf = Con::getReturnBuffer(outNumFibers * 32);
         char* buf = (char*)cbuf.evaluatePtr(vmPtr->getAllocBase());
         buf[0] = 0;

         for (U32 i = 0; i < outNumFibers; i++)
         {
            char tmp[32];
            dSprintf(tmp, sizeof(tmp), "%u", outFibers[i]);

            if (i > 0)
               dStrcat(buf, " ");

            dStrcat(buf, tmp);
         }
         
         KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeString(buf);
         result = vmPtr->valueAsString(cv);

         free(outFibers);
         free(blob);
         return result;
      }

      free(blob);
   }
   
   return "";
}


ConsoleFunction(createFiber, const char*, 1, 1, "")
{
   KorkApi::FiberId fiberId = vmPtr->createFiber();
   KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeUnsigned(fiberId);
   return vmPtr->valueAsString(cv);
}

ConsoleFunction(evalInFiber, const char*, 3, 3, "fiberId, code")
{
   KorkApi::FiberId existingFiberId = vmPtr->getCurrentFiber();
   
   KorkApi::FiberId fiberId = (KorkApi::FiberId)std::atoll(argv[1]);
   vmPtr->setCurrentFiber(fiberId);
   
   const char* returnValue = Con::evaluate(argv[2], false, nullptr);
   vmPtr->clearCurrentFiberError();
   
   vmPtr->setCurrentFiber(existingFiberId);
   return returnValue;
}

ConsoleFunction(resumeFiber, const char*, 3, 3, "fiberId, value")
{
   KorkApi::FiberId existingFiberId = vmPtr->getCurrentFiber();
   
   KorkApi::FiberId fiberId = (KorkApi::FiberId)std::atoll(argv[1]);
   vmPtr->setCurrentFiber(fiberId);
   
   KorkApi::FiberRunResult result = vmPtr->resumeCurrentFiber(KorkApi::ConsoleValue::makeString(argv[2]));
   
   vmPtr->setCurrentFiber(existingFiberId);
   return vmPtr->valueAsString(result.value);
}

ConsoleFunction(stopFiber, void, 2, 2, "fiberId")
{
   KorkApi::FiberId fiberId = (KorkApi::FiberId)std::atoll(argv[1]);
   vmPtr->cleanupFiber(fiberId);
}

ConsoleFunction(readFiberLocalVariable, const char*, 3, 3, "fiberId, localVarName")
{
   KorkApi::FiberId existingFiberId = vmPtr->getCurrentFiber();
   
   KorkApi::FiberId fiberId = (KorkApi::FiberId)std::atoll(argv[1]);
   vmPtr->setCurrentFiber(fiberId);
   
   KorkApi::ConsoleValue retValue = vmPtr->getLocalVariable(StringTable->insert(argv[2]));
   
   vmPtr->setCurrentFiber(existingFiberId);
   return vmPtr->valueAsString(retValue);
}

void MyLogger(U32 level, const char *consoleLine, void*)
{
	printf("%s\n", consoleLine);
}

int main(int argc, char **argv)
{
	Con::init();
   Sim::init();
	Con::addConsumer(MyLogger, nullptr);

   if (argc < 2)
   {
      Con::printf("Not enough args\n");
      return 1;
   }

   FileStream fs;
   if (!fs.open(argv[1], FileStream::Read))
   {
      Con::printf("Error loading file %s\n", argv[1]);
      return 1;
   }
   
   char* data = new char[fs.getStreamSize()+1];
   fs.read(fs.getStreamSize(), data);
   data[fs.getStreamSize()] = '\0';
   
   const char* res = Con::evaluate(data);

   delete[] data;

   Con::printf("Tests passed: %i, failed: %i\n", gNumPasses, gNumFails);

	return gReturnCode;
}

