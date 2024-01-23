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

#ifndef __VM_INTERNAL_H__
#define __VM_INTERNAL_H__ 1
#include "vm.h"
#include "parser.h"

int _vm_sprintf(valstruct_t *buf, const fmt_t *fmt, val_t *stack, unsigned int stackn, val_t *work, unsigned int workn, val_t *cont, unsigned int contn, unsigned int topn, ...);

int _vm_qeval(vm_t *vm, valstruct_t *ident, int *ret); //pre-evaluate val (handles tokens that bypass noeval)
void _vm_fix_open_list(vm_t *vm);
err_t _vm_open_list(vm_t *vm);
err_t _vm_open_code(vm_t *vm);
err_t _vm_close_list(vm_t *vm);
err_t _vm_close_code(vm_t *vm);

err_t _vm_join(vm_t *vm);
err_t _vm_cancel(vm_t *vm);
err_t _vm_lock(vm_t *vm);
err_t _vm_trylock(vm_t *vm);
err_t _vm_unlock(vm_t *vm);

#endif
