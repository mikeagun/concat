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

#include "vmstate_internal.h"
#include "val_stack.h"
#include "val_hash.h"
#include "val_printf.h"
#include "val_num.h"
#include "val_file.h"
#include "val_string.h"
#include "val_func.h"
#include "vm_parser.h"
#include "vm_debug.h"
#include "ops_internal.h" //helper macros for vmstate
#include <errno.h>


//don't call directly in higher-level code, use vm_init()/vm_destroy() from vm.h instead
err_t _vmstate_init(vm_t *vm) {
  val_t stack; val_list_init(&stack);
  val_t work; val_list_init(&work);
  val_t dict;
  err_t e;
  if ((e = val_dict_init(&dict))) return e;
  return _vmstate_init3(vm,&stack,&work,&dict);
}
err_t _vmstate_init2(vm_t *vm, val_t *stack, val_t *work) {
  val_t dict; val_list_init(&dict);
  return _vmstate_init3(vm,stack,work,&dict);
}
err_t _vmstate_init3(vm_t *vm, val_t *stack, val_t *work, val_t *dict) {
  err_t r;
  //throw_if(BADARGS,!val_isdict(dict)); //this is internal function, so for now we leave this check to caller
  if (!val_islist(stack) && (r = val_list_wrap(stack))) return r; //wrap non-list val for stack
  if (!val_islist(work)) {
    if (val_iscode(work) && val_list_empty(work)) work->type = TYPE_LIST; //empty code becomes empty stack
    else if ((r = val_list_wrap(work))) return r; //wrap non-list work for wstack
  }
  vm->p = NULL;

  vm->groupi=0;
  vm->noeval=0;
  sem_init(&vm->lock,0,1);
  vm->state = STOPPED;
  vm->threadid=-1;

  val_stack_init(&vm->cont);
  val_move(&vm->stack,stack);
  val_move(&vm->work,work);
  val_move(&vm->dict,dict);
  _vm_fix_open_list(vm);
  _vm_fix_scope(vm);
  vm_clearstats(vm);
  return 0;
}
err_t _vmstate_destroy(vm_t *vm) {
  err_t e;
  int mustjoin=0;
  if (ERR_LOCKED == (e = _vm_trylock(vm))) {
    //vm currently running, need to kill
    fatal_if(e,(e = _vm_cancel(vm)));
    mustjoin = 1;
  } else if (e) {
    return _fatal(e);
  } else { //we got lock, wait for thread if needed
    if (vm->state == FINISHED) mustjoin = 1;
  }
  if (mustjoin) {
    e = _vm_join(vm);
  }
  val_stack_destroy(&vm->cont);
  val_stack_destroy(&vm->stack);
  val_stack_destroy(&vm->work);
  val_dict_destroy(&vm->dict);
  if (vm->p) parser_rules_destroy(vm->p); //we use a single shared static parser for all vms

  if (e == ERR_VM_CANCELLED) {
    return 0;
  } else {
    return e;
  }
}
err_t _vmstate_clone(vm_t *vm, vm_t *orig) {
  //FIXME: review vm clone
  err_t r;
  fatal_if(r,(r = _vm_lock(orig)));

  cleanup_throw_if(ERR_MALLOC,_vm_unlock(orig),!(vm = (vm_t*)malloc(sizeof(vm_t))));
  vm->p = NULL;
  vm->groupi=orig->groupi;
  vm->noeval=orig->noeval;
  sem_init(&vm->lock,0,1);
  vm->state = STOPPED;

  if ((r = val_clone(&vm->cont,&orig->cont))) goto bad;
  if ((r = val_clone(&vm->stack,&orig->stack))) goto bad_cont;
  if ((r = val_clone(&vm->work,&orig->work))) goto bad_stack;
  //if ((r = vm_wderef(vm))) goto bad_work; //FIXME
  if ((r = val_clone(&vm->dict,&orig->dict))) goto bad_work;
  _vm_fix_open_list(vm);

  _vm_fix_scope(vm);
  vm->stats.steps = orig->stats.steps;
  vm->stats.lookups = orig->stats.lookups;
  vm->stats.max_stack = orig->stats.max_stack;
  vm->stats.max_work = orig->stats.max_work;
  return _vm_unlock(orig);

//bad_dict:
//  val_destroy(&ret->val.vm->dict);
bad_work:
  val_destroy(&vm->work);
bad_stack:
  val_destroy(&vm->stack);
bad_cont:
  val_destroy(&vm->cont);
bad:
  _vm_unlock(orig);
  return r;
}

