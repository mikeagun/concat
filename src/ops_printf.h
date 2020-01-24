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

#include "ops.h"

err_t ops_printf_init(vm_t *vm);

vm_op_handler _op_printf;       // printf ( ... "fmt" -- ) formatted print
vm_op_handler _op_printf_;      // printf ( ... "fmt" -- ) like printf, but skips printing (e.g. for calculating field widths)
vm_op_handler _op_sprintf;      // sprintf ( ... "fmt" -- "formatted_string" ) build formatted string
vm_op_handler _op_printlf;      // printlf ( (args) "fmt" -- (lefotver args) ) formatted print from list
vm_op_handler _op_printlf_;     // printlf_ ( (args) "fmt" -- (lefotver args) ) like printlf_ but skips printing (e.g. for calculating field widths)
vm_op_handler _op_sprintlf;     // sprintlf ( (args) "fmt" -- (leftover args) "formatted_string" ) build formatted string from list
vm_op_handler _op_printlf2;     // printlf2 ( (vargs) (fargs) "fmt" -- (lefotver vargs) (leftover fargs) ) formatted print from list
vm_op_handler _op_printlf2_;    // printlf2 ( (vargs) (fargs) "fmt" -- (lefotver vargs) (leftover fargs) ) like printlf2_ but skips printing (e.g. for calculating field widths)
vm_op_handler _op_sprintlf2;    // sprintlf2 ( (vargs) (fargs) "fmt" -- (leftover vargs) (leftover fargs) "formatted_string" ) build formatted string from list
