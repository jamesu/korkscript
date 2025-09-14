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

#include "platform/platform.h"

#include "console/console.h"
#include "console/consoleObject.h"
#include "console/consoleTypes.h"

#include "core/stringTable.h"
#include "console/console.h"
#include "console/typeValidators.h"
#include "sim/simBase.h"

// TOFIX: add back in if networking needed
#define INITIAL_CRC_VALUE 0


AbstractClassRep *                 AbstractClassRep::classLinkList = NULL;
static AbstractClassRep::FieldList sg_tempFieldList;
U32                                AbstractClassRep::NetClassCount  [NetClassGroupsCount][NetClassTypesCount] = {{0, },};
U32                                AbstractClassRep::NetClassBitSize[NetClassGroupsCount][NetClassTypesCount] = {{0, },};

AbstractClassRep **                AbstractClassRep::classTable[NetClassGroupsCount][NetClassTypesCount];

U32                                AbstractClassRep::classCRC[NetClassGroupsCount] = {INITIAL_CRC_VALUE, };
bool                               AbstractClassRep::initialized = false;

//--------------------------------------
const AbstractClassRep::Field *AbstractClassRep::findField(StringTableEntry name) const
{
   for(U32 i = 0; i < (U32)mFieldList.size(); i++)
      if(mFieldList[i].pFieldname == name)
         return &mFieldList[i];

   return NULL;
}

//-----------------------------------------------------------------------------

AbstractClassRep* AbstractClassRep::findFieldRoot( StringTableEntry fieldName )
{
    // Find the field.
    const Field* pField = findField( fieldName );

    // Finish if not found.
    if ( pField == NULL )
        return NULL;

    // We're the root if we have no parent.
    if ( getParentClass() == NULL )
        return this;

    // Find the field root via the parent.
    AbstractClassRep* pFieldRoot = getParentClass()->findFieldRoot( fieldName );

    // We're the root if the parent does not have it else return the field root.
    return pFieldRoot == NULL ? this : pFieldRoot;
}

//-----------------------------------------------------------------------------

AbstractClassRep* AbstractClassRep::findContainerChildRoot( AbstractClassRep* pChild )
{
    // Fetch container child.
    AbstractClassRep* pContainerChildClass = getContainerChildClass( true );

    // Finish if not found.
    if ( pContainerChildClass == NULL )
        return NULL;

    // We're the root for the child if we have no parent.
    if ( getParentClass() == NULL )
        return this;

    // Find child in parent.
    AbstractClassRep* pParentContainerChildClass = getParentClass()->findContainerChildRoot( pChild );

    // We;re the root if the parent does not contain the child else return the container root.
    return pParentContainerChildClass == NULL ? this : pParentContainerChildClass;
}

//-----------------------------------------------------------------------------

AbstractClassRep* AbstractClassRep::findClassRep(const char* in_pClassName)
{
   AssertFatal(initialized,
      "AbstractClassRep::findClassRep() - Tried to find an AbstractClassRep before AbstractClassRep::initialize().");

   for (AbstractClassRep *walk = classLinkList; walk; walk = walk->nextClass)
      if (dStricmp(walk->getClassName(), in_pClassName) == 0)
         return walk;

   return NULL;
}

//--------------------------------------
void AbstractClassRep::registerClassRep(AbstractClassRep* in_pRep)
{
   AssertFatal(in_pRep != NULL, "AbstractClassRep::registerClassRep was passed a NULL pointer!");

#ifdef TORQUE_DEBUG  // assert if this class is already registered.
   for(AbstractClassRep *walk = classLinkList; walk; walk = walk->nextClass)
   {
      AssertFatal(dStricmp(in_pRep->mClassName, walk->mClassName) != 0,
         "Duplicate class name registered in AbstractClassRep::registerClassRep()");
   }
#endif

   in_pRep->nextClass = classLinkList;
   classLinkList = in_pRep;
}

//--------------------------------------

ConsoleObject* AbstractClassRep::create(const char* in_pClassName)
{
   AssertFatal(initialized,
      "AbstractClassRep::create() - Tried to create an object before AbstractClassRep::initialize().");

   const AbstractClassRep *rep = AbstractClassRep::findClassRep(in_pClassName);
   if(rep)
      return rep->create();

   AssertWarn(0, avar("Couldn't find class rep for dynamic class: %s", in_pClassName));
   return NULL;
}

