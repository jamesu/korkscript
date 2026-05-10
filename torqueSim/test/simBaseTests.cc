//-----------------------------------------------------------------------------
// Copyright (c) 2026 korkscript contributors.
// See AUTHORS file and git repository for contributor information.
//
// SPDX-License-Identifier: MIT
//-----------------------------------------------------------------------------

#include "platform/platform.h"
#include "platform/platformString.h"
#include "console/console.h"
#include "console/consoleTypes.h"
#include "core/memStream.h"
#include "sim/simBase.h"
#include <catch2/catch.hpp>

#include <atomic>
#include <array>
#include <cstring>
#include <string>
#include <vector>

class TestSimObject : public SimObject
{
   typedef SimObject Parent;

public:
   S32 mTestValue;
   S32 mTestArray[2];
   U32 mOnAddCount;
   U32 mOnRemoveCount;
   U32 mOnGroupAddCount;
   U32 mOnGroupRemoveCount;
   U32 mOnNameChangeCount;
   U32 mOnStaticModifiedCount;
   U32 mOnInspectPreApplyCount;
   U32 mOnInspectPostApplyCount;
   U32 mOnEditorEnableCount;
   U32 mOnEditorDisableCount;
   U32 mOnDeleteNotifyCount;
   U32 mConsoleCallCount;
   U32 mSignalDispatchCount;
   S32 mLastProcessArgc;
   std::string mLastProcessArg0;
   std::string mLastName;
   std::string mLastStaticSlot;
   std::string mLastStaticValue;
   std::string mLastPayload;
   std::string mLastSignalName;

   TestSimObject()
      : mTestValue(0),
        mTestArray{0, 0},
        mOnAddCount(0),
        mOnRemoveCount(0),
        mOnGroupAddCount(0),
        mOnGroupRemoveCount(0),
        mOnNameChangeCount(0),
        mOnStaticModifiedCount(0),
        mOnInspectPreApplyCount(0),
        mOnInspectPostApplyCount(0),
        mOnEditorEnableCount(0),
        mOnEditorDisableCount(0),
        mOnDeleteNotifyCount(0),
        mConsoleCallCount(0),
        mSignalDispatchCount(0),
        mLastProcessArgc(-1)
   {
   }

   static void initPersistFields()
   {
      Parent::initPersistFields();
      addField("testValue", TypeS32, Offset(mTestValue, TestSimObject));
      addField("testArray", TypeS32, Offset(mTestArray, TestSimObject), 2);
   }

   bool onAdd() override
   {
      ++mOnAddCount;
      return Parent::onAdd();
   }

   void onRemove() override
   {
      ++mOnRemoveCount;
      Parent::onRemove();
   }

   void onGroupAdd() override
   {
      ++mOnGroupAddCount;
      Parent::onGroupAdd();
   }

   void onGroupRemove() override
   {
      ++mOnGroupRemoveCount;
      Parent::onGroupRemove();
   }

   void onNameChange(const char* name) override
   {
      ++mOnNameChangeCount;
      mLastName = name ? name : "";
      Parent::onNameChange(name);
   }

   void onStaticModified(const char* slotName, const char* newValue) override
   {
      ++mOnStaticModifiedCount;
      mLastStaticSlot = slotName ? slotName : "";
      mLastStaticValue = newValue ? newValue : "";
      Parent::onStaticModified(slotName, newValue);
   }

   void inspectPreApply() override
   {
      ++mOnInspectPreApplyCount;
      Parent::inspectPreApply();
   }

   void inspectPostApply() override
   {
      ++mOnInspectPostApplyCount;
      Parent::inspectPostApply();
   }

   void onEditorEnable() override
   {
      ++mOnEditorEnableCount;
      Parent::onEditorEnable();
   }

   void onEditorDisable() override
   {
      ++mOnEditorDisableCount;
      Parent::onEditorDisable();
   }

   void onDeleteNotify(SimObject* object) override
   {
      ++mOnDeleteNotifyCount;
      Parent::onDeleteNotify(object);
   }

   bool processArguments(S32 argc, const char** argv) override
   {
      mLastProcessArgc = argc;
      mLastProcessArg0 = (argc > 0 && argv && argv[0]) ? argv[0] : "";
      return Parent::processArguments(argc, argv);
   }

   DECLARE_CONOBJECT(TestSimObject);
};

IMPLEMENT_CONOBJECT(TestSimObject);

ConsoleMethod(TestSimObject, bumpCounter, void, 2, 2, "")
{
   object->mConsoleCallCount++;
}

ConsoleMethod(TestSimObject, recordPayload, void, 3, 3, "")
{
   object->mConsoleCallCount++;
   object->mLastPayload = argv[2];
}

ConsoleMethod(TestSimObject, handleSignal, void, 3, 3, "")
{
   object->mSignalDispatchCount++;
   object->mLastSignalName = argv[0];
   object->mLastPayload = argv[2];
}

static std::atomic<U32> gNextTestObjectId{100000};
static std::string gCapturedConsoleOutput;
extern KorkApi::Vm* sVM;

static void captureConsoleLine(U32, const char* consoleLine, void* userPtr)
{
   if (!userPtr || !consoleLine)
      return;

   auto* out = static_cast<std::string*>(userPtr);
   out->append(consoleLine);
   out->push_back('\n');
}

static std::string uniqueName(const char* prefix)
{
   return std::string(prefix) + "_" + std::to_string(gNextTestObjectId.fetch_add(1));
}

