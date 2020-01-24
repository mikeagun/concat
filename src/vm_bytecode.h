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

#ifndef __VM_BYTECODE_H__
#define __VM_BYTECODE_H__ 1
#include "vm.h"
#include "vm_err.h"

err_t _vm_dobytecode(vm_t *vm);
err_t _vm_rpush_bytecode(vm_t *vm, val_t *buf, val_t *val, int rec, int clone);
err_t vm_compile_bytecode(vm_t *vm, val_t *buf, val_t *val, int rec);


#endif
