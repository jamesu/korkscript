//-----------------------------------------------------------------------------
// Copyright (c) 2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "embed/api.h"
#include "embed/internalApi.h"
#include "console/compiler.h"
#include "console/consoleNamespace.h"
#include "engine/test/testVmFixture.h"

#include <catch2/catch.hpp>

#include <cstring>

namespace
{
   struct AdvancedElement
   {
      U32 value = 0;
   };

   struct AdvancedArray
   {
      AdvancedElement indexed[4];
      AdvancedElement keyed[4];
   };

   struct AdvancedFieldState
   {
      KorkApi::TypeId elementType = -1;
      KorkApi::TypeId arrayType = -1;
      AdvancedArray seed;
      U32 resolveCalls = 0;
      U32 writeResolveCalls = 0;
   };

   bool castAdvancedElement(void*,
                            KorkApi::Vm* vm,
                            KorkApi::TypeStorageInterface* inputStorage,
                            KorkApi::TypeStorageInterface* outputStorage,
                            void*,
                            BitSet32,
                            U32)
   {
      AdvancedElement value;
      if (inputStorage->data.storageRegister && inputStorage->data.storageRegister->isUnsigned())
      {
         value.value = (U32)vm->valueAsInt(*inputStorage->data.storageRegister);
      }
      else
      {
         void* inputPtr = inputStorage->data.storageAddress.evaluatePtr(vm->getAllocBase());
         if (inputPtr)
            value = *static_cast<AdvancedElement*>(inputPtr);
      }

      outputStorage->FinalizeStorage(outputStorage, sizeof(AdvancedElement));
      void* outputPtr = outputStorage->data.storageAddress.evaluatePtr(vm->getAllocBase());
      if (!outputPtr)
         return false;

      *static_cast<AdvancedElement*>(outputPtr) = value;
      if (outputStorage->data.storageRegister)
         *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      return true;
   }

   bool castAdvancedArray(void*,
                          KorkApi::Vm* vm,
                          KorkApi::TypeStorageInterface* inputStorage,
                          KorkApi::TypeStorageInterface* outputStorage,
                          void*,
                          BitSet32,
                          U32)
   {
      AdvancedArray value;
      void* inputPtr = inputStorage->data.storageAddress.evaluatePtr(vm->getAllocBase());
      if (inputPtr)
         value = *static_cast<AdvancedArray*>(inputPtr);

      outputStorage->FinalizeStorage(outputStorage, sizeof(AdvancedArray));
      void* outputPtr = outputStorage->data.storageAddress.evaluatePtr(vm->getAllocBase());
      if (!outputPtr)
         return false;

      *static_cast<AdvancedArray*>(outputPtr) = value;
      if (outputStorage->data.storageRegister)
         *outputStorage->data.storageRegister = outputStorage->data.storageAddress;
      return true;
   }

   bool resolveAdvancedArrayField(void* userPtr,
                                  KorkApi::Vm* vm,
                                  KorkApi::TypeStorageInterface* baseStorage,
                                  StringTableEntry fieldName,
                                  KorkApi::ConsoleValue arrayIndex,
                                  KorkApi::TypeStorageInterface* outStorage,
                                  bool wantWrite)
   {
      auto* state = static_cast<AdvancedFieldState*>(userPtr);
      auto* array = static_cast<AdvancedArray*>(baseStorage->data.storageAddress.evaluatePtr(vm->getAllocBase()));
      if (!array)
         return false;

      const U32 index = (U32)vm->valueAsInt(arrayIndex) % 4;
      AdvancedElement* element = nullptr;

      if (fieldName == nullptr)
         element = &array->indexed[index];
      else if (std::strcmp(fieldName, "key") == 0)
         element = &array->keyed[index];
      else
         return false;

      state->resolveCalls++;
      if (wantWrite)
         state->writeResolveCalls++;

      *outStorage = KorkApi::CreateFixedTypeStorage(vm->mInternal, element, (U16)state->elementType, true);
      return true;
   }

   KorkApi::ConsoleValue makeAdvancedArray(void*, void* userPtr, S32, KorkApi::ConsoleValue[])
   {
      auto* state = static_cast<AdvancedFieldState*>(userPtr);
      return KorkApi::ConsoleValue::makeTyped(&state->seed, (U16)state->arrayType);
   }

   void registerAdvancedFieldTypes(TestVmFixture& fx, AdvancedFieldState& state)
   {
      KorkApi::TypeInfo elementInfo = {};
      elementInfo.name = fx.vm->internString("AdvancedElement", false);
      elementInfo.fieldSize = sizeof(AdvancedElement);
      elementInfo.valueSize = sizeof(AdvancedElement);
      elementInfo.iFuncs.CastValueFn = castAdvancedElement;
      state.elementType = fx.vm->registerType(elementInfo);

      KorkApi::TypeInfo arrayInfo = {};
      arrayInfo.name = fx.vm->internString("AdvancedArray", false);
      arrayInfo.fieldSize = sizeof(AdvancedArray);
      arrayInfo.valueSize = sizeof(AdvancedArray);
      arrayInfo.userPtr = &state;
      arrayInfo.iFuncs.CastValueFn = castAdvancedArray;
      arrayInfo.iFuncs.ResolveFieldFn = resolveAdvancedArrayField;
      state.arrayType = fx.vm->registerType(arrayInfo);

      KorkApi::NamespaceId globalNs = fx.internal->mNSState.global();
      fx.vm->addNamespaceFunction(globalNs,
                                  fx.vm->internString("makeAdvancedArray", false),
                                  makeAdvancedArray,
                                  &state,
                                  "makeAdvancedArray()",
                                  1,
                                  1);
   }

