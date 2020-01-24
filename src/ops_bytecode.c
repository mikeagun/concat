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

#include "ops_bytecode.h"
#include "ops_internal.h"
#include "val_bytecode_internal.h"
#include "vm_bytecode.h"

err_t ops_bytecode_init(vm_t *vm) {
  err_t e=0;
  //bytecode ops
  if ((e = vm_init_op(vm,"compile",_op_compile))) goto out;
  if ((e = vm_init_op(vm,"rcompile",_op_rcompile))) goto out;
  if ((e = vm_init_op(vm,"_tobytecode",_op_tobytecode))) goto out;

out:
  return e;
}

err_t _op_compile(vm_t *vm) {
  __op_get1;

  err_t e;
  val_t t;
  val_bytecode_init(&t);
  if ((e = vm_compile_bytecode(vm,&t,p,0))) goto out;
  val_swap(p,&t);
out:
  val_destroy(&t);
  return e;
}
err_t _op_rcompile(vm_t *vm) {
  __op_get1;

  err_t e;
  val_t t;
  val_bytecode_init(&t);
  if ((e = vm_compile_bytecode(vm,&t,p,1))) goto out;
  val_swap(p,&t);
out:
  val_destroy(&t);
  return e;
}
err_t _op_tobytecode(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_isstring(p));
  p->type = TYPE_BYTECODE;
  return 0;
}
