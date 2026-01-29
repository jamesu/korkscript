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

#include "embed/api.h"
#include "embed/internalApi.h"
#include "console/consoleNamespace.h"


#include "console/consoleInternal.h"
#include "console/compiler.h"


/// Helper table to convert type ids to human readable names.
const char *typeNames[] = 
{
      "Script",
      "string",
      "int",
      "float",
      "void",
      "bool",
      "",
      "",
      "unknown_overload"
};

void printClassHeader(const char* usage, const char * className, const char * superClassName, const bool stub)
{
   if(stub) 
   {
      printf("/// Stub class");
      printf("/// ");
      printf("/// @note This is a stub class to ensure a proper class hierarchy. No ");
      printf("///       information was available for this class.");
   }

   if( usage != NULL )
   {
      // Copy Usage Document
      S32 usageLen = strlen( usage );
      KorkApi::Vector<char> usageStrV( usageLen );
      char* usageStr = usageStrV.data();
      strcpy( usageStr, usage );

      // Print Header
      printf( "/*!" );

      // Print line by line, skipping the @field lines.
      //
      // fetch first line end
      char *newLine = strchr( usageStr, '\n' );
      char *usagePtr = usageStr;
      do 
      {
         // Copy of one line
         static char lineStr[2048] = {0};
         // Keyword will hold the last keyword (word following '@' or '\') encountered.
         static char keyword[8] = {0};

         S32 lineLen = 0;

         // If not the last line, increment pointer
         if( newLine != NULL )
         {
            *newLine = '\0';
            newLine ++;
         }
         
         // Copy line and update usagePtr
         strcpy( lineStr, usagePtr );
         usagePtr = (newLine != NULL ) ? newLine : usagePtr;
         lineLen = strlen( lineStr );

         // Get the keyword. This is the first word after an '@' or '\'.
         const char* tempkw = strchr( lineStr, '@' );
         if( !tempkw )
            tempkw = strchr( lineStr, '\\' );

         // If we found a new keyword, set it, otherwise, keep using the
         // most recently found.
         if( tempkw )
         {
            strncpy( keyword, tempkw + 1, 5 );
            keyword[5] = '\0';
         }

         // Print all fields that aren't associated with the 'field' keyword.
         if( strcmp( keyword, "field" ) )
            printf( lineStr );


         // Fetch next line ending
         newLine = strchr( usagePtr, '\n' );
      } while( newLine != NULL );

      // DocBlock Footer
      printf( " */" );

   }

   // Print out appropriate class header
   if(superClassName)
      printf("class  %s : public %s {", className, superClassName ? superClassName : "");
   else if(!className)
      printf("namespace Global {");
   else
      printf("class  %s {", className);

   if(className)
      printf("  public:");

}

void printClassMethod(const bool isVirtual, const char *retType, const char *methodName, const char* args, const char*usage)
{
   if(usage && usage[0] != ';' && usage[0] != 0)
      printf("   /*! %s */", usage);
   printf("   %s%s %s(%s) {}", isVirtual ? "virtual " : "", retType, methodName, args);
}

void printGroupStart(const char * aName, const char * aDocs)
{
   printf("");
   printf("   /*! @name %s", aName);

   if(aDocs)
   {
      printf("   ");
      printf("   %s", aDocs);
   }

   printf("   @{ */");
}

void printClassMember(const bool isDeprec, const char * aType, const char * aName, const char * aDocs)
{
   printf("   /*!");

   if(aDocs)
   {
      printf("   %s", aDocs);
      printf("   ");
   }

   if(isDeprec)
      printf("   @deprecated This member is deprecated, which means that its value is always undefined.");

   printf("    */");

   printf("   %s %s;", isDeprec ? "deprecated" : aType, aName);
}

void printGroupEnd()
{
   printf("   /// @}");
   printf("");
}

void printClassFooter()
{
   printf("};");
   printf("");
}