   AdvancedElement readElement(KorkApi::Vm* vm, KorkApi::ConsoleValue value)
   {
      void* ptr = value.evaluatePtr(vm->getAllocBase());
      return ptr ? *static_cast<AdvancedElement*>(ptr) : AdvancedElement{};
   }
}

TEST_CASE_METHOD(TestVmFixture, "advancedFields parses typed indexing and routes assignment through ResolveFieldFn", "[AdvancedFields]") {
   AdvancedFieldState state;
   state.seed.indexed[1].value = 11;
   registerAdvancedFieldTypes(*this, state);

   vm->evalCode(
      "function advancedFieldIndexSmoke()\n"
      "{\n"
      "   %arr : AdvancedArray = makeAdvancedArray();\n"
      "   %arr{1} = 22;\n"
      "   return %arr{1};\n"
      "}\n"
      "\n",
      "advancedFieldIndexSmoke.cs",
      nullptr);
   KorkApi::ConsoleValue callArg = KorkApi::ConsoleValue::makeString("advancedFieldIndexSmoke");
   KorkApi::ConsoleValue result;
   REQUIRE(vm->callNamespaceFunction(vm->getGlobalNamespace(),
                                     vm->internString("advancedFieldIndexSmoke", false),
                                     1,
                                     &callArg,
                                     result));

   REQUIRE(result.typeId == state.elementType);
   INFO("resolveCalls=" << state.resolveCalls << " writeResolveCalls=" << state.writeResolveCalls);
   REQUIRE(readElement(vm, result).value == 22);
   REQUIRE(state.resolveCalls >= 2);
   REQUIRE(state.writeResolveCalls == 1);
}

TEST_CASE_METHOD(TestVmFixture, "advancedFields parses typed keyed access with optional index", "[AdvancedFields]") {
   AdvancedFieldState state;
   state.seed.keyed[0].value = 41;
   state.seed.keyed[2].value = 7;
   registerAdvancedFieldTypes(*this, state);

   vm->evalCode(
      "function advancedFieldKeySmoke()\n"
      "{\n"
      "   %arr : AdvancedArray = makeAdvancedArray();\n"
      "   %arr.key{2} = 99;\n"
      "   return %arr.key{2};\n"
      "}\n"
      "\n",
      "advancedFieldKeySmoke.cs",
      nullptr);
   KorkApi::ConsoleValue callArg = KorkApi::ConsoleValue::makeString("advancedFieldKeySmoke");
   KorkApi::ConsoleValue result;
   REQUIRE(vm->callNamespaceFunction(vm->getGlobalNamespace(),
                                     vm->internString("advancedFieldKeySmoke", false),
                                     1,
                                     &callArg,
                                     result));

   REQUIRE(result.typeId == state.elementType);
   INFO("resolveCalls=" << state.resolveCalls << " writeResolveCalls=" << state.writeResolveCalls);
   REQUIRE(readElement(vm, result).value == 99);
   REQUIRE(state.resolveCalls >= 2);
   REQUIRE(state.writeResolveCalls == 1);
}

TEST_CASE_METHOD(TestVmFixture, "advancedFields leaves untyped dotted access on the object field path", "[AdvancedFields]") {
   AdvancedFieldState state;
   registerAdvancedFieldTypes(*this, state);

   KorkApi::AstParseErrorInfo parseError = {};
   KorkApi::CompiledBlock compiled = {};
   const char* source =
      "function ordinaryDotAccessStillCompiles()\n"
      "{\n"
      "   %arr = makeAdvancedArray();\n"
      "   return %arr.key;\n"
      "}\n";

   REQUIRE(vm->enumerateAst(source, "ordinaryDotAccessStillCompiles.cs", nullptr, [](void*, const KorkApi::AstEnumerationInfo*) {
      return KorkApi::AstEnumerationContinue;
   }, &parseError) == KorkApi::AstEnumerationCompleted);
   REQUIRE(parseError.stage == KorkApi::AstParseErrorNone);
   REQUIRE(vm->compileCodeBlock(source, "ordinaryDotAccessStillCompiles.cs", &compiled));
   vm->freeCompiledBlock(compiled);
   REQUIRE(state.resolveCalls == 0);
}

TEST_CASE_METHOD(TestVmFixture, "advancedFields syntax is rejected when the feature flag is disabled", "[AdvancedFields]") {
   const bool prevAdvancedFields = internal->mCompilerResources->allowAdvancedFields;
   internal->mCompilerResources->allowAdvancedFields = false;

   KorkApi::AstParseErrorInfo parseError = {};
   REQUIRE(vm->enumerateAst(
      "function advancedFieldsDisabled()\n"
      "{\n"
      "   %arr{1};\n"
      "}\n",
      "advancedFieldsDisabled.cs",
      nullptr,
      [](void*, const KorkApi::AstEnumerationInfo*) {
         return KorkApi::AstEnumerationContinue;
      },
      &parseError) == KorkApi::AstEnumerationParseFailed);
   REQUIRE(parseError.stage == KorkApi::AstParseErrorParser);

   internal->mCompilerResources->allowAdvancedFields = prevAdvancedFields;
}
