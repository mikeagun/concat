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

#ifndef __VAL_STRING_H__
#define __VAL_STRING_H__ 1
#include "val.h"

#include <stdint.h> //for hashes


void val_string_init_handlers(struct type_handlers *h);

void val_string_init(val_t *val);
err_t val_string_init_(val_t *val, const char *str, unsigned int len);
err_t val_string_init_cstr(val_t *val, const char *str);
err_t val_string_init_dquoted(val_t *val, const char *str, unsigned int len);
err_t val_string_init_squoted(val_t *val, const char *str, unsigned int len);

err_t val_string_destroy(val_t *string);
err_t val_string_clone(val_t *val, val_t *orig);
int val_string_fprintf(val_t *string, FILE *file, const fmt_t *fmt);
int val_string_sprintf(val_t *string, val_t *buf, const fmt_t *fmt);

void val_string_clear(val_t *string);
unsigned int val_string_len(val_t *string);
int val_string_empty(val_t *string);
int val_string_small(val_t *string);

err_t val_string_deref(val_t *string);
err_t val_string_lpushc_(val_t *list, char c);
err_t val_string_rpushc_(val_t *list, char c);
err_t val_string_lpop(val_t *string, val_t *el);
err_t val_string_rpop(val_t *string, val_t *el);

err_t val_string_cat(val_t *string, val_t *rhs);
err_t val_string_lcat(val_t *string, val_t *lhs);
err_t val_string_cat_(val_t *string, const char *rhs, unsigned int len);
err_t val_string_lcat_(val_t *string, const char *lhs, unsigned int len);
err_t val_string_cat_copy(val_t *string, val_t *rhs);
err_t val_string_lcat_copy(val_t *string, val_t *lhs);

err_t val_string_substring(val_t *string, unsigned int off, int len);
err_t val_string_rsplitn(val_t *string, val_t *rhs, unsigned int i);
err_t val_string_lsplitn(val_t *string, val_t *lhs, unsigned int i);
err_t val_string_unwrap(val_t *string);

err_t val_string_ith(val_t *string, unsigned int i);
err_t val_string_dith(val_t *string, unsigned int i, val_t *el);

err_t val_string_seti(val_t *string, unsigned int i, char c);

err_t val_string_split(val_t *string);
err_t val_string_split2_(val_t *string, const char *splitters, unsigned int nsplitters);
err_t val_string_join(val_t *list);
err_t val_string_join2_(val_t *list, const char *sep, unsigned int seplen);

void val_string_trim(val_t *string); //trim leading/trailing whitespace
err_t val_string_padleft(val_t *string,char c, int n); //pad left with n chars of c
err_t val_string_padright(val_t *string,char c, int n); //pad right with n chars of c

err_t val_string_find_(val_t *string, const char *substr, unsigned int substrn);
err_t val_string_rfind_(val_t *string, const char *substr, unsigned int substrn);
err_t val_string_firstof_(val_t *string, const char *accept, unsigned int naccept);
err_t val_string_lastof_(val_t *string, const char *accept, unsigned int naccept);
err_t val_string_firstnotof_(val_t *string, const char *reject, unsigned int nreject);
err_t val_string_lastnotof_(val_t *string, const char *reject, unsigned int nreject);

err_t val_string_lreserve(val_t *string, unsigned int n);
err_t val_string_rreserve(val_t *string, unsigned int n);
err_t val_string_lextend(val_t *string, unsigned int n, char **p);
err_t val_string_rextend(val_t *string, unsigned int n, char **p);

//whether any/all chars are not '\0'
int val_string_any(val_t *string);
int val_string_all(val_t *string);

//string comparison functions
int val_string_compare(val_t *lhs, val_t *rhs);
int val_string_lt(val_t *lhs, val_t *rhs);
int val_string_eq(val_t *lhs, val_t *rhs);

int val_string_strcmp(val_t *string, const char *cstr);

uint32_t val_string_hash32(val_t *str);
uint64_t val_string_hash64(val_t *str);

const char* val_string_str(val_t *string);
const char* val_string_get(val_t *string, unsigned int i);
const char* val_string_rget(val_t *string, unsigned int i);
const char* val_string_cstr(val_t *string); //appends '\0' before returning str



//ident functions
void val_ident_init_handlers(struct type_handlers *h);
void val_ident_to_string(val_t *ident);
err_t val_ident_init_(val_t *val, const char *ident, unsigned int len);
int val_ident_fprintf(val_t *ident, FILE *file, const fmt_t *fmt);
int val_ident_sprintf(val_t *ident, val_t *buf, const fmt_t *fmt);
err_t val_ident_escape(val_t *ident);
err_t val_ident_unescape(val_t *ident);
int val_ident_escaped(val_t *ident);
unsigned int val_ident_escaped_levels(val_t *ident);
#endif
