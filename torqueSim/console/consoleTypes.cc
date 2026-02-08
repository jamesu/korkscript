#include "console/console.h"
#include "console/consoleTypes.h"
#include "platform/platformString.h"
#include "sim/simBase.h"
#include "core/stringUnit.h"

static inline const char* SkipSpaces(const char* p)
{
   while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
   return p;
}

// Types

ConsoleType( string, TypeString, sizeof(const char*), UINT_MAX, "" )
ConsoleType( stringList, TypeStringTableEntryVector, sizeof(std::vector<StringTableEntry>), UINT_MAX, "" )
ConsoleType( caseString, TypeCaseString, sizeof(const char*), UINT_MAX, "" )

ConsoleType( char, TypeS8, sizeof(U8), sizeof(U8), "" )
ConsoleType( int, TypeS32, sizeof(S32), sizeof(S32), "" )
ConsoleType( float, TypeF32, sizeof(F32), sizeof(F32), "" )
ConsoleType( bool, TypeBool, sizeof(bool), sizeof(bool), "" )
ConsoleType( enumval, TypeEnum, sizeof(S32), sizeof(S32), "" )

ConsoleType( intList, TypeS32Vector, sizeof(std::vector<S32>), UINT_MAX,"" )
ConsoleType( floatList, TypeF32Vector, sizeof(std::vector<F32>), UINT_MAX, "" )
ConsoleType( boolList, TypeBoolVector, sizeof(std::vector<bool>), UINT_MAX, "" )

ConsoleType( SimObjectPtr, TypeSimObjectPtr, sizeof(SimObject*), UINT_MAX, "" )

#if 0
ConsoleType( SimObjectName, TypeSimObjectName, sizeof(SimObject*), UINT_MAX, "" )
ConsoleType( SimObjectId, TypeSimObjectId, sizeof(SimObject*), sizeof(SimObjectId), "" )
ConsolePrepType( filename, TypeFilename, sizeof( const char* ), UINT_MAX, "" )
#endif

// TODO: should be part of API
namespace KorkApi
{
   KorkApi::TypeStorageInterface CreateRegisterStorageFromArgs(KorkApi::VmInternal* vmInternal, U32 argc, KorkApi::ConsoleValue* argv);
}

// Impls

ConsoleGetType( TypeString )
{
   const KorkApi::ConsoleValue* argv = nullptr;
   U32 argc = inputStorage ? inputStorage->data.argc : 0;
   bool isPTR = false;

   if (argc > 0 && 
      inputStorage->data.storageRegister)
   {
      argv = inputStorage->data.storageRegister;
   }
   else
   {
      argc = 1;
      argv = &inputStorage->data.storageAddress;
      isPTR = true;
   }

   // TypeString can't take tuples.
   if (argc != 1)
   {
      Con::printf("(TypeString) Cannot set multiple args to a single string.");
      return false;
   }

   // Evaluate input to C string
   const char* value = isPTR ? *((const char**)ConsoleGetInputStoragePtr()) : (const char*)vmPtr->valueAsString(argv[0]);
   if (!value)
      value = "";

    // Handle cast to specific output type (may require specialization)
    if (requestedType != TypeString &&
      requestedType != KorkApi::ConsoleValue::TypeInternalString)
    {
      KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeString(value);
      KorkApi::TypeStorageInterface castInput = KorkApi::CreateRegisterStorageFromArgs(vmPtr->mInternal, 1, &cv);
      return vmPtr->castValue(requestedType, &castInput, outputStorage, nullptr, 0);
    }
   
   // Now all we need to do is deal with whether the output is a field or not

   if (outputStorage->isField)
   {
      StringTableEntry* dst = (StringTableEntry*)ConsoleGetOutputStoragePtr();
      *dst = vmPtr->internString(value);
   }
   else
   {
      // Value output: variable size string stored in relocatable backing store (heap/STR etc.)
      U32 len = dStrlen(value) + 1;

      outputStorage->FinalizeStorage(outputStorage, len);

      char* dst = (char*)ConsoleGetOutputStoragePtr();
      if (!dst)
         return false;

      memcpy(dst, vmPtr->internString(value), len);

      // If a storage register exists, mirror the storageAddress into it (as you did before).
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      }
   }

   return true;
}

