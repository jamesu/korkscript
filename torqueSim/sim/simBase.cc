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
#include "platform/platformString.h"
#include "sim/simBase.h"
#include "core/stringTable.h"
#include "console/console.h"
#include "core/fileStream.h"
#include "core/memStream.h"

#include "console/typeValidators.h"
#include "console/consoleTypes.h"
#include "sim/dynamicTypes.h"

extern KorkApi::Vm* sVM;

namespace Sim
{
   ImplementNamedGroup(ScriptClassGroup)

}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

// BEGIN T2D BLOCK
SimFieldDictionary::Entry *SimFieldDictionary::mFreeList = NULL;

static Chunker<SimFieldDictionary::Entry> fieldChunker;

SimFieldDictionary::Entry *SimFieldDictionary::allocEntry()
{
   if(mFreeList)
   {
      Entry *ret = mFreeList;
      mFreeList = ret->next;
      return ret;
   }
   else
      return fieldChunker.alloc();
}

void SimFieldDictionary::freeEntry(SimFieldDictionary::Entry *ent)
{
   ent->next = mFreeList;
   mFreeList = ent;
}

SimFieldDictionary::SimFieldDictionary()
{
   for(U32 i = 0; i < HashTableSize; i++)
      mHashTable[i] = 0;

   mVersion = 0;
}

SimFieldDictionary::~SimFieldDictionary()
{
   for(U32 i = 0; i < HashTableSize; i++)
   {
      for(Entry *walk = mHashTable[i]; walk;)
      {
         Entry *temp = walk;
         walk = temp->next;

         free(temp->value);
         freeEntry(temp);
      }
   }
}

void SimFieldDictionary::setFieldValue(StringTableEntry slotName, const char *value, U32 typeId)
{
   U32 bucket = HashPointer(slotName) % HashTableSize;
   Entry **walk = &mHashTable[bucket];
   while(*walk && (*walk)->slotName != slotName)
      walk = &((*walk)->next);

   Entry *field = *walk;
   if(!*value && typeId == UINT_MAX)
   {
      if(field)
      {
         mVersion++;

         free(field->value);
         *walk = field->next;
         freeEntry(field);
      }
   }
   else
   {
      if(field)
      {
         free(field->value);
         field->value = strdup(value);
         if (typeId != UINT_MAX)
         {
            field->enforcedTypeId = typeId;
         }
      }
      else
      {
         mVersion++;

         field = allocEntry();
         field->value = strdup(value);
         field->slotName = slotName;
         field->next = NULL;
         if (typeId != UINT_MAX)
         {
            field->enforcedTypeId = typeId;
         }
         else
         {
            field->enforcedTypeId = 0;
         }
         *walk = field;
      }
   }
}

const char *SimFieldDictionary::getFieldValue(StringTableEntry slotName, U32* typeId)
{
   U32 bucket = HashPointer(slotName) % HashTableSize;
   
   for(Entry *walk = mHashTable[bucket];walk;walk = walk->next)
   {
      if(walk->slotName == slotName)
      {
         if (typeId)
         {
            *typeId = walk->enforcedTypeId;
         }
         return walk->value;
      }
   }
   return NULL;
}


//-----------------------------------------------------------------------------

SimObject::SimObject( const U8 namespaceLinkMask ) : mNSLinkMask( namespaceLinkMask )
{
   objectName               = NULL;
   mInternalName            = NULL;
   nextNameObject           = (SimObject*)-1;
   nextManagerNameObject    = (SimObject*)-1;
   nextIdObject             = NULL;
   mId                      = 0;
   mIdString                = StringTable->EmptyString;
   mGroup                   = 0;
   mVMNameSpace             = NULL;
   mNotifyList              = NULL;
   mTypeMask                = 0;
   mScriptCallbackGuard     = 0;
   mFieldDictionary         = NULL;
   mCanSaveFieldDictionary    = true;
   mClassName               = NULL;
   mSuperClassName          = NULL;
   mProgenitorFile          = Con::getCurrentCodeBlockFullPath();
   mPeriodicTimerID         = 0;
   mSimFlags = 0;
   vmObject = NULL;
   vm = NULL;
}


void SimFieldDictionary::assignFrom(SimFieldDictionary *dict)
{
   mVersion++;

   for(U32 i = 0; i < HashTableSize; i++)
      for(Entry *walk = dict->mHashTable[i];walk; walk = walk->next)
         setFieldValue(walk->slotName, walk->value);
}

bool compareEntries(const SimFieldDictionary::Entry* fa,
                    const SimFieldDictionary::Entry* fb)
{
    return dStricmp(fa->slotName, fb->slotName) < 0;
}

void SimFieldDictionary::writeFields(SimObject *obj, Stream &stream, U32 tabStop)
{
   const AbstractClassRep::FieldList &list = obj->getFieldList();
   std::vector<Entry *> flist;

   for(U32 i = 0; i < HashTableSize; i++)
   {
      for(Entry *walk = mHashTable[i];walk; walk = walk->next)
      {
         // make sure we haven't written this out yet:
         U32 i;
         for(i = 0; i < (U32)list.size(); i++)
            if(list[i].pFieldname == walk->slotName)
               break;

         if(i != list.size())
            continue;

         if (!obj->writeField(walk->slotName, walk->value))
            continue;

         flist.push_back(walk);
      }
   }

   // Sort Entries to prevent version control conflicts
   std::sort(flist.begin(),flist.end(),compareEntries);

   // Save them out
   for(std::vector<Entry *>::iterator itr = flist.begin(); itr != flist.end(); itr++)
   {
      U32 nBufferSize = (dStrlen( (*itr)->value ) * 2) + dStrlen( (*itr)->slotName ) + 16;
      std::vector<char> expandedBufferV( nBufferSize );
      char* expandedBuffer = expandedBufferV.data();

      stream.writeTabs(tabStop+1);

      dSprintf(expandedBuffer, nBufferSize, "%s = \"", (*itr)->slotName);
      expandEscape((char*)expandedBuffer + dStrlen(expandedBuffer), (*itr)->value);
      dStrcat(expandedBuffer, "\";\r\n");

      stream.write(dStrlen(expandedBuffer),expandedBuffer);
   }
}

void SimFieldDictionary::printFields(SimObject *obj)
{
   const AbstractClassRep::FieldList &list = obj->getFieldList();
   char expandedBuffer[4096];
   std::vector<Entry *> flist;

   for(U32 i = 0; i < HashTableSize; i++)
   {
      for(Entry *walk = mHashTable[i];walk; walk = walk->next)
      {
         // make sure we haven't written this out yet:
         U32 i;
         for(i = 0; i < (U32)list.size(); i++)
            if(list[i].pFieldname == walk->slotName)
               break;

         if(i != list.size())
            continue;

         flist.push_back(walk);
      }
   }
   std::sort(flist.begin(),flist.end(),compareEntries);

   for(std::vector<Entry *>::iterator itr = flist.begin(); itr != flist.end(); itr++)
   {
      dSprintf(expandedBuffer, sizeof(expandedBuffer), "  %s = \"", (*itr)->slotName);
      expandEscape(expandedBuffer + dStrlen(expandedBuffer), (*itr)->value);
      Con::printf("%s\"", expandedBuffer);
   }
}

//------------------------------------------------------------------------------

SimFieldDictionaryIterator::SimFieldDictionaryIterator(SimFieldDictionary * dictionary)
{
   mDictionary = dictionary;
   mHashIndex = -1;
   mEntry = 0;
   operator++();
}

SimFieldDictionaryIterator::SimFieldDictionaryIterator(KorkApi::VMIterator& itr)
{
   mDictionary = (SimFieldDictionary*)itr.userObject;
   mHashIndex = itr.count;
   mEntry = (SimFieldDictionary::Entry*)itr.internalEntry;
   if (mHashIndex == -1)
   {
      operator++();
   }
}

SimFieldDictionary::Entry* SimFieldDictionaryIterator::operator++()
{
   if(!mDictionary)
      return(mEntry);

   if(mEntry)
      mEntry = mEntry->next;

   while(!mEntry && (mHashIndex < (SimFieldDictionary::HashTableSize-1)))
      mEntry = mDictionary->mHashTable[++mHashIndex];

   return(mEntry);
}

SimFieldDictionary::Entry* SimFieldDictionaryIterator::operator*()
{
   return(mEntry);
}


void SimFieldDictionaryIterator::toVMItr(KorkApi::VMIterator& itr)
{
   itr.userObject = mDictionary;
   itr.count = mHashIndex;
   itr.internalEntry = mEntry;
}

// END T2D BLOCK


// BEGIN T2D BLOCK

//-----------------------------------------------------------------------------

IMPLEMENT_CONOBJECT(SimObject);

namespace Sim
{
    extern U32 gNextObjectId;
    extern SimIdDictionary *gIdDictionary;
    extern SimManagerNameDictionary *gNameDictionary;
    extern void cancelPendingEvents(SimObject *obj);
}

//---------------------------------------------------------------------------

KorkApi::NamespaceId SimObject::getNamespace()
{
   return sVM->getObjectNamespace(vmObject);
}

void SimObject::assignDynamicFieldsFrom(SimObject* parent)
{
   if(parent->mFieldDictionary)
   {
      if( mFieldDictionary == NULL )
         mFieldDictionary = new SimFieldDictionary;
      mFieldDictionary->assignFrom(parent->mFieldDictionary);
   }
}

void SimObject::assignFieldsFrom(SimObject *parent)
{
   getVM()->assignFieldsFromTo(parent->getVMObject(), getVMObject());
}

bool SimObject::writeField(StringTableEntry fieldname, const char* value)
{
   // Don't write empty fields.
   if (!value || !*value)
      return false;

   // Don't write ParentGroup
   if( fieldname == StringTable->insert("parentGroup") )
      return false;


   return true;
}

void SimObject::writeFields(Stream &stream, U32 tabStop)
{
   const AbstractClassRep::FieldList &list = getFieldList();
   std::string valCopy;
   std::vector<char> expandedBuffer;
   
   for(U32 i = 0; i < (U32)list.size(); i++)
   {
      const AbstractClassRep::Field* f = &list[i];

      if( f->type == AbstractClassRep::DepricatedFieldType ||
          f->type == AbstractClassRep::StartGroupFieldType ||
          f->type == AbstractClassRep::EndGroupFieldType) continue;

      // Fetch fieldname.
      StringTableEntry fieldName = StringTable->insert( f->pFieldname );

      // Fetch element count.
      const S32 elementCount = f->elementCount;

      // Skip if the field should not be written.
      // For now, we only deal with non-array fields.
      if (  elementCount == 1 &&
            f->writeDataFn != NULL &&
            f->writeDataFn( this, fieldName ) == false )
            continue;

      for(U32 j = 0; S32(j) < elementCount; j++)
      {
         char array[8];
         dSprintf( array, 8, "%d", j );
         const char *val = getDataField(fieldName, array );

         // Make a copy for the field check.
         if (!val)
            continue;

         valCopy = val;

         if (!writeField(fieldName, valCopy.c_str()))
            continue;

         size_t expandedBufferSize = ( (valCopy.size() + 1)  * 2 ) + 32;
         expandedBuffer.resize(expandedBufferSize);
         
         if(f->elementCount == 1)
         {
            dSprintf(expandedBuffer.data(), expandedBufferSize, "%s = \"", f->pFieldname);
         }
         else
         {
            dSprintf(expandedBuffer.data(), expandedBufferSize, "%s[%d] = \"", f->pFieldname, j);
         }
         
         // detect and collapse relative path information
         /*char fnBuf[1024]; // Not implemented YET
         if (f->type == TypeFilename)
         {
            Con::collapsePath(fnBuf, 1024, val);
            val = fnBuf;
         }*/

         expandEscape(expandedBuffer.data() + strlen(expandedBuffer.data()), val);
         dStrcat(expandedBuffer.data(), "\";\r\n");

         stream.writeTabs(tabStop);
         stream.write((U32)strlen(expandedBuffer.data()), expandedBuffer.data());
      }
   }    
   if(mFieldDictionary && mCanSaveFieldDictionary)
   {
      mFieldDictionary->writeFields(this, stream, tabStop);
   }
}

void SimObject::write(Stream &stream, U32 tabStop, U32 flags)
{
   // Only output selected objects if they want that.
   if((flags & SelectedOnly) && !isSelected())
      return;

   stream.writeTabs(tabStop);
   char buffer[1024];
   dSprintf(buffer, sizeof(buffer), "new %s(%s) {\r\n", getClassName(), getName() ? getName() : "");
   stream.write(dStrlen(buffer), buffer);
   writeFields(stream, tabStop + 1);
   stream.writeTabs(tabStop);
   stream.write(4, "};\r\n");
}

