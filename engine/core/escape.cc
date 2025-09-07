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

static int charConv(int in)
{
   switch(in)
   {
      case 'r':
         return '\r';
      case 'n':
         return '\n';
      case 't':
         return '\t';
      default:
         return in;
   }
}

static int getHexDigit(char c)
{
   if(c >= '0' && c <= '9')
      return c - '0';
   if(c >= 'A' && c <= 'F')
      return c - 'A' + 10;
   if(c >= 'a' && c <= 'f')
      return c - 'a' + 10;
   return -1;
}

void expandEscape(char *dest, const char *src)
{
   U8 c;
   while((c = (U8) *src++) != 0)
   {
      if(c == '\"')
      {
         *dest++ = '\\';
         *dest++ = '\"';
      }
      else if(c == '\\')
      {
         *dest++ = '\\';
         *dest++ = '\\';
      }
      else if(c == '\r')
      {
         *dest++ = '\\';
         *dest++ = 'r';
      }
      else if(c == '\n')
      {
         *dest++ = '\\';
         *dest++ = 'n';
      }
      else if(c == '\t')
      {
         *dest++ = '\\';
         *dest++ = 't';
      }
      else if(c == '\'')
      {
         *dest++ = '\\';
         *dest++ = '\'';
      }
      else if((c >= 1 && c <= 7) ||
              (c >= 11 && c <= 12) ||
              (c >= 14 && c <= 15))
      {
        /*  Remap around: \b = 0x8, \t = 0x9, \n = 0xa, \r = 0xd */
        static U8 expandRemap[15] = { 0x0,
                                        0x0,
                                        0x1,
                                        0x2,
                                        0x3,
                                        0x4,
                                        0x5,
                                        0x6,
                                        0x0,
                                        0x0,
                                        0x0,
                                        0x7,
                                        0x8,
                                        0x0,
                                        0x9 };

         *dest++ = '\\';
         *dest++ = 'c';
         if(c == 15)
            *dest++ = 'r';
         else if(c == 16)
            *dest++ = 'p';
         else if(c == 17)
            *dest++ = 'o';
         else
            *dest++ = expandRemap[c] + '0';
      }
      else if(c < 32)
      {
         *dest++ = '\\';
         *dest++ = 'x';
         S32 dig1 = c >> 4;
         S32 dig2 = c & 0xf;
         if(dig1 < 10)
            dig1 += '0';
         else
            dig1 += 'A' - 10;
         if(dig2 < 10)
            dig2 += '0';
         else
            dig2 += 'A' - 10;
         *dest++ = dig1;
         *dest++ = dig2;
      }
      else
         *dest++ = c;
   }
   *dest = '\0';
}

bool collapseEscape(char *buf)
{
   S32 len = dStrlen(buf) + 1;
   for(S32 i = 0; i < len;)
   {
      if(buf[i] == '\\')
      {
         if(buf[i+1] == 'x')
         {
            S32 dig1 = getHexDigit(buf[i+2]);
            if(dig1 == -1)
               return false;

            S32 dig2 = getHexDigit(buf[i+3]);
            if(dig2 == -1)
               return false;
            buf[i] = dig1 * 16 + dig2;
            dMemmove(buf + i + 1, buf + i + 4, len - i - 3);
            len -= 3;
            i++;
         }
         else if(buf[i+1] == 'c')
         {
            /*  Remap around: \b = 0x8, \t = 0x9, \n = 0xa, \r = 0xd */
            static U8 collapseRemap[10] = { 0x1,
                                              0x2,
                                              0x3,
                                              0x4,
                                              0x5,
                                              0x6,
                                              0x7,
                                              0xb,
                                              0xc,
                                              0xe };

            if(buf[i+2] == 'r')
                buf[i] = 15;
            else if(buf[i+2] == 'p')
               buf[i] = 16;
            else if(buf[i+2] == 'o')
               buf[i] = 17;
            else
            {
                int dig1 = buf[i+2] - '0';
                if(dig1 < 0 || dig1 > 9)
                   return false;
                buf[i] = collapseRemap[dig1];
            }
            // Make sure we don't put 0x1 at the beginning of the string.
            if ((buf[i] == 0x1) && (i == 0))
            {
               buf[i] = 0x2;
               buf[i+1] = 0x1;
               dMemmove(buf + i + 2, buf + i + 3, len - i - 1);
               len -= 1;
            }
            else
            {
               dMemmove(buf + i + 1, buf + i + 3, len - i - 2);
               len -= 2;
            }
            i++;
         }
         else
         {
            buf[i] = charConv(buf[i+1]);
            dMemmove(buf + i + 1, buf + i + 2, len - i - 1);
            len--;
            i++;
         }
      }
      else
         i++;
   }
   return true;
}

