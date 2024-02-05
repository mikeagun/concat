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

#ifndef __VAL_H__
#define __VAL_H__ 1
// val.h - concat val interface
//
// if DEBUG_FILENAME defined file vals keep filename string (for easier debugging / interactive code)
#define DEBUG_FILENAME 1

// if DEBUG_VAL defined every 64 bit val allocated with extra 64 bit val for debug
//#define DEBUG_VAL 1
// if DEBUG_VAL_EVAL defined, vm->debug_val_eval is set, and debug val is an eval type, eval it before val
//#define DEBUG_VAL_EVAL 1

//debug_val_eval requires debug_val
#ifdef DEBUG_VAL_EVAL
#define DEBUG_VAL 1
#endif


// 
// The val_t layout uses tagged pointers and NaN-space to store all types in a 64 bit integer efficiently.
// Some specific ideas came from "Inverse Pointer Unboxing" (Fairhurst, 2017).
//
// NOTE: The below is mostly current vm implementation detail which doesn't directly impact bytecode (except which types we should compile to).
//
// Core val_t Types:
// - opcode/native - index into dowork vm jump table
// - int (32 bit) - up to 47 bits available, 32 for performance
// - float (64 bit) - double float (bit inverted so all other types fit in NaN space)
// - tagged pointer to valstruct - uses bits in NaN space and outside canonical pointer space (currently 47 bits for intel) for tag
//
//
// Boxing/Unboxing:
// - use __*_val and __val_* macros/functions below for boxing/unboxing val_t
//   - use more specific (e.g. code vs lst) functions where appropriate for better debug checks
//   - opcode/native - box/unbox is no-op
//   - int (32 bit) - box is | mask, unbox is no-op (take low 32 bits)
//   - float - 64 bit double float - box/unbox is ~
//     - for float box/unbox are inline functions since we need to both invert and reinterpret bits
//   - tagged pointer - box/unbox is |/& mask
//
//
// Valstruct Types:
// - valstruct types are currently tagged as str/lst/other with bitmask
//   - could use 3 bit typecode for typecode indexing, TODO: consider typecode in tag
//   - the type member of valstruct has the specific type (tag optimizes for str/lst)
//   - an alternative tagging might be opcode/push/eval (and/or typecode as above)
// - str types - valstruct contains offset+length view on refcounted byte array buffer
//   - string - ascii string (or bytes)
//   - ident - identifier for dictionary eval
//   - bytecode - not implemented in current vm
// - lst types - valstruct contains offset+length view on refcounted val_t array buffer
//   - code - code/quotation to eval
//   - list - list of vals that just gets pushed on eval
// - ref - threadsafe refcounted reference val, TODO: ref currently too heavy
// - vm - used for threading, debugging, or other cases where vm on vm stack is useful
// - file - c FILE* (with filename if DEBUG_FILENAME is set)
// - fd - file descriptor (with filename if DEBUG_FILENAME is set)
//
// Debug Support:
// - when DEBUG_VAL defined, each 64 bit val allocated with an extra 64 bit debug val
//   - can be used for storing anything debug related (extra val is just a regular 64 bit val)
// - with DEBUG_VAL_EVAL and vm debug_val_eval flag set
//
// The current vat_t implementation is boxed into a uint64_t core type.
// This lets val_t contain opcodes/ints/floats directly, or a tagged pointer to a valstruct_t.
//
// This means every val on a stack/list (or in a dictionary) takes 64 bits,
// and for valstruct types contains a pointer to a valstruct_t.
// Boxing and unboxing are zero/one bitwise operations.
//
// dup is extremely common in concat (many ops consume operands)
// so we need to minimize the cost of cloning a val_t. All valstruct types
// are either refcounted or views on a refcounted buffer, so cloning is
// just a 64 bit copy, with recount increment (and possible valstruct copy) for valstructs.
// 
// valstructs are allocated from a memory pool (which isn't yet threadsafe, so disabled by default),
// since we so frequently create/destroy them. valstruct_t contains a type enum and and union of possible valstruct types
//
// When debug val support isn't enabled val_t is a uint64_t and there is no debug val code compiled in (zero overhead)
// With debug val enabled the val_t is a uint128_t (the second uint64 is the debug val), with efforts to minimize debug val overhead
//
//
// The main goal is to minimize cpu cycles spent typechecking and getting to dereferenced data, and
// minimize val_t size.
//
// Boxing/unboxing types is zero or one bitwise operations (zero for opcode/native, ~ for double, &/| for tagged pointers).
// The pointer tags are currently a bitmap, but could become typecode for indexing
//
// For str/lst (string-like/list-like) vals the valstruct contains a view on a reference
// counted string/list buffer, which uses a flexible array member to allocate the buffer meta+data together.
// - copy-on-write strings/lists (substring just updates view)
//
// doubles are stored bit inverted, which lets current intel pointer space (47 bits) plus a few extra bits exist in NaN space
//
//
//
// Implementation Notes:
// - current implementation uses non-canonical pointer space to store double, uint32, and tagged pointers
// - some other options would be tagged pointers (use low bits from alignment) or store other data types in double NaN-space
// - current solution requires pointer space to be no more than 47 bits, with the high bits all zero
// - this code is very-much platform dependent (but should be general across 64bit machines with 47 bit pointers plus native 64 bit integers and 64bit double floats)
//
//FIXME: pool allocators not currently threadsafe -- falling back to malloc/free for now
//TODO: standardize val function naming (e.g. underscore prefix/suffix)

