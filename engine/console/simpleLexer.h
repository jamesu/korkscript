#pragma once
#include "platform/types.h"
#include <string>
#include <string_view>
#include "core/tVector.h"
#include <cctype>
#include "core/stringTable.h"
#include <cinttypes>

namespace SimpleLexer
{

struct SrcPos
{
   S32 line;
   S32 col;
};

enum class TokenType : U32
{
   // Special
   END = 0,
   NONE,
   ILLEGAL,
   
   // Literals
   INTCONST,
   FLTCONST,
   STRATOM,   // "..."
   TAGATOM,   // '...'
   DOCBLOCK,  // lines starting with "///" (not "////")
   
   // Identifiers
   IDENT,
   VAR,       // $foo, %bar
   
   // Keywords (subset taken from the Flex rules)
   rwIN, rwCASEOR, rwBREAK, rwRETURN, rwELSE, rwASSERT, rwWHILE, rwDO,
   rwIF, rwFOREACHSTR, rwFOREACH, rwFOR, rwCONTINUE, rwDEFINE, rwDECLARE,
   rwDECLARESINGLETON, rwDATABLOCK, rwCASE, rwSWITCHSTR, rwSWITCH,
   rwDEFAULT, rwPACKAGE, rwNAMESPACE,
   rwTRY, rwCATCH,
   
   // Booleans become INTCONST in the original; we keep explicit ops below.
   // Operators / punctuation (single-char tokens return their char in Flex;
   // here we use dedicated enums for multi-char, and single char returned as its ASCII via opCHAR)
   opEQ, opNE, opGE, opLE, opAND, opOR, opCOLONCOLON,
   opMINUSMINUS, opPLUSPLUS, opSTREQ, opSTRNE, opSHL, opSHR,
   opPLASN, opMIASN, opMLASN, opDVASN, opMODASN, opANDASN, opXORASN, opORASN,
   opSLASN, opSRASN, opINTNAME, opINTNAMER,
   
   // Single-char tokens get returned as their ASCII using opCHAR with value in int field.
   opCHAR,
   
   // processed variants of opCHAR
   opPCHAR_PLUS,      // '+'
   opPCHAR_MINUS,     // '-'
   opPCHAR_SLASH,     // '/'
   opPCHAR_ASTERISK,  // '*'
   opPCHAR_CARET,     // '^'
   opPCHAR_PERCENT,   // '%'
   opPCHAR_AMPERSAND, // '&'
   opPCHAR_PIPE,      // '|'
   opPCHAR_LESS,      // '<'
   opPCHAR_GREATER,   // '>'
   opPCHAR_EXCL,      // '!'
   opPCHAR_TILDE,     // '~'
   
   opCONCAT, // @
   
   // Interpolated string control codes
   STRBEG,   // $"  (push)
   STREND    // "   (pop)  (includes end bit)
};

struct InterpolationState
{
   S32 depth;
   
   bool inLiteral;
   bool inBrace;
   bool needStrConcat;
   bool doInterp;
};

struct Token
{
   TokenType kind;
   SrcPos  pos;
   
   struct StringOffset
   {
      U32 offset;
      U32 len;
   };
   
   union
   {
      F64 value; // float/double/numeric value
      U64 ivalue; // integer value (e.g. bool or uint)
      StringOffset stringValue; // string position in buffer
      StringTableEntry stString;
   };
   
   Token()
   {
      pos = {};
      kind = TokenType::END;
      ivalue = 0;
   }
   
   Token(const Token& other)
   {
      kind = other.kind;
      pos = other.pos;
      ivalue = other.ivalue;
   }
   
   Token(const TokenType theKind)
   {
      pos = {};
      kind = theKind;
      ivalue = 0;
   }
   
   Token& operator=(const Token& other)
   {
      kind = other.kind;
      pos = other.pos;
      ivalue = other.ivalue;
      return *this;
   }
   
   
   inline bool isNone() const
   {
      return kind == TokenType::NONE;
   }
   
   inline bool isIllegal() const
   {
      return kind == TokenType::ILLEGAL;
   }
   
   inline bool isEnd() const
   {
      return kind == TokenType::END;
   }
   
   inline bool isChar(const Token& t, char ch)  const
   {
       return t.kind == TokenType::opCHAR && static_cast<char>(t.ivalue) == ch;
   }
   
