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

#include "vm.h"
#include "vm_internal.h"
#include "vm_bytecode.h"
#include "vm_err.h"
#include "vmstate_internal.h"
#include "vm_parser.h"
#include "val_include.h"
#include "ops.h"
#include "helpers.h"

#include "val_list_internal.h" //so dowork can skip high-level list functions

#include <errno.h>

//This file contains the basic concat VM that evals the work stack until empty
//  - handlers for each val type
//  - code assumes stack+work are singletons, having only 1 ref to listbuf (asserted each loop in debug version)

//VM notes:
//  - the 4 concat stacks are maintained as actual list vals (this makes subvm/thread implementation easy)
//  - VM evals the next val from the work stack until the work stack is empty
//  - on exception, if cont is empty the vm returns the exception. else we do the following steps
//    - clear work stack TODO: clear work stack, or rely on catch handler to clear rest of work stack?
//    - non-THROW/USER_THROW exceptions are pushed to the stack as ints and converted to THROW
//    - top of cont is popped and pushed to wstack
//    - eval continues as normal (with exception on top of stack, and err should be THROW/USER_THROW)
//    - TODO: stack unwinding on work, or cont???
//  - text code is compiled on read to a quotation val which is then eval'd
//  - WIP bytecode vals switch to the bytecode interpreter, which is a token-threaded bytecode VM which implements the core of the concat language
//    - the standard VM (currently) calls the functions contained in native func vals for the core of the language
//    - in either case processing an actual ident requires a dictionary lookup which is far slower, so the extra performance only matters for code that has at least had all of its idents resolved



//TODO: review file ops (especially returned vals)
//  - right now they return 1/2 args, where on non-error the top arg is just the length of the string under it (and otherwise is negative with no string under it)
//  - instead we could just return either a string or an integer error code (simpler on the stack, and can test type to know which it is)
//TODO: review cleanup/exception handling code and argument checks
//  - earlier valgrinding showed most of our time was spent in (trivial) stack twiddling requiring lots of functions calls to do not much
//    - a significant percent of this work is the cumulative checks every call makes
//      - some of the recent/current work is in reducing this
//    - we should have all necessary checks to properly throw and handle exceptions and prevent all leaks, but no more than necessary
//      - in some cases this requires redesign (e.g. in ops.c exposing stack array,
//        current refactor to expose segment of stack is greatly simplifying things)
//      - in other cases we just need to review all of the worst-case
//        call-chains that can form, and see where they can be reduced or unecessary (duplicate) checks eliminated
//      - probably more void (internal library) functions where we only check anything for debugging anyways (and then we can still get the trap out to gdb and a printed error message when we are debugging)
//  - TODO: sort out ref locking/unlocking -- maybe it needs an owning thread-id so if a thread dies abruptly we can reclaim its refs
//    - need a good way to always unlock refs on exception that escapes the lock
//    - maybe push unlock to cont stack, but then we need a way to drop it when we unlock gracefully
//      - alternatively lock acquiring implies a trycatch, but that seems heavy
//        - maybe design a cheaper try-finally and use it?
//  - TODO: goal is 100% leak-proof core language
//    - then a concat program/script will be leak-proof using the core language
//      - we destroy as early as possible currently, as opposed to GC
//        - later target is MCU, so need to be efficient with mem
//    - the only place data can reside is in one of the 4 stacks of either a root VM or a VM on the stack of another VM
//      - stack, work, dict, cont
//      - this makes running sub-vms (or even just trycatch)
//      - this also makes for nice clean ways to constrain memory/resources (TODO: support memory constraints)
//        - maximum stack sizes
//        - maximum total allocated list buffers/vals
//        - maximum allocated string buffers/bytes
//        - maximum dict sizes (number of entries + stack limit)
//    - nothing can leave a stack (other than to a temp or another stack) without getting destroyed
//      - for numbers (ints/floats) you are allowed to just clear
//    - no temp can leave its function without being destroyed or moved to one of the stacks
//      - this doesn't neccessarily apply to vals that allocate no memory (e.g. int/float)
//    - no val can be left in an inconsistent state at the return from an op such that when it is destroyed memory may be leaked
//      - e.g. leaving a buffer outside its val on return is bad, leaving 2 undestroyed vals above top-of-stack is fine (since every val in buffer will be destroyed)
//  - TODO: ops for inspecting broken list buffers (mainly for exception debugging -- inspect whole buffer, including vals that might not have been cleaned up like invalid args)
//    - looking around top plus the couple items above the stack that may have been popped is usually what you want / all you need
//  - TODO: review mapnth etc. functions for fail safety
//  - TODO: normalize failure handling (bad/bad_* label, where * is the vars that need destroying or a relevant label)
//    - if the bad label is just a skip to closer to the end of the functions we can call it bad/out
//    - right now it is a mix of if-then and bad/out labels
//  - TODO: some sort of distributed map op/technique
//    - split work of map over 1 or more threads
//    - one option is just to use existing ref
//      - add maprange to mapnth, and/or some ops for working on collections inside refs
//    - if we are doing a planned op over a list, we don't actually need locking
//      - unsafe at user level, just make sure no one touches the wrong els