//--------------------------------------
ConsoleObject* AbstractClassRep::create(const U32 groupId, const U32 typeId, const U32 in_classId)
{
   AssertFatal(initialized,
      "AbstractClassRep::create() - Tried to create an object before AbstractClassRep::initialize().");
   AssertFatal(in_classId < NetClassCount[groupId][typeId],
      "AbstractClassRep::create() - Class id out of range.");
   AssertFatal(classTable[groupId][typeId][in_classId] != NULL,
      "AbstractClassRep::create() - No class with requested ID type.");

   // Look up the specified class and create it.
   if(classTable[groupId][typeId][in_classId])
      return classTable[groupId][typeId][in_classId]->create();

   return NULL;
}

//--------------------------------------

void AbstractClassRep::registerClassWithVm(KorkApi::Vm* vm)
{
   if (mClassInfo.name == NULL)
   {
      mClassInfo.name = StringTable->insert(mClassName);
      mClassInfo.userPtr = this;
      mClassInfo.numFields = mFieldList.size();
      mClassInfo.fields = &mFieldList[0];
      // Create & Destroy
      mClassInfo.iCreate.CreateClassFn = [](void* user, KorkApi::Vm* vm, KorkApi::VMObject* object){
         AbstractClassRep* rep = static_cast<AbstractClassRep*>(user);
         ConsoleObject* obj = rep->create();
         return (void*)obj;
      };
      mClassInfo.iCreate.DestroyClassFn = [](void* user, KorkApi::Vm* vm, void* createdPtr){
         AbstractClassRep* rep = static_cast<AbstractClassRep*>(user);
         ConsoleObject* obj = static_cast<ConsoleObject*>(createdPtr);
         delete obj;
      };
      mClassInfo.iCreate.ProcessArgs = [](KorkApi::Vm* vm, KorkApi::VMObject* vmObject, const char* name, bool isDatablock, bool internalName, int argc, const char** argv){
         ConsoleObject* consoleObject = static_cast<ConsoleObject*>(vmObject->userPtr);
         SimObject* object = dynamic_cast<SimObject*>(consoleObject);
         object->setupVM(vm, vmObject);

         if (object->processArguments(argc, argv))
         {
            if(name && name[0])
            {
               if(!internalName)
                  object->assignName(name);
               else
                  object->setInternalName(name);

               // Set the original name
               //currentNewObject->setOriginalName( objectName );
            }
            
            if (!isDatablock)
            {
               object->setModStaticFields(true);
               object->setModDynamicFields(true);
            }
            
            vmObject->flags = object->getInternalFlags();
            return true;
         }
         else
         {
            object->setupVM(NULL, NULL);
         }
         
         return false;
      };
      mClassInfo.iCreate.AddObject = [](KorkApi::Vm* vm, KorkApi::VMObject* vmObject, bool placeAtRoot, U32 groupAddId) {
         ConsoleObject* consoleObject = static_cast<ConsoleObject*>(vmObject->userPtr);
         SimObject* currentNewObject = dynamic_cast<SimObject*>(consoleObject);

         if (!currentNewObject->isProperlyAdded())
         {
            if (!currentNewObject->registerObject())
            {
               return false;
            }
         }
         
         // Sync flags
         vmObject->flags = currentNewObject->getInternalFlags();
         
         // Are we dealing with a datablock?
         SimDataBlock *dataBlock = dynamic_cast<SimDataBlock *>(currentNewObject);
         static char errorBuffer[256];
         
         // If so, preload it.
         if(dataBlock && !dataBlock->preload(true, errorBuffer))
         {
            Con::errorf(ConsoleLogEntry::General, "%s: preload failed for %s: %s.", "",//getFileLine(ip-2),
                        currentNewObject->getName(), errorBuffer);
            return false;
         }
         
         // What group will we be added to, if any?
         SimGroup *grp = NULL;
         SimSet   *set = NULL;
         
         if(!placeAtRoot || !currentNewObject->getGroup())
         {
            {
               if(! placeAtRoot)
               {
                  // Otherwise just add to the requested group or set.
                  if(!Sim::findObject(groupAddId, grp))
                     Sim::findObject(groupAddId, set);
               }
               
               if(placeAtRoot)
               {
                  // Deal with the instantGroup if we're being put at the root or we're adding to a component.
                  const char *addGroupName = Con::getVariable("instantGroup");
                  if(!Sim::findObject(addGroupName, grp))
                     Sim::findObject(RootGroupId, grp);
               }
            }
            
            // If we didn't get a group, then make sure we have a pointer to
            // the rootgroup.
            if(!grp)
               Sim::findObject(RootGroupId, grp);
            
            // add to the parent group
            grp->addObject(currentNewObject);
            
            // add to any set we might be in
            if(set)
               set->addObject(currentNewObject);
         }

         return true;
      };
      mClassInfo.iCreate.GetId = [](KorkApi::VMObject* vmObject){
         ConsoleObject* consoleObject = static_cast<ConsoleObject*>(vmObject->userPtr);
         SimObject* object = dynamic_cast<SimObject*>(consoleObject);
         
         return (KorkApi::SimObjectId)object->getId();
      };
      // Custom fields
      mClassInfo.iCustomFields = {};
      mClassInfo.iCustomFields.IterateFields = [](KorkApi::Vm* vm, KorkApi::VMObject* vmObject, KorkApi::VMIterator& state, StringTableEntry* name){
         SimObject* object = NULL;
         
         if (state.userObject == NULL)
         {
            // New Iterator
            ConsoleObject* consoleObject = static_cast<ConsoleObject*>(vmObject->userPtr);
            object = dynamic_cast<SimObject*>(consoleObject);

            SimFieldDictionaryIterator itr(object->getFieldDictionary());
            itr.toVMItr(state);
         }
         else
         {
            // Advance iterator
            SimFieldDictionaryIterator itr(state);
            itr.operator++();
            itr.toVMItr(state);
         }

         if (state.internalEntry != NULL)
         {
            *name = ((SimFieldDictionary::Entry*)state.internalEntry)->slotName;
            return true;
         }
         else
         {
            *name = NULL;
            return false;
         }
      };
      mClassInfo.iCustomFields.GetFieldByIterator = [](KorkApi::Vm* vm, KorkApi::VMObject* object, KorkApi::VMIterator& state){
         KorkApi::ConsoleValue cv = KorkApi::ConsoleValue();

         if (state.userObject != NULL)
         {
            // Advance iterator
            SimFieldDictionaryIterator itr(state);
            if (itr.isValid())
            {
               return KorkApi::ConsoleValue::makeString(itr.getEntry()->value);
            }
         }

         return cv;
      };
      mClassInfo.iCustomFields.GetFieldByName = [](KorkApi::Vm* vm, KorkApi::VMObject* vmObject, const char* name){
         KorkApi::ConsoleValue cv = KorkApi::ConsoleValue();
         ConsoleObject* consoleObject = static_cast<ConsoleObject*>(vmObject->userPtr);
         SimObject* object = dynamic_cast<SimObject*>(consoleObject);
         const char* val = object->getDataFieldDynamic(StringTable->insert(name), NULL);
         return KorkApi::ConsoleValue::makeString(val);
      };
      mClassInfo.iCustomFields.SetFieldByName = [](KorkApi::Vm* vm, KorkApi::VMObject* vmObject, const char* name, KorkApi::ConsoleValue value){
         ConsoleObject* consoleObject = static_cast<ConsoleObject*>(vmObject->userPtr);
         SimObject* object = dynamic_cast<SimObject*>(consoleObject);
         object->setDataFieldDynamic(StringTable->insert(name), vm->valueAsString(value), NULL); // TODO
      };
      // Enumeration
      mClassInfo.iEnum = {};
      mClassInfo.iEnum.GetSize = [](KorkApi::VMObject* vmObject){
         ConsoleObject* consoleObject = static_cast<ConsoleObject*>(vmObject->userPtr);
         SimSet* object = dynamic_cast<SimSet*>(consoleObject);
         return object ? (U32)object->size() : (U32)0;
      };
      mClassInfo.iEnum.GetObjectAtIndex = [](KorkApi::VMObject* vmObject, U32 index){
         ConsoleObject* consoleObject = static_cast<ConsoleObject*>(vmObject->userPtr);
         SimSet* object = dynamic_cast<SimSet*>(consoleObject);
         return object ? object->at(index)->getVMObject() : (KorkApi::VMObject*)NULL;
      };
   }
   
   mLastRegisteredVmId = vm->registerClass(mClassInfo);
}

