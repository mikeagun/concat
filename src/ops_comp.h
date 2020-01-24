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

#ifndef __OPS_COMP_H__
#define __OPS_COMP_H__ 1
#include "vmstate.h"

err_t ops_comp_init(vm_t *vm);

//comparison operators
vm_op_handler _op_compare; // compare ( A B -- compare(A,B) )
vm_op_handler _op_lt;      // lt ( A B -- A<B )
vm_op_handler _op_le;      // le ( A B -- A<=B )
vm_op_handler _op_gt;      // gt ( A B -- A>B )
vm_op_handler _op_ge;      // ge ( A B -- A>=B )
vm_op_handler _op_eq;      // eq ( A B -- A=B )
vm_op_handler _op_ne;      // ne ( A B -- A!=B )


//boolean operators
//versions without trailing underscore eval args (if needed)
vm_op_handler _op_bool;    // bool ( true -- 1 ) OR ( false -- 0 )
vm_op_handler _op_not;     // not  ( true -- 0 ) OR ( false -- 1 )
vm_op_handler _op_and_;    // and_ (  A   B  -- and(A,B) )
vm_op_handler _op_or_;     // or_  (  A   B  -- or(A,B) )
vm_op_handler _op_and;     // and  ( [A] [B] -- A [[B]] only )
vm_op_handler _op_or;      // or   ( [A] [B] -- A [[B]] unless )

vm_op_handler _op_swaplt;  // swaplt ( 1 2 -- 2 1 ); ( 2 1 -- 2 1 )
vm_op_handler _op_swapgt;  // swapgt ( 1 2 -- 1 2 ); ( 2 1 -- 1 2 )

#endif
