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

#ifndef _VAL_PRINTF_H_
#define _VAL_PRINTF_H_ 1

#include "val_printf_fmt.h"
#include "val.h"
#include "vm_err.h"



/////new interface

void fmt_clear(fmt_t *fmt);
void fmt_fprint(FILE *file,const fmt_t *fmt); //for debugging

int isflag(char c);

//functions for printing vals
err_t val_printv(val_t val); //adds nl
err_t val_printV(val_t val); //adds nl
err_t val_printv_(val_t val);
err_t val_printV_(val_t val);

int val_fprintf_(val_t val, FILE *file, const fmt_t *fmt);
int val_sprintf_(val_t val, valstruct_t *buf, const fmt_t *fmt);


//formatted print functions
//print char array (returns proper error code)
int val_fprint_(FILE *file, const char *str, unsigned int len);
int val_fprint_cstr(FILE *file, const char *str);
int val_fprint_ch(FILE *file, char c);
int val_fprint_ptr(FILE *file, void *p);
int val_sprint_(valstruct_t *buf, const char *str, unsigned int len);
int val_sprint_cstr(valstruct_t *buf, const char *str);
int val_sprint_ch(valstruct_t *buf, char c);
//int val_sprint_hex(valstruct_t *buf, unsigned char c);
int val_sprint_ptr(valstruct_t *buf, void *ptr);


//c vararg printf functions (most useful for debugging) -- vals to print should be type val_t
int val_printf(const char *format, ...);
int val_fprintf(FILE *file, const char *format, ...);
int val_sprintf(valstruct_t *buf, const char *format, ...);
int val_vfprintf(FILE *file, const char *format, va_list args);
int val_vsprintf(valstruct_t *buf, const char *format, va_list args);

//formated print functions using concat lists/stacks for arguments
int val_printfv(valstruct_t *format, valstruct_t *args, int isstack);
int val_printfv_(const char *format, int len, valstruct_t *args, int isstack);

int val_fprintfv(FILE *file, valstruct_t *format, valstruct_t *v_args, int v_isstack, valstruct_t *f_args, int f_isstack);
int val_sprintfv(valstruct_t *buf, valstruct_t *format, valstruct_t *v_args, int v_isstack, valstruct_t *f_args, int f_isstack);

int val_fprintfv_(FILE *file, const char *format, int len, valstruct_t *v_args, int v_isstack, valstruct_t *f_args, int f_isstack);
int val_sprintfv_(valstruct_t *buf, const char *format, int len, valstruct_t *v_args, int v_isstack, valstruct_t *f_args, int f_isstack);



//padding helpers
int _val_fprintf_padding(FILE *file, const fmt_t *fmt, int len, int right);
int _val_sprintf_padding(valstruct_t *buf, const fmt_t *fmt, int len, int right);

//printf format parsing/argument handling helpers
int _val_printf_parse( const char **format, int *len, const char **str, fmt_t *fmt, int *vali, int *precision_arg, int *field_width_arg);
err_t _val_printf_fmt_takeargs(fmt_t *fmt, int precision_arg, int field_width_arg, valstruct_t *args, int isstack);
err_t _val_printf_takeval(valstruct_t *args, int isstack, int vali, val_t **val);
int _val_printf_special(fmt_t *fmt, int vali, valstruct_t *args, int isstack, int rlen, int *prevn);




//functions for printing truncated values (with ommissions marked)
int val_print_truncated(val_t *val, int min_field_width, int max_chars, int left_align);
int val_sprintf_truncated_(val_t *val, val_t *buf, const fmt_t *fmt, int min_field_width, int max_chars, int left_align);
int val_fprintf_truncated_(val_t *val, FILE *file, const fmt_t *fmt, int min_field_width, int max_chars, int left_align);


#endif
