#include "console/console.h"
#include "console/consoleTypes.h"
#include "core/stringTable.h"
#include "sim/simBase.h"
#include "core/stringUnit.h"

static inline const char* SkipSpaces(const char* p)
{
   while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
   return p;
}

// Types

ConsoleType( string, TypeString, sizeof(const char*), UINT_MAX, "" )
ConsoleType( stringList, TypeStringTableEntryVector, sizeof(Vector<StringTableEntry>), UINT_MAX, "" )
ConsoleType( caseString, TypeCaseString, sizeof(const char*), UINT_MAX, "" )

ConsoleType( char, TypeS8, sizeof(U8), sizeof(U8), "" )
ConsoleType( int, TypeS32, sizeof(S32), sizeof(S32), "" )
ConsoleType( float, TypeF32, sizeof(F32), sizeof(F32), "" )
ConsoleType( bool, TypeBool, sizeof(bool), sizeof(bool), "" )
ConsoleType( enumval, TypeEnum, sizeof(S32), sizeof(S32), "" )

ConsoleType( intList, TypeS32Vector, sizeof(Vector<S32>), UINT_MAX,"" )
ConsoleType( floatList, TypeF32Vector, sizeof(Vector<F32>), UINT_MAX, "" )
ConsoleType( boolList, TypeBoolVector, sizeof(Vector<bool>), UINT_MAX, "" )

#if 0
ConsoleType( SimObjectPtr, TypeSimObjectPtr, sizeof(SimObject*), UINT_MAX, "" )
ConsoleType( SimObjectName, TypeSimObjectName, sizeof(SimObject*), UINT_MAX, "" )
ConsoleType( SimObjectId, TypeSimObjectId, sizeof(SimObject*), sizeof(SimObjectId), "" )
ConsolePrepType( filename, TypeFilename, sizeof( const char* ), UINT_MAX, "" )
#endif

// Impls

ConsoleGetType( TypeString )
{
   const char* value = inputStorage->isField ? (const char *)(ConsoleGetInputStoragePtr()) : *((const char **)(ConsoleGetInputStoragePtr()));
   U32 len = dStrlen(value)+1;
   
   if (requestedType == KorkApi::ConsoleValue::TypeInternalString ||
       requestedType == TypeString)
   {
      outputStorage->FinalizeStorage(outputStorage, dStrlen(value)+1);
      memcpy(ConsoleGetOutputStoragePtr(), value, len);
      *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      return true;
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalNumber)
   {
      *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeNumber(std::atof(value));
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalUnsigned)
   {
      *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeUnsigned(std::atoll(value));
   }
   
   return false;
}

ConsoleSetType( TypeString )
{
   if (argc == 1)
   {
      if (!outputStorage->isField)
      {
         // NOTE: Assuming output is the correct size already here which should be the case
         // for non-relocatable values.
         StringTableEntry* value = (StringTableEntry*)ConsoleGetOutputStoragePtr();
         *value = StringTable->insert((const char*)argv[0].evaluatePtr(vmPtr->getAllocBase()));
      }
      else
      {
         // Variable size, uses storage API for resizing.
         const char* value = (const char*)(argv[0].evaluatePtr(vmPtr->getAllocBase()));
         U32 len = dStrlen(value)+1;
         outputStorage->FinalizeStorage(outputStorage, len);
         char* ptr = (char*)ConsoleGetOutputStoragePtr();
         memcpy(ptr, value, len);
         
         if (outputStorage->data.storageRegister)
         {
            *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
         }
      }
      
      return true;
   }
   else
   {
      Con::printf("(TypeString) Cannot set multiple args to a single string.");
      return false;
   }
}