ConsoleTypeOpDefaultNumeric( TypeString )

ConsoleGetType( TypeStringTableEntryVector )
{
   std::vector<StringTableEntry> *vec = nullptr;
   static std::vector<StringTableEntry> workVec;
   
   if (!inputStorage->isField)
   {
      if ((!outputStorage->isField &&
          (requestedType == TypeStringTableEntryVector ||
          requestedType == KorkApi::ConsoleValue::TypeInternalString)) &&
          inputStorage->data.argc == 1)
      {
         // Just copy string
         const char* ptr = (const char*)ConsoleGetInputStoragePtr();
         U32 len = dStrlen(ptr)+1;
         outputStorage->ResizeStorage(outputStorage, len);
         char* returnBuffer = (char*)outputStorage->data.storageAddress.evaluatePtr(vmPtr->getAllocBase());
         memcpy(returnBuffer, ptr, len);
         
         if (outputStorage->data.storageRegister)
         {
            *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
         }
      
         return true;
      }
      else
      {
         // Need to take long path
         vec = &workVec;
         vec->clear();
         
         if (inputStorage->data.argc > 1)
         {
            for (U32 i=0; i<inputStorage->data.argc; i++)
            {
               vec->push_back( vmPtr->internString( vmPtr->valueAsString(inputStorage->data.storageRegister[i] ) ) );
            }
         }
         else if (inputStorage->data.argc > 0)
         {
            const char* arg = vmPtr->valueAsString(inputStorage->data.storageRegister[0]);
            const U32 unitCount = StringUnit::getUnitCount(arg, ",");
            for( U32 unitIndex = 0; unitIndex < unitCount; ++unitIndex )
            {
               vec->push_back( vmPtr->internString( StringUnit::getUnit(arg, unitIndex, ",") ) );
            }
         }
      }
   }
   else
   {
      vec = (std::vector<StringTableEntry>*)ConsoleGetInputStoragePtr();
   }
   
   // Set conservative output size
   
   // Input should now be a vector; we need to convert it to the output

   if (outputStorage->isField &&
       requestedType == TypeStringTableEntryVector)
   {
      // In this case we are setting output to this type so just copy the vector
      // NOTE: we will NEVER get outputStorage->isField == true where the type isn't the native type
      std::vector<StringTableEntry> *outputVec = (std::vector<StringTableEntry>*)ConsoleGetOutputStoragePtr();
      *outputVec = *vec;
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalString ||
       requestedType == TypeStringTableEntryVector)
   {
      S32 maxReturn = 1024;
      outputStorage->ResizeStorage(outputStorage, maxReturn);
      char* returnBuffer = (char*)outputStorage->data.storageAddress.evaluatePtr(vmPtr->getAllocBase());
      returnBuffer[0] = '\0';
      S32 returnLeng = 0;
      
      for (std::vector<StringTableEntry>::iterator itr = vec->begin(); itr < vec->end(); itr++)
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
   else
   {
      static std::vector<KorkApi::ConsoleValue> tmpArgv;
      tmpArgv.resize(inputStorage->data.argc);
      
      for (U32 i=0; i<inputStorage->data.argc; i++)
      {
         tmpArgv[i] = KorkApi::ConsoleValue::makeString(vec->operator[](i));
      }
      KorkApi::TypeStorageInterface castInput = KorkApi::CreateRegisterStorageFromArgs(vmPtr->mInternal, inputStorage->data.argc, &tmpArgv[0]);
      return vmPtr->castValue(requestedType, &castInput, outputStorage, nullptr, 0);
   }
   
   return false;
}

ConsoleTypeOpDefaultNumeric( TypeStringTableEntryVector )

ConsoleGetType( TypeCaseString )
{
   const KorkApi::ConsoleValue* argv = nullptr;
   U32 argc = inputStorage ? inputStorage->data.argc : 0;
   bool isPTR = false;

   if (argc > 0 &&
      inputStorage->data.storageRegister)
   {
      argv = inputStorage->data.storageRegister;
   }
   else
   {
      argc = 1;
      argv = &inputStorage->data.storageAddress;
      isPTR = true;
   }

   // TypeString can't take tuples.
   if (argc != 1)
   {
      Con::printf("(TypeCaseString) Cannot set multiple args to a single string.");
      return false;
   }

   // Evaluate input to C string
   const char* value = isPTR ? *((const char**)ConsoleGetInputStoragePtr()) : (const char*)vmPtr->valueAsString(argv[0]);
   if (!value)
      value = "";

    // Handle cast to specific output type (may require specialization)
    if (requestedType != TypeString ||
      requestedType != KorkApi::ConsoleValue::TypeInternalString)
    {
      KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeString(value);
      KorkApi::TypeStorageInterface castInput = KorkApi::CreateRegisterStorageFromArgs(vmPtr->mInternal, 1, &cv);
      return vmPtr->castValue(requestedType, &castInput, outputStorage, nullptr, 0);
    }
   
   // Now all we need to do is deal with whether the output is a field or not

   if (!outputStorage->isField)
   {
      StringTableEntry* dst = (StringTableEntry*)ConsoleGetOutputStoragePtr();
      *dst = vmPtr->internString(value, true);
   }
   else
   {
      // Field output: variable size string stored in relocatable backing store (heap/STR etc.)
      U32 len = dStrlen(value) + 1;

      outputStorage->FinalizeStorage(outputStorage, len);

      char* dst = (char*)ConsoleGetOutputStoragePtr();
      if (!dst)
         return false;

      memcpy(dst, value, len);

      // If a storage register exists, mirror the storageAddress into it (as you did before).
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      }
   }

   return true;
}

