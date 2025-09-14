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

#include "console/console.h"
#include "console/consoleTypes.h"
#include "core/stringTable.h"
#include "sim/simBase.h"
#include "core/stringUnit.h"

//////////////////////////////////////////////////////////////////////////
// TypeString
//////////////////////////////////////////////////////////////////////////
ConsoleType( string, TypeString, sizeof(const char*), "" )

ConsoleGetType( TypeString )
{
   return KorkApi::ConsoleValue::makeString(*((const char **)(dptr)));
}

ConsoleSetType( TypeString )
{
   if(argc == 1)
      *((const char **) dptr) = StringTable->insert((const char*)argv[0].evaluatePtr(vmPtr->getAllocBase()));
   else
      Con::printf("(TypeString) Cannot set multiple args to a single string.");
}

/////////////////////////////////////////////////////////////////////////
// TypeStringEntryVector
//////////////////////////////////////////////////////////////////////////
ConsoleType( string, TypeStringTableEntryVector, sizeof(Vector<StringTableEntry>), "" )

ConsoleGetType( TypeStringTableEntryVector )
{
   Vector<StringTableEntry> *vec = (Vector<StringTableEntry>*)dptr;
   KorkApi::ConsoleValue returnValue = Con::getReturnBuffer(1024);
   char* returnBuffer = (char*)returnValue.evaluatePtr(vmPtr->getAllocBase());
   S32 maxReturn = 1024;
   returnBuffer[0] = '\0';
   S32 returnLeng = 0;
   for (Vector<StringTableEntry>::iterator itr = vec->begin(); itr < vec->end(); itr++)
   {
      // concatenate the next value onto the return string
       if ( itr == vec->begin() )
       {
           dSprintf(returnBuffer + returnLeng, maxReturn - returnLeng, "%s", *itr);
       }
       else
       {
           dSprintf(returnBuffer + returnLeng, maxReturn - returnLeng, ",%s", *itr);
       }
      returnLeng = dStrlen(returnBuffer);
   }
   return returnValue;
}

ConsoleSetType( TypeStringTableEntryVector )
{
   Vector<StringTableEntry> *vec = (Vector<StringTableEntry>*)dptr;
   // we assume the vector should be cleared first (not just appending)
   vec->clear();
   if (argc == 1)
   {
       KorkApi::ConsoleValue argV = argv[0];
       const char* arg = (const char*)argV.evaluatePtr(vmPtr->getAllocBase());
       const U32 unitCount = StringUnit::getUnitCount(arg, ",");
       for( U32 unitIndex = 0; unitIndex < unitCount; ++unitIndex )
       {
           vec->push_back( StringTable->insert( StringUnit::getUnit(arg, unitIndex, ",") ) );
       }
   }
   else if (argc > 1)
   {
      for (S32 i = 0; i < argc; i++)
      {
            KorkApi::ConsoleValue argV = argv[i];
            vec->push_back( StringTable->insert( (const char*)argV.evaluatePtr(vmPtr->getAllocBase()) ) );
      }
   }
   else
      Con::printf("Vector<String> must be set as { a, b, c, ... } or \"a,b,c, ...\"");
}


//////////////////////////////////////////////////////////////////////////
// TypeCaseString
//////////////////////////////////////////////////////////////////////////
ConsoleType( caseString, TypeCaseString, sizeof(const char*), "" )

ConsoleSetType( TypeCaseString )
{
   if(argc == 1)
      *((const char **) dptr) = StringTable->insert((const char*)argv[0].evaluatePtr(vmPtr->getAllocBase()), true);
   else
      Con::printf("(TypeCaseString) Cannot set multiple args to a single string.");
}

ConsoleGetType( TypeCaseString )
{
   return KorkApi::ConsoleValue::makeString(*((const char **)(dptr)));
}

#if TOFIX
//////////////////////////////////////////////////////////////////////////
// TypeFileName
//////////////////////////////////////////////////////////////////////////
ConsolePrepType( filename, TypeFilename, sizeof( const char* ), "" )

ConsoleSetType( TypeFilename )
{
   if(argc == 1)
   {
      char buffer[1024];
      if (Con::expandScriptFilename(buffer, 1024, argv[0]))//(Con::expandPath(buffer, 1024, argv[0]))
         *((const char **) dptr) = StringTable->insert(buffer);
      else
         Con::warnf("(TypeFilename) illegal filename detected: %s", argv[0]);
   }
   else
      Con::printf("(TypeFilename) Cannot set multiple args to a single filename.");
}

ConsoleGetType( TypeFilename )
{
   return *((const char **)(dptr));
}