void NamespaceState::printNamespaceEntries(Namespace * g, bool dumpScript, bool dumpEngine )
{
   static bool inGroup = false;

   // Go through all the entries.
   // Iterate through the methods of the namespace...
   for(Namespace::Entry *ewalk = g->mEntryList; ewalk; ewalk = ewalk->mNext)
   {
      char buffer[1024]; //< This will bite you in the butt someday.
      int eType = ewalk->mType;
      const char * funcName = ewalk->mFunctionName;

      if( ( eType == Namespace::Entry::ScriptFunctionType ) && !dumpScript )
         continue;

      if( ( eType != Namespace::Entry::ScriptFunctionType ) && !dumpEngine )
         continue;

      // If it's a function
      if(eType >= Namespace::Entry::ScriptFunctionType || eType == Namespace::Entry::OverloadMarker)
      {
         if(eType == Namespace::Entry::OverloadMarker)
         {
            // Deal with crap from the OverloadMarker case.
            // It has no type information so we have to "correct" its type.

            // Find the original
            eType = 8;
            for(Namespace::Entry *eseek = g->mEntryList; eseek; eseek = eseek->mNext)
            {
               if(!strcmp(eseek->mFunctionName, ewalk->cb.mGroupName))
               {
                  eType = eseek->mType;
                  break;
               }
            }
            // And correct the name
            funcName = ewalk->cb.mGroupName;
         }

         // A quick note  - if your usage field starts with a (, then it's auto-integrated into
         // the script docs! Use this HEAVILY!

         // We add some heuristics here as well. If you're of the form:
         // *.methodName(*)
         // then we will also extract parameters.

         const char *use = ewalk->getUsage();
         const char *bgn = strchr(use, '(');
         const char *end = strchr(use, ')');
         const char *dot = strchr(use, '.');

         while( *use == ' ' )
            use++;

         if(use[0] == '(')
         {
            if(!end)
               end = use + 1;

            use++;
            
            U32 len = (U32)(end - use);
            strncpy(buffer, use, len);
            buffer[len] = 0;

            printClassMethod(true, typeNames[eType], funcName, buffer, end+1);

            continue; // Skip to next one.
         }

         // We check to see if they're giving a prototype.
         if(dot && bgn && end)       // If there's two parentheses, and a dot...
         if(dot < bgn && bgn < end)  // And they're in the order dot, bgn, end...
         {
            use++;
            U32 len = (U32)(end - bgn - 1);
            strncpy(buffer, bgn+1, len);
            buffer[len] = 0;

            // Then let's do the heuristic-trick
            printClassMethod(true, typeNames[eType], funcName, buffer, end+1);
            continue; // Get to next item.
         }

         // Finally, see if they did it foo(*) style.
         const char* func_pos = strstr(use, funcName);
         if((func_pos) && (func_pos < bgn) && (end > bgn))
         {
            U32 len = (U32)(end - bgn - 1);
            strncpy(buffer, bgn+1, len);
            buffer[len] = 0;

            printClassMethod(true, typeNames[eType], funcName, buffer, end+1);
            continue;
         }

         // Default...
         printClassMethod(true, typeNames[eType], funcName, "", ewalk->getUsage());
      }
      else if(ewalk->mType == Namespace::Entry::GroupMarker)
      {
         if(!inGroup)
            printGroupStart(ewalk->cb.mGroupName, ewalk->getUsage());
         else
            printGroupEnd();

         inGroup = !inGroup;
      }
      else if(ewalk->mFunctionOffset)                 // If it's a builtin function...
      {
         ewalk->mCode->getFunctionArgs(buffer, ewalk->mFunctionOffset);
         printClassMethod(false, typeNames[ewalk->mType], ewalk->mFunctionName, buffer, "");
      }
      else
      {
         printf("   // got an unknown thing?? %d", ewalk->mType );
      }
   }

}