ConsoleTypeOpDefaultNumeric( TypeCaseString )

#if 0

ConsoleSetType( TypeFilename )
{
   if (argc == 1)
   {
      char buffer[1024];
      const char* strArg = vmPtr->valueAsString(argv[0]);
      if (Con::expandScriptFilename(buffer, 1024, strArg))
         *((const char **) ConsoleGetStoragePtr()) = vmPtr->internString(buffer);
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
   S8 value = inputStorage->isField ? *((S32*)(ConsoleGetInputStoragePtr())) : vmPtr->valueAsInt(inputStorage->data.storageRegister[0]);
   
   if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      outputStorage->FinalizeStorage(outputStorage, 6);
      dSprintf((char*)ConsoleGetOutputStoragePtr(), 6, "%i", value);
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      }
      
      return true;
   }
   else if (requestedType == TypeS8)
   {
      if (outputStorage->isField)
      {
         S8* dst = (S8*)ConsoleGetOutputStoragePtr();
         *dst = value;
      }
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeNumber(value);
      }
      
      return true;
   }
   else
   {
      KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeNumber(value);
      KorkApi::TypeStorageInterface castInput =
         KorkApi::CreateRegisterStorageFromArgs(vmPtr->mInternal, 1, &cv);

      return vmPtr->castValue(requestedType, &castInput, outputStorage, nullptr, 0);
   }
}

ConsoleTypeOpDefaultNumeric( TypeS8 )

ConsoleGetType( TypeS32 )
{
   S32 value = inputStorage->isField ? *((S32*)(ConsoleGetInputStoragePtr())) : vmPtr->valueAsInt(inputStorage->data.storageRegister[0]);
   
   if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      outputStorage->FinalizeStorage(outputStorage, 6);
      dSprintf((char*)ConsoleGetOutputStoragePtr(), 6, "%i", value);
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      }
      
      return true;
   }
   else if (requestedType == TypeS32)
   {
      if (outputStorage->isField)
      {
         S32* dst = (S32*)ConsoleGetOutputStoragePtr();
         *dst = value;
      }
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeNumber(value);
      }
      
      return true;
   }
   else
   {
      KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeNumber(value);
      KorkApi::TypeStorageInterface castInput =
         KorkApi::CreateRegisterStorageFromArgs(vmPtr->mInternal, 1, &cv);

      return vmPtr->castValue(requestedType, &castInput, outputStorage, nullptr, 0);
   }
}