   inline char asChar() const
   {
      return static_cast<char>(ivalue);
   }
};

class Tokenizer
{
public:
   explicit Tokenizer(_StringTable* st, std::string_view src, std::string filename, bool enableInterpolation)
   : mStringTable(st), mFilename(std::move(filename))
   {
      mPos = {};
      mBytePos = 0;
      
      U32 strSize = (U32)src.size()+1;
      if (strSize < 1)
      {
         strSize = 1;
      }
      mSource.setSize(strSize);
      memcpy(&mSource[0], &src[0], strSize);
      mSource[strSize-1] = '\0';
      mInterpState = {};
      mInterpState.doInterp = enableInterpolation;
   }
   
   inline const std::string& filename() const { return mFilename; }
   inline int line()  const { return mPos.line; }
   inline int col()   const { return mPos.col;  }
   
   const char* kindToString(TokenType k)
   {
      static const char* map[] = {
         // Special
         "END",
         "NONE",
         "ILLEGAL",
         
         // Literals
         "INTCONST",
         "FLTCONST",
         "STRATOM",   // "..."
         "TAGATOM",   // '...'
         "DOCBLOCK",  // lines starting with "///" (not "////")
         
         // Identifiers
         "IDENT",
         "VAR",       // $foo", %bar
         
         // Keywords
         "rwIN", "rwCASEOR", "rwBREAK", "rwRETURN", "rwELSE", "rwASSERT", "rwWHILE", "rwDO",
         "rwIF", "rwFOREACHSTR", "rwFOREACH", "rwFOR", "rwCONTINUE", "rwDEFINE", "rwDECLARE",
         "rwDECLARESINGLETON", "rwDATABLOCK", "rwCASE", "rwSWITCHSTR", "rwSWITCH",
         "rwDEFAULT", "rwPACKAGE", "rwNAMESPACE",
         "rwTRY", "rwCATCH",
         
         "opEQ", "opNE", "opGE", "opLE", "opAND", "opOR", "opCOLONCOLON",
         "opMINUSMINUS", "opPLUSPLUS", "opSTREQ", "opSTRNE", "opSHL", "opSHR",
         "opPLASN", "opMIASN", "opMLASN", "opDVASN", "opMODASN", "opANDASN", "opXORASN", "opORASN",
         "opSLASN", "opSRASN", "opINTNAME", "opINTNAMER",
         
         "opCHAR",
         
         "opPCHAR_PLUS",      // '+'
         "opPCHAR_MINUS",     // '-'
         "opPCHAR_SLASH",     // '/'
         "opPCHAR_ASTERISK",  // '*'
         "opPCHAR_CARET",     // '^'
         "opPCHAR_PERCENT",   // '%'
         "opPCHAR_AMPERSAND", // '&'
         "opPCHAR_PIPE",      // '|'
         "opPCHAR_LESS",      // '<'
         "opPCHAR_GREATER",   // '>'
         "opPCHAR_EXCL",      // '!'
         "opPCHAR_TILDE",     // '~'
         
         "opCONCAT",
         
         "STRBEG",
         "STREND"
      };
      
      return map[(unsigned int)k];
   }
   
   std::string stringValue(const Token& t)
   {
      if (t.kind == TokenType::TAGATOM || t.kind == TokenType::STREND || t.kind == TokenType::STRATOM || t.kind == TokenType::DOCBLOCK)
      {
         return std::string(mSource.begin() + t.stringValue.offset, mSource.begin() + t.stringValue.offset + t.stringValue.len);
      }
      else if (t.kind == TokenType::IDENT || t.kind == TokenType::VAR)
      {
        return t.stString;
      }
      else
      {
         return "";
      }
   }
   
   char* bufferAtOffset(U32 offset)
   {
      return &mSource[offset];
   }
   
   std::string toString(const Token &t)
   {
      char buf[4096];
      
      if (t.kind == TokenType::TAGATOM || t.kind == TokenType::STRATOM || t.kind == TokenType::STREND)
      {
         std::string ss(mSource.begin() + t.stringValue.offset, mSource.begin() + t.stringValue.offset + t.stringValue.len);
         snprintf(buf, sizeof(buf), "%s=\"%s\"", kindToString(t.kind), ss.c_str());
      }
      else if (t.kind == TokenType::IDENT || t.kind == TokenType::VAR)
      {
        return t.stString;
      }
      else if (t.kind == TokenType::DOCBLOCK)
      {
         std::string ss(mSource.begin() + t.stringValue.offset, mSource.begin() + t.stringValue.offset + t.stringValue.len);
         snprintf(buf, sizeof(buf), "%s=///%s", kindToString(t.kind), ss.c_str());
      }
      else if (t.kind == TokenType::INTCONST)
      {
         snprintf(buf, sizeof(buf), "%s=INT(%" PRIu64 ")", kindToString(t.kind), t.ivalue);
      }
      else if (t.kind == TokenType::FLTCONST)
      {
         snprintf(buf, sizeof(buf), "%s=FLT(%g)", kindToString(t.kind), t.value);
      }
      else if (t.kind == TokenType::opCHAR || t.kind == TokenType::opCONCAT)
      {
         snprintf(buf, sizeof(buf), "%s=CHAR(%c)", kindToString(t.kind), (unsigned char)t.ivalue);
      }
      else
      {
         snprintf(buf, sizeof(buf), "%s", kindToString(t.kind));
      }
      
      return std::string(buf);
   }
   
