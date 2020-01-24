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

#include "ops_vm.h"
#include "ops.h"
#include "vm_err.h"
#include "val_hash.h"
#include "val_string.h"
#include "val_coll.h"
#include "val_vm.h"
#include "ops_internal.h"

err_t ops_vm_init(vm_t *vm) {
  err_t r=0;
  if ((r = vm_init_op(vm,"vm",_op_vm))) return r;
  if ((r = vm_init_op(vm,"thread",_op_thread))) return r;
  if ((r = vm_init_op(vm,"thread.wait",_op_thread_wait))) return r;
  if ((r = vm_init_op(vm,"vm.state",_op_vm_state))) return r;

  return r;
}

err_t _op_vm(vm_t *vm) {
  __op_popget;

  val_t vmval,dict;
  err_t e;
  if ((e = val_clone(&dict,&vm->dict))) return e;
  if ((e = val_vm_init3(&vmval,p,p+1,&dict))) goto bad_dict;
  val_move(p,&vmval);
  return 0;

bad_dict:
  val_destroy(&dict);
  return e;
}
err_t _op_thread(vm_t *vm) {
  __op_popget;

  val_t vmval,dict;
  err_t e;
  if ((e = val_clone(&dict,&vm->dict))) return e;
  if ((e = val_vm_init3(&vmval,p,p+1,&dict))) goto bad_dict;
  val_move(p,&vmval);
  if ((e = vm_thread(p->val.vm))) return e;
  return 0;
bad_dict:
  val_destroy(&dict);
  return e;
}

err_t _op_thread_wait(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_isvm(p));
  return vm_wait(p->val.vm);
}

err_t _op_vm_state(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_isvm(p));
  return vm_qstate(p->val.vm);
}