ConsoleTypeOpDefaultNumeric( TypeS32 )

ConsoleGetType( TypeF32 )
{
   F32 value = inputStorage->isField ? *((F32*)(ConsoleGetInputStoragePtr())) : vmPtr->valueAsFloat(inputStorage->data.storageRegister[0]);
   
   if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      outputStorage->FinalizeStorage(outputStorage, 6);
      dSprintf((char*)ConsoleGetOutputStoragePtr(), 6, "%.9g", value);
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      }
      
      return true;
   }
   else if (requestedType == TypeF32)
   {
      if (outputStorage->isField)
      {
         F32* dst = (F32*)ConsoleGetOutputStoragePtr();
         *dst = value;
      }
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeNumber(value);
      }
      
      return true;
   }
   else
   {
      KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeNumber(value);
      KorkApi::TypeStorageInterface castInput =
         KorkApi::CreateRegisterStorageFromArgs(vmPtr->mInternal, 1, &cv);

      return vmPtr->castValue(requestedType, &castInput, outputStorage, nullptr, 0);
   }
}

ConsoleTypeOpDefaultNumeric( TypeF32 )

ConsoleGetType( TypeBool )
{
   bool value = inputStorage->isField ? *((bool*)(ConsoleGetInputStoragePtr())) : vmPtr->valueAsBool(inputStorage->data.storageRegister[0]);
   
   if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      outputStorage->FinalizeStorage(outputStorage, 6);
      dSprintf((char*)ConsoleGetOutputStoragePtr(), 6, "%i", value);
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      }
      
      return true;
   }
   else if (requestedType == TypeBool)
   {
      if (outputStorage->isField)
      {
         bool* dst = (bool*)ConsoleGetOutputStoragePtr();
         *dst = value;
      }
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeUnsigned(value);
      }
      
      return true;
   }
   else
   {
      KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeNumber(value);
      KorkApi::TypeStorageInterface castInput =
         KorkApi::CreateRegisterStorageFromArgs(vmPtr->mInternal, 1, &cv);

      return vmPtr->castValue(requestedType, &castInput, outputStorage, nullptr, 0);
   }
}


ConsoleTypeOpDefaultUnsigned( TypeBool )

