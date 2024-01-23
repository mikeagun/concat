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

#ifndef __VAL_NUM_INTERNAL_H__
#define __VAL_NUM_INTERNAL_H__ 1

#include "val_num.h"

//number printf helpers
//determine printf sign char (if any)
char _val_num_signchar(int isneg, int flags);
//whether an ascii digit string is all zeroes
int _val_num_string_zero(const char *n,int len);
//round digit string using nearest-half-even rounding (does nothing if len<=trunc). return value: carry from the left-most digit
int _val_num_dstring_round_nhe(char *n, int len, int trunc);
//round digit string using nearest-half-odd rounding. Useful to shorten intermediates since nhe(nho(x,n),m<(n-1)) rounds like correctly like nhe(x,m)
int _val_num_dstring_round_nho(char *n, int len, int trunc);
//format string of digits for printing
int _val_num_dstring_format(char *buf, int buflen, int *off, int *digits, int roundto, int extendto, int *sepi, int *exp, int *extraoff, int *extralen, int *expoff, int *explen, char conv, int flags);
//fprint formatted string of digits
int _val_num_dstring_fprint(FILE *file, char sign, char *buf, int buflen, int off, int digits, int roundto, int extendto, int sepi, int exp, char conv, int flags);
//sprint formatted string of digits
int _val_num_dstring_sprint(valstruct_t *strbuf, char sign, char *buf, int buflen, int off, int digits, int roundto, int extendto, int sepi, int exp, char conv, int flags);

//integer printf functions
//int _val_long_fprintf(long val, FILE *file, const struct printf_fmt *fmt);
int _val_long_sprintf(long val, valstruct_t *buf, const struct printf_fmt *fmt);
//double-float printf functions
int _val_double_fprintf(double val, FILE *file, const struct printf_fmt *fmt);
int _val_double_sprintf(double val, valstruct_t *buf, const struct printf_fmt *fmt);
//convert double to string of decimal digits
void _val_double_to_decimal(double val, char *buf, int buflen, int *offset, int *len, int *dp);
//convert double to string of hex digits
int _val_double_to_hex(double val, char *buf, int buflen, char conv, int flags, int precision);

#endif
