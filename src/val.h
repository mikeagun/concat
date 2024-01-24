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
#include <stdint.h>
#include <stdio.h>
#include <semaphore.h>

#include "vm_err.h"
#include "opcodes.h"
#include "val_printf_fmt.h"

#define DEBUG_FILENAME 1

//val packed into uint64_t
// - double floats, int32 (up to int47), and tagged 64 bit pointers stored in 64 bit field
// - lots of macros for basic type operations
//   - set of macros for casting/typechecking vals which is mostly by just a few bit twiddles
//   - other macros for operations we want to guarantee are fast
// - shared valstruct type containing union for vals which need more space than uint64
//
//valstruct and children
// - anything that doesn't fit into the int64 val_t goes into a valstruct (which the val_t contains a pointer to
// - for array-types (like string,list), the valstruct points to a second buffer struct with a flexible array member to contain the list
//   - at most 2 dereferences to get the data (deref valstruct from val_t, deref list buffer from valstruct)
//   - at most 1 deref (valstruct) to get the size of the list (since we make list types windows on buffer structs)
//
//Implementation notes
//  - current implementation uses non-canonical pointer space to store double, uint32, and tagged pointers
//  - some other options would be tagged pointers (use low bits from alignment) or store other data types in double NaN-space
//  - current solution requires pointer space to be no more than 47 bits, with the high bits all zero
//  - this code is very-much platform dependent (but should be general across 64bit machines with 47 bit pointers plus native 64 bit integers and 64bit double floats)
//FIXME: pool allocators not currently threadsafe -- falling back to malloc/free for now
//TODO: normalize and codify val function naming (e.g. underscore prefix/suffix)



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
//   - this assumption will become obsolete over time (we already have machines with 48+ bits of RAM), but for now "it works on my machine" (and if it is good enough for that abomination called javascript, ...)
//   - TODO: keep updating our val packing scheme as needed for modern hardware
//     - we will eventually need two or more bit packing schemes -- one for mcus (10-16bit addr), the other for 64bit machines (and 32 bit only if there is a usecase for it)
//     - this is all just VM implementation detail, which only impacts the language in terms of standard float/int precision and range, and doesn't directly impact bytecode (except which types we should compile to)
//
//In the inverse punboxing scheme doubles, integers, and other tagged types are stored in non-canonical pointer space, with pointers stored in canonical pointer space
// - in our scheme, opcodes are stored in the low addresses of canonical pointer space currently (and implementation specific native ops in the rest of the address space)
// - if value <=2^47-1, then it is a canonical pointer (i.e. opcode if <N_OPS, else native)
// - if value >2^51-1, then it is a bit-inverted double
//   - by inverting the bits of the ieee754 64bit float, the range of non-NaN doubles is mutually exclusive from the canonical pointer range
// - else we test tag bits in bit 48-53 to determine type
//   - we currently use these as a type bitmap, but to pack more type info we could use bits 48,49,50 as a 3 bit numeric type id


///////////////////////////// concat packed val_t /////////////////////////////////////////
// - we use above unboxing rules to get bare pointers (which we use as ops / natives) and doubles
// - for other tagged vals, we currently break them into:
//   - 32bit integer
//   - str-type valstruct val (i.e. string,ident,bytecode)
//   - lst-type valstruct val (i.e. list or code)
//   - other valstruct val (e.g. file, ref, dict, vm)
// - str and lst type vals have lots of operations that work identically on all of them, and they are the most used non-numerics, so they are the most useful to keep separate

#define val_swap(lhs,rhs) do{val_t hold__=*(lhs);*(lhs)=*(rhs);*(rhs)=hold__;}while(0)

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

typedef uint64_t val_t;
#define VAL_NULL ((val_t)0UL)

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

typedef struct _ref_t {
  //TODO: shrink down ref_t (currently largest type)
  //  extra indirection level (at least for part)
  //  use tag bit with different ref_t type instead of valstruct
  val_t val;
  unsigned int refcount;
  sem_t lock;
  sem_t wait;
  unsigned int nwait;
} ref_t;

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

struct hashtable;
struct _valstruct_t;
typedef struct _dict_t {
  struct hashtable *h;
  struct _valstruct_t *next;
} dict_t;

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

#define __val_tag(v) ((v)>>47)

