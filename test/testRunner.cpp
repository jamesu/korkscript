#include "platform/platform.h"
#include "console/console.h"
#include <stdio.h>
#include "sim/simBase.h"
#include "sim/dynamicTypes.h"
#include "core/fileStream.h"
#include "core/stringUnit.h"

S32 gReturnCode = 0;
U32 gNumPasses = 0;
U32 gNumFails = 0;

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
   U8* blob = NULL;
   
   const S32 count = StringUnit::getUnitCount(list, " \t\n");
   if (count <= 0)
      return false;

   KorkApi::FiberId* fibers = (KorkApi::FiberId*)dMalloc(sizeof(KorkApi::FiberId) * count);

   for (S32 i = 0; i < count; ++i)
   {
      const char* unit = StringUnit::getUnit(list, i, " \t\n");
      fibers[i] = (KorkApi::FiberId)dAtoi(unit);
   }

   bool ok = vmPtr->dumpFiberStateToBlob((U32)count, fibers, &blobSize, &blob);
   dFree(fibers);

   if (!ok)
   {
      return false;
   }

   if (fs.open(argv[2], FileStream::Write))
   {
      fs.write(blobSize, blob);
      didWrite = true;
   }

   dFree(blob);
   return didWrite;
}

ConsoleFunction(restoreFibers, const char*, 2, 2, "fileName")
{
   FileStream fs;
   
   if (fs.open(argv[1], FileStream::Read))
   {
      U32 blobSize = fs.getStreamSize();
      U8* blob = (U8*)dMalloc(blobSize);
      fs.read(blobSize, blob);
      
      KorkApi::FiberId* outFibers = NULL;
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

         dFree(outFibers);
         dFree(blob);
         return result;
      }

      dFree(blob);
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
   
   const char* returnValue = Con::evaluate(argv[2], false, NULL);
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
	Con::addConsumer(MyLogger, NULL);

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