#include "vm_err.h"
#include "opcodes.h"
#include "val_printf_fmt.h"

#include <stdint.h>
#include <stdio.h>
#include <semaphore.h>






/////////////////////////////// Inverse Pointer Unboxing ////////////////////////////////////////
// - not a new idea, so ideas from various sources, but some specific ideas came from: "Inverse Pointer Unboxing" (Fairhurst, 2017)
// - goal is not just to pack all types into a 64bit val, but to do so with minimum cpu cycles to do boxing, unboxing, typechecking, and type determining
//
// - the current 64bit val_t can contain one of:
//   - double (non-NaN or infinity) - bits are inverted, which has nice property that canonical pointers fit in NaN space and valid doubles are outside canonical pointer space
//   - pointer - if we currently store opcodes in normal pointer space
//   - tagged pointer - pointer to struct for extended val
//     - one of: list-type, string-type, other
//   - integer - 32 bits used, but up to 47 bits available
//
//IEEE754 64bit NaN
// - bit fields:
//   - 1 bit sign
//   - 11 bits exponent
//   - 53 bits mantissa
// - an NaN is a value where all exponent bits are 1 (first bit of mantissa 0=signalling NaN, 1=quiet NaN)
//   - mantissa can be anything except all zero (exponent all 1 with mantissa all zero is infinity NaN)
//
//64bit pointer
// - I am assuming x86_64 architecture, or another architecture with <52 bits of usable address space
//   - Some CPUs are already up to 52+ bits usable address space, so it will get harder to stick within inverse double NaN space + canonical pointer space at the same time
//   - using low bits of address (bits that are zero per alignment) is another option, but that requires more bit twiddling than this approach
//     - or we can add a couple of bit shuffles to unpack doubles at that point, or we can make doubles all valstruct types, and then even once canonical pointer space is a full 64 bits we could use the low alignment bits instead (and assuming/enforcing that valstructs are aligned)
// - 47bits, with high bits either all one or all zero (that actually makes it 48 bits, but high half typically reserved for kernel so we can assume 47 bits for now)
//   - depending on system? - high bits ignored (47 bits or less), high bits must be zero (47 bits or less), or high bits must all be the same (48 bits)
//   - high half of address range typically reserved for kernel in 48bit solutions we can still assume 47bit in userspace
//   - this assumption will become obsolete over time (we already have machines with 48+ bits of RAM), but for now "it works on my machine"
//     - after concat is stable, the plan is to refactor opcodes to support minimal vms with reduced opcode sets (e.g. for 8bit mcu)
//   - TODO: keep updating our val packing scheme as needed for modern hardware
//     - probably decouple from arch for reference implementation (with several sizes though), then have optimized impls for architectures
//     - we will eventually need two or more bit packing schemes -- one for mcus (10-16bit addr), the other for 64bit machines (and 32 bit only if there is a usecase for it)
//
//In the inverse punboxing scheme doubles, integers, and other tagged types are stored in non-canonical pointer space, with pointers stored in canonical pointer space
// - in our scheme, opcodes are stored in the low addresses of canonical pointer space currently (and implementation specific native ops in the rest of the address space)
// - if value <=2^47-1, then it is a canonical pointer (i.e. opcode if <N_OPS, else native)
// - if value >2^51-1, then it is a bit-inverted double
//   - by inverting the bits of the ieee754 64bit float, the range of non-NaN doubles is mutually exclusive from the canonical pointer range
// - else we test tag bits in bit 48-53 to determine type
//   - we currently use these as a type bitmap, but to pack more type info we could use bits 48,49,50 as a 3 bit numeric type id