ConsoleGetType( TypeStringTableEntryVector )
{
   if (!inputStorage->isField)
   {
      // TODO
      return false;
   }
   else
   {
      Vector<StringTableEntry> *vec = (Vector<StringTableEntry>*)ConsoleGetInputStoragePtr();
      outputStorage->ResizeStorage(outputStorage, 1024);
      char* returnBuffer = (char*)outputStorage->data.storageAddress.evaluatePtr(vmPtr->getAllocBase());
      
      if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
      {
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
         
         outputStorage->FinalizeStorage(outputStorage, returnLeng+1);
         
         if (outputStorage->data.storageRegister)
         {
            *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
         }
         
         return true;
      }
      else if (requestedType == KorkApi::ConsoleValue::TypeInternalNumber)
      {
         *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeNumber(vec->empty() ? 0 : std::atof(vec->first()));
      }
      else if (requestedType == KorkApi::ConsoleValue::TypeInternalUnsigned)
      {
         *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeUnsigned(vec->empty() ? 0 : std::atoll(vec->first()));
      }
   }
   
   return false;
}

ConsoleSetType( TypeStringTableEntryVector )
{
   if (outputStorage->isField)
   {
      // TODO: make vector, serialize
      return false;
   }
   else
   {
      Vector<StringTableEntry> *vec = (Vector<StringTableEntry>*)ConsoleGetOutputStoragePtr();
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
         
         return true;
      }
      else if (argc > 1)
      {
         for (S32 i = 0; i < argc; i++)
         {
            KorkApi::ConsoleValue argV = argv[i];
            vec->push_back( StringTable->insert( (const char*)argV.evaluatePtr(vmPtr->getAllocBase()) ) );
         }
         
         return true;
      }
      else
         Con::printf("Vector<String> must be set as { a, b, c, ... } or \"a,b,c, ...\"");
   }
   
   return false;
}

ConsoleSetType( TypeCaseString )
{
   if (argc == 1)
   {
      if (!outputStorage->isField)
      {
         // NOTE: Assuming output is the correct size already here which should be the case
         // for non-relocatable values.
         StringTableEntry* value = (StringTableEntry*)ConsoleGetOutputStoragePtr();
         *value = StringTable->insert((const char*)argv[0].evaluatePtr(vmPtr->getAllocBase()), true);
      }
      else
      {
         // Variable size, uses storage API for resizing.
         const char* value = (const char*)(argv[0].evaluatePtr(vmPtr->getAllocBase()));
         U32 len = dStrlen(value)+1;
         outputStorage->FinalizeStorage(outputStorage, len);
         char* ptr = (char*)ConsoleGetOutputStoragePtr();
         memcpy(ptr, value, len);
         
         if (outputStorage->data.storageRegister)
         {
            *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
         }
      }
      
      return true;
   }
   else
   {
      Con::printf("(TypeCaseString) Cannot set multiple args to a single string.");
      return false;
   }
}

ConsoleGetType( TypeCaseString )
{
   const char* value = inputStorage->isField ? (const char *)(ConsoleGetInputStoragePtr()) : *((const char **)(ConsoleGetInputStoragePtr()));
   U32 len = dStrlen(value)+1;
   
   if (requestedType == KorkApi::ConsoleValue::TypeInternalString ||
       requestedType == TypeString)
   {
      outputStorage->FinalizeStorage(outputStorage, dStrlen(value)+1);
      memcpy(ConsoleGetOutputStoragePtr(), value, len);
      *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      return true;
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalNumber)
   {
      *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeNumber(std::atof(value));
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalUnsigned)
   {
      *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeUnsigned(std::atoll(value));
   }
   
   return false;
}

#if 0
ConsoleSetType( TypeFilename )
{
   if (argc == 1)
   {
      char buffer[1024];
      const char* strArg = vmPtr->valueAsString(argv[0]);
      if (Con::expandScriptFilename(buffer, 1024, strArg))
         *((const char **) ConsoleGetStoragePtr()) = StringTable->insert(buffer);
      else
         Con::warnf("(TypeFilename) illegal filename detected: %s", argv[0]);
   }
   else
      Con::printf("(TypeFilename) Cannot set multiple args to a single filename.");
}

ConsoleGetType( TypeFilename )
{
   return KorkApi::ConsoleValue::makeString(*((const char **)(ConsoleGetStoragePtr())));
}