ConsoleGetType( TypeS32Vector )
{
   std::vector<S32> *vec = nullptr;
   static std::vector<S32> workVec;
   
   if (!inputStorage->isField)
   {
      if (!outputStorage->isField &&
          (requestedType == TypeS32Vector) &&
          inputStorage->data.argc == 1 &&
          inputStorage->data.storageRegister->typeId == TypeS32Vector)
      {
         // Just copy data
         U32* ptr = (U32*)ConsoleGetInputStoragePtr();
         U32 numElements = ptr ? *ptr++ : 0;
         U32 dataSize = (numElements * sizeof(S32));
         outputStorage->FinalizeStorage(outputStorage, sizeof(U32) +  dataSize);
         U32* returnBuffer = (U32*)ConsoleGetOutputStoragePtr();
         *returnBuffer++ = numElements;
         
         if (ptr)
         {
            memcpy(returnBuffer, ptr, dataSize);
         }
         
         if (outputStorage->data.storageRegister)
         {
            *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
         }
      
         return true;
      }
      else
      {
         // Need to take long path
         vec = &workVec;
         vec->clear();
         
         if (inputStorage->data.argc > 1)
         {
            for (U32 i=0; i<inputStorage->data.argc; i++)
            {
               vec->push_back( (S32)vmPtr->valueAsFloat(inputStorage->data.storageRegister[i]) );
            }
         }
         else if (inputStorage->data.argc > 0)
         {
            if (inputStorage->data.storageRegister->typeId == TypeS32Vector)
            {
               U32 numElems = ((U32*)ConsoleGetInputStoragePtr())[0];
               S32* data = ((S32*)ConsoleGetInputStoragePtr())+1;
               
               for (S32 i=0; i<numElems; i++)
               {
                  vec->push_back(data[i]);
               }
            }
            else
            {
               const char* values = vmPtr->valueAsString(inputStorage->data.storageRegister[0]);
               if (!values) values = "";
               const char* p   = values;
               const char* end = values + dStrlen(values);
               
               while (p < end)
               {
                  p = SkipSpaces(p);
                  if (p >= end) break;
                  
                  S32 v = 0;
                  if (dSscanf(p, "%i", &v) == 0)
                     break;
                  
                  vec->push_back(v);
                  
                  // advance to next delimiter (space separated)
                  const char* next = dStrchr(p, ' ');
                  if (!next || next >= end) break;
                  p = next + 1;
               }
            }
         }
      }
      
   }
   else
   {
      vec = (std::vector<S32>*)ConsoleGetInputStoragePtr();
   }
   
   // Set conservative output size
   
   // Input should now be a vector; we need to convert it to the output

   if (outputStorage->isField &&
       requestedType == TypeS32Vector)
   {
      // In this case we are setting output to this type so just copy the vector
      // NOTE: we will NEVER get outputStorage->isField == true where the type isn't the native type
      std::vector<S32> *outputVec = (std::vector<S32>*)ConsoleGetOutputStoragePtr();
      *outputVec = *vec;
   }
   else if (!outputStorage->isField &&
            requestedType == TypeS32Vector)
   {
      outputStorage->FinalizeStorage(outputStorage, (vec->size() * sizeof(S32)) + sizeof(U32));
      U32* vecCount = (U32*)ConsoleGetOutputStoragePtr();
      *vecCount++ = vec->size();
      
      std::copy(vec->begin(), vec->end(), (S32*)vecCount);
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      }
      
      return true;
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      S32 maxReturn = 1024;
      outputStorage->ResizeStorage(outputStorage, maxReturn);
      char* returnBuffer = (char*)outputStorage->data.storageAddress.evaluatePtr(vmPtr->getAllocBase());
      returnBuffer[0] = '\0';
      S32 returnLeng = 0;
      
      for (std::vector<S32>::iterator itr = vec->begin(); itr < vec->end(); itr++)
      {
         // concatenate the next value onto the return string
         if ( itr == vec->begin() )
         {
            dSprintf(returnBuffer + returnLeng, maxReturn - returnLeng, "%i", *itr);
         }
         else
         {
            dSprintf(returnBuffer + returnLeng, maxReturn - returnLeng, " %i", *itr);
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
   else
   {
      static std::vector<KorkApi::ConsoleValue> tmpArgv;
      tmpArgv.resize(inputStorage->data.argc);
      
      for (U32 i=0; i<inputStorage->data.argc; i++)
      {
         tmpArgv[i] = KorkApi::ConsoleValue::makeNumber(vec->operator[](i));
      }
      KorkApi::TypeStorageInterface castInput = KorkApi::CreateRegisterStorageFromArgs(vmPtr->mInternal, inputStorage->data.argc, &tmpArgv[0]);
      return vmPtr->castValue(requestedType, &castInput, outputStorage, nullptr, 0);
   }
   
   return false;
}

ConsoleTypeOpDefaultNumeric( TypeS32Vector )

ConsoleGetType( TypeF32Vector )
{
   std::vector<F32> *vec = nullptr;
   static std::vector<F32> workVec;
   
   if (!inputStorage->isField)
   {
      if (!outputStorage->isField &&
          (requestedType == TypeF32Vector) &&
          inputStorage->data.argc == 1 &&
          inputStorage->data.storageRegister->typeId == TypeF32Vector)
      {
         // Just copy data
         U32* ptr = (U32*)ConsoleGetInputStoragePtr();
         U32 numElements = ptr ? *ptr++ : 0;
         U32 dataSize = (numElements * sizeof(F32));
         outputStorage->FinalizeStorage(outputStorage, sizeof(U32) +  dataSize);
         U32* returnBuffer = (U32*)ConsoleGetOutputStoragePtr();
         *returnBuffer++ = numElements;
         
         if (ptr)
         {
            memcpy(returnBuffer, ptr, dataSize);
         }
         
         if (outputStorage->data.storageRegister)
         {
            *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
         }
      
         return true;
      }
      else
      {
         // Need to take long path
         vec = &workVec;
         vec->clear();
         
         if (inputStorage->data.argc > 1)
         {
            for (U32 i=0; i<inputStorage->data.argc; i++)
            {
               vec->push_back( (F32)vmPtr->valueAsFloat(inputStorage->data.storageRegister[i]) );
            }
         }
         else if (inputStorage->data.argc > 0)
         {
            if (inputStorage->data.storageRegister->typeId == TypeF32Vector)
            {
               U32 numElems = ((U32*)ConsoleGetInputStoragePtr())[0];
               F32* data = ((F32*)ConsoleGetInputStoragePtr())+1;
               
               for (S32 i=0; i<numElems; i++)
               {
                  vec->push_back(data[i]);
               }
            }
            else
            {
               const char* values = vmPtr->valueAsString(inputStorage->data.storageRegister[0]);
               if (!values) values = "";
               const char* p   = values;
               const char* end = values + dStrlen(values);
               
               while (p < end)
               {
                  p = SkipSpaces(p);
                  if (p >= end) break;
                  
                  F32 v = 0;
                  if (dSscanf(p, "%f", &v) == 0)
                     break;
                  
                  vec->push_back(v);
                  
                  // advance to next delimiter (space separated)
                  const char* next = dStrchr(p, ' ');
                  if (!next || next >= end) break;
                  p = next + 1;
               }
            }
         }
      }
      
   }
   else
   {
      vec = (std::vector<F32>*)ConsoleGetInputStoragePtr();
   }
   
   // Set conservative output size
   
   // Input should now be a vector; we need to convert it to the output

   if (outputStorage->isField &&
       requestedType == TypeF32Vector)
   {
      // In this case we are setting output to this type so just copy the vector
      // NOTE: we will NEVER get outputStorage->isField == true where the type isn't the native type
      std::vector<F32> *outputVec = (std::vector<F32>*)ConsoleGetOutputStoragePtr();
      *outputVec = *vec;
   }
   else if (!outputStorage->isField &&
            requestedType == TypeF32Vector)
   {
      outputStorage->FinalizeStorage(outputStorage, (vec->size() * sizeof(F32)) + sizeof(U32));
      U32* vecCount = (U32*)ConsoleGetOutputStoragePtr();
      *vecCount++ = vec->size();
      std::copy(vec->begin(), vec->end(), (F32*)vecCount);
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      }
      
      return true;
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      S32 maxReturn = 1024;
      outputStorage->ResizeStorage(outputStorage, maxReturn);
      char* returnBuffer = (char*)outputStorage->data.storageAddress.evaluatePtr(vmPtr->getAllocBase());
      returnBuffer[0] = '\0';
      S32 returnLeng = 0;
      
      for (std::vector<F32>::iterator itr = vec->begin(); itr < vec->end(); itr++)
      {
         // concatenate the next value onto the return string
         if ( itr == vec->begin() )
         {
            dSprintf(returnBuffer + returnLeng, maxReturn - returnLeng, "%.9g", *itr);
         }
         else
         {
            dSprintf(returnBuffer + returnLeng, maxReturn - returnLeng, " %.9g", *itr);
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
   else
   {
      static std::vector<KorkApi::ConsoleValue> tmpArgv;
      tmpArgv.resize(inputStorage->data.argc);
      
      for (U32 i=0; i<inputStorage->data.argc; i++)
      {
         tmpArgv[i] = KorkApi::ConsoleValue::makeNumber(vec->operator[](i));
      }
      KorkApi::TypeStorageInterface castInput = KorkApi::CreateRegisterStorageFromArgs(vmPtr->mInternal, inputStorage->data.argc, &tmpArgv[0]);
      return vmPtr->castValue(requestedType, &castInput, outputStorage, nullptr, 0);
   }
   
   return false;
}

ConsoleTypeOpDefaultNumeric( TypeF32Vector )

ConsoleGetType( TypeBoolVector )
{
   std::vector<bool> *vec = nullptr;
   static std::vector<bool> workVec;
   
   if (!inputStorage->isField)
   {
      if (!outputStorage->isField &&
          (requestedType == TypeBoolVector) &&
          inputStorage->data.argc == 1 &&
          inputStorage->data.storageRegister->typeId == TypeBoolVector)
      {
         // Just copy data
         U32* ptr = (U32*)ConsoleGetInputStoragePtr();
         U32 numElements = ptr ? *ptr++ : 0;
         U32 dataSize = (numElements * sizeof(bool));
         outputStorage->FinalizeStorage(outputStorage, sizeof(U32) +  dataSize);
         U32* returnBuffer = (U32*)ConsoleGetOutputStoragePtr();
         *returnBuffer++ = numElements;
         
         if (ptr)
         {
            memcpy(returnBuffer, ptr, dataSize);
         }
         
         if (outputStorage->data.storageRegister)
         {
            *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
         }
      
         return true;
      }
      else
      {
         // Need to take long path
         vec = &workVec;
         vec->clear();
         
         if (inputStorage->data.argc > 1)
         {
            for (U32 i=0; i<inputStorage->data.argc; i++)
            {
               vec->push_back( vmPtr->valueAsBool(inputStorage->data.storageRegister[i]) );
            }
         }
         else if (inputStorage->data.argc > 0)
         {
            if (inputStorage->data.storageRegister->typeId == TypeBoolVector)
            {
               U32 numElems = ((U32*)ConsoleGetInputStoragePtr())[0];
               bool* data = ((bool*)ConsoleGetInputStoragePtr())+1;
               
               for (S32 i=0; i<numElems; i++)
               {
                  vec->push_back(data[i]);
               }
            }
            else
            {
               const char* values = vmPtr->valueAsString(inputStorage->data.storageRegister[0]);
               if (!values) values = "";
               const char* p   = values;
               const char* end = values + dStrlen(values);
               
               while (p < end)
               {
                  p = SkipSpaces(p);
                  if (p >= end) break;
                  
                  U32 v = 0;
                  if (dSscanf(p, "%i", &v) == 0)
                     break;
                  
                  vec->push_back(v);
                  
                  // advance to next delimiter (space separated)
                  const char* next = dStrchr(p, ' ');
                  if (!next || next >= end) break;
                  p = next + 1;
               }
            }
         }
      }
      
   }
   else
   {
      vec = (std::vector<bool>*)ConsoleGetInputStoragePtr();
   }
   
   // Set conservative output size
   
   // Input should now be a vector; we need to convert it to the output

   if (outputStorage->isField &&
       requestedType == TypeBoolVector)
   {
      // In this case we are setting output to this type so just copy the vector
      // NOTE: we will NEVER get outputStorage->isField == true where the type isn't the native type
      std::vector<bool> *outputVec = (std::vector<bool>*)ConsoleGetOutputStoragePtr();
      *outputVec = *vec;
   }
   else if (!outputStorage->isField &&
            requestedType == TypeBoolVector)
   {
      outputStorage->FinalizeStorage(outputStorage, (vec->size() * sizeof(S32)) + sizeof(U32));
      U32* vecCount = (U32*)ConsoleGetOutputStoragePtr();
      *vecCount++ = vec->size();
      std::copy(vec->begin(), vec->end(), (bool*)vecCount);
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      }
      
      return true;
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      S32 maxReturn = 1024;
      outputStorage->ResizeStorage(outputStorage, maxReturn);
      char* returnBuffer = (char*)outputStorage->data.storageAddress.evaluatePtr(vmPtr->getAllocBase());
      returnBuffer[0] = '\0';
      S32 returnLeng = 0;
      
      for (std::vector<bool>::iterator itr = vec->begin(); itr < vec->end(); itr++)
      {
         // concatenate the next value onto the return string
         if ( itr == vec->begin() )
         {
            dSprintf(returnBuffer + returnLeng, maxReturn - returnLeng, "%i", *itr);
         }
         else
         {
            dSprintf(returnBuffer + returnLeng, maxReturn - returnLeng, " %i", *itr);
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
   else
   {
      static std::vector<KorkApi::ConsoleValue> tmpArgv;
      tmpArgv.resize(inputStorage->data.argc);
      
      for (U32 i=0; i<inputStorage->data.argc; i++)
      {
         tmpArgv[i] = KorkApi::ConsoleValue::makeNumber(vec->operator[](i));
      }
      KorkApi::TypeStorageInterface castInput = KorkApi::CreateRegisterStorageFromArgs(vmPtr->mInternal, inputStorage->data.argc, &tmpArgv[0]);
      return vmPtr->castValue(requestedType, &castInput, outputStorage, nullptr, 0);
   }
   
   return false;
}

ConsoleTypeOpDefaultUnsigned( TypeBoolVector )

ConsoleGetType( TypeEnum )
{
   AssertFatal(tbl, "invalid table");
   if (inputStorage->data.argc != 1) return false;
   
   S32 value = inputStorage->isField ? *((S32*)(ConsoleGetInputStoragePtr())) : (S32)vmPtr->valueAsInt(inputStorage->data.storageRegister[0]);
   
   if (requestedType == TypeEnum)
   {
      // copy to output
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeRaw((U32)value, TypeEnum, KorkApi::ConsoleValue::ZonePacked);
      }
      else
      {
         *((S32*)ConsoleGetOutputStoragePtr()) = value;
      }
      
      return true;
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      const EnumTable* tbl = (const EnumTable*)fieldUserPtr;
      const char* label = tbl->table[value].label;
      U32 len = dStrlen(label)+1;
      outputStorage->FinalizeStorage(outputStorage, len);
      memcpy(ConsoleGetOutputStoragePtr(), label, len);
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      }
      
      return true;
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalNumber)
   {
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeNumber(value);
      }
      else
      {
         *((F64*)ConsoleGetOutputStoragePtr()) = value;
      }
   }
   else if (requestedType == KorkApi::ConsoleValue::TypeInternalUnsigned)
   {
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeUnsigned((U64)value);
      }
      else
      {
         *((U64*)ConsoleGetOutputStoragePtr()) = value;
      }
      
      return true;
   }
   else
   {
      // Cast to correct value based on label
      const EnumTable* tbl = (const EnumTable*)fieldUserPtr;
      KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeString(tbl->table[value].label);
      KorkApi::TypeStorageInterface castInput =
         KorkApi::CreateRegisterStorageFromArgs(vmPtr->mInternal, 1, &cv);

      return vmPtr->castValue(requestedType, &castInput, outputStorage, nullptr, 0);
   }
   
   return true;
}

ConsoleTypeOpDefaultUnsigned( TypeEnum )

ConsoleTypeOpDefaultUnsigned( TypeSimObjectPtr )

ConsoleGetType( TypeSimObjectPtr )
{
   SimObject* valueObject = nullptr;
   SimObjectId value = 0;
   
   if (inputStorage->isField)
   {
      valueObject = *((SimObject**)(ConsoleGetInputStoragePtr()));
      if (valueObject)
      {
         value = valueObject->getId();
      }
   }
   else
   {
      value = (SimObjectId)vmPtr->valueAsInt(inputStorage->data.storageRegister[0]);
      Sim::findObject(value, valueObject);
   }
   
   if (requestedType == KorkApi::ConsoleValue::TypeInternalString)
   {
      outputStorage->FinalizeStorage(outputStorage, 6);
      dSprintf((char*)ConsoleGetOutputStoragePtr(), 6, "%u", value);
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      }
      
      return true;
   }
   else if (requestedType == TypeSimObjectPtr)
   {
      if (outputStorage->isField)
      {
         SimObject** dst = (SimObject**)ConsoleGetOutputStoragePtr();
         *dst = valueObject;
      }
      
      if (outputStorage->data.storageRegister)
      {
         *outputStorage->data.storageRegister = KorkApi::ConsoleValue::makeNumber(value);
      }
      
      return true;
   }
   else
   {
      KorkApi::ConsoleValue cv = KorkApi::ConsoleValue::makeNumber(value);
      KorkApi::TypeStorageInterface castInput =
         KorkApi::CreateRegisterStorageFromArgs(vmPtr->mInternal, 1, &cv);

      return vmPtr->castValue(requestedType, &castInput, outputStorage, nullptr, 0);
   }
}

#if 0
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
