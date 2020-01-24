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

#ifndef _VAL_VM_H_
#define _VAL_VM_H_ 1

#include "vmstate.h"

int val_vm_finished(val_t *val);

err_t val_vm_init(val_t *val);
err_t val_vm_init2(val_t *val,val_t *stack, val_t *work);
err_t val_vm_init3(val_t *val,val_t *stack, val_t *work, val_t *dict);
err_t val_vm_init_(val_t *val,vm_t *vm);
err_t val_vm_clonedict(val_t *val,vm_t *vmsrc);
err_t val_vm_destroy(val_t *val);
err_t val_vm_clone(val_t *ret, val_t *orig);

err_t val_vm_finalize(val_t *val, err_t e); //destroys vm and replaces with either output stack or val to throw

int val_vm_fprintf(val_t *val,FILE *file, const fmt_t *fmt);
int val_vm_sprintf(val_t *val,val_t *buf, const fmt_t *fmt);

#endif
