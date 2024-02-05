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

#include "vm_debug.h"
//#include "vm_debug_internal.h"
#include "vm_internal.h"
#include "val_num.h"
#include "val_vm.h"
#include "vm_err.h"
#include "val_file.h" //for debugger stdin
#include "val_printf.h"
#include "opcodes.h"

#include <stdlib.h>

err_t vm_debugger_init(vm_t *vm) {
  err_t e;
  if ((e = vm_init(vm))) return e;
  if ((e = vm_wappend(vm,val_file_stdin_ref()))) goto out_vm;
  if ((e = vm_cpush(vm,__op_val(OP_catch_interactive)))) goto out_vm;
  return 0;
out_vm:
  vm_destroy(vm);
  return e;
}

err_t vm_debug_wrap(vm_t *vm) {
  valstruct_t *debuggee;
  err_t e;
  if (!(debuggee = _val_vm_alloc())) return _throw(ERR_MALLOC);
  *debuggee->v.vm = *vm;
  if ((e = vm_debugger_init(vm))) goto out_debuggee;
  if ((e = vm_push(vm,__vm_val(debuggee)))) goto out_vm;
  _vm_fix_open_list(debuggee->v.vm);
  return 0;
out_vm:
  vm_destroy(vm);
out_debuggee:
  *vm = *debuggee->v.vm;
  _val_vm_free(debuggee);
  return e;
}

err_t vm_exec(vm_t *vm) { //destroys current vm, switches execution to vm on top of the stack
  val_t top;
  err_t e;

  if (vm_empty(vm)) return _throw(ERR_EMPTY);
  if (!val_is_vm(top = vm_top(vm))) return _throw(ERR_BADTYPE);
  if ((e = vm_pop(vm,&top))) return e;
  valstruct_t *topv = __vm_ptr(top);

  vm_destroy(vm);
  *vm = *topv->v.vm;
  _val_vm_free(topv);
  _vm_fix_open_list(vm);
  return 0;
}