bool SimObject::save(const char* pcFileName, bool bOnlySelected)
{
   static const char *beginMessage = "//--- OBJECT WRITE BEGIN ---";
   static const char *endMessage = "//--- OBJECT WRITE END ---";
   FileStream stream;
   MemStream f(0, NULL, false, false);
   std::vector<char> w;

   if (stream.open(pcFileName, FileStream::Read))
   {
      w.resize(stream.getStreamSize());
      f = MemStream(stream.getStreamSize(), w.data(), true, false);
      stream.read(stream.getStreamSize(), w.data());
      stream.close();
   }

   // check for flags <selected, ...>
   U32 writeFlags = 0;
   if(bOnlySelected)
      writeFlags |= SimObject::SelectedOnly;

   if(!stream.open(pcFileName, FileStream::Write)) 
      return false;

   char docRoot[256];
   char modRoot[256];

   dStrcpy(docRoot, pcFileName);
   char *p = dStrrchr(docRoot, '/');
   if (p) *++p = '\0';
   else  docRoot[0] = '\0';

   dStrcpy(modRoot, pcFileName);
   p = dStrchr(modRoot, '/');
   if (p) *++p = '\0';
   else  modRoot[0] = '\0';

   Con::setVariable("$DocRoot", docRoot);
   Con::setVariable("$ModRoot", modRoot);

   const char *buffer;
   U8 lineBuffer[4096];
   while(f.getStatus() == Stream::Ok)
   {
      buffer = (const char *) lineBuffer;
      f.readLine(lineBuffer, sizeof(lineBuffer));
      if(!dStrcmp(buffer, beginMessage))
         break;
      stream.write(dStrlen(buffer), buffer);
      stream.write(2, "\r\n");
   }
   stream.write(dStrlen(beginMessage), beginMessage);
   stream.write(2, "\r\n");
   write(stream, 0, writeFlags);
   stream.write(dStrlen(endMessage), endMessage);
   stream.write(2, "\r\n");
   while(f.getStatus() == Stream::Ok)
   {
      buffer = (const char *) lineBuffer;
      f.readLine(lineBuffer, sizeof(lineBuffer));
      if(!dStrcmp(buffer, endMessage))
         break;
   }
   while(f.getStatus() == Stream::Ok)
   {
      buffer = (const char *) lineBuffer;
      f.readLine(lineBuffer, sizeof(lineBuffer));
      stream.write(dStrlen(buffer), buffer);
      stream.write(2, "\r\n");
   }

   Con::setVariable("$DocRoot", NULL);
   Con::setVariable("$ModRoot", NULL);

   return true;

}

ConsoleFunctionGroupBegin( SimFunctions, "Sim Functions");

/*! Use the nameToID function to convert an object name into an object ID.
 
 Helper function for those odd cases where a string will not covert properly, but generally this can be replaced with a statement like: ("someName")
 @param objectName A string containing the name of an object.
 @return a positive non-zero value if the name corresponds to an object, or a -1 if it does not.
 
 @boundto
 Sim::findObject
 */
ConsoleFunction( nameToID, S32, 2, 2, "string objectName" )
{
   SimObject *obj = Sim::findObject(argv[1]);
   if(obj)
      return obj->getId();
   else
      return -1;
}

/*! check if the name or ID specified is a valid object.
 
 @param handle A name or ID of a possible object.
 @return true if handle refers to a valid object, false otherwise
 
 @boundto
 Sim::findObject
 */
ConsoleFunction(isObject, bool, 2, 2, "handle")
{
   if (!dStrcmp(argv[1], "0") || !dStrcmp(argv[1], ""))
      return false;
   else
      return (Sim::findObject(argv[1]) != NULL);
}

/*! cancel a previously scheduled event
 
 @param eventID The numeric ID of a previously scheduled event.
 @return No return value.
 @sa getEventTimeLeft, getScheduleDuration, getTimeSinceStart, isEventPending, schedule, SimObject::schedule
 
 @boundto
 Sim::cancelEvent
 */
ConsoleFunction(cancel, void , 2, 2, "eventID")
{
   Sim::cancelEvent(dAtoi(argv[1]));
}


/*!   See if the event associated with eventID is still pending.
 
 When an event passes, the eventID is removed from the event queue, becoming invalid, so there is no discnerable difference between a completed event and a bad event ID.
 @param eventID The numeric ID of a previously scheduled event.
 @return true if this event is still outstanding and false if it has passed or eventID is invalid.
 
 @par Example
 @code
 $Game::Schedule = schedule($Game::EndGamePause * 1000, 0, "onCyclePauseEnd");
 if( isEventPending($Game::Schedule) )  echo("got a pending event");
 @endcode
 
 @sa cancel, getEventTimeLeft, getScheduleDuration, getTimeSinceStart, schedule, SimObject::schedule
 
 @boundto
 Sim::isEventPending
 */
ConsoleFunction(isEventPending, bool, 2, 2, "eventID")
{
   return Sim::isEventPending(dAtoi(argv[1]));
}

/*!
 Determines how much time remains until the event specified by eventID occurs.
 
 @param eventID The numeric ID of a previously scheduled event.
 @return a non-zero integer value equal to the milliseconds until the event specified by eventID will occur. However, if eventID is invalid, or the event has passed, this function will return zero.
 @sa cancel, getScheduleDuration, getTimeSinceStart, isEventPending, schedule, SimObject::schedule
 
 @boundto
 Sim::getEventTimeLeft
 */
ConsoleFunction(getEventTimeLeft, S32, 2, 2, "eventID")
{
   return Sim::getEventTimeLeft(dAtoi(argv[1]));
}

/*!
 Determines how long the event associated with eventID was scheduled for.
 
 @param eventID The numeric ID of a previously scheduled event.
 @return a non-zero integer value equal to the milliseconds used in the schedule call that created this event. However, if eventID is invalid, this function will return zero.
 @sa cancel, getEventTimeLeft, getTimeSinceStart, isEventPending, schedule, SimObject::schedule
 
 @boundto
 Sim::getScheduleDuration
 */
ConsoleFunction(getScheduleDuration, S32, 2, 2, "eventID")
{
   S32 ret = Sim::getScheduleDuration(dAtoi(argv[1]));
   return ret;
}

/*!
 Determines how much time has passed since the event specified by eventID was scheduled.
 
 @param eventID The numeric ID of a previously scheduled event.
 @return a non-zero integer value equal to the milliseconds that have passed since this event was scheduled. However, if eventID is invalid, or the event has passed, this function will return zero.
 @sa cancel, getEventTimeLeft, getScheduleDuration, isEventPending, schedule, SimObject::schedule
 
 @boundto
 Sim::getTimeSinceStart
 */
ConsoleFunction(getTimeSinceStart, S32, 2, 2, "eventID")
{
   S32 ret = Sim::getTimeSinceStart(dAtoi(argv[1]));
   return ret;
}

/*!
 Schedule "functionName" to be executed with optional arguments at time t (specified in milliseconds) in the future.
 
 This function may be associated with an object ID or not. If it is associated with an object ID and the object is deleted prior to this event occurring, the event is automatically canceled.
 @param t The time to wait (in milliseconds) before executing functionName.
 @param objID An optional ID to associate this event with.
 @param functionName An unadorned (flat) function name.
 @param arg0, .. , argN - Any number of optional arguments to be passed to functionName.
 @return a non-zero integer representing the event ID for the scheduled event.
 
 @par Example
 @code
 $Game::Schedule = schedule($Game::EndGamePause * 1000, 0, "onCyclePauseEnd");
 @endcode
 
 @sa cancel, getEventTimeLeft, getScheduleDuration, getTimeSinceStart, isEventPending, SimObject::schedule
 
 @boundto
 Sim::postEvent
 */
ConsoleFunction(schedule, S32, 4, 0, "t , objID || 0 , functionName, arg0, ... , argN" )
{
   U32 timeDelta = U32(dAtof(argv[1]));
   SimObject *refObject = Sim::findObject(argv[2]);
   if(!refObject)
   {
      if(argv[2][0] != '0')
         return 0;
      
      refObject = Sim::getRootGroup();
   }
   SimConsoleEvent *evt = new SimConsoleEvent(argc - 3, argv + 3, false);
   
   S32 ret = Sim::postEvent(refObject, evt, Sim::getCurrentTime() + timeDelta);
   // #ifdef DEBUG
   //    Con::printf("ref %s schedule(%s) = %d", argv[2], argv[3], ret);
   //    Con::executef(1, "backtrace");
   // #endif
   return ret;
}

ConsoleFunctionGroupEnd( SimFunctions );


/*! save this object to a specified file
 @param fileName the file to save to
 @param selectedOnly seems to be for editors to set.  not sure how to mark anything as "selected"
 @return false if file could not be opened; true otherwise
 
 @see FileObject::writeObject, addFieldFilter, removeFieldFilter
 */
ConsoleMethod(SimObject, save, bool, 3, 4, "fileName, [selectedOnly]?")
{
   bool bSelectedOnly   =   false;
   if(argc > 3)
      bSelectedOnly   = dAtob(argv[3]);
   
   const char* filename = NULL;
   
   filename = argv[2];
   
   if(filename == NULL || *filename == 0)
      return false;
   
   return object->save(filename, bSelectedOnly);
   
}

/*! Set the objects name field.
 @param newName name for objects
 @return no return value
 
 Now the object can be invoked by this name.
 This is different than tracking an object by a variable, such as `%%myObject` or `$myObject`.
 
 Only one object can have a specific name.  Setting a second object
 with this name will remove the name from the former object.
 
 Note not to confuse this with the `internalName` which is a name for grouping purposes.
 
 @par Example
 @code
 %obj = new SimObject();
 %obj.setName("MyName");
 
 // these are now equivalent
 %obj.save();
 MyName.save();
 @endcode
 
 @par Caveat
 You can not access the name directly.  That is, you can not access `%%object.name`.
 If you do set `%%object.name` you will only succeed in creating a dynamic field named
 `name` -- an unrelated field to the actual object's name.
 
 @par Example
 @code
 SimObject("example");
 echo(example.getName());
 > example
 
 // warning! the field `name` does not exist yet
 echo(example.name);
 >
 
 // warning! this will fail to change the name!
 // it will also not warn you as it is legal syntax
 %example.name = "newExample";
 echo(%example.getName());
 > example
 
 echo(%example.name);
 > newExample
 @endcode
 
 @see setName, getId
 */
ConsoleMethod(SimObject, setName, void, 3, 3, "newName")
{
   object->assignName(argv[2]);
}

/*! Returns the name of the object
 @return the "global" name
 
 See setName() for a description of the name field.
 
 Note not to confuse this with the `internalName` which is a name for grouping purposes.
 
 @par Example
 @code
 %example = new SimObject();
 %example.setName("myObject");
 
 // now we can reference our object with variables and with its name
 %example.getId();
 > 160
 
 myObject.getId();
 > 160
 @endcode
 
 @Caveats
 See setName() for caveats.
 
 @see setName, getId
 */
ConsoleMethod(SimObject, getName, const char*, 2, 2, "")
{
   const char *ret = object->getName();
   return ret ? ret : "";
}

/*! Returns the engine class of this object such as `SimObject` or `SceneObject`
 @return class name
 
 Note that this method is defined in SimObject but is inherited by subclasses of SimObject.
 Subclasses will return the correct subclass name.
 
 Note also, getClassName() is not related to an object's `class` field!  The `class` field
 is a scripting concept that provides a "namespace" to look for user-defined functions (see getClassNamespace()).
 
 @par Example
 @code
 %example = new SimObject()
 {
 class = MyScope;
 };
 
 echo(%example.getClassName());
 > SimObject
 echo(%example.class);
 > MyScope
 @endcode
 */
ConsoleMethod(SimObject, getClassName, const char*, 2, 2, "")
{
   const char *ret = object->getClassName();
   return ret ? ret : "";
}

/*! Return the value of any field.
 This can be a static ("built-in") field or a dynamic ("add-on") field.
 
 Normally, you would get a field directly as `%%object.field`.
 However, in some cases you may want to use getFieldValue().  For instance,
 suppose you allow the field name to be passed into a function.  You can still
 get that field with `%%object.getFieldValue(%%field)`.
 
 @param fieldName the name of the field
 @return the value of the field
 
 @par Example
 @code
 // create a SimObject and set its 'class' field for our example
 %example = new SimObject()
 {
 class = "MyClass";
 }
 
 // 'class' is a static "built-in" field.  retrieve it directly and with getFieldValue()
 echo(%example.class);
 > MyClass
 
 echo(%example.getFieldValue(class));
 > MyClass
 
 // set a dynamic "add-on" field
 %example.myField = "myValue";
 echo(%example.myField);
 > myValue
 
 echo(%example.getFieldValue(myField));
 > myValue
 @endcode
 */
