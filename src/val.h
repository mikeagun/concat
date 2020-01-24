//Copyright (C) 2020 D. Michael Agun
//
//Licensed under the Apache License, Version 2.0 (the "License");
//you may not use this file except in compliance with the License.
//You may obtain a copy of the License at
//
//http://www.apache.org/licenses/LICENSE-2.0
//
//Unless required by applicable law or agreed to in writing, software
//distributed under the License is distributed on an "AS IS" BASIS,
//WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//See the License for the specific language governing permissions and
//limitations under the License.

#ifndef __VAL_H__
#define __VAL_H__ 1
#include <stdio.h>
#include "val_printf_fmt.h"
#include "vm_err.h"
//#include "vm_debug.h"

//This file describes val, which is the universal type in concat
//  - everything on a stack/list/quote/anywhere is a val (val_t in the c code)
//  - with optimized array types each el is logically a val, just implementation optimized
//  - use is* words in concat for typechecking, and val_is* functions in the c code
//    - see ops.c for the functions/words
//  The following core types are defined. Every val is one of these types in the code currently
//  - NULL         - nothing value
//  - BYTECODE     - compiled bytecode -- uses STRING as backend
//  - INT          - integer number (stored as valint_t)
//  - FLOAT        - floating point number (stored as double)
//  - STRING       - string of chars -- allocated off-stack
//  - IDENT        - identifier -- same as STRING, but evals to eval of dict[ident]
//  - LIST         - list of vals -- listbuf allocated off-stack
//  - CODE         - quotation -- same as LIST, but evals contents left-to-right
//  - HASH         - hashtable -- ht buckets allocated off-stack
//  - NATIVE_FUNC  - native function (c function to call which returns err_t and takes vm_t)
//  - FILE         - open file (for reading programs, files, or stdin)
//  - REF          - smart-pointer style object (refcounted ref to val_t)
//  - VM           - concat VM (thread or debuggee typically)
//  - INVALID      - used for cleared mem -- mainly so listbuf can destroy different buf elements at different times

#define val_swap(lhs,rhs) do{val_t hold__=*(lhs);*(lhs)=*(rhs);*(rhs)=hold__;}while(0)

typedef double valfloat_t;
typedef long valint_t;

//mantissa bits for valfloat_t
#define VALFLOAT_MANT_BITS 53

//TODO: consider new implementation
//  - another local branch has some punboxing code for 64bit machines with 48bit pointers
//  - current implementation has very heavy vals
//  - another option would be using bytecode vals for stack/wstack when running bytecode vm
//    - this would let high-level VM stay heavy (and easy to debug, nearly 1-to-1 with concat code)
//    - when switching to bytecode (and staying in bytecode), we could run with faster+smaller bytecode vals
//TODO: optimized array-types -- already have byte arrays via string, need int/float arrays and maybe matrices
//  - could also leverage string and just add some extra types+ops which use string buffer for backend
//  - if we are doing that maybe just make code/list also a specialization (set element size in bytes as multiplier for malloc len)
//  - optimized array ops to match (e.g. standard math operators)
//TODO: singleton list -- maybe just a standard list with refcount ignored (until clone, then both become standard lists with refcounts)
//  - I'm mainly thinking for bytecode VM and shaving instructions for typical cases
//  (e.g. while you are building up a list it is probably a singleton, so don't need all the checks)
//TODO: light dictionary -- just a list of kv pairs to in-order scan or binary search over
//  - if MCU build has dictionaries at all, it will probably be just the light dict
//  - could also be used for local variables (if most idents pre-resolved, then only need to look up the locals in the small scope on top)
//  - maybe implement in concat instead of as basic type?

//these are defined in their respecive files, but we need pointers to them here
struct val_t_;
typedef struct val_t_ val_t;
struct val_string_buffer;
struct val_list_buffer;
struct val_file_struct;
struct val_ref_struct;
struct val_hash_struct;
struct vm_state;


//string type
struct val_string { 
  struct val_string_buffer *b;
  unsigned int offset;
  unsigned int len;
};

//list type
struct val_list {
  struct val_list_buffer *b;
  unsigned int offset; //current offset into list
  unsigned int len; //current number of entries in list
};


struct vm_state; //define vm_state just so NATIVE_FUNC can take a pointer to the vm
typedef int (vm_op_handler)(struct vm_state *vm);

//native function type (stores function pointer + name)
struct val_func {
  vm_op_handler *f;
  const char *name;
  char keep; //whether to leave this func on the work stack when we exec (i.e. this val will pop itself when ready)
  unsigned char opcode; //opcode if specified (else 0, which is the invalid OP_end)
};

