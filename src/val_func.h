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

#ifndef __VAL_FUNC_H__
#define __VAL_FUNC_H__ 1
#include "val.h"
#include "vm.h"


//TODO: normalize naming on natives/funcs

//struct vm_state; //define vm_state just so NATIVE_FUNC can take a pointer to the vm
//typedef int (vm_op_handler)(struct vm_state *vm);


//native function type (stores function pointer + name)
//struct val_func {
//  vm_op_handler *f;
//  const char *name;
//};

val_t* val_func_init(val_t *val, int (*func)(struct vm_state *vm), const char *name);
val_t* val_func_init_(val_t *val, int (*func)(struct vm_state *vm), const char *name, unsigned char opcode);
val_t* val_func_init_keep(val_t *val, int (*func)(struct vm_state *vm), const char *name);
val_t* val_func_init_keep_(val_t *val, int (*func)(struct vm_state *vm), const char *name, unsigned char opcode);

vm_op_handler* val_func_f(val_t *f);
const char* val_func_name(val_t *f);
int val_func_keep(val_t *f);
err_t val_func_exec(val_t *f, vm_t *vm);

void val_func_init_handlers(struct type_handlers *h);
int val_func_fprintf(val_t *val, FILE *file, const fmt_t *fmt);
int val_func_sprintf(val_t *val, val_t *buf, const fmt_t *fmt);

#endif