err_t vm_init(vm_t *vm) {
  err_t r = _vmstate_init(vm);
  if (!r) val_init_type_handlers();
  if (!r) r = vm_init_ops(vm);
  return r;
}

err_t vm_destroy(vm_t *vm) {
  return _vmstate_destroy(vm);
}

static void * _vm_thread(void *arg) {
  err_t r;
  vm_t *vm = (vm_t*)arg;
  //FIXME: use deferred cancellation and/or clean-up handlers and/or data destructors so state after cancel is determinate and valid
  //  - see pthread_cancel(3)
  if ((r = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL))) {
    r = _throw(ERR_THREAD);
  } else {
    r = vm_dowork(vm,-1);
  }
  vm->state = FINISHED;
  _vm_unlock(vm);
  return (void*)(intptr_t)r;
}

err_t vm_thread(vm_t *vm) {
  static int _nextid = 0;
  fatal_if(ERR_LOCK,_vm_lock(vm));
  throw_if(ERR_BADARGS,vm->state != STOPPED);
  vm->state = RUNNING;
  vm->threadid = refcount_inc(_nextid);
  err_t r;
  if ((r = pthread_create(&vm->thread,NULL,_vm_thread,vm))) {
    vm->state = STOPPED;
  }
  //else {
  //  pthread_detach(vm->thread);
  //}
  return r;
}
err_t _vm_eval_final(val_t *vm) {
  err_t r;
  if (ERR_LOCKED == (r = _vm_trylock(vm->val.vm))) {
    r = vm_wait(vm->val.vm);
  } else {
    r = vm_dowork(vm->val.vm,-1);
  }
  return val_vm_finalize(vm,r);
}

