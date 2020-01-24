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

#ifndef __OPCODE_H__
#define __OPCODE_H__ 1
#include <inttypes.h>


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


typedef unsigned char opcode_t;

//TODO: sort out single-macro-list version
#define OPCODES(opcode) \
    opcode(end),  opcode(break),   opcode(native),   opcode(keep_native), \
    opcode(zero), opcode(one), opcode(int8),   opcode(int64),   opcode(double), \
    opcode(qstring),   opcode(string),   opcode(ident),   opcode(bytecode), \
    opcode(empty_list),   opcode(empty_code), opcode(list), opcode(code), \
    opcode(pop),   opcode(swap),   opcode(dup), \
    opcode(empty),   opcode(lpop),   opcode(lpush),   opcode(rpop),   opcode(rpush),   opcode(first),   opcode(ith), \
    opcode(add),   opcode(sub),   opcode(inc),   opcode(dec),   opcode(print), \
    opcode(eq), opcode(ne), opcode(lt), opcode(le), opcode(gt), opcode(ge), \
    opcode(eval),  opcode(if),   opcode(if_),   opcode(ifelse),   opcode(ifelse_),   opcode(only),   opcode(unless)

//These opcodes are the core bytecodes which the bytecode interpreter uses
#define OP_ENUM(opcode) OP_##opcode
enum opcode { OPCODES(OP_ENUM), NUM_OPCODES };
#undef OP_ENUM

//This is for the set of labels matching the opcode enum for token-threaded-bytecode
#define OP_LABEL(opcode) &&op_##opcode
#define OPTABLE OPCODES(OP_LABEL)


#endif
