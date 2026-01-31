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

struct MyPoint3F
{
   F32 x,y,z;
};

// Hack until we define these properly in the API
namespace KorkApi
{
TypeStorageInterface CreateRegisterStorageFromArgs(KorkApi::VmInternal* vmInternal, U32 argc, KorkApi::ConsoleValue* argv);
}

ConsoleType( MyPoint3F, TypeMyPoint3F, sizeof(MyPoint3F), sizeof(MyPoint3F), "" )

ConsoleTypeOpDefault( TypeMyPoint3F )

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
         const char* s = vmPtr->valueAsString(argv[0]);
         if (!s) s = "";

         dSscanf(s, "%g %g %g", &v.x, &v.y, &v.z);
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

      return vmPtr->castValue(requestedType, &castInput, outputStorage, tbl, flag);
   }
}

class Player : public SimObject
{
   typedef SimObject Parent;
   
public:
   
   MyPoint3F mPosition;
   
   Player()
   {
      mPosition = {};
   }
   
   static void initPersistFields()
   {
      Parent::initPersistFields();
      addField("position", TypeMyPoint3F, Offset(mPosition, Player));
   }
   
   DECLARE_CONOBJECT(Player);
};

IMPLEMENT_CONOBJECT(Player);

ConsoleMethod(Player, jump, void, 2, 2, "")
{
   object->mPosition.z += 10;
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
   
   Con::evaluatef("echo(\"Hello world\" SPC TorqueScript SPC is SPC amazing);");
   
   FileStream fs;
   if (!fs.open(argv[1], FileStream::Read))
   {
      printf("Error loading file %s\n", argv[1]);
      return 1;
   }
   
   char* data = new char[fs.getStreamSize()+1];
   fs.read(fs.getStreamSize(), data);
   data[fs.getStreamSize()] = '\0';
   
   Con::evaluate(data);

	return 0;
}