void vm_clearstats(vm_t *vm) {
  vm->stats.steps = 0;
  vm->stats.lookups = 0;
  vm->stats.max_stack = vm_stacksize(vm);
  vm->stats.max_work = vm_wstacksize(vm);
}

unsigned int vm_stacksize(vm_t *vm) {
  return val_list_len(vm->open_list);
}
unsigned int vm_wstacksize(vm_t *vm) {
  return val_list_len(&vm->work);
}
int vm_empty(vm_t *vm) {
  return 0 == val_list_len(&vm->stack);
}
int vm_wempty(vm_t *vm) {
  return 0 == val_list_len(&vm->work);
}

err_t vm_reset(vm_t *vm) {
  val_stack_clear(&vm->work);
  val_stack_clear(&vm->cont);
  //NOTE: doesn't clear stack (but does reset groupi)
  int r;
  if ((r = val_dict_pop_to1(&vm->dict))) return r; //TODO: re-init dict
  vm->scope = val_dict_h(&vm->dict);
  
  vm->groupi=0;
  vm->noeval=0;
  vm->open_list = &vm->stack;
  return 0;
}



int vm_noeval(vm_t *vm) {
  return vm->noeval;
}
int vm_hascont(vm_t *vm) {
  return !val_list_empty(&vm->cont);
}
int vm_pushcw(vm_t *vm) {
  err_t e;
  val_t *cp,*q;
  if ((e = vm_cpopn_(vm,1,&cp))) return e;
  if (val_iscode(cp) && val_list_empty(cp)) goto bad_cont;
  if ((e = vm_wextend_(vm,1,&q))) goto bad_cont;
  val_move(q,cp);
  return 0;
bad_cont:
  val_destroy(cp);
  return e;
}

void _vm_fix_open_list(vm_t *vm) {
  unsigned int i;
  vm->open_list = &vm->stack;
  vm->noeval=0;
  for(i=0;i<vm->groupi;++i) {
    vm->open_list = val_stack_top(vm->open_list);
    if (!vm->noeval && val_iscode(vm->open_list)) {
      vm->noeval = i+1;
    }
  }
}
//TODO: clean up open collection handling
err_t _vm_open_coll(vm_t *vm, val_t *coll) {
  err_t r;
  if ((r = val_stack_push(vm->open_list,coll))) { val_destroy(coll); return r; }
  vm->open_list = val_stack_top(vm->open_list);
  vm->groupi++;
  if (!vm->noeval && val_iscode(vm->open_list)) vm->noeval = vm->groupi;
  return 0;
}
err_t _vm_open_list(vm_t *vm) {
  val_t t;
  val_list_init(&t);
  return _vm_open_coll(vm,&t);
}
err_t _vm_open_code(vm_t *vm) {
  val_t t;
  val_code_init(&t);
  return _vm_open_coll(vm,&t);
}
err_t _vm_close_list(vm_t *vm) {
  throw_if(ERR_UNEXPECTED_EOL,!vm->groupi || !val_islist(vm->open_list));
  vm->groupi--;
  _vm_fix_open_list(vm);
  return 0;
}
err_t _vm_close_code(vm_t *vm) {
  throw_if(ERR_UNEXPECTED_EOC,!vm->groupi || !val_iscode(vm->open_list));
  vm->groupi--;
  _vm_fix_open_list(vm);
  return 0;
}

