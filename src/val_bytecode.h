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

#ifndef __VAL_BYTECODE_H__
#define __VAL_BYTECODE_H__ 1

#include "val.h"


val_t val_empty_bytecode();

err_t val_bytecode_init_empty(val_t *v);

int val_bytecode_fprintf(valstruct_t *v,FILE *file, const struct printf_fmt *fmt);
int val_bytecode_sprintf(valstruct_t *v,valstruct_t *buf, const struct printf_fmt *fmt);

err_t bytecode_rpush(valstruct_t *b, val_t val);


unsigned int _bytecode_write_vbyte32(char *buf, uint32_t v);
unsigned int _bytecode_read_vbyte32(const char *ip, uint32_t *v);


#endif
