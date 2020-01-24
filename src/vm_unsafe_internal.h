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

#ifndef _VM_UNSAFE_INTERNAL_H_
#define _VM_UNSAFE_INTERNAL_H_ 1

#include "vm_unsafe.h"

vm_op_handler _op_unsafe;

vm_op_handler _op_unsafe_pop;
vm_op_handler _op_unsafe_dup;
vm_op_handler _op_unsafe_swap;

vm_op_handler _op_unsafe_inc;
vm_op_handler _op_unsafe_dec;
vm_op_handler _op_unsafe_add;

val_t *getv_unsafe(val_t *key);
val_t *get_unsafe(const char *key);
err_t def_unsafe(const char *key, val_t *val);
err_t defv_unsafe(val_t *key, val_t *val);

err_t init_unsafe_op_(const char *word, val_t *val);
err_t init_unsafe_op(const char *word, const char *name, vm_op_handler *op);
err_t init_unsafe_op_keep(const char *word, const char *name, vm_op_handler *op);
err_t init_unsafe_compile_op(vm_t *vm,const char *word, const char *code);

#endif