void _vm_fix_scope(vm_t *vm) {
  vm->scope = val_dict_h(&vm->dict);
  //FIXME: also fix dict next pointers, or are they OK???
}
err_t vm_new_scope(vm_t *vm) {
  err_t r;
  if ((r = val_dict_pushnew(&vm->dict))) return _fatal(r);
  vm->scope = val_dict_h(&vm->dict);
  return 0;
}
err_t vm_pop_scope(vm_t *vm, val_t *hash) {
  err_t r;
  if ((r = val_dict_pop(&vm->dict,hash))) return _fatal(r);
  vm->scope = val_dict_h(&vm->dict);
  return 0;
}
err_t vm_push_scope(vm_t *vm, val_t *hash) {
  err_t r;
  if ((r = val_dict_push(&vm->dict,hash))) return _fatal(r);
  vm->scope = val_dict_h(&vm->dict);
  return 0;
}

int vm_has(vm_t *vm, unsigned int n) {
  return val_list_len(vm->open_list)>=n;
}
int vm_check1(vm_t *vm, enum valtype t) {
  return val_stack_check1(vm->open_list,t);
}
int vm_check2(vm_t *vm, enum valtype ta, enum valtype tb) {
  return val_stack_check2(vm->open_list,ta,tb);
}
int vm_check3(vm_t *vm, enum valtype ta, enum valtype tb, enum valtype tc) {
  return val_stack_check3(vm->open_list,ta,tb,tc);
}
err_t vm_popget_(vm_t *vm, val_t **p) {
  return val_stack_popget_(vm->open_list,p);
}
err_t vm_pop2get_(vm_t *vm, val_t **p) {
  return val_stack_pop2get_(vm->open_list,p);
}
err_t vm_popget2_(vm_t *vm, val_t **p) {
  return val_stack_popget2_(vm->open_list,p);
}
err_t vm_get1_(vm_t *vm, val_t **p) {
  return val_stack_get1_(vm->open_list,p);
}
err_t vm_get2_(vm_t *vm, val_t **p) {
  return val_stack_get2_(vm->open_list,p);
}
err_t vm_get3_(vm_t *vm, val_t **p) {
  return val_stack_get3_(vm->open_list,p);
}
err_t vm_getn_(vm_t *vm, unsigned int n, val_t **p) {
  return val_stack_getn_(vm->open_list,n,p);
}
err_t vm_pop1_(vm_t *vm, val_t **p) {
  return val_stack_pop1_(vm->open_list,p);
}
err_t vm_pop2_(vm_t *vm, val_t **p) {
  return val_stack_pop2_(vm->open_list,p);
}
err_t vm_pop3_(vm_t *vm, val_t **p) {
  return val_stack_pop3_(vm->open_list,p);
}
err_t vm_popn_(vm_t *vm, unsigned int n, val_t **p) {
  return val_stack_popn_(vm->open_list,n,p);
}
err_t vm_popngetn_(vm_t *vm, unsigned int npop, unsigned int nget, val_t **p) {
  return val_stack_popngetn_(vm->open_list,npop,nget,p);
}
void vm_unpop1_(vm_t *vm) {
  return val_stack_unpop1_(vm->open_list);
}
void vm_unpop2_(vm_t *vm) {
  return val_stack_unpop2_(vm->open_list);
}
void vm_unpop3_(vm_t *vm) {
  return val_stack_unpop3_(vm->open_list);
}

err_t vm_extend_(vm_t *vm, unsigned int n, val_t **p) {
  return val_stack_extend_(vm->open_list,n,p);
}

err_t vm_wextend_(vm_t *vm, unsigned int n, val_t **p) {
  return val_stack_extend_(&vm->work,n,p);
}
err_t vm_cextend_(vm_t *vm, unsigned int n, val_t **cp) {
  return val_stack_extend_(&vm->cont,n,cp);
}
err_t vm_cpopn_(vm_t *vm, unsigned int n, val_t **cp) {
  return val_stack_popn_(&vm->cont,n,cp);
}

