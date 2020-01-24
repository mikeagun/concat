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

#ifndef __OPS_VM_H__
#define __OPS_VM_H__ 1
#include "vmstate.h"

err_t ops_vm_init(vm_t *vm);

//create VM val
vm_op_handler _op_vm;      // vm ( (A...) (B...) -- vm{ A <|> B } )
vm_op_handler _op_thread;  // vm ( (A...) (B...) -- vm{ A <|> B } ) //starts vm in new thread
vm_op_handler _op_thread_wait;  // thread.wait ( {vm} -- {vm} ) //wait for thread to finish
vm_op_handler _op_vm_state; // vm.state ( {vm} -- {vm} ) //prints vm state

vm_op_handler _op_vm_mapstack; //{vm: 
#endif
