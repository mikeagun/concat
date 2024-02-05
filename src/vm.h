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

#ifndef __VM_H__
#define __VM_H__ 1
#include "val.h"
#include "pthread.h"

// TODO: support these (and/or update stat list)
struct vm_stats {
  unsigned int steps;
  unsigned int lookups;
  unsigned int max_stack;
  unsigned int max_work;
}; 


// vm_t - state needed for a running vm (plus thread and lock)
// - stacks - stack, work, dict, and cont valstruct
// - parser - currently just global default vm_parser
// - nested list/code tracking
// - thread, lock, and thread state
// - stats (for debugging)
// - debug_val_eval flag (if DEBUG_VAL_EVAL defined)
//
typedef struct _vm_t {
  //TODO: consider overheads of making vm val members valstructs vs val_t
  valstruct_t stack; //main stack (the actual open-stack is based on groupi below)
  valstruct_t *open_list; //open list object for current stack
  valstruct_t work; //work stack
  valstruct_t cont; //continuation stack (exception handlers)
  valstruct_t dict; //dictionary stack (top is current scope)

  struct parser_rules *p; //currently this is just the global vm_parser - TODO: just use global or add concat support
  unsigned int noeval; //whether vm is currently in no-eval mode - parsing quotation, just copy everything except code close ']'
  unsigned int groupi;

  pthread_t thread; //thread for parallel evaluation
  int threadid; //used for printing threadid in state

  sem_t lock; //lock for thread
  enum state_enum { //thread state
    STOPPED, //no thread attached
    RUNNING, //currently running (and locked)
    FINISHED //was running, now just waiting for join
  } state;

  struct vm_stats stats;
#ifdef DEBUG_VAL_EVAL
  int debug_val_eval;
#endif
} vm_t;

// concat initialization to call once (run-time initialization of concat)
err_t concat_init();



int vm_init(vm_t *vm);
err_t vm_init2(vm_t *vm, valstruct_t *stack, valstruct_t *work);
err_t vm_init3(vm_t *vm, valstruct_t *stack, valstruct_t *work, valstruct_t *dict);
void vm_clone(vm_t *vm, vm_t *orig);
void vm_destroy(vm_t *vm);

// vm_validate - validates 
err_t vm_validate(vm_t *vm);

int vm_parse_input(vm_t *vm, const char *str, int len, valstruct_t *code);
int vm_parse_code(vm_t *vm, const char *str, int len, valstruct_t *code);

int vm_list(vm_t *vm);
int vm_listn(vm_t *vm, unsigned int n);
err_t vm_stackline(vm_t *vm);
err_t vm_printstate(vm_t *vm);
int vm_qstate(vm_t *vm);
int vm_vstate(vm_t *vm);
int vm_fprintf(vm_t *vm, FILE *file, const fmt_t *fmt);
int vm_sprintf(vm_t *vm, valstruct_t *buf, const fmt_t *fmt);

err_t vm_wait(vm_t *vm); //wait for running VM to finish (and join thread)

int vm_noeval(vm_t *vm);

int vm_dict_put_compile(vm_t *vm, const char *opname, const char *code_str);
int vm_dict_put_(vm_t *vm, const char *key, val_t val);
int vm_dict_put(vm_t *vm, valstruct_t *key, val_t val);
val_t vm_dict_get(vm_t *vm, valstruct_t *key);

err_t vm_val_rresolve(vm_t *vm, val_t *val);
err_t vm_val_resolve(vm_t *vm, val_t *val);

int vm_empty(vm_t *vm);
err_t vm_push(vm_t *vm, val_t val);
err_t vm_wpush(vm_t *vm, val_t val);
err_t vm_cpush(vm_t *vm, val_t val);
err_t vm_wappend(vm_t *vm, val_t val);
err_t vm_pop(vm_t *vm, val_t *val);
err_t vm_drop(vm_t *vm);
val_t vm_top(vm_t *vm);
err_t vm_trycatch(vm_t *vm, val_t tryval, val_t catchval); //prep vm for trycatch

int vm_hascont(vm_t *vm);
err_t vm_pushcw(vm_t *vm);

int vm_stack_printf(vm_t *vm, const char *format);
int vm_stack_printf_(vm_t *vm, const char *format, int len);

int vm_finished(vm_t *vm);
err_t vm_runthread(vm_t *vm);

err_t vm_dowork(vm_t *vm);

#endif