   Token scanInterpLiteralSegment()
   {
      char endQuote = 0;
      
      if (peek() == '"')
      {
         // blank string, so ignore
         Token te = make(TokenType::STREND);
         te.stringValue.offset = (U32)mBytePos;
         te.stringValue.len = 0;
         mSource[mBytePos] = '\0';
         // ...
         advance();
         mInterpState.depth--;
         mInterpState.inLiteral = false;
         mInterpState.inBrace = mInterpState.depth > 0;
         return te;
      }
      
      S64 bp = mBytePos;
      
      Token tok = decodeStringInPlace(&mSource[0],
                                      mSource.size(),
                                      bp,
                                      TokenType::STRATOM,
                                      '"',
                                      '{',
                                      &endQuote,
                                      true);
      
      mBytePos = (U32)bp;
      
      if (endQuote == '"')
      {
         // End of string
         mInterpState.depth--;
         mInterpState.inLiteral = false;
         mInterpState.inBrace = mInterpState.depth > 0;
         tok.kind = TokenType::STREND;
         return tok;
      }
      else if (endQuote == '{')
      {
         mInterpState.inBrace = true;
         mInterpState.inLiteral = false;
         mInterpState.needStrConcat = true;
      }
      
      return tok;
   }
   
   
   bool havePendingConcat() const { return mInterpState.needStrConcat; }
   
   Token emitPendingConcat()
   {
      mInterpState.needStrConcat = false;
      return makeConcat(0);
   }
   
   Token next()
   {
      for (;;)
      {
         // Interpolated strings - handle pending @
         if (havePendingConcat())
         {
            return emitPendingConcat();
         }
         
         Token t;
         
         if (eof())
         {
            return make(TokenType::END);
         }
         
         // Skip skipping spaces if in interp mode
         bool noSkipSpaces = mInterpState.depth > 0 &&
         mInterpState.inLiteral &&
         mInterpState.inBrace == false;
         
         if (!noSkipSpaces)
         {
            // Handle newlines / whitespace
            if (peek() == '\r')
            {
               advance();
               continue;
            }
            
            if (peek() == '\n')
            {
               advanceNewline();
               continue;
            }
            
            if (isSpace(peek()))
            {
               skipSpaces();
               continue;
            }
            
            // Line docblocks: ("///" [^/] ... newline)+
            if (matchDocblockStart())
            {
               return scanDocblock();
            }
            
            // C++-style comment //
            if (peek() == '/' && peek(1) == '/' && !(peek(2) == '/' && peek(3) != '/'))
            {
               skipLine(); continue;
            }
            
            // C-style comment /* ... */
            if (bpeek2('/', '*'))
            {
               if (!skipBlockComment())
               {
                  return illegal("unterminated block comment");
               }
               continue;
            }
         }
         
         // Handle being inside the string literal (NOT the expression)
         if (mInterpState.depth > 0 &&
             mInterpState.inLiteral &&
             mInterpState.inBrace == false)
         {
            t = scanInterpLiteralSegment();
            if (t.kind != TokenType::NONE)
            {
               return t;
            }
         }
         
         // Handle being inside the interpolation expression i.e. { ... } inside $""
         if (mInterpState.inBrace)
         {
            char c = peek();
            
            if (c == ';')
            {
               advance();
               return illegal("';' not allowed inside interpolated expression");
            }
            
            if (c == '}')
            {
               advance();
               mInterpState.inBrace = false;
               
               // Finished the { expr } for this interpolation.
               // Next token should be a literal piece or closing quote.
               mInterpState.inLiteral = true;
               mInterpState.needStrConcat = true;
               
               continue;
            }
         }
         
         // Handle start of interpolated string (i.e. $")
         if (bpeek2('$', '"') && mInterpState.doInterp)
         {
            advance(); // '$'
            advance(); // '"'
            
            mInterpState.depth++;
            mInterpState.inLiteral = true;
            mInterpState.inBrace = false;
            
            return make(TokenType::STRBEG);
         }
         
         // Quoted strings
         if (beither2('"', '\''))
         {
            return scanString(peek() == '\'' ? TokenType::TAGATOM : TokenType::STRATOM, peek());
         }
         
         // Multi-char operators (longest first)
         t = scanMultiOps();
         if (!t.isNone())
         {
            return t;
         }
         
         // Special words mapping to pseudo-characters: NL, TAB, SPC, @
         t = scanMagicAtoms();
         if (!t.isNone())
         {
            return t;
         }
         
         // Hex literal: 0xNNN
         if (peek() == '0' && beither2('x', 'X', 1) && isHex(peek(2)))
         {
            return scanHex();
         }
         
         // Float / Integer
         if (std::isdigit((unsigned char)peek()) ||
             (peek()=='.' && std::isdigit((unsigned char)peek(1))) )
         {
            Token t = scanNumber();
            if (t.kind != TokenType::NONE)
            {
               return t;
            }
         }
         
         // VAR: [$%][A-Za-z_][ :A-Za-z0-9_]*[A-Za-z0-9_]
         if (beither2('$', '%') && isLetter(peek(1)))
         {
            return scanVar();
         }
         
         // ILID: [$%][0-9]+[A-Za-z_]... (illegal)
         if (beither2('$', '%') &&
             std::isdigit((unsigned char)peek(1)))
         {
            return illegal("variables must begin with letters");
         }
         
         // Identifier / keywords: [A-Za-z_][A-Za-z0-9_]*
         if (isLetter(peek()))
         {
            return scanIdentOrKeyword();
         }
         
         // Single-char tokens
         static const std::string singles = "?[]()+-*/<>|.!:;{},&%^~=.";
         if (singles.find(peek()) != std::string::npos)
         {
            char ch = (char)peek();
            Token t = makeChar(ch);
            advance();
            // special: '.' is included above to match Flex behavior
            return t;
         }
         
         // Anything else = illegal
         return make(TokenType::ILLEGAL);
      }
   }
   
private:
   Vector<char> mSource;
   std::string mFilename;
   U32 mBytePos;
   SrcPos mPos;
   InterpolationState mInterpState;
   
public:
   _StringTable* mStringTable;
private:
   