//dowork processes the queued items at the top of the work stack
//most types just get eval'd, but types that contain code use special handling
err_t vm_dowork(vm_t *vm, int max_steps) {
  //TODO: ensure cont is handled correctly for non-failure cases
  //  - maybe throw ERR_TERM to terminate cleanly?
  val_t *wstack = &vm->work;
  err_t r = 0;

  while(max_steps && !val_list_empty(wstack)) {
    debug_assert_r(val_list_singleton(wstack));
    if (max_steps > 0) max_steps--;
    vm_op_handler *op;
    int len;

    int in_code=0;
    val_t *w, *code;
code_empty_retry:
    w = val_list_rget(wstack,0);
    if (val_iscode(w)) { //dowork loop REQUIRES any quotations pushed onto wstack are non-empty (though code-inside-code can be empty)
      if (val_list_empty(w)) {
        _val_list_rdrop(wstack);
        goto code_empty_retry;
      }
      in_code=1;
      code=w;
      w = _val_list_get(code,0);
    }
    VM_DEBUG_EVAL(w);

    val_t wt;
#define __popw do{ if(in_code){r=_val_list_lpop(code,&wt); if(val_list_empty(code))_val_list_rdrop(wstack);}else{r=_val_list_rpop(wstack,&wt);} w=&wt; }while(0);
#define __dropw do{ if(in_code){_val_list_ldrop(code); if(val_list_empty(code)) _val_list_rdrop(wstack);}else{_val_list_rdrop(wstack);} }while(0);
    if (val_isident(w) && _vm_qeval(vm,w,&r)) {
      __dropw
    } else if (vm_noeval(vm) && !val_isfile(w)) {
      goto pushw;
    } else {
      switch(w->type) {
        case TYPE_NULL: case TYPE_HASH: case TYPE_REF:
        case TYPE_FLOAT: case TYPE_INT:
        case TYPE_STRING: case TYPE_LIST: case TYPE_CODE:
          goto pushw;
        case TYPE_VM:
          __popw if (!r) {
            r = _vm_eval_final(w);
            if (r==0 || r==ERR_THROW || r==ERR_USER_THROW) {
              err_t ret;
              if ((ret = vm_push(vm,w))) return _fatal(ret);
            } else {
              val_destroy(w);
            }
          }
          break;
        case TYPE_BYTECODE:
          r = _vm_dobytecode(vm);
          break;
        case TYPE_NATIVE_FUNC: //built-in
          op = val_func_f(w);
          if (!val_func_keep(w)) { __dropw } //if we aren't keeping the func, pop it first
          r = op(vm);
          break;
        case TYPE_IDENT:
          if (val_ident_escaped(w)) { //escaped ident
            __popw if (!r) {
              r = val_ident_unescape(w);
              if (!r) r = vm_push(vm,w);
              else val_destroy(w);
            }
          } else {
            val_t *def = vm_dict_get(vm,w);
            if (def) {
              if (!(r = val_clone(&wt,def))) {
                __dropw r = vm_wpush(vm,&wt);
                if (r) val_destroy(&wt);
              }
            } else {
              fprintf(stderr,"unknown word '%.*s'\n",val_string_len(w),val_string_str(w));
              r = vm_throw(vm,ERR_UNDEFINED);
            }
          }
          break;
      case TYPE_FILE:
        len = val_file_readline_(w,FILE_BUF,FILE_BUFLEN); //FIXME: FILE_BUF not threadsafe
        if (len==FILE_EOF) { __dropw } //done with file

        else if (len>=0) {
          if (len==0 || (len==1 && FILE_BUF[0]=='\n')) break;
          val_code_init(&wt);
          r = vm_compile_input(vm,FILE_BUF,len,&wt);
          if (r) {
            printf("parse error: %d in %*.s\n", r,len,FILE_BUF);
            val_destroy(&wt);
          } else if (val_isnull(&wt) || val_list_empty(&wt)) {
            val_destroy(&wt);
          } else if (wt.type == VAL_INVALID) {
            printf("compile error\n");
            r = _fatal(ERR_FATAL);
          } else {
            if ((r = vm_wpush(vm,&wt))) val_destroy(&wt);
          }
        } else { //read error
          r=_throw(len);
        }
        break;
      default:
        r = _fatal(ERR_BADTYPE);
      }
    }
#undef __popw
#undef __dropw
    goto errcheck;
pushw:
    if (in_code) {
      r = val_stack_push_from_list(vm->open_list,code);
      if (val_list_empty(code)) _val_list_rdrop(wstack);
    } else {
      r = val_stack_push_from(vm->open_list,wstack);
    }
errcheck:
    if (r) {
      if (err_isfatal(r)) return r;
      else {
        if (err_isfatal(r = vm_throw(vm,r))) return _fatal(r);
        if (vm_hascont(vm)) {
          if ((r = vm_pushcw(vm))) return _fatal(r);
          r=0;
        } else {
          return r;
        }
      }
    }
  }
  return 0;
}