err_t vm_getextend1_(vm_t *vm, val_t **p) {
  return val_stack_getextend1_(vm->open_list,p);
}
err_t vm_getnextendn_(vm_t *vm, unsigned int nget, unsigned int npush, val_t **p) {
  return val_stack_getnextendn_(vm->open_list,nget,npush,p);
}
void vm_unpush1_(vm_t *vm) {
  return val_stack_unpush1_(vm->open_list);
}
void vm_unpush2_(vm_t *vm) {
  return val_stack_unpush2_(vm->open_list);
}
void vm_unpush3_(vm_t *vm) {
  return val_stack_unpush3_(vm->open_list);
}

val_t* vm_dict_get_cstr(vm_t *vm, const char *key) {
  return hash_get(vm->scope,key);
}
val_t* vm_dict_get_(vm_t *vm, const char *key, unsigned int len) {
  return hash_get_(vm->scope,key,len);
}
val_t* vm_dict_get(vm_t *vm, val_t *key) {
  return hash_getv(vm->scope,key);
}

err_t vm_dict_put(vm_t *vm, val_t *key, val_t *val) {
  throw_if(ERR_DICT,-1 == hash_putv(vm->scope,key,val,1));
  val_destroy(key);
  return 0;
}
err_t vm_dict_swap(vm_t *vm, val_t *key, val_t *val) {
  val_t *v;
  throw_if(ERR_UNDEFINED, !(v = hash_getv(vm->scope,key)));
  val_swap(v,val);
  return 0;
}
err_t vm_dict_take(vm_t *vm, val_t *key, val_t *val) {
  val_t *v;
  throw_if(ERR_UNDEFINED,!(v = hash_getv(vm->scope,key)));
  val_null_init(val);
  val_swap(v,val);
  return 0;
}

err_t vm_push(vm_t *vm, val_t *el) {
  return val_stack_push(vm->open_list,el);
}

err_t vm_push2(vm_t *vm, val_t *a, val_t *b) {
  return val_stack_push2(vm->open_list,a,b);
}

err_t vm_push3(vm_t *vm, val_t *a, val_t *b, val_t *c) {
  return val_stack_push3(vm->open_list,a,b,c);
}
err_t vm_pop(vm_t *vm, val_t *val) { //pop top of stack
  return val_stack_pop(vm->open_list,val);
}

err_t vm_pushsw(vm_t *vm) {
  __op_pop1;
  if (val_iscode(p) && val_list_empty(p)) {
    val_destroy(p);
  } else {
    __op_wpush1;
    val_move(q,p);
  }
  return 0;
}

err_t vm_wgetn_(vm_t *vm, unsigned int n, val_t **q) {
  return val_stack_getn_(&vm->work,n,q);
}
err_t vm_wappend(vm_t *vm, val_t *val) {
  if (val_iscode(val) && val_list_empty(val)) return val_destroy(val);
  return val_stack_rpush(&vm->work,val);
}
err_t vm_wpush(vm_t *vm, val_t *w) {
  return val_stack_push(&vm->work, w);
}
err_t vm_wpush2(vm_t *vm, val_t *a, val_t *b) {
  return val_stack_push2(&vm->work,a,b);
}
err_t vm_wpush3(vm_t *vm, val_t *a, val_t *b, val_t *c) {
  return val_stack_push3(&vm->work,a,b,c);
}

err_t vm_hold(vm_t *vm, val_t *val) {
  __op_wpush1;
  err_t e;
  if ((e = val_wprotect(val))) goto bad;
  val_move(q,val);
  return 0;
bad:
  val_destroy(val);
  return e;
}

err_t vm_wpop(vm_t *vm, val_t *val) { //pop top of work stack
  return val_stack_pop(&vm->work,val);
}

