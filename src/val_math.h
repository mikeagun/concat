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

#ifndef __VAL_MATH_H__
#define __VAL_MATH_H__ 1

#include "val.h"

void _val_dbl_toint32(val_t *a);
void _val_int32_todbl(val_t *a);

err_t val_num_parse(val_t *val, const char *s, unsigned int len);
err_t val_int_parse(val_t *val, const char *s, unsigned int len);
err_t val_double_parse(val_t *val, const char *s, unsigned int len);
int _hfloat_parse(double *val, const char *s, unsigned int len);

//defines several versions of binary op -- one typechecked, and several for if we already know the types and don't need to check
#define _declare_val_num_binop(op) void _val_dbl_##op(val_t *a,val_t b); void _val_int32_##op(val_t *a, val_t b); int val_##op(val_t *a, val_t b); void _val_##op(val_t *a, val_t b);
#define _declare_val_num_unaryop(op) void _val_dbl_##op(val_t *a); void _val_int32_##op(val_t *a); int val_##op(val_t *a); void _val_##op(val_t *a);

_declare_val_num_binop(add)
_declare_val_num_binop(sub)
_declare_val_num_binop(mul)
_declare_val_num_binop(div)
_declare_val_num_unaryop(neg)
_declare_val_num_unaryop(inc)
_declare_val_num_unaryop(dec)

#endif
