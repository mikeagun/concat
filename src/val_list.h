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

#ifndef __VAL_LIST_H__
#define __VAL_LIST_H__ 1
// val_list.h - concat list val interface (used for list and code/quotation vals)

#include "val.h"

//minimum allocation size
#define LST_MIN_ALLOC 4

//FIXME: document list val interface

//list_fmt_t - used for formatting of list printing
//  reverse: reverses indexing of elements (print in reverse order)
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
extern const list_fmt_t *list_fmt_V; //standard print/printV format
extern const list_fmt_t *list_fmt_lines; //print 1 el per line, no braces
extern const list_fmt_t *list_fmt_rlines; //print 1 el per line in reverse order, no braces
extern const list_fmt_t *list_fmt_join; //print els with no sep or braces (e.g. cat strings)

int val_list_format_fmt(list_fmt_t *lfmt, const fmt_t *fmt);
void val_list_format_listn(list_fmt_t *lfmt, int n);
void val_list_format_join2(list_fmt_t *lfmt, const char *sep, unsigned int seplen);


int _val_lst_empty(valstruct_t *v);
int _val_lst_small(valstruct_t *v);
val_t* _val_lst_buf(valstruct_t *v);
val_t* _val_lst_begin(valstruct_t *v);
val_t* _val_lst_off(valstruct_t *v, int i);
val_t* _val_lst_end(valstruct_t *v);
val_t* _val_lst_bufend(valstruct_t *v);
unsigned int _val_lst_offset(valstruct_t *v);
unsigned int _val_lst_len(valstruct_t *v);
unsigned int _val_lst_size(valstruct_t *v);



//int val_list_print(val_t val);
//int val_list_print_open(val_t val, unsigned int levels);
//
//int _val_lst_print_contents(valstruct_t *v);
//int _val_lst_sprint_contents(valstruct_t *buf,valstruct_t *v);
//
//int _val_list_print(valstruct_t *v);
//int _val_code_print(valstruct_t *v);
//
//int _val_list_sprint(valstruct_t *buf, valstruct_t *v);
//int _val_code_sprint(valstruct_t *buf, valstruct_t *v);

val_t _lstval_init(enum val_type type);

lbuf_t* _lbuf_alloc(unsigned int size);
void _lst_release(valstruct_t *v);

val_t val_empty_list();
val_t val_empty_code();
void _val_list_init(valstruct_t *v);
void _val_code_init(valstruct_t *v);
void _val_lst_destroy_(valstruct_t *v);
void _val_lst_clear(valstruct_t *v);
void _val_lst_swap(valstruct_t *a, valstruct_t *b);
void _val_lst_clone(valstruct_t *ret, valstruct_t *orig);
//err_t _val_lst_deep_clone(valstruct_t *ret, valstruct_t *orig); //TODO: IMPLEMENTME -- only partially implemented right now

err_t val_list_wrap(val_t *val);
err_t val_code_wrap(val_t *val);

err_t val_list_wrap_arr(val_t *dest, val_t *p, int n);
err_t val_list_wrap_arr_clone(val_t *dest, val_t *p, int n);

err_t val_list_wrap2(val_t *dst, val_t a, val_t b);
err_t val_list_wrap3(val_t *dst, val_t a, val_t b, val_t c);
err_t val_list_wrap3_clone(val_t *dst, val_t a, val_t b, val_t c);
err_t val_list_wrapn(val_t *dst, int n, ...);


err_t val_lst_ith(val_t *lst, unsigned int i); //replace lst with ith element of lst
err_t _val_lst_rdign(valstruct_t *lst, unsigned int n); //dig out nth el from the right and push to end
err_t _val_lst_rburyn(valstruct_t *lst, unsigned int n); //bury rightmost el n in from the right
err_t _val_lst_rflipn(valstruct_t *lst, unsigned int n); //flip n right vals

