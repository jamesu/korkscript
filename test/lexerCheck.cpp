#include "platform/platform.h"
#include "console/console.h"
#include "console/simpleLexer.h"
#include "core/fileStream.h"
#include <stdio.h>

/*

Code to verify lexer generates identical output to original lexer.

*/

bool gPrintTokens = false;

void MyLogger(ConsoleLogEntry::Level level, const char *consoleLine)
{
	printf("%s\n", consoleLine);
}

#include "console/ast.h"

template< typename T >
struct Token
{
   T value;
   S32 lineNumber;
};

// Can't have ctors in structs used in unions, so we have this.
template< typename T >
inline Token< T > MakeToken( T value, U32 lineNumber )
{
   Token< T > result;
   result.value = value;
   result.lineNumber = lineNumber;
   return result;
}

const char* getCMDSVal();
U32 getCMDIVal();
F32 getCMDFVal();

void printHex(const char *str) {
    while (*str) {
        printf("%02x ", (unsigned char)*str);
        str++;
    }
    printf("\n");
}

bool LEXMatches(int lexRet, SimpleLexer::Tokenizer& lex, SimpleLexer::Token& tok)
{
   //printf("fval=%f tok=%f\n", getCMDFVal(), tok.value);
   /*if (lexRet == 295) {
      printf("sval=%s\n", getCMDSVal());
      printHex(getCMDSVal());
      printf("TOK\n");
      printHex(lex.stringValue(tok).c_str());
   }*/
   switch (lexRet) {
      case 258: return tok.kind == SimpleLexer::TokenType::rwDEFINE && true;
         //case 259: return tok.kind == SimpleLexer::TokenType::rwENDDEF && true;
      case 260: return tok.kind == SimpleLexer::TokenType::rwDECLARE && true;
      case 261: return tok.kind == SimpleLexer::TokenType::rwDECLARESINGLETON && true;
      case 262: return tok.kind == SimpleLexer::TokenType::rwBREAK && true;
      case 263: return tok.kind == SimpleLexer::TokenType::rwELSE && true;
      case 264: return tok.kind == SimpleLexer::TokenType::rwCONTINUE && true;
         //case 265: return tok.kind == SimpleLexer::TokenType::rwGLOBAL && true;
      case 266: return tok.kind == SimpleLexer::TokenType::rwIF && true;
         //case 267: return tok.kind == SimpleLexer::TokenType::rwNIL && true;
      case 268: return tok.kind == SimpleLexer::TokenType::rwRETURN && true;
      case 269: return tok.kind == SimpleLexer::TokenType::rwWHILE && true;
      case 270: return tok.kind == SimpleLexer::TokenType::rwDO && true;
         //case 271: return tok.kind == SimpleLexer::TokenType::rwENDIF && true;
         //case 272: return tok.kind == SimpleLexer::TokenType::rwENDWHILE && true;
         //case 273: return tok.kind == SimpleLexer::TokenType::rwENDFOR && true;
      case 274: return tok.kind == SimpleLexer::TokenType::rwDEFAULT && true;
      case 275: return tok.kind == SimpleLexer::TokenType::rwFOR && true;
      case 276: return tok.kind == SimpleLexer::TokenType::rwFOREACH && true;
      case 277: return tok.kind == SimpleLexer::TokenType::rwFOREACHSTR && true;
      case 278: return tok.kind == SimpleLexer::TokenType::rwIN && true;
      case 279: return tok.kind == SimpleLexer::TokenType::rwDATABLOCK && true;
      case 280: return tok.kind == SimpleLexer::TokenType::rwSWITCH && true;
      case 281: return tok.kind == SimpleLexer::TokenType::rwCASE && true;
      case 282: return tok.kind == SimpleLexer::TokenType::rwSWITCHSTR && true;
      case 283: return tok.kind == SimpleLexer::TokenType::rwCASEOR && true;
      case 284: return tok.kind == SimpleLexer::TokenType::rwPACKAGE && true;
      case 285: return tok.kind == SimpleLexer::TokenType::rwNAMESPACE && true;
         //case 286: return tok.kind == SimpleLexer::TokenType::rwCLASS && true;
      case 287: return tok.kind == SimpleLexer::TokenType::rwASSERT && true;
      case 288: return tok.kind == SimpleLexer::TokenType::ILLEGAL && true;
         //case 289: return tok.kind == SimpleLexer::TokenType::CHRCONST && true;
      case 290: return tok.kind == SimpleLexer::TokenType::INTCONST && tok.ivalue == getCMDIVal();
         //case 291: return tok.kind == SimpleLexer::TokenType::TTAG && true;
      case 292: return tok.kind == SimpleLexer::TokenType::VAR && strcasecmp(lex.stringValue(tok).c_str(), getCMDSVal()) == 0;
      case 293: return tok.kind == SimpleLexer::TokenType::IDENT && strcasecmp(lex.stringValue(tok).c_str(), getCMDSVal()) == 0;
         //case 294: return tok.kind == SimpleLexer::TokenType::TYPEIDENT && strcmp(lex.stringValue(tok).c_str(), getCMDSVal()) == 0;
      case 295: return tok.kind == SimpleLexer::TokenType::DOCBLOCK && strcmp(lex.stringValue(tok).c_str(), getCMDSVal()) == 0;
      case 296:
         return tok.kind == SimpleLexer::TokenType::STRATOM && strcmp(lex.stringValue(tok).c_str(), getCMDSVal()) == 0;
      case 297: return tok.kind == SimpleLexer::TokenType::TAGATOM && strcmp(lex.stringValue(tok).c_str(), getCMDSVal()) == 0;
      case 298: 
         return tok.kind == SimpleLexer::TokenType::FLTCONST && (F32)tok.value == getCMDFVal();
      case 299: return tok.kind == SimpleLexer::TokenType::opINTNAME && true;
      case 300: return tok.kind == SimpleLexer::TokenType::opINTNAMER && true;
      case 301: return tok.kind == SimpleLexer::TokenType::opMINUSMINUS && true;
      case 302: return tok.kind == SimpleLexer::TokenType::opPLUSPLUS && true;
         //case 303: return tok.kind == SimpleLexer::TokenType::STMT_SEP && true;
      case 304: return tok.kind == SimpleLexer::TokenType::opSHL && true;
      case 305: return tok.kind == SimpleLexer::TokenType::opSHR && true;
      case 306: return tok.kind == SimpleLexer::TokenType::opPLASN && true;
      case 307: return tok.kind == SimpleLexer::TokenType::opMIASN && true;
      case 308: return tok.kind == SimpleLexer::TokenType::opMLASN && true;
      case 309: return tok.kind == SimpleLexer::TokenType::opDVASN && true;
      case 310: return tok.kind == SimpleLexer::TokenType::opMODASN && true;
      case 311: return tok.kind == SimpleLexer::TokenType::opANDASN && true;
      case 312: return tok.kind == SimpleLexer::TokenType::opXORASN && true;
      case 313: return tok.kind == SimpleLexer::TokenType::opORASN && true;
      case 314: return tok.kind == SimpleLexer::TokenType::opSLASN && true;
      case 315: return tok.kind == SimpleLexer::TokenType::opSRASN && true;
         //case 316: return tok.kind == SimpleLexer::TokenType::opCAT && true;
      case 317: return tok.kind == SimpleLexer::TokenType::opEQ && true;
      case 318: return tok.kind == SimpleLexer::TokenType::opNE && true;
      case 319: return tok.kind == SimpleLexer::TokenType::opGE && true;
      case 320: return tok.kind == SimpleLexer::TokenType::opLE && true;
      case 321: return tok.kind == SimpleLexer::TokenType::opAND && true;
      case 322: return tok.kind == SimpleLexer::TokenType::opOR && true;
      case 323: return tok.kind == SimpleLexer::TokenType::opSTREQ && true;
      case 324: return tok.kind == SimpleLexer::TokenType::opCOLONCOLON && true;
         //case 325: return tok.kind == SimpleLexer::TokenType::opNTASN && true;
         //case 326: return tok.kind == SimpleLexer::TokenType::opNDASN && true;
         //case 327: return tok.kind == SimpleLexer::TokenType::opMDASN && true;
      case 328: return tok.kind == SimpleLexer::TokenType::opSTRNE && true;
         //case 329: return tok.kind == SimpleLexer::TokenType::UNARY && true;
         
      default:
         // Check if we have @ SPC etc
         if ((tok.kind == SimpleLexer::TokenType::opCHAR) && lexRet == tok.ivalue)
         {
            return true;
         }
         else if (lexRet == '@' && tok.kind == SimpleLexer::TokenType::opCONCAT)
         {
            //printf("@ IVALUE=%i TOK=%i\n", getCMDIVal(), tok.ivalue);
            return tok.ivalue == getCMDIVal();
         }
         
         if (lexRet == 0 && tok.kind == SimpleLexer::TokenType::END)
         {
            return true;
         }
         
         return false;
   }
}