ConsolePrepData( TypeFilename )
{
   if( Con::expandScriptFilename( buffer, bufferSize, data ) ) // if( Con::expandPath( buffer, bufferSize, data ) )
      return buffer;
   else
   {
      Con::warnf("(TypeFilename) illegal filename detected: %s", data);
      return data;
   }
}
#endif

//////////////////////////////////////////////////////////////////////////
// TypeS8
//////////////////////////////////////////////////////////////////////////
ConsoleType( char, TypeS8, sizeof(U8), "" )

ConsoleGetType( TypeS8 )
{
   KorkApi::ConsoleValue returnBufferV = Con::getReturnBuffer(256);
   char* returnBuffer = (char*)returnBufferV.evaluatePtr(vmPtr->getAllocBase());
   dSprintf(returnBuffer, 256, "%d", *((U8 *) dptr) );
   return returnBufferV;
}

ConsoleSetType( TypeS8 )
{
   if(argc == 1)
      *((U8 *) dptr) = vmPtr->valueAsInt(argv[0]);
   else
      Con::printf("(TypeU8) Cannot set multiple args to a single S32.");
}

//////////////////////////////////////////////////////////////////////////
// TypeS32
//////////////////////////////////////////////////////////////////////////
ConsoleType( int, TypeS32, sizeof(S32), "" )

ConsoleGetType( TypeS32 )
{
   KorkApi::ConsoleValue returnBufferV = Con::getReturnBuffer(256);
   char* returnBuffer = (char*)returnBufferV.evaluatePtr(vmPtr->getAllocBase());
   dSprintf(returnBuffer, 256, "%d", *((S32 *) dptr) );
   return returnBufferV;
}

ConsoleSetType( TypeS32 )
{
   if(argc == 1)
      *((S32 *) dptr) = vmPtr->valueAsInt(argv[0]);
   else
      Con::printf("(TypeS32) Cannot set multiple args to a single S32.");
}

#if TOFIX
//////////////////////////////////////////////////////////////////////////
// TypeS32Vector
//////////////////////////////////////////////////////////////////////////
ConsoleType( intList, TypeS32Vector, sizeof(Vector<S32>), "" )

ConsoleGetType( TypeS32Vector )
{
   Vector<S32> *vec = (Vector<S32> *)dptr;
   S32 buffSize = ( vec->size() * 15 ) + 16 ;
   char* returnBuffer = Con::getReturnBuffer( buffSize );
   S32 maxReturn = buffSize;
   returnBuffer[0] = '\0';
   S32 returnLeng = 0;
   for (Vector<S32>::iterator itr = vec->begin(); itr != vec->end(); itr++)
   {
      // concatenate the next value onto the return string
      dSprintf(returnBuffer + returnLeng, maxReturn - returnLeng, "%d ", *itr);
      // update the length of the return string (so far)
      returnLeng = dStrlen(returnBuffer);
   }
   // trim off that last extra space
   if (returnLeng > 0 && returnBuffer[returnLeng - 1] == ' ')
      returnBuffer[returnLeng - 1] = '\0';
   return returnBuffer;
}

ConsoleSetType( TypeS32Vector )
{
   Vector<S32> *vec = (Vector<S32> *)dptr;
   // we assume the vector should be cleared first (not just appending)
   vec->clear();
   if(argc == 1)
   {
      const char *values = argv[0];
      const char *endValues = values + dStrlen(values);
      S32 value;
      // advance through the string, pulling off S32's and advancing the pointer
      while (values < endValues && dSscanf(values, "%d", &value) != 0)
      {
         vec->push_back(value);
         const char *nextValues = dStrchr(values, ' ');
         if (nextValues != 0 && nextValues < endValues)
            values = nextValues + 1;
         else
            break;
      }
   }
   else if (argc > 1)
   {
      for (S32 i = 0; i < argc; i++)
         vec->push_back(dAtoi(argv[i]));
   }
   else
      Con::printf("Vector<S32> must be set as { a, b, c, ... } or \"a b c ...\"");
}

#endif

//////////////////////////////////////////////////////////////////////////
// TypeF32
//////////////////////////////////////////////////////////////////////////
ConsoleType( float, TypeF32, sizeof(F32), "" )

ConsoleGetType( TypeF32 )
{
   KorkApi::ConsoleValue returnBufferV = Con::getReturnBuffer(256);
   char* returnBuffer = (char*)returnBufferV.evaluatePtr(vmPtr->getAllocBase());
   dSprintf(returnBuffer, 256, "%.9g", *((F32 *) dptr) );
   return returnBufferV;
}
ConsoleSetType( TypeF32 )
{
   if(argc == 1)
      *((F32 *) dptr) = vmPtr->valueAsFloat(argv[0]);
   else
      Con::printf("(TypeF32) Cannot set multiple args to a single F32.");
}

