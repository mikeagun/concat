//Copyright (C) 2024 D. Michael Agun
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

#ifndef __VM_ERR_H__
#define __VM_ERR_H__ 1

//This file is our exception handling library
// - exceptions are all negative integers counting down from -1 (TODO: to LAST_ERROR)
// - FIXME: err_t is our error code type that should be returned by any functions that return success (0) or an exception -- validate universal use
// - other functions (e.g. printf family) return a non-negative integer or an exception
// - the error codes ERR_THROW and ERR_USER_THROW indicate that the exception is the top val on the stack
//   - other error codes get converted to THROW by the vm before popping the continuation stack
//   - this means although the error codes are always integers, the exception can also be any val (by pushing it to the stack)
//
//throwing exceptions
//  - exceptions are thrown by returning _throw(ERRCODE) or _fatal(ERRCODE)
//    - these macros let us print the line an exception came from (and send sigint to trap out to gdb) when debugging
//    - when not debugging, _throw just returns ERRCODE, and _fatal prints where the exception came from (since we want to minimize the number of these)
//    - TODO: should _fatal report line number in release mode???
//  - argcheck_r() should be used by all internal library functions to validate arguments
//    - this is for arguments that are normally assumed to be correct
//    - when not debugging, argcheck is a no-op
//    - for debugging vm internals, on failed argcheck prints an error message, and then _throws(ERR_BADARGS)
//    - use argcheck() for internal functions that return void
//  - debug_assert() and debug_assert_r() can be used to insert checks for debugging (and not otherwise)
//    - both print an error message with where the assert came from
//      - debug_assert_r() returns _fatal(ERR_ASSERT)
//      - debug_assert() just calls _fatal(ERR_ASSERT) (so we can trap out to a c debugger)
//
//handling exceptions
//  - all functions must check all err_t return codes 
//    - and check return ints that might be exceptions
//  - on getting an exception, all functions must clean up temp/broken state (usually by destroying vals),
//    and then return either that exception or a superceding one
//  - err_code(err_t) and err_string(err_t) return ERRCODE names and error descriptions as cstr


//for use with gdb so we can break on errors
//#define INTERRUPT_ON_THROW 1
//#define INTERRUPT_ON_FATAL 1

//asserts and arg checks
#define DEBUG_CHECKS 1

//#include "vm.h" //can't include (loop)
#include <stdio.h>
#include <stdint.h> //just needed for val_t forward declaration typedef


#define ERRCODES(errcode) \
  errcode(OK,"OK"), \
  errcode(FATAL, "Unrecoverable error"), \
  errcode(NULL, "Null value"), \
  errcode(BREAK, "Breakpoint"), \
  errcode(UNDEFINED, "Undefined"), \
  errcode(BADTYPE, "Bad type"), \
  errcode(EMPTY, "Empty"), \
  errcode(BADARGS, "Bad argument(s)"), \
  errcode(MISSINGARGS, "Missing argument(s)"), \
  errcode(UNEXPECTED_EOL, "Unexpected end-of-list"), \
  errcode(UNEXPECTED_EOC, "Unexpected end-of-code"), \
  errcode(DICT, "Dictionary error"), \
  errcode(NO_DEBUG, "Debug info not enabled"), \
  errcode(NO_DEBUGGEE, "No debuggee"), \
  errcode(DEBUGGEE_EMPTY, "Debuggee empty"), \
  errcode(BAD_OP, "Bad operation"), \
  errcode(NO_PARSER, "Failed to initialize parser"), \
  errcode(BADPARSE, "Parse error"), \
  errcode(IO_ERROR, "IO error"), \
  errcode(EOF, "EOF"), \
  errcode(BADESCAPE, "Bad escape in string"), \
  errcode(LOCKED, "Locked val"), \
  errcode(UNLOCKED, "Unlocked val"), \
  errcode(LOCK, "Lock error"), \
  errcode(THREAD, "Thread error"), \
  errcode(VM_CANCELLED, "VM Cancelled"), \
  errcode(MALLOC, "Malloc failed"), \
  errcode(SYSTEM, "System call failed"), \
  errcode(ASSERT, "Failed Debug assert"), \
  errcode(NOT_IMPLEMENTED, "Not implemented"), \
  errcode(STARTDEBUG, "Debug trigger"), \
  errcode(THROW, "Internal throw"), \
  errcode(USER_THROW, "Throw from code")

//this enum only so we don't have to count them
#define ERR_PENUM(e,s) POS_ERR_##e
enum pos_error { ERRCODES(ERR_PENUM), _NEXT_ERROR, N_ERRORS = _NEXT_ERROR-1 };
#undef ERR_PENUM