ConsoleMethod(SimObject, getFieldValue, const char*, 3, 3, "fieldName")
{
   const char *fieldName = StringTable->insert( argv[2] );
   return object->getDataField( fieldName, NULL );
}

/*! Set the value of any field.
 This can be a static ("built-in") field or a dynamic ("add-on") field.
 
 Normally, you would set a field directly as `%%object.field = value`.
 However, in some cases you may want to use setFieldValue().  For instance,
 suppose you allow the field name to be passed into a function.  You can still
 set that field with `%%object.setFieldValue(%field, "myValue")`.
 
 @param fieldName the name of the field to set
 @param value the value to set
 @return always returns true
 
 @par Example
 @code
 // create a SimObject
 %example = new SimObject();
 
 // 'class' is a static "built-in" field.  set it directly and with setFieldValue()
 echo(%example.class);
 >
 
 %example.class = "MyClass";
 echo(%example.class);
 > MyClass
 
 %example.setFieldValue(class, "AnotherClass");
 echo(%example.class);
 > AnotherClass
 
 // set a dynamic "add-on" field
 echo(%example.myField);
 >
 
 %example.myField = "myValue";
 echo(%example.myField);
 > myValue
 
 %example.setFieldValue(anotherField, "anotherValue");
 echo(%example.anotherField);
 > anotherValue
 @endcode
 */
ConsoleMethod(SimObject, setFieldValue, bool, 4, 4, "fieldName,value")
{
   const char *fieldName = StringTable->insert(argv[2]);
   const char *value = argv[3];
   
   object->setDataField( fieldName, NULL, value );
   
   return true;
   
}

//-----------------------------------------------------------------------------
// Set the internal name, can be used to find child objects
// in a meaningful way, usually from script, while keeping
// common script functionality together using the controls "Name" field.
//-----------------------------------------------------------------------------
/*! sets the objects "internal" name
 @param internalName the name used for group access
 @return nothing returned
 
 Not to be confused with the object's `Name`, the internal name is used to
 find this object within a group.  Each object may be in one group, ultimately
 forming a tree (usually for GUI related classes).  See SimGroup for more information.
 
 @see SimGroup, getInternalName, isChildOfGroup, getGroup
 */
ConsoleMethod( SimObject, setInternalName, void, 3, 3, "string InternalName")
{
   object->setInternalName(argv[2]);
}

void SimObject::setInternalName(const char* newname)
{
   if(newname)
      mInternalName = StringTable->insert(newname);
}

/*! returns the objects "internal" name
   @return the internalName used for group access

   Not to be confused with the object's `Name`, the internal name is used to
   find this object within a group.  Each object may be in one group, ultimately
   forming a tree (usually for GUI related classes).  See SimGroup for more information.

   @see SimGroup, setInternalName, isChildOfGroup, getGroup
*/
ConsoleMethod( SimObject, getInternalName, const char*, 2, 2, "")
{
   return object->getInternalName();
}

StringTableEntry SimObject::getInternalName()
{
   return mInternalName;
}

/*! Returns the `Namespace` of this object as set by the user.
 @return The Namespace as set in the object's `class` field.
 
 The class namespace is a a scripting concept that provides a "namespace" in which the engine looks
 to find user-defined scripting functions. It can be set, and reset, by the user
 by using setClassNamespace().  Alternatively, it can be set directly using the `class` field of the object.
 
 Note that this can easily be confused with getClassName(), which is unrelated, and returns the "true"
 engine class name of an object, such as `SimObject`.
 
 See setClassNamespace() for examples.
 
 @see setClassNamespace
 */
ConsoleMethod(SimObject, getClassNamespace, const char*, 2, 2, "")
{
   return object->getClassNamespace();
}

/*! Return the superclass `Namespace` of this object as set by the user.
 
 An object can have a primary and secondary `Namespace` also known as its
 `class` and `superclass`.  If a user-defined function is not found in the `class`
 then the `superclass` is searched.
 
 @see getClassNamespace
 */
ConsoleMethod(SimObject, getSuperClassNamespace, const char*, 2, 2, "")
{
   return object->getSuperClassNamespace();
}

/*! Sets the `Namespace` of this object.
 @return no return value
 
 The class namespace is a a scripting concept that provides a "namespace" in which the engine looks
 to find user-defined scripting functions. It can be set, and reset, by the user using setClassNamespace().
 Alternatively, it can be set directly using the `class` field of the object.
 
 The `Namespace` or `class` can then be returned with getClassNamespace().  Note that this can easily be
 confused with getClassName(), which is unrelated, and returns the "true" engine class name of an object,
 such as `SimObject`.
 
 @par Example
 @code
 %example = new SimObject()
 {
 class = MyScope;
 };
 
 echo(%example.class);
 > MyScope
 
 // set the namespace using setNamespace()
 %example.setClassNamespace(DifferentScope);
 echo(%example.class);
 > DifferentScope
 
 // set the namespace directly using the field 'class'
 %example.class = YetAnotherScope;
 echo(%example.getClassNamespace());
 > YetAnotherScope
 @endcode
 
 @see getClassNamespace
 */
ConsoleMethod(SimObject, setClassNamespace, void, 2, 3, "nameSpace")
{
   object->setClassNamespace(argv[2]);
}

/*! Sets the superclass `Namespace` of this object.
 
 An object can have a primary and secondary `Namespace` also known as its
 `class` and `superclass`.  If a user-defined function is not found in the `class`
 then the `superclass` is searched.
 
 @see setClassNamespace
 */
ConsoleMethod(SimObject, setSuperClassNamespace, void, 2, 3, "")
{
   object->setSuperClassNamespace(argv[2]);
}

/*! Dynamically call a method by a string name
 
 Normally you would call a method in the form `%object.myMethod(param1, param2)`.
 Alternatively, you can use `%object.call(myMethod, param1, param2)`.  This can be
 useful if, for instance, you don't know which method to call in advance.
 
 @par Example
 @code
 %method = "setClassNamespace";
 %newNamespace = "myNamespace";
 
 %object.call(%method, %newNamespace);
 @endcode
 */
ConsoleMethod( SimObject, call, const char*, 2, 0, "methodName, [args]*")
{
   argv[1] = argv[2];
   return Con::execute( object, argc - 1, argv + 1 );
}

/*! Write the class hierarchy of an object to the console.
 
 @return no return value
 
 @par Example
 @code
 new SimGroup(sg);
 echo(sg.dumpClassHierarchy());
 > SimGroup ->
 > SimSet ->
 > SimObject
 @endcode
 */
ConsoleMethod(SimObject, dumpClassHierarchy, void, 2, 2, "")
{
   object->dumpClassHierarchy();
}

/*! returns true if this object is of the specified class or a subclass of the specified class
 @return true if a class or subclass of the given class
 
 @par Example
 @code
 %example = new SceneObject();
 
 echo(%example.isMemberOfClass(SimObject);
 > 1
 
 echo(%example.isMemberOfClass(SimSet);
 > 0
 @endcode
 */
ConsoleMethod(SimObject, isMemberOfClass, bool, 3, 3,  "string classname")
{
   
   AbstractClassRep* pRep = object->getClassRep();
   while(pRep)
   {
      if(!dStricmp(pRep->getClassName(), argv[2]))
      {
         //matches
         return true;
      }
      
      pRep  =      pRep->getParentClass();
   }
   
   return false;
}

/*! get the unique numeric ID -- or "handle" -- of this object.
 
 @return Returns the numeric ID.
 
 The id is provided for you by the simulator upon object creation.  You can not change it
 and it likely will not be reused by any other object after this object is deleted.
 
 @par Example
 @code
 new SimObject(example);
 echo(example.getId());
 > 1752
 @endcode
 
 @par Caveat
 You can not access the id directly.  That is, you can not access `%%object.id`.
 If you do set `%%object.id` you will only succeed in creating a dynamic field named
 `id` -- an unrelated field to the actual object's id.
 
 @par Example
 @code
 %example = SimObject();
 echo(%example.getId());
 > 1753
 
 // warning! this will fail to change the id!
 // it will also not warn you as it is legal syntax
 %example.id = 50;
 echo(%example.getId());
 > 1753
 
 echo(%example.id);
 > 50
 @endcode
 
 @sa getName, setName
 */
ConsoleMethod(SimObject, getId, S32, 2, 2, "")
{
   return object->getId();
}

/*! determines if this object is contained in a SimGroup and if so, which one.
 @return Returns the ID of the SimGroup this shape is in or zero if the shape is not contained in a SimGroup
 
 
 @see SimGroup, getInternalName, setInternalName, isChildOfGroup
 */
ConsoleMethod(SimObject, getGroup, S32, 2, 2, "")
{
   SimGroup *grp = object->getGroup();
   if(!grp)
      return -1;
   return grp->getId();
}

/*! Use the delete method to delete this object.
 When an object is deleted, it automatically
 + Unregisters its ID and name (if it has one) with the engine.
 + Removes itself from any SimGroup or SimSet it may be a member of.
 + (eventually) returns the memory associated with itself and its non-dynamic members.
 + Cancels all pending %obj.schedule() events.
 
 For objects in the GameBase, ScriptObject, or GUIControl hierarchies, an object will first: Call the onRemove() method for the object's namespace
 @return No return value.
 */
ConsoleMethod(SimObject, delete, void, 2, 2, "")
{
   object->deleteObject();
}

/*! schedule an action to be executed upon this object in the future.
 
 @param time Time in milliseconds till action is scheduled to occur.
 @param command Name of the command to execute. This command must be scoped to this object
 (i.e. It must exist in the namespace of the object), otherwise the schedule call will fail.
 @param arg1...argN These are optional arguments which will be passed to the command.
 This version of schedule automatically passes the ID of %obj as arg0 to command.
 @return Returns an integer schedule ID.
 
 The major difference between this and the ::schedule() console function is that if this object is deleted prior
 to the scheduled event, the event is automatically canceled. Times should not be treated as exact since some
 'simulation delay' is to be expected. The minimum resolution for a scheduled event is 32 ms, or one tick.
 
 The existence of command is not validated. If you pass an invalid console method name, the
 schedule() method will still return a schedule ID, but the subsequent event will fail silently.
 
 To manipulate the scheduled event, use the id returned with the system schedule functions.
 
 @see ::schedule
 */
ConsoleMethod(SimObject,schedule, S32, 4, 0, "time , command , [arg]* ")
{
   U32 timeDelta = U32(dAtof(argv[2]));
   argv[2] = argv[3];
   argv[3] = argv[1];
   SimConsoleEvent *evt = new SimConsoleEvent(argc - 2, argv + 2, true);
   S32 ret = Sim::postEvent(object, evt, Sim::getCurrentTime() + timeDelta);
   // #ifdef DEBUG
   //    Con::printf("obj %s schedule(%s) = %d", argv[3], argv[2], ret);
   //    Con::executef(1, "backtrace");
   // #endif
   return ret;
}

static bool compareFields(const AbstractClassRep::Field* fa,
                   const AbstractClassRep::Field* fb)
{
    return dStricmp(fa->pFieldname, fb->pFieldname) < 0;
}

/*! return the number of dynamic ("add-on") fields.
 @return the number of dynamic fields
 
 Note that static (or "built-in") fields are not counted.  For instance,
 `SimObject.class` will not count.
 
 See getDynamicField() for an explanation and examples.
 
 @see getDynamicField, getField, getFieldCount
 */
ConsoleMethod(SimObject, getDynamicFieldCount, S32, 2, 2, "")
{
   S32 count = 0;
   SimFieldDictionary* fieldDictionary = object->getFieldDictionary();
   for (SimFieldDictionaryIterator itr(fieldDictionary); *itr; ++itr)
      count++;
   
   return count;
}

/*! Return the field name of a specific dynamic ("add-on") field by index.
 @param index the dynamic field for which to retrieve the name
 @return the name of the field
 
 You would normally access dynamic fields directly `%%object.field` or
 indirectly `%%object.getFieldValue(%%field)`.  However, you may not know the
 field's names or otherwise need to iterate over the fields.  Use getDynamicFieldCount()
 to get the number of dynamic fields, and then iterate over them with this function.
 
 Note that only dynamic ("add-on") fields will be surfaced.  Static ("built-in") fields
 like `SimSet.class` will not be counted or listed.
 
 While static and dynamic fields have separate functions to get their counts and names, they
 share getFieldValue() and setFieldValue() to read and set any field by name.
 
 Also note that the order of the fields by an index has no meaning.  It is not alphabetical,
 the order created, or otherwise.
 
 @par Example
 @code
 %count = %example.getDynamicFieldCount();
 for (%i = 0; %i < %count; %i++)
 {
 %fieldName = %example.getDynamicField(%i);
 %fieldValue = %example.getFieldValue(%fieldName);
 echo(%fieldName @ " = " @ %fieldValue);
 }
 @endcode
 
 @see getDynamicFieldCount, getField, getFieldCount
 */
