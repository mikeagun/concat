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

#ifndef __VAL_BYTECODE_INTERNAL_H__
#define __VAL_BYTECODE_INTERNAL_H__ 1
#include "val_bytecode.h"
#include "opcode.h"
#include <inttypes.h>


//typedef unsigned char opcode_t;
//
//enum opcode   {   OP_end=0,  OP_break,   OP_native,   OP_keep_native,
//    OP_int8,   OP_int64,   OP_double,   OP_qstring,   OP_string,   OP_ident,   OP_bytecode,   OP_empty_list,   OP_empty_code, OP_list, OP_code,
//    OP_pop,   OP_swap,   OP_dup,
//    OP_empty,   OP_lpop,   OP_lpush,   OP_rpop,   OP_rpush,   OP_first,   OP_ith,
//    OP_add,   OP_sub,   OP_inc,   OP_dec,   OP_print,
//    OP_eq, OP_ne, OP_lt, OP_le, OP_gt, OP_ge,
//    OP_eval,  OP_if,   OP_if_,   OP_ifelse,   OP_ifelse_,   OP_only,   OP_unless,
//    NUM_OPCODES
//};
//
//#define OPTABLE &&op_end, &&op_break, &&op_native, &&op_keep_native, \
//  &&op_int8, &&op_int64, &&op_double, &&op_qstring, &&op_string, &&op_ident, &&op_bytecode, &&op_empty_list, &&op_empty_code, &&op_list, &&op_code, \
//  &&op_pop, &&op_swap, &&op_dup, \
//  &&op_empty, &&op_lpop, &&op_lpush, &&op_rpop, &&op_rpush, &&op_first, &&op_ith, \
//  &&op_add, &&op_sub, &&op_inc, &&op_dec, &&op_print, \
//  &&op_eq, &&op_ne, &&op_lt, &&op_le, &&op_gt, &&op_ge, \
//  &&op_eval, &&op_if, &&op_if_, &&op_ifelse, &&op_ifelse_, &&op_only, &&op_unless



const unsigned char* _val_bytecode_string(val_t *val, val_t *code, const unsigned char *ip, unsigned int len);
const unsigned char* _val_bytecode_bytecode(val_t *val, val_t *code, const unsigned char *ip, unsigned int len);
const unsigned char* _val_bytecode_ident(val_t *val, val_t *code, const unsigned char *ip, unsigned int len);

int val_bytecode_canpush(val_t *val);


err_t val_bytecode_rpush_opcode(val_t *code, opcode_t op);
err_t _val_bytecode_rpush_native(val_t *code, val_t *native);
err_t val_bytecode_rpush_native(val_t *code, vm_op_handler *op, int keep);
err_t _val_bytecode_rpush_int(val_t *code, val_t *val);
err_t _val_bytecode_rpush_float(val_t *code, val_t *val);
//err_t val_bytecode_rpush_string(val_t *code, const char *str, unsigned int len);
//err_t val_bytecode_rpush_ident(val_t *code, const char *str, unsigned int len);
//err_t val_bytecode_rpush_bytecode(val_t *code, const char *str, unsigned int len);
err_t _val_bytecode_rpush_stringtype(val_t *code, val_t *val);
//err_t _val_bytecode_rpush_list(val_t *code, val_t *val);
//err_t _val_bytecode_rpush_code(val_t *code, val_t *val);
err_t _val_bytecode_rpush_listtype(val_t *code, val_t *val);
const unsigned char* _val_bytecode_parse_listtype(unsigned char *ip, val_t *code, val_t *val);
const unsigned char* _val_bytecode_parse_listcontents(const unsigned char *ip, val_t *code, val_t *val);

const unsigned char* _bytecode_read_op2native(const unsigned char *ip, val_t *val, int keep);
const unsigned char* _bytecode_read_op(const unsigned char *ip, vm_op_handler **op);

err_t val_bytecode_rpush_ith(val_t *code, int i);

err_t _val_bytecode_rpush_char(val_t *code, char c);
err_t _val_bytecode_rpush_int32(val_t *code, int32_t i);

err_t _val_bytecode_rpush_vbyte32(val_t *code, uint32_t v);
const unsigned char* _bytecode_read_vbyte32(const unsigned char *ip, uint32_t *v);
unsigned char* _bytecode_write_vbyte32(unsigned char *ip, uint32_t v);

#endif