#if TOFIX
//////////////////////////////////////////////////////////////////////////
// TypeF32Vector
//////////////////////////////////////////////////////////////////////////
ConsoleType( floatList, TypeF32Vector, sizeof(Vector<F32>), "" )

ConsoleGetType( TypeF32Vector )
{
   Vector<F32> *vec = (Vector<F32> *)dptr;
   S32 buffSize = ( vec->size() * 15 ) + 16 ;
   char* returnBuffer = Con::getReturnBuffer( buffSize );
   S32 maxReturn = buffSize;
   returnBuffer[0] = '\0';
   S32 returnLeng = 0;
   for (Vector<F32>::iterator itr = vec->begin(); itr != vec->end(); itr++)
   {
      // concatenate the next value onto the return string
      dSprintf(returnBuffer + returnLeng, maxReturn - returnLeng, "%g ", *itr);
      // update the length of the return string (so far)
      returnLeng = dStrlen(returnBuffer);
   }
   // trim off that last extra space
   if (returnLeng > 0 && returnBuffer[returnLeng - 1] == ' ')
      returnBuffer[returnLeng - 1] = '\0';
   return returnBuffer;
}

ConsoleSetType( TypeF32Vector )
{
   Vector<F32> *vec = (Vector<F32> *)dptr;
   // we assume the vector should be cleared first (not just appending)
   vec->clear();
   if(argc == 1)
   {
      const char *values = argv[0];
      const char *endValues = values + dStrlen(values);
      F32 value;
      // advance through the string, pulling off F32's and advancing the pointer
      while (values < endValues && dSscanf(values, "%.9g", &value) != 0)
      {
         vec->push_back(value);
         const char *nextValues = dStrchr(values, ' ');
         if (nextValues != 0 && nextValues < endValues)
            values = nextValues + 1;
         else
            break;
      }
   }
   else if (argc > 1)
   {
      for (S32 i = 0; i < argc; i++)
         vec->push_back(dAtof(argv[i]));
   }
   else
      Con::printf("Vector<F32> must be set as { a, b, c, ... } or \"a b c ...\"");
}

#endif

//////////////////////////////////////////////////////////////////////////
// TypeBool
//////////////////////////////////////////////////////////////////////////
ConsoleType( bool, TypeBool, sizeof(bool), "" )

ConsoleGetType( TypeBool )
{
   return KorkApi::ConsoleValue::makeInt(*((bool *) dptr) ? 1 : 0);
}

ConsoleSetType( TypeBool )
{
   if(argc == 1)
      *((bool *) dptr) = vmPtr->valueAsBool(argv[0]);
   else
      Con::printf("(TypeBool) Cannot set multiple args to a single bool.");
}

#if TOFIX

//////////////////////////////////////////////////////////////////////////
// TypeBoolVector
//////////////////////////////////////////////////////////////////////////
ConsoleType( boolList, TypeBoolVector, sizeof(Vector<bool>), "" )

ConsoleGetType( TypeBoolVector )
{
   Vector<bool> *vec = (Vector<bool>*)dptr;
   char* returnBuffer = Con::getReturnBuffer(1024);
   S32 maxReturn = 1024;
   returnBuffer[0] = '\0';
   S32 returnLeng = 0;
   for (Vector<bool>::iterator itr = vec->begin(); itr < vec->end(); itr++)
   {
      // concatenate the next value onto the return string
      dSprintf(returnBuffer + returnLeng, maxReturn - returnLeng, "%d ", (*itr == true ? 1 : 0));
      returnLeng = dStrlen(returnBuffer);
   }
   // trim off that last extra space
   if (returnLeng > 0 && returnBuffer[returnLeng - 1] == ' ')
      returnBuffer[returnLeng - 1] = '\0';
   return(returnBuffer);
}

ConsoleSetType( TypeBoolVector )
{
   Vector<bool> *vec = (Vector<bool>*)dptr;
   // we assume the vector should be cleared first (not just appending)
   vec->clear();
   if (argc == 1)
   {
      const char *values = argv[0];
      const char *endValues = values + dStrlen(values);
      S32 value;
      // advance through the string, pulling off bool's and advancing the pointer
      while (values < endValues && dSscanf(values, "%d", &value) != 0)
      {
         vec->push_back(value == 0 ? false : true);
         const char *nextValues = dStrchr(values, ' ');
         if (nextValues != 0 && nextValues < endValues)
            values = nextValues + 1;
         else
            break;
      }
   }
   else if (argc > 1)
   {
      for (S32 i = 0; i < argc; i++)
         vec->push_back(dAtob(argv[i]));
   }
   else
      Con::printf("Vector<bool> must be set as { a, b, c, ... } or \"a b c ...\"");
}
#endif