ConsoleMethod(SimObject, getDynamicField, const char*, 3, 3, "index")
{
   SimFieldDictionary* fieldDictionary = object->getFieldDictionary();
   SimFieldDictionaryIterator itr(fieldDictionary);
   S32 index = dAtoi(argv[2]);
   for (S32 i = 0; i < index; i++)
   {
      if (!(*itr))
      {
         Con::warnf("Invalid dynamic field index passed to SimObject::getDynamicField!");
         return NULL;
      }
      ++itr;
   }
   
   KorkApi::ConsoleValue bufferV = Con::getReturnBuffer(256);
   char* buffer = (char*)bufferV.evaluatePtr(vmPtr->getAllocBase());
   if (*itr)
   {
      SimFieldDictionary::Entry* entry = *itr;
      dSprintf(buffer, 256, "%s\t%s", entry->slotName, entry->value);
      return buffer;
   }
   
   Con::warnf("Invalid dynamic field index passed to SimObject::getDynamicField!");
   return NULL;
}

/*! dump the object to  the console.
 
 Use the dump method to display the following information about this object:
 + All static and dynamic fields that are non-null
 + All engine and script-registered console methods (including superclass methods) for this object
 @return No return value
 */
ConsoleMethod(SimObject,dump, void, 2, 2, "")
{
   object->dump();
}


void SimObject::dump()
{
   const AbstractClassRep::FieldList &list = getFieldList();
   char expandedBuffer[4096];

   Con::printf("Static Fields:");
   std::vector<const AbstractClassRep::Field *> flist;
   
   for(U32 i = 0; i < (U32)list.size(); i++)
      flist.push_back(&list[i]);

   std::sort(flist.begin(),flist.end(),compareFields);

   for(std::vector<const AbstractClassRep::Field *>::iterator itr = flist.begin(); itr != flist.end(); itr++)
   {
      const AbstractClassRep::Field* f = *itr;
      if( f->type == AbstractClassRep::DepricatedFieldType ||
          f->type == AbstractClassRep::StartGroupFieldType ||
          f->type == AbstractClassRep::EndGroupFieldType) continue;

      StringTableEntry steField = getVM()->internString(f->pFieldname);
      for(U32 j = 0; S32(j) < f->elementCount; j++)
      {
         char arrayValue[32];
         snprintf(arrayValue, sizeof(arrayValue), "%u", j);
         KorkApi::ConsoleValue fieldValue = getVM()->getObjectField(getVMObject(), steField, arrayValue);
         const char* val = getVM()->valueAsString(fieldValue);

         if(!val /*|| !*val*/)
            continue;
         if(f->elementCount == 1)
         {
            dSprintf(expandedBuffer, sizeof(expandedBuffer), "  %s = \"", f->pFieldname);
         }
         else
         {
            dSprintf(expandedBuffer, sizeof(expandedBuffer), "  %s[%d] = \"", f->pFieldname, j);
         }
         
         expandEscape(expandedBuffer + dStrlen(expandedBuffer), val);
         Con::printf("%s\"", expandedBuffer);
      }
   }

   Con::printf("Dynamic Fields:");
   if(getFieldDictionary())
   {
      getFieldDictionary()->printFields(this);
   }

   Con::printf("Methods:");
   KorkApi::NamespaceId nsId = getNamespace();

   getVM()->enumerateNamespace(nsId, getVM(), [](void* userPtr, StringTableEntry funcName, const char* usage){
      Con::printf("  %s() - %s", funcName, usage);
   });
}

/*! Use the getType method to get the type for this object.

    @return Returns a bit mask containing one or more set bits.

   This is here for legacy purposes.
   
   This type is an integer value composed of bitmasks. For simplicity, these bitmasks
   are defined in the engine and exposed for our use as global variables.
    To simplify the writing of scripts, a set of globals has been provided containing
   the bit setting for each class corresponding to a particular type.
    @sa getClassName
*/
ConsoleMethod(SimObject, getType, S32, 2, 2, "")
{
   return((S32)object->getType());
}

//---------------------------------------------------------------------------

bool SimObject::isMethod( const char* methodName )
{
   if( !methodName || !methodName[0] )
      return false;

   StringTableEntry stname = StringTable->insert( methodName );

   if (getVM())
      return getVM()->isNamespaceFunction(getNamespace(), stname);
   
   return false;
}

/*! Returns wether the method exists for this object.
 
 @returns true if the method exists; false otherwise
 
 The method must be a "built-in" method, or one that is not user-defined in script.
 It must also be a direct method on the object, and not a behavior defined in a Behavior.
 */
ConsoleMethod(SimObject, isMethod, bool, 3, 3, "string methodName")
{
   return object->isMethod( argv[2] );
}

/*! return the number of static ("built-in") fields.
 @return the number of dynamic fields
 
 Note that dynamic (or "add-on") fields are not counted.  For instance,
 `%%object.class` will count, but `%%object.myField` will not.
 
 See getField() for an explanation and examples.
 
 @see getDynamicField, getDynamicFieldCount, getField
 */
ConsoleMethod( SimObject, getFieldCount, S32, 2, 2, "")
{
   const AbstractClassRep::FieldList &list = object->getFieldList();
   const AbstractClassRep::Field* f;
   U32 numDummyEntries = 0;
   for(int i = 0; i < list.size(); i++)
   {
      f = &list[i];
      
      if( f->type == AbstractClassRep::DepricatedFieldType ||
         f->type == AbstractClassRep::StartGroupFieldType ||
         f->type == AbstractClassRep::EndGroupFieldType )
      {
         numDummyEntries++;
      }
   }
   
   return list.size() - numDummyEntries;
}

/*! Return the field name of a specific static ("built-in") field by index.
 @param index the static field for which to retrieve the name
 @return the name of the field
 
 You would normally access static fields directly `%%object.class` or
 indirectly `%%object.getFieldValue(%%field)`.  However, you may not know the
 field's names or otherwise need to iterate over the fields.  Use getFieldCount()
 to get the number of static fields, and then iterate over them with this function.
 
 Note that only static ("built-in") fields will be surfaced.  Dynamic ("add-on") fields
 like `%%SimSet.myField` will not be counted or listed.
 
 While static and dynamic fields have separate functions to get their counts and names, they
 share getFieldValue() and setFieldValue() to read and set any field by name.
 
 Also note that the order of the fields by an index has no meaning.  It is not alphabetical,
 the order created, or otherwise.
 
 @par Example
 @code
 %count = %example.getFieldCount();
 for (%i = 0; %i < %count; %i++)
 {
 %fieldName = %example.getField(%i);
 %fieldValue = %example.getFieldValue(%fieldName);
 echo(%fieldName @ " = " @ %fieldValue);
 }
 @endcode
 
 @see getDynamicField, getDynamicFieldCount, getFieldCount
 
 */
ConsoleMethod( SimObject, getField, const char*, 3, 3, "int index")
{
   S32 index = dAtoi( argv[2] );
   const AbstractClassRep::FieldList &list = object->getFieldList();
   if( ( index < 0 ) || ( index >= list.size() ) )
      return "";
   
   const AbstractClassRep::Field* f;
   S32 currentField = 0;
   for(int i = 0; i < list.size() && currentField <= index; i++)
   {
      f = &list[i];
      
      // skip any dummy fields
      if(f->type == AbstractClassRep::DepricatedFieldType ||
         f->type == AbstractClassRep::StartGroupFieldType ||
         f->type == AbstractClassRep::EndGroupFieldType)
      {
         continue;
      }
      
      if(currentField == index)
         return f->pFieldname;
      
      currentField++;
   }
   
   // if we found nada, return nada.
   return "";
}

const char *SimObject::tabComplete(const char *prevText, S32 baseLen, bool fForward)
{
   return vm->tabCompleteNamespace(vm->getObjectNamespace(vmObject), prevText, baseLen, fForward);
}

//-----------------------------------------------------------------------------

void SimObject::setDataField(StringTableEntry slotName, const char *array, const char *value)
{
   getVM()->setObjectField(getVMObject(),slotName, KorkApi::ConsoleValue::makeString(value), array);
}

void SimObject::setDataFieldDynamic(StringTableEntry slotName, const char *array, const char *value, U32 typeId)
{
   if(!mFieldDictionary)
      mFieldDictionary = new SimFieldDictionary;

   if(!array)
   {
      mFieldDictionary->setFieldValue(slotName, value, typeId);
   }
   else
   {
      char buf[256];
      dStrcpy(buf, slotName);
      dStrcat(buf, array);
      mFieldDictionary->setFieldValue(StringTable->insert(buf), value, typeId);
   }
}

//-----------------------------------------------------------------------------

void  SimObject::dumpClassHierarchy()
{
   AbstractClassRep* pRep = getClassRep();
   while(pRep)
   {
      Con::warnf("%s ->", pRep->getClassName());
      pRep  =  pRep->getParentClass();
   }
}

//-----------------------------------------------------------------------------

const char *SimObject::getDataField(StringTableEntry slotName, const char *array)
{
   return getVM()->valueAsString(getVM()->getObjectField(getVMObject(), slotName, array));
}


const char *SimObject::getDataFieldDynamic(StringTableEntry slotName, const char *array, U32* outTypeId)
{
   if(!mFieldDictionary)
      return "";

   if (outTypeId)
   {
      *outTypeId = 0;
   }
   
   if(!array)
   {
      if (const char* val = mFieldDictionary->getFieldValue(slotName, outTypeId))
         return val;
   }
   else
   {
      static char buf[256];
      dStrcpy(buf, slotName);
      dStrcat(buf, array);
      if (const char* val = mFieldDictionary->getFieldValue(StringTable->insert(buf), outTypeId))
         return val;
   }
   
   return "";
}

//-----------------------------------------------------------------------------

SimObject::~SimObject()
{
   delete mFieldDictionary;

   AssertFatal(nextNameObject == (SimObject*)-1,avar(
                  "SimObject::~SimObject:  Not removed from dictionary: name %s, id %i",
                  objectName, mId));
   AssertFatal(nextManagerNameObject == (SimObject*)-1,avar(
                  "SimObject::~SimObject:  Not removed from manager dictionary: name %s, id %i",
                  objectName,mId));
   AssertFatal(!isProperlyAdded(), "SimObject::object "
               "missing call to SimObject::onRemove");
}

//-----------------------------------------------------------------------------

bool SimObject::isLocked()
{
   if(!mFieldDictionary)
      return false;

   const char * val = mFieldDictionary->getFieldValue( StringTable->insert( "locked", false ) );

   return( val ? dAtob(val) : false );
}

//-----------------------------------------------------------------------------

void SimObject::setLocked( bool b = true )
{
   setDataField(StringTable->insert("locked", false), NULL, b ? "true" : "false" );
}

//-----------------------------------------------------------------------------

bool SimObject::isHidden()
{
   if(!mFieldDictionary)
      return false;

   const char * val = mFieldDictionary->getFieldValue( StringTable->insert( "hidden", false ) );
   return( val ? dAtob(val) : false );
}

//-----------------------------------------------------------------------------

void SimObject::setHidden(bool b = true)
{
   setDataField(StringTable->insert("hidden", false), NULL, b ? "true" : "false" );
}

//-----------------------------------------------------------------------------

bool SimObject::onAdd()
{
   mSimFlags |= Added;

   linkNamespaces();

   // onAdd() should return FALSE if there was an error
   return true;
}

//-----------------------------------------------------------------------------

void SimObject::onRemove()
{
   mSimFlags |= ~Added;

   unlinkNamespaces();
}

//-----------------------------------------------------------------------------

void SimObject::onGroupAdd()
{
}

//---------------------------------------------------------------------------

void SimObject::onGroupRemove()
{
}

//---------------------------------------------------------------------------

void SimObject::onDeleteNotify(SimObject*)
{
}

//---------------------------------------------------------------------------

void SimObject::onNameChange(const char*)
{
}

//---------------------------------------------------------------------------

void SimObject::onStaticModified(const char* slotName, const char* newValue)
{
}

//---------------------------------------------------------------------------

bool SimObject::processArguments(S32 argc, const char**)
{
   return argc == 0;
}

//---------------------------------------------------------------------------

bool SimObject::isChildOfGroup(SimGroup* pGroup)
{
   if(!pGroup)
      return false;

   //if we *are* the group in question,
   //return true:
   if(pGroup == dynamic_cast<SimGroup*>(this))
      return true;

   SimGroup* temp =  mGroup;
   while(temp)
   {
      if(temp == pGroup)
         return true;
      temp = temp->mGroup;
   }

   return false;
}

