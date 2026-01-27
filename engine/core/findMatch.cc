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
#include "core/findMatch.h"
#include <array>

//--------------------------------------------------------------------------------
// NAME
//   FindMatch::FindMatch( const char *_expression, S32 maxNumMatches )
//
// DESCRIPTION
//   Class to match regular expressions (file names)
//   only works with '*','?', and 'chars'
//
// ARGUMENTS
//   _expression  -  The regular expression you intend to match (*.??abc.bmp)
//   _maxMatches  -  The maximum number of strings you wish to match.
//
// RETURNS
//
// NOTES
//
//--------------------------------------------------------------------------------

FindMatch::FindMatch( U32 _maxMatches )
{
   maxMatches = _maxMatches;
   matchList.reserve( maxMatches );
}

FindMatch::FindMatch( const char *_expression, U32 _maxMatches )
{
   setExpression( _expression );
   maxMatches = _maxMatches;
   matchList.reserve( maxMatches );
}

FindMatch::~FindMatch()
{
   matchList.clear();
}

void FindMatch::setExpression( const char *_expression )
{
   size_t len = strlen(_expression);
   expression.resize(len+1);
   memcpy(expression.data(), _expression, len+1);
   std::transform(expression.begin(), expression.begin()+len+1, expression.begin(), dToupper);
}

bool FindMatch::findMatch( const char *str, bool caseSensitive )
{
   if ( isFull() )
      return false;
   
   std::array<char, 512> nstr;
   size_t len = strlen(str);
   if (len > 511)
   {
      len = 511;
   }
   memcpy( nstr.data(), str, len );
   std::transform(nstr.begin(), nstr.begin()+len+1, nstr.begin(), dToupper);

   if ( isMatch( expression.data(), nstr.data(), caseSensitive ) )
   {
      matchList.push_back( (char*)str );
      return true;
   }
   return false;
}

inline bool IsCharMatch( char e, char s, bool caseSensitive )
{
   return ( ( e == '?' ) || ( caseSensitive && e == s ) || ( dToupper(e) == dToupper(s) ) );
}

bool FindMatch::isMatch( const char *exp, const char *str, bool caseSensitive )
{
   while ( *str && ( *exp != '*' ) )
   {
      if ( !IsCharMatch( *exp++, *str++, caseSensitive ) )
         return false;
   }

   const char* cp = NULL;
   const char* mp = NULL;

   while ( *str )
   {
      if ( *exp == '*' )
      {
         if ( !*++exp )
            return true;

         mp = exp;
         cp = str+1;
      }
      else if ( IsCharMatch( *exp, *str, caseSensitive ) )
      {
         exp++;
         str++;
      }
      else
      {
         exp = mp;
         str = cp++;
      }
   }

   while ( *exp == '*' )
      exp++;

   return !*exp;
}


bool FindMatch::isMatchMultipleExprs( const char *exps, const char *str, bool caseSensitive )
{
   char *tok = 0;
   S32 len = strlen(exps);

   KorkApi::Vector<char> e;
   e.resize(len+1);
   memcpy(e.data(), exps, len+1);

   // [tom, 12/18/2006] This no longer supports space separated expressions as
   // they don't work when the paths have spaces in.

   // search for each expression. return true soon as we see one.
   for( tok = strtok(e.data(),"\t"); tok != NULL; tok = strtok(NULL,"\t"))
   {
      if( isMatch( tok, str, caseSensitive) )
      {
         return true;
      }
   }

   return false;
}

