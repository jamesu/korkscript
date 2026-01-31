#pragma once

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

namespace Compiler
{
   /// The opcodes for the TorqueScript VM.
   enum CompiledInstructions
   {
      OP_FUNC_DECL,
      OP_CREATE_OBJECT,
      OP_ADD_OBJECT,
      OP_END_OBJECT,
      OP_FINISH_OBJECT,

      OP_JMPIFFNOT,
      OP_JMPIFNOT,
      OP_JMPIFF,
      OP_JMPIF,
      OP_JMPIFNOT_NP,
      OP_JMPIF_NP,    // 10
      OP_JMP,
      OP_RETURN,
      OP_RETURN_VOID,
      OP_RETURN_FLT,
      OP_RETURN_UINT,

      OP_CMPEQ,
      OP_CMPGR,
      OP_CMPGE,
      OP_CMPLT,
      OP_CMPLE,
      OP_CMPNE,
      OP_XOR,         // 20
      OP_MOD,
      OP_BITAND,
      OP_BITOR,
      OP_NOT,
      OP_NOTF,
      OP_ONESCOMPLEMENT,

      OP_SHR,
      OP_SHL,
      OP_AND,
      OP_OR,          // 30

      OP_ADD,
      OP_SUB,
      OP_MUL,
      OP_DIV,
      OP_NEG,

      OP_SETCURVAR,
      OP_SETCURVAR_CREATE,
      OP_SETCURVAR_ARRAY,
      OP_SETCURVAR_ARRAY_CREATE,

      OP_LOADVAR_UINT,// 40
      OP_LOADVAR_FLT,
      OP_LOADVAR_STR,
      OP_LOADVAR_VAR,

      OP_SAVEVAR_UINT,
      OP_SAVEVAR_FLT,
      OP_SAVEVAR_STR,
      OP_SAVEVAR_VAR,

      OP_SETCUROBJECT,
      OP_SETCUROBJECT_NEW,
      OP_SETCUROBJECT_INTERNAL,

      OP_SETCURFIELD,
      OP_SETCURFIELD_ARRAY, // 50
      OP_SETCURFIELD_TYPE,  // NOTE: this now uses the local type table

      OP_LOADFIELD_UINT,
      OP_LOADFIELD_FLT,
      OP_LOADFIELD_STR,

      OP_SAVEFIELD_UINT,
      OP_SAVEFIELD_FLT,
      OP_SAVEFIELD_STR,

      OP_STR_TO_UINT,
      OP_STR_TO_FLT,
      OP_STR_TO_NONE,  // 60
      OP_FLT_TO_UINT,
      OP_FLT_TO_STR,
      OP_FLT_TO_NONE,
      OP_UINT_TO_FLT,
      OP_UINT_TO_STR,
      OP_UINT_TO_NONE,
      OP_COPYVAR_TO_NONE,

      OP_LOADIMMED_UINT,
      OP_LOADIMMED_FLT,
      OP_TAG_TO_STR,
      OP_LOADIMMED_STR, // 70
      OP_DOCBLOCK_STR,  // 76
      OP_LOADIMMED_IDENT,

      OP_CALLFUNC_RESOLVE,
      OP_CALLFUNC,

      OP_ADVANCE_STR,
      OP_ADVANCE_STR_APPENDCHAR,
      OP_ADVANCE_STR_COMMA,
      OP_ADVANCE_STR_NUL,
      OP_REWIND_STR,
      OP_TERMINATE_REWIND_STR,  // 80
      OP_COMPARE_STR,

      OP_PUSH,          // String
      OP_PUSH_UINT,     // Integer
      OP_PUSH_FLT,      // Float
      OP_PUSH_VAR,      // Variable
      OP_PUSH_FRAME,    // Frame

      OP_ASSERT,
      OP_BREAK,
      
      OP_ITER_BEGIN,       ///< Prepare foreach iterator.
      OP_ITER_BEGIN_STR,   ///< Prepare foreach$ iterator.
      OP_ITER,             ///< Enter foreach loop.
      OP_ITER_END,         ///< End foreach loop.

      
      // NEW OPCODES
      
      // Exceptions
      OP_PUSH_TRY,
      OP_PUSH_TRY_STACK,
      OP_POP_TRY,
      OP_THROW,
      OP_DUP_UINT,

      // Typed vars
      OP_PUSH_TYPED,        // basically the same as OP_PUSH but checks typed vars
      
      // var / field -> typed
      OP_LOADVAR_TYPED,     // same as OP_LOADVAR_STR except it checks typed vars
      OP_LOADVAR_TYPED_REF, // same as OP_LOADVAR_TYPED except it references the var
      OP_LOADFIELD_TYPED,   // loads object field into typed value
      
      // typed -> var
      OP_SAVEVAR_TYPED,      // save value on stack to variable
      OP_SAVEFIELD_TYPED,    // save value on stack to object field

      // native -> typed
      OP_STR_TO_TYPED,      // perform conversion from value on stack to type
      OP_FLT_TO_TYPED,      // perform conversion from value on float stack to type
      OP_UINT_TO_TYPED,     // perform conversion from value on uint stack to type

      // typed -> native
      OP_TYPED_TO_STR,      // reverse of those...
      OP_TYPED_TO_FLT,      // 
      OP_TYPED_TO_UINT,     // 
      OP_TYPED_TO_NONE,     // 
      
      // ops
      OP_TYPED_OP,          // perform op on typed value (relative to OP_CMPEQ)
      OP_TYPED_OP_REVERSE,  // reverse of OP_TYPED_OP
      OP_TYPED_UNARY_OP,    // handles -%val and such

      // extra useful
      OP_SETCURFIELD_NONE,  // needed for field unset
      OP_SETVAR_FROM_COPY,  // reset copy var

      // Extra missing field related ops
      OP_LOADFIELD_VAR,     // loads object field into current var 
      OP_SAVEFIELD_VAR,     // save current variable to object field
      OP_SETCURVAR_TYPE,    // set type of current var

      // Dynamic type id set
      OP_SET_DYNAMIC_TYPE_FROM_VAR,   // sets dynamic type to type of var
      OP_SET_DYNAMIC_TYPE_FROM_FIELD, // sets dynamic typeid to field type
      OP_SET_DYNAMIC_TYPE_FROM_ID,    // sets dynamic typeid to specific type (for casts)
      OP_SET_DYNAMIC_TYPE_TO_NULL,

      // Tuple type assignments
      // (these basically act like function calls)
      OP_SAVEVAR_MULTIPLE,         // i.e. %var = 1,2,3 (NOT ALLOWED YET)
      OP_SAVEVAR_MULTIPLE_TYPED,   // i.e. %var : type = 1,2,3
      OP_SAVEFIELD_MULTIPLE,       // i.e. obj.field = 1,2,3 OR field = 1,2,3; inside decl

      OP_INVALID   // 90
   };
}