/*! test if this object is in a specified group (or subgroup of it)
 @param groupID the ID of the group being tested
 @returns true if we are in the specified simgroup or a subgroup of it; false otherwise
 
 @see SimGroup, getInternalName, setInternalName, getGroup
 */
ConsoleMethod(SimObject, isChildOfGroup, bool, 3,3, "groupID")
{
   SimGroup* pGroup = dynamic_cast<SimGroup*>(Sim::findObject(dAtoi(argv[2])));
   if(pGroup)
   {
      return object->isChildOfGroup(pGroup);
   }
   
   return false;
}

//-----------------------------------------------------------------------------

U32 SimObject::getDataFieldType( StringTableEntry slotName, const char* array )
{
   const AbstractClassRep::Field* field = findField( slotName );
   if( field )
      return field->type;

   return 0;
}

//---------------------------------------------------------------------------

static Chunker<SimObject::Notify> notifyChunker(128000);
SimObject::Notify *SimObject::mNotifyFreeList = NULL;

SimObject::Notify *SimObject::allocNotify()
{
   if(mNotifyFreeList)
   {
      SimObject::Notify *ret = mNotifyFreeList;
      mNotifyFreeList = ret->next;
      return ret;
   }
   return notifyChunker.alloc();
}

void SimObject::freeNotify(SimObject::Notify* note)
{
   AssertFatal(note->type != SimObject::Notify::Invalid, "Invalid notify");
   note->type = SimObject::Notify::Invalid;
   note->next = mNotifyFreeList;
   mNotifyFreeList = note;
}

//------------------------------------------------------------------------------

SimObject::Notify* SimObject::removeNotify(void *ptr, SimObject::Notify::Type type)
{
   Notify **list = &mNotifyList;
   while(*list)
   {
      if((*list)->ptr == ptr && (*list)->type == type)
      {
         SimObject::Notify *ret = *list;
         *list = ret->next;
         return ret;
      }
      list = &((*list)->next);
   }
   return NULL;
}

void SimObject::deleteNotify(SimObject* obj)
{
   AssertFatal(!obj->isDeleted(),
               "SimManager::deleteNotify: Object is being deleted");
   Notify *note = allocNotify();
   note->ptr = (void *) this;
   note->next = obj->mNotifyList;
   note->type = Notify::DeleteNotify;
   obj->mNotifyList = note;

   note = allocNotify();
   note->ptr = (void *) obj;
   note->next = mNotifyList;
   note->type = Notify::ClearNotify;
   mNotifyList = note;

   //obj->deleteNotifyList.pushBack(this);
   //clearNotifyList.pushBack(obj);
}

void SimObject::registerReference(SimObject **ptr)
{
   Notify *note = allocNotify();
   note->ptr = (void *) ptr;
   note->next = mNotifyList;
   note->type = Notify::ObjectRef;
   mNotifyList = note;
}

void SimObject::unregisterReference(SimObject **ptr)
{
   Notify *note = removeNotify((void *) ptr, Notify::ObjectRef);
   if(note)
      freeNotify(note);
}

void SimObject::clearNotify(SimObject* obj)
{
   Notify *note = obj->removeNotify((void *) this, Notify::DeleteNotify);
   if(note)
      freeNotify(note);

   note = removeNotify((void *) obj, Notify::ClearNotify);
   if(note)
      freeNotify(note);
}

void SimObject::processDeleteNotifies()
{
   // clear out any delete notifies and
   // object refs.

   while(mNotifyList)
   {
      Notify *note = mNotifyList;
      mNotifyList = note->next;

      AssertFatal(note->type != Notify::ClearNotify, "Clear notes should be all gone.");

      if(note->type == Notify::DeleteNotify)
      {
         SimObject *obj = (SimObject *) note->ptr;
         Notify *cnote = obj->removeNotify((void *)this, Notify::ClearNotify);
         obj->onDeleteNotify(this);
         freeNotify(cnote);
      }
      else
      {
         // it must be an object ref - a pointer refs this object
         *((SimObject **) note->ptr) = NULL;
      }
      freeNotify(note);
   }
}

void SimObject::clearAllNotifications()
{
   for(Notify **cnote = &mNotifyList; *cnote; )
   {
      Notify *temp = *cnote;
      if(temp->type == Notify::ClearNotify)
      {
         *cnote = temp->next;
         Notify *note = ((SimObject *) temp->ptr)->removeNotify((void *) this, Notify::DeleteNotify);
         freeNotify(temp);
         freeNotify(note);
      }
      else
         cnote = &(temp->next);
   }
}

//---------------------------------------------------------------------------

bool gAllowClassName = false;

void SimObject::initPersistFields()
{
   Parent::initPersistFields();
   
   addGroup("SimBase");
   addField("canSaveDynamicFields",    TypeBool,      Offset(mCanSaveFieldDictionary, SimObject), &writeCanSaveDynamicFields, "");
   addField("internalName",            TypeString,       Offset(mInternalName, SimObject), &writeInternalName, "");   
   addProtectedField("parentGroup",    TypeSimObjectPtr, Offset(mGroup, SimObject), &setParentGroup, &writeParentGroup, "Group hierarchy parent of the object." );
   endGroup("SimBase");

   // Namespace Linking.
   //registerClassNameFields(); // TGE compat - this should only be allowed on GameBase or ScriptObjectxw
}

void SimObject::registerClassNameFields()
{
   addGroup("Namespace Linking");
   //addProtectedField("superclass", TypeString, Offset(mSuperClassName, SimObject), &setSuperClass, NULL, &writeSuperclass, "Script Class of object.");
   //addProtectedField("className",      TypeString, Offset(mClassName,      SimObject), &setClass,      NULL, &writeClass, "Script SuperClass of object.");
   endGroup("Namespace Linking");
}

//-----------------------------------------------------------------------------

SimObject* SimObject::clone( const bool copyDynamicFields )
{
    // Craete cloned object.
    SimObject* pCloneObject = dynamic_cast<SimObject*>( ConsoleObject::create(getClassName()) );
    if (!pCloneObject)
    {
        Con::errorf("SimObject::clone() - Unable to create cloned object.");
        return NULL;
    }

    // Register object.
    if ( !pCloneObject->registerObject() )
    {
        Con::warnf("SimObject::clone() - Unable to register cloned object.");
        delete pCloneObject;
        return NULL;
    }

    // Copy object.
    copyTo( pCloneObject );

    // Copy over dynamic fields if requested.
    if ( copyDynamicFields )
        pCloneObject->assignDynamicFieldsFrom( this );

    return pCloneObject;
}


//-----------------------------------------------------------------------------

void SimObject::copyTo(SimObject* object)
{
   object->mClassName = mClassName;
   object->mSuperClassName = mSuperClassName;
   object->mVMNameSpace = NULL;
   object->linkNamespaces();
}

//-----------------------------------------------------------------------------

bool SimObject::setParentGroup(void* userPtr,
                               KorkApi::Vm* vmPtr,
                               KorkApi::TypeStorageInterface* inputStorage,
                               KorkApi::TypeStorageInterface* outputStorage,
                               const EnumTable* tbl,
                               BitSet32 flag,
                               U32 requestedType)
{
   SimGroup *parent = NULL;
   SimObject *object = static_cast<SimObject*>(outputStorage->fieldObject);
   if (object == NULL || inputStorage->data.argc != 1)
   {
      return false;
   }

   if(Sim::findObject(vmPtr->valueAsString(*inputStorage->data.storageRegister), parent))
   {
      parent->addObject(object);
   }
   
   return true;
}


bool SimObject::addToSet(SimObjectId spid)
{
   if (!isProperlyAdded())
      return false;

   SimObject* ptr = Sim::findObject(spid);
   if (ptr) 
   {
      SimSet* sp = dynamic_cast<SimSet*>(ptr);
      AssertFatal(sp != 0,
                  "SimObject::addToSet: "
                  "ObjectId does not refer to a set object");
      if (sp)
      {
         sp->addObject(this);
         return true;
      }
   }
   return false;
}

bool SimObject::addToSet(const char *ObjectName)
{
   if (!isProperlyAdded())
      return false;

   SimObject* ptr = Sim::findObject(ObjectName);
   if (ptr) 
   {
      SimSet* sp = dynamic_cast<SimSet*>(ptr);
      AssertFatal(sp != 0,
                  "SimObject::addToSet: "
                  "ObjectName does not refer to a set object");
      if (sp)
      {
         sp->addObject(this);
         return true;
      }
   }
   return false;
}

bool SimObject::removeFromSet(SimObjectId sid)
{
   if (!isProperlyAdded())
      return false;

   SimSet *set;
   if(Sim::findObject(sid, set))
   {
      set->removeObject(this);
      return true;
   }
   return false;
}

bool SimObject::removeFromSet(const char *objectName)
{
   if (!isProperlyAdded())
      return false;

   SimSet *set;
   if(Sim::findObject(objectName, set))
   {
      set->removeObject(this);
      return true;
   }
   return false;
}

void SimObject::inspectPreApply()
{
}

void SimObject::inspectPostApply()
{
}

void SimObject::linkNamespaces()
{
   if( mVMNameSpace )
      unlinkNamespaces();
   
   StringTableEntry parent = StringTable->insert( getClassName() );
   if( ( mNSLinkMask & LinkSuperClassName ) && mSuperClassName && mSuperClassName[0] )
   {
      if( Con::linkNamespaces( parent, mSuperClassName ) )
         parent = mSuperClassName;
      else
         mSuperClassName = StringTable->EmptyString; // CodeReview Is this behavior that we want?
                                                      // CodeReview This will result in the mSuperClassName variable getting hosed
                                                      // CodeReview if Con::linkNamespaces returns false. Looking at the code for
                                                      // CodeReview Con::linkNamespaces, and the call it makes to classLinkTo, it seems
                                                      // CodeReview like this would only fail if it had bogus data to begin with, but
                                                      // CodeReview I wanted to note this behavior which occurs in some implementations
                                                      // CodeReview but not all. -patw
   }

   // ClassName -> SuperClassName
   if ( ( mNSLinkMask & LinkClassName ) && mClassName && mClassName[0] )
   {
      if( Con::linkNamespaces( parent, mClassName ) )
         parent = mClassName;
      else
         mClassName = StringTable->EmptyString; // CodeReview (See previous note on this code)
   }

   // ObjectName -> ClassName
   StringTableEntry objectName = getName();
   if( objectName && objectName[0] && 
       strcasecmp(objectName, getClassRep()->getClassName()) != 0 )
   {
      if( sVM->linkNamespace( parent, objectName ) )
      {
         parent = objectName;
      }
   }

   // Store our namespace.
   mVMNameSpace = sVM->findNamespace(parent);
   getVM()->setObjectNamespace(getVMObject(), mVMNameSpace);
}

void SimObject::unlinkNamespaces()
{
   if (!mVMNameSpace)
      return;

   // Restore NameSpace's
   StringTableEntry child = getName();
   if( child && child[0] )
   {
      if( ( mNSLinkMask & LinkClassName ) && mClassName && mClassName[0])
      {
         if( Con::unlinkNamespaces( mClassName, child ) )
            child = mClassName;
      }

      if( ( mNSLinkMask & LinkSuperClassName ) && mSuperClassName && mSuperClassName[0] )
      {
         if( Con::unlinkNamespaces( mSuperClassName, child ) )
            child = mSuperClassName;
      }

      Con::unlinkNamespaces( getClassName(), child );
   }
   else
   {
      child = mClassName;
      if( child && child[0] )
      {
         if( ( mNSLinkMask & LinkSuperClassName ) && mSuperClassName && mSuperClassName[0] )
         {
            if( Con::unlinkNamespaces( mSuperClassName, child ) )
               child = mSuperClassName;
         }

         Con::unlinkNamespaces( getClassName(), child );
      }
      else
      {
         if( ( mNSLinkMask & LinkSuperClassName ) && mSuperClassName && mSuperClassName[0] )
            Con::unlinkNamespaces( getClassName(), mSuperClassName );
      }
   }

   mVMNameSpace = NULL;
   getVM()->setObjectNamespace(getVMObject(), NULL);
}

void SimObject::setClassNamespace( const char *classNamespace )
{
    mClassName = StringTable->insert( classNamespace );
    if (isProperlyAdded())
        linkNamespaces();
}

void SimObject::setSuperClassNamespace( const char *superClassNamespace )
{
    mSuperClassName = StringTable->insert( superClassNamespace );
    if (isProperlyAdded())
        linkNamespaces();
}