err_t vm_step(vm_t *vm, int max_steps, unsigned int min_wstack) { //run work with various limits (for debugging))
  val_t *wstack = &vm->work;
  err_t r = 0;

  while(max_steps && !val_list_empty(wstack)) {
    debug_assert_r(wstack->val.list.b->refcount == 1);
    if (max_steps > 0) max_steps--;
    vm_op_handler *op;
    int len;

    int in_code=0;
    val_t *w, *code;
code_empty_retry:
    w = val_list_rget(wstack,0);
    if (val_iscode(w)) { //dowork REQUIRES any quotations pushed onto wstack are non-empty (though code-inside-code can be empty)
      if (val_list_empty(w)) {
        _val_list_rdrop(wstack);
        goto code_empty_retry;
      }
      in_code=1;
      code=w;
      w = _val_list_get(code,0);
    }
    VM_DEBUG_EVAL(w);

    val_t wt;
#define __popw do{ if(in_code){r=_val_list_lpop(code,&wt); if(val_list_empty(code))_val_list_rdrop(wstack);}else{r=_val_list_rpop(wstack,&wt);} w=&wt; }while(0);
#define __dropw do{ if(in_code){_val_list_ldrop(code); if(val_list_empty(code)) _val_list_rdrop(wstack);}else{_val_list_rdrop(wstack);} }while(0);
    if (val_isident(w) && _vm_qeval(vm,w,&r)) {
      __dropw
    } else if (vm_noeval(vm) && !val_isfile(w)) {
      goto pushw;
    } else {
      switch(w->type) {
        case TYPE_NULL: case TYPE_HASH: case TYPE_REF:
        case TYPE_FLOAT: case TYPE_INT:
        case TYPE_STRING: case TYPE_LIST: case TYPE_CODE:
          goto pushw;
        case TYPE_VM:
          __popw if (!r) {
            r = _vm_eval_final(w);
            if (r==0 || r==ERR_THROW || r==ERR_USER_THROW) {
              err_t ret;
              if ((ret = vm_push(vm,w))) return _fatal(ret);
            } else {
              val_destroy(w);
            }
          }
          break;
        case TYPE_BYTECODE:
          r = _vm_dobytecode(vm);
          break;
        case TYPE_NATIVE_FUNC: //built-in
          op = val_func_f(w);
          if (!val_func_keep(w)) { __dropw } //if we aren't keeping the func, pop it first
          r = op(vm);
          break;
        case TYPE_IDENT:
          if (val_ident_escaped(w)) { //escaped ident
            __popw if (!r) {
              r = val_ident_unescape(w);
              if (!r) r = vm_push(vm,w);
              else val_destroy(w);
            }
          } else {
            val_t *def = vm_dict_get(vm,w);
            if (def) {
              if (!(r = val_clone(&wt,def))) {
                __dropw r = vm_wpush(vm,&wt);
                if (r) val_destroy(&wt);
              }
            } else {
              fprintf(stderr,"unknown word '%.*s'\n",val_string_len(w),val_string_str(w));
              r = vm_throw(vm,ERR_UNDEFINED);
            }
          }
          break;
      case TYPE_FILE:
        len = val_file_readline_(w,FILE_BUF,FILE_BUFLEN); //FIXME: FILE_BUF not threadsafe
        if (len==FILE_EOF) { __dropw } //done with file
        else if (len>=0) {
          if (len==0 || (len==1 && FILE_BUF[0]=='\n')) break;
          val_code_init(&wt);
          r = vm_compile_input(vm,FILE_BUF,len,&wt);
          if (r) {
            printf("parse error: %d in %*.s\n", r,len,FILE_BUF);
            val_destroy(&wt);
          } else if (val_isnull(&wt) || val_list_empty(&wt)) {
            val_destroy(&wt);
          } else if (wt.type == VAL_INVALID) {
            printf("compile error\n");
            r = _fatal(ERR_FATAL);
          } else {
            if ((r = vm_wpush(vm,&wt))) val_destroy(&wt);
          }
        } else { //read error
          r=_throw(len);
        }
        break;
      default:
        r = _fatal(ERR_BADTYPE);
      }
    }
#undef __popw
#undef __dropw
    goto errcheck;
pushw:
    if (in_code) {
      r = val_stack_push_from_list(vm->open_list,code);
      if (val_list_empty(code)) _val_list_rdrop(wstack);
    } else {
      r = val_stack_push_from(vm->open_list,wstack);
    }
errcheck:
    //update stats
    vm->stats.steps++;
    if (vm_stacksize(vm) > vm->stats.max_stack) vm->stats.max_stack = vm_stacksize(vm);
    if (vm_wstacksize(vm) > vm->stats.max_work) vm->stats.max_work = vm_wstacksize(vm);

    if (r || vm_wstacksize(vm) < min_wstack) return r;
  }
  return 0;
}


