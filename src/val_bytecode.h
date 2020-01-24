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

#ifndef __VAL_BYTECODE_H__
#define __VAL_BYTECODE_H__ 1

#include "val.h"
#include "opcode.h"

void val_bytecode_init_handlers(struct type_handlers *h);

int val_bytecode_fprintf(val_t *val,FILE *file, const fmt_t *fmt);
int val_bytecode_sprintf(val_t *val,val_t *buf, const fmt_t *fmt);
void val_bytecode_init(val_t *val);

int _bytecode_rpush_int(val_t *buf, valint_t i);
int _bytecode_rpush_float(val_t *buf, valfloat_t f);
int _bytecode_rpush_string(val_t *buf, opcode_t type, const char *str, unsigned int len);

int _bprintf__byte(val_t *buf, uint8_t v);
int _bprintf__opcode(val_t *buf, opcode_t op);
int _bsprintf__vbyte32(char *buf, uint32_t v);







////======== Old Interface ========
//void val_bytecode_init_handlers(struct type_handlers *h);
//
//int val_bytecode_fprintf(val_t *val,FILE *file, const fmt_t *fmt);
//int val_bytecode_sprintf(val_t *val,val_t *buf, const fmt_t *fmt);
//void val_bytecode_val_to_string(val_t *val);
//
//void val_bytecode_init(val_t *val);
//err_t val_bytecode_init_cstr(val_t *val, const char *str);
//err_t val_bytecode_init_(val_t *val, char *str, unsigned int len, unsigned int size);
//
//err_t val_bytecode_rpush(val_t *code, val_t *val);
//err_t val_bytecode_lpop(val_t *code, val_t *val);
//err_t val_bytecode_cat(val_t *code, val_t *rhs);
#endif