#include "console/cmdgram.h"

void CMDSetScanBuffer(const char *sb, const char *fn);
void CMDrestart(FILE* in);
int CMDlex(void);

const char* getCMDSVal()
{
   return CMDlval.s.value;
}

U32 getCMDIVal()
{
   return CMDlval.i.value;
}

F32 getCMDFVal()
{
   return CMDlval.f.value;
}

bool ensureLexMatches(const char* buf, const char* filename)
{
   CMDSetScanBuffer(buf, filename);
   CMDrestart(NULL);
   std::string theBuf(buf);
   SimpleLexer::Tokenizer lex(StringTable, theBuf, filename);

   int lexI = 0;
   SimpleLexer::Token t;
   int matchCount = 0;

   do
   {
      lexI = CMDlex();
      t = lex.next();
      if (gPrintTokens)
      {
         Con::printf("%s", lex.toString(t).c_str());
      }

      if (!LEXMatches(lexI, lex, t))
      {
      	Con::printf("%s: LEX Doesn't match! (lexI=%i)\n", filename, lexI);
      	return false;
      }
      matchCount++;
      
   } while (lexI != 0);
   
   Con::printf("%s: Lexer matches (%i tokens)!\n", filename, matchCount);
   return true;
}


int procMain(int argc, char **argv)
{
   if (argc < 2)
   {
      Con::printf("Not enough args");
      return 1;
   }
   
   for (int i=2; i<argc; i++)
   {
      if (strcmp(argv[i], "-v") == 0)
      {
         gPrintTokens = true;
      }
   }
   
	FileStream fs;
	if (!fs.open(argv[1], FileStream::Read))
	{
		Con::printf("Error loading file %s\n", argv[1]);
		return 1;
	}

	char* data = new char[fs.getStreamSize()+1];
	fs.read(fs.getStreamSize(), data);
	data[fs.getStreamSize()] = '\0';

	int ret = ensureLexMatches(data, argv[1]) ? 0 : 1;
	delete[] data;
	return ret;
}

int main(int argc, char **argv)
{
	Con::init();
	Con::addConsumer(MyLogger);

	int ret = procMain(argc, argv);

	Con::shutdown();

	return ret;
}