//eval -- handle a val (so push if it is a data type, or take appropriate action if it is an operator)
//destroys val, so clone val first if you still need it (and ensure all type handlers destroy val)
err_t vm_eval(vm_t *vm, val_t *val) {
  //printf("vm_eval: "); val_simpleprint(val); printf("\n");
  err_t ret;
  if (_vm_qeval(vm,val,&ret)) {
    val_destroy(val);
    return ret;
  } else if (val_ispush(val) || val_iscode(val) || val_isfile(val)) {
    return vm_push(vm,val);
  } else {
    switch(val->type) {
      case TYPE_VM:
        if (vm_noeval(vm)) {
          return vm_push(vm,val);
        } else {
          ret = _vm_eval_final(val);
          if (!ret) {
            ret = vm_push(vm,val);
          } else if (ret == ERR_THROW || ret == ERR_USER_THROW) {
            err_t r;
            if ((r = vm_push(vm,val))) return _fatal(r);
          } else {
            val_destroy(val);
          }
          return ret;
        }
      case TYPE_IDENT:
        if (vm_noeval(vm)) {
          return vm_push(vm,val);
        } else if (val_ident_escaped(val)) { //escaped ident
          val_string_substring(val,1,-1);
          return vm_push(vm,val);
        } else {
          val_t *v = vm_dict_get(vm,val);
          if (!v) {
            fprintf(stderr,"unknown word '%.*s'\n",val_string_len(val),val_string_str(val));
            val_destroy(val);
            return vm_throw(vm,ERR_UNDEFINED);
          } else {
            val_destroy(val);
            val_t t;
            if ((ret = val_clone(&t,v))) return ret;
            return vm_wpush(vm,&t);
          }
        }
      case TYPE_NATIVE_FUNC: //built-in
        if (val_func_keep(val)) {
          val_t *q;
          if ((ret = vm_wextend_(vm,1,&q))) { val_destroy(val); return ret; }
          val_move(q,val);
          ret = val_func_exec(q,vm);
        } else {
          ret = val_func_exec(val,vm);
          val_destroy(val);
        }
        if (ret) printf("error\n");
        return ret;
      case TYPE_BYTECODE:
      case TYPE_NULL:
      case VAL_INVALID:
      default:
        return _fatal(ERR_BADTYPE);
        break;
    }
  }
  return _fatal(ERR_FATAL);
}

err_t vm_eval_code(vm_t *vm, const char *str, int len) {
  if (!vm->p) {
    fatal_if(ERR_NO_PARSER, !(vm->p = get_vm_parser()));
  }
  if (len == 0 || str[0] == '\0') return 0;
  //struct parser_state pstate;
  //parser_state_init(&pstate);

  //printf("vm_evalraw '%s'\n",str);
  //TODO: return some flag and state so we can handle continuations
  return parser_eval(vm->p,str,len,NULL,vm_eval_handler,vm,NULL,NULL,vm_eval_handler,vm);
}

