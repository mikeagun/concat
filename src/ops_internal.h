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

#ifndef __OPS_INTERNAL_H__
#define __OPS_INTERNAL_H__ 1

#include "ops.h"

err_t vm_init_op(vm_t *vm, const char *word, vm_op_handler *op);
err_t vm_init_compile_op(vm_t *vm, const char *word, const char *code);

//helper macros to acquire/release stack vals
//  - I'm hoping (though not sure yet) these will help transition to bytecode vs old use of stack functions
//  - by convention we call stack pointer p, work pointer q, cont pointer cp
//    - the pop/get/push macros declare and set (or just set for _more macros)
//      the appropriate pointer to the start of the allocated/requested range
//      the op is then required to init/destroy (push/pop) or leave valid (get) the vals in that range
//    - e.g. __op_popget declares and initializes p to top-2, then decrements stack length
//      - op has access to the top 2 els of stack via p,p+1, and must destroy/move out p+1
//  - all the arg macros require semicolon at end (though not do{}while(0) wrapped) so the syntax highlighting in my editor is happy
//    - only the ones that take an argument are function-like macros (e.g. __op_popget; OR __op_pop(4);)

#define __op_popget \
  throw_if(ERR_MISSINGARGS,!vm_has(vm,2)); \
  val_t *p; \
  vm_popget_(vm,&p)

#define __op_pop2get \
  throw_if(ERR_MISSINGARGS,!vm_has(vm,3)); \
  val_t *p; \
  vm_pop2get_(vm,&p)

#define __op_popget2 \
  throw_if(ERR_MISSINGARGS,!vm_has(vm,3)); \
  val_t *p; \
  vm_popget2_(vm,&p)

#define __op_popngetn(npop,nget) \
  throw_if(ERR_MISSINGARGS,!vm_has(vm,npop+nget)); \
  val_t *p; \
  vm_popngetn_(vm,npop,nget,&p)

#define __op_popngetn_more(npop,nget) \
  throw_if(ERR_MISSINGARGS,!vm_has(vm,npop+nget)); \
  vm_popngetn_(vm,npop,nget,&p)

#define __op_getpush \
  throw_if(ERR_MISSINGARGS,!vm_has(vm,1)); \
  val_t *p; \
  vm_getextend1_(vm,&p)

#define __op_get2push \
  throw_if(ERR_MISSINGARGS,!vm_has(vm,2)); \
  val_t *p; \
  vm_getnextendn_(vm,2,1,&p)

#define __op_getpush2 \
  throw_if(ERR_MISSINGARGS,!vm_has(vm,1)); \
  val_t *p; \
  vm_getnextendn_(vm,1,2,&p)

#define __op_push(n) \
  val_t *p; \
  do{err_t e; if ((e = vm_extend_(vm,n,&p))) return e; }while(0)

#define __op_push_more(n) \
  do{err_t e; if ((e = vm_extend_(vm,n,&p))) return e; }while(0)

#define __op_push1 __op_push(1)
#define __op_push2 __op_push(2)
#define __op_push3 __op_push(3)
#define __op_push1_more __op_push_more(1)

#define __op_pop1 \
  throw_vm_empty(vm); \
  val_t *p; \
  vm_pop1_(vm,&p)

#define __op_pop2 \
  throw_if(ERR_MISSINGARGS,!vm_has(vm,2)); \
  val_t *p; \
  vm_pop2_(vm,&p)

#define __op_pop3 \
  throw_if(ERR_MISSINGARGS,!vm_has(vm,3)); \
  val_t *p; \
  vm_pop3_(vm,&p)

#define __op_pop(n) \
  throw_if(ERR_MISSINGARGS,!vm_has(vm,n)); \
  val_t *p; \
  vm_popn_(vm,n,&p)

#define __op_pop_more(n) \
  throw_if(ERR_MISSINGARGS,!vm_has(vm,n)); \
  vm_popn_(vm,n,&p)

#define __op_get1 \
  throw_vm_empty(vm); \
  val_t *p; \
  vm_get1_(vm,&p)

#define __op_get2 \
  throw_if(ERR_MISSINGARGS,!vm_has(vm,2)); \
  val_t *p; \
  vm_get2_(vm,&p)

#define __op_get3 \
  throw_if(ERR_MISSINGARGS,!vm_has(vm,3)); \
  val_t *p; \
  vm_get3_(vm,&p)

#define __op_wget(n) \
  val_t *q; \
  fatal_if(ERR_MISSINGARGS,val_list_len(&vm->work)<n); \
  do{err_t e; if ((e = vm_wgetn_(vm,n,&q))) return e; }while(0)

#define __op_wpush(n) \
  val_t *q; \
  do{err_t e; if ((e = vm_wextend_(vm,n,&q))) return e; }while(0)

#define __op_wpush1 __op_wpush(1)
#define __op_wpush2 __op_wpush(2)
#define __op_wpush3 __op_wpush(3)
#define __op_wpush4 __op_wpush(4)

#define __op_cpush(n) \
  val_t *cp; \
  do{err_t e; if ((e = vm_cextend_(vm,n,&cp))) return e; }while(0)

#define __op_cpush1 __op_cpush(1)
#define __op_cpush2 __op_cpush(2)

#define __op_cpop(n) \
  fatal_if(ERR_MISSINGARGS,val_list_len(&vm->cont)<n); \
  val_t *cp; \
  vm_cpopn_(vm,n,&cp)

#define __op_cpop1 __op_cpop(1)
#define __op_cpop2 __op_cpop(2)

//generate local var for integer val_t arg and perform bounds check (or other condition)
//  - takes val_int before verifying it it integer, but throws before continuing if it isn't
#define __op_take_iarg(p,argname,cond) \
  valint_t argname = val_int(p); \
  throw_if (ERR_BADARGS,!val_isint(p) || !(cond)); \
  val_clear(p)

//replace top with an (int result) expression over p (which points to top)
#define __op_int_replace(e) \
  throw_vm_empty(vm); \
  val_t *p; \
  err_t r; \
  if ((r = vm_get1_(vm,&p))) return r; \
  val_int_replace(p,e); \
  return 0;



#endif
