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

#ifndef __VMSTATE_INTERNAL_H__
#define __VMSTATE_INTERNAL_H__ 1
#include "vmstate.h"

//don't call directly in higher-level code, use vm_init()/vm_destroy() from vm.h instead
err_t _vmstate_init(vm_t *vm);
err_t _vmstate_init2(vm_t *vm, val_t *stack, val_t *work);
err_t _vmstate_init3(vm_t *vm, val_t *stack, val_t *work, val_t *dict);
err_t _vmstate_destroy(vm_t *vm);
err_t _vmstate_clone(vm_t *vm, vm_t *orig);

void _vm_fix_open_list(vm_t *vm); //call this any time we update groupi
void _vm_fix_scope(vm_t *vm); //call this any time we update the dictionary stack

err_t _vm_open_list(vm_t *vm);
err_t _vm_open_code(vm_t *vm);
err_t _vm_close_list(vm_t *vm);
err_t _vm_close_code(vm_t *vm);

err_t _vm_join(vm_t *vm);
err_t _vm_cancel(vm_t *vm);
err_t _vm_lock(vm_t *vm);
err_t _vm_trylock(vm_t *vm);
err_t _vm_unlock(vm_t *vm);

err_t _vm_try(vm_t *vm);
err_t _vm_trydebug(vm_t *vm);
err_t _vm_endtrydebug(vm_t *vm);
err_t _vm_endtry(vm_t *vm);
err_t vm_cpush_debugfallback(vm_t *vm);

//int _vmstate_sprintf(vm_t *vm, val_t *buf, const fmt_t *fmt);
//int _vmstate_fprintf(vm_t *vm, FILE *file, const fmt_t *fmt);

#endif