//TODO: add option to compile idents to native funcs
err_t vm_compile_input(vm_t *vm, const char *str, int len, val_t *code) {
  if (!vm->p) {
    fatal_if(ERR_NO_PARSER, !(vm->p = get_vm_parser()));
  }
  else if (len == 0 || str[0] == '\0') return 0;

  return parser_eval(vm->p,str,len,NULL,vm_save_input_handler,code,NULL,NULL,vm_save_input_handler,code);
}

err_t _vm_qeval(vm_t *vm, val_t *val, err_t *ret) {
  if (val_isident(val)) {
    unsigned int len = val_string_len(val);
    char c='\0';
    if (len) c = *val_string_get(val,0);
    if (c=='_') {
      if (0 == val_string_strcmp(val,"_quit")) {
        exit(0);
      } else if (0 == val_string_strcmp(val,"_list")) {
        *ret = vm_list(vm);
        return 1;
      } else if (0 == val_string_strcmp(val,"_debug")) {
        *ret = vm_debug_wrap(vm);
        return 1;
      //} else if (0 == val_string_strcmp(val,"_clear")) {
      //  *ret = val_stack_clear(vm->open_list);
      //  return 1;
      } else if (0 == val_string_strcmp(val,"_pop")) {
        *ret = val_stack_pop(&vm->stack,NULL);
        vm->groupi=0;
        vm->noeval=0;
        vm->open_list = &vm->stack;
        return 1;
      }
    } else if (len==1 && is_grouping_op(c)) {
      switch(*val_string_get(val,0)) {
        case '[': *ret = _vm_open_code(vm); return 1;
        case ']': *ret = _vm_close_code(vm); return 1;
        case '(': *ret = _vm_open_list(vm); return 1;
        case ')': *ret = _vm_close_list(vm); return 1;
        //case '{': case '}': return 0;
        default: *ret = _fatal(ERR_FATAL); return 1;
      }
    }
  }
  return 0;
}

err_t vm_compile_code(vm_t *vm, const char *str, int len, val_t *code, int opt) {
  if (!vm->p) {
    fatal_if(ERR_NO_PARSER, !(vm->p = get_vm_parser()));
  }
  else if (len == 0 || str[0] == '\0') return 0;
  struct _vm_compile_code_state pstate;
  pstate.root_list = code;
  pstate.open_list = code;
  pstate.groupi=0;
  pstate.opt=opt;
  pstate.vm=vm;

  return parser_eval(vm->p,str,len,NULL,vm_save_code_handler,&pstate,NULL,NULL,vm_save_code_handler,&pstate);
}

err_t vm_eval_handler(const char *tok, int len, int state, int target, void* arg) {
  vm_t *vm = arg;
  //enum parse_state pstate = state;
  //enum parse_state ptarget = target;

  err_t ret = vm_dowork(vm,-1);
  val_t t;
  if (!ret) ret = vm_eval_tok(tok,len,state,target,&t);
  if (ret) return _fatal(ret);

  if (!val_isnull(&t)) {
    ret = vm_eval(vm,&t);
    if (!ret) ret = vm_dowork(vm,-1);
  }

  return ret;
}

err_t vm_save_input_handler(const char *tok, int len, int state, int target, void* arg) {
  val_t *code=arg;

  err_t ret;

  val_t t;
  ret = vm_eval_tok(tok,len,state,target,&t);
  if (ret) return ret;

  if (!val_isnull(&t)) {
    ret = val_list_rpush(code,&t); if (ret) return ret;
  }

  return ret;
}