////TODO: this needs rewrite -- it is a hacked port of the old debugger
//
//int vm_debuggee(vm_t *vm, vm_t **debuggee) {
//  __op_get1;
//  throw_if(ERR_BADARGS,!val_isvm(p) || p->val.vm->state != STOPPED);
//  *debuggee = p->val.vm;
//  return 0;
//}
//
//int vm_debuggee_arg(vm_t *vm, vm_t **debuggee, val_t **arg) {
//  __op_get2;
//  throw_if(ERR_BADARGS,!val_isvm(p) || p->val.vm->state != STOPPED);
//  *debuggee = p->val.vm;
//  *arg = p+1;
//  return 0;
//}
//
//int vm_debuggee_popiarg(vm_t *vm, vm_t **debuggee, int *arg) {
//  __op_popget;
//  throw_if(ERR_BADARGS,!val_isvm(p) || p->val.vm->state != STOPPED || !val_isint(p+1));
//  *debuggee = p->val.vm;
//  *arg = val_int(p+1);
//  val_clear(p);
//  return 0;
//}
//
//int vm_debug_wrap(vm_t *vm) {
//  vm_t *debuggee;
//  fatal_if(ERR_FATAL,!(debuggee = (vm_t*)malloc(sizeof(vm_t))));
//  *debuggee = *vm; //copy vm over to *debuggee
//  int r;
//  val_t t;
//  if ((r = vm_init(vm))) goto out_bad; //re-init vm for debugger
//  else if ((r = _vm_debug_init(vm))) goto out_vm;
//  else if ((r = val_vm_init_(&t,debuggee))) goto out_vm;
//  else if ((r = vm_push(vm,&t))) goto out_vm;
//  _vm_fix_open_list(debuggee); //fix open_list pointer
//  vm_clearstats(debuggee);
//  return 0;
//out_vm:
//  vm_destroy(vm);
//out_bad:
//  *vm = *debuggee;
//  free(debuggee);
//  return r;
//}
//
////stop debugging, destroy debugger, and continue execution in debuggee
//int vm_debug_unwrap(vm_t *vm) {
//  __op_pop1;
//  throw_if(ERR_BADARGS,!val_isvm(p));
//  val_t d;
//  val_move(&d,p);
//  vm_destroy(vm);
//  *vm = *d.val.vm;
//  free(d.val.vm);
//  _vm_fix_open_list(vm); //fix open_list pointer
//  return 0;
//}
//
//int ops_debug_init(vm_t *vm) {
//  int r=0;
//  r |= vm_init_op(vm,"debug",_op_debug);
//  return r;
//}
//
//int _vm_debug_init(vm_t *vm) {
//  int r=0;
//  r |= vm_init_op(vm,"pnext",_op_pnext);
//
//  r |= vm_init_op(vm,"end_debug",_op_end_debug);
//  r |= vm_init_op(vm,"dpop",_op_dpop);
//  r |= vm_init_op(vm,"dwpop",_op_dwpop);
//  r |= vm_init_op(vm,"dpush",_op_dpush);
//  r |= vm_init_op(vm,"dwpush",_op_dwpush);
//  r |= vm_init_op(vm,"dtake",_op_dtake);
//  r |= vm_init_op(vm,"dwtake",_op_dwtake);
//  r |= vm_init_op(vm,"ddup",_op_ddup);
//  r |= vm_init_op(vm,"dwdup",_op_dwdup);
//  r |= vm_init_op(vm,"ddupnext",_op_ddupnext);
//
//  r |= vm_init_op(vm,"deval",_op_deval);
//
//  r |= vm_init_op(vm,"dlist",_op_dlist);
//  r |= vm_init_op(vm,"dlistn",_op_dlistn);
//  r |= vm_init_op(vm,"dwlist",_op_dwlist);
//  r |= vm_init_op(vm,"dwlistn",_op_dwlistn);
//  r |= vm_init_op(vm,"dpeek",_op_dpeek);
//  r |= vm_init_op(vm,"dwpeek",_op_dwpeek);
//  r |= vm_init_op(vm,"dstate",_op_dstate);
//  r |= vm_init_op(vm,"dqstate",_op_dqstate);
//  r |= vm_init_op(vm,"dvstate",_op_dvstate);
//  r |= vm_init_op(vm,"dstats",_op_dstats);
//  r |= vm_init_compile_op(vm,"stats.steps","dfirst");
//  r |= vm_init_compile_op(vm,"stats.smaxsize","dsecond");
//  r |= vm_init_compile_op(vm,"stats.wmaxsize","dthird");
//  r |= vm_init_compile_op(vm,"stats.ssize","dfourth");
//  r |= vm_init_compile_op(vm,"stats.wsize","dfifth");
//  r |= vm_init_compile_op(vm,"stats.diff","[ \\lpop dip swapd swap - ] map");
//  r |= vm_init_compile_op(vm,"stats.print","\"( steps=%1$v smaxsize=%2$v wmaxsize=%3$v ssize=%4$v wsize=%5$v )\" printlf");
//  r |= vm_init_op(vm,"clearstats",_op_clearstats);
//
//  r |= vm_init_op(vm,"dstack",_op_dstack);
//  r |= vm_init_op(vm,"dwstack",_op_dwstack);
//  r |= vm_init_op(vm,"dsetstack",_op_dsetstack);
//  r |= vm_init_op(vm,"dwsetstack",_op_dwsetstack);
//
//  r |= vm_init_op(vm,"step",_op_step);
//  r |= vm_init_op(vm,"s",_op_step);
//  r |= vm_init_op(vm,"stepq",_op_stepq);
//  r |= vm_init_op(vm,"stepn",_op_stepn);
//  r |= vm_init_compile_op(vm,"nextn","\\nextq times dqstate");
//  r |= vm_init_op(vm,"continue",_op_continue);
//
//  r |= vm_init_op(vm,"splitnext",_op_splitnext);
//  //r |= vm_init_op(vm,"slicenext",_op_slicenext);
//  r |= vm_init_op(vm,"nextw",_op_nextw);
//  r |= vm_init_op(vm,"hasnext",_op_hasnext);
//  r |= vm_init_op(vm,"next",_op_next);
//  r |= vm_init_op(vm,"n",_op_next);
//  r |= vm_init_op(vm,"nextq",_op_nextq);
//  //r |= vm_init_compile_op(vm,"dwparsefiles","dwstack [ dup isfile [ () swap [ parsecode_ dup empty \\pop [swap lpush] ifelse ] eachline pop ] [ () lpush ] ifelse ] mmap dwsetstack");
//
//  r |= vm_init_compile_op(vm,"dprofile","dstats [nextq] dip "
//      "'Initial stats: ' print_ stats.print "
//      "[dstats "
//      "'\nFinal stats:   ' print_ stats.print] dip "
//      "swap stats.diff "
//      "'\nSteps Taken: ' print_ stats.steps print "
//      "pop "
//      "dqstate"
//      );
//  r |= vm_init_compile_op(vm,"continue.until","stepq [ hasnext [dup dip swap not] 0 ifelse ] \\stepq while pop dqstate");
//  r |= vm_init_compile_op(vm,"continue.untilword","stepq [ hasnext [dup ddupnext eq not] 0 ifelse ] \\stepq while pop dqstate");
//
//  return r;
//}
//
//int vm_debug_pnext(vm_t *vm) {
//  return _fatal(ERR_NOT_IMPLEMENTED);
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  val_t *w = _vm_getnext(d);
//  if (w) {
//    printf("next = ");
//    val_printV(w);
//  } else {
//    printf("next = NONE\n");
//  }
//  return 0;
//}
//
//int vm_debug_qstate(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  if ((r = vm_qstate(d))) return r;
//  return 0;
//}
//
//int vm_debug_vstate(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  if ((r = vm_vstate(d))) return r;
//  return 0;
//}
//
//int _op_pnext(vm_t *vm) {
//  return vm_debug_pnext(vm);
//}
//
//int _op_debug(vm_t *vm) {
//  int r;
//  val_t t;
//  if ((r = vm_debug_wrap(vm))) return r;
//  if ((r = vm_wappend(vm,val_file_init_stdin(&t)))) return r;
//  r = vm_set_interactive_debug(vm);
//  return r;
//}
//
//int _op_end_debug(vm_t *vm) {
//  return vm_debug_unwrap(vm);
//}
//
//int _op_dlist(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  return vm_list(d);
//}
//int _op_dlistn(vm_t *vm) {
//  int r;
//  vm_t *d;
//  val_t *top;
//  if ((r = vm_debuggee_arg(vm,&d,&top))) return r;
//  throw_if(ERR_BADARGS,!val_isint(top));
//  valint_t n = val_int(top);
//  vm_pop(vm,NULL);
//  return vm_listn(d,n);
//}
//
//int _op_dwlist(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  return val_stack_list(&d->work);
//}
//int _op_dwlistn(vm_t *vm) {
//  int r;
//  vm_t *d;
//  val_t *top;
//  if ((r = vm_debuggee_arg(vm,&d,&top))) return r;
//  throw_if(ERR_BADARGS,!val_isint(top));
//  valint_t n = val_int(top);
//  vm_pop(vm,NULL);
//  return val_stack_listn(&d->work,n);
//}
//
//int _op_dpeek(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  throw_vm_empty(d);
//  val_t *p;
//  vm_get1_(d,&p);
//  val_printV(p);
//  return 0;
//}
//
//int _op_dwpeek(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  throw_if(ERR_EMPTY,vm_wempty(d));
//  val_printV(val_stack_top(&d->work));
//  return 0;
//}
//
//int _op_dstate(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  return vm_printstate(d);
//}
//
//int _op_dqstate(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  if ((r = vm_qstate(d))) return r;
//  return 0;
//}
//
//int _op_dvstate(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  if ((r = vm_vstate(d))) return r;
//  return 0;
//}
//
//int _op_dstats(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  val_t stats;
//  val_list_init(&stats);
//  if ((r = val_list_rreserve(&stats,5))) return r;
//  val_t t;
//  if ((r = val_list_rpush(&stats,val_int_init(&t,d->stats.steps)))) return r;     //first
//  if ((r = val_list_rpush(&stats,val_int_init(&t,d->stats.max_stack)))) return r; //second
//  if ((r = val_list_rpush(&stats,val_int_init(&t,d->stats.max_work)))) return r;  //third
//  if ((r = val_list_rpush(&stats,val_int_init(&t,vm_stacksize(d))))) return r;    //fourth
//  if ((r = val_list_rpush(&stats,val_int_init(&t,vm_wstacksize(d))))) return r;   //fifth
//  r = vm_push(vm,&stats);
//  return r;
//}
//
//int _op_clearstats(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  vm_clearstats(d);
//  return 0;
//}
//
//int _op_dstack(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  val_t t;
//  if ((r = val_clone(&t,&d->stack))) return r;
//  return vm_push(vm,&t);
//}
//
//int _op_dwstack(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  val_t t;
//  if ((r = val_clone(&t,&d->work))) return r;
//  return vm_push(vm,&t);
//}
//
//int _op_dsetstack(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  throw_if(ERR_EMPTY,vm_empty(vm));
//  val_destroy(&d->stack);
//  if ((r = vm_pop(vm,&d->stack))) {
//    val_list_empty(&d->stack);
//  }
//  d->groupi=0;
//  d->noeval=0;
//  _vm_fix_open_list(d);
//  return r;
//}
//
//int _op_dwsetstack(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  throw_vm_empty(vm);
//  val_destroy(&d->stack);
//  if ((r = vm_pop(vm,&d->work))) {
//    val_list_empty(&d->work);
//  }
//  return r;
//}
//
//
//int _op_dpop(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  return vm_pop(d,NULL);
//}
//
//int _op_dwpop(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  return val_stack_pop(&d->work,NULL);
//}
//
//int _op_dpush(vm_t *vm) { //push val to debuggee stack
//  __op_pop1;
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  return vm_push(d,p);
//}
//int _op_dwpush(vm_t *vm) { //push val to debuggee work stack
//  __op_pop1;
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  return vm_wpush(vm,p);
//}
//
//int _op_dtake(vm_t *vm) { //take val from debuggee stack
//  __op_push1;
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  return vm_pop(vm,p);
//}
//int _op_dwtake(vm_t *vm) { //take val from debuggee work stack
//  __op_push1;
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  return vm_wpop(vm,p);
//}
//
//int _op_ddup(vm_t *vm) { //take val from debuggee stack
//  __op_push1;
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  throw_vm_empty(d);
//
//  val_t *dp;
//  if ((r = vm_wgetn_(vm,1,&dp))) return r;
//  if ((r = val_clone(p,dp))) return r;
//  return 0;
//}
//
//int _op_dwdup(vm_t *vm) { //copy val from debuggee work stack
//  __op_push1;
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  throw_if(ERR_EMPTY,vm_wempty(d));
//  val_t *dq;
//  if ((r = vm_wgetn_(vm,1,&dq))) return r;
//  if ((r = val_clone(p,dq))) return r;
//  return 0;
//}
//
//int _op_ddupnext(vm_t *vm) { //copy val from debuggee work stack (next to be eval'd)
//  int r;
//  vm_t *d;
//  val_t t;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  throw_if(ERR_EMPTY,vm_wempty(d));
//  if ((r = val_clone(&t,_vm_getnext(d)))) return r;
//  return vm_push(vm,&t);
//}
//
//
//int _op_deval(vm_t *vm) {
//  int r;
//  vm_t *d;
//  val_t *dq;
//  __op_pop1;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  if ((r = vm_wextend_(vm,1,&dq))) return r;
//  val_move(dq,p);
//  
//  if (err_isfatal(r = vm_step(d,-1,vm_wstacksize(d)))) return r;
//  else if (r) r = vm_debug_throw(vm,d,r);
//
//  vm_debug_qstate(vm);
//  return r;
//}
//
//
//int _op_step(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  if (err_isfatal(r = vm_step(d,1,0))) return r;
//  else if (r) r = vm_debug_throw(vm,d,r);
//  vm_debug_qstate(vm);
//  return r;
//}
//
//int _op_stepq(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  if (err_isfatal(r = vm_step(d,1,0))) return r;
//  else if (r) r = vm_debug_throw(vm,d,r);
//  return r;
//}
//
//int _op_stepn(vm_t *vm) {
//  int r;
//  int n;
//  vm_t *d;
//  if ((r = vm_debuggee_popiarg(vm,&d,&n))) return r;
//  if (err_isfatal(r = vm_step(d,n,0))) return r;
//  else if (r) r = vm_debug_throw(vm,d,r);
//  vm_debug_qstate(vm);
//  return r;
//}
//
//int _op_continue(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  int ret =  vm_step(d,-1,0);
//  vm_debug_qstate(vm);
//  return ret;
//}
//
//int _op_splitnext(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  throw_if(ERR_EMPTY,vm_wempty(d));
//
//  if (err_isfatal(r = vm_splitnextn(d,1))) return r;
//  vm_debug_qstate(vm);
//  return r;
//}
//
//int _op_nextw(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  throw_if(ERR_EMPTY,vm_wempty(d));
//
//  if (err_isfatal(r = vm_step(d,-1,vm_wstacksize(d)))) return r;
//  else if (r) r = vm_debug_throw(vm,d,r);
//  vm_debug_qstate(vm);
//  return r;
//}
//
//int _op_hasnext(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  val_t t;
//  return vm_push(vm,val_int_init(&t,!vm_wempty(d)));
//}
//
//int _op_next(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  throw_if(ERR_EMPTY,vm_wempty(d));
//
//  if ((r = vm_splitnextn(d,1))) return r;
//  
//  if (err_isfatal(r = vm_step(d,-1,vm_wstacksize(d)))) return r;
//  else if (r) r = vm_debug_throw(vm,d,r);
//  vm_debug_qstate(vm);
//  return r;
//}
//
//int _op_nextq(vm_t *vm) {
//  int r;
//  vm_t *d;
//  if ((r = vm_debuggee(vm,&d))) return r;
//  throw_if(ERR_EMPTY,vm_wempty(d));
//
//  if ((r = vm_splitnextn(d,1))) return r;
//
//  if (err_isfatal(r = vm_step(d,-1,vm_wstacksize(d)))) return r;
//  else if (r) r = vm_debug_throw(vm,d,r);
//  return r;
//}
//
//int _op_break(vm_t *vm) {
//  return -1;
//}