   // --- utilities
   bool eof(S32 off = 0) const
   {
      return mBytePos + off + 1 >= mSource.size();
   }
   
   char peek(S32 off = 0) const
   {
      return eof(off) ? '\0' : mSource[mBytePos + off];
   }
   
   inline bool beither2(char a, char b, int offset=0)
   {
      const char next = peek(offset);
      return next == a || next == b;
   }
   
   inline bool bpeek2(char a, char b) const
   {
      return peek() == a && peek(1) == b;
   }
   
   inline bool bpeek3(char a, char b, char c) const
   {
      return peek() == a && peek(1) == b && peek(2) == c;
   }
   
   void advance()
   {
      if (eof()) return;
      if (mSource[mBytePos] == '\n') { ++mPos.line; mPos.col = 1; }
      else { ++mPos.col; }
      ++mBytePos;
   }
   
   void advanceNewline()  // current is '\n'
   {
      ++mBytePos; ++mPos.line; mPos.col = 1;
   }
   
   static bool isSpace(char c)
   {
      return c==' ' || c=='\t' || c=='\v' || c=='\f';
   }
   
   static bool isLetter(char c)
   {
      return std::isalpha((unsigned char)c) || c=='_';
   }
   
   static bool isIdTail(char c)
   {
      return std::isalnum((unsigned char)c) || c=='_';
   }
   
   static bool isVarMid(char c)
   {
      return std::isalnum((unsigned char)c) || c=='_' || c==':';
   }
   
   static bool isHex(char c)
   {
      return std::isdigit((unsigned char)c) ||
      (c>='a'&&c<='f') || (c>='A'&&c<='F');
   }
   
   void skipSpaces()
   {
      while (isSpace(peek()))
      {
         advance();
      }
   }
   
   void skipLine()
   {
      while (!eof() && peek()!='\n')
      {
         advance(); /* leave newline to outer loop */
      }
   }
   
   bool skipBlockComment()
   {
      // consume /*
      advance(); advance();
      char prev = 0;
      while (!eof())
      {
         char c = peek();
         if (c == '\n')
         {
            advanceNewline();
         }
         else
         {
            advance();
         }
         if (prev=='*' && c=='/')
         {
            return true;
         }
         prev = c;
      }
      return false;
   }
   
public:
   Token make(TokenType k)
   {
      Token t; t.kind = k; t.pos = mPos; return t;
   }
private:
   
   Token makeChar(char ch)
   {
      Token t;
      t.kind = TokenType::opCHAR;
      t.pos = mPos;
      t.ivalue = (U64)ch; return t;
   }
   