#define val_swap(lhs,rhs) do{val_t hold__=*(lhs);*(lhs)=*(rhs);*(rhs)=hold__;}while(0)
#define val64_swap(lhs,rhs) do{uint64_t hold__=*(lhs);*(lhs)=*(rhs);*(rhs)=hold__;}while(0)

//NULL is an opcode (which throws NULL exception)
#define VAL_NULL ((val_t)0UL)

enum val_type {
  TYPE_STRING,
  TYPE_IDENT,
  TYPE_BYTECODE,
  TYPE_LIST,
  TYPE_CODE,
  TYPE_DICT,
  TYPE_REF,
  TYPE_FILE,
  TYPE_FD,
  TYPE_VM,
  //TYPE_NATIVE,
  //TYPE_DOUBLE,
  //TYPE_INT,
};

// when DEBUG_VAL defined, every uint64_t val has a second val attached for debugging use
// - makes val_t uint128_t (val is still 64 bits)
// - other than doubling val_t size doesn't otherwise affect valstruct
// - since debug info is a val, it can be used for anything:
//   - tracking source script and builtin evaluation (.cat / .c line, pos, and stack comments)
//   - c vm debugging (tracking c source code evaluation)
//   - abstract interpretor / evaluation tracking info
//   - type/value/stack-effect validation
//   - validating/calculating constraints
// - with DEBUG_VAL_EVAL, non-null eval-type debug vals are evaluated in place of regular val
//   - regular val is placed on top of stack, which allows for validation/transformation/logging before evaluating regular val (or not)
//   - splits regular/debug vals (so regular val with no debug info attached is on top of stack)
// - FIXME: right now assumes little endian -- at confirm with static assert
typedef uint64_t val64_t; //basic val type (same as val_t when no DEBUG_VAL)
#ifdef DEBUG_VAL

typedef __uint128_t val_t; //each 64 bit val paired with second debug val

// get the debug val for a val_t (alternatively could right shift)
#define __val_dbg_val(val) ((val64_t*)(&(val)))[1]
//#define __val_dbg_val(val) ((val) >> 64)
// swap debug and regular val in val_t
#define __val_dbg_swap(val) val64_swap(&val, &__val_dbg_val(val))
// construct val with debug value attached
#define __val_dbg(val,dbg) ( ((val_t)((val64_t)(dbg)) << 64) | (val_t)(val) )

//strip debug val from val_t
#define __val_dbg_strip(val) ((val_t)((val64_t)(val)))

// set regular value (retain debug val) of val_t
#define __val_set(val,newval) do{ *(val64_t*)(val) = (val64_t)(newval); }while(0)

// re-set regular value (destroy old regular val) of val_t
// - ignores debug val of newval (so calling code is responsible for that)
#define __val_reset(val,newval) do{ val_destroy(__val_dbg_strip(*val)); *(val64_t*)(val) = (val64_t)(newval); }while(0)

#define __val_dbg_destroy(val) do{ val64_t* dbg = &__val_dbg_val(val); if (!val_is_op(*dbg)) { val_destroy((val_t)*dbg); *dbg = 0; } }while(0)

#else //DEBUG_VAL not set, debug val macros should be no-ops (and debug val treated as NULL)

//normal val_t is uint64_t (i.e. same as val64_t)
typedef uint64_t val_t;

//debug val not enabled - NULL
#define __val_dbg_val(val) VAL_NULL
//debug val not enabled - replaces val with NULL
#define __val_dbg_swap(val) do{ val = VAL_NULL; }while(0)
//debug val not enabled - ignores second arg
#define __val_dbg(val,dbg) (val)
//debug val not enabled - no-op
#define __val_dbg_strip(val) (val)