val_t* _vm_getnext(vm_t *vm) { //get next item to eval
  debug_assert(!vm_empty(vm));
  err_t e;
  val_t *p;
  if ((e = val_stack_get1_(&vm->work,&p))) {
    _fatal(e);
    return NULL;
  } else if (val_iscode(p)) {
    return val_list_head(p);
  } else {
    return p;
  }
}
err_t _vm_popnext(vm_t *vm, val_t *val) { //pop next item to eval
  err_t e;
  val_t *p;
  if ((e = val_stack_get1_(&vm->work,&p))) return _fatal(e);
  if (val_iscode(p)) {
    if ((e = val_list_lpop(p,val))) return _fatal(e);
    return 0;
  } else {
    return vm_wpop(vm,val);
  }
}
err_t vm_splitnextn(vm_t *vm, unsigned int n) { //if the next work item is a quotation, split it so it contains at most n items
  __op_wget(1);

  if (val_iscode(q) && val_list_len(q)>n) {
    val_t t;
    err_t r;
    if ((r = val_list_lsplitn(q,&t,n))) return r;
    return vm_wpush(vm,&t);
  } else {
    return 0;
  }
}


err_t _vm_catch(vm_t *vm) {
  __op_pop1; //save exception from top of stack
  __op_cpop(3); //cpop catch args
  val_t err;
  val_move(&err,p);
  val_destroy(&vm->work); //destroy stacks
  val_destroy(&vm->stack);
  vm->groupi = val_int(cp+2); //restore groupi
  val_clear(cp+2);
  val_move(&vm->stack,cp+1);
  val_move(&vm->work,cp);

  err_t e;
  if ((e = val_list_deref(&vm->stack))) return _fatal(e); //restore stack
  if ((e = val_list_deref(&vm->work))) return _fatal(e); //restore wstack with catch handler
  _vm_fix_open_list(vm); //update vm->open_list (in case groupi!=0)

  __op_push1_more;
  val_move(p,&err);

  return 0;
}

err_t _vm_try(vm_t *vm) {
  throw_if(ERR_MISSINGARGS,!vm_has(vm,2));
  __op_pop2; //pop try+catch
  __op_cpush(4);

  err_t e; //TODO: make this safe for all possible failure cases
  if ((e = val_clone(cp,&vm->work))) return e; //save workstack
  if ((e = val_stack_push(cp,p+1))) return e; //push catch handler to saved workstack (will also deref)
  if ((e = val_clone(cp+1,&vm->stack))) return e; //save stack
  if ((e = val_list_deref(cp+1))) return e; //deref cloned stack
  val_int_init(cp+2,vm->groupi);
  val_func_init(cp+3,_vm_catch,"_catch");
  __op_wpush2;
  
  val_move(q+1,p);
  val_func_init(q,_vm_endtry,"_endtry");
  return 0;
}

err_t _vm_trydebug(vm_t *vm) {
  __op_pop1;
  __op_wpush2;
  __op_cpush1;
  val_func_init(q,_vm_endtrydebug,"_endtrydebug");
  val_func_init(cp,_vm_debug_catch,"_debugcatch");
  val_move(q+1,p);
  return 0;
}

err_t _vm_endtrydebug(vm_t *vm) {
  return val_stack_pop(&vm->cont,NULL);
}

err_t _vm_endtry(vm_t *vm) {
  __op_cpop(4);
  val_destroy(cp);
  val_destroy(cp+1);
  val_destroy(cp+2);
  val_destroy(cp+3);
  return 0;
}
err_t vm_cpush_debugfallback(vm_t *vm) { //
  __op_cpush1;
  val_func_init(cp,_vm_debug_fallback,"_debugfallback");
  return 0;
}

err_t _vm_catchinteractive(vm_t *vm) {
  vm_perror(vm);
  vm_reset(vm);
  val_t *q,*cp;
  err_t e;
  if ((e = vm_wextend_(vm,1,&q))) return _fatal(e);
  if ((e = vm_cextend_(vm,1,&cp))) return _fatal(e);

  val_file_init_stdin(q);
  val_func_init(cp,_vm_catchinteractive,"_catchinteractive");
  return 0;
}

err_t vm_set_interactive_debug(vm_t *vm) {
  val_t *cp = val_list_tail(&vm->cont);
  if (cp && val_isfunc(cp) && val_func_f(cp) == _vm_catchinteractive) return 0; //already interactive

  err_t e;
  if ((e = vm_cextend_(vm,1,&cp))) return _fatal(e);
  val_func_init(cp,_vm_catchinteractive,"_catchinteractive");
  return 0;
}





