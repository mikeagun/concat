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

#ifndef __VM_H__
#define __VM_H__ 1
#include "vmstate.h"

#define OPT_NONE (0)
#define OPT_DEFAULT (0)
#define OPT_NATIVEFUNC (1)

int vm_init(vm_t *vm);
int vm_destroy(vm_t *vm);

int vm_thread(vm_t *vm);

int vm_dowork(vm_t *vm, int max_steps); //run any pending work in this vm, but only up to max_steps evals


int vm_step(vm_t *vm, int steps, unsigned int min_wstack); //run work with various limits and statistics tracking

int vm_eval(vm_t *vm, val_t *val); //evaluate a val (e.g. this is called on every input token-val of a script file)

int vm_eval_code(vm_t *vm, const char *str, int len);
int vm_compile_input(vm_t *vm, const char *str, int len, val_t *code);
int vm_compile_code(vm_t *vm, const char *str, int len, val_t *code, int opt);

int vm_resolve_ident(vm_t *vm, val_t *ident);

int _vm_eval_final(val_t *vm);

#endif