void NamespaceState::dumpClasses( bool dumpScript, bool dumpEngine )
{
   KorkApi::Vector<Namespace*> vec;
   trashCache();
   vec.reserve( 1024 );

   // We use mHashSequence to mark if we have traversed...
   // so mark all as zero to start.
   for(Namespace *walk = mNamespaceList; walk; walk = walk->mNext)
      walk->mHashSequence = 0;

   for(Namespace *walk = mNamespaceList; walk; walk = walk->mNext)
   {
      KorkApi::Vector<Namespace*> stack;
      stack.reserve( 1024 );

      // Get all the parents of this namespace... (and mark them as we go)
      Namespace *parentWalk = walk;
      while(parentWalk)
      {
         if(parentWalk->mHashSequence != 0)
            break;
         if(parentWalk->mPackage == 0)
         {
            parentWalk->mHashSequence = 1;   // Mark as traversed.
            stack.push_back(parentWalk);
         }
         parentWalk = parentWalk->mParent;
      }

      // Load stack into our results vector.
      while(stack.size())
      {
         vec.push_back(stack[stack.size() - 1]);
         stack.pop_back();
      }
   }

   // Go through previously discovered classes
   U32 i;
   for(i = 0; i < (U32)vec.size(); i++)
   {
      const char *className = vec[i]->mName;
      const char *superClassName = vec[i]->mParent ? vec[i]->mParent->mName : NULL;

      // Skip the global namespace, that gets dealt with in dumpFunctions
      if(!className) continue;

      // If we're just dumping script functions, then we don't want to dump
      // a class that only contains script functions. So, we iterate over all
      // the functions.
      if( !dumpScript )
      {
         bool found = false;
         for(Namespace::Entry *ewalk = vec[i]->mEntryList; ewalk; ewalk = ewalk->mNext)
         {
            if( ewalk->mType != Namespace::Entry::ScriptFunctionType )
            {
               found = true;
               break;
            }
         }
         if( !found )
            continue;
      }

      // And we do the same for engine functions.
      if( !dumpEngine )
      {
         bool found = false;
         for(Namespace::Entry *ewalk = vec[i]->mEntryList; ewalk; ewalk = ewalk->mNext)
         {
            if( ewalk->mType == Namespace::Entry::ScriptFunctionType )
            {
               found = true;
               break;
            }
         }
         if( !found )
            continue;
      }

      // Filter useless namespaces
      if(vec[i]->mEntryList == NULL)// && vec[i]->mClassRep == NULL)
      {
         // Print out a short stub so we get a proper class hierarchy.
         if(superClassName) { // Filter hack; we don't want non-inheriting classes...
            printClassHeader( NULL, className,superClassName, true);
            printClassFooter();
         }
         continue;
      }

      // Print the header for the class..
      printClassHeader(vec[i]->getUsage(), className, superClassName, false);

      // Deal with entries.
      printNamespaceEntries(vec[i], dumpScript, dumpEngine);

      KorkApi::ClassInfo* classInfo = mVmInternal->getClassInfoByName(vec[i]->mName);

      // Since all fields are defined in the engine, if we're not dumping
      // engine stuff, than we shouldn't dump the fields.
      if(dumpEngine && classInfo)
      {
         // Get information about the parent's fields...
         KorkApi::ClassInfo* parentClassInfo = vec[i]->mParent ? mVmInternal->getClassInfoByName(vec[i]->mName) : NULL;

         // Go through all our fields...
         for(U32 j = 0; j < (U32)classInfo->numFields; j++)
         {
            KorkApi::FieldInfo& info = classInfo->fields[j];
            
            switch(info.type)
            {
            case KorkApi::StartGroupFieldType:
               printGroupStart(info.pGroupname, info.pFieldDocs);
               break;
            case KorkApi::EndGroupFieldType:
               printGroupEnd();
               break;
            default:
            case KorkApi::DepricatedFieldType:
               {
                  bool isDeprecated = false;//(info.type == AbstractClassRep::DepricatedFieldType);

                  if(isDeprecated)
                  {
                     printClassMember(
                        true,
                        "<deprecated>",
                        info.pFieldname,
                        info.pFieldDocs
                        );
                  }
                  else
                  {
                     KorkApi::TypeInfo* typeInfo = info.type >= 0 ? &mVmInternal->mTypes[info.type] : NULL;

                     printClassMember(
                        false,
                        typeInfo ? typeInfo->name : "<unknown>",
                        info.pFieldname,
                        info.pFieldDocs
                        );
                  }
               }
            }
         }
      }

      if( dumpScript )
      {
         // Print out fields defined in script docs for this namespace.
         // These fields are specified by the 'field' keyword in the usage
         // string.

         // The field type and name.
         char fieldName[256];
         char fieldDoc[1024];

         // Usage string iterator.
         const char* field = vec[i]->getUsage();

         while( field )
         {
            // Find the first field keyword.
            const char* tempField = strstr( field, "@field" );
            if( !tempField )
               tempField = strstr( field, "\\field" );

            field = tempField;

            if( !field )
               break;

            // Move to the field name.
            field += 7;

            // Copy the field type and name. These should both be followed by a
            // space so only in this case will we actually store it.
            S32 spaceCount = 0;
            S32 index = 0;
            bool valid = false;
            while( field && ( *field != '\n' ) )
            {
               if( index >= 255 )
                  break;

               if( *field == ' ' )
                  spaceCount++;

               if( spaceCount == 2 )
               {
                  valid = true;
                  break;
               }

               fieldName[index++] = *field;
               field++;
            }

            if( !valid )
               continue;

            fieldName[index] = '\0';

            // Now copy from field to the next keyword.
            const char* nextKeyword = strchr( field, '@' );
            if( !nextKeyword )
               nextKeyword = strchr( field, '\\' );

            // Grab the length of the doc string.
            S32 docLen = strlen( field );
            if( nextKeyword )
               docLen = (U32)(nextKeyword - field);

            // Make sure it will fit in the buffer.
            if( docLen > 1023 )
               docLen = 1023;

            // Copy.
            strncpy( fieldDoc, field, docLen );
            fieldDoc[docLen] = '\0';
            field += docLen;

            // Print
            printf( "   /*!" );
            printf( "   %s", fieldDoc );
            printf( "    */" );
            printf( "   %s;", fieldName );
         }
      }

      // Close the class/namespace.
      printClassFooter();
   }
}

void NamespaceState::dumpFunctions( bool dumpScript, bool dumpEngine )
{
   // Get the global namespace.
   Namespace* g = find(NULL); //->mParent;

   printClassHeader(NULL, NULL,NULL, false);

   while(g) 
   {
      printNamespaceEntries(g, dumpScript, dumpEngine );
      g = g->mParent;
   }

   printClassFooter();
}