//canonical pointer (core opcode if < N_OPS, implementation specific native otherwise)
#define val_is_null(v) (VAL_NULL==(v))
#define val_is_op(v) ((v)<=(1UL<<47)-1)
#define val_is_opcode(v) ((v)<N_OPCODES)
#define val_is_native(v) ((v)>=N_OPCODES && (v)<=(1UL<<47)-1)
#define __val_op(v) (*(int*)(&v))
#define __op_val(op) ((val_t)(op))

//double in non-canonical pointer space (pointers and other types fit into NaN-space for the inverted double)
#define val_is_double(v) ((v)>(1UL<<51)-1)
double __val_dbl(val_t val);
val_t __dbl_val(const double f);
//mantissa bits for double
#define VALDBL_MANT_BITS 53

//int32 in non-canonical pointer space (can actually use up to 47 bits, but 32 bits standard for performance)
#define val_is_int(v) (((v)>>47) == _INT_TAG)
#define __int_val(i) ( (uint64_t)((uint32_t)(i)) | ((uint64_t)_INT_TAG<<47) )
#define __int32_val(i) ( (uint64_t)((uint32_t)(i)) | ((uint64_t)_INT_TAG<<47) )
#define __int47_val(i) ( (uint64_t)((uint64_t)(i) & ((1UL<<47)-1)) | ((uint64_t)_INT_TAG<<47) )
#define __val_int(v) (*((int32_t*)&(v)))
#define __val_int32(v) (*((int32_t*)&(v)))
#define __val_int47(v) (*((int64_t*)&(v)) & ~((uint64_t)_INT_TAG<<47))


#define val_is_str(v) (((v)>>47) == _STR_TAG)
#define val_is_lst(v) (((v)>>47) == _LST_TAG)
#define val_is_val(v) (((v)>>47) == _VAL_TAG)
#define __str_ptr(v) ((valstruct_t*)((v)& ~(((uint64_t)_STR_TAG<<47))))
#define __lst_ptr(v) ((valstruct_t*)((v)& ~(((uint64_t)_LST_TAG<<47))))
#define __val_ptr(v) ((valstruct_t*)((v)& ~(((uint64_t)_VAL_TAG<<47))))
//generic val->pointer -- if we have specific bit set/clear instructions the specific versions above may be faster, and also they tend to cause nice segfaults when our assumption fails
#define val_ptr(v) ((valstruct_t*)((v)& ((1UL<<47)-1)))
#define __str_val(p) ((uint64_t)(p) | ((uint64_t)_STR_TAG<<47))
#define __lst_val(p) ((uint64_t)(p) | ((uint64_t)_LST_TAG<<47))
#define __val_val(p) ((uint64_t)(p) | ((uint64_t)_VAL_TAG<<47))

#ifdef VAL_POINTER_CHECKS
//FIXME: implement typechecking in pointer functions/macros
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



#define val_true (__int_val(1))
#define val_false (__int_val(0))



valstruct_t* _valstruct_alloc();
void _valstruct_release(valstruct_t *v);


void val_destroy(val_t val);
void val_destroyn(val_t *p, size_t n);

int val_clone(val_t *val, val_t orig);
int val_clonen(val_t *val, val_t *orig, unsigned int n);

//val validation functions (only use during VM debugging) -- these dig into and validate the state of vals to look for memory problems
err_t val_validate(val_t val);
err_t val_validaten(val_t *p, unsigned int n);

//mmemcpy/memmove for val arrays
#define valcpy(dst, src, n) ((val_t*)memcpy(dst,src,sizeof(val_t)*(n)))
#define valmove(dst, src, n) ((val_t*)memmove(dst,src,sizeof(val_t)*(n)))

//clear value(s) to INVALID
#define val_clear(p) do{ *(p) = VAL_NULL; }while(0)
#define val_clearn(p, len) (memset(p,'\0',sizeof(val_t)*(len)))


err_t val_tostring(val_t *str, val_t val);

int val_ispush(val_t val); //if evaluation of val is identity function

//wrap a value so it's evaluation is the original value
err_t val_protect(val_t *val);
//err_t val_wprotect(val_t *val);

int val_as_bool(val_t val);

int val_compare(val_t lhs,val_t rhs);
int val_eq(val_t lhs,val_t rhs);
int val_lt(val_t lhs,val_t rhs);
#define val_gt(lhs,rhs) val_lt(rhs,lhs)

#endif
