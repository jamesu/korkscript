#include "platform/platform.h"
#include "console/console.h"
#include <stdio.h>
#include "sim/simBase.h"
#include "sim/dynamicTypes.h"
#include "core/fileStream.h"

struct MyPoint3F
{
   F32 x,y,z;
};

DECLARE_CONSOLETYPE(TypeMyPoint3F);

ConsoleType( MyPoint3F, TypeMyPoint3F, sizeof(MyPoint3F), "" )

ConsoleGetType( TypeMyPoint3F )
{
   static char buf[96];
   MyPoint3F* pt = reinterpret_cast<MyPoint3F*>(dptr);
   KorkApi::ConsoleValue cv;
   U32 realRequestedType = requestedZone & KorkApi::TypeDirectCopyMask;

   if (realRequestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      char* destPtr = buf;
      
      if (requestedZone != KorkApi::ConsoleValue::ZoneExternal)
      {
         cv = vmPtr->getStringInZone(requestedZone, 256);
         if (cv.isNull())
         {
            return cv;
         }
      }
      else
      {
         cv = KorkApi::ConsoleValue::makeString(destPtr);
      }
      
      dSprintf(destPtr, 256, "%g %g %g", pt->x, pt->y, pt->z);
      return cv;
   }
   else if ((requestedType & KorkApi::TypeDirectCopy) != 0) // i.e. same type
   {
      cv = vmPtr->getTypeInZone(requestedZone, realRequestedType);
      *((MyPoint3F*)cv.ptr()) = *pt;
      return cv;
   }
}

ConsoleSetType( TypeMyPoint3F )
{
   MyPoint3F* p = reinterpret_cast<MyPoint3F*>(dptr);
   if (!p)
      return;
   
   if (argc >= 3)
   {
      p->x = vmPtr->valueAsFloat(argv[0]);
      p->y = vmPtr->valueAsFloat(argv[1]);
      p->z = vmPtr->valueAsFloat(argv[2]);
   }
   else if (argc == 1)
   {
      if (argv[0].typeId == typeId)
      {
         *p = *((MyPoint3F*)(argv[0].evaluatePtr(vmPtr->getAllocBase())));
      }
      else if (argv[0].typeId == KorkApi::ConsoleValue::TypeInternalString)
      {
         *p = {};
         const char* inputStr = (const char*)(argv[0].evaluatePtr(vmPtr->getAllocBase()));
         sscanf(inputStr, "%f %f %f", &p->x, &p->y, &p->z);
      }
      else
      {
         *((MyPoint3F*)dptr) = {};
      }
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
   
   //Con::evaluatef("echo(\"Hello world\" SPC TorqueScript SPC is SPC amazing);");
   
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