int vm_debug_val_init(val_t *val, const char *func) {
  return -1;
}

int vm_debug_val_nodestroy(val_t *val) {
  return -1;
}

int vm_debug_val_destroy(val_t *val) {
  return -1;
}

int vm_debug_val_clone(val_t *val, val_t *orig) {
  return -1;
}

int vm_debug_val_wpush(vm_t *vm, val_t *val) {
  return -1;
}

int vm_debug_eval(vm_t *vm, val_t *val) {
  vm_qstate(vm);
  return -1;
}

//int _vm_debug_catch(vm_t *vm) {
//  int r = vm_debug_wrap(vm);
//  vm_t *d;
//  val_t t;
//  if (!r) r = vm_debuggee(vm,&d);
//  if (!r) r = vm_wappend(vm,val_file_init_stdin(&t));
//  if (!r) r = vm_set_interactive_debug(vm);
//  if (!r) val_code_init(&t);
//  if (!r) r = val_stack_push(&d->cont,&t); //push empty cont so if we pass _endtrydebug we are fine
//
//  if (r) return _fatal(r);
//  else {
//    if (vm_empty(d)) {
//      printf("Trap to debugger (missing exception)\n");
//      return 0;
//    } else {
//      if ((r = val_stack_push_from(vm->open_list,d->open_list))) return _fatal(r);
//      __op_get1;
//      if (0>(r = val_fprint_cstr(stdout,"Trap to debugger with ERROR: "))) return _fatal(r);
//      if (0>(r = val_fprintf_(p,stdout,fmt_V))) return _fatal(r);
//      if (0>(r = val_fprint_(stdout,"\n",1))) return _fatal(r);
//      return 0;
//    }
//  }
//}
//
//int _vm_debug_fallback(vm_t *vm) {
//  int r = vm_debug_wrap(vm);
//  vm_t *d;
//  val_t t;
//  if (!r) r = vm_debuggee(vm,&d);
//  if (!r) r = vm_wappend(vm,val_file_init_stdin(&t));
//  if (!r) r = vm_set_interactive_debug(vm);
//
//  if (r) return _fatal(r);
//  else {
//    if (vm_empty(d)) {
//      printf("Trap to debugger (unknown exception)\n");
//      return 0;
//    } else {
//      if ((r = val_stack_push_from(vm->open_list,d->open_list))) return _fatal(r);
//      __op_get1;
//      if (0>(r = val_fprint_cstr(stdout,"Trap to debugger with ERROR: "))) return _fatal(r);
//      if (0>(r = val_fprintf_(p,stdout,fmt_V))) return _fatal(r);
//      if (0>(r = val_fprint_(stdout,"\n",1))) return _fatal(r);
//      return 0;
//    }
//  }
//}
