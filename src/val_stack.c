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

#include "val_stack.h"
#include "val_printf.h"
#include "val_list_internal.h"
#include "vm_err.h"

void val_stack_init(val_t *stack) {
  val_list_init(stack);
}
int val_stack_destroy(val_t *stack) {
  return val_list_destroy(stack);
}

int val_stack_reserve(val_t *stack, unsigned int n) {
  return val_list_rreserve(stack,n);
}

//int val_stack_singleton(val_t *stack) {
//  return val_list_singleton(stack);
//}

int val_stack_check1(val_t *stack, enum valtype t) {
  return val_list_len(stack)>=1 && _val_stack_get(stack,0)->type == t;
}
int val_stack_check2(val_t *stack, enum valtype ta, enum valtype tb) {
  if(val_list_len(stack)<2) return 0;
  val_t *p = _val_list_rget(stack,1);
  return p[0].type == ta && p[1].type == tb;
}
int val_stack_check3(val_t *stack, enum valtype ta, enum valtype tb, enum valtype tc) {
  if(val_list_len(stack)<3) return 0;
  val_t *p = _val_list_rget(stack,2);
  return p[0].type == ta && p[1].type == tb && p[2].type == tc;
}
err_t val_stack_popget_(val_t *stack, val_t **p) {
  argcheck_r(val_list_len(stack)>=2 && val_list_singleton(stack));
  stack->val.list.len--;
  *p = _val_list_get(stack,stack->val.list.len-1);
  return 0;
}
err_t val_stack_pop2get_(val_t *stack, val_t **p) {
  argcheck_r(val_list_len(stack)>=3 && val_list_singleton(stack));
  stack->val.list.len-=2;
  *p = _val_list_get(stack,stack->val.list.len-1);
  return 0;
}
err_t val_stack_popget2_(val_t *stack, val_t **p) {
  argcheck_r(val_list_len(stack)>=3 && val_list_singleton(stack));
  stack->val.list.len--;
  *p = _val_list_get(stack,stack->val.list.len-2);
  return 0;
}
err_t val_stack_get1_(val_t *stack, val_t **p) {
  argcheck_r(val_list_len(stack)>=1 && val_list_singleton(stack));
  *p = _val_stack_get(stack,0);
  return 0;
}
err_t val_stack_get2_(val_t *stack, val_t **p) {
  argcheck_r(val_list_len(stack)>=2 && val_list_singleton(stack));
  *p = _val_stack_get(stack,1);
  return 0;
}
err_t val_stack_get3_(val_t *stack, val_t **p) {
  argcheck_r(val_list_len(stack)>=3 && val_list_singleton(stack));
  *p = _val_stack_get(stack,2);
  return 0;
}
err_t val_stack_getn_(val_t *stack, unsigned int n, val_t **p) {
  argcheck_r(val_list_len(stack)>=n && val_list_singleton(stack));
  *p = _val_stack_get(stack,n-1);
  return 0;
}
err_t val_stack_pop1_(val_t *stack, val_t **p) {
  argcheck_r(val_list_len(stack)>=1 && val_list_singleton(stack));
  stack->val.list.len--;
  *p = _val_list_get(stack,stack->val.list.len);
  return 0;
}
err_t val_stack_pop2_(val_t *stack, val_t **p) {
  argcheck_r(val_list_len(stack)>=2 && val_list_singleton(stack));
  stack->val.list.len-=2;
  *p = _val_list_get(stack,stack->val.list.len);
  return 0;
}
err_t val_stack_pop3_(val_t *stack, val_t **p) {
  argcheck_r(val_list_len(stack)>=2 && val_list_singleton(stack));
  stack->val.list.len-=3;
  *p = _val_list_get(stack,stack->val.list.len);
  return 0;
}
err_t val_stack_popn_(val_t *stack, unsigned int n, val_t **p) {
  argcheck_r(val_list_len(stack)>=n && val_list_singleton(stack));
  stack->val.list.len-=n;
  *p = _val_list_get(stack,stack->val.list.len);
  return 0;
}
err_t val_stack_popngetn_(val_t *stack, unsigned int npop, unsigned int nget, val_t **p) {
  argcheck_r(val_list_len(stack)>=(npop+nget) && val_list_singleton(stack));
  stack->val.list.len-=npop;
  *p = _val_list_get(stack,stack->val.list.len)-nget;
  return 0;
}
void val_stack_unpop1_(val_t *stack) {
  argcheck(_val_list_buf(stack) && _val_list_buf(stack)->size >= val_list_len(stack)+1);
  stack->val.list.len++;
}
void val_stack_unpop2_(val_t *stack) {
  argcheck(_val_list_buf(stack) && _val_list_buf(stack)->size >= val_list_len(stack)+2);
  stack->val.list.len += 2;
}
void val_stack_unpop3_(val_t *stack) {
  argcheck(_val_list_buf(stack) && _val_list_buf(stack)->size >= val_list_len(stack)+3);
  stack->val.list.len += 3;
}

