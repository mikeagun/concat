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

#include "val_vm.h"
#include "vm_err.h"
#include "vm_internal.h"
#include "val_printf.h"
#include "val_dict.h"
#include "val_list.h"
#include "val_string.h"
//#include "val_num.h"

#include <valgrind/helgrind.h>

//TODO: better and more consistent handling of vm run state (e.g. atomic op instead of lock)
//  - maybe make all vm objects vm vals, then main() just instantiates a vm val and evals it
//TODO: clean up location of ANNOTATE_HAPPENS_BEFORE/AFTER/FORGET_ALL calls
//  - right now these are all in vm.c, if we move more of the vm state handling here then they will go here

int val_vm_finished(valstruct_t *vm) {
  err_t r;
  if (ERR_LOCKED == (r=_vm_trylock(vm->v.vm))) return 0;
  else if (r) return 0;
  else if (vm_finished(vm->v.vm)) r=1;
  _vm_unlock(vm->v.vm);
  return r;
}

valstruct_t* _val_vm_alloc() {
  valstruct_t *v = _valstruct_alloc();
  if(!(v->v.vm = (vm_t*)malloc(sizeof(vm_t)))) {
    _valstruct_release(v);
    return NULL;
  }
  v->type = TYPE_VM;
  return v;
}

void _val_vm_free(valstruct_t *vm) {
  free(vm->v.vm);
  _valstruct_release(vm);
}

err_t val_vm_init(val_t *val) {
  valstruct_t *v;
  if (!(v = _val_vm_alloc())) {
    return _throw(ERR_MALLOC);
  }
  err_t e;
  if ((e = vm_init(v->v.vm))) {
    _val_vm_free(v);
    return e;
  }
  *val = __vm_val(v);
  return 0;
}
err_t val_vm_init2(val_t *val,valstruct_t *stack, valstruct_t *work) {
  valstruct_t *v;
  if (!(v = _val_vm_alloc())) {
    return _throw(ERR_MALLOC);
  }
  err_t e;
  if ((e = vm_init2(v->v.vm,stack,work))) {
    _val_vm_free(v);
    return e;
  }
  *val = __vm_val(v);
  return 0;
}
err_t val_vm_init3(val_t *val,valstruct_t *stack, valstruct_t *work, valstruct_t *dict) {
  valstruct_t *v;
  if (!(v = _val_vm_alloc())) {
    return _throw(ERR_MALLOC);
  }
  err_t e;
  if ((e = vm_init3(v->v.vm,stack,work,dict))) {
    _val_vm_free(v);
    return e;
  }
  *val = __vm_val(v);
  return 0;
}
err_t val_vm_init_(val_t *val,vm_t *vm) {
  valstruct_t *v;
  if (!(v = _valstruct_alloc())) {
    return _throw(ERR_MALLOC);
  }
  v->v.vm = vm;
  *val = __vm_val(v);
  return 0;
}
void val_vm_clonedict(valstruct_t *vmdst,vm_t *vmsrc) {
  _val_dict_destroy_(&vmdst->v.vm->dict);
  _val_dict_clone_(&vmdst->v.vm->dict,&vmsrc->dict);
}

err_t val_vm_clone(val_t *ret, vm_t *orig) {
  valstruct_t *v;
  if (!(v = _val_vm_alloc())) return _throw(ERR_MALLOC);
  vm_clone(v->v.vm,orig);
  *ret = __vm_val(v);
  return 0;
}
void _val_vm_destroy(valstruct_t *vm) {
  //TODO: for debugchecks, make sure vm is unlocked (FIXME: when we pop a vm, make sure it is cancelled)
  vm_destroy(vm->v.vm);
  _val_vm_free(vm);
}

inline err_t _val_vm_runthread(valstruct_t *vm) {
  return vm_runthread(vm->v.vm);
}

err_t val_vm_eval_final(val_t *ret, valstruct_t *vm) {
  err_t e;
  if (ERR_LOCKED == (e = _vm_trylock(vm->v.vm))) {
    e = vm_wait(vm->v.vm);
  } else {
    ANNOTATE_HAPPENS_AFTER(vm->v.vm);
    e = vm_dowork(vm->v.vm);
  }
  return val_vm_finalize(ret,vm,e);
}
err_t val_vm_finalize(val_t *ret, valstruct_t *vm, err_t e) { //destroys vm and replaces with either output stack or val to throw
  //don't need to interrupt since val-vm already should have
  if (e == ERR_THROW || e == ERR_USER_THROW) { //throw top of stack
    if (vm_empty(vm->v.vm)) {
      *ret = __int_val(_throw(ERR_EMPTY));
    } else {
      vm_pop(vm->v.vm,ret);
    }
  } else if (e) { //else exception in e
    *ret = __int_val(e);
    return ERR_THROW;
  } else { //no error, replace val with stack
    *ret = val_empty_list();
    valstruct_t *tv = __lst_ptr(*ret);
    //we steal stack without adding and removing reference
    *tv = vm->v.vm->stack;
    vm->v.vm->stack.v.lst.len = 0;
    vm->v.vm->stack.v.lst.buf = NULL;
  }
  _val_vm_destroy(vm);
  return e;
}

int val_vm_fprintf(valstruct_t *vm,FILE *file, const fmt_t *fmt) {
  int r;
  val_t buf = val_empty_string();
  if (0>(r = val_vm_sprintf(vm,__str_ptr(buf),fmt))) goto out;
  r = val_fprintf_(buf,file,fmt_v);
out:
  val_destroy(buf);
  return r;
}

int val_vm_sprintf(valstruct_t *vm, valstruct_t *buf, const fmt_t *fmt) {
  //TODO: use fmt to set up sprintf calls
  int r;
  if (ERR_LOCKED == (r = _vm_trylock(vm->v.vm))) {
    return val_sprint_cstr(buf,"vm(LOCKED)");
  } else if (!r) {
    int rlen=0;
    if (0>(r = val_sprint_cstr(buf,"vm("))) goto bad; rlen += r;
    if (0>(r = vm_sprintf(vm->v.vm,buf,fmt))) goto bad; rlen += r;
    if (0>(r = val_sprint_cstr(buf,")"))) goto bad; rlen += r;
    r = rlen; //if we got here everything went well, copy accumulated return length to r
bad:
    _vm_unlock(vm->v.vm);
  }
  return r;
}