static std::string evalConsole(const std::string& script)
{
   const char* result = Con::evaluate(script.c_str());
   return result ? result : "";
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

TEST_CASE("SimObject registration and identity", "[SimObject]") {
   const std::string rootName = uniqueName("simObject");
   const U32 customId = 100001 + gNextTestObjectId.fetch_add(1);

   TestSimObject* obj = createRegisteredObjectWithId<TestSimObject>(rootName, customId);

   REQUIRE(obj->isProperlyAdded());
   REQUIRE(obj->getName() == StringTable->insert(rootName.c_str()));
   REQUIRE(obj->getId() == customId);
   REQUIRE(Sim::findObject(rootName.c_str()) == obj);
   REQUIRE(Sim::findObject(customId) == obj);
   REQUIRE(obj->getGroup() == nullptr);
   REQUIRE_FALSE(obj->isChildOfGroup(Sim::getRootGroup()));
   REQUIRE(obj->mOnAddCount == 1);
   REQUIRE(obj->mOnNameChangeCount == 1);

   TestSimObject* idOnly = new TestSimObject();
   REQUIRE(idOnly->registerObject(customId + 2000));
   REQUIRE(idOnly->getId() == customId + 2000);
   REQUIRE(idOnly->getName() == nullptr);
   idOnly->deleteObject();

   const std::string renamed = uniqueName("renamed");
   obj->assignName(renamed.c_str());
   REQUIRE(obj->mOnNameChangeCount == 2);
   REQUIRE(obj->mLastName == renamed);
   REQUIRE(Sim::findObject(rootName.c_str()) == nullptr);
   REQUIRE(Sim::findObject(renamed.c_str()) == obj);

   const U32 newId = customId + 1000;
   obj->setId(newId);
   REQUIRE(obj->getId() == newId);
   REQUIRE(Sim::findObject(newId) == obj);
   REQUIRE(Sim::findObject(customId) == nullptr);

   obj->setProgenitorFile("test/simBaseTests.cc");
   REQUIRE(std::string(obj->getProgenitorFile()) == "test/simBaseTests.cc");

   obj->setPeriodicTimerID(42);
   REQUIRE(obj->isPeriodicTimerActive());
   REQUIRE(obj->getPeriodicTimerID() == 42);
   obj->setPeriodicTimerID(0);
   REQUIRE_FALSE(obj->isPeriodicTimerActive());

   obj->deleteObject();
   REQUIRE(Sim::findObject(renamed.c_str()) == nullptr);
}

TEST_CASE("SimObject fields and copy helpers", "[SimObject]") {
   TestSimObject* source = createRegisteredObject<TestSimObject>(uniqueName("source"));
   TestSimObject* target = createRegisteredObject<TestSimObject>(uniqueName("target"));

   REQUIRE(source->processArguments(0, nullptr));
   const char* argList[] = {"alpha"};
   REQUIRE_FALSE(source->processArguments(1, argList));
   REQUIRE(source->mLastProcessArgc == 1);
   REQUIRE(source->mLastProcessArg0 == "alpha");

   REQUIRE(source->getDataFieldType(StringTable->insert("testValue"), nullptr) == TypeS32);
   source->mTestValue = 37;
   char staticBuffer[1024] = {};
   MemStream staticStream(sizeof(staticBuffer), staticBuffer, true, true);
   source->writeFields(staticStream, 0);
   REQUIRE(std::string(staticBuffer).find("testValue") != std::string::npos);
   REQUIRE(std::string(staticBuffer).find("37") != std::string::npos);

   source->setDataFieldDynamic(StringTable->insert("dynamicField"), nullptr, "dynamicValue", TypeString);
   REQUIRE(std::string(source->getDataFieldDynamic(StringTable->insert("dynamicField"), nullptr)) == "dynamicValue");

   source->setDataFieldDynamic(StringTable->insert("typedField"), nullptr, "123", TypeS32);
   U32 typedFieldType = 0;
   REQUIRE(std::string(source->getDataFieldDynamic(StringTable->insert("typedField"), nullptr, &typedFieldType)) == "123");
   REQUIRE(typedFieldType == TypeS32);
   source->setInternalName("sourceInternal");
   REQUIRE(source->getInternalName() == StringTable->insert("sourceInternal"));

   TestSimObject* copied = createRegisteredObject<TestSimObject>(uniqueName("copied"));
   source->setClassNamespace("sourceClass");
   source->setSuperClassNamespace("sourceSuper");
   source->copyTo(copied);
   REQUIRE(copied->getClassNamespace() == StringTable->insert("sourceClass"));
   REQUIRE(copied->getSuperClassNamespace() == StringTable->insert("sourceSuper"));

   target->setInternalName("targetInternal");
   REQUIRE_NOTHROW(target->assignFieldsFrom(source));

   target->assignDynamicFieldsFrom(source);
   REQUIRE(std::string(target->getDataFieldDynamic(StringTable->insert("dynamicField"), nullptr)) == "dynamicValue");
   REQUIRE(std::string(target->getDataFieldDynamic(StringTable->insert("typedField"), nullptr)) == "123");

   TestSimObject* cloneNoDyn = dynamic_cast<TestSimObject*>(source->clone(false));
   REQUIRE(cloneNoDyn != nullptr);
   REQUIRE(std::string(cloneNoDyn->getDataFieldDynamic(StringTable->insert("dynamicField"), nullptr)).empty());
   cloneNoDyn->deleteObject();

   TestSimObject* cloneWithDyn = dynamic_cast<TestSimObject*>(source->clone(true));
   REQUIRE(cloneWithDyn != nullptr);
   REQUIRE(std::string(cloneWithDyn->getDataFieldDynamic(StringTable->insert("dynamicField"), nullptr)) == "dynamicValue");
   REQUIRE(std::string(cloneWithDyn->getDataFieldDynamic(StringTable->insert("typedField"), nullptr)) == "123");
   cloneWithDyn->deleteObject();

   source->clearDynamicFields();
   REQUIRE(std::string(source->getDataFieldDynamic(StringTable->insert("dynamicField"), nullptr)).empty());
   REQUIRE(std::string(source->getDataFieldDynamic(StringTable->insert("typedField"), nullptr)).empty());

   target->deleteObject();
   copied->deleteObject();
   source->deleteObject();
}

TEST_CASE("SimObject reflection and field enumeration", "[SimObject][Console]") {
   const std::string objectName = uniqueName("reflectObj");
   TestSimObject* obj = createRegisteredObject<TestSimObject>(objectName);

   obj->getVMObject()->flags |= KorkApi::ModStaticFields | KorkApi::ModDynamicFields;
   obj->setInternalName("reflectInternal");
   obj->setProgenitorFile("test/simBaseTests.cc");
   obj->setDataField(StringTable->insert("testValue"), nullptr, "37");
   evalConsole(objectName + ".testArray[1] = \"41\";");
   obj->setDataField(StringTable->insert("testArray"), "1", "41");
   obj->setDataFieldDynamic(StringTable->insert("dynamicField"), nullptr, "dynamicValue", TypeString);
   obj->setDataFieldDynamic(StringTable->insert("dynamicArray"), "1", "dynamicArrayValue", TypeString);

   REQUIRE(obj->getDataField(StringTable->insert("testArray"), "1") != nullptr);
   REQUIRE(std::string(obj->getDataField(StringTable->insert("testArray"), "1")) == "41");
   REQUIRE(obj->getDataFieldType(StringTable->insert("testArray"), "1") == TypeS32);
   REQUIRE(std::string(obj->getDataFieldDynamic(StringTable->insert("dynamicArray"), "1")) == "dynamicArrayValue");

   const std::string fieldCountExpr = objectName + ".getFieldCount();";
   const S32 fieldCount = dAtoi(evalConsole(fieldCountExpr).c_str());
   REQUIRE(fieldCount > 0);

   std::vector<std::string> fieldNames;
   for (S32 i = 0; i < fieldCount; ++i)
      fieldNames.push_back(evalConsole(objectName + ".getField(" + std::to_string(i) + ");"));
   REQUIRE(std::find(fieldNames.begin(), fieldNames.end(), "testValue") != fieldNames.end());
   REQUIRE(std::find(fieldNames.begin(), fieldNames.end(), "testArray") != fieldNames.end());

   const S32 dynamicCount = dAtoi(evalConsole(objectName + ".getDynamicFieldCount();").c_str());
   REQUIRE(dynamicCount >= 2);

   std::vector<std::string> dynamicFields;
   for (S32 i = 0; i < dynamicCount; ++i)
      dynamicFields.push_back(evalConsole(objectName + ".getDynamicField(" + std::to_string(i) + ");"));
   REQUIRE(std::any_of(dynamicFields.begin(), dynamicFields.end(), [](const std::string& line) {
      return line.find("dynamicField") != std::string::npos && line.find("dynamicValue") != std::string::npos;
   }));
   REQUIRE(std::any_of(dynamicFields.begin(), dynamicFields.end(), [](const std::string& line) {
      return line.find("dynamicArray1") != std::string::npos && line.find("dynamicArrayValue") != std::string::npos;
   }));

   REQUIRE(evalConsole(objectName + ".getClassName();") == "TestSimObject");
   REQUIRE(evalConsole(objectName + ".getInternalName();") == "reflectInternal");
   REQUIRE(evalConsole(objectName + ".getProgenitorFile();") == "test/simBaseTests.cc");
   REQUIRE(evalConsole(objectName + ".getType();") == "0");
   REQUIRE(evalConsole(objectName + ".isMethod(\"bumpCounter\");") == "1");
   REQUIRE(evalConsole(objectName + ".isMethod(\"nope\");") == "0");
   REQUIRE(obj->getMethodNamespace("bumpCounter") != StringTable->EmptyString);
   REQUIRE(evalConsole(objectName + ".getMethodNamespace(\"bumpCounter\");") == "1");

   evalConsole(objectName + ".call(\"bumpCounter\");");
   REQUIRE(obj->mConsoleCallCount == 1);

   obj->deleteObject();
}

TEST_CASE("SimObject write and save behavior", "[SimObject][Serialization]") {
   SimGroup* group = createRegisteredObject<SimGroup>(uniqueName("writeGroup"));
   const std::string objectName = uniqueName("writeObj");
   TestSimObject* obj = createRegisteredObject<TestSimObject>(objectName);
   group->addObject(obj);

   obj->getVMObject()->flags |= KorkApi::ModStaticFields | KorkApi::ModDynamicFields;
   obj->setInternalName("writeInternal");
   obj->setDataField(StringTable->insert("testValue"), nullptr, "");
   evalConsole(objectName + ".testArray[0] = \"12\";");
   obj->setDataField(StringTable->insert("testArray"), "0", "12");
   obj->setDataFieldDynamic(StringTable->insert("dynamicField"), nullptr, "writtenValue", TypeString);
   obj->setCanSaveDynamicFields(false);
   REQUIRE_FALSE(obj->getCanSaveDynamicFields());

   char buffer[8192] = {};
   MemStream stream(sizeof(buffer), buffer, true, true);
   obj->write(stream, 0);
   std::string output = buffer;
   REQUIRE(output.find("new TestSimObject(") != std::string::npos);
   REQUIRE(output.find("parentGroup") == std::string::npos);
   REQUIRE(output.find("testArray[0]") != std::string::npos);
   REQUIRE(output.find("dynamicField") == std::string::npos);

   obj->setCanSaveDynamicFields(true);
   REQUIRE(obj->getCanSaveDynamicFields());
   std::memset(buffer, 0, sizeof(buffer));
   MemStream stream2(sizeof(buffer), buffer, true, true);
   obj->write(stream2, 0);
   output = buffer;
   REQUIRE(output.find("dynamicField") != std::string::npos);
   REQUIRE(output.find("writtenValue") != std::string::npos);
   REQUIRE(output.find("parentGroup") == std::string::npos);
   REQUIRE(output.find("testArray[0]") != std::string::npos);

   obj->deleteObject();
   group->deleteObject();
}

TEST_CASE("SimObject lifecycle hooks and callback guard", "[SimObject]") {
   TestSimObject* obj = createRegisteredObject<TestSimObject>(uniqueName("hookObj"));

   obj->inspectPreApply();
   obj->inspectPostApply();
   obj->onEditorEnable();
   obj->onEditorDisable();
   REQUIRE(obj->mOnInspectPreApplyCount == 1);
   REQUIRE(obj->mOnInspectPostApplyCount == 1);
   REQUIRE(obj->mOnEditorEnableCount == 1);
   REQUIRE(obj->mOnEditorDisableCount == 1);

   REQUIRE(obj->getScriptCallbackGuard() == 0);
   obj->pushScriptCallbackGuard();
   obj->pushScriptCallbackGuard();
   REQUIRE(obj->getScriptCallbackGuard() == 2);
   obj->popScriptCallbackGuard();
   obj->popScriptCallbackGuard();
   REQUIRE(obj->getScriptCallbackGuard() == 0);

   obj->deleteObject();
}

TEST_CASE("SimObject notification cleanup edge cases", "[SimObject][Notifications]") {
   TestSimObject* target = createRegisteredObject<TestSimObject>(uniqueName("cleanupTarget"));
   TestSimObject* watcher = createRegisteredObject<TestSimObject>(uniqueName("cleanupWatcher"));

   SimObject* trackedTarget = target;
   target->registerReference(&trackedTarget);
   target->unregisterReference(&trackedTarget);
   target->deleteObject();
   REQUIRE(trackedTarget == target);

   TestSimObject* notifyTarget = createRegisteredObject<TestSimObject>(uniqueName("clearTarget"));
   TestSimObject* notifyWatcher = createRegisteredObject<TestSimObject>(uniqueName("clearWatcher"));
   notifyWatcher->deleteNotify(notifyTarget);
   notifyWatcher->clearAllNotifications();
   notifyTarget->deleteObject();
   REQUIRE(notifyWatcher->mOnDeleteNotifyCount == 0);

   watcher->deleteObject();
   notifyWatcher->deleteObject();
}

TEST_CASE("SimFieldDictionary basic behavior", "[SimFieldDictionary]") {
   SimFieldDictionary dict;
   REQUIRE(dict.getVersion() == 0);

   const StringTableEntry foo = StringTable->insert("fooField");
   const StringTableEntry bar = StringTable->insert("barField");

   dict.setFieldValue(foo, "17", TypeS32);
   dict.setFieldValue(bar, "hello", TypeString);
   REQUIRE(dict.getVersion() == 2);

   REQUIRE(std::string(dict.getFieldValue(foo)) == "17");
   REQUIRE(std::string(dict.getFieldValue(bar)) == "hello");

   std::vector<std::string> keys;
   std::vector<std::string> values;
   std::vector<U32> types;
   for (SimFieldDictionaryIterator itr(&dict); itr.isValid(); ++itr)
   {
      auto* entry = *itr;
      REQUIRE(entry != nullptr);
      keys.emplace_back(entry->slotName);
      values.emplace_back(entry->value ? entry->value : "");
      types.emplace_back(entry->enforcedTypeId);
   }

   REQUIRE(keys.size() == 2);
   REQUIRE(values.size() == 2);
   REQUIRE(types.size() == 2);
   REQUIRE(std::find(keys.begin(), keys.end(), "fooField") != keys.end());
   REQUIRE(std::find(keys.begin(), keys.end(), "barField") != keys.end());
   REQUIRE(std::find(types.begin(), types.end(), TypeS32) != types.end());
   REQUIRE(std::find(types.begin(), types.end(), TypeString) != types.end());

   SimFieldDictionary copy;
   copy.assignFrom(&dict);
   REQUIRE(std::string(copy.getFieldValue(bar)) == "hello");
}

TEST_CASE("Registered fields and field access controls", "[SimObject][Fields]") {
   TestSimObject* obj = createRegisteredObject<TestSimObject>(uniqueName("fieldObj"));

   // Enable both field paths so the test can verify the static and dynamic branches independently.
   obj->getVMObject()->flags |= KorkApi::ModStaticFields | KorkApi::ModDynamicFields;

   obj->setDataField(StringTable->insert("testValue"), nullptr, "37");
   REQUIRE(obj->mTestValue == 37);
   REQUIRE(std::string(obj->getDataField(StringTable->insert("testValue"), nullptr)) == "37");
   REQUIRE(obj->getDataFieldType(StringTable->insert("testValue"), nullptr) == TypeS32);

   obj->setDataField(StringTable->insert("dynamicField"), nullptr, "dynamicValue");
   REQUIRE(std::string(obj->getDataField(StringTable->insert("dynamicField"), nullptr)) == "dynamicValue");
   REQUIRE(obj->getDataFieldType(StringTable->insert("dynamicField"), nullptr) == 0);

   obj->getVMObject()->flags &= ~KorkApi::ModStaticFields;
   obj->setDataField(StringTable->insert("testValue"), nullptr, "99");
   REQUIRE(obj->mTestValue == 37);
   REQUIRE(std::string(obj->getDataField(StringTable->insert("testValue"), nullptr)) == "37");

   obj->getVMObject()->flags &= ~KorkApi::ModDynamicFields;
   obj->setDataField(StringTable->insert("dynamicField"), nullptr, "changed");
   REQUIRE(std::string(obj->getDataField(StringTable->insert("dynamicField"), nullptr)) == "dynamicValue");

   obj->deleteObject();
}

TEST_CASE("Locked and hidden flags", "[SimObject]") {
   TestSimObject* obj = createRegisteredObject<TestSimObject>(uniqueName("lockObj"));
   obj->getVMObject()->flags |= KorkApi::ModDynamicFields;

   obj->setLocked(true);
   obj->setHidden(true);
   REQUIRE(obj->isLocked());
   REQUIRE(obj->isHidden());

   obj->setLocked(false);
   obj->setHidden(false);
   REQUIRE_FALSE(obj->isLocked());
   REQUIRE_FALSE(obj->isHidden());

   obj->deleteObject();
}

TEST_CASE("Delete notifications and processDeleteNotifies", "[SimObject]") {
   TestSimObject* target = createRegisteredObject<TestSimObject>(uniqueName("notifyTarget"));
   TestSimObject* watcher = createRegisteredObject<TestSimObject>(uniqueName("notifyWatcher"));

   SimObject* trackedTarget = target;
   target->registerReference(&trackedTarget);
   watcher->deleteNotify(target);

   target->deleteObject();

   REQUIRE(trackedTarget == nullptr);
   REQUIRE(watcher->mOnDeleteNotifyCount == 1);

   watcher->deleteObject();
}

TEST_CASE("Signal dispatch", "[SimObject][Signals]") {
   TestSimObject* source = createRegisteredObject<TestSimObject>(uniqueName("signalSource"));
   TestSimObject* listener = createRegisteredObject<TestSimObject>(uniqueName("signalListener"));

   const StringTableEntry signalName = StringTable->insert("unitSignal");
   Con::evaluate("signal TestSimObject::unitSignal(%amount);");

   REQUIRE(source->addSignalListener(signalName, listener, StringTable->insert("handleSignal")));
   REQUIRE(source->hasSignal(signalName));

   KorkApi::ConsoleValue signalArgs[1];
   signalArgs[0] = KorkApi::ConsoleValue::makeString("payloadValue");
   source->triggerSignal(nullptr, signalName, 1, signalArgs);

   REQUIRE(listener->mSignalDispatchCount == 1);
   REQUIRE(listener->mLastSignalName == "unitSignal");
   REQUIRE(listener->mLastPayload == "payloadValue");

   REQUIRE(source->removeSignalListener(signalName, listener, StringTable->insert("handleSignal")));
   listener->mLastPayload.clear();
   source->triggerSignal(nullptr, signalName, 1, signalArgs);
   REQUIRE(listener->mSignalDispatchCount == 1);

   listener->deleteObject();
   source->deleteObject();
}

TEST_CASE("SimObject dump and tabComplete", "[SimObject]") {
   TestSimObject* obj = createRegisteredObject<TestSimObject>(uniqueName("dumpObj"));
   obj->getVMObject()->flags |= KorkApi::ModStaticFields | KorkApi::ModDynamicFields;
   obj->setDataField(StringTable->insert("testValue"), nullptr, "21");
   obj->setDataFieldDynamic(StringTable->insert("dynamicField"), nullptr, "dynValue", TypeString);

   gCapturedConsoleOutput.clear();
   Con::addConsumer(captureConsoleLine, &gCapturedConsoleOutput);
   obj->dump();
   Con::removeConsumer(captureConsoleLine, &gCapturedConsoleOutput);

   REQUIRE(gCapturedConsoleOutput.find("Static Fields:") != std::string::npos);
   REQUIRE(gCapturedConsoleOutput.find("testValue") != std::string::npos);
   REQUIRE(gCapturedConsoleOutput.find("Dynamic Fields:") != std::string::npos);
   REQUIRE(gCapturedConsoleOutput.find("dynamicField") != std::string::npos);
   REQUIRE(gCapturedConsoleOutput.find("Methods:") != std::string::npos);

   const char* completion = obj->tabComplete("bump", 4, true);
   REQUIRE(completion != nullptr);
   REQUIRE(std::string(completion) == "bumpCounter");

   obj->deleteObject();
}

TEST_CASE("setParentGroup updates membership", "[SimObject][SimGroup]") {
   SimGroup* groupA = createRegisteredObject<SimGroup>(uniqueName("parentA"));
   SimGroup* groupB = createRegisteredObject<SimGroup>(uniqueName("parentB"));
   TestSimObject* obj = createRegisteredObject<TestSimObject>(uniqueName("parented"));

   obj->getVMObject()->flags |= KorkApi::ModStaticFields;

   obj->setDataField(StringTable->insert("parentGroup"), nullptr, groupA->getName());
   REQUIRE(obj->getGroup() == groupA);
   REQUIRE(obj->isChildOfGroup(groupA));
   REQUIRE(obj->mOnGroupAddCount == 1);

   obj->setDataField(StringTable->insert("parentGroup"), nullptr, groupB->getName());
   REQUIRE(obj->getGroup() == groupB);
   REQUIRE(obj->isChildOfGroup(groupB));
   REQUIRE(obj->mOnGroupAddCount == 2);

   obj->deleteObject();
   groupA->deleteObject();
   groupB->deleteObject();
}

TEST_CASE("Namespace linking and unlinking", "[SimObject][Namespaces]") {
   TestSimObject* obj = createRegisteredObject<TestSimObject>(uniqueName("nsObj"));
   const StringTableEntry classNsName = StringTable->insert(uniqueName("ClassNS").c_str());
   const StringTableEntry superNsName = StringTable->insert(uniqueName("SuperNS").c_str());
   const StringTableEntry otherClassNsName = StringTable->insert(uniqueName("OtherClassNS").c_str());
   const StringTableEntry otherSuperNsName = StringTable->insert(uniqueName("OtherSuperNS").c_str());
   const StringTableEntry classSignal = StringTable->insert("classSignal");
   const StringTableEntry superSignal = StringTable->insert("superSignal");
   const StringTableEntry otherClassSignal = StringTable->insert("otherClassSignal");
   const StringTableEntry otherSuperSignal = StringTable->insert("otherSuperSignal");

   Con::evaluate((std::string("signal ") + classNsName + "::classSignal();").c_str());
   Con::evaluate((std::string("signal ") + superNsName + "::superSignal();").c_str());
   Con::evaluate((std::string("signal ") + otherClassNsName + "::otherClassSignal();").c_str());
   Con::evaluate((std::string("signal ") + otherSuperNsName + "::otherSuperSignal();").c_str());

   // Rebinding the class and super namespaces should update the visible signal set in each step.
   obj->setSuperClassNamespace(superNsName);
   obj->setClassNamespace(classNsName);
   REQUIRE(obj->getClassNamespace() == classNsName);
   REQUIRE(obj->getSuperClassNamespace() == superNsName);
   REQUIRE(obj->hasSignal(classSignal));
   REQUIRE(obj->hasSignal(superSignal));

   obj->setClassNamespace(otherClassNsName);
   REQUIRE(obj->getClassNamespace() == otherClassNsName);
   REQUIRE_FALSE(obj->hasSignal(classSignal));
   REQUIRE(obj->hasSignal(otherClassSignal));
   REQUIRE(obj->hasSignal(superSignal));

   obj->setSuperClassNamespace(otherSuperNsName);
   REQUIRE(obj->getSuperClassNamespace() == otherSuperNsName);
   REQUIRE_FALSE(obj->hasSignal(superSignal));
   REQUIRE(obj->hasSignal(otherSuperSignal));

   obj->deleteObject();
}

TEST_CASE("SimConsoleEvent copies payload and invokes object methods", "[SimConsoleEvent]") {
   TestSimObject* obj = createRegisteredObject<TestSimObject>(uniqueName("eventObj"));

   char payloadBuffer[] = "payloadBeforeCopy";
   KorkApi::ConsoleValue args[3];
   args[0] = KorkApi::ConsoleValue::makeString("recordPayload");
   args[1] = KorkApi::ConsoleValue::makeString("");
   args[2] = KorkApi::ConsoleValue::makeString(payloadBuffer);

   SimConsoleEvent evt(3, args, true);
   dStrcpy(payloadBuffer, "mutatedPayload");
   evt.process(obj);

   REQUIRE(obj->mConsoleCallCount == 1);
   REQUIRE(obj->mLastPayload == "payloadBeforeCopy");

   obj->deleteObject();
}

TEST_CASE("SimObject scheduling and event utilities", "[SimObject][Events]") {
   TestSimObject* obj = createRegisteredObject<TestSimObject>(uniqueName("scheduleObj"));
   const std::string objectName = std::string(obj->getName());
   const U32 startTime = Sim::getCurrentTime();

   const std::string scheduleExpr = objectName + ".schedule(5, \"recordPayload\", \"canceledPayload\");";
   const U32 eventId = dAtoi(evalConsole(scheduleExpr).c_str());
   REQUIRE(eventId != 0);
   REQUIRE(Sim::isEventPending(eventId));
   REQUIRE(Sim::getScheduleDuration(eventId) == 5);
   REQUIRE(Sim::getEventTimeLeft(eventId) == 5);
   REQUIRE(Sim::getTimeSinceStart(eventId) == 0);
   REQUIRE(dAtoi(evalConsole("getSimTime();").c_str()) == static_cast<S32>(Sim::getCurrentTime()));

   Sim::advanceToTime(startTime + 2);
   REQUIRE(Sim::isEventPending(eventId));
   REQUIRE(Sim::getEventTimeLeft(eventId) == 3);
   REQUIRE(Sim::getTimeSinceStart(eventId) == 2);
   REQUIRE(obj->mConsoleCallCount == 0);

   evalConsole("cancel(" + std::to_string(eventId) + ");");
   REQUIRE_FALSE(Sim::isEventPending(eventId));
   REQUIRE(Sim::getEventTimeLeft(eventId) == 0);
   REQUIRE(Sim::getTimeSinceStart(eventId) == 0);

   const std::string execScheduleExpr = objectName + ".schedule(3, \"recordPayload\", \"scheduledPayload\");";
   const U32 execEventId = dAtoi(evalConsole(execScheduleExpr).c_str());
   REQUIRE(execEventId != 0);
   REQUIRE(Sim::isEventPending(execEventId));

   Sim::advanceToTime(startTime + 5);
   REQUIRE_FALSE(Sim::isEventPending(execEventId));
   REQUIRE(obj->mConsoleCallCount == 1);
   REQUIRE(obj->mLastPayload == "scheduledPayload");

   obj->deleteObject();
}

TEST_CASE("SimObject set membership helpers", "[SimObject][SimSet]") {
   SimSet* set = createRegisteredObject<SimSet>(uniqueName("set"));
   TestSimObject* obj = createRegisteredObject<TestSimObject>(uniqueName("member"));

   REQUIRE_FALSE(obj->addToSet("does_not_exist"));
   REQUIRE(obj->addToSet(set->getName()));
   REQUIRE(set->size() == 1);
   REQUIRE(set->front() == obj);
   REQUIRE(set->last() == obj);
   REQUIRE(obj->getGroup() == nullptr);

   REQUIRE(obj->removeFromSet(set->getId()));
   REQUIRE(set->size() == 0);

   REQUIRE(obj->addToSet(set->getId()));
   REQUIRE(set->size() == 1);
   REQUIRE(obj->removeFromSet(set->getName()));
   REQUIRE(set->size() == 0);

   obj->deleteObject();
   set->deleteObject();
}

TEST_CASE("SimSet and SimGroup membership semantics", "[SimSet][SimGroup]") {
   SimSet* set = createRegisteredObject<SimSet>(uniqueName("plainSet"));
   SimGroup* groupA = createRegisteredObject<SimGroup>(uniqueName("groupA"));
   SimGroup* groupB = createRegisteredObject<SimGroup>(uniqueName("groupB"));
   TestSimObject* obj = createRegisteredObject<TestSimObject>(uniqueName("member"));

   REQUIRE(obj->getGroup() == nullptr);

   groupA->addObject(obj);
   REQUIRE(obj->getGroup() == groupA);
   REQUIRE(obj->isChildOfGroup(groupA));

   set->addObject(obj);
   REQUIRE(obj->getGroup() == groupA);
   REQUIRE(set->size() == 1);
   REQUIRE(set->front() == obj);

   groupB->addObject(obj);
   REQUIRE(obj->getGroup() == groupB);
   REQUIRE(obj->isChildOfGroup(groupB));
   REQUIRE_FALSE(obj->isChildOfGroup(groupA));
   REQUIRE(set->size() == 1);
   REQUIRE(set->findObject(obj->getName()) == obj);

   obj->deleteObject();
   set->deleteObject();
   groupA->deleteObject();
   groupB->deleteObject();
}

TEST_CASE("SimSet and SimGroup edge cases", "[SimSet][SimGroup]") {
   SimSet* set = createRegisteredObject<SimSet>(uniqueName("edgeSet"));
   SimGroup* group = createRegisteredObject<SimGroup>(uniqueName("edgeGroup"));
   TestSimObject* obj = createRegisteredObject<TestSimObject>(uniqueName("edgeMember"));

   REQUIRE_FALSE(set->reOrder(obj, nullptr));
   set->removeObject(obj);
   REQUIRE(set->size() == 0);
   set->popObject();
   REQUIRE(set->size() == 0);

   group->addObject(group);
   REQUIRE(group->size() == 0);
   REQUIRE(group->getGroup() == nullptr);

   group->removeObject(obj);
   REQUIRE(group->size() == 0);

   obj->deleteObject();
   set->deleteObject();
   group->deleteObject();
}

TEST_CASE("SimSet container operations and traversal", "[SimSet]") {
   SimSet* set = createRegisteredObject<SimSet>(uniqueName("set"));
   TestSimObject* first = createRegisteredObject<TestSimObject>(uniqueName("first"));
   TestSimObject* middle = createRegisteredObject<TestSimObject>(uniqueName("middle"));
   TestSimObject* last = createRegisteredObject<TestSimObject>(uniqueName("last"));
   TestSimObject* child = createRegisteredObject<TestSimObject>(uniqueName("child"));
   SimSet* nested = createRegisteredObject<SimSet>(uniqueName("nested"));

   REQUIRE(set->empty());
   // Build a mixed tree so the test can exercise ordering, nested traversal, and child lookup together.
   set->addObject(first);
   set->addObject(middle);
   set->addObject(last);
   set->addObject(nested);
   nested->addObject(child);

   REQUIRE(set->size() == 4);
   REQUIRE(set->front() == first);
   REQUIRE(set->first() == first);
   REQUIRE(set->last() == nested);
   REQUIRE(set->operator[](0) == first);
   REQUIRE(set->at(1) == middle);
   REQUIRE(set->containsType<SimSet>());
   REQUIRE(set->containsType<TestSimObject>());

   // The iterator should walk direct members and then descend into the nested set.
   std::vector<SimObject*> iterated;
   for (SimSetIterator itr(set); *itr; ++itr)
      iterated.push_back(*itr);

   REQUIRE(iterated.size() == 5);
   REQUIRE(iterated[0] == first);
   REQUIRE(iterated[1] == middle);
   REQUIRE(iterated[2] == last);
   REQUIRE(iterated[3] == nested);
   REQUIRE(iterated[4] == child);

   REQUIRE(set->reOrder(last, middle));
   REQUIRE(set->operator[](0) == first);
   REQUIRE(set->operator[](1) == last);
   REQUIRE(set->operator[](2) == middle);
   REQUIRE(set->operator[](3) == nested);

   set->pushObjectToBack(first);
   REQUIRE(set->last() == first);
   set->bringObjectToFront(first);
   REQUIRE(set->first() == first);

   set->popObject();
   REQUIRE(set->size() == 3);
   REQUIRE(set->last() == middle);

   set->pushObject(nested);
   REQUIRE(set->size() == 4);
   REQUIRE(set->last() == nested);

   middle->setInternalName("middleInternal");
   child->setInternalName("childInternal");
   REQUIRE(set->findObjectByInternalName(StringTable->insert("middleInternal"), false) == middle);
   REQUIRE(set->findObjectByInternalName(StringTable->insert("childInternal"), false) == nullptr);
   REQUIRE(set->findObjectByInternalName(StringTable->insert("childInternal"), true) == child);

   REQUIRE(set->findObject(first->getName()) == first);
   const std::string nestedPath = std::string(nested->getName()) + "/" + child->getName();
   REQUIRE(set->findObject(nestedPath.c_str()) == child);

   // This is the recursion check: without the deep flag, nested children are skipped.
   REQUIRE(first->mConsoleCallCount == 0);
   REQUIRE(child->mConsoleCallCount == 0);

   set->callOnChildren("bumpCounter", 0, nullptr, false);
   REQUIRE(first->mConsoleCallCount == 1);
   REQUIRE(middle->mConsoleCallCount == 1);
   REQUIRE(last->mConsoleCallCount == 1);
   REQUIRE(child->mConsoleCallCount == 0);

   set->callOnChildren("bumpCounter", 0, nullptr, true);
   REQUIRE(first->mConsoleCallCount == 2);
   REQUIRE(middle->mConsoleCallCount == 2);
   REQUIRE(last->mConsoleCallCount == 2);
   REQUIRE(child->mConsoleCallCount == 1);

   // `write()` should emit the set and its nested contents in the same structural shape.
   char buffer[4096] = {};
   MemStream stream(sizeof(buffer), buffer, true, true);
   set->write(stream, 0);
   REQUIRE(std::string(buffer).find("new SimSet(") != std::string::npos);
   REQUIRE(std::string(buffer).find(first->getName()) != std::string::npos);

   child->deleteObject();
   nested->deleteObject();
   first->deleteObject();
   middle->deleteObject();
   last->deleteObject();
   set->deleteObject();
}

TEST_CASE("SimSet deletion and clear behavior", "[SimSet]") {
   SimSet* set = createRegisteredObject<SimSet>(uniqueName("deleteSet"));
   TestSimObject* a = createRegisteredObject<TestSimObject>(uniqueName("a"));
   TestSimObject* b = createRegisteredObject<TestSimObject>(uniqueName("b"));

   set->addObject(a);
   set->addObject(b);
   SimObject* trackedA = a;
   SimObject* trackedB = b;
   a->registerReference(&trackedA);
   b->registerReference(&trackedB);

   set->clear();
   REQUIRE(set->size() == 0);
   REQUIRE(trackedA == a);
   REQUIRE(trackedB == b);
   REQUIRE(Sim::findObject(a->getId()) == a);
   REQUIRE(Sim::findObject(b->getId()) == b);

   set->addObject(a);
   set->addObject(b);
   set->deleteObjects();
   REQUIRE(trackedA == nullptr);
   REQUIRE(trackedB == nullptr);
   REQUIRE(set->size() == 0);

   set->deleteObject();
}

TEST_CASE("SimSet and SimGroup destruction semantics", "[SimSet][SimGroup]") {
   SimSet* set = createRegisteredObject<SimSet>(uniqueName("destructSet"));
   SimGroup* group = createRegisteredObject<SimGroup>(uniqueName("destructGroup"));
   TestSimObject* setMember = createRegisteredObject<TestSimObject>(uniqueName("setMember"));
   TestSimObject* groupMember = createRegisteredObject<TestSimObject>(uniqueName("groupMember"));

   SimObject* trackedSetMember = setMember;
   SimObject* trackedGroupMember = groupMember;
   setMember->registerReference(&trackedSetMember);
   groupMember->registerReference(&trackedGroupMember);
   const U32 groupMemberId = groupMember->getId();

   set->addObject(setMember);
   group->addObject(groupMember);

   // A SimSet only stores membership; a SimGroup owns direct children and deletes them with itself.
   set->deleteObject();
   REQUIRE(trackedSetMember != nullptr);
   REQUIRE(Sim::findObject(setMember->getId()) == setMember);
   REQUIRE(trackedGroupMember != nullptr);
   REQUIRE(Sim::findObject(groupMemberId) == groupMember);

   group->deleteObject();
   REQUIRE(trackedGroupMember == nullptr);
   REQUIRE(Sim::findObject(groupMemberId) == nullptr);

   setMember->deleteObject();
}

TEST_CASE("SimGroup hierarchy and recursive lookup", "[SimGroup]") {
   SimGroup* outer = createRegisteredObject<SimGroup>(uniqueName("outer"));
   SimGroup* inner = createRegisteredObject<SimGroup>(uniqueName("inner"));
   TestSimObject* child = createRegisteredObject<TestSimObject>(uniqueName("child"));
   TestSimObject* mover = createRegisteredObject<TestSimObject>(uniqueName("mover"));

   REQUIRE(outer->processArguments(0, nullptr));

   outer->addObject(inner);
   inner->addObject(child);
   REQUIRE(child->getGroup() == inner);
   REQUIRE(child->isChildOfGroup(inner));
   REQUIRE(child->isChildOfGroup(outer));
   REQUIRE(child->mOnGroupAddCount == 1);

   const std::string childPath = std::string(inner->getName()) + "/" + child->getName();
   REQUIRE(outer->findObject(childPath.c_str()) == child);
   REQUIRE(inner->findObject(child->getName()) == child);

   outer->addObject(mover);
   REQUIRE(mover->getGroup() == outer);
   inner->addObject(mover);
   REQUIRE(mover->getGroup() == inner);
   REQUIRE(mover->mOnGroupAddCount == 2);
   const std::string moverPath = std::string(inner->getName()) + "/" + mover->getName();
   REQUIRE(outer->findObject(moverPath.c_str()) == mover);

   const std::string directName = uniqueName("namedChild");
   TestSimObject* named = createRegisteredObject<TestSimObject>(directName);
   inner->addObject(named, 424242);
   REQUIRE(named->getId() == 424242);
   REQUIRE(inner->findObject(named->getName()) == named);
   REQUIRE(named->mOnGroupAddCount == 1);

   TestSimObject* renamed = createRegisteredObject<TestSimObject>(uniqueName("renameChild"));
   inner->addObject(renamed, "assignedName");
   REQUIRE(std::string(renamed->getName()) == "assignedName");
   REQUIRE(inner->findObject("assignedName") == renamed);
   REQUIRE(renamed->mOnGroupAddCount == 1);

   std::vector<SimObject*> groupIterated;
   for (SimGroupIterator itr(outer); *itr; ++itr)
      groupIterated.push_back(*itr);
   REQUIRE(groupIterated.size() == 5);
   REQUIRE(groupIterated[0] == inner);
   REQUIRE(groupIterated[1] == child);
   REQUIRE(groupIterated[2] == mover);
   REQUIRE(groupIterated[3] == named);
   REQUIRE(groupIterated[4] == renamed);

   inner->removeObject(renamed);
   REQUIRE(renamed->getGroup() == nullptr);
   REQUIRE(renamed->mOnGroupRemoveCount == 1);

   SimObject* trackedChild = child;
   SimObject* trackedInner = inner;
   child->registerReference(&trackedChild);
   inner->registerReference(&trackedInner);
   outer->deleteObject();
   REQUIRE(trackedChild == nullptr);
   REQUIRE(trackedInner == nullptr);
   renamed->deleteObject();
}

TEST_CASE("SimDataBlock serialization and modified-key behavior", "[SimDataBlock]") {
   SimDataBlock* baseBlock = createRegisteredObjectWithId<SimDataBlock>(uniqueName("baseBlock"), DataBlockObjectIdFirst);
   SimDataBlock* orderedBlock = createRegisteredObjectWithId<SimDataBlock>(uniqueName("orderedBlock"), DataBlockObjectIdFirst + 1);
   SimDataBlock* outOfRangeBlock = createRegisteredObjectWithId<SimDataBlock>(uniqueName("outOfRangeBlock"), DataBlockObjectIdLast + 100);
   SimDataBlockGroup* dataBlockGroup = dynamic_cast<SimDataBlockGroup*>(Sim::getDataBlockGroup());

   // The in-range ids participate in normal datablock ordering, while the out-of-range id exercises the alternate path.
   REQUIRE_FALSE(baseBlock->isClientOnly());
   REQUIRE_FALSE(orderedBlock->isClientOnly());
   REQUIRE(outOfRangeBlock->isClientOnly());
   REQUIRE(baseBlock->getModifiedKey() > 0);
   REQUIRE(orderedBlock->getModifiedKey() > 0);
   REQUIRE(outOfRangeBlock->getModifiedKey() > 0);
   REQUIRE(dataBlockGroup != nullptr);

   const S32 baseModifiedBefore = baseBlock->getModifiedKey();
   baseBlock->onStaticModified("internalName", "baseInternal");
   baseBlock->onStaticModified("canSaveDynamicFields", "0");
   REQUIRE(baseBlock->getModifiedKey() != baseModifiedBefore);

   baseBlock->setDataFieldDynamic(StringTable->insert("dbDynamic"), nullptr, "dbValue", TypeString);
   REQUIRE(std::string(baseBlock->getDataFieldDynamic(StringTable->insert("dbDynamic"), nullptr)) == "dbValue");

   char serializationBuffer[4096] = {};
   MemStream serializationStream(sizeof(serializationBuffer), serializationBuffer, true, true);
   baseBlock->write(serializationStream, 0);
   std::string serializationOutput = serializationBuffer;
   REQUIRE(serializationOutput.find("datablock SimDataBlock(") != std::string::npos);
   REQUIRE(serializationOutput.find("dbDynamic") != std::string::npos);

   baseBlock->setCanSaveDynamicFields(false);
   std::memset(serializationBuffer, 0, sizeof(serializationBuffer));
   MemStream serializationStream2(sizeof(serializationBuffer), serializationBuffer, true, true);
   baseBlock->write(serializationStream2, 0);
   serializationOutput = serializationBuffer;
   REQUIRE(serializationOutput.find("dbDynamic") == std::string::npos);

   char packedBuffer[4096] = {};
   MemStream packedStream(sizeof(packedBuffer), packedBuffer, true, true);
   outOfRangeBlock->write(packedStream, 0);
   std::string packedOutput = packedBuffer;
   REQUIRE(packedOutput.find("new SimDataBlock(") != std::string::npos);

   // `write()` is plain script serialization. `packData` / `unpackData` cover the transport-specific path.
   REQUIRE(baseBlock->preload(true, packedBuffer));
   baseBlock->packData(nullptr);
   baseBlock->unpackData(nullptr);

   const S32 baseKeyAfterStaticWrite = baseBlock->getModifiedKey();
   const S32 outOfRangeKeyBeforeWrite = outOfRangeBlock->getModifiedKey();
   outOfRangeBlock->onStaticModified("internalName", "outOfRangeInternal");
   outOfRangeBlock->onStaticModified("canSaveDynamicFields", "0");
   REQUIRE(outOfRangeBlock->getModifiedKey() != outOfRangeKeyBeforeWrite);
   REQUIRE(baseKeyAfterStaticWrite == baseBlock->getModifiedKey());

   dataBlockGroup->sort();
   S32 serverIndex = -1;
   S32 orderedIndex = -1;
   S32 index = 0;
   for (SimGroupIterator itr(dataBlockGroup); *itr; ++itr, ++index)
   {
      if (*itr == baseBlock)
         serverIndex = index;
      if (*itr == orderedBlock)
         orderedIndex = index;
   }
   REQUIRE(serverIndex >= 0);
   REQUIRE(orderedIndex >= 0);
   REQUIRE(serverIndex > orderedIndex);

   baseBlock->deleteObject();
   orderedBlock->deleteObject();
   outOfRangeBlock->deleteObject();
}