err_t val_stack_extend_(val_t *stack, unsigned int n, val_t **p) {
  return _val_list_rextend(stack,n,p);
}

err_t val_stack_getextend1_(val_t *stack, val_t **p) {
  argcheck_r(val_list_len(stack)>=1);
  err_t e;
  if ((e = _val_list_rextend(stack,1,p))) return e;
  (*p)--;
  return 0;
}
err_t val_stack_getnextendn_(val_t *stack, unsigned int nget, unsigned int npush, val_t **p) {
  argcheck_r(val_list_len(stack)>=nget);
  err_t e;
  if ((e = _val_list_rextend(stack,npush,p))) return e;
  *p -= nget;
  return 0;
}
void val_stack_unpush1_(val_t *stack) {
  argcheck(val_list_len(stack)>=1 && _val_stack_get(stack,0)->type == VAL_INVALID);
  stack->val.list.len--;
}
void val_stack_unpush2_(val_t *stack) {
  argcheck(val_list_len(stack)>=2
      && _val_stack_get(stack,0)->type == VAL_INVALID
      && _val_stack_get(stack,1)->type == VAL_INVALID);
  stack->val.list.len-=2;
}
void val_stack_unpush3_(val_t *stack) {
  argcheck(val_list_len(stack)>=3
      && _val_stack_get(stack,0)->type == VAL_INVALID
      && _val_stack_get(stack,1)->type == VAL_INVALID
      && _val_stack_get(stack,2)->type == VAL_INVALID);
  stack->val.list.len-=3;
}

int val_stack_push(val_t *stack, val_t *el) {
  return val_list_rpush(stack,el);
}
int val_stack_push2(val_t *stack, val_t *a, val_t *b) {
  return val_list_rpush2(stack,a,b);
}
int val_stack_push3(val_t *stack, val_t *a, val_t *b, val_t *c) {
  return val_list_rpush3(stack,a,b,c);
}
int val_stack_pushn(val_t *stack, int n, ...) {
  int r;
  va_list ap;
  va_start(ap, n);
  r = val_list_vrpushn(stack,n,ap);
  va_end(ap);
  return r;
}

int val_stack_rpush(val_t *stack, val_t *el) {
  return val_list_lpush(stack,el);
}

int val_stack_pusheach(val_t *stack, val_t *list) {
  return val_list_cat(stack,list);
}


val_t* val_stack_top(val_t *stack) {
  return val_list_tail(stack);
}
int val_stack_pop(val_t *stack, val_t *top) {
  return val_list_rpop(stack,top);
}
int val_stack_pop2(val_t *stack,val_t *next,val_t *top) {
  throw_if(ERR_BADARGS,val_list_len(stack) < 2);
  int r;
  if ((r = val_list_rpop(stack,top))) return r;
  r = val_list_rpop(stack,next);
  return r;
}
int val_stack_pop3(val_t *stack, val_t *third, val_t *next, val_t *top) {
  throw_if(ERR_BADARGS,val_list_len(stack) < 3);
  int r;
  if ((r = val_list_rpop(stack,top))) return r;
  if ((r = val_list_rpop(stack,next))) return r;
  r = val_list_rpop(stack,third);
  return r;
}
val_t* val_stack_get(val_t *stack, unsigned int i) {
  return val_list_rget(stack,i);
}

val_t* val_stack_rget(val_t *stack, unsigned int i) {
  return val_list_get(stack,i);
}

val_t* _val_stack_get(val_t *stack, unsigned int i) {
  return _val_list_rget(stack,i);
}

val_t* _val_stack_rget(val_t *stack, unsigned int i) {
  return _val_list_get(stack,i);
}