ConsolePrepData( TypeFilename )
{
   if ( Con::expandScriptFilename( buffer, bufferSize, data ) ) // if ( Con::expandPath( buffer, bufferSize, data ) )
      return buffer;
   else
   {
      Con::warnf("(TypeFilename) illegal filename detected: %s", data);
      return data;
   }
}


#endif

ConsoleGetType( TypeS8 )
{
   S8 value = *((S8*)(ConsoleGetInputStoragePtr()));
   
   if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      outputStorage->FinalizeStorage(outputStorage, 6);
      dSprintf((char*)ConsoleGetOutputStoragePtr(), 6, "%i", value);
      *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      return true;
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalNumber)
   {
      *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeNumber(value);
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalUnsigned)
   {
      *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeUnsigned((U64)value);
   }
}

ConsoleSetType( TypeS8 )
{
   if (argc == 1)
   {
      S8* value = (S8*)ConsoleGetOutputStoragePtr();
      *value = vmPtr->valueAsInt(argv[0]);
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeNumber(*value);
      }
      
      return true;
   }
   else
   {
      Con::printf("(TypeS8) Cannot set multiple args to a single S8.");
      return false;
   }
}

ConsoleGetType( TypeS32 )
{
   S32 value = *((S32*)(ConsoleGetInputStoragePtr()));
   
   if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      outputStorage->FinalizeStorage(outputStorage, 6);
      dSprintf((char*)ConsoleGetOutputStoragePtr(), 12, "%i", value);
      *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      return true;
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalNumber)
   {
      *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeNumber(value);
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalUnsigned)
   {
      *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeUnsigned((U64)value);
   }
}

ConsoleSetType( TypeS32 )
{
   if (argc == 1)
   {
      S32* value = (S32*)ConsoleGetOutputStoragePtr();
      *value = (S32)vmPtr->valueAsInt(argv[0]);
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeNumber(*value);
      }
      
      return true;
   }
   else
   {
      Con::printf("(TypeS32) Cannot set multiple args to a single S32.");
   }
}

ConsoleGetType( TypeF32 )
{
   F32 value = *((F32*)(ConsoleGetInputStoragePtr()));
   
   if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      outputStorage->FinalizeStorage(outputStorage, 64);
      dSprintf((char*)ConsoleGetOutputStoragePtr(), 64, "%.9g", value);
      *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      return true;
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalNumber)
   {
      *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeNumber(value);
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalUnsigned)
   {
      *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeUnsigned((U64)value);
   }
}
ConsoleSetType( TypeF32 )
{
   if (argc == 1)
      *((F32 *) ConsoleGetOutputStoragePtr()) = vmPtr->valueAsFloat(argv[0]);
   else
      Con::printf("(TypeF32) Cannot set multiple args to a single F32.");
}

ConsoleGetType( TypeBool )
{
   bool value = *((bool*)(ConsoleGetInputStoragePtr()));
   
   if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      outputStorage->FinalizeStorage(outputStorage, 6);
      dSprintf((char*)ConsoleGetOutputStoragePtr(), 6, "%i", value);
      *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      return true;
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalNumber)
   {
      *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeNumber(value);
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalUnsigned)
   {
      *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeUnsigned((U64)value);
   }
}

ConsoleSetType( TypeBool )
{
   if (argc == 1)
      *((bool *) ConsoleGetOutputStoragePtr()) = vmPtr->valueAsBool(argv[0]);
   else
      Con::printf("(TypeBool) Cannot set multiple args to a single bool.");
}