//This is our core data type in this version - every value/expression/list/anything is one of these
struct val_t_ {
  enum valtype {
    TYPE_NULL, //nothing value
    TYPE_BYTECODE, //compiled bytecode
    TYPE_INT, //integer number (stored as valint_t)
    TYPE_FLOAT, //floating point number (stored as double)
    TYPE_STRING, //string of chars
    TYPE_IDENT, //identifier (same as STRING, but with different evaluation rules)
    TYPE_LIST, //array/list of values stored off-stack (uses val.list)
    TYPE_CODE, //code block stored off-stack (uses val.list) (i.e. anonymous function/operator, i.e. quotation)
    TYPE_HASH, //hashtable -- only the hashtable pointer is stored on-stack, the actual buckets are allocated off stack
    TYPE_NATIVE_FUNC, //func function to call (this is used for predefined operators, or later if we want to compile sections of code)
    TYPE_FILE, //open-file info for reading programs from files or stdin
    TYPE_REF, //smart-pointer style object (refcounted ref to val_t)
    TYPE_VM, //vm (thread / debuggee)
    VAL_NTYPES, //sentinel enum value for number of types we've declared
    VAL_INVALID=VAL_NTYPES
  } type;
  union {
    valint_t i;
    valfloat_t f;
    struct val_string string;
    struct val_list list;
    struct val_hash_struct *hash;
    struct val_func func;
    struct val_file_struct *file;
    struct val_ref_struct *ref;
    struct vm_state *vm;
  } val;
  //VM_DEBUG_MEMBERS
};

//These are the handlers that must be defined for each val type
typedef int (destroy_handler)(val_t *orig);
typedef int (clone_handler)(val_t *ret, val_t *orig);
typedef int (val_fprintf_handler)(val_t *val, FILE* file, const fmt_t *fmt);
typedef int (val_sprintf_handler)(val_t *val, val_t *buf, const fmt_t *fmt);

struct type_handlers {
  destroy_handler *destroy;
  clone_handler *clone;
  val_fprintf_handler *fprintf;
  val_sprintf_handler *sprintf;
} type_handlers[VAL_NTYPES+1];

//mmemcpy/memmove functions for val arrays
val_t* valcpy(val_t *dst, val_t *src, size_t n);
val_t* valmove(val_t *dst, val_t *src, size_t n);

//clone/destroy array functions
err_t valcln(val_t *dst, val_t *src, size_t n);
err_t valdestroy(val_t *p, size_t n);

//clear value(s) to INVALID
void val_clear(val_t *p);
void valclr(val_t *p, unsigned int len);

//some basic printing implementations
err_t val_basic_clone(val_t *ret, val_t *orig);
err_t val_basic_destroy(val_t *val);
err_t val_basic_print(val_t *val);
err_t val_basic_print_code(val_t *val);

//add each set of type handlers to this func
void val_init_type_handlers();

//clone/destroy/move vals
err_t val_clone(val_t *ret, val_t *orig);
err_t val_destroy(val_t *val);
void val_move(val_t *ret, val_t *orig);

//init NULL val
void val_null_init(val_t *val);

//wrap a value so it's evaluation is the original value
err_t val_protect(val_t *val);
//wrap a value so when it is eval'd on the work stack its evaluation is the original value
//  - difference from val_protect is we also need to quote code vals
err_t val_wprotect(val_t *val);

//type checks
int val_isvalid(val_t *val); //is a valid type (eval otherwise indicates VM error)
int val_isnull(val_t *val);       //NULL val -- not used much yet, TODO: locked ref swap should be NULL
int val_islisttype(val_t *val); //code or list
int val_islist(val_t *val);       //list val
int val_iscode(val_t *val);       //quotation (code val)
int val_isnumber(val_t *val);   //numeric data type
int val_isint(val_t *val);        //integer (currently valint_t)
int val_isfloat(val_t *val);      //float (currently double)
int val_isstringtype(val_t *val); //type based on string
int val_isstring(val_t *val);     //string val
int val_isident(val_t *val);      //identifier (to look up in dictionary)
int val_isbytecode(val_t *val);   //WIP type for bytecode
int val_isfile(val_t *val);       //OS file (c FILE* currently)
int val_isref(val_t *val);        //threadsafe (locked) shared reference val
int val_isvm(val_t *val);         //concat VM (either for debugging or threading)
int val_isfunc(val_t *val);       //native function
int val_ishash(val_t *val);       //hashtable (used for dictionary and scoping)

int val_ispush(val_t *val);     //is simple data type (so eval = push and work eval = push)
int val_iscoll(val_t *val);     //is a collection datatype that standard collection ops work on (currently list,code,string)

#endif