err_t _val_lst_deref(valstruct_t *lst);
err_t _val_lst_cleanderef(valstruct_t *lst); //deref (or clean if singleref)
err_t _val_lst_realloc(valstruct_t *v, unsigned int left, unsigned int right);
err_t _val_lst_cat(valstruct_t *lst, valstruct_t *suffix);
err_t _val_lst_lpush(valstruct_t *lst, val_t el);
err_t _val_lst_rpush(valstruct_t *lst, val_t el);
err_t _val_lst_rpush2(valstruct_t *lst, val_t a, val_t b);
err_t _val_lst_rpush3(valstruct_t *lst, val_t a, val_t b, val_t c);
err_t _val_lst_vrpushn(valstruct_t *lst, int n, va_list vals);
err_t _val_lst_rpushn(valstruct_t *lst, int n, ...);
err_t _val_lst_rpush_fromr(valstruct_t *lst, valstruct_t *src_lst);
err_t _val_lst_lpop(valstruct_t *lst, val_t *head);
err_t _val_lst_rpop(valstruct_t *lst, val_t *tail);
err_t _val_lst_ldrop(valstruct_t *lst);
err_t _val_lst_rdrop(valstruct_t *lst);

err_t _val_lst_lreserve(valstruct_t *v, unsigned int n);
err_t _val_lst_rreserve(valstruct_t *v, unsigned int n);

err_t _val_lst_lextend(valstruct_t *v, unsigned int n, val_t **p);
err_t _val_lst_rextend(valstruct_t *v, unsigned int n, val_t **p);

void _val_lst_sublst(valstruct_t *lst, unsigned int off, unsigned int len);
err_t _val_lst_splitn(valstruct_t *lst, val_t *rhs, unsigned int off);

//list comparison functions -- compare lists element-wise
int _val_lst_compare(valstruct_t *lhs, valstruct_t *rhs);
int _val_lst_lt(valstruct_t *lhs, valstruct_t *rhs);
int _val_lst_eq(valstruct_t *lhs, valstruct_t *rhs);

//basic list printf
int val_list_fprintf_simple(valstruct_t *v, FILE *file, const struct printf_fmt *fmt);
int val_list_sprintf_simple(valstruct_t *v, valstruct_t *buf, const struct printf_fmt *fmt);

//list printf functions (list_fmt gives extra formatting control, simple versions use list_fmt_V)
int val_list_fprintf(valstruct_t *lst, FILE *file, const fmt_t *fmt);
int val_list_sprintf(valstruct_t *lst, valstruct_t *buf, const fmt_t *fmt);
int val_list_fprintf_(valstruct_t *lst, FILE *file, const list_fmt_t *lfmt, const fmt_t *el_fmt);
int val_list_sprintf_(valstruct_t *lst, valstruct_t *buf, const list_fmt_t *lfmt, const fmt_t *el_fmt);

//functions for printing lists without close braces (e.g. open list on the stack)
int val_list_fprintf_open_(valstruct_t *lst, FILE *file, const list_fmt_t *lfmt, const fmt_t *el_fmt, unsigned int levels);
int val_list_sprintf_open_(valstruct_t *lst, valstruct_t *buf, const list_fmt_t *lfmt, const fmt_t *el_fmt, unsigned int levels);

//functions for printing list-like things (e.g. pointer+len, len+va_list)
int val_listp_fprintf(FILE *file, val_t *p, int len, const list_fmt_t *lfmt, const fmt_t *val_fmt);
int val_listp_sprintf(valstruct_t *buf, val_t *p, int len, const list_fmt_t *lfmt, const fmt_t *val_fmt);
int val_vlist_fprintf(FILE *file, const list_fmt_t *lfmt, const fmt_t *fmt, int len, va_list vals);
int val_vlist_sprintf(valstruct_t *buf, const list_fmt_t *lfmt, const fmt_t *fmt, int len, va_list vals);

//these print a pointer+len list with a va_list appended
//  - these are used mainly for optimized code where we may be holding one or more vals outside the list, but want to print them as if they were in the list
//  - if lfmt->reverse the va_list is printed before, else after the pointer+len list -- in both cases the va_list is printed first-to-last
int val_listp_fprintf_extra(FILE *file, val_t *p, int len, const list_fmt_t *lfmt, const fmt_t *val_fmt, int extran, va_list extra);
int val_listp_sprintf_extra(valstruct_t *buf, val_t *p, int len, const list_fmt_t *lfmt, const fmt_t *val_fmt, int extran, va_list extra);

#endif