   Token makeConcat(char ch)
   {
      Token t;
      t.kind = TokenType::opCONCAT;
      t.pos = mPos;
      t.ivalue = (U64)ch; return t;
   }
   
   Token illegal(const char*)
   {
      return make(TokenType::ILLEGAL);
   }
   
   // --- docblocks
   bool matchDocblockStart() const
   {
      // "///" followed by a non-slash
      return (bpeek2('/', '/') && peek(2)=='/' && peek(3)!='/');
   }
   
   Token scanDocblock()
   {
      Token t = make(TokenType::DOCBLOCK);
      
      // Start at the first '/' of the initial "///"
      const S64 sliceBegin = mBytePos;
      
      // 1) Decode/mark pass: null out "///" and any '\r', keep '\n'
      //    Keep consuming consecutive "///..." lines.
      while (matchDocblockStart())
      {
         // null the three slashes
         mSource[mBytePos + 0] = '\0';
         mSource[mBytePos + 1] = '\0';
         mSource[mBytePos + 2] = '\0';
         
         // consume "///"
         advance(); advance(); advance();
         
         // copy-through until end-of-line; CRs are nulled (stripped)
         while (!eof() && peek() != '\n')
         {
            if (peek() == '\r')
            {
               mSource[mBytePos] = '\0'; // strip CR
               advance();
            }
            else
            {
               // leave character as-is; we'll compact later
               advance();
            }
         }
         
         // Preserve newline as part of the docblock and update line count
         if (!eof() && peek() == '\n')
         {
            advanceNewline();
         }
      }
      
      // Base boundary is where the run ended: current cursor (exclusive)
      const S64 baseEnd = mBytePos;
      
      // 2) Compaction pass: move non-zero bytes left over [sliceBegin, baseEnd)
      S64 write = sliceBegin;
      for (S64 read = sliceBegin; read < baseEnd; ++read)
      {
         char c = mSource[(U32)read];
         if (c != '\0')
         {
            mSource[(U32)(write++)] = c;
         }
      }
      
      // 3) Zero-fill the remainder up to the base boundary
      for (S64 i = write; i < baseEnd; ++i)
      {
         mSource[(U32)i] = '\0';
      }
      
      // Build token as a slice into the (now compacted) buffer
      t.stringValue.offset = (U32)sliceBegin;
      t.stringValue.len = (U32)(write - sliceBegin);
      return t;
   }
   
   // --- magic atoms: NL, TAB, SPC, @
   Token scanMagicAtoms()
   {
      if (matchWord("NL"))  return makeConcat('\n');
      if (matchWord("TAB")) return makeConcat('\t');
      if (matchWord("SPC")) return makeConcat(' ');
      if (peek()=='@')      { advance(); return makeConcat(0); }
      return make(TokenType::NONE);
   }
   
   bool matchWord(const char* w)
   {
      S64 n = std::char_traits<char>::length(w);
      for (S64 k=0;k<n;++k)
      {
         if (peek(k)!=w[k])
         {
            return false;
         }
      }
      
      // next char must not be an id tail (to keep "NLOOPS" from matching "NL")
      if (isIdTail(peek(n)))
      {
         return false;
      }
      
      // consume
      for (S64 k=0;k<n;++k)
      {
         advance();
      }
      
      return true;
   }
   
   inline Token emit(TokenType k, int n)
   {
      Token t = make(k);
      
      for (int i=0;i<n;i++)
      {
         advance();
      }
      
      return t;
   }
   