//debug val not enabled - just set val = newval
#define __val_set(val,newval) do{ *(val) = newval; }while(0)
#define __val_reset(val,newval) do{ val_destroy(*val); *(val) = newval; }while(0)

#define __val_dbg_destroy(val) do{}while(0)

#endif


// str_t/lst_t valstructs hold an offset+length view on a refcounted buffer
// sbuf_t/lbuf_t structs hold the actual bytes/vals in flexible array member
// - buf destroyed when last str/lst referencing it is destroyed
// - copy-on-write (when isn't already single owner)
// - we also keep a dirty flag for lbuf to know whether vals outside off+len must be destroyed
//   - if clean only need to destroy within current view

typedef struct _sbuf_t {
  unsigned int size;
  unsigned int refcount;
  char p[];
} sbuf_t;

typedef struct _lbuf_t {
  unsigned int size;
  unsigned int refcount;
  unsigned int dirty;
  val_t p[];
} lbuf_t;

typedef struct _str_t {
  unsigned int off;
  unsigned int len;
  sbuf_t *buf;
} str_t;

typedef struct _lst_t {
  unsigned int off;
  unsigned int len;
  lbuf_t *buf;
} lst_t;


// ref_t valstruct contains a shared val_t, refcount, and synchronization primitives
// - supports lock/unlock (at language level via guard ops for safety)
// - wait/signal/broadcast to implement any other thread synchronization on top of ref_t

typedef struct _ref_t {
  // TODO: shrink down ref_t (currently largest type)
  // - extra indirection level (at least for part)?
  // - use tag bit with different ref_t type instead of valstruct?
  val_t val;
  unsigned int refcount;
  sem_t lock;
  sem_t wait;
  unsigned int nwait;
} ref_t;

// file_t/fd_t contain FILE_* or integer file descriptor respecitvely for filesystem access
// - with DEBUG_FILENAME they also keep filename string for debug printing

typedef struct _file_t {
  FILE *f;
  enum { NONE=0,DOCLOSE=1} flags;
  int refcount;
#ifdef DEBUG_FILENAME
  char *fname; //name of the file (just for printing to user, not needed in real impl)
  //TODO: support DEBUG_LINENO
#endif
} file_t;

typedef struct _fd_t {
  int fd;
  enum { FD_NONE=0,FD_DOCLOSE=1} flags;
  int refcount;
#ifdef DEBUG_FILENAME
  char *fname; //name of the file (just for printing to user, not needed in release)
  //TODO: support DEBUG_LINENO
#endif
} fd_t;

// dict_t - scoped hashtable-based dictionary (maps string to val_t)
// - the op/builtin dictionary is loaded by _vm_init_dict in vm.c
struct hashtable;
struct _valstruct_t;
typedef struct _dict_t {
  struct hashtable *h;
  struct _valstruct_t *next;
} dict_t;

// valstruct_t has a type enum and union of implemented valstruct types
typedef struct _valstruct_t {
  enum val_type type;
  union {
    str_t str;
    lst_t lst;
    dict_t dict;
    ref_t ref;
    file_t file;
    fd_t fd;
    vm_t *vm;
  } v;
} valstruct_t;

//opcodes and natives use _OP_TAG - opcode if <N_OPS, native op else
#define _OP_TAG    (0x0000)
//integer type (normally 32bits, but actually a 47 bit field)
#define _INT_TAG    (0x0001)
#define _STR_TAG    (0x0002)
#define _LST_TAG    (0x0004)
//generic value type (look at valstruct to get 
#define _VAL_TAG    (0x0008)
//TODO: figure out what to do with these? (reference val, bare native, errnum, short ident, bitmap, and s9 come to mind) -- or remap types altogether
#define _TAG5       (0x00F0)

#define __val_tag(v) ((val64_t)(v)>>47)

//with DEBUG_VAL defined, val_t is uint128_t - need cast to uint64_t to ignore debug val for normal ops

// canonical pointer (core opcode if < N_OPS, implementation specific native otherwise)
#define val_is_null(v) (VAL_NULL==((val64_t)(v)))
#define val_is_op(v) ((val64_t)(v)<=(1UL<<47)-1)
#define val_is_opcode(v) ((val64_t)(v)<N_OPCODES)
#define val_is_native(v) ((val64_t)(v)>=N_OPCODES && (val64_t)(v)<=(1UL<<47)-1)
#define __val_op(v) (*(int*)(&v))
#define __op_val(op) ((val_t)(op))

