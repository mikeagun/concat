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

#ifndef __OPS_BYTECODE_H__
#define __OPS_BYTECODE_H__ 1

#include "ops.h"

err_t ops_bytecode_init(vm_t *vm);
vm_op_handler _op_compile;
vm_op_handler _op_rcompile;
vm_op_handler _op_tobytecode;


#endif