   // --- multi-char operators
   Token scanMultiOps()
   {
      // 3-char first
      if (bpeek3('-', '-', '>')) return emit(TokenType::opINTNAMER, 3);
      if (bpeek3('>', '>', '=')) return emit(TokenType::opSRASN, 3);
      if (bpeek3('<', '<', '=')) return emit(TokenType::opSLASN, 3);
      
      // 2-char
      if (bpeek2('=', '=')) return emit(TokenType::opEQ, 2);
      if (bpeek2('!', '=')) return emit(TokenType::opNE, 2);
      if (bpeek2('>', '=')) return emit(TokenType::opGE, 2);
      if (bpeek2('<', '=')) return emit(TokenType::opLE, 2);
      if (bpeek2('&', '&')) return emit(TokenType::opAND, 2);
      if (bpeek2('|', '|')) return emit(TokenType::opOR, 2);
      if (bpeek2(':', ':')) return emit(TokenType::opCOLONCOLON, 2);
      if (bpeek2('-', '-')) return emit(TokenType::opMINUSMINUS, 2);
      if (bpeek2('+', '+')) return emit(TokenType::opPLUSPLUS, 2);
      if (bpeek2('$', '=')) return emit(TokenType::opSTREQ, 2);
      if (bpeek3('!', '$', '=')) return emit(TokenType::opSTRNE, 3); // actually 3-char, but fine here
      if (bpeek2('<', '<')) return emit(TokenType::opSHL, 2);
      if (bpeek2('>', '>')) return emit(TokenType::opSHR, 2);
      if (bpeek2('+', '=')) return emit(TokenType::opPLASN, 2);
      if (bpeek2('-', '=')) return emit(TokenType::opMIASN, 2);
      if (bpeek2('*', '=')) return emit(TokenType::opMLASN, 2);
      if (bpeek2('/', '=')) return emit(TokenType::opDVASN, 2);
      if (bpeek2('%', '=')) return emit(TokenType::opMODASN, 2);
      if (bpeek2('&', '=')) return emit(TokenType::opANDASN, 2);
      if (bpeek2('^', '=')) return emit(TokenType::opXORASN, 2);
      if (bpeek2('|', '=')) return emit(TokenType::opORASN, 2);
      if (bpeek2('-', '>')) return emit(TokenType::opINTNAME, 2);
      
      return make(TokenType::NONE);
   }
   
   // --- numbers
   Token scanNumber()
   {
      SrcPos p = mPos;
      S64 start = mBytePos;
      bool sawDot = false;
      bool sawExp = false;
      
      auto isDigit = [](char c) {
         return std::isdigit((unsigned char)c)!=0;
      };
      
      if (peek()=='.')
      {
         sawDot = true;
         advance();
         if (!isDigit(peek()))
         {
            mBytePos=start;
            mPos=p;
            return make(TokenType::NONE);
         }
      }
      
      while (isDigit(peek()))
      {
         advance();
      }
      
      if (peek()=='.')
      {
         sawDot = true;
         advance();
         while (isDigit(peek()))
         {
            advance();
         }
      }
      
      if (beither2('e', 'E'))
      {
         sawExp = true; advance();
         if (beither2('+', '-'))
         {
            advance();
         }
         
         if (!isDigit(peek())) // backtrack on bad exponent
         {
            mBytePos = start;
            mPos = p;
            return make(TokenType::NONE);
         }
         while (isDigit(peek()))
         {
            advance();
         }
      }
      
      std::string s = std::string(mSource.begin() + start,
                                  mSource.begin() + start + (mBytePos - start));
      
      if (sawDot || sawExp)
      {
         // float
         Token t = make(TokenType::FLTCONST);
         t.value = std::stod(s);
         
#ifndef PRECISE_NUMBERS
         F32 f = static_cast<F32>(t.value);
         t.value = f;
#endif
         return t;
      }
      else
      {
         // integer
         Token t = make(TokenType::INTCONST);
         t.ivalue = std::stoll(s);
         return t;
      }
   }
   
   Token scanHex()
   {
      SrcPos p = mPos;
      S64 start = mBytePos;
      advance(); advance(); // 0x
      while (isHex(peek()))
      {
         advance();
      }
      
      std::string s = std::string(mSource.begin() + start,
                                  mSource.begin() + start + (mBytePos - start));
      
      // std::stoi handles 0x prefix with base 16 if specified, but since we consumed, parse manually
      int val = std::stoi(s, nullptr, 16);
      Token t = make(TokenType::INTCONST);
      t.ivalue = val;
      return t;
   }
   
   // --- strings
   static int hexVal(char c)
   {
      if (c>='0'&&c<='9') return c-'0';
      if (c>='a'&&c<='f') return 10 + (c-'a');
      if (c>='A'&&c<='F') return 10 + (c-'A');
      return -1;
   }
   static char convEscape(char c)
   {
      switch (c)
      {
         case 'n': return '\n';
         case 'r': return '\r';
         case 't': return '\t';
         case '\\':return '\\';
         case '"': return '"';
         case '\'':return '\'';
         default:  return c;
      }
   }
   