//--------------------------------------

S32 QSORT_CALLBACK ACRCompare(const void *aptr, const void *bptr)
{
   const AbstractClassRep *a = *((const AbstractClassRep **) aptr);
   const AbstractClassRep *b = *((const AbstractClassRep **) bptr);

   if(a->mClassType != b->mClassType)
      return a->mClassType - b->mClassType;
   return dStricmp(a->getClassName(), b->getClassName());
}

void AbstractClassRep::initialize()
{
   AssertFatal(!initialized, "Duplicate call to AbstractClassRep::initialize()!");
   Vector<AbstractClassRep *> dynamicTable(__FILE__, __LINE__);

   AbstractClassRep *walk;

   // Initialize namespace references...
   for (walk = classLinkList; walk; walk = walk->nextClass)
   {
      walk->mNamespace = Con::lookupNamespace(StringTable->insert(walk->getClassName()));
      //walk->mNamespace->mClassRep = walk;
   }

   // Initialize field lists... (and perform other console registration).
   for (walk = classLinkList; walk; walk = walk->nextClass)
   {
      // sg_tempFieldList is used as a staging area for field lists
      // (see addField, addGroup, etc.)
      sg_tempFieldList.setSize(0);

      walk->init();

      // So if we have things in it, copy it over...
      if (sg_tempFieldList.size() != 0)
      {
         if( !walk->mFieldList.size())
            walk->mFieldList = sg_tempFieldList;
         else
            destroyFieldValidators( sg_tempFieldList );
      }

      // And of course delete it every round.
      sg_tempFieldList.clear();
   }

   // Calculate counts and bit sizes for the various NetClasses.
   for (U32 group = 0; group < NetClassGroupsCount; group++)
   {
      U32 groupMask = 1 << group;

      // Specifically, for each NetClass of each NetGroup...
      for(U32 type = 0; type < NetClassTypesCount; type++)
      {
         // Go through all the classes and find matches...
         for (walk = classLinkList; walk; walk = walk->nextClass)
         {
            if(walk->mClassType == type && walk->mClassGroupMask & groupMask)
               dynamicTable.push_back(walk);
         }

         // Set the count for this NetGroup and NetClass
         NetClassCount[group][type] = dynamicTable.size();
         if(!NetClassCount[group][type])
            continue; // If no classes matched, skip to next.

         // Sort by type and then by name.
         dQsort((void *)dynamicTable.address(), dynamicTable.size(), sizeof(AbstractClassRep *), ACRCompare);

         // Allocate storage in the classTable
         classTable[group][type] = new AbstractClassRep*[NetClassCount[group][type]];

         // Fill this in and assign class ids for this group.
         for(U32 i = 0; i < NetClassCount[group][type];i++)
         {
            classTable[group][type][i] = dynamicTable[i];
            dynamicTable[i]->mClassId[group] = i;
         }

         // And calculate the size of bitfields for this group and type.
         NetClassBitSize[group][type] =
               getBinLog2(getNextPow2(NetClassCount[group][type] + 1));

         dynamicTable.clear();
      }
   }

   // Ok, we're golden!
   initialized = true;

}