err_t vm_wait(vm_t *vm) {
  err_t e;
  fatal_if(e,(e = _vm_lock(vm)));
  if (vm->state != STOPPED) { //need to join thread
    e = _vm_join(vm);
    vm->state = STOPPED; //STOPPED after we have called join
  }
  fatal_if(e,(e = _vm_unlock(vm)));
  return e;
}
err_t _vm_join(vm_t *vm) { //must already have lock
  void *ret;
  throw_if(ERR_THREAD,(pthread_join(vm->thread, &ret)));
  if (ret == PTHREAD_CANCELED) {
    return ERR_VM_CANCELLED;
  } else {
    err_t e=(int)(intptr_t)ret;
    return e ? _throw(e) : e; //rethrow joined thread exception in joining thread
  }
}
err_t _vm_cancel(vm_t *vm) {
  //err_t e;
  //throw_if(ERR_THREAD,(e = pthread_cancel(vm->thread)));
  pthread_cancel(vm->thread);
  return 0;
}
err_t _vm_lock(vm_t *vm) {
  throw_if(ERR_LOCK,sem_wait(&vm->lock));
  return 0;
}
err_t _vm_trylock(vm_t *vm) {
  err_t r;
  if (!(r = sem_trywait(&vm->lock))) return 0;
  else if (errno == EAGAIN) return ERR_LOCKED;
  else return _throw(ERR_LOCK);
}
err_t _vm_unlock(vm_t *vm) {
  throw_if(ERR_LOCK,sem_post(&vm->lock));
  return 0;
}

int vm_list(vm_t *vm) {
  return val_stack_list(vm->open_list);
}
int vm_listn(vm_t *vm, unsigned int n) {
  return val_stack_listn(vm->open_list,n);
}

err_t vm_printstate(vm_t *vm) {
  printf("Stack:\n");
  vm_list(vm);
  printf("\nWork:\n");
  val_stack_list(&vm->work);
  printf("\nCont:\n");
  val_stack_list(&vm->cont);
  return 0;
}

int vm_qstate(vm_t *vm) {
  fmt_t fmt = *fmt_V;
  fmt.precision = 120;
  err_t r;
  val_t buf; //sprint to buf first to help with threaded debugging
  val_string_init(&buf);
  if (0>(r = val_sprint_cstr(&buf,"State: "))) goto out;
  if (0>(r = vmstate_sprintf(vm,&buf,&fmt))) goto out;
  if (0>(r = val_sprint_(&buf,"\n",1))) goto out;
  r = val_fprintf_(&buf,stdout,fmt_v);
out:
  val_destroy(&buf);
  return r >= 0 ? 0 : r;
}
int vm_vstate(vm_t *vm) {
  err_t r;
  if (0>(r = val_fprint_cstr(stdout,"State: "))) return r;
  if (0>(r = vmstate_fprintf(vm,stdout,fmt_V))) return r;
  if (0>(r = val_fprint_(stdout,"\n",1))) return r;
  return 0;
}

int vmstate_fprintf(vm_t *vm, FILE *file, const fmt_t *fmt) {
  err_t r;
  val_t buf;
  val_string_init(&buf);
  if (0>(r = vmstate_sprintf(vm,&buf,fmt))) {
    val_destroy(&buf);
    return r;
  }
  return val_fprintf_(&buf,file,fmt_v);
}

