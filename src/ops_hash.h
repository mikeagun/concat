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

#ifndef _OPS_HASH_H_
#define _OPS_HASH_H_ 1
#include "vm.h"

//dictionary operators
err_t ops_hash_init(vm_t *vm);

vm_op_handler _op_hget;
vm_op_handler _op_hhas;
vm_op_handler _op_hset;

vm_op_handler _op_defined;  // defined ( A -- bool )
vm_op_handler _op_getdef;  // getdef ( A -- def(A) )
vm_op_handler _op_def;     // def ( A B -- )
vm_op_handler _op_mapdef;  // mapdef ( [A] I -- I getdef A I def )
vm_op_handler _op_resolve;

vm_op_handler _op_scope;     // scope ( [A] -- beginscope A endscope )
vm_op_handler _op_savescope;     // scope ( [A] -- beginscope A endscope )
vm_op_handler _op_endscope; // ends dictionary scope
vm_op_handler _op_usescope;
vm_op_handler _op_usescope_;
vm_op_handler _op_popscope;


#endif
