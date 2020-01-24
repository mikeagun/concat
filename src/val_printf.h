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

#ifndef _VM_PRINTF_H_
#define _VM_PRINTF_H_ 1

#include "val_printf_fmt.h"
#include "val_stack.h"



/////new interface

void fmt_clear(fmt_t *fmt);
void fmt_fprint(FILE *file,const fmt_t *fmt); //for debugging

int isflag(char c);

//functions for printing vals
int val_printv(val_t *val); //adds nl
int val_printV(val_t *val); //adds nl
int val_printv_(val_t *val);
int val_printV_(val_t *val);
int val_peekV(val_t *val); //adds nl

int val_fprintf_(val_t *val, FILE *file, const fmt_t *fmt);
int val_sprintf_(val_t *val, val_t *buf, const fmt_t *fmt);


//formatted print functions

//c vararg printf functions (most useful for debugging)
int val_fprintf(FILE *file, const char *format, ...);
int val_sprintf(val_t *buf, const char *format, ...);

//formated print functions using concat lists/stacks for arguments
int val_printfv(val_t *format, val_t *args, int isstack);
int val_printfv_(const char *format, int len, val_t *args, int isstack);

int val_fprintfv(FILE *file, val_t *format, val_t *v_args, int v_isstack, val_t *f_args, int f_isstack);
int val_sprintfv(val_t *buf, val_t *format, val_t *v_args, int v_isstack, val_t *f_args, int f_isstack);

int val_fprintfv_(FILE *file, const char *format, int len, val_t *v_args, int v_isstack, val_t *f_args, int f_isstack);
int val_sprintfv_(val_t *buf, const char *format, int len, val_t *v_args, int v_isstack, val_t *f_args, int f_isstack);



//print char array (returns proper error code)
int val_fprint_(FILE *file, const char *str, unsigned int len);
int val_fprint_cstr(FILE *file, const char *str);

int val_sprint_(val_t *buf, const char *str, unsigned int len);
int val_sprint_cstr(val_t *buf, const char *str);

//padding helpers
int _val_fprintf_padding(FILE *file, const fmt_t *fmt, int len, int right);
int _val_sprintf_padding(val_t *buf, const fmt_t *fmt, int len, int right);

//printf format parsing/argument handling helpers
int _val_printf_parse( const char **format, int *len, const char **str, fmt_t *fmt, int *vali, int *precision_arg, int *field_width_arg);
err_t _val_printf_fmt_takeargs(fmt_t *fmt, int precision_arg, int field_width_arg, val_t *args, int isstack);
err_t _val_printf_takeval(val_t *args, int isstack, int vali, val_t **val);
int _val_printf_special(fmt_t *fmt, int vali, val_t *args, int isstack, int rlen, int *prevn);




//functions for printing truncated values (with ommissions marked)
int val_print_truncated(val_t *val, int min_field_width, int max_chars, int left_align);
int val_sprintf_truncated_(val_t *val, val_t *buf, const fmt_t *fmt, int min_field_width, int max_chars, int left_align);
int val_fprintf_truncated_(val_t *val, FILE *file, const fmt_t *fmt, int min_field_width, int max_chars, int left_align);



/////old interface

//int val_printf(val_t *args, int isstack, const char *format, int len);
//int val_fprintf(FILE* file, val_t *v_args, int v_isstack, val_t *f_args, int f_isstack, const char *format, int len);
//int val_sprintf(val_t *buf, val_t *v_args, int v_isstack, val_t *f_args, int f_isstack, val_t *format);
//
//int val_fprintf_pad(FILE *file, const fmt_t *fmt, int len);
//int val_sprintf_pad(val_t *buf, const fmt_t *fmt, int len);
//int val_sprintf_padleft(val_t *buf, const fmt_t *fmt, int len);
//
//void val_printf_clear(fmt_t *fmt);
//int val_printf_parsespec(val_t *args, int isstack, fmt_t *fmt, const char *spec, int len, int *vali);
//
//int val_sprint_(val_t *buf, const char *str, unsigned int len);
//int val_sprint_cstr(val_t *buf, const char *str);
//
//int val_basic_fprintf(val_t *val, FILE* file, const fmt_t *fmt);
//int val_basic_sprintf(val_t *val, val_t *buf, const fmt_t *fmt);

#endif