int val_stack_top2(val_t *stack, val_t **top, val_t **next) {
  throw_if(ERR_BADARGS,!val_stack_has(stack,2));
  *top = val_list_rget(stack,0);
  *next = val_list_rget(stack,1);
  return 0;
}
int val_stack_top3(val_t *stack, val_t **top, val_t **next, val_t **third) {
  throw_if(ERR_BADARGS,!val_stack_has(stack,3));
  *top = val_list_rget(stack,0);
  *next = val_list_rget(stack,1);
  *third = val_list_rget(stack,2);
  return 0;
}


err_t val_stack_rlist(val_t *stack) { //print stack from top to bottom
  if (!val_list_empty(stack)) {
    err_t e;
    if (0>(e = val_list_fprintf_(stack,stdout,list_fmt_rlines,fmt_V))) return e;
    if (0>(e = val_fprint_(stdout,"\n",1))) return e;
  }
  return 0;
}
err_t val_stack_list(val_t *stack) { //print stack from bottom to top
  if (!val_list_empty(stack)) {
    err_t e;
    if (0>(e = val_list_fprintf_(stack,stdout,list_fmt_lines,fmt_V))) return e;
    if (0>(e = val_fprint_(stdout,"\n",1))) return e;
  }
  return 0;
}
err_t val_stack_listn(val_t *stack,unsigned int n) { //print top n items of stack from bottom to top
  list_fmt_t lfmt;
  val_list_format_listn(&lfmt,n);
  return val_list_fprintf_(stack,stdout,&lfmt,fmt_V);
}

err_t val_stack_list_open(val_t *stack, unsigned int levels) {
  if (!levels) return val_stack_list(stack);
  return val_list_fprintf_open_(stack,stdout,list_fmt_lines,fmt_V,levels);
  printf(" ");
  return 0;
}

err_t val_stack_listn_open(val_t *stack, unsigned int levels, unsigned int n) {
  if (!levels) return val_stack_listn(stack,n);
  list_fmt_t lfmt;
  val_list_format_listn(&lfmt,n);
  return val_list_fprintf_open_(stack,stdout,&lfmt,fmt_V,levels);
  printf(" ");
  return 0;
}

//int val_stack_printn_open(val_t *stack, unsigned int levels, unsigned int n) {
//  if (!levels) return val_stack_printn(stack,n);
//  unsigned int i,len=val_list_len(stack);
//  if (!len) return 0;
//  if (n > len) n = len;
//
//  for(i=len-n;i+1<len;i++) {
//    val_print_code(_val_list_get(stack,i));
//    printf("\n");
//  }
//  val_list_fprintf_open(_val_list_get(stack,i),stdout,fmt_V,levels);
//  printf(" ");
//  return 0;
//}

void val_stack_clear(val_t *stack) { //clear stack
  val_list_clear(stack);
}

//functions for checking size of stack
int val_stack_empty(val_t *stack) {
  return val_list_empty(stack);
}
int val_stack_has(val_t *stack, unsigned int n) {
  return val_list_len(stack) >= n;
}
unsigned int val_stack_len(val_t *stack) {
  return val_list_len(stack);
}

int val_stack_push_from(val_t *stack, val_t *from) {
  throw_list_empty(from);
  int r;
  if ((r = val_stack_reserve(stack,1))) return r;
  stack->val.list.len++;
  r = val_stack_pop(from,_val_stack_get(stack,0));
  return r;
}

int val_stack_push_from_list(val_t *stack, val_t *from) {
  throw_list_empty(from);
  int r;
  if ((r = val_stack_reserve(stack,1))) return r;
  stack->val.list.len++;
  r = val_list_lpop(from,_val_stack_get(stack,0));
  return r;
}

int val_stack_deref(val_t *stack) {
  return val_list_deref(stack);
}

int val_stack_swap(val_t *stack) {
  throw_if(ERR_BADARGS,!val_stack_has(stack,2));
  int r;
  if ((r = val_stack_deref(stack))) return r;
  val_swap(_val_stack_get(stack,0),_val_stack_get(stack,1));
  return 0;
}

int val_stack_dign(val_t *stack, unsigned int n) {
  return val_list_dign(stack,n);
}

int val_stack_buryn(val_t *stack, unsigned int n) {
  return val_list_buryn(stack,n);
}

int val_stack_flipn(val_t *stack, unsigned int n) {
  return val_list_flipn(stack,n);
}

val_t* val_stacklist_get(val_t *list, int isstack, int i) {
  if (isstack) return val_stack_get(list,i);
  else return val_list_get(list,i);
}
int val_stacklist_pop(val_t *list, int isstack, val_t *el) {
  if (isstack) return val_stack_pop(list,el);
  else return val_list_lpop(list,el);
}