// double in non-canonical pointer space (pointers and tags fit into NaN-space for the inverted double)
// - these ones need to be inline functions instead of macros since we need a temp var for invert+cast
#define val_is_double(v) ((v)>(1UL<<51)-1)
double __val_dbl(val_t val);
val_t __dbl_val(const double f);
//mantissa bits for double
#define VALDBL_MANT_BITS 53

// int32 in non-canonical pointer space (can actually use up to 47 bits, but 32 bits standard for performance)
#define val_is_int(v) ((((uint64_t)(v))>>47) == _INT_TAG)
#define __int_val(i) (val_t)( (uint64_t)((uint32_t)(i)) | ((uint64_t)_INT_TAG<<47) )
#define __int32_val(i) (val_t)( (uint64_t)((uint32_t)(i)) | ((uint64_t)_INT_TAG<<47) )
#define __int47_val(i) (val_t)( (uint64_t)((uint64_t)(i) & ((1UL<<47)-1)) | ((uint64_t)_INT_TAG<<47) )
#define __val_int(v) (*((int32_t*)&(v)))
#define __val_int32(v) (*((int32_t*)&(v)))
#define __val_int47(v) (*((int64_t*)&(v)) & ~((uint64_t)_INT_TAG<<47))

// str/lst/other tagged valstruct pointer
#define val_is_str(v) ((((uint64_t)(v))>>47) == _STR_TAG)
#define val_is_lst(v) ((((uint64_t)(v))>>47) == _LST_TAG)
#define val_is_val(v) ((((uint64_t)(v))>>47) == _VAL_TAG)
#define __str_ptr(v) ((valstruct_t*)((uint64_t)(v)& ~(((uint64_t)_STR_TAG<<47))))
#define __lst_ptr(v) ((valstruct_t*)((uint64_t)(v)& ~(((uint64_t)_LST_TAG<<47))))
#define __val_ptr(v) ((valstruct_t*)((uint64_t)(v)& ~(((uint64_t)_VAL_TAG<<47))))
// generic valstruct pointer -- if we have specific bit set/clear instructions the specific versions above may be faster, and also they tend to cause nice segfaults when our assumption fails
#define val_ptr(v) ((valstruct_t*)((v)& ((1UL<<47)-1)))
#define __str_val(p) (val_t)((uint64_t)(p) | ((uint64_t)_STR_TAG<<47))
#define __lst_val(p) (val_t)((uint64_t)(p) | ((uint64_t)_LST_TAG<<47))
#define __val_val(p) (val_t)((uint64_t)(p) | ((uint64_t)_VAL_TAG<<47))

#ifdef VAL_POINTER_CHECKS
//FIXME: implement typechecking in pointer functions/macros
//valstruct_t *__string_ptr(val_t t);
//valstruct_t* __string_ptr(val_t t);
//valstruct_t* __ident_ptr(val_t t);
//valstruct_t* __bytecode_ptr(val_t t);
//valstruct_t* __list_ptr(val_t t);
//valstruct_t* __code_ptr(val_t t);
//valstruct_t* __dict_ptr(val_t t);
//valstruct_t* __ref_ptr(val_t t);
//valstruct_t* __file_ptr(val_t t);
//valstruct_t* __fd_ptr(val_t t);
//valstruct_t* __vm_ptr(val_t t);
//
//val_t __string_val(valstruct_t *p);
//val_t __ident_val(valstruct_t *p);
//val_t __bytecode_val(valstruct_t *p);
//val_t __list_val(valstruct_t *p);
//val_t __code_val(valstruct_t *p);
//val_t __dict_val(valstruct_t *p);
//val_t __ref_val(valstruct_t *p);
//val_t __file_val(valstruct_t *p);
//val_t __fd_val(valstruct_t *p);
//val_t __vm_val(valstruct_t *p);
#else
#define __string_ptr(v) __str_ptr(v)
#define __ident_ptr(v) __str_ptr(v)
#define __bytecode_ptr(v) __str_ptr(v)
#define __list_ptr(v) __lst_ptr(v)
#define __code_ptr(v) __lst_ptr(v)
#define __dict_ptr(v) __val_ptr(v)
#define __ref_ptr(v) __val_ptr(v)
#define __file_ptr(v) __val_ptr(v)
#define __fd_ptr(v) __val_ptr(v)
#define __vm_ptr(v) __val_ptr(v)