void AbstractClassRep::destroyFieldValidators( AbstractClassRep::FieldList &mFieldList )
{
   for(S32 i = mFieldList.size()-1; i>=0; i-- )
   {
      TypeValidator **p = &mFieldList[i].validator;
      if( *p )
      {
         delete *p;
         *p = NULL;
      }
   }
}

//------------------------------------------------------------------------------
//-------------------------------------- ConsoleObject

char replacebuf[1024];
char* suppressSpaces(const char* in_pname)
{
    U32 i = 0;
    char chr;
    do
    {
        chr = in_pname[i];
        replacebuf[i++] = (chr != 32) ? chr : '_';
    } while(chr);

    return replacebuf;
}

void ConsoleObject::addGroup(const char* in_pGroupname, const char* in_pGroupDocs)
{
   // Remove spaces.
   char* pFieldNameBuf = suppressSpaces(in_pGroupname);

   // Append group type to fieldname.
   dStrcat(pFieldNameBuf, "_begingroup");

   // Create Field.
   AbstractClassRep::Field f;
   f.pFieldname   = StringTable->insert(pFieldNameBuf);
   f.pGroupname   = StringTable->insert(in_pGroupname);

   if(in_pGroupDocs)
      f.pFieldDocs   = StringTable->insert(in_pGroupDocs);
   else
      f.pFieldDocs   = NULL;

   f.type         = AbstractClassRep::StartGroupFieldType;
   f.elementCount = 0;
   f.groupExpand  = false;
   f.validator    = NULL;
   f.ovrSetValue  = NULL;
   f.ovrCopyValue = NULL;
   f.writeDataFn  = &defaultProtectedWriteFn;

   // Add to field list.
   sg_tempFieldList.push_back(f);
}

