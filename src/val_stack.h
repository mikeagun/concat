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

#ifndef __VAL_STACK_H__
#define __VAL_STACK_H__ 1

#include "val_list.h"

#include <stdarg.h>

//stack is just a rpush/rpop - based stack interface for list objects
//  - stack, work, dict, and cont (the 4 places vals can exist in a VM) are all stacks (lists accessed as stacks)

void val_stack_init(val_t *stack);
err_t val_stack_destroy(val_t *stack);
err_t val_stack_reserve(val_t *stack, unsigned int n);

//new interface for checking and getting/popping els
//  - only for singleton stacks (e.g. stack/wstack)
int val_stack_check1(val_t *stack, enum valtype t);
int val_stack_check2(val_t *stack, enum valtype ta, enum valtype tb);
int val_stack_check3(val_t *stack, enum valtype ta, enum valtype tb, enum valtype tc); //c is top
err_t val_stack_popget_(val_t *stack, val_t **p);
err_t val_stack_pop2get_(val_t *stack, val_t **p);
err_t val_stack_popget2_(val_t *stack, val_t **p);
err_t val_stack_get1_(val_t *stack, val_t **p);
err_t val_stack_get2_(val_t *stack, val_t **p);
err_t val_stack_get3_(val_t *stack, val_t **p);
err_t val_stack_getn_(val_t *stack, unsigned int n, val_t **p);
err_t val_stack_pop1_(val_t *stack, val_t **p);
err_t val_stack_pop2_(val_t *stack, val_t **p);
err_t val_stack_pop3_(val_t *stack, val_t **p);
err_t val_stack_popn_(val_t *stack, unsigned int n, val_t **p);
err_t val_stack_popngetn_(val_t *stack, unsigned int npop, unsigned int nget, val_t **p);
void val_stack_unpop1_(val_t *stack);
void val_stack_unpop2_(val_t *stack);
void val_stack_unpop3_(val_t *stack);

err_t val_stack_extend_(val_t *stack, unsigned int n, val_t **p);
err_t val_stack_getextend1_(val_t *stack, val_t **p);
err_t val_stack_getnextendn_(val_t *stack, unsigned int nget, unsigned int npush, val_t **p);
void val_stack_unpush1_(val_t *stack);
void val_stack_unpush2_(val_t *stack);
void val_stack_unpush3_(val_t *stack);

err_t val_stack_push(val_t *stack, val_t *el);
err_t val_stack_push2(val_t *stack, val_t *a, val_t *b);
err_t val_stack_push3(val_t *stack, val_t *a, val_t *b, val_t *c);
err_t val_stack_pushn(val_t *stack, int n, ...);
err_t val_stack_rpush(val_t *stack, val_t *el);
err_t val_stack_pusheach(val_t *stack, val_t *list);


val_t* val_stack_top(val_t *stack);
err_t val_stack_pop(val_t *stack, val_t *top);
err_t val_stack_pop2(val_t *stack,val_t *next,val_t *top);
err_t val_stack_pop3(val_t *stack,val_t *third, val_t *next,val_t *top);
//int val_stack_popd(val_t *stack, val_t *next);
//val_t* val_stack_pop_ret(val_t *stack);
val_t* val_stack_get(val_t *stack, unsigned int i);
val_t* val_stack_rget(val_t *stack, unsigned int i);

val_t* _val_stack_get(val_t *stack, unsigned int i);
val_t* _val_stack_rget(val_t *stack, unsigned int i);

err_t val_stack_top2(val_t *stack, val_t **top, val_t **next);
err_t val_stack_top3(val_t *stack, val_t **top, val_t **next, val_t **third);

err_t val_stack_rlist(val_t *stack); //print stack from top to bottom
err_t val_stack_list(val_t *stack); //print stack from bottom to top
err_t val_stack_listn(val_t *stack, unsigned int n); //print top n items of stack from bottom to top

//print top n items of stack from bottom to top, where the last levels of groups are still open
err_t val_stack_listn_open(val_t *stack,unsigned int levels,unsigned int n);

//print stack from bottom to top, where the last levels of groups are still open
err_t val_stack_list_open(val_t *stack,unsigned int levels);

void val_stack_clear(val_t *stack); //clear stack

//functions for checking size of stack
int val_stack_empty(val_t *stack);
int val_stack_has(val_t *stack, unsigned int n);
unsigned int val_stack_len(val_t *stack);

err_t val_stack_push_from(val_t *stack, val_t *from);
err_t val_stack_push_from_list(val_t *stack, val_t *from);

err_t val_stack_deref(val_t *stack);

err_t val_stack_swap(val_t *stack);

err_t val_stack_dign(val_t *stack, unsigned int n);
err_t val_stack_buryn(val_t *stack, unsigned int n);
err_t val_stack_flipn(val_t *stack, unsigned int n);

val_t* val_stacklist_get(val_t *list, int isstack, int i);
err_t val_stacklist_pop(val_t *list, int isstack, val_t *el);

#endif
