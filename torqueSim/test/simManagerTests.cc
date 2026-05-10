//-----------------------------------------------------------------------------
// Copyright (c) 2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

#include "console/console.h"
#include "sim/simBase.h"

#include <catch2/catch.hpp>

#include <atomic>
#include <string>
#include <utility>

namespace
{
   static std::atomic<U32> gNextSimManagerTestId{300000};

   std::string uniqueName(const char* prefix)
   {
      return std::string(prefix) + "_" + std::to_string(gNextSimManagerTestId.fetch_add(1));
   }

   template <class T, class... Args>
   static T* createRegisteredObject(const std::string& name, Args&&... args)
   {
      T* obj = new T(std::forward<Args>(args)...);
      REQUIRE(obj->registerObject(name.c_str()));
      return obj;
   }

   template <class T, class... Args>
   static T* createRegisteredObjectWithId(const std::string& name, U32 id, Args&&... args)
   {
      T* obj = new T(std::forward<Args>(args)...);
      REQUIRE(obj->registerObject(name.c_str(), id));
      return obj;
   }

   class ManagerLookupObject : public SimObject
   {
      typedef SimObject Parent;

   public:
      ManagerLookupObject() = default;

      DECLARE_CONOBJECT(ManagerLookupObject);
   };
}

IMPLEMENT_CONOBJECT(ManagerLookupObject);

TEST_CASE("SimManager resolves global groups through absolute paths", "[SimManager]") {
   // The manager bootstraps a root group and a datablock group; both should resolve from absolute paths.
   REQUIRE(Sim::getRootGroup() != nullptr);
   REQUIRE(Sim::getDataBlockGroup() != nullptr);
   REQUIRE(Sim::findObject("RootGroup") == Sim::getRootGroup());
   REQUIRE(Sim::findObject("/DataBlockGroup") == Sim::getDataBlockGroup());
}

TEST_CASE("SimManager resolves names and id-prefixed paths", "[SimManager]") {
   SimSet* group = createRegisteredObjectWithId<SimSet>(uniqueName("managerSet"), 310001);
   ManagerLookupObject* child = createRegisteredObject<ManagerLookupObject>(uniqueName("managerChild"));
   group->addObject(child);

   // `Sim::findObject` should follow both the name-based and the id-based path forms.
   const std::string namePath = std::string(group->getName()) + "/" + child->getName();
   const std::string idPath = std::to_string(group->getId()) + "/" + child->getName();
   REQUIRE(Sim::findObject(group->getName()) == group);
   REQUIRE(Sim::findObject(namePath.c_str()) == child);
   REQUIRE(Sim::findObject(idPath.c_str()) == child);

   const std::string missingPath = std::to_string(group->getId()) + "/no_such_child";
   REQUIRE(Sim::findObject(missingPath.c_str()) == nullptr);

   child->deleteObject();
   group->deleteObject();
}

TEST_CASE("SimManager resolves ConsoleValue lookups by value kind", "[SimManager]") {
   SimSet* group = createRegisteredObjectWithId<SimSet>(uniqueName("cvSet"), 320001);
   ManagerLookupObject* child = createRegisteredObject<ManagerLookupObject>(uniqueName("cvChild"));
   group->addObject(child);

   // The ConsoleValue overload should dispatch numeric values directly to id lookups.
   REQUIRE(Sim::findObject(KorkApi::ConsoleValue::makeUnsigned(group->getId())) == group);
   REQUIRE(Sim::findObject(KorkApi::ConsoleValue::makeNumber((F64)group->getId())) == group);

   child->deleteObject();
   group->deleteObject();
}

TEST_CASE("SimManager returns nullptr for missing lookup targets", "[SimManager]") {
   // Bad input should fail cleanly rather than dereferencing lookup state.
   REQUIRE(Sim::findObject(nullptr) == nullptr);
   REQUIRE(Sim::findObject("") == nullptr);
   REQUIRE(Sim::findObject("definitely_missing") == nullptr);
   REQUIRE(Sim::findObject(999999999u) == nullptr);
   REQUIRE(Sim::findObject(KorkApi::ConsoleValue::makeString("missing_path")) == nullptr);
}