err_t vm_save_code_handler(const char *tok, int len, int state, int target, void* arg) {
  struct _vm_compile_code_state *pstate = arg;

  const int opt = pstate->opt;
  vm_t *vm = pstate->vm;
  err_t r;

  val_t t;
  if ((r = vm_eval_tok(tok,len,state,target,&t))) return r;

  if (val_isident(&t) && val_string_len(&t) == 1 && is_grouping_op(*val_string_get(&t,0))) { //group ops
    r = -1;
    unsigned int i;
    switch(*val_string_get(&t,0)) {
      case '[':
        r = val_destroy(&t);
        if (!r) val_code_init(&t);
        if (!r) r = val_list_rpush(pstate->open_list,&t);
        if (!r) pstate->groupi++;
        if (!r) pstate->open_list = val_list_tail(pstate->open_list);
        break;
      case ']':
        val_destroy(&t);
        throw_if(ERR_UNEXPECTED_EOC,pstate->groupi<1 || !val_iscode(pstate->open_list));
        pstate->groupi--;
        pstate->open_list = pstate->root_list;
        for(i=0;i<pstate->groupi;++i) {
          pstate->open_list = val_list_tail(pstate->open_list);
        }
        break;
      case '(':
        r = val_destroy(&t);
        if (!r) val_list_init(&t);
        if (!r) r = val_list_rpush(pstate->open_list,&t);
        if (!r) pstate->groupi++;
        if (!r) pstate->open_list = val_list_tail(pstate->open_list);
        break;
      case ')':
        val_destroy(&t);
        throw_if(ERR_UNEXPECTED_EOC,pstate->groupi<1 || val_iscode(pstate->open_list));
        pstate->groupi--;
        pstate->open_list = pstate->root_list;
        for(i=0;i<pstate->groupi;++i) {
          pstate->open_list = val_list_tail(pstate->open_list);
        }
        break;
    }
    return r;
  } else if (opt & OPT_NATIVEFUNC && val_isident(&t)) {
    r = vm_resolve_ident(vm,&t);
    if (!r) r = val_list_rpush(pstate->open_list,&t);
    return r;
  } else if (!val_isnull(&t)) {
    return val_list_rpush(pstate->open_list,&t);
  } else {
    val_destroy(&t); //only relevant for val tracing
  }
  return 0;
}

err_t vm_resolve_ident(vm_t *vm, val_t *ident) {
  if (val_isident(ident)) {
    int levels = val_ident_escaped_levels(ident);
    err_t r;
    if (levels) {
      val_t w;
      val_clone(&w,ident);
      r = val_string_substring(&w,levels,-1);
      if (r) return r;
      val_t *def = vm_dict_get(vm,&w);
      val_destroy(&w);

      if (!def) {
        return 0;
      } else {
        if (val_iscode(def) && val_list_len(def)==1) def=val_list_get(def,0); //quotation with single entry -- dequote
        while (val_isident(def)) { //recursively resolve idents
          fatal_if(ERR_FATAL,val_eq(ident,def)); //resolves to itself
          def = vm_dict_get(vm,def);
          if (!def) return 0; //couldn't resolve, just give up
          if (val_iscode(def) && val_list_len(def)==1) def=val_list_get(def,0); //quotation with single entry -- dequote
        }
        r = val_clone(&w,def);
        while(!r && levels--) {
          r = val_code_wrap(&w);
        }
        if (r) {
          val_destroy(&w);
          return r;
        } else {
          val_swap(ident,&w);
          val_destroy(&w);
          return 0;
        }
      }
    } else {
      val_t *def = vm_dict_get(vm,ident);
      if (!def) return 0;
      if (val_iscode(def) && val_list_len(def)==1) def=val_list_get(def,0); //quotation with single entry -- dequote
      while (val_isident(def)) { //recursively resolve idents
        fatal_if(ERR_FATAL,val_eq(ident,def)); //resolves to itself
        def = vm_dict_get(vm,def);
        if (!def) return 0; //couldn't resolve, just give up
        if (val_iscode(def) && val_list_len(def)==1) def=val_list_get(def,0); //quotation with single entry -- dequote
      }
      if (!(val_ispush(def) || val_isfunc(def))) return 0;

      val_t w;
      r = val_clone(&w,def);
      if (r) return r;
      val_swap(ident,&w);
      val_destroy(&w);
      return 0;
    }
  } else {
    return 0;
  }
}

