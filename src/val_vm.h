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

#ifndef _VAL_VM_H_
#define _VAL_VM_H_ 1

#include "vm.h"

int val_vm_finished(valstruct_t *vm);

valstruct_t* _val_vm_alloc();
void _val_vm_free(valstruct_t *vm);
err_t val_vm_init(val_t *val);
err_t val_vm_init2(val_t *val,valstruct_t *stack, valstruct_t *work);
err_t val_vm_init3(val_t *val,valstruct_t *stack, valstruct_t *work, valstruct_t *dict);
err_t val_vm_init_(val_t *val,vm_t *vm);
void val_vm_clonedict(valstruct_t *vmdst,vm_t *vmsrc);
void _val_vm_destroy(valstruct_t *vm);
err_t val_vm_clone(val_t *ret, vm_t *orig);

err_t _val_vm_runthread(valstruct_t *vm);
err_t val_vm_eval_final(val_t *ret, valstruct_t *vm);
err_t val_vm_finalize(val_t *ret, valstruct_t *vm, err_t e); //destroys vm and replaces with either output stack or val to throw

int val_vm_fprintf(valstruct_t *vm,FILE *file, const fmt_t *fmt);
int val_vm_sprintf(valstruct_t *vm,valstruct_t *buf, const fmt_t *fmt);

#endif
