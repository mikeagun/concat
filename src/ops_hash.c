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

#include "ops_hash.h"
#include "val_hash_internal.h"
#include "ops.h"
#include "ops_internal.h"
#include "val_func.h"
#include "val_num.h"
#include "vm_err.h"

err_t ops_hash_init(vm_t *vm) {
  err_t r=0;
  if ((r = vm_init_op(vm,"hget",_op_hget))) return r;
  if ((r = vm_init_op(vm,"hhas",_op_hhas))) return r;
  //if ((r = vm_init_op(vm,"hset",_op_hset))) return r;
  //
  if ((r = vm_init_op(vm,"defined",_op_defined))) return r;
  if ((r = vm_init_op(vm,"getdef",_op_getdef))) return r;
  if ((r = vm_init_op(vm,"def",_op_def))) return r;
  if ((r = vm_init_op(vm,"mapdef",_op_mapdef))) return r;

  if ((r = vm_init_op(vm,"resolve",_op_resolve))) return r;

  if ((r = vm_init_op(vm,"scope",_op_scope))) return r;
  if ((r = vm_init_op(vm,"savescope",_op_savescope))) return r;
  if ((r = vm_init_op(vm,"usescope",_op_usescope))) return r;
  if ((r = vm_init_op(vm,"usescope_",_op_usescope_))) return r;

  if ((r = vm_init_compile_op(vm,"rresolve", "resolve dup iscode [\\rresolve map] if"))) return r;
  return r;
}

err_t _op_hget(vm_t *vm) {
  __op_get2; //hash word
  throw_if(ERR_BADARGS,!val_ishash(p) || !(val_isstring(p+1)||val_isident(p+1)));

  val_t *def;
  throw_if(ERR_UNDEFINED,!(def = hash_getv(p->val.hash->hash,p+1)));
  val_destroy(p+1);
  return val_clone(p+1,def);
}

err_t _op_hhas(vm_t *vm) {
  __op_get2; //hash word
  throw_if(ERR_BADARGS,!val_ishash(p) || !(val_isstring(p+1)||val_isident(p+1)));

  val_int_replace(p+1, (NULL != hash_getv(p->val.hash->hash,p+1)));
  return 0;
}

//err_t _op_hset(vm_t *vm) {
//  return -1;
//}

err_t _op_defined(vm_t *vm) {
  __op_get1; //word
  throw_if(ERR_BADARGS,!(val_isstring(p)||val_isident(p)));
  val_int_replace(p+1, (NULL != vm_dict_get(vm,p)));
  return 0;
}

err_t _op_getdef(vm_t *vm) {
  __op_get1; //word
  throw_if(ERR_BADARGS,!(val_isstring(p)||val_isident(p)));

  val_t *def;
  throw_if(ERR_UNDEFINED,!(def = vm_dict_get(vm,p)));
  val_destroy(p);
  return val_clone(p,def);
}

err_t _op_def(vm_t *vm) {
  __op_pop2; //def word
  return vm_dict_put(vm,p+1,p);
}
err_t _op_mapdef(vm_t *vm) {
  __op_popget; //word [body]
  throw_if(ERR_BADARGS,!(val_isstring(p)||val_isident(p)));
  val_t *def;
  if (!(def = vm_dict_get(vm,p))) return _throw(ERR_UNDEFINED);
  __op_wpush3;


  val_func_init(q,_op_def,"def"); //last is to redefine word
  val_move(q+1,p); //word in q+1
  val_null_init(p);
  val_swap(p,def); //def swapped out to p (with NULL)
  val_move(q+2,p+1); //wtop = eval body
  return val_wprotect(q+1);
}

err_t _op_resolve(vm_t *vm) {
  __op_get1;
  return vm_resolve_ident(vm,p);
}

err_t _op_scope(vm_t *vm) {
  __op_pop1;
  __op_wpush2;
  val_move(q+1,p);
  val_func_init(q,_op_endscope,"_endscope");
  return vm_new_scope(vm);
}

err_t _op_savescope(vm_t *vm) {
  __op_pop1;
  __op_wpush2;
  val_move(q+1,p);
  val_func_init(q,_op_popscope,"_popscope");
  return vm_new_scope(vm);
}

err_t _op_endscope(vm_t *vm) {
  return vm_pop_scope(vm,NULL);
}

err_t _op_usescope(vm_t *vm) {
  __op_pop2;
  __op_wpush2;
  val_move(q+1,p);
  val_func_init(q,_op_popscope,"_popscope");
  return vm_push_scope(vm,p);
}

err_t _op_usescope_(vm_t *vm) {
  __op_pop2;
  __op_wpush2;
  val_move(q+1,p);
  val_func_init(q,_op_endscope,"_endscope");
  return vm_push_scope(vm,p);
}

err_t _op_popscope(vm_t *vm) {
  __op_push1;
  return vm_pop_scope(vm,p);
}