//////////////////////////////////////////////////////////////////////////
// TypeEnum
//////////////////////////////////////////////////////////////////////////
ConsoleType( enumval, TypeEnum, sizeof(S32), "" )

ConsoleGetType( TypeEnum )
{
   AssertFatal(tbl, "Null enum table passed to getDataTypeEnum()");
   S32 dptrVal = *(S32*)dptr;
   for (S32 i = 0; i < tbl->size; i++)
   {
      if (dptrVal == tbl->table[i].index)
      {
         return KorkApi::ConsoleValue::makeString(tbl->table[i].label);
      }
   }

   //not found
   return KorkApi::ConsoleValue::makeString("");
}

ConsoleSetType( TypeEnum )
{
   AssertFatal(tbl, "Null enum table passed to setDataTypeEnum()");
   if (argc != 1) return;

   S32 val = 0;
   const char* sval = (const char*)argv[0].evaluatePtr(vmPtr->getAllocBase());
   for (S32 i = 0; i < tbl->size; i++)
   {
      if (! dStricmp(sval, tbl->table[i].label))
      {
         val = tbl->table[i].index;
         break;
      }
   }
   *((S32 *) dptr) = val;
}

//////////////////////////////////////////////////////////////////////////
// TypeSimObjectPtr
//////////////////////////////////////////////////////////////////////////
ConsoleType( SimObjectPtr, TypeSimObjectPtr, sizeof(SimObject*), "" )

ConsoleSetType( TypeSimObjectPtr )
{
   if(argc == 1)
   {
      SimObject **obj = (SimObject **)dptr;
      *obj = Sim::findObject((const char*)argv[0].evaluatePtr(vmPtr->getAllocBase()));
   }
   else
      Con::printf("(TypeSimObjectPtr) Cannot set multiple args to a single S32.");
}

ConsoleGetType( TypeSimObjectPtr )
{
   SimObject **obj = (SimObject**)dptr;
   KorkApi::ConsoleValue returnBufferV = Con::getReturnBuffer(256);
   char* returnBuffer = (char*)returnBufferV.evaluatePtr(vmPtr->getAllocBase());
   const char* Id =  *obj ? (*obj)->getName() ? (*obj)->getName() : (*obj)->getIdString() : StringTable->EmptyString;
   dSprintf(returnBuffer, 256, "%s", Id);
   return returnBufferV;
}

//////////////////////////////////////////////////////////////////////////
// TypeSimObjectName
//////////////////////////////////////////////////////////////////////////
ConsoleType( SimObjectName, TypeSimObjectName, sizeof(SimObject*), "" )

ConsoleSetType( TypeSimObjectName )
{
   if(argc == 1)
   {
      SimObject **obj = (SimObject **)dptr;
      *obj = Sim::findObject((const char*)argv[0].evaluatePtr(vmPtr->getAllocBase()));
   }
   else
      Con::printf("(TypeSimObjectName) Cannot set multiple args to a single S32.");
}

ConsoleGetType( TypeSimObjectName )
{
   SimObject **obj = (SimObject**)dptr;
   KorkApi::ConsoleValue returnBufferV = Con::getReturnBuffer(256);
   char* returnBuffer = (char*)returnBufferV.evaluatePtr(vmPtr->getAllocBase());
   dSprintf(returnBuffer, 128, "%s", *obj && (*obj)->getName() ? (*obj)->getName() : "");
   return returnBufferV;
}


//////////////////////////////////////////////////////////////////////////
// TypeSimObjectId
//////////////////////////////////////////////////////////////////////////
ConsoleType( SimObjectId, TypeSimObjectId, sizeof(SimObject*), "" )

ConsoleSetType( TypeSimObjectId )
{
   if(argc == 1)
   {
      SimObject **obj = (SimObject **)dptr;
      *obj = Sim::findObject((const char*)argv[0].evaluatePtr(vmPtr->getAllocBase()));
   }
   else
      Con::printf("(TypeSimObjectId) Cannot set multiple args to a single S32.");
}

ConsoleGetType( TypeSimObjectId )
{
   SimObject **obj = (SimObject**)dptr;
   KorkApi::ConsoleValue returnBufferV = Con::getReturnBuffer(256);
   char* returnBuffer = (char*)returnBufferV.evaluatePtr(vmPtr->getAllocBase());
   dSprintf(returnBuffer, 128, "%s", *obj ? (*obj)->getIdString() : StringTable->EmptyString );
   return returnBufferV;
}

