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

#include "vm_unsafe_internal.h"
#include "vmstate.h"
#include "ops_internal.h"
#include "valhash.h"
#include "val_func.h"
#include "val_stack.h"
#include "val_num.h"
#include "vm_err.h"

#include "val_list_internal.h"

#define unsafe_stack(vm) (vm->open_list)
#define unsafe_deref(stack) r = val_stack_deref(stack); if (r) return r
#define unsafe_top(stack) (stack->val.list.b->p + stack->val.list.offset + stack->val.list.len - 1)
#define unsafe_push(stack,val) val_list_rpush(stack,val)
#define unsafe_drop(stack) if (stack->val.list.b->refcount == 1) val_destroy(unsafe_top(stack)); --stack->val.list.len


//This file contains unsafe versions of various commands
//NOTE: currently the performance benefits are pretty minimal (2-3% for simple tested cases), since right now most of these are simply skipping/shortening safety checks
//  - first pass at simple code optimization
//  - right now they are only "mostly" unsafe (e.g. deref vm if needed)
//  - more comprehensive solution would be to decompose natives to sub-operations (e.g. type check, validate, operate, cleanup) and optimize by cutting steps where safe

struct hashtable *unsafe_hash = NULL;

err_t ops_unsafe_init(vm_t *vm) {
  err_t r=0;
  r |= vm_init_op(vm,"unsafe",_op_unsafe);

  fatal_if(ERR_FATAL,0 == (unsafe_hash = alloc_hashtable(DEFAULT_HASH_BUCKETS,NULL)));

  r |= init_unsafe_op("pop","unsafe_pop",_op_unsafe_pop);
  r |= init_unsafe_op("dup","unsafe_dup",_op_unsafe_dup);
  r |= init_unsafe_op("swap","unsafe_swap",_op_unsafe_swap);

  r |= init_unsafe_op("inc","unsafe_inc",_op_unsafe_inc);
  r |= init_unsafe_op("dec","unsafe_dec",_op_unsafe_dec);
  r |= init_unsafe_op("+","unsafe_+",_op_unsafe_add);

  return r;
}

err_t vm_unsafe(vm_t *vm, val_t *val) {
  val_t *u = NULL;
  if (val_isident(val)) {
    u = getv_unsafe(val);
  } else if (val_isfunc(val)) {
    u = get_unsafe(val->val.func.name); //TODO fix this when we have standardized op-codes
  }
  if (u) {
    val_destroy(val);
    return val_clone(val,u);
  }
  return 0;
}

val_t *getv_unsafe(val_t *key) {
  return hash_getv(unsafe_hash,key);
}
val_t *get_unsafe(const char *key) {
  return hash_get(unsafe_hash,key);
}

//TODO: right now these are all fatal since they are only currently used at init time
err_t def_unsafe(const char *key, val_t *val) {
  fatal_if(ERR_DICT,-1 == hash_put_kcopy(unsafe_hash,key,val,1));
  return 0;
}

err_t defv_unsafe(val_t *key, val_t *val) {
  fatal_if(ERR_DICT,-1 == hash_putv(unsafe_hash,key,val,1));
  val_destroy(key); //TODO: should we here or let caller? -- if later keys are vals then here is better
  return 0;
}

err_t init_unsafe_op_(const char *word, val_t *val) {
  fatal_if(ERR_DICT,-1 == hash_put_kcopy(unsafe_hash,word,val,0));
  return 0;
}

err_t init_unsafe_op(const char *word, const char *name, vm_op_handler *op) {
  val_t t;
  fatal_if(ERR_DICT,-1 == hash_put_kcopy(unsafe_hash,word,val_func_init(&t,op,name),0));
  return 0;
}

err_t init_unsafe_op_keep(const char *word, const char *name, vm_op_handler *op) {
  val_t t;
  fatal_if(ERR_DICT,-1 == hash_put_kcopy(unsafe_hash,word,val_func_init_keep(&t,op,name),0));
  return 0;
}

err_t init_unsafe_compile_op(vm_t *vm,const char *word, const char *code) {
  val_t t;
  err_t r;
  val_code_init(&t);
  if ((r = vm_compile_code(vm,code,strlen(code),&t,OPT_NATIVEFUNC))) return r;
  fatal_if(ERR_DICT, -1 == hash_put_kcopy(unsafe_hash,word,&t,0));
  return 0;
}


err_t _op_unsafe(vm_t *vm) {
  __op_get1;
  return vm_unsafe(vm,p);
}


err_t _op_unsafe_pop(vm_t *vm) {
  val_t *stack = unsafe_stack(vm);
  unsafe_drop(stack);
  return 0;
}

err_t _op_unsafe_dup(vm_t *vm) {
  val_t *stack = unsafe_stack(vm);
  val_t *top = unsafe_top(stack);
  err_t r;
  val_t t;
  if ((r = val_clone(&t,top))) return r;
  if ((r = unsafe_push(stack,&t))) return r;
  return 0;
}

err_t _op_unsafe_swap(vm_t *vm) {
  err_t r;
  val_t *stack = unsafe_stack(vm);
  unsafe_deref(stack);
  val_t *top = unsafe_top(stack);
  val_swap(top,top-1);
  return 0;
}

err_t _op_unsafe_inc(vm_t *vm) {
  err_t r;
  val_t *stack = unsafe_stack(vm);
  unsafe_deref(stack);
  val_t *top = unsafe_top(stack);
  ++top->val.i;
  return 0;
}

err_t _op_unsafe_dec(vm_t *vm) {
  err_t r;
  val_t *stack = unsafe_stack(vm);
  unsafe_deref(stack);
  val_t *top = unsafe_top(stack);
  --top->val.i;
  return 0;
}

err_t _op_unsafe_add(vm_t *vm) {
  err_t r;
  val_t *stack = unsafe_stack(vm);
  unsafe_deref(stack);
  val_t *top = unsafe_top(stack);
  val_num_add(top-1,top);
  unsafe_drop(stack);
  return 0;
}