ConsoleGetType( TypeS32Vector )
{
   if (inputStorage->isField)
   {
      Vector<S32>* vec = (Vector<S32>*)ConsoleGetInputStoragePtr();

      if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
      {
         const U32 buffSize = (vec->size() * 15) + 16;
         outputStorage->FinalizeStorage(outputStorage, buffSize);

         char* out = (char*)ConsoleGetOutputStoragePtr();
         out[0] = '\0';

         S32 len = 0;
         for (Vector<S32>::iterator itr = vec->begin(); itr != vec->end(); ++itr)
         {
            dSprintf(out + len, buffSize - len, "%d ", *itr);
            len = dStrlen(out);
         }

         if (len > 0 && out[len - 1] == ' ')
            out[len - 1] = '\0';

         outputStorage->FinalizeStorage(outputStorage, dStrlen(out) + 1);

         if (outputStorage->data.storageRegister)
            *outputStorage->data.storageRegister = outputStorage->data.storageAddress;

         return true;
      }
      else if (requestedType == KorkApi::ConsoleValue::TypeInternalNumber)
      {
         if (outputStorage->data.storageRegister)
            *outputStorage->data.storageRegister =
               KorkApi::ConsoleValue::makeNumber(vec->empty() ? 0.0 : (F64)vec->first());
         return true;
      }
      else if (requestedType == KorkApi::ConsoleValue::TypeInternalUnsigned)
      {
         if (outputStorage->data.storageRegister)
            *outputStorage->data.storageRegister =
               KorkApi::ConsoleValue::makeUnsigned(vec->empty() ? 0 : (U64)vec->first());
         return true;
      }

      return false;
   }
   else
   {
      // TODO
      return false;
   }
}

ConsoleSetType( TypeS32Vector )
{
   if (outputStorage->isField)
   {
      Vector<S32>* vec = (Vector<S32>*)ConsoleGetOutputStoragePtr();
      vec->clear();

      if (argc == 1)
      {
         const char* values = vmPtr->valueAsString(argv[0]);
         if (!values) values = "";
         const char* p   = values;
         const char* end = values + dStrlen(values);

         while (p < end)
         {
            p = SkipSpaces(p);
            if (p >= end) break;

            S32 v = 0;
            if (dSscanf(p, "%d", &v) == 0)
               break;

            vec->push_back(v);

            // advance to next delimiter (space separated)
            const char* next = dStrchr(p, ' ');
            if (!next || next >= end) break;
            p = next + 1;
         }

         return true;
      }
      else if (argc > 1)
      {
         for (S32 i = 0; i < argc; ++i)
            vec->push_back(vmPtr->valueAsInt(argv[i]));
         return true;
      }

      Con::printf("Vector<S32> must be set as { a, b, c } or \"a b c\"");
      return false;
   }
   else
   {
      // TODO
      return false;
   }
}


ConsoleGetType( TypeF32Vector )
{
   if (inputStorage->isField)
   {
      Vector<F32>* vec = (Vector<F32>*)ConsoleGetInputStoragePtr();

      if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
      {
         const U32 buffSize = (vec->size() * 15) + 16;
         outputStorage->FinalizeStorage(outputStorage, buffSize);

         char* out = (char*)ConsoleGetOutputStoragePtr();
         out[0] = '\0';

         S32 len = 0;
         for (Vector<F32>::iterator itr = vec->begin(); itr != vec->end(); ++itr)
         {
            dSprintf(out + len, buffSize - len, "%g ", *itr);
            len = dStrlen(out);
         }

         if (len > 0 && out[len - 1] == ' ')
            out[len - 1] = '\0';

         outputStorage->FinalizeStorage(outputStorage, dStrlen(out) + 1);

         if (outputStorage->data.storageRegister)
            *outputStorage->data.storageRegister = outputStorage->data.storageAddress;

         return true;
      }
      else if (requestedType == KorkApi::ConsoleValue::TypeInternalNumber)
      {
         if (outputStorage->data.storageRegister)
            *outputStorage->data.storageRegister =
               KorkApi::ConsoleValue::makeNumber(vec->empty() ? 0.0 : (F64)vec->first());
         return true;
      }

      return false;
   }
   else
   {
      // TODO
      return false;
   }
}