void ConsoleObject::endGroup(const char*  in_pGroupname)
{
   // Remove spaces.
   char* pFieldNameBuf = suppressSpaces(in_pGroupname);

   // Append group type to fieldname.
   dStrcat(pFieldNameBuf, "_endgroup");

   // Create Field.
   AbstractClassRep::Field f;
   f.pFieldname   = StringTable->insert(pFieldNameBuf);
   f.pGroupname   = StringTable->insert(in_pGroupname);
   f.pFieldDocs   = NULL;
   f.type         = AbstractClassRep::EndGroupFieldType;
   f.groupExpand  = false;
   f.validator    = NULL;
   f.ovrSetValue  = NULL;
   f.ovrCopyValue = NULL;
   f.writeDataFn  = &defaultProtectedWriteFn;
   f.elementCount = 0;

   // Add to field list.
   sg_tempFieldList.push_back(f);
}

void ConsoleObject::addField(const char*  in_pFieldname,
                       const U32 in_fieldType,
                       const dsize_t in_fieldOffset,
                       const char* in_pFieldDocs)
{
   addField(
      in_pFieldname,
      in_fieldType,
      in_fieldOffset,
      &defaultProtectedWriteFn,
      1,
      NULL,
      in_pFieldDocs);
}

void ConsoleObject::addField(const char*  in_pFieldname,
                       const U32 in_fieldType,
                       const dsize_t in_fieldOffset,
                       AbstractClassRep::WriteDataNotify in_writeDataFn,
                       const char* in_pFieldDocs)
{
   addField(
      in_pFieldname,
      in_fieldType,
      in_fieldOffset,
      in_writeDataFn,
      1,
      NULL,
      in_pFieldDocs);
}

void ConsoleObject::addField(const char*  in_pFieldname,
                       const U32 in_fieldType,
                       const dsize_t in_fieldOffset,
                       const U32 in_elementCount,
                       EnumTable *in_table,
                       const char* in_pFieldDocs)
{
   addField(
      in_pFieldname,
      in_fieldType,
      in_fieldOffset,
      &defaultProtectedWriteFn,
      in_elementCount,
      in_table,
      in_pFieldDocs);
}

void ConsoleObject::addField(const char*  in_pFieldname,
                       const U32 in_fieldType,
                       const dsize_t in_fieldOffset,
                       AbstractClassRep::WriteDataNotify in_writeDataFn,
                       const U32 in_elementCount,
                       EnumTable *in_table,
                       const char* in_pFieldDocs)
{
   AbstractClassRep::Field f;
   
   f.pFieldname   = StringTable->insert(in_pFieldname);
   f.pGroupname   = NULL;

   if(in_pFieldDocs)
      f.pFieldDocs   = StringTable->insert(in_pFieldDocs);
   else
      f.pFieldDocs   = NULL;

   f.type         = in_fieldType;
   f.offset       = in_fieldOffset;
   f.elementCount = in_elementCount;
   f.table        = in_table;
   f.validator    = NULL;
   
   f.ovrSetValue  = NULL;
   f.ovrCopyValue = NULL;
   f.writeDataFn  = in_writeDataFn;

   sg_tempFieldList.push_back(f);
}

void ConsoleObject::addProtectedField(const char*  in_pFieldname,
                       const U32 in_fieldType,
                       const dsize_t in_fieldOffset,
                       AbstractClassRep::SetValue in_setDataFn,
                       AbstractClassRep::CopyValue in_getDataFn,
                       const char* in_pFieldDocs)
{
   addProtectedField(
      in_pFieldname,
      in_fieldType,
      in_fieldOffset,
      in_setDataFn,
      in_getDataFn,
      &defaultProtectedWriteFn,
      1,
      NULL,
      in_pFieldDocs);
}

void ConsoleObject::addProtectedField(const char*  in_pFieldname,
                       const U32 in_fieldType,
                       const dsize_t in_fieldOffset,
                                      AbstractClassRep::SetValue in_setDataFn,
                                      AbstractClassRep::CopyValue in_getDataFn,
                       AbstractClassRep::WriteDataNotify in_writeDataFn,
                       const char* in_pFieldDocs)
{
   addProtectedField(
      in_pFieldname,
      in_fieldType,
      in_fieldOffset,
      in_setDataFn,
      in_getDataFn,
      in_writeDataFn,
      1,
      NULL,
      in_pFieldDocs);
}

