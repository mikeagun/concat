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

#ifndef __VAL_STRING_H__
#define __VAL_STRING_H__ 1

#include "val.h"

#include <string.h>

//gcc allows initialization of flexible array members, but c standard doesn't (i.e. .p below)
//#define CONST_STRING(name,string) sbuf_t name##_buf = { .size = sizeof(string)-1, .refcount = 1, .p = string }; valstruct_t name##_ = { .type = TYPE_STRING, .v.str = { .off = 0, .len = NONE, .buf = &name##_buf } };
//CONST_STRING(val_const_concat,"concat");
//sbuf_t val_const_concat_buf = { .size = sizeof("concat")-1, .refcount = 1, .p = "concat" };
//valstruct_t val_const_concat_ = { .type = TYPE_STRING, .v.str = { .off = 0, .len = NONE, .buf = &val_const_concat_buf } };

int _val_str_empty(valstruct_t *str);
int _val_str_small(valstruct_t *str);

char* _val_str_buf(valstruct_t *v);
char* _val_str_begin(valstruct_t *v);
char* _val_str_off(valstruct_t *v, int i);
char* _val_str_end(valstruct_t *v);
unsigned int _val_str_len(valstruct_t *v);
unsigned int _val_str_size(valstruct_t *v);

int _val_str_escaped(valstruct_t *str); //whether char char of string is '\'
unsigned int _val_str_escaped_levels(valstruct_t *str); //how many leading '\' symbols are there
void _val_str_unescape(valstruct_t *str); //delete first char from string -- ASSUMES non-empty
err_t _val_str_escape(valstruct_t *str); //insert '\' at start of string (i.e. to escape ident)

int _val_str_simpleprint(valstruct_t *v);
int _val_str_fprint_quoted(valstruct_t *v, FILE *file,int precision);
int _val_str_sprint_quoted(valstruct_t *v, valstruct_t *buf, int precision);

val_t _strval_alloc(enum val_type type);
err_t _strval_init(val_t *v, enum val_type type);
err_t val_string_init_cstr(val_t *val, const char *str, unsigned int n);
err_t val_ident_init_cstr(val_t *val, const char *str, unsigned int n);
err_t val_string_init_quoted(val_t *val, const char *str, unsigned int len);
val_t val_string_temp_cstr(const char *str, unsigned int n);

sbuf_t* _sbuf_alloc(unsigned int size);
void _sbuf_release(sbuf_t *buf);

void _val_str_clone(valstruct_t *ret, valstruct_t *str);
void _val_str_destroy(valstruct_t *str);
void _val_str_destroy_(valstruct_t *str);

val_t val_empty_string();
val_t val_empty_ident();

err_t val_string_init_empty(val_t *v);
err_t val_ident_init_empty(val_t *v);

err_t _val_str_lreserve(valstruct_t *v, unsigned int n);
err_t _val_str_rreserve(valstruct_t *v, unsigned int n);
err_t _val_str_lextend(valstruct_t *v, unsigned int n, char **p);
err_t _val_str_rextend(valstruct_t *v, unsigned int n, char **p);

//TODO: reconsider rcat name -- this seems opposite from other names like rpush...
err_t _val_str_cat(valstruct_t *str, valstruct_t *suffix);
err_t _val_str_cat_cstr(valstruct_t *str, const char *s, unsigned int len);
err_t _val_str_rcat_cstr(valstruct_t *str, const char *s, unsigned int len);
err_t _val_str_cat_ch(valstruct_t *str, char c);
err_t _val_str_rcat_ch(valstruct_t *str, char c);
err_t _val_str_cat_copy(valstruct_t *str, valstruct_t *suffix);
err_t _val_str_rcat(valstruct_t *str, valstruct_t *prefix);
err_t _val_str_rcat_copy(valstruct_t *str, valstruct_t *prefix);

//adds a null after end-of-string (if not already there) so we can use as cstr
err_t _val_str_make_cstr(valstruct_t *str);

void _val_str_clear(valstruct_t *str);
void _val_str_substr(valstruct_t *str, unsigned int off, unsigned int len);
void _val_str_trim(valstruct_t *str);
err_t _val_str_splitn(valstruct_t *str, val_t *rhs, unsigned int off);
//unsafe cloned substring (doesn't do bounds checking)
err_t _val_str_substr_clone(val_t *ret, valstruct_t *str, unsigned int off, unsigned int len);

int _val_str_compare(valstruct_t *lhs, valstruct_t *rhs);
int _val_str_lt(valstruct_t *lhs, valstruct_t *rhs);
int _val_str_eq(valstruct_t *lhs, valstruct_t *rhs);
int _val_str_cstr_compare(valstruct_t *str, const char *cstr,unsigned int len);
int _val_str_strcmp(valstruct_t *str, const char *cstr);

int _val_str_find(valstruct_t *str, const char *substr, unsigned int len);
int _val_str_findstr(valstruct_t *str, valstruct_t *substr);

int _val_str_padright(valstruct_t *str, char c, int n);
int _val_str_padleft(valstruct_t *str, char c, int n);

uint32_t _val_str_hash32(valstruct_t *str);
uint64_t _val_str_hash64(valstruct_t *str);
uint32_t _val_cstr_hash32(const char *s, unsigned int n);
uint64_t _val_cstr_hash64(const char *s, unsigned int n);


int val_string_fprintf(valstruct_t *v,FILE *file, const struct printf_fmt *fmt);
int val_string_sprintf(valstruct_t *v,valstruct_t *buf, const struct printf_fmt *fmt);
int val_ident_fprintf(valstruct_t *v,FILE *file, const struct printf_fmt *fmt);
int val_ident_sprintf(valstruct_t *v,valstruct_t *buf, const struct printf_fmt *fmt);

#endif