ConsoleSetType( TypeF32Vector )
{
   if (outputStorage->isField)
   {
      Vector<F32>* vec = (Vector<F32>*)ConsoleGetOutputStoragePtr();
      vec->clear();
      
      if (argc == 1)
      {
         const char* values = vmPtr->valueAsString(argv[0]);
         if (!values) values = "";
         const char* p   = values;
         const char* end = values + dStrlen(values);
         
         while (p < end)
         {
            p = SkipSpaces(p);
            if (p >= end) break;
            
            S32 v = 0;
            if (dSscanf(p, "%g", &v) == 0)
               break;
            
            vec->push_back(v);
            
            // advance to next delimiter (space separated)
            const char* next = dStrchr(p, ' ');
            if (!next || next >= end) break;
            p = next + 1;
         }
         
         return true;
      }
      else if (argc > 1)
      {
         for (S32 i = 0; i < argc; ++i)
            vec->push_back(vmPtr->valueAsInt(argv[i]));
         return true;
      }
      
      Con::printf("Vector<F32> must be set as { a, b, c } or \"a b c\"");
      return false;
   }
   else
   {
      // TODO
      return false;
   }
}

ConsoleGetType( TypeBoolVector )
{
   if (inputStorage->isField)
   {
      Vector<bool>* vec = (Vector<bool>*)ConsoleGetInputStoragePtr();

      if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
      {
         const U32 buffSize = (vec->size() * 3) + 16;
         outputStorage->FinalizeStorage(outputStorage, buffSize);

         char* out = (char*)ConsoleGetOutputStoragePtr();
         out[0] = '\0';

         S32 len = 0;
         for (Vector<bool>::iterator itr = vec->begin(); itr != vec->end(); ++itr)
         {
            dSprintf(out + len, buffSize - len, "%d ", (*itr ? 1 : 0));
            len = dStrlen(out);
         }

         if (len > 0 && out[len - 1] == ' ')
            out[len - 1] = '\0';

         outputStorage->FinalizeStorage(outputStorage, dStrlen(out) + 1);

         if (outputStorage->data.storageRegister)
            *outputStorage->data.storageRegister = outputStorage->data.storageAddress;

         return true;
      }
      else if (requestedType == KorkApi::ConsoleValue::TypeInternalUnsigned)
      {
         if (outputStorage->data.storageRegister)
            *outputStorage->data.storageRegister =
               KorkApi::ConsoleValue::makeUnsigned(vec->empty() ? 0 : (vec->first() ? 1 : 0));
         return true;
      }
      else if (requestedType == KorkApi::ConsoleValue::TypeInternalNumber)
      {
         if (outputStorage->data.storageRegister)
            *outputStorage->data.storageRegister =
               KorkApi::ConsoleValue::makeNumber(vec->empty() ? 0.0 : (vec->first() ? 1.0 : 0.0));
         return true;
      }

      return false;
   }
   else
   {
      // TODO
      return false;
   }
}

ConsoleSetType( TypeBoolVector )
{
   if (outputStorage->isField)
   {
      Vector<bool>* vec = (Vector<bool>*)ConsoleGetOutputStoragePtr();
      vec->clear();
      
      if (argc == 1)
      {
         const char* values = vmPtr->valueAsString(argv[0]);
         const char* end    = values + dStrlen(values);
         
         S32 v;
         while (values < end && dSscanf(values, "%d", &v) != 0)
         {
            vec->push_back(v != 0);
            const char* next = dStrchr(values, ' ');
            if (!next || next >= end)
               break;
            values = next + 1;
         }
         return true;
      }
      else if (argc > 1)
      {
         for (S32 i = 0; i < argc; ++i)
            vec->push_back(vmPtr->valueAsBool(argv[i]));
         return true;
      }
      
      Con::printf("Vector<bool> must be set as { a, b, c } or \"a b c\"");
      return false;
   }
   else
   {
      // TODO
      return false;
   }
}

ConsoleGetType( TypeEnum )
{
   AssertFatal(tbl, "invalid table");
   
   S32 value = *((S32*)(ConsoleGetInputStoragePtr()));
   
   if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      for (S32 i = 0; i < tbl->size; i++)
      {
         if (value == tbl->table[i].index)
         {
            U32 len = dStrlen(tbl->table[i].label)+1;
            outputStorage->FinalizeStorage(outputStorage, len);
            memcpy(ConsoleGetOutputStoragePtr(), tbl->table[i].label, len);
            break;
         }
      }
      
      *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      return true;
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalNumber)
   {
      *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeNumber(value);
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalUnsigned)
   {
      *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeUnsigned((U64)value);
   }
   else
   {
      return false;
   }
   
   return true;
}

