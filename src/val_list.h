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

#ifndef __VAL_LIST_H__
#define __VAL_LIST_H__ 1
#include "val.h"

#include <stdint.h> //for hashes

//used for formatting of list printing
//  reverse: reverses indexing of elements (preint reverse order)
//  max_bytes: maximum number of chars to print
//    - -1 for unlimited
//    - prints as normal if print length <= max_bytes
//    - truncates contents and inserts trunc if limit is passed
//    - truncates tail of list if trunc_tail, head of list otherwise
//    - min truncated print length is trunclen if !braces, and if braces then openlen+braceseplen+trunclen+closelen
//    - useful for fitting to columns or keeping debug output reasonable lengths
//  max_els: maximum number of els to print
//    - -1 for unlimited
//    - prints as normal if list len <= max_els
//    - truncates contents of list at max_els - 1 and inserts trunc as last el if limit is passed
//    - useful for e.g. printing at most 5 lines of output -- the whole stack/list if len<=5, or 4 lines + "..."
//  - can handle formatting sublists at different depths differently
//    - if sublist_fmt != NULL, sublists formatted using sublist_fmt
//    - if sublist_fmt is NULL, sublists formatted using el_fmt
//    - sublist_fmt can point at self to format all lists the same
//TODO: write the exhaustive/sufficient test cases to confirm this all works
//TODO: after a while, review whether there should be more/fewer options
typedef struct list_fmt_ {
  int reverse;
  int max_bytes;
  int max_els;
  int trunc_tail;

  const char *sep;
  int seplen;
  const char *trunc;
  int trunclen;

  int braces;
  const char *bracesep;
  int braceseplen;

  const struct list_fmt_ *sublist_fmt;
} list_fmt_t;

//standard list formats
extern const list_fmt_t *list_fmt_V; //standard print/print_code format
extern const list_fmt_t *list_fmt_lines; //print 1 el per line, no braces
extern const list_fmt_t *list_fmt_rlines; //print 1 el per line in reverse order, no braces
extern const list_fmt_t *list_fmt_join; //print els with no sep or braces (e.g. cat strings)

err_t val_list_format_fmt(list_fmt_t *lfmt, const fmt_t *fmt);
void val_list_format_listn(list_fmt_t *lfmt, int n);
void val_list_format_join2(list_fmt_t *lfmt, const char *sep, unsigned int seplen);

void val_list_init_handlers(struct type_handlers *h);

void val_list_init(val_t *val);

err_t val_list_destroy(val_t *list);
err_t val_list_clone(val_t *val, val_t *orig);
int val_list_fprintf(val_t *list, FILE *file, const fmt_t *fmt);
int val_list_sprintf(val_t *list, val_t *buf, const fmt_t *fmt);
int val_list_fprintf_(val_t *val, FILE *file, const list_fmt_t *lfmt, const fmt_t *el_fmt);
int val_list_sprintf_(val_t *val, val_t *buf, const list_fmt_t *lfmt, const fmt_t *el_fmt);
int val_list_fprintf_open_(val_t *list, FILE *file, const list_fmt_t *lfmt, const fmt_t *el_fmt, unsigned int levels);
int val_list_sprintf_open_(val_t *val, val_t *buf, const list_fmt_t *lfmt, const fmt_t *el_fmt, unsigned int levels);

void val_list_clear(val_t *list);
unsigned int val_list_len(val_t *list);
int val_list_empty(val_t *list);
int val_list_small(val_t *list);
int val_list_singleton(val_t *list);

err_t val_list_deref(val_t *list);
err_t val_list_lpush(val_t *list, val_t *el);
err_t val_list_rpush(val_t *list, val_t *el);
err_t val_list_rpush2(val_t *list, val_t *a, val_t *b);
err_t val_list_rpush3(val_t *list, val_t *a, val_t *b, val_t *c); //c is top
err_t val_list_vrpushn(val_t *list, int n, va_list ap);
err_t val_list_rpushn(val_t *list, int n, ...);
err_t val_list_append(val_t *list, val_t *val);
err_t val_list_lpop(val_t *list, val_t *el);
err_t val_list_rpop(val_t *list, val_t *el);
err_t val_list_rpop2(val_t *list, val_t *a, val_t *b);
err_t val_list_rpop3(val_t *list, val_t *a, val_t *b, val_t *c); //c is top
err_t val_list_vrpopn_(val_t *list, int n, va_list ap); //doesn't support destroying on NULL arg
err_t val_list_rpopn_(val_t *list, int n, ...); //doesn't support destroying on NULL arg

err_t val_list_cat(val_t *list, val_t *rhs);
err_t val_list_lcat(val_t *list, val_t *lhs);

err_t val_list_sublist(val_t *list, unsigned int i, int len);
err_t val_list_rsplitn(val_t *list, val_t *rhs, unsigned int i);
err_t val_list_lsplitn(val_t *list, val_t *lhs, unsigned int i);

err_t val_list_wrap(val_t *val);
err_t val_list_wrap2(val_t *dest, val_t *a, val_t *b);
err_t val_list_wrap3(val_t *dest, val_t *a, val_t *b, val_t *c); //c is top
err_t val_list_wrap_arr(val_t *dest, val_t *p, unsigned int n);
err_t val_list_clone_wrap_arr(val_t *dest, val_t *p, unsigned int n);
err_t val_list_wrapn(val_t *dest, int n, ...);
err_t val_list_unwrap(val_t *list);

err_t val_list_ith(val_t *list, unsigned int i);
err_t val_list_dith(val_t *list, unsigned int i, val_t *el);
err_t val_list_seti(val_t *list, unsigned int i, val_t *val);
err_t val_list_swapi(val_t *list, unsigned int i, val_t *val);

err_t val_list_dign(val_t *list, unsigned int n);
err_t val_list_buryn(val_t *list, unsigned int n);
err_t val_list_flipn(val_t *list, unsigned int n);

//int val_list_find(val_t *list, val_t *el);
//int val_list_rfind(val_t *list, val_t *el);
//int val_list_findp(val_t *list, int(pred)(val_t* el));
//int val_list_rfindp(val_t *list, int(pred)(val_t* el));

err_t val_list_lreserve(val_t *list, unsigned int n);
err_t val_list_rreserve(val_t *list, unsigned int n);

//uint32_t val_list_hash32(val_t *str);
//uint64_t val_list_hash64(val_t *str);

//whether any/all values are true values
int val_list_any(val_t *list);
int val_list_all(val_t *list);

//list comparison functions
int val_list_compare(val_t *lhs, val_t *rhs);
int val_list_lt(val_t *lhs, val_t *rhs);
int val_list_eq(val_t *lhs, val_t *rhs);

err_t val_list_qsort(val_t *list);
err_t val_list_rqsort(val_t *list);

val_t* val_list_head(val_t *list);
val_t* val_list_tail(val_t *list);
val_t* val_list_get(val_t *list, unsigned int i);
val_t* val_list_rget(val_t *list, unsigned int i);


err_t val_list_lextend(val_t *list, unsigned int n, char **p); //FIXME: internal
err_t val_list_rextend(val_t *list, unsigned int n, char **p);

//code functions
void val_code_init_handlers(struct type_handlers *h);
void val_code_init(val_t *val);
err_t val_code_wrap(val_t *val);

#endif
