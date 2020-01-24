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

#ifndef __VMSTATE_H__
#define __VMSTATE_H__ 1

#include "val.h"
#include "vm_err.h"
#include <pthread.h>
#include <semaphore.h>

struct vm_stats {
  unsigned int steps;
  unsigned int lookups;
  unsigned int max_stack;
  unsigned int max_work;
}; 

//core struct representing VM state
//  - a VM consists of 4 stacks (stack,work,dict,cont), plus groupi while processing input lists (to handle open/close parens)
//  - for threading there is also a lock and state variable to protect access to VM state when the VM is being managed by another thread
//  - for debugging there is also a stats struct that is updated when stepping
//  - a vm val contains a (singleton) pointer to a vm_state
//  - scope points to the start of the dictionary hashtable chain
//  - open_list points to the currently open stack (this is how we build sublists)
//    - groupi tells us the nesting level so we can fix open_list
//    - noeval tells us whether open_list (or a containing list) is suppressing eval
//      - currently code suppresses eval, list doesn't (unless it is nested inside code)
//      - this lets you build a list while treating that list as the stack
typedef struct vm_state {
  val_t stack; //primary stack
  val_t work;  //work stack (execution continues until work stack is empty)
  val_t cont;  //error continuation stack (exception handler stack)

  val_t dict; //dictionary stack containing scopes
  struct hashtable *scope; //dictionary for word lookup

  unsigned int noeval; //are we in a no-eval group (also the index of the highest-level no-eval group + 1)
  val_t *open_list; //the currently open group, or the stack if there are no open groups
  unsigned int groupi; //how many levels deep we are in nested groups

  struct parser_rules *p;
  struct vm_stats stats; //debugging stats (updated while stepping)
  pthread_t thread;
  int threadid; //used for printing threadid in state

  sem_t lock; //lock for thread
  enum state_enum { //thread state
    STOPPED, //no thread attached
    RUNNING, //currently running (and locked)
    FINISHED //was running, now just waiting for join
  } state;
} vm_t;

void vm_clearstats(vm_t *vm);
unsigned int vm_stacksize(vm_t *vm);
unsigned int vm_wstacksize(vm_t *vm);
int vm_empty(vm_t *vm);
int vm_wempty(vm_t *vm);
err_t vm_reset(vm_t *vm);
int vm_noeval(vm_t *vm);
int vm_hascont(vm_t *vm);
int vm_pushcw(vm_t *vm);

err_t _vm_open_coll(vm_t *vm, val_t *coll);
err_t _vm_open_list(vm_t *vm);
err_t _vm_open_code(vm_t *vm);
err_t _vm_close_list(vm_t *vm);
err_t _vm_close_code(vm_t *vm);
err_t vm_new_scope(vm_t *vm);
err_t vm_pop_scope(vm_t *vm, val_t *hash);
err_t vm_push_scope(vm_t *vm, val_t *hash);

int vm_has(vm_t *vm, unsigned int n);
int vm_check1(vm_t *vm, enum valtype t);
int vm_check2(vm_t *vm, enum valtype ta, enum valtype tb);
int vm_check3(vm_t *vm, enum valtype ta, enum valtype tb, enum valtype tc); //c is top
//these functions form the primary interface to the vm for the ops_internal macros
//  - these should be reviewed, and probably reduced
err_t vm_popget_(vm_t *vm, val_t **p);
err_t vm_pop2get_(vm_t *vm, val_t **p);
err_t vm_popget2_(vm_t *vm, val_t **p);
err_t vm_get1_(vm_t *vm, val_t **a);
err_t vm_get2_(vm_t *vm, val_t **p);
err_t vm_get3_(vm_t *vm, val_t **p);
err_t vm_getn_(vm_t *vm, unsigned int n, val_t **p);
err_t vm_pop1_(vm_t *vm, val_t **p);
err_t vm_pop2_(vm_t *vm, val_t **p);
err_t vm_pop3_(vm_t *vm, val_t **p);
err_t vm_popn_(vm_t *vm, unsigned int n, val_t **p);
err_t vm_popngetn_(vm_t *vm, unsigned int npop, unsigned int nget, val_t **p);
void vm_unpop1_(vm_t *vm);
void vm_unpop2_(vm_t *vm);
void vm_unpop3_(vm_t *vm);
err_t vm_getextend1_(vm_t *vm, val_t **p);
err_t vm_getnextendn_(vm_t *vm, unsigned int nget, unsigned int npush, val_t **p);
void vm_unpush1_(vm_t *vm);
void vm_unpush2_(vm_t *vm);
void vm_unpush3_(vm_t *vm);
err_t vm_extend_(vm_t *vm, unsigned int n, val_t **p);

err_t vm_wextend_(vm_t *vm, unsigned int n, val_t **q);

err_t vm_cextend_(vm_t *vm, unsigned int n, val_t **cp);
err_t vm_cpopn_(vm_t *vm, unsigned int n, val_t **cp);


//dictionary lookup functions
val_t* vm_dict_get_cstr(vm_t *vm, const char *key);
val_t* vm_dict_get_(vm_t *vm, const char *key, unsigned int len);
val_t* vm_dict_get(vm_t *vm, val_t *key);

//dictionary update functions
err_t vm_dict_put(vm_t *vm, val_t *key, val_t *val);
err_t vm_dict_swap(vm_t *vm, val_t *key, val_t *val);
err_t vm_dict_take(vm_t *vm, val_t *key, val_t *val);

err_t vm_push(vm_t *vm, val_t *el);
//err_t vm_push2(vm_t *vm, val_t *a, val_t *b);
//err_t vm_push3(vm_t *vm, val_t *a, val_t *b, val_t *c);
err_t vm_pop(vm_t *vm, val_t *el);

err_t vm_pushsw(vm_t *vm);
err_t vm_wpop(vm_t *vm, val_t *val);
val_t* _vm_getnext(vm_t *vm);
err_t _vm_popnext(vm_t *vm, val_t *val);
err_t vm_splitnextn(vm_t *vm, unsigned int n); //if the next work item is a quotation, split it so it contains at most n items

err_t vm_wgetn_(vm_t *vm, unsigned int n, val_t **q);
err_t vm_wappend(vm_t *vm, val_t *val);
err_t vm_wpush(vm_t *vm, val_t *w);
//err_t vm_wpush2(vm_t *vm, val_t *a, val_t *b);
//err_t vm_wpush3(vm_t *vm, val_t *a, val_t *b, val_t *c);
err_t vm_hold(vm_t *vm, val_t *val);

//exception handling and debugging functions
err_t _vm_try(vm_t *vm);
err_t _vm_trydebug(vm_t *vm);
err_t vm_cpush_debugfallback(vm_t *vm);
err_t vm_set_interactive_debug(vm_t *vm);

err_t vm_wait(vm_t *vm); //wait for running VM to finish (and join thread)

int vm_list(vm_t *vm);
int vm_listn(vm_t *vm, unsigned int n);
err_t vm_printstate(vm_t *vm);
int vm_qstate(vm_t *vm); //short 1-line version of state
int vm_vstate(vm_t *vm); //verbose 1-line version of state

int vmstate_fprintf(vm_t *vm, FILE *file, const fmt_t *fmt);
int vmstate_sprintf(vm_t *vm, val_t *buf, const fmt_t *fmt);

#endif