/*! @class SimObject
   SimObject is the base class for all other scripted classes.
   This means that all other "simulation" classes -- be they SceneObjects, Scenes, or plain-old SimObjects
   -- can use the methods and fields of SimObject.
   
   @par Identity

   When we create a SimObject with `::new`, it is given a unique id which is returned by `::new`.
   We usually save the id in our own variables.  Alternatively, we can give the SimObject a name which we can
   use to directly manipulate it.  This name can be set with the `::new` operator or it can be added later.

   If we give an object a name, then we can write script methods that are "scoped" to run on this object only.
   For instance, if we have a SimObject named `MyObject`, and if we call `MyObject.myMethod()`, then this
   call will be handled by a method we named `MyObject::myMethod` if one exists.

   See getId(), getName(), setName()

   @par Static and Dynamic Fields

   Each subclass of SimObject will provide important fields.  For instance, a SceneObject will have a position,
   scale, etc.  These are known as "static" or "built-in" fields.  Additionally, you can add any number of your
   own fields, for example `myField` or `hitPoints`.  These are known as "dynamic" or "add-on" fields.
   To do so only requires you to set the field directly with `%%myObject.myField = myValue;`.  Attempting to
   retrieve a field that does not exist (yet) returns nothing.  There is no error or warning.

   Note that static fields exist for every object of a class, while dynamic fields are unique to any one instance.  Adding
   `myField` to one SceneObject does not add it to all SceneObjects.

   For working with fields more programmatically, see *Reflection* below.

   @par Script Namespace

   We can attach a `Namespace` to an object.  Then calls to this object
   will be handled by the script functions in that Namespace.  For instance, if we set `%%myObject.class = MyScope` then
   the call `%%myObject.myMethod` will be handled with a method named `MyScope::myMethod()`.  (If we also named the object `MyObject`, then
   if there is a `MyObject::myMethod()` it will run.  Otherwise, Torque2D will look for `MyScope::myMethod()` and run that
   if found.)
   
   Finally there is also a *secondary* `Namespace` that will receive the call if neither the `Name` nor the *primary* `Namespace`
   had a method to handle the call.

   Unfortunately, these `Namespaces` are called `Classes` in this context.  You set the `class` or `superclass`.  But this
   should not be confused with the SimObject's "real" class which is SimObject or Scene as examples.

   See getClassNamespace(), setClassNamespace(), getSuperClassNamespace(), setSuperClassNamespace()

   @par Reflection

   SimObject supports "reflection" -- the run-time inspection of an object's methods, fields, etc.  For instance,
   we can ask an object what class it instantiates, what dynamic fields it has, etc.  We can also use this feature to
   call a method on an object even if we only know the string name of the method.

   See getClassName(), isMemberOfClass(), isMethod(), dump(), dumpClassHierarchy(), call()

   See getFieldValue(), setFieldValue(), getFieldCount(), getField(), getDynamicFieldCount(), getDynamicField()

   @par Scheduling Callbacks

   We can set a SimObject to regularly call its own onTimer() callback.  Additionally, we can schedule a single call to
   any method at any time in the future.

   See startTimer(), stopTimer(), isTimerActive(), schedule()

   @par Groups
   TBD

   @par Persistence
   canSaveDynamicFields bool = "1" - Whether a save() shall save the object's Dynamic Fields (member fields created by TorqueScript)

*/

/*! Sets the progenitor file responsible for this instances creation.
    @param file The progenitor file responsible for this instances creation.
    @return No return value.
*/
ConsoleMethod(SimObject, setProgenitorFile, void, 3, 3, "file")
{
    object->setProgenitorFile( argv[2] );
}

//-----------------------------------------------------------------------------

/*! Gets the progenitor file responsible for this instances creation.
    @return The progenitor file responsible for this instances creation.
*/
ConsoleMethod(SimObject, getProgenitorFile, const char*, 2, 2, "")
{
    return object->getProgenitorFile();
}

/*! return the type of a field, such as "int" for an Integer
   @param fieldName field of the object to get the type of
   @return string name of the type; or nothing if the field isn't found

   No warning will be shown if the field isn't found.

   @par Example
   @code
   new sprite(s);
   echo(s.getFieldType(frame));
   > int

   echo(s.getFieldType(blendcolor));
   > ColorF

   echo(s.getFieldType(angle));
   > float

   echo(s.getFieldType(position));
   > Vector2

   echo(s.getFieldType(class));
   > string

   echo(s.getFieldType(notAField));
   >
   @endcode
*/
ConsoleMethod(SimObject, getFieldType, const char*, 3, 3, "fieldName")
{
   const char *fieldName = StringTable->insert( argv[2] );
   U32 typeID = object->getDataFieldType( fieldName, NULL );
   KorkApi::TypeInfo* type = vmPtr->getTypeInfo(typeID);
   return type ? type->name : "";
}

/*! Clones the object.
    @param copyDynamicFields Whether the dynamic fields should be copied to the cloned object or not.  Optional: Defaults to false.
    @return (newObjectID) The newly cloned object's id if successful, otherwise a 0.
*/
ConsoleMethod(SimObject, clone, S32, 2, 3, "[copyDynamicFields = false]?")
{
    // Fetch copy dynamic fields flag.
    const bool copyDynamicFields = ( argc >= 3 ) ? dAtob( argv[2] ) : false;

    // Clone Object.
    SimObject* pClonedObject = object->clone( copyDynamicFields );

    // Finish if object was not cloned.
    if ( pClonedObject == NULL )
        return 0;

    return pClonedObject->getId();
}

#if 0
/*! Starts a periodic timer for this object.
    Sets a timer on the object that, when it expires, will cause the object to execute the onTimer() callback.
    The timer event will continue to occur at regular intervals until setTimerOff() is called.
    @param callbackFunction The name of the callback function to call for each timer repetition.
    @param timePeriod The period of time (in milliseconds) between each callback.
    @param repeat The number of times the timer should repeat.  If not specified or zero then it will run infinitely
    @return No return Value.
*/
ConsoleMethod(SimObject, startTimer, bool, 4, 5, "callbackFunction, float timePeriod, [repeat]?")
{
    // Is the periodic timer running?
    if ( object->getPeriodicTimerID() != 0 )
    {
        // Yes, so cancel it.
        Sim::cancelEvent( object->getPeriodicTimerID() );

        // Reset Timer ID.
        object->setPeriodicTimerID( 0 );
    }

    // Fetch the callback function.
    StringTableEntry callbackFunction = StringTable->insert( argv[2] );

    // Does the function exist?
    if ( !object->isMethod( callbackFunction ) )
    {
        // No, so warn.
        Con::warnf("SimObject::startTimer() - The callback function of '%s' does not exist.", callbackFunction );
        return false;
    }

    // Fetch the time period.
    const S32 timePeriod = dAtoi(argv[3]);

    // Is the time period valid?
    if ( timePeriod < 1 )
    {
        // No, so warn.
        Con::warnf("SimObject::startTimer() - The time period of '%d' is invalid.", timePeriod );
        return false;
    }        

    // Fetch the repeat count.
    const S32 repeat = argc >= 5 ? dAtoi(argv[4]) : 0;

    // Create Timer Event.
    SimObjectTimerEvent* pEvent = new SimObjectTimerEvent( callbackFunction, (U32)timePeriod, (U32)repeat );

    // Post Event.
    object->setPeriodicTimerID( Sim::postEvent( object, pEvent, Sim::getCurrentTime() + timePeriod ) );

    return true;
}

//-----------------------------------------------------------------------------

/*! Stops the periodic timer for this object.
    @return No return Value.
*/
ConsoleMethod(SimObject, stopTimer, void, 2, 2, "")
{
    // Finish if the periodic timer isn't running.
    if ( object->getPeriodicTimerID() == 0 )
        return;

    // Cancel It.
    Sim::cancelEvent( object->getPeriodicTimerID() );

    // Reset Timer ID.
    object->setPeriodicTimerID( 0 );
}

//-----------------------------------------------------------------------------

/*! Checks whether the periodic timer is active for this object or not.
    @return Whether the periodic timer is active for this object or not.
*/
ConsoleMethod(SimObject, isTimerActive, bool, 2, 2, "")
{
    return object->isPeriodicTimerActive();
}

// END T2D BLOCK
#endif


// BEGIN T2D+T3D BLOCK
//---------------------------------------------------------------------------

IMPLEMENT_CO_DATABLOCK_V1(SimDataBlock);
SimObjectId SimDataBlock::sNextObjectId = DataBlockObjectIdFirst;
S32 SimDataBlock::sNextModifiedKey = 0;

//---------------------------------------------------------------------------

SimDataBlock::SimDataBlock()
{
   setModDynamicFields(true);
   setModStaticFields(true);
}

//-----------------------------------------------------------------------------

bool SimDataBlock::onAdd()
{

   Parent::onAdd();
   
   // This initialization is done here, and not in the constructor,
   // because some jokers like to construct and destruct objects
   // (without adding them to the manager) to check what class
   // they are.
   modifiedKey = ++sNextModifiedKey;
   AssertFatal(sNextObjectId <= DataBlockObjectIdLast,
               "Exceeded maximum number of data blocks");
   
   // add DataBlock to the DataBlockGroup unless it is client side ONLY DataBlock
   if (getId() >= DataBlockObjectIdFirst && getId() <= DataBlockObjectIdLast)
      if (SimGroup* grp = Sim::getDataBlockGroup())
         grp->addObject(this);
   
   return true;
}

//-----------------------------------------------------------------------------

void SimDataBlock::assignId()
{
   // We don't want the id assigned by the manager, but it may have
   // already been assigned a correct data block id.
   if ( isClientOnly() )
      setId(sNextObjectId++);
}

//-----------------------------------------------------------------------------

void SimDataBlock::onStaticModified(const char* slotName, const char* newValue)
{
   modifiedKey = sNextModifiedKey++;
}

//-----------------------------------------------------------------------------

void SimDataBlock::packData(BitStream*)
{
}

//-----------------------------------------------------------------------------

void SimDataBlock::unpackData(BitStream*)
{
}

//-----------------------------------------------------------------------------

bool SimDataBlock::preload(bool, char errorStr[256])
{
   return true;
}

//-----------------------------------------------------------------------------

/*! Use the deleteDataBlocks function to cause a server to delete all datablocks that have thus far been loaded and defined.
    This is usually done in preparation of downloading a new set of datablocks, such as occurs on a mission change, but it's also good post-mission cleanup
    @return No return value.
*/
ConsoleFunction(deleteDataBlocks, void, 1, 1, "")
{
   // delete from last to first:
   SimGroup *grp = Sim::getDataBlockGroup();
   for(S32 i = grp->size() - 1; i >= 0; i--)
   {
      SimObject *obj = (*grp)[i];
      obj->deleteObject();
   }
   SimDataBlock::sNextObjectId = DataBlockObjectIdFirst;
   SimDataBlock::sNextModifiedKey = 0;
}

//-----------------------------------------------------------------------------

void SimDataBlock::write(Stream &stream, U32 tabStop, U32 flags)
{
   // Only output selected objects if they want that.
   if((flags & SelectedOnly) && !isSelected())
      return;
   
   stream.writeTabs(tabStop);
   char buffer[1024];
   
   // Client side datablocks are created with 'new' while
   // regular server datablocks use the 'datablock' keyword.
   if ( isClientOnly() )
      dSprintf(buffer, sizeof(buffer), "new %s(%s) {\r\n", getClassName(), getName() ? getName() : "");
   else
      dSprintf(buffer, sizeof(buffer), "datablock %s(%s) {\r\n", getClassName(), getName() ? getName() : "");
   
   stream.write(dStrlen(buffer), buffer);
   writeFields(stream, tabStop + 1);
   
   stream.writeTabs(tabStop);
   stream.write(4, "};\r\n");
}

// END T2D+T3D BLOCK

//---------------------------------------------------------------------------

// BEGIN T2D BLOCK
//////////////////////////////////////////////////////////////////////////
// Sim Set
//////////////////////////////////////////////////////////////////////////

void SimSet::addObject(SimObject* obj)
{
   lock();
   objectList.push_back(obj);
   deleteNotify(obj);
   unlock();
}

void SimSet::removeObject(SimObject* obj)
{
   lock();
   auto itr = std::find(objectList.begin(), objectList.end(), obj);
   if (itr != objectList.end())
   {
      objectList.erase(itr);
   }
   clearNotify(obj);
   unlock();
}

void SimSet::pushObject(SimObject* pObj)
{
   lock();
   objectList.push_back(pObj);
   deleteNotify(pObj);
   unlock();
}

void SimSet::popObject()
{
   MutexHandle handle;
   handle.lock(mMutex);

   if (objectList.size() == 0) 
   {
      AssertWarn(false, "Stack underflow in SimSet::popObject");
      return;
   }

   SimObject* pObject = objectList.back();
   objectList.pop_back();
   clearNotify(pObject);
}