   Token decodeStringInPlace(char* buf, S64 bufEnd, S64& idx, TokenType tokenType, const char quote, const char altQuote, char* outQuote, bool dontSkipStart=false)
   {
      const S64 open = idx;
      if (open >= bufEnd || bufEnd <= 0 || idx < 0)
      {
         return illegal("end of input");
      }
      
      bool firstOut = true;
      
      // Track boundaries:
      S64 base_boundary = -1;      // where the closing quote is (exclusive)
      S64 effective_boundary = -1; // overridden by \c0 (first occurrence)
      
      // advance past the opening quote
      if (!dontSkipStart)
      {
         ++idx;
      }
      
      Token t = make(tokenType);
      
      // 1) decode pass (in-place)
      for (;;)
      {
         if (idx >= bufEnd)
         {
            return illegal("string not closed");
         }
         
         char c = buf[idx];
         
         if (c == '\n')
         {
            // raw newline not allowed
            return illegal("newline not allowed in string");
         }
         if (c == '\r')
         {
            // strip CR
            buf[idx++] = '\0';
            continue;
         }
         if (c == quote || c == altQuote)
         {
            // closing quote
            base_boundary = idx; // exclusive
            buf[idx++] = '\0';   // zap the closing quote
            if (outQuote)
            {
               *outQuote = c;
            }
            break;
         }
         
         if (c == '\\')
         {
            const U64 slashIDX = idx;
            const U64 codeIDX = slashIDX + 1;
            if (codeIDX >= bufEnd)
            {
               return illegal("invalid string");
            }
            
            char e = buf[codeIDX];
            
            // \xHH
            if (e == 'x')
            {
               if (codeIDX+2 >= bufEnd) return illegal("invalid hex token");
               int h1 = hexVal(buf[codeIDX+1]);
               int h2 = hexVal(buf[codeIDX+2]);
               if (h1 < 0 || h2 < 0) return illegal("invalid hex token");
               unsigned char byte = static_cast<unsigned char>(h1*16 + h2);
               
               // guard against first byte == 0x01
               if (firstOut && byte == 0x01)
               {
                  buf[slashIDX] = char(0x02);
                  buf[slashIDX+1] = char(0x01);
                  // finish end i.e. \xHH -> ##HH
                  buf[slashIDX+2] = '\0';
                  buf[slashIDX+3] = '\0';
               }
               else
               {
                  buf[slashIDX] = char(byte);
                  // finish end i.e. \xHH -> #xHH
                  buf[slashIDX+1] = '\0';
                  buf[slashIDX+2] = '\0';
                  buf[slashIDX+3] = '\0';
               }
               
               idx += 4; // i.e. next after \xHH_
               firstOut = false;
               continue;
            }
            
            // \c* family
            if (e == 'c')
            {
               if (codeIDX+1 >= bufEnd)
               {
                  return illegal("incomplete");
               }
               
               char k = buf[codeIDX+1];
               
               unsigned char byte = 0;
               if      (k == 'r') byte = 15;
               else if (k == 'p') byte = 16;
               else if (k == 'o') byte = 17;
               else
               {  // note: '0' handled above
                  static const unsigned char map[10] =
                  { 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0xB, 0xC, 0xE };
                  int dig = (int)k - '0';
                  if (dig < 0 || dig > 9)
                  {
                     return illegal("invalid escape");
                  }
                  byte = map[dig];
               }
               
               if (firstOut && byte == 0x01)
               {
                  buf[slashIDX] = char(0x02);
                  buf[slashIDX+1] = char(0x01);
                  buf[slashIDX+2] = '\0';
               }
               else
               {
                  buf[slashIDX] = char(byte);
                  buf[slashIDX+1] = '\0';
                  buf[slashIDX+2] = '\0';
               }
               
               idx += 3;  // i.e. next after \cX_
               firstOut = false;
               continue;
            }
            
            // standard short escapes (or fallback)
            {
               char m = convEscape(e);
               if (firstOut && static_cast<unsigned char>(m) == 0x01)
               {
                  buf[slashIDX] = char(0x02);
                  buf[slashIDX+1] = char(0x01);
               }
               else
               {
                  buf[slashIDX] = m;
                  buf[slashIDX+1] = '\0';
               }
               
               idx += 2; // i.e. after \X_
               firstOut = false;
               continue;
            }
         }
         
         // ordinary byte
         firstOut = false;
         idx++; // next char
      }
      
      // choose effective boundary (\c0 overrides)
      S64 boundary = (effective_boundary >= 0)
      ? effective_boundary
      : base_boundary;
      
      // 2) compaction pass over [start, boundary):
      const S64 start = !dontSkipStart ? (open + 1) : open;
      S64 read = start;
      S64 write = start;
      while (read < boundary)
      {
         char b = buf[read++];
         if (b != '\0')
            buf[write++] = b;
      }
      
      // 3) zero any remaining bytes up to the BASE boundary (not just effective)
      for (S64 i = write; i < base_boundary; ++i)
      {
         buf[i] = '\0';
      }
      
      // Set token value
      t.stringValue.offset = (U32)start;
      t.stringValue.len = (U32)(write - start);
      return t;
   }
   
