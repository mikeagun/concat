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

#ifndef __VAL_NUM_H__
#define __VAL_NUM_H__

#include "val.h"


val_t* val_int_init(val_t *val, valint_t i);
void val_int_replace(val_t *val, valint_t i);
val_t* val_float_init(val_t *val, valfloat_t f);

valint_t val_int(val_t *val);
valfloat_t val_float(val_t *val);
void val_int_set(val_t *val, valint_t i);
void val_float_set(val_t *val, valfloat_t f);

int val_num_compare(val_t *lhs, val_t *rhs);
int val_num_lt(val_t *lhs, val_t *rhs);
int val_num_eq(val_t *lhs, val_t *rhs);

err_t val_num_add(val_t *val, val_t *rhs);
err_t val_num_sub(val_t *val, val_t *rhs);
err_t val_num_mul(val_t *val, val_t *rhs);
err_t val_num_div(val_t *val, val_t *rhs);
err_t val_num_mod(val_t *val, val_t *rhs);
err_t val_num_neg(val_t *val);
err_t val_num_abs(val_t *val);
err_t val_num_inc(val_t *val);
err_t val_num_dec(val_t *val);
void val_int_dec_(val_t *val);
err_t val_num_sqrt(val_t *val);
err_t val_num_log(val_t *val);

valint_t val_num_int(val_t *val);
valfloat_t val_num_float(val_t *val);

err_t val_num_toint(val_t *val);
err_t val_num_tofloat(val_t *val);

int val_int_fprintf(val_t *val, FILE *file, const fmt_t *fmt);
int val_int_sprintf(val_t *val, val_t *buf, const fmt_t *fmt);
int val_float_fprintf(val_t *val, FILE *file, const fmt_t *fmt);
int val_float_sprintf(val_t *val, val_t *buf, const fmt_t *fmt);
int _val_int_fprintf(valint_t val, FILE *file, const fmt_t *fmt);
int _val_int_sprintf(valint_t val, val_t *buf, const fmt_t *fmt);
int _val_float_fprintf(valfloat_t val, FILE *file, const fmt_t *fmt);
int _val_float_sprintf(valfloat_t val, val_t *buf, const fmt_t *fmt);
int _val_num_dstring_sprint(val_t *strbuf, char sign, char *buf, int buflen, int off, int digits, int roundto, int extendto, int sepi, int exp, char conv, int flags);

void val_int_init_handlers(struct type_handlers *h);
void val_float_init_handlers(struct type_handlers *h);

int _val_num_parse(val_t *val, const char *s, unsigned int len); //returns -1 on error, not exception
err_t val_num_parse(val_t *val, const char *s, unsigned int len);
err_t val_int_parse(val_t *val, const char *s, unsigned int len);
err_t val_float_parse(val_t *val, const char *s, unsigned int len);
int _hfloat_parse(double *val, const char *s, unsigned int len);
#endif