//-----------------------------------------------------------------------------

void SimSet::callOnChildren( const char * method, S32 argc, const char *argv[], bool executeOnChildGroups )
{
   // Prep the arguments for the console exec...
   // Make sure and leave args[1] empty.
   const char* args[21];
   args[0] = method;
   for (S32 i = 0; i < argc; i++)
      args[i + 2] = argv[i];

   for( iterator i = begin(); i != end(); i++ )
   {
      SimObject *childObj = static_cast<SimObject*>(*i);

      if( childObj->isMethod( method ) )
         Con::execute(childObj, argc + 2, args);

      if( executeOnChildGroups )
      {
         SimSet* childSet = dynamic_cast<SimSet*>(*i);
         if ( childSet )
            childSet->callOnChildren( method, argc, argv, executeOnChildGroups );
      }
   }
}

bool SimSet::reOrder( SimObject *obj, SimObject *target )
{
   MutexHandle handle;
   handle.lock(mMutex);

   iterator itrS, itrD;
   if ( (itrS = find(begin(),end(),obj)) == end() )
   {
      return false;  // object must be in list
   }

   if ( obj == target )
   {
      return true;   // don't reorder same object but don't indicate error
   }

   if ( !target )    // if no target, then put to back of list
   {
      if ( itrS != (end()-1) )      // don't move if already last object
      {
         objectList.erase(itrS);    // remove object from its current location
         objectList.push_back(obj); // push it to the back of the list
      }
   }
   else              // if target, insert object in front of target
   {
      if ( (itrD = find(begin(),end(),target)) == end() )
         return false;              // target must be in list

      objectList.erase(itrS);

      //Tinman - once itrS has been erased, itrD won't be pointing at the same place anymore - re-find...
      itrD = find(begin(),end(),target);
      objectList.insert(itrD,obj);
   }

   return true;
}   

void SimSet::onDeleteNotify(SimObject *object)
{
   removeObject(object);
   Parent::onDeleteNotify(object);
}

void SimSet::onRemove()
{
   MutexHandle handle;
   handle.lock(mMutex);

   std::sort(objectList.begin(), objectList.end(), SortSimObjectList);
   
   if (objectList.size())
   {
      // This backwards iterator loop doesn't work if the
      // list is empty, check the size first.
      for (SimObjectList::iterator ptr = objectList.end() - 1;
         ptr >= objectList.begin(); ptr--)
      {
         clearNotify(*ptr);
      }
   }

   handle.unlock();

   Parent::onRemove();
}

void SimSet::write(Stream &stream, U32 tabStop, U32 flags)
{
   MutexHandle handle;
   handle.lock(mMutex);

   // export selected only?
   if((flags & SelectedOnly) && !isSelected())
   {
      for(U32 i = 0; i < (U32)size(); i++)
         (*this)[i]->write(stream, tabStop, flags);

      return;

   }

   stream.writeTabs(tabStop);
   char buffer[1024];
   dSprintf(buffer, sizeof(buffer), "new %s(%s) {\r\n", getClassName(), getName() ? getName() : "");
   stream.write(dStrlen(buffer), buffer);
   writeFields(stream, tabStop + 1);

   if(size())
   {
      stream.write(2, "\r\n");
      for(U32 i = 0; i < (U32)size(); i++)
         (*this)[i]->write(stream, tabStop + 1, flags);
   }

   stream.writeTabs(tabStop);
   stream.write(4, "};\r\n");
}


/*! @class SimSet

   A container for a sequence of unique SimObjects.

   @par Overview

   A SimSet is a specized container: an ordered set of references to SimObjects.
   As the name "set" implies, a SimObject can appear no more than once within a particular SimSet.
   Attempting to add an object multiple times will not change the SimSet not does it (nor should it) warn you.
   A SimSet keeps its items ordered, however, so it is more than a mathematical "set."  You can reorder the
   objects.
   
   A Simset only *references* SimObjects.  The deletion of a SimSet will not delete the objects in it.
   Likewise, removing an object from a SimSet does not delete that object.
   Note that a SimObject can be a member of any number of SimSets.

   When a SimObject is deleted, it will be automatically removed from any SimSets it is in.
   This is one of a SimSets most powerful features.  There can be no invalid references to objects
   because you can not insert a non-existent reference, and references to SimObjects are automatically removed
   when those objects are deleted.

   *Due to its many capabilities, a SimSet is usually the appropriate structure for keeping Collections in Torque2D.*

   Note that only SimObjects can be held in SimSets.  Strings, for instance, can not.
   But, because SimObject is the base class for almost all script classes, you can add almost any script class to a SimSet.

   The SimSet's member objects are stored initially in the order of insertion (add()),
   and can be removed (remove()), retrieved (getObject()), and queried (isMember()).
   The SimSet can have all its members counted (getCount()), printed (listObjects()), and removed (clear()).
   A member can be reordered via bringToFront() and pushToBack(), or re-ordered relative to another via reorderChild().

   @par Examples

   **Creating and Adding**

   We create the SimSet, then create objects to put in it, then we add them all in.

   @code
   new SimSet(Fruit);
   echo(Fruit.getCount());
   > 0

   // Apple, Pear, etc will be SimObjects
   new SimObject(Apple);
   new SimObject(Pear);
   new SimObject(Orange);
   new SimObject(Fig);
   new SimObject(Persimmon);

   // add our fruit to the SimSet named Fruit
   Fruit.add(Apple);
   Fruit.add(Pear);
   Fruit.add(Orange);
   Fruit.add(Fig);
   Fruit.add(Persimmon);
   echo(Fruit.getCount());
   > 5
   @endcode

   **Uniqueness**

   Continuing the above example, each member of the SimSet appears exactly once: the SimSet is a mathematically proper set.

   @code
   // Apple is already added.
   Fruit.add(Apple);
   echo(Fruit.getCount());
   > 5
   @endcode

   **Re-ordering**

   The members of a SimSet are well ordered. Let us move a different object to the front.

   @code
   echo(Fruit.getObject(0).getName());
   > Apple

   Fruit.bringToFront(Persimmon);
   echo(Fruit.getObject(0).getName());
   > Persimmon
   @endcode

   Now we move a different member to the back.

   @code
   Fruit.pushToBack(Apple);
   echo(Fruit.getObject(4).getName());
   > Apple
   @endcode

   Finally, we move the Fig member to precede Pear. Note that all of the other members retain their relative order.

   @code
   Fruit.listObjects();
   > 2887,"Persimmon": SimObject 
   > 2884,"Pear": SimObject 
   > 2885,"Orange": SimObject 
   > 2886,"Fig": SimObject 
   > 2883,"Apple": SimObject 

   Fruit.reorderChild(Fig, Pear);
   Fruit.listObjects();
   > 2887,"Persimmon": SimObject 
   > 2886,"Fig": SimObject 
   > 2885,"Orange": SimObject 
   > 2884,"Pear": SimObject 
   > 2883,"Apple": SimObject 
   @endcode

   **Removing**

   @code
   echo(Fruit.isMember(Apple));
   > 1

   Fruit.remove(Apple);
   echo(Fruit.isMember(Apple));
   > 0

   // Apple was not deleted
   echo(isObject(Apple));
   > 1

   Fruit.clear();
   echo(Fruit.getCount());
   > 0

   // The fruit SimObjects are not deleted by clear() either.  For example...
   echo(isObject(Persimmon));
   > 1
   @endcode

   @par caveat

   Suppose you want to delete the items in a SimSet.  Remember that, as you delete each one, it is automatically
   removed from the SimSet, which in turn changes the index of any items that came after the one you just deleted!

   @code
   // DO NOT DO THIS!   this may lead to tears!
   for (%i = 0; %i < %mySet.getCount(); %i++)
   {
      %object = %mySet.getObject(%i);
      %object.delete();
   }
   @endcode

   The problem is that you delete the object at index 0.  This, in turn, moves the objects down one index so
   that what was at index 1 is not at index 0.  But your loop will not try index 0 again, where there is a fresh
   object.  Instead it will skip to index 1.  You will only delete half the items.

   @code
   // fixed it
   while (%mySet.getCount() > 0)
   {
      %object = %mySet.getObject(0);
      %object.delete();
    }

   // or this will work too.  see why?
   for (%i = %mySet.getCount() - 1; %i >= 0; %i--)
   {
      %object = %mySet.getObject(%i);
      %object.delete();
    }
   @endcode

*/

/*! Prints the object data within the set
    @return No return value
*/
ConsoleMethod(SimSet, listObjects, void, 2, 2, "")
{
   object->lock();
   SimSet::iterator itr;
   for(itr = object->begin(); itr != object->end(); itr++)
   {
      SimObject *obj = *itr;
      bool isSet = dynamic_cast<SimSet *>(obj) != 0;
      const char *name = obj->getName();
      if(name)
         Con::printf("   %d,\"%s\": %s %s", obj->getId(), name,
         obj->getClassName(), isSet ? "(g)":"");
      else
         Con::printf("   %d: %s %s", obj->getId(), obj->getClassName(),
         isSet ? "(g)" : "");
   }
   object->unlock();
}


/*! Appends given SimObject (or list of SimObjects) to the SimSet.
    @param obj_1..obj_n list of SimObjects to add
    @return No return value
*/
ConsoleMethod(SimSet, add, void, 3, 0, "obj1, [obj2]*")
{
   for(S32 i = 2; i < argc; i++)
   {
      SimObject *obj = Sim::findObject(argv[i]);
      if(obj)
         object->addObject(obj);
      else
         Con::printf("Set::add: Object \"%s\" doesn't exist", argv[i]);
   }
}

/*! Removes given SimObject (or list of SimObjects) from the SimSet.
    @param obj_1..obj_n list of SimObjects to remove
   The SimObjects are not deleted.  An attempt to remove a SimObject that is not present
   in the SimSet will print a warning and continue.
    @return No return value
*/
ConsoleMethod(SimSet, remove, void, 3, 0, "obj1, [obj2]*")
{
   for(S32 i = 2; i < argc; i++)
   {
      SimObject *obj = Sim::findObject(argv[i]);
      object->lock();
      if(obj && object->find(object->begin(),object->end(),obj) != object->end())
         object->removeObject(obj);
      else
         Con::printf("Set::remove: Object \"%s\" does not exist in set", argv[i]);
      object->unlock();
   }
}

//-----------------------------------------------------------------------------

/*! Clears the Simset
   This does not delete the cleared SimObjects.
    @return No return value
*/
ConsoleMethod(SimSet, clear, void, 2, 2, "")
{
   object->clear();
}

//-----------------------------------------------------------------------------

/*! Deletes all the objects in the SimSet.
    @return No return value
*/
ConsoleMethod(SimSet, deleteObjects, void, 2, 2, "")
{
    object->deleteObjects();
}

//-----------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////-
// Make Sure Child 1 is Ordered Just Under Child 2.
//////////////////////////////////////////////////////////////////////////-
/*! Bring child 1 before child 2
   Both SimObjects must already be child objects.  If not, the operation silently fails.
    @param child1 The child you wish to set first
    @param child2 The child you wish to set after child1
    @return No return value.
*/
ConsoleMethod(SimSet, reorderChild, void, 4,4,  "SimObject child1, SimObject child2")
{
   SimObject* pObject = Sim::findObject(argv[2]);
   SimObject* pTarget    = Sim::findObject(argv[3]);

   if(pObject && pTarget)
   {
      object->reOrder(pObject,pTarget);
   }
}

/*! @return Returns the number of objects in the SimSet
*/
ConsoleMethod(SimSet, getCount, S32, 2, 2, "")
{
   return object->size();
}

/*! Returns a member SimObject of the SimSet
   @param index into this ordered collection (zero-based).
   @return Returns the ID of the desired object or -1 on failure
*/
ConsoleMethod(SimSet, getObject, S32, 3, 3, "index")
{
   S32 objectIndex = dAtoi(argv[2]);
   if(objectIndex < 0 || objectIndex >= S32(object->size()))
   {
      Con::printf("Set::getObject index out of range.");
      return -1;
   }
   return ((*object)[objectIndex])->getId();
}

/*! @return Returns true if specified object is a member of the set, and false otherwise
*/
ConsoleMethod(SimSet, isMember, bool, 3, 3, "object")
{
   SimObject *testObject = Sim::findObject(argv[2]);
   if(!testObject)
   {
      Con::printf("SimSet::isMember: %s is not an object.", argv[2]);
      return false;
   }

   object->lock();
   for(SimSet::iterator i = object->begin(); i != object->end(); i++)
   {
      if(*i == testObject)
      {
         object->unlock();
         return true;
      }
   }
   object->unlock();

   return false;
}

