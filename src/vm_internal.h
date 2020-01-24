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

#ifndef __VM_INTERNAL_H__
#define __VM_INTERNAL_H__ 1
#include "vmstate.h"
#include "parser.h"

int _vm_qeval(vm_t *vm, val_t *ident, err_t *ret); //pre-evaluate val (handles tokens that bypass noeval)

parse_handler_t vm_eval_handler;
parse_handler_t vm_save_input_handler;
parse_handler_t vm_save_code_handler;

struct _vm_compile_code_state {
  val_t *root_list;
  val_t *open_list;
  unsigned int groupi;
  int opt;
  vm_t *vm;
};

#endif
