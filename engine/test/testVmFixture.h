//-----------------------------------------------------------------------------
// Copyright (c) 2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

#pragma once

#include "embed/api.h"

#include <cstdlib>

class TestVmFixture
{
public:
   TestVmFixture()
   {
      KorkApi::Config cfg{};
      cfg.mallocFn = [](size_t size, void* user) -> void*
      {
         (void)user;
         return std::malloc(size);
      };
      cfg.freeFn = [](void* ptr, void* user)
      {
         (void)user;
         std::free(ptr);
      };
      cfg.enableTypes = true;
      cfg.enableAdvancedFields = true;
      cfg.maxFibers = 16;
      vm = KorkApi::createVM(&cfg);
      internal = vm->mInternal;
   }

   ~TestVmFixture()
   {
      KorkApi::destroyVM(vm);
   }

   KorkApi::Vm* vm = nullptr;
   KorkApi::VmInternal* internal = nullptr;
};
