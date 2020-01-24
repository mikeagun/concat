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

#ifndef __OPS_FILE_H__
#define __OPS_FILE_H__ 1
#include "vmstate.h"

err_t ops_file_init(vm_t *vm);

vm_op_handler _op_include;

vm_op_handler _op_openr;
vm_op_handler _op_open;
vm_op_handler _op_stdin;
vm_op_handler _op_stdout;
vm_op_handler _op_stderr;
vm_op_handler _op_readline;
vm_op_handler _op_stdin_readline;
vm_op_handler _op_read;
vm_op_handler _op_writeline;
vm_op_handler _op_write;

vm_op_handler _op_eachline;

#endif