   Token scanString(TokenType type, char quote)
   {
      S64 bp = mBytePos;
      Token t = decodeStringInPlace(&mSource[0],
                                 mSource.size(),
                                 bp,
                                 type,
                                 quote,
                                 quote,
                                 NULL);
      mBytePos = (U32)bp;
      return t;
   }
   
   // --- identifiers / keywords
   Token scanIdentOrKeyword()
   {
      Token t = make(TokenType::NONE);
      t.stringValue.offset = (U32)mBytePos;
      const char* startStr = &mSource[mBytePos];
      bool addedStar = false;
      SrcPos starPos;
      
      advance(); // first letter/underscore already checked
      while (isIdTail(peek()))
      {
         advance();
      }
      
      // Handle $
      if (peek() == '$')
      {
         starPos = mPos;
         advance();
         addedStar = true;
      }
      
      struct CheckTable
      {
         const char* val;
         TokenType kind;
      };
      
      static CheckTable chkTable[] = {
         {"in",         TokenType::rwIN},
         {"or",         TokenType::rwCASEOR},
         {"break",      TokenType::rwBREAK},
         {"return",     TokenType::rwRETURN},
         {"else",       TokenType::rwELSE},
         {"assert",     TokenType::rwASSERT},
         {"while",      TokenType::rwWHILE},
         {"do",         TokenType::rwDO},
         {"if",         TokenType::rwIF},
         {"try",         TokenType::rwTRY},
         {"catch",         TokenType::rwCATCH},
         {"foreach$",   TokenType::rwFOREACHSTR},
         {"foreach",    TokenType::rwFOREACH},
         {"for",        TokenType::rwFOR},
         {"continue",   TokenType::rwCONTINUE},
         {"function",   TokenType::rwDEFINE},
         {"new",        TokenType::rwDECLARE},
         {"singleton",  TokenType::rwDECLARESINGLETON},
         {"datablock",  TokenType::rwDATABLOCK},
         {"case",       TokenType::rwCASE},
         {"switch$",    TokenType::rwSWITCHSTR},
         {"switch",     TokenType::rwSWITCH},
         {"default",    TokenType::rwDEFAULT},
         {"package",    TokenType::rwPACKAGE},
         {"namespace",  TokenType::rwNAMESPACE},
         {"true",       TokenType::INTCONST},
         {"false",      TokenType::INTCONST}
      };
      
      for (U32 i=0; i<sizeof(chkTable) / sizeof(chkTable[0]); i++)
      {
         //printf("CHK %s %c%c%c%c%c%c%c=%i\n", chkTable[i].val, startStr[0], startStr[1], startStr[2], startStr[3], startStr[4], startStr[5], startStr[6], strlen(chkTable[i].val));
         if (strncmp(startStr, chkTable[i].val, std::max<S64>(mBytePos - t.stringValue.offset, strlen(chkTable[i].val))) == 0)
         {
            t.kind = chkTable[i].kind;
            if (t.kind == TokenType::INTCONST)
            {
               t.ivalue = (startStr[0] == 't') ? 1 : 0;
            }
            else
            {
               t.ivalue = 0;
            }
            break;
         }
      }
      
      if (t.kind == TokenType::NONE)
      {
         if (addedStar)
         {
            // rewind
            mBytePos--;
            mPos = starPos;
         }
         
         // Default to IDENT
         t.kind = TokenType::IDENT;
         t.stString = mStringTable->insertn(&mSource[t.stringValue.offset], (U32)(mBytePos - t.stringValue.offset));
      }
      
      return t;
   }
   
   // --- VAR
   // Assumes first two tokens are valid
   Token scanVar()
   {
      Token t = make(TokenType::VAR);
      t.stringValue.offset = (U32)mBytePos;
      
      // [$%]
      advance();
      // LETTER
      advance();
      
      // After the first LETTER, we already have a valid end.
      S64 lastGood = mBytePos;
      SrcPos lastGoodSrc = mPos;
      
      // Consume while in VARMID, but remember last position where we ended on IDTAIL.
      while (isVarMid(peek()))
      {
         char c = peek();
         advance();
         if (isIdTail(c))
         {
            lastGood = mBytePos; // we can legally end here
            lastGoodSrc = mPos;
         }
      }
      
      // Rewind to the last valid end to drop any trailing ':'s
      if (mBytePos > lastGood)
      {
         size_t rewind = mBytePos - lastGood;
         mBytePos = lastGood;
         mPos = lastGoodSrc;
      }
      
      // TODO: TYPEID match
      
      // Build token as a view into the source
      t.stString = mStringTable->insertn(&mSource[t.stringValue.offset], (U32)(mBytePos - t.stringValue.offset));
      return t;
   }
};

}