void ConsoleObject::addProtectedField(const char*  in_pFieldname,
                       const U32 in_fieldType,
                       const dsize_t in_fieldOffset,
                                      AbstractClassRep::SetValue in_setDataFn,
                                      AbstractClassRep::CopyValue in_getDataFn,
                       const U32 in_elementCount,
                       EnumTable *in_table,
                       const char* in_pFieldDocs)
{
   addProtectedField(
      in_pFieldname,
      in_fieldType,
      in_fieldOffset,
      in_setDataFn,
      in_getDataFn,
      &defaultProtectedWriteFn,
      in_elementCount,
      in_table,
      in_pFieldDocs);
}

void ConsoleObject::addProtectedField(const char*  in_pFieldname,
                       const U32 in_fieldType,
                       const dsize_t in_fieldOffset,
                                      AbstractClassRep::SetValue in_setDataFn,
                                      AbstractClassRep::CopyValue in_getDataFn,
                       AbstractClassRep::WriteDataNotify in_writeDataFn,
                       const U32 in_elementCount,
                       EnumTable *in_table,
                       const char* in_pFieldDocs)
{
   AbstractClassRep::Field f;
   f.pFieldname   = StringTable->insert(in_pFieldname);
   f.pGroupname   = NULL;

   if(in_pFieldDocs)
      f.pFieldDocs   = StringTable->insert(in_pFieldDocs);
   else
      f.pFieldDocs   = NULL;

   f.type         = in_fieldType;
   f.offset       = in_fieldOffset;
   f.elementCount = in_elementCount;
   f.table        = in_table;
   f.validator    = NULL;

   f.ovrSetValue    = in_setDataFn;
   f.ovrCopyValue    = in_getDataFn;
   f.writeDataFn  = in_writeDataFn;

   sg_tempFieldList.push_back(f);
}

void ConsoleObject::addFieldV(const char*  in_pFieldname,
                       const U32 in_fieldType,
                       const dsize_t in_fieldOffset,
                       TypeValidator *v,
                       const char* in_pFieldDocs)
{
   AbstractClassRep::Field f;
   f.pFieldname   = StringTable->insert(in_pFieldname);
   f.pGroupname   = NULL;
   if(in_pFieldDocs)
      f.pFieldDocs   = StringTable->insert(in_pFieldDocs);
   else
      f.pFieldDocs   = NULL;
   f.type         = in_fieldType;
   f.offset       = in_fieldOffset;
   f.elementCount = 1;
   f.table        = NULL;
   f.ovrSetValue  = NULL;
   f.ovrCopyValue = NULL;
   f.writeDataFn  = &defaultProtectedWriteFn;
   f.validator    = v;
   v->fieldIndex  = sg_tempFieldList.size();

   sg_tempFieldList.push_back(f);
}

void ConsoleObject::addDepricatedField(const char *fieldName)
{
   AbstractClassRep::Field f;
   f.pFieldname   = StringTable->insert(fieldName);
   f.pGroupname   = NULL;
   f.pFieldDocs   = NULL;
   f.type         = AbstractClassRep::DepricatedFieldType;
   f.offset       = 0;
   f.elementCount = 0;
   f.table        = NULL;
   f.validator    = NULL;
   f.ovrSetValue  = NULL;
   f.ovrCopyValue = NULL;
   f.writeDataFn  = &defaultProtectedWriteFn;

   sg_tempFieldList.push_back(f);
}


bool ConsoleObject::removeField(const char* in_pFieldname)
{
   for (U32 i = 0; i < (U32)sg_tempFieldList.size(); i++) {
      if (dStricmp(in_pFieldname, sg_tempFieldList[i].pFieldname) == 0) {
         sg_tempFieldList.erase(i);
         return true;
      }
   }

   return false;
}

//--------------------------------------
void ConsoleObject::initPersistFields()
{
}

//--------------------------------------
void ConsoleObject::consoleInit()
{
}

ConsoleObject::~ConsoleObject()
{
}

//--------------------------------------
AbstractClassRep* ConsoleObject::getClassRep() const
{
   return NULL;
}


void AbstractClassRep::registerWithVM(KorkApi::Vm* vm)
{
   for (AbstractClassRep* walk = classLinkList; walk; walk = walk->nextClass)
   {
      walk->registerClassWithVm(vm);
   }
}