int vmstate_sprintf(vm_t *vm, val_t *buf, const fmt_t *fmt) {
  err_t r;
  int rlen=0;
  const char *stacksep = "  <|>  ";
  int stackseplen = 7;

  if (vm->threadid>=0) {
    if (0>(r=val_sprint_cstr(buf,"tid:"))) return r; rlen += r;
    if (0>(r=_val_int_sprintf(vm->threadid,buf,fmt_v))) return r; rlen += r;
    if (0>(r=val_sprint_cstr(buf," "))) return r; rlen += r;
  }


  list_fmt_t lfmt = *list_fmt_V; //stack format
  list_fmt_t sublist_fmt = *list_fmt_V; //sublist format within stack
  lfmt.braces = 0;
  sublist_fmt.trunc_tail = 1;

  list_fmt_t wfmt = lfmt; //work stack format
  list_fmt_t wsublist_fmt = sublist_fmt; //sublist format within wstack
  wfmt.reverse = 1;
  wfmt.trunc_tail = 1;

  lfmt.sublist_fmt = &sublist_fmt;
  wfmt.sublist_fmt = &wsublist_fmt;

  if (fmt->precision >= 0) { //fmt->precision: total max bytes for vm (first pass checks for overflow)
    int stacklim = fmt->precision-rlen-stackseplen;
    if (stacklim < 3) stacklim = 3;
    lfmt.max_bytes = stacklim+1; //+1 to detect overflow when other stack empty
    wfmt.max_bytes = lfmt.max_bytes;
    //sublist_fmt.max_bytes = lfmt.max_bytes;
    //wsublist_fmt.max_bytes = lfmt.max_bytes;
    int rs,rw;
    if (0>(rs = val_list_sprintf_(&vm->stack,NULL,&lfmt,fmt_V))) return rs;
    if (0>(rw = val_list_sprintf_(&vm->work,NULL,&wfmt,fmt_V))) return rw;
    if (rs+rw>stacklim) { //need truncation, choose appropriate limits for each stack
      //we want to use all available chars, and each stack should be guaranteed at least stacklim/2
      if (rw <= stacklim/2) { //just need to truncate rw
        lfmt.max_bytes = stacklim-rw;
        if (lfmt.max_bytes < 3) lfmt.max_bytes = 3; //at least 3 chars for empty or ...
        //sublist_fmt.max_bytes = lfmt.max_bytes/2;
        //if (sublist_fmt.max_bytes < 3) sublist_fmt.max_bytes = 3;
      } else if (rs <= stacklim/2) { //just need to truncate rs
        wfmt.max_bytes = stacklim-rs;
        if (wfmt.max_bytes < 3) wfmt.max_bytes = 3; //at least 3 chars for empty or ...
        //wsublist_fmt.max_bytes = wfmt.max_bytes/2;
        //if (wsublist_fmt.max_bytes < 3) wsublist_fmt.max_bytes = 3;
      } else { //truncate both
        lfmt.max_bytes = (stacklim+1)/2;
        wfmt.max_bytes = stacklim/2;
        if (lfmt.max_bytes < 3) lfmt.max_bytes = 3; //at least 3 chars for empty or ...
        if (wfmt.max_bytes < 3) wfmt.max_bytes = 3; //at least 3 chars for empty or ...
        //sublist_fmt.max_bytes = lfmt.max_bytes/2;
        //wsublist_fmt.max_bytes = wfmt.max_bytes/2;
        //if (wsublist_fmt.max_bytes < 3) wsublist_fmt.max_bytes = 3;
        //if (sublist_fmt.max_bytes < 3) sublist_fmt.max_bytes = 3;
      }
    }
  }

  if (0>(r = val_list_sprintf_(&vm->stack,buf,&lfmt,fmt_V))) return r; rlen += r;
  if (0>(r = val_sprint_(buf,stacksep,stackseplen))) return r; rlen += r;
  if (fmt->precision >= 0 && rlen+wfmt.max_bytes > fmt->precision) { //for threading -- in case lengths changed
    wfmt.max_bytes = fmt->precision - rlen;
    if (wfmt.max_bytes<3) wfmt.max_bytes=3;
  }
  if (0>(r = val_list_sprintf_(&vm->work,buf,&wfmt,fmt_V))) return r; rlen += r;
  //debug_assert_r(fmt->precision < 0 || rlen <= fmt->precision); //doesn't play nice with threading


  //if (lfmt.max_bytes > 0 && (fmt->precision-stackseplen)%2) lfmt.max_bytes++; //if limit odd, add extra 1 to stack
  return rlen;
}