#define __string_val(p) __str_val(p)
#define __ident_val(p) __str_val(p)
#define __bytecode_val(p) __str_val(p)
#define __list_val(p) __lst_val(p)
#define __code_val(p) __lst_val(p)
#define __dict_val(p) __val_val(p)
#define __ref_val(p) __val_val(p)
#define __file_val(p) __val_val(p)
#define __fd_val(p) __val_val(p)
#define __vm_val(p) __val_val(p)
#endif

// the specific valstruct types are checked by checking pointer tag and then valstruct.type
#define val_is_string(val) (val_is_str(val) && __str_ptr(val)->type == TYPE_STRING)
#define val_is_ident(val) (val_is_str(val) && __str_ptr(val)->type == TYPE_IDENT)
#define val_is_bytecode(val) (val_is_str(val) && __str_ptr(val)->type == TYPE_BYTECODE)
#define val_is_list(val) (val_is_lst(val) && __lst_ptr(val)->type == TYPE_LIST)
#define val_is_code(val) (val_is_lst(val) && __lst_ptr(val)->type == TYPE_CODE)
#define val_is_file(val) (val_is_val(val) && __val_ptr(val)->type == TYPE_FILE)
#define val_is_fd(val) (val_is_val(val) && __val_ptr(val)->type == TYPE_FD)
#define val_is_dict(val) (val_is_val(val) && __val_ptr(val)->type == TYPE_DICT)
#define val_is_ref(val) (val_is_val(val) && __val_ptr(val)->type == TYPE_REF)
#define val_is_vm(val) (val_is_val(val) && __val_ptr(val)->type == TYPE_VM)

//functions for dealing with vals (mostly just for gdb inspection, we use the above macros in code)
//TODO: add these back (with new names, or macro switch to pick macros or functions, or just use inline functions for all)
//valstruct_t* __val_ptr(val_t val);
//int32_t __val_int(val_t val);
//double __val_dbl(val_t val);
//val_t __str_val(valstruct_t *v);
//val_t __lst_val(valstruct_t *v);
//val_t __val_val(valstruct_t *v);
//val_t __int_val(int32_t i);
//val_t __dbl_val(double v);




valstruct_t* _valstruct_alloc();
void _valstruct_release(valstruct_t *v);


void val_destroy(val_t val);
void val_destroyn(val_t *p, size_t n);

int val_clone(val_t *val, val_t orig);
int val_clonen(val_t *val, val_t *orig, unsigned int n);

//val validation functions (only use during vm debugging) -- these dig into and validate the state of vals to look for memory problems
err_t val_validate(val_t val);
err_t val_validaten(val_t *p, unsigned int n);

//mmemcpy/memmove for val arrays
#define valcpy(dst, src, n) ((val_t*)memcpy(dst,src,sizeof(val_t)*(n)))
#define valmove(dst, src, n) ((val_t*)memmove(dst,src,sizeof(val_t)*(n)))

//clear value(s) to NULL
#define val_clear(p) do{ *(p) = VAL_NULL; }while(0)
#define val_clearn(p, len) (memset(p,'\0',sizeof(val_t)*(len)))

// tostring just calls val_sprintf_ (with fmt_V)
err_t val_tostring(val_t *str, val_t val);

// val_ispush - check if push type - push types are pushed onto the stack when evaluated
int val_ispush(val_t val); //if evaluation of val is identity function

// val_protect - wrap a value so eval returns original value
err_t val_protect(val_t *val);
//err_t val_wprotect(val_t *val);

// conditional ops use the bool value of a val, the integers 0/1 are used for bool return values
#define val_true (__int_val(1))
#define val_false (__int_val(0))

// val_as_bool
int val_as_bool(val_t val);

// relative val comparison for sorting
int val_compare(val_t lhs,val_t rhs);
int val_eq(val_t lhs,val_t rhs);
int val_lt(val_t lhs,val_t rhs);
#define val_gt(lhs,rhs) val_lt(rhs,lhs)

#endif
