//Copyright (C) 2024 D. Michael Agun
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

#ifndef __VAL_OP_H__
#define __VAL_OP_H__ 1
#include "val.h"
#include "val_printf_fmt.h"
int val_op_fprintf(int op,FILE *file, const struct printf_fmt *fmt);
int val_op_sprintf(int op,valstruct_t *buf, const struct printf_fmt *fmt);


#endif
