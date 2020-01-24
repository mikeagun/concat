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

#include "val_vm.h"
#include "vm_err.h"
#include "val_printf.h"
#include "val_num.h"
#include "val_string.h"
#include "val_hash.h"
#include "vmstate_internal.h"

int val_vm_finished(val_t *val) {
  err_t r;
  if (ERR_LOCKED == (r=_vm_trylock(val->val.vm))) return 0;
  else if (r) return 0;
  else if (vm_wempty(val->val.vm)) r=1;
  _vm_unlock(val->val.vm);
  return r;
}

err_t val_vm_init(val_t *val) {
  val->type = TYPE_VM;
  fatal_if(ERR_FATAL,!(val->val.vm = (vm_t*)malloc(sizeof(vm_t))));
  return _vmstate_init(val->val.vm);
}
err_t val_vm_init2(val_t *val,val_t *stack, val_t *work) {
  val->type = TYPE_VM;
  fatal_if(ERR_FATAL,!(val->val.vm = (vm_t*)malloc(sizeof(vm_t))));
  return _vmstate_init2(val->val.vm,stack,work);
}
err_t val_vm_init3(val_t *val,val_t *stack, val_t *work, val_t *dict) {
  val->type = TYPE_VM;
  err_t r;
  fatal_if(ERR_FATAL,!(val->val.vm = (vm_t*)malloc(sizeof(vm_t))));
  if ((r = _vmstate_init3(val->val.vm,stack,work,dict))) return r;
  return 0;
}
err_t val_vm_init_(val_t *val,vm_t *vm) {
  val->type = TYPE_VM;
  val->val.vm = vm;
  return 0;
}
err_t val_vm_clonedict(val_t *val,vm_t *vmsrc) {
  err_t r;
  val_destroy(&val->val.vm->dict);
  if ((r = val_clone(&val->val.vm->dict,&vmsrc->dict))) {
    val_list_init(&val->val.vm->dict);
  }
  val->val.vm->scope = val_dict_h(&val->val.vm->dict);
  return r;
}

err_t val_vm_clone(val_t *ret, val_t *orig) {
  vm_t *vm;
  fatal_if(ERR_FATAL,!(vm = (vm_t*)malloc(sizeof(vm_t))));
  err_t r;
  if ((r = _vmstate_clone(vm,orig->val.vm))) goto vm_clone_bad;

  ret->type = TYPE_VM;
  ret->val.vm = vm;
  return 0;

vm_clone_bad:
  free(ret->val.vm);
  return r;
}
err_t val_vm_destroy(val_t *val) {
  _vmstate_destroy(val->val.vm);
  free(val->val.vm);
  return 0;
}
err_t val_vm_finalize(val_t *val, err_t e) { //destroys vm and replaces with either output stack or val to throw
  //don't need to interrupt since val-vm already should have
  if (e == ERR_THROW || e == ERR_USER_THROW) { //throw top of stack
    if (vm_empty(val->val.vm)) {
      val_destroy(val);
      val_int_init(val,_throw(ERR_EMPTY));
    } else {
      val_t t;
      vm_pop(val->val.vm,&t);
      val_destroy(val);
      val_move(val,&t);
    }
    return e;
  } else if (e) { //else exception in e
    val_destroy(val);
    val_int_init(val,e);
    return ERR_THROW;
  } else { //no error, replace val with stack
    val_t t;
    val_list_init(&t);
    val_swap(&t,&val->val.vm->stack);
    val_destroy(val);
    val_move(val,&t);
    return 0;
  }
}

int val_vm_fprintf(val_t *val,FILE *file, const fmt_t *fmt) {
  err_t r;
  val_t buf;
  val_string_init(&buf);
  if (0>(r = val_vm_sprintf(val,&buf,fmt))) goto out;
  r = val_fprintf_(&buf,file,fmt_v);
out:
  val_destroy(&buf);
  return r;
}

int val_vm_sprintf(val_t *val, val_t *buf, const fmt_t *fmt) {
  //TODO: use fmt to set up sprintf calls
  err_t r;
  vm_t *vm = val->val.vm;
  if (ERR_LOCKED == (r = _vm_trylock(vm))) {
    return val_sprint_cstr(buf,"vm(LOCKED)");
  } else if (!r) {
    int ret=0;
    if (0>(r = val_sprint_cstr(buf,"vm("))) goto bad; ret += r;
    if (0>(r = vmstate_sprintf(vm,buf,fmt))) goto bad; ret += r;
    if (0>(r = val_sprint_cstr(buf,")"))) goto bad; ret += r;
bad:
    _vm_unlock(vm);
  }
  return r;
}

void val_vm_init_handlers(struct type_handlers *h) {
  h->fprintf = val_vm_fprintf;
  h->sprintf = val_vm_sprintf;
  h->destroy = val_vm_destroy;
  h->clone = val_vm_clone;
}