#define ERR_ENUM(e,s) ERR_##e = -POS_ERR_##e
typedef enum { ERRCODES(ERR_ENUM), LAST_ERROR=-N_ERRORS } err_t;
#undef ERR_ENUM

extern const char *errcodes[];
extern const char *errcstrings[];

const char *err_code(err_t err);
const char *err_string(err_t err);
err_t err_parse(const char *str, unsigned int len);

void err_fprintf(FILE *file, err_t err);
void fatal_fprintf(FILE *file, err_t err);

#ifdef DEBUG_VAL
typedef __uint128_t val_t; //forward declaration for debug val_t
#else
typedef uint64_t val_t; //forward declaration
#endif
err_t errval_fprintf(FILE *file, val_t err);

void vm_perrornum(err_t err);
void vm_pfatal(err_t err);

#define cleanup_throw_if(e,cleanup,c) do{ if(c) { cleanup; return _throw(e); } }while(0)
#define throw_if_goto(e,label,c) do{ if(c) { r=_throw(e); goto label; } }while(0)
#define throw_if(e,c) do{ if(c) { return _throw(e); } }while(0)

#ifdef DEBUG_CHECKS
#define ARGCHECKS 1
#define debug_assert(c) do{ if(!(c)) { fprintf(stderr,"FAILED ASSERT(%s) at %s:%d\n",#c,__FILE__,__LINE__); _fatal(ERR_ASSERT); } }while(0)
#define debug_assert_r(c) do{ if(!(c)) { fprintf(stderr,"FAILED ASSERT(%s) at %s:%d\n",#c,__FILE__,__LINE__); return _fatal(ERR_ASSERT); } }while(0)
#define fatal_if(e,c) do{ if(c) { fprintf(stderr,"FATAL %s(%s) at %s:%d\n",err_code(e),#c,__FILE__,__LINE__); return _fatal(e); } }while(0)
#else
#define debug_assert(c) do{} while(0)
#define debug_assert_r(c) do{} while(0)
#define fatal_if(e,c) if (c) return _fatal(e)
#endif

//all arguments should be checked before calling library functions (so we can throw the most relevant error to user code)
//  - all library functions should also argcheck() their args so we can debug the vm more easily
//  - argcheck failures are fatal, since they are never supposed to fail
//  - when not debugging concat internals, argchecks are no-ops
//  TODO: add corresponding opcheck() command(s)
//  FIXME: use these -- we merged them into punbox but aren't using yet
#ifdef ARGCHECKS
#define argcheck(c) do{ if(!(c)) { fprintf(stderr,"FAILED ARGCHECK(%s) at %s:%d\n",#c,__FILE__,__LINE__); _fatal(ERR_BADARGS); } }while(0)
#define argcheck_r(c) do{ if(!(c)) { fprintf(stderr,"FAILED ARGCHECK(%s) at %s:%d\n",#c,__FILE__,__LINE__); return _fatal(ERR_BADARGS); } }while(0)
#else
#define argcheck(c) do{}while(0)
#define argcheck_r(c) do{}while(0)
#endif


#define throw_lst_empty(list) throw_if(ERR_EMPTY,_val_lst_empty(list))
//#define throw_coll_empty(coll) throw_if(ERR_EMPTY,val_coll_empty(coll))
//#define throw_vm_empty(vm) throw_if(ERR_EMPTY,!vm_has(vm,1))


#ifdef INTERRUPT_ON_THROW
err_t _throw_(err_t err);
#define _throw(err) (fprintf(stderr,"ERROR(%s,%d) at %s:%d\n",err_code(err),err,__FILE__,__LINE__), _throw_(err))
#else
#define _throw(err) (err)
#endif

#ifdef INTERRUPT_ON_FATAL
err_t _fatal_(err_t err);
#define _fatal(err) (fprintf(stderr,"FATAL %s(%d) at %s:%d\n",err_code(err),err,__FILE__,__LINE__), _fatal_(err))
#else
#define _fatal(err) (fprintf(stderr,"FATAL ERROR(%d) at %s:%d\n",err,__FILE__,__LINE__), ERR_FATAL)
//#define _fatal(err) (ERR_FATAL)
#endif

typedef struct _vm_t vm_t; //prevent include loop
err_t vm_perror(vm_t *vm);
//
err_t vm_fatal_str(int ret, const char *msg);
err_t vm_throw(vm_t *vm, err_t err);
err_t vm_debug_throw(vm_t *vm, err_t err);
err_t vm_user_throw(vm_t *vm);

int err_isfatal(err_t err);

#endif