/*! Brings SimObject to front of set.
   If the SimObject is not in the set, do nothing.
    @return No return value.
*/
ConsoleMethod(SimSet, bringToFront, void, 3, 3, "object")
{
   SimObject *obj = Sim::findObject(argv[2]);
   if(!obj)
      return;
   object->bringObjectToFront(obj);
}

/*! Sends item to back of set.
   If the SimObject is not in the set, do nothing.
    @return No return value.
*/
ConsoleMethod(SimSet, pushToBack, void, 3, 3, "object")
{
   SimObject *obj = Sim::findObject(argv[2]);
   if(!obj)
      return;
   object->pushObjectToBack(obj);
}

// Also known as reorderChild
ConsoleMethod(SimSet, reorder, void, 4,4,  "SimObject child1, SimObject child2")
{
   SimObject* pObject = Sim::findObject(argv[2]);
   SimObject* pTarget    = Sim::findObject(argv[3]);

   if(pObject && pTarget)
   {
      object->reOrder(pObject,pTarget);
   }
}

/*! Call a method on all objects contained in the set.
   @param method The name of the method to call.
   @param args The arguments to the method.
   @note This method recurses into all SimSets that are children to the set.
   @see callOnChildrenNoRecurse" )
*/
ConsoleMethod(SimSet, callOnChildren, void, 3, 0, "string method, [string args]* ")
{
   object->callOnChildren( argv[2], argc - 3, argv + 3 );
}

IMPLEMENT_CONOBJECT_CHILDREN(SimSet);


void SimSet::deleteObjects( void )
{
    lock();
        while(size() > 0 )
        {
            objectList[0]->deleteObject();
        }
    unlock();
}

void SimSet::clear()
{
   lock();
   while (size() > 0)
      removeObject(objectList.back());
   unlock();
}

//////////////////////////////////////////////////////////////////////////

// END T2D BLOCK

//////////////////////////////////////////////////////////////////////////
// SimGroup
//////////////////////////////////////////////////////////////////////////

SimGroup::~SimGroup()
{
   lock();
   for (iterator itr = begin(); itr != end(); itr++)
      nameDictionary.remove(*itr);

   // XXX Move this later into Group Class
   // If we have any objects at this point, they should
   // already have been removed from the manager, so we
   // can just delete them directly.
   std::sort(objectList.begin(), objectList.end(), SortSimObjectList);
   
   while (!objectList.empty())
   {
      delete objectList.back();
      objectList.pop_back();
   }

   unlock();
}


//////////////////////////////////////////////////////////////////////////

void SimGroup::addObject(SimObject* obj)
{
   lock();

   // Make sure we aren't adding ourself.  This isn't the most robust check
   // but it should be good enough to prevent some self-foot-shooting.
   if(obj == this)
   {
      Con::errorf("SimGroup::addObject - (%d) can't add self!", getIdString());
      unlock();
      return;
   }

   if (obj->mGroup != this) 
   {
      if (obj->mGroup)
         obj->mGroup->removeObject(obj);
      nameDictionary.insert(obj);
      obj->mGroup = this;
      objectList.push_back(obj); // force it into the object list
      // doesn't get a delete notify
      obj->onGroupAdd();
   }
   unlock();
}

void SimGroup::removeObject(SimObject* obj)
{
   lock();
   if (obj->mGroup == this) 
   {
      obj->onGroupRemove();
      nameDictionary.remove(obj);
      auto itr = std::find(objectList.begin(), objectList.end(), obj);
      if (itr != objectList.end())
      {
         objectList.erase(itr);
      }
      obj->mGroup = 0;
   }
   unlock();
}

//////////////////////////////////////////////////////////////////////////

void SimGroup::onRemove()
{
   lock();
   std::sort(objectList.begin(), objectList.end(), SortSimObjectList);
   if (objectList.size())
   {
      // This backwards iterator loop doesn't work if the
      // list is empty, check the size first.
      for (SimObjectList::iterator ptr = objectList.end() - 1;
         ptr >= objectList.begin(); ptr--)
      {
          // T2DJUNK WE NEED THIS? if ( (*ptr)->isProperlyAdded() )
          {
             (*ptr)->onGroupRemove();
             (*ptr)->mGroup = NULL;
             (*ptr)->unregisterObject();
             (*ptr)->mGroup = this;
          }
      }
   }
   SimObject::onRemove();
   unlock();
}

//////////////////////////////////////////////////////////////////////////

SimObject *SimGroup::findObject(const char *namePath)
{
   // find the end of the object name
   S32 len;
   for(len = 0; namePath[len] != 0 && namePath[len] != '/'; len++)
      ;

   StringTableEntry stName = StringTable->lookupn(namePath, len);
   if(!stName)
      return NULL;

   SimObject *root = nameDictionary.find(stName);

   if(!root)
      return NULL;

   if(namePath[len] == 0)
      return root;

   return root->findObject(namePath + len + 1);
}

SimObject *SimSet::findObject(const char *namePath)
{
   // find the end of the object name
   S32 len;
   for(len = 0; namePath[len] != 0 && namePath[len] != '/'; len++)
      ;

   StringTableEntry stName = StringTable->lookupn(namePath, len);
   if(!stName)
      return NULL;

   lock();
   for(SimSet::iterator i = begin(); i != end(); i++)
   {
      if((*i)->getName() == stName)
      {
         unlock();
         if(namePath[len] == 0)
            return *i;
         return (*i)->findObject(namePath + len + 1);
      }
   }
   unlock();
   return NULL;
}

SimObject* SimObject::findObject(const char* )
{
   return NULL;
}

//////////////////////////////////////////////////////////////////////////

bool SimGroup::processArguments(S32, const char **)
{
   return true;
}

/*! Returns the object with given internal name
    @param name The internal name of the object you wish to find
    @param searchChildren Set this true if you wish to search all children as well.
    @return Returns the ID of the object.
*/
ConsoleMethod( SimSet, findObjectByInternalName, S32, 3, 4, "string name, [bool searchChildren]?")
{

   StringTableEntry pcName = StringTable->insert(argv[2]);
   bool searchChildren = false;
   if (argc > 3)
      searchChildren = dAtob(argv[3]);

   SimObject* child = object->findObjectByInternalName(pcName, searchChildren);
   if(child)
      return child->getId();
   return 0;
}

SimObject* SimSet::findObjectByInternalName(const char* internalName, bool searchChildren)
{
   iterator i;
   for (i = begin(); i != end(); i++)
   {
      SimObject *childObj = static_cast<SimObject*>(*i);
      if(childObj->getInternalName() == internalName)
         return childObj;
      else if (searchChildren)
      {
         SimSet* childSet = dynamic_cast<SimSet*>(*i);
         if (childSet)
         {
            SimObject* found = childSet->findObjectByInternalName(internalName, searchChildren);
            if (found) return found;
         }
      }
   }

   return NULL;
}

//////////////////////////////////////////////////////////////////////////

IMPLEMENT_CONOBJECT(SimGroup);
//IMPLEMENT_CONOBJECT(SimSet);

// END T2D BLOCK

//----------------------------------------------------------------------------


// BEGIN T2D BLOCK

//-----------------------------------------------------------------------------

SimConsoleEvent::SimConsoleEvent(S32 argc, const char **argv, bool onObject)
{
   mOnObject = onObject;
   mArgc = argc;
   U32 totalSize = 0;
   S32 i;
   for(i = 0; i < argc; i++)
      totalSize += dStrlen(argv[i]) + 1;
   totalSize += sizeof(char *) * argc;

   mArgv = (char **) malloc(totalSize);
   char *argBase = (char *) &mArgv[argc];

   for(i = 0; i < argc; i++)
   {
      mArgv[i] = argBase;
      dStrcpy(mArgv[i], argv[i]);
      argBase += dStrlen(argv[i]) + 1;
   }
}

SimConsoleEvent::~SimConsoleEvent()
{
   free(mArgv);
}

void SimConsoleEvent::process(SimObject* object)
{
// #ifdef DEBUG
//    Con::printf("Executing schedule: %d", sequenceCount);
// #endif
    if(mOnObject)
      Con::execute(object, mArgc, const_cast<const char**>( mArgv ));
   else
   {
      // Grab the function name. If '::' doesn't exist, then the schedule is
      // on a global function.
      char* func = dStrstr( mArgv[0], (char*)"::" );
      if( func )
      {
         // Set the first colon to NULL, so we can reference the namespace.
         // This is okay because events are deleted immediately after
         // processing. Maybe a bad idea anyway?
         func[0] = '\0';

         // Move the pointer forward to the function name.
         func += 2;
         
         KorkApi::NamespaceId nsId = sVM->findNamespace(sVM->internString(mArgv[0]));
         if (nsId != 0)
         {
            KorkApi::ConsoleValue localArgv[KorkApi::MaxArgs];
            KorkApi::ConsoleValue::convertArgsReverse(mArgc, (const char**)mArgv, localArgv);
            
            KorkApi::ConsoleValue retV = KorkApi::ConsoleValue();
            sVM->callNamespaceFunction(nsId, sVM->internString(func), mArgc, localArgv, retV);
         }
      }
      else
      {
         KorkApi::ConsoleValue localArgv[KorkApi::MaxArgs];
         KorkApi::ConsoleValue::convertArgsReverse(mArgc, (const char**)mArgv, localArgv);
         KorkApi::ConsoleValue retV = KorkApi::ConsoleValue();
         sVM->callNamespaceFunction(sVM->getGlobalNamespace(), sVM->internString(func), mArgc, localArgv, retV);
         Con::execute(mArgc, const_cast<const char**>( mArgv ));
      }
   }
}

// END T2D BLOCK

//-----------------------------------------------------------------------------

// BEGIN T2D BLOCK

SimConsoleThreadExecCallback::SimConsoleThreadExecCallback() : retVal(NULL)
{
   sem = Semaphore::createSemaphore(0);
}

SimConsoleThreadExecCallback::~SimConsoleThreadExecCallback()
{
   Semaphore::destroySemaphore(sem);
}

void SimConsoleThreadExecCallback::handleCallback(const char *ret)
{
   retVal = ret;
   Semaphore::releaseSemaphore(sem);
}

const char *SimConsoleThreadExecCallback::waitForResult()
{
   if(Semaphore::acquireSemaphore(sem, true))
   {
      return retVal;
   }

   return NULL;
}

//-----------------------------------------------------------------------------

SimConsoleThreadExecEvent::SimConsoleThreadExecEvent(S32 argc, const char **argv, bool onObject, SimConsoleThreadExecCallback *callback) : 
   SimConsoleEvent(argc, argv, onObject),
   cb(callback)
{
}

void SimConsoleThreadExecEvent::process(SimObject* object)
{
   const char *retVal;
   if(mOnObject)
      retVal = Con::execute(object, mArgc, const_cast<const char**>( mArgv ));
   else
      retVal = Con::execute(mArgc, const_cast<const char**>( mArgv ));

   if(cb)
      cb->handleCallback(retVal);
}

// END T2D BLOCK


// BEGIN T2D BLOCK

//-----------------------------------------------------------------------------

/*! get the time, in ticks, that has elapsed since the engine started executing.
 
 @return the time in ticks since the engine was started.
 @sa getRealTime
 
 @boundto
 Sim::getCurrentTime
 */
ConsoleFunction( getSimTime, S32, 1, 1, "")
{
   return Sim::getCurrentTime();
}

//-----------------------------------------------------------------------------


inline void SimSetIterator::push_back_stack(SimSet* set)
{
   Entry e;
   e.set = set;
   e.itr = set->begin();
   stack.push_back(e);
}


//-----------------------------------------------------------------------------

SimSetIterator::SimSetIterator(SimSet* set)
{
   if (!set->empty())
      push_back_stack(set);
}

//-----------------------------------------------------------------------------

SimObject* SimSetIterator::operator++()
{
   SimSet* set;
   if ((set = dynamic_cast<SimSet*>(*stack.back().itr)) != 0) 
   {
      if (!set->empty()) 
      {
         push_back_stack(set);
         return *stack.back().itr;
      }
   }

   while (++stack.back().itr == stack.back().set->end()) 
   {
      stack.pop_back();
      if (stack.empty())
         return 0;
   }
   return *stack.back().itr;
}  

SimObject* SimGroupIterator::operator++()
{
   SimGroup* set;
   if ((set = dynamic_cast<SimGroup*>(*stack.back().itr)) != 0) 
   {
      if (!set->empty()) 
      {
         push_back_stack(set);
         return *stack.back().itr;
      }
   }

   while (++stack.back().itr == stack.back().set->end()) 
   {
      stack.pop_back();
      if (stack.empty())
         return 0;
   }
   return *stack.back().itr;
}  

// END T2D BLOCK