ConsoleSetType( TypeEnum )
{
   AssertFatal(tbl, "invalid table");
   if (argc != 1) return false;
   
   S32 val = 0;
   
   if (argv[0].isUnsigned() ||
       argv[0].isFloat())
   {
      val = (S32)vmPtr->valueAsInt(argv[0]);
   }
   else
   {
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
   }
   
   *((S32 *) ConsoleGetOutputStoragePtr()) = val;
   
   if (outputStorage->data.storageRegister)
   {
      *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeNumber(val);
   }
}

#if 0

ConsoleSetType( TypeSimObjectPtr )
{
   if (argc == 1)
   {
      SimObject **obj = (SimObject **)ConsoleGetStoragePtr();
      *obj = Sim::findObject((const char*)argv[0].evaluatePtr(vmPtr->getAllocBase()));
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeUnsigned(*obj ? (*obj)->getId() : 0);
      }
   }
   else
   {
      Con::printf("(TypeSimObjectPtr) Cannot set multiple args to a single S32.");
   }
}

ConsoleGetType( TypeSimObjectPtr )
{
   SimObject* obj = NULL;
   
   if (inputStorage->isField)
   {
      obj = *((SimObject**)(ConsoleGetInputStoragePtr()));
   }
   else
   {
      obj = Sim::findObject((const char*)ConsoleGetInputStoragePtr()));
   }
   
   if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      outputStorage->FinalizeStorage(outputStorage, 64);
      dSprintf((char*)ConsoleGetOutputStoragePtr(), 64, "%s", value);
      *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      return true;
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalNumber)
   {
      *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeNumber(value);
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalUnsigned)
   {
      *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeUnsigned((U64)value);
   }
   
   
   SimObject **obj = (SimObject**)ConsoleGetStoragePtr();
   KorkApi::ConsoleValue returnBufferV = Con::getReturnBuffer(256);
   char* returnBuffer = (char*)returnBufferV.evaluatePtr(vmPtr->getAllocBase());
   const char* Id =  *obj ? (*obj)->getName() ? (*obj)->getName() : (*obj)->getIdString() : StringTable->EmptyString;
   dSprintf(returnBuffer, 256, "%s", Id);
   return returnBufferV;
}

ConsoleSetType( TypeSimObjectName )
{
   if (argc == 1)
   {
      SimObject **obj = (SimObject **)ConsoleGetStoragePtr();
      *obj = Sim::findObject((const char*)argv[0].evaluatePtr(vmPtr->getAllocBase()));
   }
   else
      Con::printf("(TypeSimObjectName) Cannot set multiple args to a single S32.");
}

ConsoleGetType( TypeSimObjectName )
{
   SimObject **obj = (SimObject**)ConsoleGetStoragePtr();
   KorkApi::ConsoleValue returnBufferV = Con::getReturnBuffer(256);
   char* returnBuffer = (char*)returnBufferV.evaluatePtr(vmPtr->getAllocBase());
   dSprintf(returnBuffer, 128, "%s", *obj && (*obj)->getName() ? (*obj)->getName() : "");
   return returnBufferV;
}


ConsoleSetType( TypeSimObjectId )
{
   if (argc == 1)
   {
      SimObject **obj = (SimObject **)ConsoleGetStoragePtr();
      *obj = Sim::findObject((const char*)argv[0].evaluatePtr(vmPtr->getAllocBase()));
   }
   else
   {
      Con::printf("(TypeSimObjectId) Cannot set multiple args to a single S32.");
   }
}

ConsoleGetType( TypeSimObjectId )
{
   SimObject **obj = (SimObject**)ConsoleGetStoragePtr();
   KorkApi::ConsoleValue returnBufferV = Con::getReturnBuffer(256);
   char* returnBuffer = (char*)returnBufferV.evaluatePtr(vmPtr->getAllocBase());
   dSprintf(returnBuffer, 128, "%s", *obj ? (*obj)->getIdString() : StringTable->EmptyString );
   return returnBufferV;
}

#endif
