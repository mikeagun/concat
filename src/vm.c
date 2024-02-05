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

#include "opcodes.h"
#include "vm.h"
#include "vm_internal.h"
#include "vm_err.h"
#include "vm_debug.h"
#include "vm_parser.h"
#include "val.h"
#include "val_list.h"
#include "val_string.h"
#include "val_num.h"
#include "val_math.h"
#include "val_file.h"
#include "val_fd.h"
#include "val_dict.h"
#include "val_ref.h"
#include "val_printf.h"
#include "val_sort.h"
#include "val_vm.h"
#include "helpers.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <valgrind/helgrind.h>
#include <math.h>
#include <stdlib.h>
#include <errno.h>

//needed for platform natives:
#include <unistd.h>

//valgrind helgrind macros that should be inserted everywhere appropriate
//ANNOTATE_HAPPENS_BEFORE_FORGET_ALL
//ANNOTATE_HAPPENS_BEFORE
//ANNOTATE_HAPPENS_AFTER
//ANNOTATE_NEW_MEMORY
//ANNOTATE_RWLOCK_CREATE
//ANNOTATE_RWLOCK_DESTROY
//ANNOTATE_RWLOCK_ACQUIRED
//ANNOTATE_RWLOCK_RELEASED

//TODO: resolve syntax questions
// - file ops -- file on top or bottom???
//   - top more like list (lpop/lpush/rpop/...)
//   - bottom probably more intuitive (esp for simple usage)
//   - bottom better for cases where most of the data can be handled/created without anything from below the file on the stack
//     - e.g. writing a bunch of fixed strings
//   - top helps/encourages keeping file off stack (dip between the points where we need it)
//     - whichever is slightly more efficient, top may encourage better coding (dip rather than push lower on stack)
//   - top better for cases where most of the data needed to create or process the file is below the file on the stack
//   - top better for repeated file ops (e.g. 3 readlines in a row, or build up 3 lines, then write all 3)
// - dictionary ops -- dict on top or bottom???
//   - see file notes above
// - type checking ops
//   - should they normally test (push result on top of val) or replace val with result???
//   - separate ops per type, or single vm op to get type of val as int/string plus some category checking ops???
//     - and then int or string??? -- int faster, string more readable

//TODO: clean up argument handling (typechecking, collecting op args, returning vals from op)
//TODO: FIXME: ops to implement (as opcode, quotation, or native func):
//  math (trig and rand):
//    sin
//    cos
//    tan
//    asin
//    acos
//    atan
//    atan2
//    rand
//    randf
//    srand
//  string manipulation (searching and splitting):
//    split
//    split2
//    find
//    rfind
//    firstof
//    lastof
//    firstnotof
//    lastnotof
//    join
//    join2
//  code and list manipulation
//    tocode -- from list and from bytecode
//    tolist
//    rsetnth -- setnth with opposite arg order for tighter mapnth-style loops
//  loops:
//    zipwith
//    genrec
//  file:
//    eachline
//  networking: -- one option is to just add a few network socket commands and have sockets be files (the linuxy solution)
//
//  IPC: -- one option is to just add pipes (as files) with some wrapper functions, but shared mem (e.g. cross-process ref) might also be nice
//
//  bytecode:
//    compile
//    rcompile
//    _tobytecode
//  debugging: -- some of these may be implemented differently depending on whether we are compiled in debug mode
//    vm.hasnext
//    vm.step
//    vm.next
//    vm.splitnext
//    vm.nextw
//    vm.stats -- only when compiled in debug mode -- returns internal vm counters
//    debuginfo -- name??? only when compiled in debug mode -- takes val, returns attached debug info
//  time: -- need to figure out general solution (right now we use int32 for integers, so using standard unix timestamps only works until 2038)
//    time -- only works until 2038 in 32bit int -- figure out long-term solution (e.g. int64 support, or use int47)
//    time.us -- won't fit in 32bit int -- figure this out
//    localtime -- like perl localtime (which just uses c localtime and extracts struct tm fields into list)
//    gmtime -- like perl gmtime (which just uses c gmtime and extracts struct tm fields into list)
//    sleep
//    usleep
//  system: -- have a way to handle pipes, redirection, collecting output to string, providing input from string
//    system/exec
//  ref:
//    some way to signal/bcast within lock -- more general solution than sigwaitwhile
//      - option: _signal, _bcast -- these would use the current ref from cont by scanning cont until we find bare ref
//
//

err_t concat_init() {
  err_t e;
  if ((e = concat_file_init())) return e;
  return 0;
}

//TODO: should we use qeval, or have separate (possibly global) dictionary for pre-evaluation/noeval-evaluation???

//possible unhandled errors to look for:
//  - locked ref errors (instead of normal exceptions, use lock-specific labels)
//  - open file errors
//  - and ops that pop/push lots of vals

int vm_noeval(vm_t *vm) {
  return vm->noeval;
}
int _vm_qeval(vm_t *vm, valstruct_t *ident, err_t *e) { //pre-evaluate val (handles tokens that bypass noeval)
  //TODO: add word that pushes current line number in file (at least in debug compile)
  //if (val_is_str(val) && __str_ptr(val)->type == TYPE_IDENT) {
    unsigned int len = _val_str_len(ident);
    char c='\0';
    if (len) c = *_val_str_begin(ident);
    if (c=='_') {
      if (0 == _val_str_strcmp(ident,"_quit")) {
        exit(0);
      } else if (0 == _val_str_strcmp(ident,"_list")) {
        *e = vm_list(vm);
        return 1;
      //} else if (0 == _val_str_strcmp(ident,"_debug")) {
      //  *e = vm_debug_wrap(vm);
      //  return 1;
      //} else if (0 == _val_str_strcmp(val,"_clear")) {
      //  *ret = val_stack_clear(vm->open_list);
      //  return 1;
      } else if (0 == _val_str_strcmp(ident,"_pop")) {
        vm->groupi=0;
        vm->noeval=0;
        vm->open_list = &vm->stack;
        *e = vm_drop(vm);
        return 1;
      }
    } else if (len==1 && is_grouping_op(c)) {
      switch(c) {
        case '[': *e = _vm_open_code(vm); return 1;
        case ']': *e = _vm_close_code(vm); return 1;
        case '(': *e = _vm_open_list(vm); return 1;
        case ')': *e = _vm_close_list(vm); return 1;
        //case '{': case '}': return 0;
        default: *e = _fatal(ERR_FATAL); return 1;
      }
    }
  //}
  return 0;
}

void _vm_fix_open_list(vm_t *vm) {
  unsigned int i;
  vm->open_list = &vm->stack;
  vm->noeval=0;
  for(i=0;i<vm->groupi;++i) {
    vm->open_list = __lst_ptr(_val_lst_end(vm->open_list)[-1]);
    if (!vm->noeval && vm->open_list->type == TYPE_CODE) {
      vm->noeval = i+1;
    }
  }
}
err_t _vm_open_list(vm_t *vm) {
  val_t t = val_empty_list();
  err_t r;
  if ((r = _val_lst_rpush(vm->open_list,t))) { val_destroy(t); return r; }
  vm->open_list = __lst_ptr(t);
  vm->groupi++;
  return 0;
}
err_t _vm_open_code(vm_t *vm) {
  val_t t = val_empty_code();
  err_t r;
  if ((r = _val_lst_rpush(vm->open_list,t))) { val_destroy(t); return r; }
  vm->open_list = __lst_ptr(t);
  vm->groupi++;
  if (!vm->noeval) vm->noeval = vm->groupi;
  return 0;
}
err_t _vm_close_list(vm_t *vm) {
  throw_if(ERR_UNEXPECTED_EOL,!vm->groupi || vm->open_list->type != TYPE_LIST);
  vm->groupi--;
  _vm_fix_open_list(vm);
  return 0;
}
err_t _vm_close_code(vm_t *vm) {
  throw_if(ERR_UNEXPECTED_EOC,!vm->groupi || vm->open_list->type != TYPE_CODE);
  vm->groupi--;
  _vm_fix_open_list(vm);
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
  ANNOTATE_HAPPENS_AFTER(vm);
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
  if (!(r = sem_trywait(&vm->lock))) {
    return 0;
  } else if (errno == EAGAIN) return ERR_LOCKED;
  else return _throw(ERR_LOCK);
}
err_t _vm_unlock(vm_t *vm) {
  throw_if(ERR_LOCK,sem_post(&vm->lock));
  return 0;
}


//FIXME: delete dict_put args as needed (validate)
int vm_dict_put_op(vm_t *vm, opcode_t op) {
  return _val_dict_put_(&vm->dict,opstrings[op],strlen(opstrings[op]),__op_val(op));
}
int vm_dict_put_compile(vm_t *vm, const char *opname, const char *code_str) {
  val_t t = val_empty_code();
  valstruct_t *tv = __code_ptr(t);
  err_t e;
  if ((e = _vm_parse_code(vm->p,code_str,strlen(code_str),tv))) { val_destroy(t); return e; }
  if ((e = vm_val_rresolve(vm,&t))) { val_destroy(t); return e; }

  //if (_val_lst_len(tv) == 1 && !val_is_code(*_val_lst_begin(tv))) { 
  //  val_t thead;
  //  if ((e = _val_lst_lpop(tv,&thead))) { val_destroy(t); return e; }
  //  val_destroy(t);
  //  t = thead;
  //}

  return _val_dict_put_(&vm->dict,opname,strlen(opname),t);
}
int vm_dict_put_(vm_t *vm, const char *opname, val_t val) {
  int r;
  if (0 >= (r = _val_dict_put_(&vm->dict,opname,strlen(opname),val))) val_destroy(val);
  return r;
}
int vm_dict_put(vm_t *vm, valstruct_t *opname, val_t val) {
  int r;
  if (0 >= (r = _val_dict_put(&vm->dict,opname,val))) {
    _val_str_destroy(opname);
    val_destroy(val);
  }
  return r;
}
val_t vm_dict_get(vm_t *vm, valstruct_t *key) {
  return _val_dict_get(&vm->dict,key);
}

err_t vm_val_rresolve(vm_t *vm, val_t *val) {
  err_t e;
  if ((e = vm_val_resolve(vm,val))) return e;
  if (val_is_code(*val) && !_val_lst_empty(__code_ptr(*val))) {
    valstruct_t *lst = __code_ptr(*val);
    if ((e = _val_lst_deref(lst))) return e;
    val_t *p,*end;
    for(p=_val_lst_begin(lst),end=_val_lst_end(lst);p!=end;++p) {
      if ((e = vm_val_rresolve(vm,p))) return e;
    }
  }
  return 0;
}

err_t vm_val_resolve(vm_t *vm, val_t *val) {
  if (!val_is_ident(*val)) return 0;
  valstruct_t *ident = __ident_ptr(*val);
  if (_val_str_escaped(ident)) {
    int levels = _val_str_escaped_levels(ident);
    val_t t;
    err_t e;
    if ((e = val_clone(&t,*val))) return e;
    _val_str_substr(__str_ptr(t),levels,-1);
    val_t def = vm_dict_get(vm,__str_ptr(t));
    val_destroy(t);

    if (val_is_null(def)) return 0; //unknown ident

    if (val_is_code(def) && _val_lst_len(__code_ptr(def))==1) def=*_val_lst_begin(__code_ptr(def)); //quotation with single entry -- dequote

    while (val_is_ident(def)) { //recursively resolve idents
      fatal_if(ERR_FATAL,val_eq(*val,def)); //resolves to itself
      def = vm_dict_get(vm,__ident_ptr(def));
      if (!def) return 0; //couldn't resolve, just give up
      if (val_is_code(def) && _val_lst_len(__code_ptr(def))==1) def=*_val_lst_begin(__code_ptr(def)); //quotation with single entry -- dequote
    }
    if (!(val_ispush(def) || val_is_op(def))) return 0;

    e = val_clone(&t,def);
    while(!e && levels--) {
      e = val_code_wrap(&t);
    }
    if (e) {
      val_destroy(t);
      return e;
    } else {
      val_destroy(*val);
      *val = t;
      return 0;
    }
  } else {
    val_t def = vm_dict_get(vm,ident);
    if (val_is_null(def)) return 0; //unknown ident

    if (val_is_code(def) && _val_lst_len(__code_ptr(def))==1) def=*_val_lst_begin(__code_ptr(def)); //quotation with single entry -- dequote

    while (val_is_ident(def)) { //recursively resolve idents
      fatal_if(ERR_FATAL,val_eq(*val,def)); //resolves to itself
      def = vm_dict_get(vm,__ident_ptr(def));
      if (!def) return 0; //couldn't resolve, just give up
      if (val_is_code(def) && _val_lst_len(__code_ptr(def))==1) def=*_val_lst_begin(__code_ptr(def)); //quotation with single entry -- dequote
    }
    if (!(val_ispush(def) || val_is_op(def))) return 0;

    val_t t;
    err_t e;
    if ((e = val_clone(&t,def))) return e;
    val_destroy(*val);
    *val = t;
    return 0;
  }
}

int vm_empty(vm_t *vm) { return _val_lst_empty(vm->open_list); }
err_t vm_push(vm_t *vm, val_t val) { return _val_lst_rpush(vm->open_list,val); }
err_t vm_wpush(vm_t *vm, val_t val) { return _val_lst_rpush(&vm->work,val); }
err_t vm_cpush(vm_t *vm, val_t val) { return _val_lst_rpush(&vm->cont,val); }
err_t vm_cpush2(vm_t *vm, val_t a, val_t b) { return _val_lst_rpush2(&vm->cont,a,b); }
err_t vm_cpush3(vm_t *vm, val_t a, val_t b, val_t c) { return _val_lst_rpush3(&vm->cont,a,b,c); }

err_t vm_wappend(vm_t *vm, val_t val) {
  if (val_is_lst(val) && __lst_ptr(val)->type == TYPE_CODE && _val_lst_empty(__lst_ptr(val))) {
    //we skip appending empty code
    val_destroy(val);
    return 0;
  } else {
    return _val_lst_lpush(&vm->work,val);
  }
}

err_t vm_pop(vm_t *vm, val_t *val) { return _val_lst_rpop(vm->open_list,val); }
err_t vm_drop(vm_t *vm) { return _val_lst_rdrop(vm->open_list); }

val_t vm_top(vm_t *vm) { //TODO: should we even error check if directly returning val_t??? (or can we return error in val_t)
  if (_val_lst_empty(vm->open_list)) return _throw(ERR_EMPTY);
  return *(_val_lst_end(vm->open_list)-1);
}

//prep vm for trycatch try
//TODO: do we package catch ops into single vals, or just make sure _catch is on top and let _endtry/_catch clean things up appropriately???
// - originally I decided to skip creating extra list vals (and it's a little faster), but that does mean hard to read cont stack and more opening for bugs
//TODO: do something better than cloning entire wstack for trycatch
//  - option 1: push wstack to cont (then just have the try code + endtry on wstack), restore wstack on endtry
//  - option 2: save wstack size before try code, truncate wstack on catch and then push catch code
//  - we don't need to worry about stack because that can be solved with apply around try-catch (which will push it to wstack first)
//  - a future implementation using ropes could avoid this issue -- cheap to save copy and then continue modifying stack
err_t vm_trycatch(vm_t *vm, val_t tryval, val_t catchval) {
  err_t e;
  valstruct_t *tstack, *twork;
  //first save deref'd copy of stack
  if (!(tstack = _valstruct_alloc())) return _throw(ERR_MALLOC);
  _val_lst_clone(tstack,&vm->stack);
  if ((e = _val_lst_deref(tstack))) goto bad_stack;

  //save copy of work stack (with catch handler on top)
  if (!(twork = _valstruct_alloc())) {
    e = _throw(ERR_MALLOC);
    goto bad_stack;
  }
  _val_lst_clone(twork,&vm->work);
  if ((e = _val_lst_rpush(twork, catchval))) goto bad_work; //when we push we will also deref so we only have a copy

  //update vm wstack
  if ((e = _val_lst_rpush(&vm->work,__op_val(OP__endtry)))) goto bad_work; //if we reach here do cleanup
  if ((e = _val_lst_rpush(&vm->work,tryval))) goto bad_work; //push try onto wstack

  //prep continuation stack
  val_t *cp; //new element on cont stack
  if ((e = _val_lst_rextend(&vm->cont, 4, &cp))) goto bad_work;
  cp[0] = __lst_val(twork);
  cp[1] = __lst_val(tstack);
  cp[2] = __int_val(vm->groupi);
  cp[3] = __op_val(OP__catch);
  return 0;

bad_work:
  val_destroy(__lst_val(twork));
bad_stack:
  val_destroy(__lst_val(tstack));
  return e;
}

//after exception, restore state from cont and prep vm to run catch handler
err_t vm_catch(vm_t *vm, val_t err) {
  val_t t;
  err_t e;
  //val_t err;
  //if ((e = _val_lst_rpop(vm->open_list,&err)))
  //e = __val_int(err);
  //if (e == ERR_THROW || e == ERR_USER_THROW) {
  //  //throw, so we need to get the actual exception -- TODO: do we need this, or do we always handle THROW before we get here
  //  if ((e = _val_lst_rpop(vm->open_list,&err))) return _fatal(e);
  //}
  //TODO: rpop3 to reduce error checks

  //restore groupi
  if ((e = _val_lst_rpop(&vm->cont,&t))) return _fatal(e);
  vm->groupi = __val_int(t);

  //restore stack contents
  if ((e = _val_lst_rpop(&vm->cont,&t))) return _fatal(e);
  _val_lst_swap(&vm->stack,__lst_ptr(t));
  val_destroy(t);

  //restore wstack (with catch handler pushed)
  if ((e = _val_lst_rpop(&vm->cont,&t))) return _fatal(e);
  _val_lst_swap(&vm->work,__lst_ptr(t));
  val_destroy(t);

  _vm_fix_open_list(vm);
  //reserve 2 on stack so we can push err and still have the required 1 free space
  if ((e = _val_lst_rreserve(vm->open_list,2))) return _fatal(e);
  _val_lst_rpush(vm->open_list, err);

  return 0;
}

int vm_hascont(vm_t *vm) {
  return !_val_lst_empty(&vm->cont);
}
err_t vm_pushcw(vm_t *vm) {
  err_t e;
  val_t cont;
  if ((e = _val_lst_rpop(&vm->cont,&cont))) return e;
  //if (val_is_lst(cont) && __lst_ptr(cont)->type == TYPE_CODE && _val_lst_empty(__lst_ptr(cont))) goto badargs_cont;
  if ((e = vm_wpush(vm,cont))) goto bad_cont;
  return 0;
//badargs_cont:
//  e = _throw(ERR_BADARGS);
bad_cont:
  val_destroy(cont);
  return e;
}

int vm_stack_printf(vm_t *vm, const char *format) {
  return val_printfv_(format, strlen(format), vm->open_list,1);
}
//int vm_printf_(vm_t *vm, const char *format, int len) {
//  return val_fprintf(stdout, vm->open_list,1, vm->open_list,1, format, len);
//}

//TODO: global dict val that vms inheret child scope of OR separate op dict per-vm???
// - A gives faster+smaller VM init, B reduces scope level of ops by 1 (right now we do B)
// - another option (requiring more changes) would be ref clones in child VM, with parent dict becoming CoW if modified before child completes
//   - could either use CoW, or add "write layer" scope on write if there are multiple refs
//     - "write layer" would be transparent, and collapsable once ref to parent layer becomes singleton
//     - popping scope would pop all "write layers" + 1
//   - this might be the best balance of fast dict cloning, immutable inherited dicts (so we don't need them to be threadsafe), and no need to lock or mark readonly when dict has multiple refs
//     - adds extra scope dereferencing, so slows down dict in some cases, but also doesn't require locking or deep copying

//TODO: probably instead just error if a put fails (allow overwrite dictionary, at least as option)
err_t _vm_init_dict(vm_t *vm) {
  //TODO: make initial dict entries const structs (save us some VM startup time)
  //TODO: consider evaluating hardcoded initialization bytecode to build dict and prep vm
  err_t e;
  val_t t;

  //
  //Dictionary entries for opcodes
  //
  if (0>(e = vm_dict_put_op(vm,OP_break))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_eval))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_parsecode))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_parsecode_))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_pop))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_swap))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_dup))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_dup2))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_dup3))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_dupn))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_dign))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_buryn))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_flipn))) goto out_err;


  if (0>(e = vm_dict_put_op(vm,OP_empty))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_small))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_size))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_lpop))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_lpush))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_rpop))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_rpush))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_cat))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_rappend))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_splitn))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_strhash))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_getbyte))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_setbyte))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_first))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_last))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_nth))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_rest))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_dfirst))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_dlast))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_dnth))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_swapnth))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_setnth))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_collapse))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_restore))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_expand))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_sort))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_rsort))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_clearlist))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_quote))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_wrap))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_wrapn))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_protect))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_add))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_sub))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_mul))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_div))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_inc))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_dec))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_neg))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_abs))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_sqrt))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_log))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_pow))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_mod))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_bit_and))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_bit_or))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_bit_xor))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_bit_not))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_bit_lshift))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_bit_rshift))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_lt))) goto out_err;
  if (0>(e = vm_dict_put_(vm,"lt",__op_val(OP_lt)))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_le))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_gt))) goto out_err;
  if (0>(e = vm_dict_put_(vm,"gt",__op_val(OP_gt)))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_ge))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_eq))) goto out_err;
  if (0>(e = vm_dict_put_(vm,"eq",__op_val(OP_eq)))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_ne))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_compare))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_bool))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_not))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_and))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_and_))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_or))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_or_))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_find))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_parsenum))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_toint))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_tofloat))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_tostring))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_toident))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_substr))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_trim))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_isnum))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_isint))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_isfloat))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_isstring))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_isident))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_isnative))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_islist))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_iscode))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_islisttype))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_isref))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_isdict))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_isfile))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_isvm))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_ispush))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_dip))) goto out_err;
  //if (0>(e = vm_dict_put_op(vm,OP_dip2))) goto out_err;
  //if (0>(e = vm_dict_put_op(vm,OP_dip3))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_dipn))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_sip))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_sip2))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_sip3))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_sipn))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_0apply))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_1apply))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_2apply))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_3apply))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_napply))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_if))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_if_))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_ifelse))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_ifelse_))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_only))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_unless))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_swaplt))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_swapgt))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_list))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_print))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_print_))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_printV))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_printV_))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_printf))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_fprintf))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_sprintf))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_printlf))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_sprintlf))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_printlf2))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_sprintlf2))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_clear))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_qstate))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_vstate))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_defined))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_getdef))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_def))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_mapdef))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_resolve))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_rresolve))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_scope))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_savescope))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_usescope))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_usescope_))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_dict_has))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_dict_get))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_dict_put))) goto out_err;

  t = val_file_stdin_ref();
  if ((e = val_code_wrap(&t))) goto out_err;
  if (0>(e = vm_dict_put_(vm,"stdin",t))) goto out_err;
  t = val_file_stdout_ref();
  if ((e = val_code_wrap(&t))) goto out_err;
  if (0>(e = vm_dict_put_(vm,"stdout",t))) goto out_err;
  t = val_file_stderr_ref();
  if ((e = val_code_wrap(&t))) goto out_err;
  if (0>(e = vm_dict_put_(vm,"stderr",t))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_open))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_close))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_readline))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_stdin_readline))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_read))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_write))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_seek))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_fpos))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_ref))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_deref))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_refswap))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_guard))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_guard_sig))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_guard_bcast))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_guard_waitwhile))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_guard_sigwaitwhile))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_wait))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_signal))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_broadcast))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_vm))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_thread))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_debug))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_vm_exec))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_vm_continue))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_vm_thread))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_vm_stack))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_vm_wstack))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_vm_setstack))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_vm_wsetstack))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_vm_qstate))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_vm_vstate))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_trycatch))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_trydebug))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_throw))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_perror))) goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_open_code))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_open_list))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_close_list))) goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_quit)))goto out_err;

  if (0>(e = vm_dict_put_op(vm,OP_debug_eval)))goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_debug_noeval)))goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_debug_set)))goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_debug_get)))goto out_err;

  //
  //implementation-specific natives (opcodes after OP_quit)
  //
  if (0>(e = vm_dict_put_op(vm,OP_fork)))goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_socket)))goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_socket_listen)))goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_socket_accept)))goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_socket_connect)))goto out_err;
  if (0>(e = vm_dict_put_op(vm,OP_effects)))goto out_err;

  //
  //named constants
  //
  if (0>(e = vm_dict_put_(vm,"true",val_true))) goto out_err;
  if (0>(e = vm_dict_put_(vm,"false",val_false))) goto out_err;
  if (0>(e = vm_dict_put_(vm,"pi",__dbl_val(3.14159265358979323846)))) goto out_err;
  if (0>(e = vm_dict_put_(vm,"e",__dbl_val(2.71828182845904523536)))) goto out_err;


  //
  //basic dictionary of compiled quotations
  //
  //TODO: ops for non-printing printf funcs???
  if (0>(e = vm_dict_put_compile(vm,"printlf_","sprintlf pop")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"printlf2_","sprintlf2 pop")))goto out_err;

  //TODO: second/third opcodes???
  if (0>(e = vm_dict_put_compile(vm,"second","2 nth")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"third","3 nth")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"dsecond","2 dnth")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"dthird","3 dnth")))goto out_err;


  if (0>(e = vm_dict_put_compile(vm,"fourth","4 nth")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"fifth","5 nth")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"sixth","6 nth")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"seventh","7 nth")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"eigth","8 nth")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"ninth","9 nth")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"tenth","10 nth")))goto out_err;

  if (0>(e = vm_dict_put_compile(vm,"dfourth","4 dnth")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"dfifth","5 dnth")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"dsixth","6 dnth")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"dseventh","7 dnth")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"deigth","8 dnth")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"dninth","9 dnth")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"dtenth","10 dnth")))goto out_err;

  //TODO: opcodes for dup/pop top several items???
  if (0>(e = vm_dict_put_compile(vm,"2dup","dup2 dup2")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"3dup","dup3 dup3 dup3")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"2pop","pop pop")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"3pop","pop pop pop")))goto out_err;

  //TODO: opcodes for dip'd pop/dup/swap???
  if (0>(e = vm_dict_put_compile(vm,"popd","swap pop")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"dupd","\\dup dip")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"swapd","\\swap dip")))goto out_err;

  //TODO: opcodes common list manipulation (wrap2,wrap3,mapnth)???
  if (0>(e = vm_dict_put_compile(vm,"wrap2","wrap lpush")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"wrap3","wrap lpush lpush")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"mapnth","dup [ swapd 0 swap swapnth bury2 \\eval dip ] dip swapd setnth")))goto out_err;


  //various interconstructions
  //TODO: opcodes for dip2/dip3???
  if (0>(e = vm_dict_put_compile(vm,"dip2","2 dipn")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"dip3","3 dipn")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"dipe","dip eval")))goto out_err; //TODO: opcode for tail calls
  //if (0>(e = vm_dict_put_compile(vm,"and","swap dip only bool")))goto out_err;
  //if (0>(e = vm_dict_put_compile(vm,"or","swap dip unless bool")))goto out_err;

  if (0>(e = vm_dict_put_compile(vm,"apply","[\\expand dip eval] 2apply"))) goto out_err;

  //TODO: common dig/bury/flip opcodes???
  //TODO: shuffle op??? -- would take 32bit int and rewrite e.g. the top 4-8 items of stack (could handle all shuffling of top few items with one op)
  if (0>(e = vm_dict_put_compile(vm,"dig2","2 dign")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"dig3","3 dign")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"bury2","2 buryn")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"bury3","3 buryn")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"flip3","3 flipn")))goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"flip4","4 flipn")))goto out_err;


  //TODO: at least basic loop opcodes
  if (0>(e = vm_dict_put_compile(vm,"each","[ dup3 size [ [ \\lpop dip dup dip2 ] dip dup eval ] if ] dup eval pop pop pop"))) goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"eachr","[ dup3 size [ [ \\rpop dip dup dip2 ] dip dup eval ] if ] dup eval pop pop pop"))) goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"while","[ dup3 dip3 dig3 [ [ dup dip2 ] dip dup eval ] if ] dup eval pop pop pop"))) goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"times","[ dup3 0 > [ [ \\dec dip dup dip2 ] dip dup eval ] if ] dup eval pop pop pop"))) goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"loop_","[[dup dip] dip dup eval] dup eval"))) goto out_err;


  //TODO: at least some of these loop words should probably be ops, which ones???
  if (0>(e = vm_dict_put_compile(vm,"filter","dup2 clearlist flip3 [ flip3 dup3 flip3 dup dip3 [ dig2 \\rpush \\popd ifelse] dip ] each pop"))) goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"filter2","[ dup clearlist dup flip3 ] dip swap [ bury3 \\dup dip3 dup 4 dipn 4 dign [ \\rpush dip2 ] [ [ swapd rpush ] dip ] ifelse ] each pop"))) goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"map","dup2 clearlist flip3 [ bury2 dup dip2 \\rpush dip ] each pop"))) goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"mapr","dup2 clearlist flip3 [ bury2 dup dip2 \\lpush dip ] eachr pop"))) goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"mmap","dup2 clearlist flip3 [ bury2 dup dip2 \\rappend dip ] each pop"))) goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"cleave","dup clearlist swap [ swap [sip swap] dip rpush ] each swap pop"))) goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"spread","dup clearlist swap dup size swap [ dup2 inc dipn dup inc dign dig2 rpush swap dec ] each pop"))) goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"bi","[ dup2 \\eval dip ] dip eval"))) goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"tri","[ dup2 \\eval dip ] dip2 [ dup2 \\eval dip ] dip eval"))) goto out_err;
  
  if (0>(e = vm_dict_put_compile(vm,"findp","0 1 [ [ inc dup3 empty [ pop 0 0 ] 1 ifelse ] 0 ifelse ] [ \\lpop dip2 dup2 dip3 dig3 not ] while popd popd dec"))) goto out_err;
  //FIXME: sorted_findp
  if (0>(e = vm_dict_put_compile(vm,"sorted_findp","0 dup3 size [ dup 0 > ] [ dup 2 / [ - dec ] sip dup 4 buryn [ splitn lpop swapd ] 4 dipn [ dup dip3 ] dip3 6 dign dup 0 = [ pop popd + \\3pop dip -1 ] [ 0 < [ swap [ + inc ] dip 4 dign pop ] [ popd dig3 pop ] ifelse_ ] ifelse_ ] while 0 = [ 3pop -1 ] if_"))) goto out_err;

  if (0>(e = vm_dict_put_compile(vm,"evalr","[dup2 ispush [swap dip dup eval] unless] dup eval pop pop"))) goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"linrec","dig2 [ [ 4 dupn 4 dipn 4 dign not [ inc dup3 4 dipn dup2 eval ] if ] 0 dup2 eval [ pop pop pop ] dip ] dip2 dip2 times"))) goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"binrec","[ 5 dupn 5 dipn 5 dign [ 4 dupn 5 dipn ] [ dup3 5 dipn 5 dign dup2 dip 5 buryn dup eval dup2 5 dipn ] ifelse ] dup eval pop pop pop pop pop"))) goto out_err;

  if (0>(e = vm_dict_put_compile(vm,"openr","\"r\" open"))) goto out_err;
  //TODO: use include searchpath (which also should be modifiable) when opening file for include
  if (0>(e = vm_dict_put_compile(vm,"include","fopenr eval"))) goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"writeline","\"\n\" cat write"))) goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"eachline","swap [ readline dup isint not ] [ bury2 dup2 dip2 ] while [pop pop] dip"))) goto out_err; //TODO: dup eval impl???
  if (0>(e = vm_dict_put_compile(vm,"isferr","0 <"))) goto out_err;
  if (0>(e = vm_dict_put_compile(vm,"isfeof","-18 ="))) goto out_err;

  //if (0>(e = vm_dict_put_compile(vm,"vm.next","vm.stack lpop [ dup iscode [ ] [ ] ifelse ..."))) goto out_err;
  return 0;
out_err:
  return e;
}

err_t vm_init(vm_t *vm) {
  //TODO: clean error handling (and cleanup)
  sem_init(&vm->lock,0,1);
  _val_list_init(&vm->stack);
  _val_list_init(&vm->work);
  _val_list_init(&vm->cont);
  vm->open_list = &vm->stack;
  vm->groupi=0;
  vm->noeval=0;
#ifdef DEBUG_VAL_EVAL
  vm->debug_val_eval = 0;
#endif
  vm->state = STOPPED;

  if (!(vm->p = vm_get_parser())) return _throw(ERR_NO_PARSER);

  int e;
  if ((e = _val_dict_init(&vm->dict))) return e;
  if ((e = _vm_init_dict(vm))) return e;

  return 0;
}
err_t vm_init2(vm_t *vm, valstruct_t *stack, valstruct_t *work) {
  sem_init(&vm->lock,0,1);
  vm->stack = *stack; _valstruct_release(stack);
  vm->work = *work; _valstruct_release(work);
  _val_list_init(&vm->cont);
  vm->open_list = &vm->stack;
  vm->groupi=0;
  vm->noeval=0;
  vm->state = STOPPED;

  if (!(vm->p = vm_get_parser())) return _throw(ERR_NO_PARSER);

  int e;
  if ((e = _val_dict_init(&vm->dict))) return e;
  if ((e = _vm_init_dict(vm))) return e;

  return 0;
}
err_t vm_init3(vm_t *vm, valstruct_t *stack, valstruct_t *work, valstruct_t *dict) {
  sem_init(&vm->lock,0,1);
  vm->stack = *stack; _valstruct_release(stack);
  vm->work = *work; _valstruct_release(work);
  vm->dict = *dict; _valstruct_release(dict);
  vm->open_list = &vm->stack;
  vm->groupi=0;
  vm->noeval=0;
  vm->state = STOPPED;

  if (!(vm->p = vm_get_parser())) return _throw(ERR_NO_PARSER);
  return 0;
}
void vm_clone(vm_t *vm, vm_t *orig) {
  sem_init(&vm->lock,0,1);
  _val_lst_clone(&vm->stack,&orig->stack);
  _val_lst_clone(&vm->work,&orig->work);
  _val_dict_clone_(&vm->dict,&orig->work);
  vm->groupi=orig->groupi;
  _vm_fix_open_list(vm);
  vm->state = STOPPED;
  vm->p = orig->p;
}
void vm_destroy(vm_t *vm) {
  int mustjoin=0;
  err_t e;
  if (ERR_LOCKED == (e = _vm_trylock(vm))) {
    //vm currently running, need to kill
    if ((e = _vm_cancel(vm))) _fatal(e); return;
    mustjoin = 1;
  } else if (e) {
    _fatal(e); return;
  } else { //we got lock, wait for thread if needed
    if (vm->state == FINISHED) mustjoin = 1;
  }
  if (mustjoin) {
    e = _vm_join(vm);
  }
  _val_lst_destroy_(&vm->stack);
  _val_lst_destroy_(&vm->work);
  _val_lst_destroy_(&vm->cont);
  _val_dict_destroy_(&vm->dict);
  sem_destroy(&vm->lock);
  ANNOTATE_HAPPENS_BEFORE_FORGET_ALL(vm);
  //if (e == ERR_VM_CANCELLED) {
  //  return 0;
  //} else {
  //  return e;
  //}
}

err_t vm_validate(vm_t *vm) {
  err_t e;
  if ((e = val_validate(__lst_val(&vm->stack)))) return e;
  if ((e = val_validate(__lst_val(&vm->work)))) return e;
  if ((e = val_validate(__lst_val(&vm->cont)))) return e;
  if ((e = val_validate(__lst_val(&vm->dict)))) return e;
  return 0;
}
err_t _vm_validate(vm_t *vm, val_t *stack, unsigned int stackn, val_t *work, unsigned int workn, int state, val_t top) {
  err_t e;
  if ((e = val_validaten(stack,stackn))) return e;
  if ((e = val_validaten(work,workn))) return e;
  if ((e = val_validate(__lst_val(&vm->cont)))) return e;
  if ((e = val_validate(__dict_val(&vm->dict)))) return e;

  if (state < 0 || state > 2) return _throw(ERR_BADTYPE);
  if (state>0) return val_validate(top);
  else return 0;
}

err_t vm_reset(vm_t *vm) {
  _val_lst_clear(&vm->work);
  _val_lst_clear(&vm->cont);
  //NOTE: doesn't clear stack (but does reset groupi)
  //int r;
  //if ((r = val_dict_pop_to1(&vm->dict))) return r; //TODO: re-init dict
  //vm->scope = val_dict_h(&vm->dict);
  
  vm->groupi=0;
  vm->noeval=0;
  vm->open_list = &vm->stack;
  return 0;
}


int vm_parse_input(vm_t *vm, const char *str, int len, valstruct_t *code) {
  return _vm_parse_input(vm->p,str,len,code);
}
int vm_parse_code(vm_t *vm, const char *str, int len, valstruct_t *code) {
  return _vm_parse_code(vm->p,str,len,code);
}

int vm_list(vm_t *vm) {
  //return val_stack_list(vm->open_list);
  if (!vm_empty(vm)) {
    err_t e;
    if (0>(e = val_list_fprintf_(vm->open_list,stdout,list_fmt_lines,fmt_V))) return e;
    if (0>(e = val_fprint_(stdout,"\n",1))) return e;
  }
  return 0;
}
int vm_listn(vm_t *vm, unsigned int n) {
  //return val_stack_listn(vm->open_list,n);
  list_fmt_t lfmt;
  val_list_format_listn(&lfmt,n);
  return val_list_fprintf_(vm->open_list,stdout,&lfmt,fmt_V);
}
//print stack as list (this is used for print stack on exit)
err_t vm_stackline(vm_t *vm) {
  err_t e;
  //if (0 > (e = val_list_fprintf_(vm->open_list,stdout,list_fmt_V,fmt_V))) return e;
  if (0 > (e = val_fprintf(stdout, "%V\n", __lst_val(vm->open_list)))) return e;
  return 0;
}

err_t vm_printstate(vm_t *vm) {
  printf("Stack:\n");
  vm_list(vm);
  printf("\nWork:\n");
  val_list_fprintf_(&vm->work,stdout,list_fmt_rlines,fmt_V);
  printf("\nCont:\n");
  val_list_fprintf_(&vm->cont,stdout,list_fmt_rlines,fmt_V);
  return 0;
}

err_t vm_qstate(vm_t *vm) {
  fmt_t fmt = *fmt_V;
  fmt.precision = 120;
  err_t r;
  val_t buf = val_empty_string(); //sprint to buf first to help with threaded debugging
  valstruct_t *bufv = __str_ptr(buf);
  if (0>(r = val_sprint_cstr(bufv,"State: "))) goto out;
  if (0>(r = vm_sprintf(vm,bufv,&fmt))) goto out;
  if (0>(r = val_sprint_ch(bufv,'\n'))) goto out;
  r = val_string_fprintf(bufv,stdout,fmt_v);
out:
  val_destroy(buf);
  return r >= 0 ? 0 : r;
}

err_t _vm_qstate(vm_t *vm,val_t *stack, unsigned int stackn, val_t *work, unsigned int workn, int state, val_t top) {
  fmt_t fmt = *fmt_V;
  fmt.precision = 120;
  err_t r;
  val_t buf = val_empty_string(); //sprint to buf first to help with threaded debugging
  valstruct_t *bufv = __str_ptr(buf);
  if (0>(r = val_sprint_cstr(bufv,"State: "))) goto out;
  if (0>(r = _vm_sprintf(bufv,&fmt,stack,stackn,work,workn,vm->cont.v.lst.buf ? _val_lst_begin(&vm->cont) : NULL, _val_lst_len(&vm->cont), state>0 ? 1 : 0, top))) goto out;
  if (0>(r = val_sprint_ch(bufv,'\n'))) goto out;
  r = val_string_fprintf(bufv,stdout,fmt_v);
out:
  val_destroy(buf);
  return r >= 0 ? 0 : r;
}

err_t vm_vstate(vm_t *vm) {
  err_t r;
  if (0>(r = val_fprint_cstr(stdout,"State: "))) return r;
  if (0>(r = vm_fprintf(vm,stdout,fmt_V))) return r;
  if (0>(r = val_fprint_(stdout,"\n",1))) return r;
  return 0;
}

err_t _vm_vstate(vm_t *vm,val_t *stack, unsigned int stackn, val_t *work, unsigned int workn, int state, val_t top) {
  int r; 
  val_t buf = val_empty_string(); //sprint to buf first to help with threaded debugging
  valstruct_t *bufv = __str_ptr(buf);
  if (0>(r = val_sprint_cstr(bufv,"State: "))) goto out;
  if (0>(r = _vm_sprintf(bufv,fmt_V,stack,stackn,work,workn,vm->cont.v.lst.buf ? _val_lst_begin(&vm->cont) : NULL, _val_lst_len(&vm->cont), state>0 ? 1 : 0, top))) goto out;
  if (0>(r = val_sprint_ch(bufv,'\n'))) goto out;
  r = val_string_fprintf(bufv,stdout,fmt_v);
out:
  val_destroy(buf);
  return r >= 0 ? 0 : r;
}

int vm_fprintf(vm_t *vm, FILE *file, const fmt_t *fmt) {
  err_t r;
  val_t buf = val_empty_string();
  if (0>(r = vm_sprintf(vm,__str_ptr(buf),fmt))) goto out;
  r = val_string_fprintf(__str_ptr(buf),file,fmt_v);
out:
  val_destroy(buf);
  return r;
}

int vm_sprintf(vm_t *vm, valstruct_t *buf, const fmt_t *fmt) {
  unsigned int stackn = _val_lst_len(&vm->stack), workn = _val_lst_len(&vm->work), contn = _val_lst_len(&vm->cont);
  return _vm_sprintf(buf,fmt,
      stackn ? _val_lst_begin(&vm->stack) : NULL, stackn,
      workn ? _val_lst_begin(&vm->work) : NULL, workn,
      contn ? _val_lst_begin(&vm->cont) : NULL, contn,
      0);
}

//FIXME: if top of stack is list, doesn't get proper truncation
int _vm_sprintf(valstruct_t *buf, const fmt_t *fmt, val_t *stack, unsigned int stackn, val_t *work, unsigned int workn, val_t *cont, unsigned int contn, unsigned int topn, ...) {
  err_t r;
  int rlen=0;
  const char *stacksep = "  <|>  "; //separator between stack and work stack
  int stackseplen = 7;

  //if (vm->threadid>=0) {
  //  if (0>(r=val_sprint_cstr(buf,"tid:"))) return r; rlen += r;
  //  if (0>(r=_val_int_sprintf(vm->threadid,buf,fmt_v))) return r; rlen += r;
  //  if (0>(r=val_sprint_cstr(buf," "))) return r; rlen += r;
  //}


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
    if (topn) {
      va_list topvals;
      va_start(topvals, topn);
      if (0>(rs = val_listp_sprintf_extra(NULL,stack,stackn,&lfmt,fmt_V,topn,topvals))) return rs;
      va_end(topvals);
    } else {
      if (0>(rs = val_listp_sprintf(NULL,stack,stackn,&lfmt,fmt_V))) return rs;
    }
    if (0>(rw = val_listp_sprintf(NULL,work,workn,&wfmt,fmt_V))) return rw;
    if (rs+rw>stacklim) { //need truncation, choose appropriate limits for each stack
      //we want to use all available chars, and each stack should be guaranteed at least stacklim/2
      if (rw <= stacklim/2) { //just need to truncate rs (stack)
        lfmt.max_bytes = stacklim-rw;
        if (lfmt.max_bytes < 3) lfmt.max_bytes = 3; //at least 3 chars for empty or ...
        //sublist_fmt.max_bytes = lfmt.max_bytes/2;
        //if (sublist_fmt.max_bytes < 3) sublist_fmt.max_bytes = 3;
      } else if (rs <= stacklim/2) { //just need to truncate rw (work stack)
        wfmt.max_bytes = stacklim-rs;
        if (wfmt.max_bytes < 3) wfmt.max_bytes = 3; //at least 3 chars for empty or ...
        //wsublist_fmt.max_bytes = wfmt.max_bytes/2;
        //if (wsublist_fmt.max_bytes < 3) wsublist_fmt.max_bytes = 3;
      } else { //truncate both
        lfmt.max_bytes = (stacklim+1)/2; //if stacklim is odd, lfmt gets the extra
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

  if (topn) {
    va_list topvals;
    va_start(topvals, topn);
    if (0>(r = val_listp_sprintf_extra(buf,stack,stackn,&lfmt,fmt_V,topn,topvals))) return r; rlen += r;
    va_end(topvals);
  } else {
    if (0>(r = val_listp_sprintf(buf,stack,stackn,&lfmt,fmt_V))) return r; rlen += r;
  }
  if (0>(r = val_sprint_(buf,stacksep,stackseplen))) return r; rlen += r;
  if (fmt->precision >= 0 && rlen+wfmt.max_bytes > fmt->precision) { //for threading -- in case lengths changed
    wfmt.max_bytes = fmt->precision - rlen;
    if (wfmt.max_bytes<3) wfmt.max_bytes=3;
  }
  if (0>(r = val_listp_sprintf(buf,work,workn,&wfmt,fmt_V))) return r; rlen += r;
  //debug_assert_r(fmt->precision < 0 || rlen <= fmt->precision); //doesn't play nice with threading


  //if (lfmt.max_bytes > 0 && (fmt->precision-stackseplen)%2) lfmt.max_bytes++; //if limit odd, add extra 1 to stack
  return rlen;

}

int vm_finished(vm_t *vm) {
  return _val_lst_empty(&vm->work);
}

static void * _vm_thread(void *arg) {
  err_t e;
  vm_t *vm = (vm_t*)arg;
  //FIXME: use deferred cancellation and/or clean-up handlers and/or data destructors so state after cancel is determinate and valid
  //  - see pthread_cancel(3)
  if ((e = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL))) {
    e = _throw(ERR_THREAD);
  } else {
    e = vm_dowork(vm);
    ANNOTATE_HAPPENS_BEFORE(vm);
  }
  vm->state = FINISHED;
  _vm_unlock(vm);
  return (void*)(intptr_t)e;
}
err_t vm_runthread(vm_t *vm) {
  static int _nextid = 0;
  fatal_if(ERR_LOCK,_vm_lock(vm));
  throw_if(ERR_BADARGS,vm->state != STOPPED);
  vm->state = RUNNING;
  vm->threadid = refcount_inc(_nextid);
  err_t e;
  if ((e = pthread_create(&vm->thread,NULL,_vm_thread,vm))) {
    vm->state = STOPPED;
    e = _throw(ERR_THREAD);
    _vm_unlock(vm);
  }
  //else {
  //  pthread_detach(vm->thread);
  //}
  return e;
}


err_t _vm_reserve(valstruct_t *stackval, val_t **stackbase, val_t **stack, val_t **stackend) {
  stackval->v.lst.len = *stack-*stackbase; //first we need to fix length (we just use the pointers during fast stack manips)
  err_t e;
  unsigned int rspace = _val_lst_len(stackval)/2;
  if (rspace < 4) rspace=4; //TODO: select good initialization and minimum values

  unsigned int cur_rspace = stackval->v.lst.buf==NULL ? 0 : _val_lst_size(stackval) - _val_lst_len(stackval) - _val_lst_offset(stackval);

  //FIXME: don't always need cleanderef, just for sprintf and aother times we can muck up stack
  if (rspace <= cur_rspace) {
    _val_lst_cleanderef(stackval);
  } else { //need more space -- realloc
    if ((e = _val_lst_realloc(stackval,1,rspace))) return e; //TODO: do/when do we need lspace -1???
  }
  *stackbase = _val_lst_begin(stackval);
  *stack = _val_lst_end(stackval);
  *stackend = _val_lst_bufend(stackval);
  return 0;
}

// vm_dowork(vm) - evaluates work stack until empty or error
// 
// This function is the core vm work loop.
// We keep pointers 
//
//
err_t vm_dowork(vm_t *vm) {
#define OP_LABEL0(op,opstr,effects) &&op_##op##_0
#define OP_LABEL1(op,opstr,effects) &&op_##op##_1
#define OP_LABEL2(op,opstr,effects) &&op_##op##_2
  const void *_ops[] = { OPCODES(OP_LABEL0), OPCODES(OP_LABEL1), OPCODES(OP_LABEL2) };
#undef OP_LABEL0
#undef OP_LABEL1
#undef OP_LABEL2
  const void **stateops[] = { _ops, _ops+N_OPS, _ops+2*N_OPS };

  // - my main goals were: keep it short, minimize number of conditionals in common cases, minimize CPU instructions-per-op
  //   - we duplicate some portions to eliminate some checks we always need otherwise, but want to balance code bloat and instructions-per-op
  //   - (mostly) duplicate eval handlers for in code / evaling vals directly from stack -- optimize instr/op when wtop is quotation
  // - towards that end I've implemented dowork as a 3-state VM
  //   - state 0: unknown stack state, but at least 2 free slots in stack
  //   - state 1: top of stack in top, rest of stack unknown but at least 1 slot free
  //   - state 2: top of stack in top, stackn>0, unknown stack space
  //   - every opcode needs to be implemented for each state (though some are the same, and most fall through to eachother)
  // - with 3 states, many opcodes have at least 1 state where they run free wrt the stack (no stack checks)
  //   - we still need typechecks because this is the pure unoptimized interpreter (0-2 bit twiddles + 1 conditional for the main types)
  //     - with pre-optimized code (at least where typesafety can be guaranteed), can skip typechecks and run direct/token-threaded code
  //   - the less-convenient cases can be coerced to the desired state and then fall through in many cases (so << 3X the code for 3 labels per op)
  //     - this tends to result in suboptimal code in some cases (that may not be compiler-optimizable), but we can later optimize the ones that matter
  //       - e.g. extra jumps in code, instead of duplicating an instruction
  //   - the cost for reducing conditionals on the stack is that we need to update state during most ops (so we can then jump to the appropriate state)
  //     - TODO: document some benchmarking between 1/2/3 state vm (I tried 1 and 3, 3 was significantly faster, but 2 was never tested)
  //
  // TODO: reconsider code structure (debug step, rextend, top2 vars)
  //   - consider registers for the top 2 in the stack instead of just top (would need profiling to support it)
  //   - how do we implement single-stepping (something better than slice, save rest and insert break?) -- debug_val solves this I think
  //   - use *_rextend style for push/wpush/cpush (still treat top special) -- lets us push all error checking to the top to simplify leak prevention and also allows us to later skip error checks (and allocations) when safe
  //
  // TODO: consider abstract interpreter VM
  //   - would determine stack effects just by evaluation
  //     - i.e. abstract VM performs abstract evaluation of stack effects
  //   - this could give the best performance, which may allow for some interesting tools
  //     - e.g. live bug checking and coding assistance
  //   - implement by adding abstract_vm val type (and a few opcodes to work with it)
  //   - maintains an abstract state and tags each val with an abstract type
  //   - support for various abstract domains for each concrete/abstract type
  //     - also a way to tie-in custom domains
  //   - widening operators to control granularity
  //
  //
  //

  //dowork - this is the core VM evaluation function -- it evaluates until it finishes, hits a stop point (like [break]), or throws an unhandled exception
  // - based around token-threaded dispatch, with mostly (heavier) token threading used for normal evaluation, and pure token threading used for bytecode evaluation
  //   - i.e. vm opcode execution consists of calls like `goto lookuptable[opcode]`
  //   - in the best case a switch statement should be just as fast, and more readable and maintainable, but in practice (based on benchmarking the original impl) is significantly slower and doesn't let us play tricks like shortcutting loops, which can save siginificant numbers of CPU cycles



  //TODO: explicitly clean stackval & workval, keep them clean, then just clear them before leaving (or switching stacks) -- don't need to clear vals as we work
  err_t e,ee; //ee for temp use in error handlers where we still need to return e

  val_t top; //register for top item in stack (if state>0, top contains the top of stack, stack[-1] contains the second item)
  int state=0; //zero items in registers

  //now we get the stack valstruct pointers and stack base/end/current pointers
  // - since most ops in concat are just working with the last few stack/workstack
  //   elements, makes sense to optimize here
  // - this means any time the vm changes workval/stackval, call the appropriate
  //   RESTORESTACK macro

  valstruct_t *stackval = vm->open_list;
  val_t *stackbase=(stackval->v.lst.buf ? _val_lst_begin(stackval): NULL), *stack=stackbase+_val_lst_len(stackval), *stackend;
  if (!stackbase || (stackval->v.lst.buf->refcount>1) || stack==_val_lst_bufend(stackval)) {
    if ((e = _vm_reserve(stackval,&stackbase,&stack,&stackend))) return e;
  } else {
    stackend = _val_lst_bufend(stackval);
  }

  valstruct_t *workval = &vm->work;
  val_t *workbase=(workval->v.lst.buf ? _val_lst_begin(workval) : NULL), *work=workbase+_val_lst_len(workval), *workend;
  if (!workbase || (workval->v.lst.buf->refcount>1) || work==_val_lst_bufend(workval)) {
    if ((e = _vm_reserve(workval,&workbase,&work,&workend))) return e;
  } else {
    workend = _val_lst_bufend(workval);
  }

  //temp vars
  val_t t;
#ifdef DEBUG_VAL
  val64_t dbg; //debug val
#ifdef DEBUG_VAL_EVAL
  int debug_val_eval = vm->debug_val_eval; //whether we should eval debug val in place of regular val
#endif
#endif
  valstruct_t *v,*tv;
  val_t *p;

  //unsigned int tui;
  //int ti;
  int i,n;
  double f;
  vm_t *d;
  pid_t pid;


  //val_t *codep,*codeend; //pointers for iterating through code val



#define STATE_0 do{state=0;}while(0)
#define STATE_1 do{state=1;}while(0)
#define STATE_2 do{state=2;}while(0)

#define SET_LOOP_RETURN do{_op_return=&&loop_return;}while(0)
#define SET_CODE_RETURN do{_op_return=&&code_return;}while(0)
#define SET_NOEVAL_RETURN do{_op_return=&&noeval_return;}while(0)

//TODO: clean up naming convension for stack macros (reserve, fix, restore, ...)
//TODO: more macros to clean up (and make safer) the low-level op handling code
//TODO: consistent val access macros to be able to fully validate val create/destroy/replace and debug vals
#define RESERVE do{ VM_TRY(_vm_reserve(stackval,&stackbase,&stack,&stackend)); }while(0)
#define RESERVE_fatal do{ if ((e = _vm_reserve(stackval,&stackbase,&stack,&stackend))) E_FATAL(e); }while(0)
#define WRESERVE do{ VM_TRY(_vm_reserve(workval,&workbase,&work,&workend)); }while(0)
#define WRESERVE_fatal do{ if ((e = _vm_reserve(workval,&workbase,&work,&workend))) E_FATAL(e); }while(0)

#define FIXSTACK do{ if (state>0) { if (state>1 && stack==stackend) RESERVE; *(stack++)=top; state=0; } stackval->v.lst.len = stack-stackbase; }while(0)
#define FIXWSTACK do{ workval->v.lst.len = work-workbase; }while(0)
#define FIXSTACKS do{ FIXSTACK; FIXWSTACK; }while(0)

#define RESTOREWSTACK do{ workbase = _val_lst_begin(workval); work = _val_lst_end(workval); workend = _val_lst_bufend(workval); }while(0)
#define RESTORESTACK do{ state=0; stackbase = _val_lst_begin(stackval); stack = _val_lst_end(stackval); stackend = _val_lst_bufend(stackval); if (stack==stackend) RESERVE; }while(0)
#define RESTORESTACKS do{ RESTORESTACK; RESTOREWSTACK; }while(0)

#define PUSH_0(x) do{ top=(x); state=1; }while(0)
#define PUSH_1(x) do{ *(stack++)=top; top=(x); state=2; }while(0)
#define PUSH_2(x) do{ if (stack==stackend) RESERVE; *(stack++)=top; top=(x); }while(0)
#define PUSH_12(x) do{ if (stack==stackend) RESERVE; *(stack++)=top; top=(x); state=2; }while(0)
#define PUSH_2_fatal(x) do{ if (stack==stackend) RESERVE_fatal; *(stack++)=top; top=(x); }while(0)
#define PUSH(x) do{ switch(state) { case 0: PUSH_0(x); break; case 1: PUSH_1(x); break; default: PUSH_2(x); } }while(0)
#define PUSH_fatal(x) do{ switch(state) { case 0: PUSH_0(x); break; case 1: PUSH_1(x); break; default: PUSH_2_fatal(x); } }while(0)

#define WPUSH(x) do{ if (work==workend) WRESERVE; *(work++)=x; }while(0)
#define WPUSH_fatal(x) do{ if (work==workend) WRESERVE_fatal; *(work++)=x; }while(0)
#define WPUSH2(x,y) do{ if ((workend-work) < 2) WRESERVE; *(work++)=x; *(work++)=y; }while(0)
#define WPUSH3(x,y,z) do{ if ((workend-work) < 3) WRESERVE; *(work++)=x; *(work++)=y; *(work++)=z; }while(0)
#define WPROTECT(x) do{ if (work==workend) WRESERVE; VM_TRY(val_protect(&(x))); *(work++)=x; }while(0)

#define BURY1_1(x) do{ *(stack++)=x; state=2; }while(0)
#define BURY1_2(x) do{ if (stack==stackend) RESERVE; *(stack++)=x; }while(0)

#define _TOP_0 (stack[-1])
#define _TOP_1 (top)
#define _TOP_2 (top)
#define _TOP_12 (top)

#define _SECOND_0 (stack[-2])
#define _SECOND_1 (stack[-1])
#define _SECOND_2 (stack[-1])

#define _THIRD_0 (stack[-3])
#define _THIRD_1 (stack[-2])
#define _THIRD_2 (stack[-2])


#define _FOURTH_2 (stack[-3])

#define POP_0 do{ if (stack==stackbase) E_EMPTY; val_destroy(*(--stack)); val_clear(stack); }while(0)
#define POP_1 do{ val_destroy(top); state=0; }while(0)
#define POP_2 do{ val_destroy(top); top=*(--stack); val_clear(stack); state=1; }while(0)
#define POP2_2 do{ val_destroy(top); val_destroy(*(--stack)); val_clear(stack); state=0; }while(0)
#define POP2_3 do{ val_destroy(top); val_destroy(*(--stack)); val_clear(stack); top=*(--stack); val_clear(stack); state=1; }while(0)
#define POP_12 do{ val_destroy(top); if (--state) { top=*(--stack); val_clear(stack); } }while(0)
#define POPD_2 do{ val_destroy(*(--stack)); val_clear(stack); state=1; }while(0)
//#define POP do{ if (state) { POP_12; } else { POP_0; } } while(0)


//the POP macros with underscore prefix assume val is already destroyed/moved, so we just need to clear
//#define _POP_0 do{ if (stack==stackbase) E_EMPTY; val_clear(--stack); } while(0)
#define _POP_1 do{ state=0; }while(0)
#define _POP_2 do{ top=*(--stack); val_clear(stack); state=1; }while(0)
#define _POP_12 do{ if (--state) { top=*(--stack); val_clear(stack); } }while(0)
#define _POPD_2 do{ val_clear(--stack); state=1; }while(0)
#define _POP2_2 do{ val_clear(--stack); state=0; }while(0)

#define QSTATE _vm_qstate(vm,stackbase,stack-stackbase,workbase,work-workbase,state,top)
#define VSTATE _vm_vstate(vm,stackbase,stack-stackbase,workbase,work-workbase,state,top)
#define VALIDATE _vm_validate(vm,stackbase,stack-stackbase,workbase,work-workbase,state,top)


//#define CPOP(x) do{ vm->v. } while(0)


//the HAVE macros check if we have enough vals in the stack (ignoring top register)
#define HAVE(n) (stack-stackbase>=(n))
//#define ASSERT_HAVE1_0 do{ if (stack==stackbase) E_EMPTY; } while(0)
#define STATE_0TO1 do{ if (stack==stackbase) E_EMPTY; top=*(--stack); val_clear(stack); state=1; } while(0)
#define STATE_1TO2 do{ if (stack==stackbase) E_EMPTY; state=2; } while(0)
#define STATE_2TO1 do{ if (stack==stackend) RESERVE; state=1; } while(0)


  //TODO: best way to compute the goto address for the state (array/multiply)
  // - current options:
  //   - array lookup (current)
  //   - multiply state by num ops per state
#define GOTO_OP(x) goto *stateops[state][__val_op(x)]
//#define GOTO_OP(x) goto *_ops[state*N_OPS+__val_op(x)]



//#define VM_DEBUG_STEP 1
//VM_DEBUG_STEP enables printing of every step of the VM
#ifdef VM_DEBUG_STEP
#define NEXT goto vm_debug_step
#define NEXTW do{ SET_LOOP_RETURN; goto vm_debug_step; }while(0)
#else
#define NEXT goto *_op_return
#define NEXTW do{ SET_LOOP_RETURN; goto loop_next; }while(0)
#endif

//VM_try and VM_try_t assume an argument that returns an err_t; VM_TRY_t additionally makes sure t is destroyed
#define VM_TRY(f) do{ if ((e = (f))) goto handle_err; }while(0)
#define VM_TRY_t(f) do{ if ((e = (f))) goto handle_err_t; }while(0)
#define HANDLE_e do{ goto handle_err; }while(0)
#define HANDLE_e_t do{ goto handle_err_t; }while(0)

//#define VM_DEBUG_ERR 1
//VM_DEBUG_ERR controls whether vm jumps to error handler, or handles error on line error is thrown from
//  - flag is useful for debugging ops in gdb (especially with INTERRUPT_ON_THROW)
#ifdef VM_DEBUG_ERR
#define E_NULL do{ e = _throw(ERR_NULL); HANDLE_e; } while(0)
#define E_EMPTY do{ e = _throw(ERR_EMPTY); HANDLE_e; } while(0)
#define E_BADTYPE do{ e = _throw(ERR_BADTYPE); HANDLE_e; } while(0)
#define E_BADOP do{ e = _throw(ERR_BAD_OP); HANDLE_e; } while(0)
#define E_BADARGS do{ FIXSTACKS; vm_vstate(vm); e = _throw(ERR_BADARGS); HANDLE_e; } while(0)
#define E_MISSINGARGS do{ FIXSTACKS; vm_vstate(vm); e = _throw(ERR_MISSINGARGS); HANDLE_e; } while(0)
#define E_UNDEFINED do{ e = _throw(ERR_UNDEFINED); HANDLE_e; } while(0)
#define E_NOIMPL do{ e = _throw(ERR_NOT_IMPLEMENTED); HANDLE_e; } while(0)
#define E_NODEBUG do{ e = _throw(ERR_NO_DEBUG); HANDLE_e; } while(0)
//#define E_MALLOC do{ e = _throw(ERR_MALLOC); HANDLE_e; } while(0)
#define E_FATAL(err) do{ e = _fatal(err); HANDLE_e; } while(0)
#else
#define E_NULL goto err_null
#define E_EMPTY goto err_empty
#define E_BADTYPE goto err_badtype
#define E_BADOP goto err_badop
#define E_BADARGS goto err_badargs
#define E_MISSINGARGS goto err_missingargs
#define E_UNDEFINED goto err_undefined
#define E_NOIMPL goto err_noimpl
#define E_NODEBUG goto err_nodebug
//#define E_MALLOC goto err_malloc
#define E_FATAL(err) do{ e = err; goto err_fatal; }while(0)
#endif

  void **_op_return = &&loop_return;
  val_t w;

#ifdef VM_DEBUG_STEP
  //print first step
  NEXT;
#endif

  while(work != workbase) {
loop_next:
    w = *(--work); //get next workitem and decrement work ptr (still need to destroy/clear *work as needed below)
    VM_DEBUG_EVAL(&w);
#ifdef DEBUG_VAL_EVAL
    // DEBUG_VAL_EVAL - non-null eval-type debug vals are evaluated in place of regular val
    // - regular val is placed on top of stack, which allows for validation/transformation/logging before evaluating regular val (or not)
    // - splits regular/debug vals (so regular val with no debug info attached is on top of stack)
    if( debug_val_eval ) {
      dbg = __val_dbg_val(w);
      if (!val_is_null(dbg) && !val_ispush(dbg) ) { //replace w on workstack with debug val, push w (w/o debug) to stack
        PUSH(__val_dbg_strip(w));
        w = (val_t)dbg;
        *(work++) = w;
      }
    }
#endif
    switch(__val_tag(w)) {
      case _OP_TAG: //this is an opcode (maybe native later for op > N_OPS / N_OPCODES) <================
        val_clear(work); //not actually needed for inline type
        GOTO_OP(w); //jump to opcode
      case _STR_TAG: //this is a str-type - figure out if ident, string, or bytecode (eval ident,bytecode) <================
        v = __str_ptr(w);
        switch(v->type) {
          case TYPE_IDENT: //ident - unescape and push or eval from dictionary <====
            if (_val_str_escaped(v)) { //escaped - unescape and push <==
              _val_str_unescape(v);
              PUSH(w);
              val_clear(work); //clear *work since we moved it to stack
            } else { //not escaped - eval from dictionary <==
              t = vm_dict_get(vm,v);
              if (!val_is_null(t)) { //found def <==
                val_destroy(w);
                if (val_is_op(t)) { //if op then we immediately jump to it
                  val_clear(work);
#ifdef DEBUG_VAL_EVAL
                  //in debug_val mode we need to check for debug val attached to op
                  dbg = __val_dbg_val(t);
                  if (debug_val_eval && !val_is_null(dbg) && !val_ispush(dbg)) {
                    //need to clone debug val since it is in dict def
                    PUSH(__val_dbg_strip(t));
                    VM_TRY(val_clone(&t,dbg));
                    WPUSH(t);
                    NEXTW;
                  } else {
                    GOTO_OP(t);
                  }
#else
                  GOTO_OP(t);
#endif
                } else { //else we replace ident with definition on work stack
                  val_t def;
                  VM_TRY(val_clone(&def,t));
                  *(work++) = def; //replace *work with def
                }
              } else { //undefined - print error and throw undefined <====
                fflush(stdout);
                fprintf(stderr,"unknown word '%.*s'\n",_val_str_len(v),_val_str_begin(v)); //TODO: only when in terminal
                //TODO: can we pass undefined word to exception handler???
                fflush(stderr);
                ++work; // for debugging purposes, leave ident on stack
                E_UNDEFINED;
              }
            }
            break;
          case TYPE_BYTECODE: //bytecode - not implemented yet in this version <====
            E_NOIMPL;
          default: //string (or other str-based push types we add) - push it <====
            PUSH(w);
            val_clear(work); //clear *work since we moved it to stack
        }
        break;
      case _LST_TAG: //this is a lst-type - figure out if code or list (eval code) <================
        v = __lst_ptr(w);
        switch(v->type) {
          case TYPE_CODE: //code val -- eval next val in w
            if (_val_lst_empty(v)) { val_destroy(*work); val_clear(work); NEXT; }
            SET_CODE_RETURN;
            ++work; //undo the decrement we did above (if w not empty we keep it at top of stack)

            //TODO: reduce code duplication between code/stack handling if possible (or remove this todo)
code_return: //we keep looping here until w is empty or the work stack gets updated
            //TODO: we don't actually need to fix off/len until end if we keep pointers into w (but then need to handle extra cases for non-singleton code or just clone everything)
            //TODO: optimization (lpop vs iterate with pointers vs mixed)
            //  - lpop up front is simple, and makes for simple last el handling
            //  - iterating with pointers could skip lots of administration for some common cases (e.g. opcode, file)
            VM_TRY(_val_lst_lpop(v,&t));
            if (_val_lst_empty(v)) { //last el
              val_destroy(*(--work)); val_clear(work);
              SET_LOOP_RETURN;
            }
#ifdef DEBUG_VAL_EVAL
            //if debug_val_eval, we eval debug val in place of val (if debug val is not null/push-type)
            if( debug_val_eval ) {
              dbg = __val_dbg_val(t);
              if (!val_is_null(dbg) && !val_ispush(dbg) ) { //replace w on workstack with debug val, push w (w/o debug) to stack
                PUSH(__val_dbg_strip(t));
                t = (val_t)dbg;
              }
            }
#endif
            switch(__val_tag(t)) {
              case _OP_TAG:
                GOTO_OP(t);
              case _STR_TAG:
                tv = __str_ptr(t);
                switch(tv->type) {
                  case TYPE_IDENT:
                    if (_val_str_escaped(tv)) { //escaped, push unescaped copy onto stack
                      _val_str_unescape(__ident_ptr(t));
                      PUSH(t);
                    } else { //need to eval
                      t = vm_dict_get(vm,tv);
                      if (!val_is_null(t)) {
                        _val_str_destroy(tv);
                        if (val_is_op(t)) { //if op then we immediately jump to it
#ifdef DEBUG_VAL_EVAL
                          //in debug_val mode we need to check for debug val attached to op
                          dbg = __val_dbg_val(t);
                          if (debug_val_eval && !val_is_null(dbg) && !val_ispush(dbg)) {
                            //need to clone debug val since it is in dict def
                            PUSH(__val_dbg_strip(t));
                            VM_TRY(val_clone(&t,dbg));
                            WPUSH(t);
                            NEXTW;
                          } else {
                            GOTO_OP(t);
                          }
#else
                          GOTO_OP(t);
#endif
                        } else { //else we push definition onto work stack TODO: if push type, directly push to stack instead
                          VM_TRY(val_clone(&t,t));
                          WPUSH(t);
                          NEXTW;
                        }
                      } else { //undefined
                        fflush(stdout);
                        fprintf(stderr,"unknown word '%.*s'\n",_val_str_len(tv),_val_str_begin(tv)); //TODO: only when in terminal
                        fflush(stderr);
                        _val_str_destroy(tv); //TODO: should we push to stack for debugging?
                        E_UNDEFINED;
                      }
                    }
                    break;
                  case TYPE_BYTECODE:
                    E_NOIMPL;
                  default:
                    PUSH(t);
                }
                break;
              case _LST_TAG:
                //tv = __lst_ptr(t);
                PUSH(t);
                break;
              case _VAL_TAG:
                tv = __val_ptr(t);
                switch(tv->type) {
                  //TODO: decide on file behaviour, I switched it to this so we can have a simpler [protect] for files
                  // - original impl was that files always eval their contents (even inside quotation)
                  // - new impl is that files are pushed when encountered inside quotation, bare files eval their contents
                  //TODO: TYPE_FD -- same considerations as TYPE_FILE
                  //case TYPE_FILE:
                  //  //FIXME: totally not threadsafe (TODO: use per-thread FILE_BUF)
                  //  //TODO: should we wpush file, or just lpush back onto code list
                  //  if (0>(n = _val_file_readline_(tv,FILE_BUF,FILE_BUFLEN))) {
                  //    val_destroy(t);
                  //    if (n != ERR_EOF) { e=n; HANDLE_e; }
                  //  } else if (n==0 || (n==1 && FILE_BUF[0]=='\n')) { //empty line
                  //    WPUSH(t);
                  //    NEXTW;
                  //  } else {
                  //    WPUSH(t);
                  //    t = val_empty_code();
                  //    VM_TRY_t(_vm_parse_input(vm->p,FILE_BUF,n,__lst_ptr(t)));
                  //    else if (_val_lst_empty(__lst_ptr(t))) {
                  //      val_destroy(t);
                  //    } else {
                  //      WPUSH(t);
                  //    }
                  //    NEXTW;
                  //  }
                  //  break;
                  case TYPE_VM:
                    e = val_vm_eval_final(&t,tv);
                    if (e==0 || e==ERR_THROW || e==ERR_USER_THROW) {
                      PUSH(t);
                    }
                    if (e) HANDLE_e;
                    break;
                  //case TYPE_DICT:
                  //case TYPE_FILE: -- see commented out case above
                  //case TYPE_FD: -- see commented out case above
                  default:
                    PUSH(t);
                }
                break;
              case _TAG5:
                E_BADTYPE;
              //case _INT_TAG:
              default: //int or double
                PUSH(t);
            }
            NEXT; //keep looping on current code val until it is empty
          default: //non-code lists just get pushed
            PUSH(w);
            val_clear(work); //clear *work since we moved it to stack
        }
        break;
      case _VAL_TAG: // this is a val-type - check valstruct type <================
        v = __val_ptr(w);
        switch(v->type) {
          case TYPE_FILE:
            ++work; //undo the decrement we did above (file stays on stack until empty)
            if (0>(e = _val_file_readline_(v,FILE_BUF,FILE_BUFLEN))) {
              val_destroy(w); val_clear(--work); //done with file;
              if (e != ERR_EOF) HANDLE_e;
            } else if (e==0 || (e==1 && FILE_BUF[0]=='\n')) { //empty line
              goto loop_next; //TODO: or should we just re-read in a tight loop over empty lines???
            } else {
              int len = e;
              t = val_empty_code();
              VM_TRY_t(_vm_parse_input(vm->p,FILE_BUF,len,__lst_ptr(t)));
              if (_val_lst_empty(__lst_ptr(t))) {
                val_destroy(t);
              } else {
                WPUSH(t);
              }
              goto loop_next;
            }
            break;

          case TYPE_VM:
            e = val_vm_eval_final(&t,v);
            val_clear(work);
            if (e==0 || e==ERR_THROW || e==ERR_USER_THROW) {
              PUSH(t);
            }
            if (e) HANDLE_e;
            break;
          //case TYPE_DICT:
          //case TYPE_REF:
          //case TYPE_FD:
          default:
            val_clear(work); //clear *work since we moved it to stack
            PUSH(w);
        }
        break;
      case _TAG5: // we don't use this type tage yet <================
        E_BADTYPE;
      //case _INT_TAG:
      default: //this is a double or int - push it <================
        val_clear(work); //not needed for inline type
        PUSH(w);
    }
#ifdef VM_DEBUG_STEP
    NEXT;
#endif
loop_return:
    continue;
  }

  FIXSTACKS;
  return 0;


  //in noeval mode, we don't use the stack pointers, but directly operate on vm->open_list (remember to update stackval and call RESTORESTACK before leaving noeval)
  while(work != workbase) { //FIXME: validate correctly freeing/clearing/using wtop
//noeval_next:
    w = *(--work); //get next workitem and decrement work ptr (still need to destroy/clear *work as needed below)
    VM_DEBUG_EVAL(&w);
    if (val_is_file(w)) {
      ++work; //undo the decrement we did above (file stays on wstack until empty)
      v = __file_ptr(w);
      if (0>(e = _val_file_readline_(v,FILE_BUF,FILE_BUFLEN))) {
        val_destroy(w); val_clear(--work); //done with file;
        if (e != ERR_EOF) goto handle_noeval_err;
      } else if (e==0 || (e==1 && FILE_BUF[0]=='\n')) { //empty line
        //continue;
      } else {
        int len = e;
        t = val_empty_code();
        if ((e = _vm_parse_input(vm->p,FILE_BUF,len,__lst_ptr(t)))) {
          val_destroy(t);
          goto handle_noeval_err;
        } else if (_val_lst_empty(__lst_ptr(t))) {
          val_destroy(t);
        } else {
          WPUSH(t);
        }
      }
    } else if (val_is_ident(w) && _vm_qeval(vm,__ident_ptr(w),&e)) {
      val_destroy(w); val_clear(work); //done with file -- assumes qeval doesn't use or modify wstack

      if (!vm->noeval) { //done with noeval, just closed outermost code
        stackval = vm->open_list;
        stackbase = _val_lst_begin(stackval);
        stack = stackbase + _val_lst_len(stackval);
        stackend = _val_lst_bufend(stackval);
        if (stack == stackend) RESERVE;
        STATE_0;
        NEXTW;
      }
    } else if (val_is_code(w)) {
      ++work; //undo above dec (code stays on wstack until empty)
      v = __code_ptr(w);
      if (_val_lst_empty(v)) {
        val_destroy(w); val_clear(--work); //done with quotation
      } else {
        val_t whead = *_val_lst_begin(v);
        if (val_is_ident(whead) && _vm_qeval(vm,__ident_ptr(whead),&e)) {
          if ((e = _val_lst_ldrop(v))) goto handle_noeval_err; //drop ident we just qeval'd

          if (!vm->noeval) { //just closed outermost code, done with noeval
            stackval = vm->open_list;
            stackbase = _val_lst_begin(stackval);
            stack = stackbase + _val_lst_len(stackval);
            stackend = _val_lst_bufend(stackval);
            if (stack == stackend) RESERVE;
            STATE_0;
            NEXTW;
          }
        } else {
          if ((e = _val_lst_lpop(v,&t))) goto handle_noeval_err;
          if ((e = vm_push(vm,t))) goto handle_noeval_err;
        }
      }
    } else {
      if ((e = vm_push(vm,w))) goto handle_noeval_err;
      else {
        val_clear(work);
      }
    }
#ifdef VM_DEBUG_STEP
    NEXT;
#endif
noeval_return:
    continue;
  }
  E_EMPTY; //ran out of work while still in noeval

//opcode handlers
//each opcode needs to define 3 labels:
//  op_opname_0:
//  op_opname_1:
//  op_opname_2:
//
//the VM state on jumping to one of those labels will match the label suffix, so you can do (or skip) any necessary stack checks
//  - can define multiple labels before the same code if the code is the same for those states


  //TODO: full vaidation of debug val handling (that every op clones/copies/destroys debug val appropriately)


op_NULL_0:
op_NULL_1:
op_NULL_2:
  E_NULL;
op_end_0:
op_end_1:
op_end_2:
  //FIXME: cleanup
  E_NOIMPL;
op_break_0:
op_break_1:
op_break_2:
  FIXSTACKS;
  return ERR_BREAK;

op_eval_0: STATE_0TO1;
op_eval_1:
  WPUSH(_TOP_1);
  _POP_1;
  NEXTW;
op_eval_2:
  WPUSH(_TOP_2);
  _POP_2;
  NEXTW;
op_parsecode_0: STATE_0TO1;
op_parsecode_1:
op_parsecode_2:
  if (!val_is_str(_TOP_12)) E_BADTYPE;
  tv = __str_ptr(_TOP_12);
  t = val_empty_code();
  VM_TRY_t(vm_parse_code(vm,_val_str_begin(tv),_val_str_len(tv),__lst_ptr(t)));
  __val_reset(&_TOP_12,t);
  NEXT;
op_parsecode__0: STATE_0TO1;
op_parsecode__1:
op_parsecode__2:
  if (!val_is_str(_TOP_12)) E_BADTYPE;
  tv = __str_ptr(_TOP_12);
  t = val_empty_code();
  VM_TRY_t(vm_parse_input(vm,_val_str_begin(tv),_val_str_len(tv),__lst_ptr(t)));
  __val_reset(&_TOP_12,t);
  NEXT;
op_pop_0:
  POP_0;
  NEXT;
op_pop_1:
op_pop_2:
  POP_12;
  NEXT;
op_swap_0: 
  if (!HAVE(2)) E_EMPTY;
  top = _SECOND_0;
  _SECOND_0=_TOP_0;
  val_clear(--stack);
  STATE_2;
  NEXT;
op_swap_1: STATE_1TO2;
op_swap_2:
  t = _TOP_2;
  _TOP_2 = _SECOND_2;
  _SECOND_2 = t;
  NEXT;
op_dup_0:
  if (!HAVE(1)) E_EMPTY;
  VM_TRY(val_clone(&top,_TOP_0));
  STATE_2;
  NEXT;
op_dup_1:
  VM_TRY(val_clone(&t,_TOP_1));
  PUSH_1(t);
  NEXT;
op_dup_2:
  VM_TRY(val_clone(&t,_TOP_2));
  PUSH_2(t);
  NEXT;
op_dup2_0: STATE_0TO1;
op_dup2_1: STATE_1TO2;
op_dup2_2:
  VM_TRY(val_clone(&t,_SECOND_2));
  PUSH_2(t);
  NEXT;
op_dup3_0: STATE_0TO1;
op_dup3_1: //TODO: cases
op_dup3_2:
  if (!HAVE(2)) E_EMPTY;
  STATE_2;
  VM_TRY(val_clone(&t,_THIRD_2));
  PUSH_2(t);
  NEXT;
op_dupn_0: STATE_0TO1;
op_dupn_1:
op_dupn_2:
  if (!val_is_int(_TOP_12)) E_BADARGS;
  i = __val_int(_TOP_12); __val_dbg_destroy(_TOP_2);
  if (i <= 0 || !HAVE(i)) E_BADARGS;
  //STATE_2;

  VM_TRY(val_clone(&_TOP_12,stack[-i]));
  NEXT;
op_dign_0: STATE_0TO1;
op_dign_1:
op_dign_2:
  if (!val_is_int(_TOP_12)||!HAVE(1+__val_int(_TOP_12))) E_BADARGS;
  i = __val_int(_TOP_12); __val_dbg_destroy(_TOP_2);
  _TOP_2 = stack[-i-1]; //dig out nth
  valmove(stack-i-1,stack-i,i); //shift vals left
  val_clear(--stack);
  NEXT;
op_buryn_0: STATE_0TO1;
op_buryn_1:
op_buryn_2:
  if (!val_is_int(_TOP_12)||!HAVE(1+__val_int(_TOP_12))) E_BADARGS;
  STATE_2;
  i = __val_int(_TOP_2); __val_dbg_destroy(_TOP_2);
  _TOP_2 = _THIRD_2; //save val getting shuffled to top by bury
  valmove(stack-i,stack-i-1,i-1); //shift vals right 1 to make space
  stack[-i-1] = _SECOND_2; //bury top
  val_clear(--stack);
  NEXT;
op_flipn_0: STATE_0TO1;
op_flipn_1:
op_flipn_2:
  if (!val_is_int(_TOP_12)||!HAVE(__val_int(_TOP_12))) E_BADARGS;
  n = __val_int(_TOP_12); __val_dbg_destroy(_TOP_12);
  for(i=0;i<n/2;++i) {
    val_swap(stack-1-i,stack-1-(n-i-1));
  }
  _POP_12;
  NEXT;
op_empty_0: STATE_0TO1;
op_empty_1:
op_empty_2:
  if (val_is_lst(_TOP_12)) {
    t = __int_val(_val_lst_empty(__lst_ptr(_TOP_12)));
  } else if (val_is_str(_TOP_12)) {
    t = __int_val(_val_str_empty(__str_ptr(_TOP_12)));
  } else {
    E_BADTYPE;
  }
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;
op_small_0: STATE_0TO1;
op_small_1:
op_small_2:
  if (val_is_lst(_TOP_12)) {
    t = __int_val(1>=_val_lst_len(__lst_ptr(_TOP_12)));
  } else if (val_is_str(_TOP_12)) {
    t = __int_val(1>=_val_str_len(__str_ptr(_TOP_12)));
  } else {
    E_BADTYPE;
  }
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;
op_size_0: STATE_0TO1;
op_size_1:
op_size_2:
  if (val_is_lst(_TOP_12)) {
    t = __int_val(_val_lst_len(__lst_ptr(_TOP_12)));
  } else if (val_is_str(_TOP_12)) {
    t = __int_val(_val_str_len(__str_ptr(_TOP_12)));
  } else {
    E_BADTYPE;
  }
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;
op_lpop_0: STATE_0TO1;
op_lpop_1:
  if (val_is_lst(_TOP_1)) {
    VM_TRY(_val_lst_lpop(__lst_ptr(_TOP_1),&t));
    BURY1_1(t);
    NEXT;
  } else if (val_is_str(_TOP_1)) {
    VM_TRY(_val_str_splitn(__str_ptr(_TOP_1),&t,1));
    PUSH_1(t);
    NEXT;
  } else {
    E_BADTYPE;
  }
op_lpop_2:
  if (val_is_lst(_TOP_2)) {
    VM_TRY(_val_lst_lpop(__lst_ptr(_TOP_2),&t));
    BURY1_2(t);
    NEXT;
  } else if (val_is_str(_TOP_2)) {
    VM_TRY(_val_str_splitn(__str_ptr(_TOP_2),&t,1));
    PUSH_2(t);
    NEXT;
  } else {
    E_BADTYPE;
  }
op_lpush_0: STATE_0TO1;
op_lpush_1: STATE_1TO2;
op_lpush_2:
  if (val_is_lst(_TOP_2)) {
    VM_TRY(_val_lst_lpush(__lst_ptr(_TOP_2),_SECOND_2));
    _POPD_2;
    NEXT;
  } else if (val_is_str(_TOP_2)) {
    if (val_is_str(_SECOND_2)) {
      VM_TRY(_val_str_rcat(__str_ptr(_TOP_2),__str_ptr(_SECOND_2)));
      __val_dbg_destroy(_SECOND_2); //destroy debug val for second (top retains)
      _POPD_2;
      NEXT;
    } else {
      VM_TRY(val_tostring(&t,_SECOND_2));
      VM_TRY(_val_str_rcat(__str_ptr(_TOP_2),__string_ptr(t)));
      POPD_2;
      NEXT;
    }
  } else {
    E_BADTYPE;
  }
op_rpop_0: STATE_0TO1;
op_rpop_1:
  if (val_is_lst(_TOP_1)) {
    VM_TRY(_val_lst_rpop(__lst_ptr(_TOP_1),&t));
    BURY1_1(t);
    NEXT;
  } else if (val_is_str(_TOP_1)) {
    VM_TRY(_val_str_splitn(__str_ptr(_TOP_1),&t,1));
    PUSH_1(t);
    NEXT;
  } else {
    E_BADTYPE;
  }
op_rpop_2:
  if (val_is_lst(_TOP_2)) {
    VM_TRY(_val_lst_rpop(__lst_ptr(_TOP_2),&t));
    BURY1_2(t);
    NEXT;
  } else if (val_is_str(_TOP_2)) {
    VM_TRY(_val_str_splitn(__str_ptr(_TOP_2),&t,1));
    PUSH_2(t);
    NEXT;
  } else {
    E_BADTYPE;
  }
op_rpush_0: STATE_0TO1;
op_rpush_1: STATE_1TO2;
op_rpush_2:
  if (val_is_lst(_TOP_2)) {
    VM_TRY(_val_lst_rpush(__lst_ptr(_TOP_2),_SECOND_2));
    _POPD_2;
    NEXT;
  } else if (val_is_str(_TOP_2)) {
    if (val_is_str(_SECOND_2)) {
      VM_TRY(_val_str_cat(__str_ptr(_TOP_2),__str_ptr(_SECOND_2)));
      _POPD_2;
      NEXT;
    } else {
      VM_TRY(val_tostring(&t,_SECOND_2));
      VM_TRY(_val_str_cat(__str_ptr(_TOP_2),__string_ptr(t)));
      POPD_2;
      NEXT;
    }
  } else {
    E_BADTYPE;
  }

op_cat_0: STATE_0TO1;
op_cat_1: STATE_1TO2;
op_cat_2:
  if (val_is_lst(_SECOND_2)) {
    if (!val_is_lst(_TOP_2)) E_BADTYPE;
    VM_TRY(_val_lst_cat(__lst_ptr(_SECOND_2),__lst_ptr(_TOP_2)));

    //need to destroy debug val of top (second keeps its debug val)
    // NOTE: could do this up top, but this means on type err we don't destroy debug into
    __val_dbg_destroy(_TOP_2);

    _TOP_2 = _SECOND_2;
    _POPD_2;
    NEXT;
  } else if (val_is_str(_SECOND_2)) {
    if (!val_is_str(_TOP_2)) {
      VM_TRY(val_tostring(&t,_TOP_2));
      val_destroy(_TOP_2);
      _TOP_2 = t;
    }
    VM_TRY(_val_str_cat(__str_ptr(_SECOND_2),__str_ptr(_TOP_2)));
    __val_dbg_destroy(_TOP_2); //need to destroy debug val of top (second keeps its debug val)
    _TOP_2 = _SECOND_2;
    _POPD_2;
    NEXT;
  } else {
    E_BADTYPE;
  }
op_rappend_0: STATE_0TO1;
op_rappend_1: STATE_1TO2;
op_rappend_2:
  if (val_is_lst(_TOP_2)) {
    if (val_is_lst(_SECOND_2)) {
      VM_TRY(_val_lst_cat(__lst_ptr(_TOP_2),__lst_ptr(_SECOND_2)));

      //need to destroy debug val of second (for other list on top cases second added along with debug)
      __val_dbg_destroy(_SECOND_2);
    } else {
      VM_TRY(_val_lst_rpush(__lst_ptr(_TOP_2),_SECOND_2));
    }
    _POPD_2;
    NEXT;
  } else if (val_is_str(_TOP_2)) {
    if (!val_is_str(_SECOND_2)) {
      VM_TRY(val_tostring(&t,_SECOND_2));
      val_destroy(_SECOND_2);
      _TOP_2 = t;
    }
    VM_TRY(_val_str_cat(__str_ptr(_TOP_2),__str_ptr(_SECOND_2)));

    //need to destroy debug val of second (string on top keeps its debug val)
    __val_dbg_destroy(_SECOND_2);

    _POPD_2;
    NEXT;
  } else {
    E_BADTYPE;
  }

op_splitn_0: STATE_0TO1;
op_splitn_1: STATE_1TO2;
op_splitn_2:
  if (!val_is_int(_TOP_2)) E_BADTYPE;
  i = __val_int(_TOP_2); __val_dbg_destroy(_TOP_2);
  if (val_is_lst(_SECOND_2)) {
    if (i < 0 || (unsigned int)i > _val_lst_len(__lst_ptr(_SECOND_2))) E_BADARGS;
    VM_TRY(_val_lst_splitn(__lst_ptr(_SECOND_2),&_TOP_2,i));
    NEXT;
  } else if (val_is_str(_SECOND_2)) {
    if (i < 0 || (unsigned int)i > _val_str_len(__str_ptr(_SECOND_2))) E_BADARGS;
    VM_TRY(_val_str_splitn(__str_ptr(_SECOND_2),&_TOP_2,i));
    NEXT;
  } else {
    E_BADTYPE;
  }

op_strhash_0: STATE_0TO1;
op_strhash_1:
op_strhash_2:
  if (!val_is_str(_TOP_12)) E_BADTYPE;
  __val_dbg_destroy(_TOP_12); //throw away debug val
  t = _val_str_hash32(__str_ptr(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;

op_getbyte_0: STATE_0TO1;
op_getbyte_1: STATE_1TO2;
op_getbyte_2:
  if (!val_is_str(_SECOND_2) || !val_is_int(_TOP_2)) E_BADTYPE;
  i = __int_val(_TOP_2); __val_dbg_destroy(_TOP_2); //throw away debug val from index
  tv = __str_ptr(_SECOND_2);
  if (i < 0 || (uint32_t)i >= _val_str_len(tv)) E_BADARGS;
  //NOTE: we skip destroying top, which is ok here because we know it is an int (with inline value)
  _TOP_2 = __int_val(_val_str_begin(tv)[i]);
  NEXT;

op_setbyte_0: STATE_0TO1;
op_setbyte_1:
op_setbyte_2:
  if (!HAVE(2)) E_MISSINGARGS;
  if (!val_is_str(_THIRD_2) || !val_is_int(_SECOND_2) || !val_is_int(_TOP_2)) E_BADTYPE;
  i = __int_val(_TOP_2); __val_dbg_destroy(_TOP_2); //throw away debug val from index
  __val_dbg_destroy(_SECOND_2); //throw away debug val from byte
  tv = __str_ptr(_THIRD_2);
  VM_TRY(_val_str_rreserve(tv,0));
  if (i < 0 || (uint32_t)i >= _val_str_len(tv)) E_BADARGS;
  _val_str_begin(tv)[i] = (char)__val_int(_SECOND_2);

  _TOP_2 = _THIRD_2; //we can safely skip destroy both top and second are inline ints, so we just overwrite top with third and clear second/third
  val_clear(--stack);
  val_clear(--stack);
  STATE_1;
  NEXT;

op_first_0: STATE_0TO1;
op_first_1:
op_first_2:
  if (val_is_lst(_TOP_12)) {
    VM_TRY(_val_lst_lpop(__lst_ptr(_TOP_12),&t));
    val_destroy(_TOP_12);
    _TOP_12 = t;
    NEXT;
  } else if (val_is_str(_TOP_12)) {
    _val_str_substr(__str_ptr(_TOP_12),0,1);
    NEXT;
  } else {
    E_BADTYPE;
  }
op_last_0: STATE_0TO1;
op_last_1:
op_last_2:
  if (val_is_lst(_TOP_12)) {
    VM_TRY(_val_lst_rpop(__lst_ptr(_TOP_12),&t));
    val_destroy(_TOP_12);
    _TOP_12 = t;
    NEXT;
  } else if (val_is_str(_TOP_12)) {
    _val_str_substr(__str_ptr(_TOP_12),_val_str_len(__str_ptr(_TOP_12))-1,1);
    NEXT;
  } else {
    E_BADTYPE;
  }
op_nth_0: STATE_0TO1;
op_nth_1: STATE_1TO2;
op_nth_2:
  if (!val_is_int(_TOP_2) || !val_is_lst(_SECOND_2)) E_BADARGS;
  i = __int_val(_TOP_2); __val_dbg_destroy(_TOP_2); //throw away debug val from index
  if ((i < 1 || (unsigned int)i > _val_lst_len(__lst_ptr(_SECOND_2)))) E_BADARGS;

  VM_TRY(val_lst_ith(&_SECOND_2,i-1));
  _TOP_2 = _SECOND_2; val_clear(--stack);
  STATE_1;
  NEXT;

op_rest_0: STATE_0TO1;
op_rest_1:
op_rest_2:
  if (val_is_lst(_TOP_12)) {
    VM_TRY(_val_lst_ldrop(__lst_ptr(_TOP_12)));
    NEXT;
  } else if (val_is_str(_TOP_12)) {
    if (_val_str_empty(__str_ptr(_TOP_12))) E_EMPTY;
    _val_str_substr(__str_ptr(_TOP_12),1,-1);
    NEXT;
  } else {
    E_BADTYPE;
  }

op_dfirst_0: STATE_0TO1;
op_dfirst_1:
op_dfirst_2:
  if (val_is_lst(_TOP_12)) {
    if (_val_lst_empty(__lst_ptr(_TOP_12))) E_EMPTY;
    VM_TRY(val_clone(&t, *_val_lst_begin(__lst_ptr(_TOP_12))));
    PUSH(t);
    NEXT;
  } else if (val_is_str(_TOP_12)) {
    if (_val_str_empty(__lst_ptr(_TOP_12))) E_EMPTY;
    VM_TRY(val_clone(&t,_TOP_12));
    _val_str_substr(__str_ptr(t),0,1);
    PUSH(t);
    NEXT;
  } else {
    E_BADTYPE;
  }
op_dlast_0: STATE_0TO1;
op_dlast_1:
op_dlast_2:
  if (val_is_lst(_TOP_12)) {
    if (_val_lst_empty(__lst_ptr(_TOP_12))) E_EMPTY;
    VM_TRY(val_clone(&t, _val_lst_end(__lst_ptr(_TOP_12))[-1]));
    PUSH(t);
    NEXT;
  } else if (val_is_str(_TOP_12)) {
    if (_val_str_empty(__str_ptr(_TOP_12))) E_EMPTY;
    VM_TRY(val_clone(&t,_TOP_12));
    _val_str_substr(__str_ptr(t),_val_str_len(__str_ptr(t))-1,1);
    PUSH(t);
    NEXT;
  } else {
    E_BADTYPE;
  }
op_dnth_0: STATE_0TO1;
op_dnth_1: STATE_1TO2;
op_dnth_2:
  if (!val_is_int(_TOP_2) || !val_is_lst(_SECOND_2)) E_BADARGS;
  i = __int_val(_TOP_2); __val_dbg_destroy(_TOP_2); //throw away debug val from index
  if ((i < 1 || (unsigned int)i > _val_lst_len(__lst_ptr(_SECOND_2)))) E_BADARGS;
  VM_TRY(val_clone(&_TOP_2, _val_lst_begin(__lst_ptr(_SECOND_2))[i-1]));
  NEXT;

op_swapnth_0: STATE_0TO1;
op_swapnth_1:
op_swapnth_2:
  if (!HAVE(2) || !val_is_int(_TOP_2) || !val_is_lst(_THIRD_2)) E_BADARGS;
  i = __int_val(_TOP_2); __val_dbg_destroy(_TOP_2); //throw away debug val from index
  tv = __lst_ptr(_THIRD_2);
  if ((i < 1 || (unsigned int)i > _val_lst_len(tv))) E_BADARGS;
  VM_TRY(_val_lst_deref(tv));
  p = _val_lst_begin(tv)+i-1;
  val_swap(p,&_SECOND_2);
  _POP_2; //_POP safe since we know top is an int
  NEXT;

op_setnth_0: STATE_0TO1;
op_setnth_1:
op_setnth_2:
  if (!HAVE(2) || !val_is_int(_TOP_2) || !val_is_lst(_THIRD_2)) E_BADARGS;
  i = __int_val(_TOP_2); __val_dbg_destroy(_TOP_2); //throw away debug val from index
  tv = __lst_ptr(_THIRD_2);
  if ((i < 1 || (unsigned int)i > _val_lst_len(tv))) E_BADARGS;
  VM_TRY(_val_lst_deref(tv));
  p = _val_lst_begin(tv)+i-1;
  val_destroy(*p);
  *p = _SECOND_2;
  _POP2_2;
  NEXT;

op_collapse_0:
op_collapse_1:
op_collapse_2:
  FIXSTACK;
  top = val_empty_list();
  _val_lst_swap(__lst_ptr(top),stackval);
  stackbase = stack = NULL;
  RESERVE;
  STATE_1;
  NEXT;

op_restore_0: STATE_0TO1;
op_restore_1:
op_restore_2:
  if (!val_is_lst(_TOP_12)) E_BADTYPE;
  __val_dbg_destroy(_TOP_12); //throw away debug val from list
  STATE_0;
  FIXSTACK;
  _val_lst_swap(__lst_ptr(_TOP_12),stackval);
  VM_TRY_t(_val_lst_cat(stackval,__lst_ptr(top)));
  RESTORESTACK;
  NEXT;

op_expand_0: STATE_0TO1;
op_expand_1:
op_expand_2:
  if (!val_is_lst(_TOP_12)) E_BADTYPE;
  __val_dbg_destroy(_TOP_12); //throw away debug val from list
  STATE_0;
  FIXSTACK;
  VM_TRY_t(_val_lst_cat(stackval,__lst_ptr(top)));
  RESTORESTACK;
  NEXT;

op_sort_0: STATE_0TO1;
op_sort_1:
op_sort_2:
  if (!val_is_lst(_TOP_12)) E_BADTYPE;
  tv = __lst_ptr(_TOP_12);
  VM_TRY(_val_lst_deref(tv));
  val_qsort(_val_lst_begin(tv),_val_lst_len(tv));
  NEXT;

op_rsort_0: STATE_0TO1;
op_rsort_1:
op_rsort_2:
  if (!val_is_lst(_TOP_12)) E_BADTYPE;
  tv = __lst_ptr(_TOP_12);
  VM_TRY(_val_lst_deref(tv));
  val_rqsort(_val_lst_begin(tv),_val_lst_len(tv));
  NEXT;

op_clearlist_0: STATE_0TO1;
op_clearlist_1:
op_clearlist_2:
  if (!val_is_lst(_TOP_12)) E_BADTYPE;
  _val_lst_clear(__lst_ptr(_TOP_12));
  NEXT;

op_quote_0: STATE_0TO1;
op_quote_1:
op_quote_2:
  VM_TRY(val_code_wrap(&_TOP_12));
  NEXT;

op_wrap_0: STATE_0TO1;
op_wrap_1:
op_wrap_2:
  VM_TRY(val_list_wrap(&_TOP_12));
  NEXT;

op_wrapn_0: STATE_0TO1;
op_wrapn_1:
op_wrapn_2:
  if (!val_is_int(_TOP_12) || !HAVE(__val_int(_TOP_12))) E_BADARGS;
  i = __val_int(_TOP_12);
  VM_TRY(val_list_wrap_arr(&_TOP_12,stack-i,i));
  stack -= i;
  STATE_1;
  NEXT;

op_protect_0: STATE_0TO1;
op_protect_1:
op_protect_2:
  VM_TRY(val_protect(&_TOP_12));
  NEXT;

  //TODO: use the math ops defined in val_math.h
op_add_0: STATE_0TO1;
op_add_1: STATE_1TO2;
op_add_2:
  //TODO: for type checking we can use the tag bits to create an integer we can effectiently get the case from (using e.g. switch)
  //TODO: standardize debug val semantics for math ops (currently second (first arg pushed) retains)
  STATE_1;
  __val_dbg_destroy(_TOP_2); //destroy dbg val of top (second retains)
  if (val_is_int(_TOP_2)) {
    if (val_is_int(_SECOND_2)) {
      __val_set(&_SECOND_2, __int_val( __val_int(_SECOND_2) + __val_int(_TOP_2) ) );
      _POP_2;
      NEXT;
    } else if (val_is_double(_SECOND_2)) {
      __val_set(&_SECOND_2,__dbl_val( __val_dbl(_SECOND_2) + (double)__val_int(_TOP_2) ));
      _POP_2;
      NEXT;
    }
  } else if (val_is_double(_TOP_2)) {
    if (val_is_int(_SECOND_2)) {
      __val_set(&_SECOND_2,__dbl_val( (double)__val_int(_SECOND_2) + __val_dbl(_TOP_2) ));
      _POP_2;
      NEXT;
    } else if (val_is_double(_SECOND_2)) {
      __val_set(&_SECOND_2,__dbl_val( __val_dbl(_SECOND_2) + __val_dbl(_TOP_2) ));
      _POP_2;
      NEXT;
    }
  }
  E_BADTYPE;

op_sub_0: STATE_0TO1;
op_sub_1: STATE_1TO2;
op_sub_2:
  STATE_1;
  __val_dbg_destroy(_TOP_2); //destroy dbg val of top (second retains)
  if (val_is_int(_TOP_2)) {
    if (val_is_int(_SECOND_2)) {
      __val_set(&_SECOND_2, __int_val( __val_int(_SECOND_2) - __val_int(_TOP_2) ) );
      _POP_2;
      NEXT;
    } else if (val_is_double(_SECOND_2)) {
      __val_set(&_SECOND_2,__dbl_val( __val_dbl(_SECOND_2) - (double)__val_int(_TOP_2) ));
      _POP_2;
      NEXT;
    }
  } else if (val_is_double(_TOP_2)) {
    if (val_is_int(_SECOND_2)) {
      __val_set(&_SECOND_2,__dbl_val( (double)__val_int(_SECOND_2) - __val_dbl(_TOP_2) ));
      _POP_2;
      NEXT;
    } else if (val_is_double(_SECOND_2)) {
      __val_set(&_SECOND_2,__dbl_val( __val_dbl(_SECOND_2) - __val_dbl(_TOP_2) ));
      _POP_2;
      NEXT;
    }
  }
  E_BADTYPE;

op_mul_0: STATE_0TO1;
op_mul_1: STATE_1TO2;
op_mul_2:
  STATE_1;
  __val_dbg_destroy(_TOP_2); //destroy dbg val of top (second retains)
  if (val_is_int(_TOP_2)) {
    if (val_is_int(_SECOND_2)) {
      __val_set(&_SECOND_2, __int_val( __val_int(_SECOND_2) * __val_int(_TOP_2) ) );
      _POP_2;
      NEXT;
    } else if (val_is_double(_SECOND_2)) {
      __val_set(&_SECOND_2,__dbl_val( __val_dbl(_SECOND_2) * (double)__val_int(_TOP_2) ));
      _POP_2;
      NEXT;
    }
  } else if (val_is_double(_TOP_2)) {
    if (val_is_int(_SECOND_2)) {
      __val_set(&_SECOND_2,__dbl_val( (double)__val_int(_SECOND_2) * __val_dbl(_TOP_2) ));
      _POP_2;
      NEXT;
    } else if (val_is_double(_SECOND_2)) {
      __val_set(&_SECOND_2,__dbl_val( __val_dbl(_SECOND_2) * __val_dbl(_TOP_2) ));
      _POP_2;
      NEXT;
    }
  }
  E_BADTYPE;

op_div_0: STATE_0TO1;
op_div_1: STATE_1TO2;
op_div_2:
  STATE_1;
  __val_dbg_destroy(_TOP_2); //destroy dbg val of top (second retains)
  if (val_is_int(_TOP_2)) {
    if (val_is_int(_SECOND_2)) {
      __val_set(&_SECOND_2, __int_val( __val_int(_SECOND_2) / __val_int(_TOP_2) ) );
      _POP_2;
      NEXT;
    } else if (val_is_double(_SECOND_2)) {
      __val_set(&_SECOND_2,__dbl_val( __val_dbl(_SECOND_2) / (double)__val_int(_TOP_2) ));
      _POP_2;
      NEXT;
    }
  } else if (val_is_double(_TOP_2)) {
    if (val_is_int(_SECOND_2)) {
      __val_set(&_SECOND_2,__dbl_val( (double)__val_int(_SECOND_2) / __val_dbl(_TOP_2) ));
      _POP_2;
      NEXT;
    } else if (val_is_double(_SECOND_2)) {
      __val_set(&_SECOND_2,__dbl_val( __val_dbl(_SECOND_2) / __val_dbl(_TOP_2) ));
      _POP_2;
      NEXT;
    }
  }
  E_BADTYPE;

op_inc_0: STATE_0TO1;
op_inc_1:
op_inc_2:
  if (val_is_int(_TOP_12)) {
    ++*(int32_t*)(&_TOP_12);
  } else if (val_is_double(_TOP_12)) {
    __val_set(&_TOP_12, __dbl_val(__val_dbl(_TOP_12) + 1));
  } else {
    E_BADTYPE;
  }
  NEXT;

op_dec_0: STATE_0TO1;
op_dec_1:
op_dec_2:
  if (val_is_int(_TOP_12)) {
    --*(int32_t*)(&_TOP_12);
  } else if (val_is_double(_TOP_12)) {
    __val_set(&_TOP_12, __dbl_val(__val_dbl(_TOP_12) - 1));
  } else {
    E_BADTYPE;
  }
  NEXT;

op_neg_0: STATE_0TO1;
op_neg_1:
op_neg_2:
  if (val_is_int(_TOP_12)) {
    *(int32_t*)(&_TOP_12) *= -1;
  } else if (val_is_double(_TOP_12)) {
    __val_set(&_TOP_12, __dbl_val(-__val_dbl(_TOP_12)));
  } else {
    E_BADTYPE;
  }
  NEXT;

op_abs_0: STATE_0TO1;
op_abs_1:
op_abs_2:
  if (val_is_int(_TOP_12)) {
    i = __val_int(_TOP_12);
    if (i<0) {
      *(int32_t*)(&_TOP_12) *= -1;
    }
  } else if (val_is_double(_TOP_12)) {
    f = __val_dbl(_TOP_12);
    if (f<0) {
      f *= -1;
      __val_set(&_TOP_12, __dbl_val(f));
    }
  } else {
    E_BADTYPE;
  }
  NEXT;

  //TODO: sqrt and log -- cast to double or maintain number type???
  // - pow already upcasts, so maybe that is what we should do here too
op_sqrt_0: STATE_0TO1;
op_sqrt_1:
op_sqrt_2:
  if (val_is_int(_TOP_12)) {
    __val_set(&_TOP_12, __int_val( (int)sqrt( (double)__val_int(_TOP_12) ) ));
  } else if (val_is_double(_TOP_12)) {
    __val_set(&_TOP_12, __dbl_val(sqrt(__val_dbl(_TOP_12))));
  } else {
    E_BADTYPE;
  }
  NEXT;

op_log_0: STATE_0TO1;
op_log_1:
op_log_2:
  if (val_is_int(_TOP_12)) {
    __val_set(&_TOP_12, __int_val( (int)log( (double)__val_int(_TOP_12) ) ));
  } else if (val_is_double(_TOP_12)) {
    __val_set(&_TOP_12, __dbl_val(log(__val_dbl(_TOP_12))));
  } else {
    E_BADTYPE;
  }
  NEXT;

op_pow_0: STATE_0TO1;
op_pow_1: STATE_1TO2;
op_pow_2:
  STATE_1; //after add only top is guaranteed
  __val_dbg_destroy(_TOP_2); //destroy dbg val of top (second retains)
  if (val_is_int(_TOP_2)) {
    if (val_is_int(_SECOND_2)) {
      __val_set(&_SECOND_2, __dbl_val(pow((double)__val_int(_SECOND_2),(double)__val_int(_TOP_2))));
      _POP_2;
      NEXT;
    } else if (val_is_double(_SECOND_2)) {
      __val_set(&_SECOND_2, __dbl_val( pow( __val_dbl(_SECOND_2),(double)__val_int(_TOP_2) ) ));
      _POP_2;
      NEXT;
    }
  } else if (val_is_double(_TOP_2)) {
    if (val_is_int(_SECOND_2)) {
      __val_set(&_SECOND_2, __dbl_val( pow( (double)__val_int(_SECOND_2), __val_dbl(_TOP_2) ) ));
      _POP_2;
      NEXT;
    } else if (val_is_double(_SECOND_2)) {
      __val_set(&_SECOND_2, __dbl_val( pow( __val_dbl(_SECOND_2), __val_dbl(_TOP_2) ) ));
      _POP_2;
      NEXT;
    }
  }
  E_BADTYPE;

op_mod_0: STATE_0TO1;
op_mod_1: STATE_1TO2;
op_mod_2:
  if (!val_is_int(_TOP_2) || !val_is_int(_SECOND_2)) E_BADTYPE;
  __val_dbg_destroy(_TOP_2);
  __val_set(&_SECOND_2, __int_val(__val_int(_SECOND_2) % __val_int(_TOP_2)));
  _POP_2;
  NEXT;

op_bit_and_0: STATE_0TO1;
op_bit_and_1: STATE_1TO2;
op_bit_and_2:
  if (!val_is_int(_TOP_2) || !val_is_int(_SECOND_2)) E_BADTYPE;
  __val_dbg_destroy(_TOP_2);
  __val_set(&_SECOND_2, __int_val(__val_int(_SECOND_2) & __val_int(_TOP_2)));
  _POP_2;
  NEXT;
op_bit_or_0: STATE_0TO1;
op_bit_or_1: STATE_1TO2;
op_bit_or_2:
  if (!val_is_int(_TOP_2) || !val_is_int(_SECOND_2)) E_BADTYPE;
  __val_dbg_destroy(_TOP_2);
  __val_set(&_SECOND_2, __int_val(__val_int(_SECOND_2) | __val_int(_TOP_2)));
  _POP_2;
  NEXT;
op_bit_xor_0: STATE_0TO1;
op_bit_xor_1: STATE_1TO2;
op_bit_xor_2:
  if (!val_is_int(_TOP_2) || !val_is_int(_SECOND_2)) E_BADTYPE;
  __val_dbg_destroy(_TOP_2);
  __val_set(&_TOP_12, __int_val(__val_int(_SECOND_2) ^ __val_int(_TOP_2)));
  _POP_2;
  NEXT;
op_bit_not_0: STATE_0TO1;
op_bit_not_1:
op_bit_not_2:
  if (!val_is_int(_TOP_12)) E_BADTYPE;
  __val_set(&_TOP_12, __int_val( ~__val_int(_TOP_12)));
  NEXT;

op_bit_lshift_0: STATE_0TO1;
op_bit_lshift_1: STATE_1TO2;
op_bit_lshift_2:
  if (!val_is_int(_TOP_2) || !val_is_int(_SECOND_2)) E_BADTYPE;
  __val_dbg_destroy(_TOP_2);
  __val_set(&_SECOND_2, __int_val(__val_int(_SECOND_2) << __val_int(_TOP_2)));
  _POP_2;
  NEXT;
op_bit_rshift_0: STATE_0TO1;
op_bit_rshift_1: STATE_1TO2;
op_bit_rshift_2:
  if (!val_is_int(_TOP_2) || !val_is_int(_SECOND_2)) E_BADTYPE;
  __val_dbg_destroy(_TOP_2);
  __val_set(&_SECOND_2, __int_val(__val_int(_SECOND_2) >> __val_int(_TOP_2)));
  _POP_2;
  NEXT;

op_lt_0: STATE_0TO1;
op_lt_1: STATE_1TO2;
op_lt_2:
  t = __int_val(val_lt(_SECOND_2,_TOP_2));
  val_destroy(*(--stack));
  val_clear(stack);
  val_destroy(_TOP_2);
  _TOP_2 = t;
  STATE_1;
  NEXT;
op_le_0: STATE_0TO1;
op_le_1: STATE_1TO2;
op_le_2:
  t = __int_val(!val_lt(_TOP_2,_SECOND_2));
  val_destroy(*(--stack));
  val_clear(stack);
  val_destroy(_TOP_2);
  _TOP_2 = t;
  STATE_1;
  NEXT;
op_gt_0: STATE_0TO1;
op_gt_1: STATE_1TO2;
op_gt_2:
  t = __int_val(val_lt(_TOP_2,_SECOND_2));
  val_destroy(*(--stack));
  val_clear(stack);
  val_destroy(_TOP_2);
  _TOP_2 = t;
  STATE_1;
  NEXT;
op_ge_0: STATE_0TO1;
op_ge_1: STATE_1TO2;
op_ge_2:
  t = __int_val(!val_lt(_SECOND_2,_TOP_2));
  val_destroy(*(--stack));
  val_clear(stack);
  val_destroy(_TOP_2);
  _TOP_2 = t;
  STATE_1;
  NEXT;
op_eq_0: STATE_0TO1;
op_eq_1: STATE_1TO2;
op_eq_2:
  t = __int_val(val_eq(_SECOND_2,_TOP_2));
  val_destroy(*(--stack));
  val_clear(stack);
  val_destroy(_TOP_2);
  _TOP_2 = t;
  STATE_1;
  NEXT;
op_ne_0: STATE_0TO1;
op_ne_1: STATE_1TO2;
op_ne_2:
  t = __int_val(!val_eq(_SECOND_2,_TOP_2));
  val_destroy(*(--stack));
  val_clear(stack);
  val_destroy(_TOP_2);
  _TOP_2 = t;
  STATE_1;
  NEXT;
op_compare_0: STATE_0TO1;
op_compare_1: STATE_1TO2;
op_compare_2:
  t = __int_val(val_compare(_SECOND_2,_TOP_2));
  val_destroy(*(--stack));
  val_clear(stack);
  val_destroy(_TOP_2);
  _TOP_2 = t;
  STATE_1;
  NEXT;

op_bool_0: STATE_0TO1;
op_bool_1: 
op_bool_2: 
  t = __int_val(0 != val_as_bool(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;

op_not_0: STATE_0TO1;
op_not_1: 
op_not_2: 
  t = __int_val(0 == val_as_bool(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;

op_and_0: STATE_0TO1;
op_and_1: STATE_1TO2;
op_and_2: //TODO: clean up OP_and/OP_or
  if (val_ispush(_SECOND_2)) {
    i = val_as_bool(_SECOND_2);
    val_destroy(*(--stack)); val_clear(stack);
    if (i==0 || val_ispush(_TOP_2)) {
      t = __int_val(0!=i && 0!=val_as_bool(_TOP_2));
      val_destroy(_TOP_2);
      _TOP_2 = t;
      STATE_1;
      NEXT;
    } else {
      WPUSH2(__op_val(OP_bool),_TOP_2);
      STATE_0;
      NEXTW;
    }
  } else {
    WPUSH2(__op_val(OP_bool),__op_val(OP_only));
    WPROTECT(_TOP_2);
    WPUSH(_SECOND_2);
    val_clear(--stack);
    STATE_0;
    NEXTW;
  }

op_and__0: STATE_0TO1;
op_and__1: STATE_1TO2;
op_and__2:
  t = __int_val(0!=val_as_bool(_SECOND_2) && 0!=val_as_bool(_TOP_2));
  __val_reset(&_SECOND_2, t);
  POP_2;
  NEXT;

op_or_0: STATE_0TO1;
op_or_1: STATE_1TO2;
op_or_2:
  if (val_ispush(_SECOND_2)) {
    i = val_as_bool(_SECOND_2);
    val_destroy(*(--stack)); val_clear(stack);
    if (i || val_ispush(_TOP_2)) {
      t = __int_val(0!=i || 0!=val_as_bool(_TOP_2));
      val_destroy(_TOP_2);
      _TOP_2 = t;
      STATE_1;
      NEXT;
    } else {
      WPUSH2(__op_val(OP_bool),_TOP_2);
      STATE_0;
      NEXTW;
    }
  } else {
    WPUSH2(__op_val(OP_bool),__op_val(OP_unless));
    WPROTECT(_TOP_2);
    WPUSH(_SECOND_2);
    val_clear(--stack);
    STATE_0;
    NEXTW;
  }

op_or__0: STATE_0TO1;
op_or__1: STATE_1TO2;
op_or__2:
  t = __int_val(0!=val_as_bool(_SECOND_2) || 0!=val_as_bool(_TOP_2));
  __val_reset(&_SECOND_2, t);
  POP_2;
  NEXT;

op_find_0: STATE_0TO1;
op_find_1: STATE_1TO2;
op_find_2:
  if (!val_is_str(_TOP_2) || !val_is_str(_SECOND_2)) E_BADTYPE;
  t = __int_val(_val_str_findstr(__str_ptr(_SECOND_2),__str_ptr(_TOP_2)));
  __val_reset(&_TOP_2,t); //preserves debug val from search string on index (list debug val destroyed)
  POPD_2;
  NEXT;

op_parsenum_0: STATE_0TO1;
op_parsenum_1: 
op_parsenum_2: 
  if (!val_is_str(_TOP_12)) E_BADTYPE;
  VM_TRY(val_num_parse(&t,_val_str_begin(__str_ptr(_TOP_12)),_val_str_len(__str_ptr(_TOP_12))));
  __val_reset(&_TOP_12,t); //preserve debug val from string
  NEXT;

op_toint_0: STATE_0TO1;
op_toint_1: 
op_toint_2: 
  if (val_is_double(_TOP_12)) {
    t = __int_val((int32_t)__val_dbl(_TOP_12));
    __val_reset(&_TOP_12,t);
  } else if (!val_is_int(_TOP_12)) {
    E_BADTYPE;
  }
  NEXT;

op_tofloat_0: STATE_0TO1;
op_tofloat_1: 
op_tofloat_2: 
  if (val_is_int(_TOP_12)) {
    t = __dbl_val((double)__val_int(_TOP_12));
    __val_reset(&_TOP_12,t);
  } else if (!val_is_double(_TOP_12)) {
    E_BADTYPE;
  }
  NEXT;

op_tostring_0: STATE_0TO1;
op_tostring_1: 
op_tostring_2: 
  if (!val_is_str(_TOP_12)) {
    VM_TRY(val_tostring(&t,_TOP_12));
    __val_reset(&_TOP_12,t);
  }
  __str_ptr(_TOP_12)->type = TYPE_STRING;
  NEXT;

op_toident_0: STATE_0TO1;
op_toident_1: 
op_toident_2: 
  if (!val_is_str(_TOP_12)) {
    VM_TRY(val_tostring(&t,_TOP_12));
    __val_reset(&_TOP_12,t);
  }
  __str_ptr(_TOP_12)->type = TYPE_IDENT;
  NEXT;

op_substr_0: STATE_0TO1;
op_substr_1: 
op_substr_2: 
  if (!HAVE(2)) E_EMPTY;
  if (!val_is_str(_THIRD_2) || !val_is_int(_SECOND_2) || !val_is_int(_TOP_2)) E_BADTYPE;
  _val_str_substr(__str_ptr(_THIRD_2),__val_int(_SECOND_2),__val_int(_TOP_2));
  __val_dbg_destroy(_SECOND_2); //don't need actual destroy since just int
  __val_dbg_destroy(_TOP_2);
  top = _THIRD_2;
  val_clear(--stack);
  val_clear(--stack);
  STATE_1;
  NEXT;

op_trim_0: STATE_0TO1;
op_trim_1: 
op_trim_2: 
  if (!val_is_str(_TOP_12)) E_BADTYPE;
  _val_str_trim(__str_ptr(_TOP_12));
  NEXT;

  //TODO: a bunch of typecheck ops, or a few generic ones and builtins
op_isnum_0: STATE_0TO1;
op_isnum_1: 
op_isnum_2: 
  t = __int_val(val_is_int(_TOP_12) || val_is_double(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;
op_isint_0: STATE_0TO1;
op_isint_1: 
op_isint_2: 
  t = __int_val(val_is_int(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;
op_isfloat_0: STATE_0TO1;
op_isfloat_1: 
op_isfloat_2: 
  t = __int_val(val_is_double(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;
op_isstring_0: STATE_0TO1;
op_isstring_1: 
op_isstring_2: 
  t = __int_val(val_is_string(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;
op_isident_0: STATE_0TO1;
op_isident_1: 
op_isident_2: 
  t = __int_val(val_is_ident(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;
op_isnative_0: STATE_0TO1;
op_isnative_1: 
op_isnative_2: 
  t = __int_val(val_is_op(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;
op_islist_0: STATE_0TO1;
op_islist_1: 
op_islist_2: 
  t = __int_val(val_is_list(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;
op_iscode_0: STATE_0TO1;
op_iscode_1: 
op_iscode_2: 
  t = __int_val(val_is_code(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;
op_islisttype_0: STATE_0TO1;
op_islisttype_1: 
op_islisttype_2: 
  t = __int_val(val_is_lst(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;
op_isdict_0: STATE_0TO1;
op_isdict_1: 
op_isdict_2: 
  t = __int_val(val_is_dict(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;
op_isref_0: STATE_0TO1;
op_isref_1: 
op_isref_2: 
  t = __int_val(val_is_ref(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;
op_isfile_0: STATE_0TO1;
op_isfile_1: 
op_isfile_2: 
  t = __int_val(val_is_file(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;
op_isvm_0: STATE_0TO1;
op_isvm_1: 
op_isvm_2: 
  t = __int_val(val_is_vm(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;
op_ispush_0: STATE_0TO1;
op_ispush_1: 
op_ispush_2: 
  t = __int_val(val_ispush(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = t;
  NEXT;

op_dip_0: STATE_0TO1;
op_dip_1: STATE_1TO2;
op_dip_2:
  WPROTECT(_SECOND_2);
  val_clear(--stack);
  WPUSH(_TOP_2);
  STATE_0;
  NEXTW;
op_dip2_0: STATE_0TO1;
op_dip2_1:
op_dip2_2:
  if (!HAVE(2)) E_BADARGS;
  WPROTECT(_SECOND_2);
  WPROTECT(_THIRD_2);
  val_clear(--stack);
  val_clear(--stack);
  WPUSH(_TOP_2);
  STATE_0;
  NEXTW;
op_dip3_0: STATE_0TO1;
op_dip3_1:
op_dip3_2:
  if (!HAVE(3)) E_BADARGS;
  VM_TRY(val_list_wrap3(&t,_FOURTH_2,_THIRD_2,_SECOND_2));
  val_clear(--stack);
  val_clear(--stack);
  val_clear(--stack);
  WPUSH3(__op_val(OP_expand),t,_TOP_2);
  STATE_0;
  NEXTW;
op_dipn_0: STATE_0TO1;
op_dipn_1: STATE_1TO2;
op_dipn_2:
  if (!val_is_int(_TOP_2) || !HAVE(1+__val_int(_TOP_2))) E_BADARGS;
  i = __val_int(_TOP_2); __val_dbg_destroy(_TOP_2);
  VM_TRY(val_list_wrap_arr(&t,stack-i-1,i));
  WPUSH3(__op_val(OP_expand),t,_SECOND_2);
  val_clear(&_SECOND_2);
  stack -= i+1;
  STATE_0;
  NEXTW;

op_sip_0: STATE_0TO1;
op_sip_1: STATE_1TO2;
op_sip_2:
  VM_TRY(val_clone(&t,_SECOND_2));
  WPROTECT(t);
  WPUSH(_TOP_2);
  _POP_2;
  NEXTW;
op_sip2_0: STATE_0TO1;
op_sip2_1:
op_sip2_2:
  if (!HAVE(2)) E_BADARGS;
  VM_TRY(val_clone(&t,_SECOND_2));
  WPROTECT(t);
  VM_TRY(val_clone(&t,_THIRD_2));
  WPROTECT(t);
  WPUSH(_TOP_2);
  _POP_2;
  NEXTW;
op_sip3_0: STATE_0TO1;
op_sip3_1:
op_sip3_2:
  if (!HAVE(3)) E_BADARGS;
  VM_TRY(val_list_wrap3_clone(&t,_FOURTH_2,_THIRD_2,_SECOND_2));
  WPUSH3(__op_val(OP_expand),t,_TOP_2);
  _POP_2;
  NEXTW;
op_sipn_0: STATE_0TO1;
op_sipn_1:
op_sipn_2:
  if (!val_is_int(_TOP_2) || !HAVE(1+__val_int(_TOP_2))) E_BADARGS;
  i = __val_int(_TOP_2);
  VM_TRY(val_list_wrap_arr_clone(&t,stack-i-1,i));
  WPUSH3(__op_val(OP_expand),t,_SECOND_2);
  val_clear(--stack);
  STATE_0;
  NEXTW;

//FIXME: 0apply,1apply,2apply,3apply,napply -- potential to leak on err
op_0apply_0: STATE_0TO1;
op_0apply_1:
op_0apply_2:
  STATE_0;
  FIXSTACK;
  t = val_empty_list();
  _val_lst_swap(__lst_ptr(t),stackval);
  stackbase = stack = NULL;
  RESERVE;
  //FIXME: don't leak t on err (also find the others)
  WPUSH3(__op_val(OP_restore),t,top);
  NEXT;
op_1apply_0: STATE_0TO1;
op_1apply_1:
op_1apply_2:
  if (!HAVE(1)) E_EMPTY;
  STATE_0;
  FIXSTACK;
  VM_TRY(_val_lst_splitn(stackval,&t,_val_lst_len(stackval)-1));
  _val_lst_swap(stackval,__lst_ptr(t));
  stackbase = NULL; stack = stackbase+1;
  RESERVE;
  //FIXME: don't leak t on err (also find the others)
  WPUSH3(__op_val(OP_restore),t,top);
  NEXT;
op_2apply_0: STATE_0TO1;
op_2apply_1:
op_2apply_2:
  if (!HAVE(2)) E_EMPTY;
  STATE_0;
  FIXSTACK;
  VM_TRY(_val_lst_splitn(stackval,&t,_val_lst_len(stackval)-2));
  _val_lst_swap(stackval,__lst_ptr(t));
  stackbase = NULL; stack = stackbase+2;
  RESERVE;
  //FIXME: don't leak t on err (also find the others)
  WPUSH3(__op_val(OP_restore),t,top);
  NEXT;
op_3apply_0: STATE_0TO1;
op_3apply_1:
op_3apply_2:
  if (!HAVE(3)) E_EMPTY;
  STATE_0;
  FIXSTACK;
  VM_TRY(_val_lst_splitn(stackval,&t,_val_lst_len(stackval)-3));
  _val_lst_swap(stackval,__lst_ptr(t));
  stackbase = NULL; stack = stackbase+3;
  RESERVE;
  //FIXME: don't leak t on err (also find the others)
  WPUSH3(__op_val(OP_restore),t,top);
  NEXT;
op_napply_0: STATE_0TO1;
op_napply_1: STATE_1TO2;
op_napply_2:
  if (!val_is_int(top) || !HAVE(__val_int(top))) E_EMPTY;
  i = __val_int(top); __val_dbg_destroy(_TOP_2);
  top = *(--stack); val_clear(stack);
  STATE_0;
  FIXSTACK;
  VM_TRY(_val_lst_splitn(stackval,&t,_val_lst_len(stackval)-i));
  _val_lst_swap(stackval,__lst_ptr(t));
  stackbase = NULL; stack = stackbase+i;
  RESERVE;
  //FIXME: don't leak t on err (also find the others)
  WPUSH3(__op_val(OP_restore),t,top);
  NEXT;

op_if_0: STATE_0TO1;
op_if_1: STATE_1TO2;
op_if_2:
  if (val_ispush(_SECOND_2)) {
    goto op_if__2;
  } else {
    WPUSH(__op_val(OP_if_));
    WPROTECT(_TOP_2);
    WPUSH(_SECOND_2);
    _POP2_2;
    NEXTW;
  }
op_if__0: STATE_0TO1;
op_if__1: STATE_1TO2;
op_if__2:
  if (val_as_bool(_SECOND_2)) {
    WPUSH(_TOP_2);
    val_destroy(_SECOND_2);
    val_clear(--stack); STATE_0;
    NEXTW;
  } else {
    POP2_2;
    NEXT;
  }

op_ifelse_0: STATE_0TO1;
op_ifelse_1:
op_ifelse_2:
  if (!HAVE(2)) E_EMPTY;
  if (val_ispush(_THIRD_2)) {
    goto op_ifelse__2;
  } else {
    WPUSH(__op_val(OP_ifelse_));
    WPROTECT(_TOP_2);
    WPROTECT(_SECOND_2);
    WPUSH(_THIRD_2);
    STATE_0;
    val_clear(--stack);
    val_clear(--stack);
    NEXTW;
  }

op_ifelse__0: STATE_0TO1;
op_ifelse__1:
op_ifelse__2:
  if (!HAVE(2)) E_EMPTY;
  if (val_as_bool(_THIRD_2)) {
    val_destroy(_TOP_2);
    WPUSH(_SECOND_2); val_clear(--stack);
  } else {
    WPUSH(_TOP_2);
    val_destroy(*(--stack)); val_clear(stack);
  }
  STATE_0;
  val_destroy(*(--stack)); val_clear(stack);
  NEXTW;

op_only_0: STATE_0TO1;
op_only_1: STATE_1TO2;
op_only_2:
  if (val_as_bool(_SECOND_2)) {
    WPUSH(_TOP_2);
    val_destroy(_SECOND_2);
    val_clear(--stack); STATE_0;
    NEXTW;
  } else {
    POP_2;
    NEXT;
  }

op_unless_0: STATE_0TO1;
op_unless_1: STATE_1TO2;
op_unless_2:
  if (val_as_bool(_SECOND_2)) {
    POP_2;
    NEXT;
  } else {
    WPUSH(_TOP_2);
    val_destroy(_SECOND_2);
    val_clear(--stack); STATE_0;
    NEXTW;
  }

op_swaplt_0: STATE_0TO1;
op_swaplt_1: STATE_1TO2;
op_swaplt_2:
  if(val_lt(_SECOND_2,_TOP_2)) {
    val_swap(&_SECOND_2,&_TOP_2);
  }
  NEXT;
op_swapgt_0: STATE_0TO1;
op_swapgt_1: STATE_1TO2;
op_swapgt_2:
  if(val_lt(_TOP_2,_SECOND_2)) {
    val_swap(&_SECOND_2,&_TOP_2);
  }
  NEXT;

op_list_0:
op_list_1:
op_list_2:
  FIXSTACK;
  vm_list(vm);
  NEXT;

op_print_0: STATE_0TO1;
op_print_1:
op_print_2:
  //VM_TRY(val_print(top));
  VM_TRY(val_printv(_TOP_12));
  POP_12;
  NEXT;
op_print__0: STATE_0TO1;
op_print__1:
op_print__2:
  //VM_TRY(val_print_(top));
  VM_TRY(val_printv_(_TOP_12));
  POP_12;
  NEXT;
op_printV_0: STATE_0TO1;
op_printV_1:
op_printV_2:
  //VM_TRY(val_print_code(top));
  VM_TRY(val_printV(_TOP_12));
  POP_12;
  NEXT;
op_printV__0: STATE_0TO1;
op_printV__1:
op_printV__2:
  //VM_TRY(val_print_code_(top));
  VM_TRY(val_printV_(_TOP_12));
  POP_12;
  NEXT;
op_printf_0: STATE_0TO1;
op_printf_1:
op_printf_2:
  if (!val_is_str(_TOP_12)) E_BADTYPE;
  STATE_0;
  FIXSTACK; //fixes stack (without top, so we still need to destroy after)
  e = val_printfv(__str_ptr(top), vm->open_list,1);
  RESTORESTACK;
  val_destroy(top);
  if (0>e) HANDLE_e;
  NEXT;
op_fprintf_0: STATE_0TO1;
op_fprintf_1: STATE_1TO2;
op_fprintf_2:
  if (!val_is_str(_TOP_2) || !val_is_file(_SECOND_2)) E_BADTYPE;
  t = _SECOND_2; val_clear(--stack);
  STATE_0;
  FIXSTACK; //fixes stack (without top, so we still need to destroy after)
  e = val_fprintfv(_val_file_f(__file_ptr(t)),__str_ptr(top), vm->open_list,1, vm->open_list,1);
  RESTORESTACK;
  val_destroy(top);
  top = t; STATE_1; //move file back to top of stack
  if (0>e) HANDLE_e;
  NEXT;
op_sprintf_0: STATE_0TO1;
op_sprintf_1:
op_sprintf_2:
  if (!val_is_str(_TOP_12)) E_BADTYPE;
  STATE_0;
  FIXSTACK; //fixes stack (without top, so we still need to destroy after)
  t = val_empty_string();
  e = val_sprintfv(__str_ptr(t),__str_ptr(top), vm->open_list,1, vm->open_list,1);
  //_val_lst_cleanderef(vm->open_list);
  RESTORESTACK;
  RESERVE;
  val_destroy(top);
  top = t; STATE_1; //move buf string to top of stack
  if (0>e) HANDLE_e;
  NEXT;
  //this version of sprintf cats onto existing string instead of making new one
  //  - acts as a "stringbuilder", so faster for many individual appends, but requires empty string arg when just creating a string
  //if (!val_is_str(_TOP_2) || !val_is_str(_SECOND_2)) E_BADTYPE;
  //t = _SECOND_2; val_clear(--stack);
  //STATE_0;
  //FIXSTACK;
  //e = val_sprintfv(__str_ptr(t),__str_ptr(top), vm->open_list,1, vm->open_list,1);
  //RESTORESTACK;
  //val_destroy(top);
  //top = t; STATE_1; //move buf string to top of stack
  //if (0>e) HANDLE_e;
  //NEXT;
op_printlf_0: STATE_0TO1;
op_printlf_1: STATE_1TO2;
op_printlf_2:
  if (!val_is_str(_TOP_12) || !val_is_lst(_SECOND_2)) E_BADTYPE;
  e = val_printfv(__str_ptr(_TOP_2), __lst_ptr(_SECOND_2),0);
  POP_2;
  if (0>e) HANDLE_e;
  NEXT;
op_sprintlf_0: STATE_0TO1;
op_sprintlf_1: STATE_1TO2;
op_sprintlf_2:
  if (!val_is_str(_TOP_12) || !val_is_lst(_SECOND_2)) E_BADTYPE;
  t = val_empty_string();
  e = val_sprintfv(__str_ptr(t),__str_ptr(_TOP_2), __lst_ptr(_SECOND_2),0, __lst_ptr(_SECOND_2),0);

  if (0>e) {
    HANDLE_e;
  } else {
    __val_reset(&_TOP_2,t);
  }
  NEXT;
op_printlf2_0: STATE_0TO1;
op_printlf2_1:
op_printlf2_2:
  if (!HAVE(2)) E_MISSINGARGS; STATE_2;
  if (!val_is_str(_TOP_12) || !val_is_lst(_SECOND_2) || !val_is_lst(_THIRD_2)) E_BADTYPE;
  e = val_fprintfv(stdout,__str_ptr(_TOP_2), __lst_ptr(_THIRD_2),0, __lst_ptr(_SECOND_2),0);

  if (0>e) {
    HANDLE_e;
  } else {
    val_destroy(_TOP_2);
    _TOP_2 = _SECOND_2;
    val_clear(--stack); //we stay in state 2 since we had at least 3 on top of stack (now 2)
  }
  NEXT;
op_sprintlf2_0: STATE_0TO1;
op_sprintlf2_1:
op_sprintlf2_2:
  if (!HAVE(2)) E_MISSINGARGS; STATE_2;
  if (!val_is_str(_TOP_12) || !val_is_lst(_SECOND_2) || !val_is_lst(_THIRD_2)) E_BADTYPE;
  t = val_empty_string();
  e = val_sprintfv(__str_ptr(t),__str_ptr(_TOP_2), __lst_ptr(_THIRD_2),0, __lst_ptr(_SECOND_2),0);

  if (0>e) {
    HANDLE_e;
  } else {
    __val_reset(&_TOP_2, t);
  }
  NEXT;
op_clear_0:
op_clear_1:
op_clear_2:
  FIXSTACK;
  _val_lst_clear(stackval);
  RESTORESTACK; //will automatically alloc initial stack space
  NEXT;
op_qstate_0:
op_qstate_1:
op_qstate_2:
  //FIXSTACKS;
  //vm_qstate(vm);
  QSTATE;
  NEXT;
op_vstate_0:
op_vstate_1:
op_vstate_2:
  //FIXSTACKS;
  //vm_vstate(vm);
  VSTATE;
  NEXT;

op_defined_0: STATE_0TO1;
op_defined_1:
op_defined_2:
  if (!val_is_str(_TOP_12)) E_BADARGS;
  t = vm_dict_get(vm,__str_ptr(_TOP_12));
  val_destroy(_TOP_12);
  _TOP_12 = __int_val(val_is_null(t) ? 0 : 1);
  NEXT;
op_getdef_0: STATE_0TO1;
op_getdef_1:
op_getdef_2:
  if (!val_is_str(_TOP_12)) E_BADARGS;
  t = vm_dict_get(vm,__str_ptr(_TOP_12));
  if (val_is_null(t)) {
    fflush(stdout);
    fprintf(stderr,"unknown word '%.*s'\n",_val_str_len(__str_ptr(_TOP_12)),_val_str_begin(__str_ptr(_TOP_12))); //TODO: only when in terminal
    fflush(stderr);
    E_UNDEFINED;
  }
  val_destroy(_TOP_12);
  VM_TRY(val_clone(&_TOP_12,t));
  NEXT;
op_def_0: STATE_0TO1;
op_def_1: STATE_1TO2;
op_def_2:
  if (!val_is_str(_TOP_2)) E_BADARGS;
  __val_dbg_destroy(_TOP_2); //dict doesn't keep debug val for key
  if (0>(e = vm_dict_put(vm,__str_ptr(_TOP_2),_SECOND_2))) HANDLE_e;
  val_clear(--stack);
  STATE_0;
  NEXT;
op_mapdef_0: STATE_0TO1;
op_mapdef_1: STATE_1TO2;
op_mapdef_2:
  if (!val_is_str(_TOP_2)) E_BADARGS;
  val_clear(&t);
  VM_TRY(_val_dict_swap(&vm->dict,__str_ptr(_TOP_2),&t));
  WPUSH(__op_val(OP_def));
  WPROTECT(_TOP_2);
  WPUSH(_SECOND_2);
  top = t;
  _POPD_2;
  NEXTW;
op_resolve_0: STATE_0TO1;
op_resolve_1:
op_resolve_2:
  VM_TRY(vm_val_resolve(vm,&_TOP_12));
  NEXT;
op_rresolve_0: STATE_0TO1;
op_rresolve_1:
op_rresolve_2:
  VM_TRY(vm_val_rresolve(vm,&_TOP_12));
  NEXT;
op_scope_0: STATE_0TO1;
op_scope_1:
op_scope_2:
  VM_TRY(_val_dict_newscope(&vm->dict));
  WPUSH2(__op_val(OP__endscope),_TOP_12);
  _POP_12;
  NEXTW;
op_savescope_0: STATE_0TO1;
op_savescope_1:
op_savescope_2:
  VM_TRY(_val_dict_newscope(&vm->dict));
  WPUSH2(__op_val(OP__popscope),_TOP_12);
  _POP_12;
  NEXTW;
op__endscope_0:
op__endscope_1:
op__endscope_2:
  //FIXME: add debug opcode assert macro -- normally we can assume dict has > 1 scopes since this is internal opcode
  if (!(_val_dict_canpop(&vm->dict))) E_EMPTY;
  _val_dict_dropscope(&vm->dict);
  NEXT;
op__popscope_0:
op__popscope_1:
op__popscope_2:
  //FIXME: add debug opcode assert macro
  if (!(_val_dict_canpop(&vm->dict))) E_EMPTY;
  _val_dict_popscope(&vm->dict,&t);
  PUSH(t);
  NEXT;
op_usescope_0: STATE_0TO1;
op_usescope_1: STATE_1TO2;
op_usescope_2:
  if (!val_is_dict(_SECOND_2)) E_BADTYPE;
  _val_dict_pushscope(&vm->dict,__dict_ptr(_SECOND_2));
  val_clear(--stack);
  WPUSH2(__op_val(OP__popscope),_TOP_2);
  STATE_0;
  NEXTW;
op_usescope__0: STATE_0TO1;
op_usescope__1: STATE_1TO2;
op_usescope__2:
  if (!val_is_dict(_SECOND_2)) E_BADTYPE;
  _val_dict_pushscope(&vm->dict,__dict_ptr(_SECOND_2));
  val_clear(--stack);
  WPUSH2(__op_val(OP__endscope),_TOP_2);
  STATE_0;
  NEXTW;
op_dict_has_0: STATE_0TO1;
op_dict_has_1: STATE_1TO2;
op_dict_has_2:
  if (!val_is_str(_TOP_2) || !val_is_dict(_SECOND_2)) E_BADARGS;
  __val_dbg_destroy(_TOP_2); //drop debug val from key string
  t = __int_val(val_is_null(_val_dict_get(__dict_ptr(_SECOND_2),__str_ptr(_TOP_2))) ? 0 : 1);
  val_destroy(_TOP_2);
  _TOP_2 = t;
  NEXT;
op_dict_get_0: STATE_0TO1;
op_dict_get_1: STATE_1TO2;
op_dict_get_2:
  if (!val_is_str(_TOP_2) || !val_is_dict(_SECOND_2)) E_BADARGS;
  __val_dbg_destroy(_TOP_2); //drop debug val from key string
  if (val_is_null(t = _val_dict_get(__dict_ptr(_SECOND_2),__str_ptr(_TOP_2)))) E_UNDEFINED;
  val_destroy(_TOP_2);
  VM_TRY(val_clone(&_TOP_2,t));
  NEXT;
op_dict_put_0: STATE_0TO1;
op_dict_put_1:
op_dict_put_2:
  if (!HAVE(2) || !val_is_str(_TOP_2) || !val_is_dict(_THIRD_2)) E_BADARGS;
  __val_dbg_destroy(_TOP_2); //drop debug val from key string
  if (0>(e = _val_dict_put(__dict_ptr(_THIRD_2),__str_ptr(_TOP_2),_SECOND_2))) HANDLE_e; e=0;
  _POP2_2;
  NEXT;

op_open_0: STATE_0TO1;
op_open_1: STATE_1TO2;
op_open_2:
  if (!val_is_str(_SECOND_2) || !val_is_str(_TOP_2)) E_BADARGS;
  VM_TRY(_val_str_make_cstr(__str_ptr(_SECOND_2)));
  VM_TRY(_val_str_make_cstr(__str_ptr(_TOP_2)));
  VM_TRY(val_file_init(&t,_val_str_begin(__str_ptr(_SECOND_2)),_val_str_begin(__str_ptr(_TOP_2))));
  val_destroy(_TOP_2);
  _TOP_2 = t;
  POPD_2;
  NEXT;

op_close_0: STATE_0TO1;
op_close_1:
op_close_2:
  if (val_is_file(_TOP_12)) {
    VM_TRY(_val_file_close(__file_ptr(_TOP_12)));
  } else if (val_is_fd(_TOP_12)) {
    VM_TRY(_val_fd_close(__fd_ptr(_TOP_12)));
  } else {
    E_BADARGS;
  }
  NEXT;

op_readline_0: STATE_0TO1;
op_readline_1:
op_readline_2:
  if (!val_is_file(_TOP_12)) E_BADARGS;
  t = val_empty_string();
  if (0>(n = _val_file_readline(__file_ptr(_TOP_12),__str_ptr(t)))) {
    val_destroy(t);
    t = __int_val(n);
  }
  PUSH(t);
  NEXT;

op_stdin_readline_0:
op_stdin_readline_1:
op_stdin_readline_2:
  t = val_empty_string();
  if (0>(n = _val_file_readline(&_val_file_stdin,__str_ptr(t)))) {
    val_destroy(t);
    t = __int_val(n);
  }
  PUSH(t);
  NEXT;

op_read_0: STATE_0TO1;
op_read_1: STATE_1TO2;
op_read_2:
  if (!val_is_int(_TOP_2)) E_BADARGS;
  if (val_is_file(_SECOND_2)) {
    t = val_empty_string();
    if (0>(n = _val_file_read(__file_ptr(_SECOND_2),__str_ptr(t),__val_int(_TOP_2)))) {
      val_destroy(t);
      t = __int_val(n);
    }
  } else if (val_is_fd(_SECOND_2)) {
    t = val_empty_string();
    if (0>(n = _val_fd_read(__fd_ptr(_SECOND_2),__str_ptr(t),__val_int(_TOP_2)))) {
      val_destroy(t);
      t = __int_val(n);
    }
  } else {
    E_BADARGS;
  }

  __val_set(&_TOP_2, t); //can skip destroy since just an inline int
  NEXT;

op_write_0: STATE_0TO1;
op_write_1: STATE_1TO2;
op_write_2:
  if (!val_is_string(_TOP_2)) E_BADARGS;
  if (val_is_file(_SECOND_2)) {
    VM_TRY(_val_file_write(__file_ptr(_SECOND_2),__str_ptr(_TOP_2)));
  } else if (val_is_fd(_SECOND_2)) {
    VM_TRY(_val_fd_write(__fd_ptr(_SECOND_2),__str_ptr(_TOP_2)));
  } else {
    E_BADARGS;
  }
  POP_2;
  NEXT;

op_seek_0: STATE_0TO1;
op_seek_1: STATE_1TO2;
op_seek_2:
  if (!val_is_int(_TOP_2)) E_BADARGS;
  i = __val_int(_TOP_2); __val_dbg_destroy(_TOP_2);
  _POP_2; //top is int so we don't need to destroy

  if (val_is_file(_TOP_1)) {
    if (0>(e = _val_file_seek(__file_ptr(_TOP_1),i,SEEK_SET))) HANDLE_e;
  } else if(val_is_fd(_TOP_1)) {
    if (0>(e = _val_fd_seek(__fd_ptr(_TOP_1),i,SEEK_SET))) HANDLE_e;
  } else {
    E_BADARGS;
  }
  NEXT;

op_fpos_0: STATE_0TO1;
op_fpos_1:
op_fpos_2:
  if (val_is_file(_SECOND_2)) {
    PUSH(__int_val(_val_file_pos(__file_ptr(_TOP_12))));
  } else if(val_is_fd(_SECOND_2)) {
    PUSH(__int_val(_val_fd_pos(__fd_ptr(_TOP_12))));
  } else {
    E_BADARGS;
  }
  NEXT;

  //FIXME: validate full error checking and error recovery/cleanup for ref ops
op_ref_0: STATE_0TO1;
op_ref_1:
op_ref_2:
  VM_TRY(val_ref_wrap(&_TOP_12));
  NEXT;
op_deref_0: STATE_0TO1;
op_deref_1:
op_deref_2:
  if (!val_is_ref(_TOP_12)) E_BADTYPE;
  VM_TRY(val_ref_unwrap(&_TOP_12));
  NEXT;
op_refswap_0: STATE_0TO1; //TODO: refswap semantics: B ref(A) -- A ref(B) OR ref(A) B -- ref(B) A
op_refswap_1: STATE_1TO2;
op_refswap_2:
  if (!val_is_ref(_SECOND_2)) E_BADTYPE;
  VM_TRY(val_ref_swap(__ref_ptr(_SECOND_2),&_TOP_2));
  NEXT;

op_guard_0: STATE_0TO1;
op_guard_1: STATE_1TO2;
op_guard_2:
  if (!val_is_ref(_SECOND_2)) E_BADTYPE;
  VM_TRY(_val_ref_lock(__ref_ptr(_SECOND_2)));
  val_clear(&t);
  _val_ref_swap(__ref_ptr(_SECOND_2),&t);
  VM_TRY(vm_cpush2(vm,_SECOND_2,__op_val(OP_catch_unguard)));
  val_clear(--stack);
  WPUSH2(__op_val(OP_unguard),_TOP_2);
  PUSH_0(t);
  NEXTW;
op_guard_sig_0: STATE_0TO1;
op_guard_sig_1: STATE_1TO2;
op_guard_sig_2:
  if (!val_is_ref(_SECOND_2)) E_BADTYPE;
  VM_TRY(_val_ref_lock(__ref_ptr(_SECOND_2)));
  val_clear(&t);
  _val_ref_swap(__ref_ptr(_SECOND_2),&t);
  VM_TRY(vm_cpush2(vm,_SECOND_2,__op_val(OP_catch_unguard)));
  val_clear(--stack);
  WPUSH2(__op_val(OP_unguard_sig),_TOP_2);
  PUSH_0(t);
  NEXTW;
op_guard_bcast_0: STATE_0TO1;
op_guard_bcast_1: STATE_1TO2;
op_guard_bcast_2:
  if (!val_is_ref(_SECOND_2)) E_BADTYPE;
  VM_TRY(_val_ref_lock(__ref_ptr(_SECOND_2)));
  val_clear(&t);
  _val_ref_swap(__ref_ptr(_SECOND_2),&t);
  VM_TRY(vm_cpush2(vm,_SECOND_2,__op_val(OP_catch_unguard)));
  val_clear(--stack);
  WPUSH2(__op_val(OP_unguard_bcast),_TOP_2);
  PUSH_0(t);
  NEXTW;
op_guard_waitwhile_0: STATE_0TO1;
op_guard_waitwhile_1:
op_guard_waitwhile_2:
  // ref(x) [testwait] [postwait] guard.waitwhile  --  <|> guard x [testwait] (ref() [testwait] [postwait]) waitwhile_
  // x testwait -- x bool
  if (!HAVE(2)) E_MISSINGARGS;
  if (!val_is_ref(_THIRD_2)) E_BADTYPE;

  //TODO: or should we just get ref from cont (and not also include it in waitwhile_ arg???
  VM_TRY(val_list_wrap3(&_THIRD_2,_THIRD_2,_SECOND_2,_TOP_2));
  p=_val_lst_begin(__lst_ptr(_THIRD_2));
  VM_TRY(val_clone(&_SECOND_2,p[1]));
  WPUSH3(__op_val(OP_waitwhile_),_THIRD_2,_SECOND_2);
  val_clear(--stack);
  val_clear(--stack);
  STATE_1;

  tv = __ref_ptr(p[0]);
  VM_TRY(_val_ref_lock(tv));
  VM_TRY(_val_ref_clone(&_TOP_2,tv));
  VM_TRY(vm_cpush2(vm,_TOP_2,__op_val(OP_catch_unguard)));
  val_clear(&_TOP_2);
  _val_ref_swap(__ref_ptr(p[0]),&_TOP_2);

  NEXTW;
op_guard_sigwaitwhile_0: STATE_0TO1;
op_guard_sigwaitwhile_1:
op_guard_sigwaitwhile_2:
  // ref(x) [testwait] [postwait] guard.sigwaitwhile  --  <|> guard x [testwait] (ref() [testwait] [postwait]) sigwaitwhile_
  // x testwait -- x bool
  if (!HAVE(2)) E_MISSINGARGS;
  if (!val_is_ref(_THIRD_2)) E_BADTYPE;

  VM_TRY(val_list_wrap3(&_THIRD_2,_THIRD_2,_SECOND_2,_TOP_2)); //FIXME: cleanup t
  p=_val_lst_begin(__lst_ptr(_THIRD_2));
  VM_TRY(val_clone(&_SECOND_2,p[1]));
  WPUSH3(__op_val(OP_sigwaitwhile_),_THIRD_2,_SECOND_2);
  val_clear(--stack);
  val_clear(--stack);
  STATE_1;

  VM_TRY(_val_ref_lock(__ref_ptr(p[0])));
  VM_TRY(_val_ref_clone(&_TOP_2,tv));
  VM_TRY(vm_cpush2(vm,_TOP_2,__op_val(OP_catch_unguard)));
  val_clear(&_TOP_2);
  _val_ref_swap(__ref_ptr(p[0]),&_TOP_2);

  NEXTW;

op_unguard_0: STATE_0TO1;
op_unguard_1:
op_unguard_2:
  // x ||< ref() catch_unguard --  ref(x)
  //need to cpop catch+ref, swap in top, and unlock ref
  VM_TRY(_val_lst_rpop(&vm->cont,&t));
  if (t != __op_val(OP_catch_unguard)) E_BADOP; //TODO: debug assert
  VM_TRY(_val_lst_rpop(&vm->cont,&t));
  if (!val_is_ref(t)) E_BADTYPE; //TODO: debug assert
  tv = __ref_ptr(t);

  _val_ref_swap(tv,&_TOP_12);
  VM_TRY(_val_ref_unlock(__ref_ptr(t)));
  if (!val_is_null(_TOP_12)) E_BADOP; //TODO: debug assert
  _TOP_12 = t;
  
  NEXT;
  
op_unguard_sig_0: STATE_0TO1;
op_unguard_sig_1:
op_unguard_sig_2:
  //need to cpop catch+ref, swap in top, signal and unlock ref
  if ((e = _val_lst_rpop(&vm->cont,&t))) E_FATAL(e);
  if (t != __op_val(OP_catch_unguard)) E_BADOP; //TODO: debug assert
  if ((e = _val_lst_rpop(&vm->cont,&t))) E_FATAL(e);
  if (!val_is_ref(t)) E_BADTYPE; //TODO: debug assert
  tv = __ref_ptr(t);

  _val_ref_swap(tv,&_TOP_12);
  if ((e = _val_ref_signal(tv))) {
    if ((ee = _val_ref_unlock(__ref_ptr(t)))) E_FATAL(ee);
    HANDLE_e;
  }
  VM_TRY(_val_ref_unlock(__ref_ptr(t)));
  if (!val_is_null(_TOP_12)) E_BADOP; //TODO: debug assert
  _TOP_12 = t;
  
  NEXT;
  
op_unguard_bcast_0: STATE_0TO1;
op_unguard_bcast_1:
op_unguard_bcast_2:
  //need to cpop catch+ref, swap in top, broadcast to all waiters and unlock ref
  if ((e = _val_lst_rpop(&vm->cont,&t))) E_FATAL(e);
  if (t != __op_val(OP_catch_unguard)) E_BADOP; //TODO: debug assert
  if ((e = _val_lst_rpop(&vm->cont,&t))) E_FATAL(e);
  if (!val_is_ref(t)) E_BADTYPE; //TODO: debug assert
  tv = __ref_ptr(t);

  _val_ref_swap(tv,&_TOP_12);
  if ((e = _val_ref_broadcast(tv))) {
    if ((ee = _val_ref_unlock(__ref_ptr(t)))) E_FATAL(ee);
    HANDLE_e;
  }
  VM_TRY(_val_ref_unlock(__ref_ptr(t)));
  if (!val_is_null(_TOP_12)) E_BADOP; //TODO: debug assert
  _TOP_12 = t;
  
  NEXT;
  
op_catch_unguard_0: STATE_0TO1;
op_catch_unguard_1:
op_catch_unguard_2:
  //need to cpop ref, unlock, and destroy ref
  //TODO: debug asserts to validate args instead of if statement -- these are internal opcodes so can (normally) assume args are valid
  if ((ee = _val_lst_rpop(&vm->cont,&t))) E_FATAL(ee);
  if (!val_is_ref(t)) E_FATAL(ERR_BADTYPE);
  if ((ee = _val_ref_unlock(__ref_ptr(t)))) { e = ee; HANDLE_e; }
  val_destroy(t);
  e = ERR_THROW; //original exception already on top of stack, pop next continuation handler
  HANDLE_e;

op_waitwhile__0: STATE_0TO1;
op_waitwhile__1:
op_waitwhile__2:
  // x bool (ref() [testwait] [postwait]) waitwhile_  --  x postwait unguard  OR  wait(ref(x)) x testwait (ref() [testwait] [postwait]) waitwhile_
  //FIXME: test + debug
  //FIXME: on err, cleanup lock and cont
  if (!HAVE(2)) E_MISSINGARGS; //TODO: debug assert -- missing args means vm bug (or mucking about with debug tools)
  if (!val_is_list(_TOP_12)) E_BADARGS; //FIXME: debug assert top=lst, len(top)=3, top[0]=ref
  p = _val_lst_begin(__lst_ptr(_TOP_12));
  tv = __ref_ptr(p[0]);
  if (val_as_bool(_SECOND_2)) { //still need to wait -- swap in val, unlock, wait, lock, testwait
    _val_ref_swap(tv,&_THIRD_2);
    if (!val_is_null(_THIRD_2)) E_BADOP; //TODO: debug assert

    VM_TRY(_val_ref_wait(tv)); //unlock and wait

    VM_TRY(val_clone(&t,p[1])); //clone testwait for next check

    //TODO: do all the stack manip here (outside the lock)

    if ((e = _val_ref_lock(tv))) { val_destroy(t); HANDLE_e; }
    _val_ref_swap(tv,&_THIRD_2);

    WPUSH3(__op_val(OP_waitwhile_),top,t); //TODO: destroy t on err

    val_destroy(*(--stack)); val_clear(stack); //destroy+clear bool
    top = *(--stack); val_clear(stack); //place x on top
    STATE_1;
  } else { //done waiting -- postwait and unlock
    VM_TRY(_val_lst_rpop(__lst_ptr(top),&t));
    WPUSH2(__op_val(OP_unguard),t);
    POP2_2;
  }

  NEXTW;
op_sigwaitwhile__0: STATE_0TO1;
op_sigwaitwhile__1:
op_sigwaitwhile__2:
  // x bool (ref() [testwait] [postwait]) waitwhile_  --  x postwait unguard  OR  wait(ref(x)) x testwait (ref() [testwait] [postwait]) waitwhile_
  //FIXME: test + debug
  //FIXME: on err, cleanup lock and cont
  if (!HAVE(2)) E_MISSINGARGS; //TODO: debug assert -- missing args means vm bug (or mucking about with debug tools)
  if (!val_is_list(_TOP_12)) E_BADARGS; //FIXME: debug assert top=lst, len(top)=3, top[0]=ref
  p = _val_lst_begin(__lst_ptr(top));
  tv = __ref_ptr(p[0]);
  if (val_as_bool(_SECOND_2)) { //still need to wait -- swap in val, unlock, wait, lock, testwait
    _val_ref_swap(tv,&_THIRD_2);
    if (!val_is_null(_THIRD_2)) E_BADOP; //TODO: debug assert

    VM_TRY(_val_ref_signal(tv)); //sig before unlock
    VM_TRY(_val_ref_wait(tv)); //unlock and wait

    VM_TRY(val_clone(&t,p[1])); //clone testwait for next check

    //TODO: do all the stack manip here (outside the lock)

    if ((e = _val_ref_lock(tv))) { val_destroy(t); HANDLE_e; }
    _val_ref_swap(tv,&_THIRD_2);

    WPUSH3(__op_val(OP_waitwhile_),top,t); //TODO: destroy t on err

    val_destroy(*(--stack)); val_clear(stack); //destroy+clear bool
    top = *(--stack); val_clear(stack); //place x on top
    STATE_1;
  } else { //done waiting -- postwait and unlock
    VM_TRY(_val_lst_rpop(__lst_ptr(top),&t));
    WPUSH2(__op_val(OP_unguard),t);
    POP2_2;
  }

  NEXTW;

op_wait_0: STATE_0TO1; //TODO: IMPLEMENTME (or remove -- we don't directly use these at language level)
op_wait_1:
op_wait_2:
  E_NOIMPL;
op_signal_0: STATE_0TO1;
op_signal_1:
op_signal_2:
  E_NOIMPL;
op_broadcast_0: STATE_0TO1;
op_broadcast_1:
op_broadcast_2:
  E_NOIMPL;

op_vm_0: STATE_0TO1;
op_vm_1: STATE_1TO2;
op_vm_2:
  if (!val_is_lst(_TOP_2) || !val_is_lst(_SECOND_2)) E_BADTYPE;
  __val_dbg_destroy(_SECOND_2); //vm keeps debug val of work stack (top), drops second debug val

  //VM_TRY(val_vm_init2(&t,__lst_ptr(_SECOND_2),__lst_ptr(_TOP_2))); //TODO: do we copy dict to child vm???
  //  - if we don't copy dict, then default init child dict (could pass scope/dict val to child as needed)
  //FIXME: make dict is threadsafe (either do deep clone, or don't copy over, or something)
  VM_TRY(_val_dict_clone(&t,&vm->dict));
  VM_TRY_t(val_vm_init3(&t,__lst_ptr(_SECOND_2),__lst_ptr(_TOP_2),__dict_ptr(t)));
  __val_set(&_TOP_2, t); //replace top val with vm (retains old top debug)
  _POPD_2;
  NEXT;
op_thread_0: STATE_0TO1;
op_thread_1: STATE_1TO2;
op_thread_2:
  if (!val_is_lst(_TOP_2) || !val_is_lst(_SECOND_2)) E_BADTYPE;
  __val_dbg_destroy(_SECOND_2); //vm keeps debug val of work stack (top), drops second debug val
  //FIXME: make dict is threadsafe (either do deep clone, or don't copy over, or something)
  //  - see op_vm above
  VM_TRY(_val_dict_clone(&t,&vm->dict));
  //FIXME: make sure we pass threadsafe stacks (e.g. any refcounted objects should have their own references so we can CoW)
  VM_TRY_t(val_vm_init3(&t,__lst_ptr(_SECOND_2),__lst_ptr(_TOP_2),__dict_ptr(t)));
  tv = __vm_ptr(t);
  VM_TRY(_val_lst_deref(&tv->v.vm->stack));
  VM_TRY(_val_lst_deref(&tv->v.vm->work));
  __val_set(&_TOP_2, t); //replace top val with vm (retains old top debug)
  _POPD_2;
  VM_TRY(_val_vm_runthread(__vm_ptr(t)));
  NEXT;
op_debug_0:
op_debug_1:
op_debug_2:
  FIXSTACKS;
  VM_TRY(vm_debug_wrap(vm));
  RESTORESTACKS;
  NEXTW;
op_vm_exec_0:
op_vm_exec_1:
op_vm_exec_2:
  FIXSTACKS;
  VM_TRY(vm_exec(vm));
  RESTORESTACKS;
  NEXTW;

op_vm_continue_0: STATE_0TO1;
op_vm_continue_1:
op_vm_continue_2:
  if (!val_is_vm(_TOP_12)) E_BADTYPE;
  d = __vm_ptr(_TOP_12)->v.vm;
  t = __int_val(vm_dowork(d));
  PUSH(t);
  NEXT;

op_vm_thread_0: STATE_0TO1;
op_vm_thread_1:
op_vm_thread_2:
  if (!val_is_vm(_TOP_12)) E_BADTYPE;
  tv = __vm_ptr(_TOP_12);
  VM_TRY(_val_lst_deref(&tv->v.vm->stack));
  VM_TRY(_val_lst_deref(&tv->v.vm->work));
  VM_TRY(_val_lst_deref(&tv->v.vm->cont));
  VM_TRY(_val_vm_runthread(__vm_ptr(_TOP_12)));
  NEXT;

op_vm_stack_0: STATE_0TO1;
op_vm_stack_1:
op_vm_stack_2:
  if (!val_is_vm(_TOP_12)) E_BADTYPE;
  d = __vm_ptr(_TOP_12)->v.vm;
  VM_TRY(val_clone(&t,__lst_val(&d->stack)));
  PUSH(t);
  NEXT;

op_vm_wstack_0: STATE_0TO1;
op_vm_wstack_1:
op_vm_wstack_2:
  if (!val_is_vm(_TOP_12)) E_BADTYPE;
  d = __vm_ptr(_TOP_12)->v.vm;
  VM_TRY(val_clone(&t,__lst_val(&d->work)));
  PUSH(t);
  NEXT;

op_vm_setstack_0: STATE_0TO1;
op_vm_setstack_1: STATE_1TO2;
op_vm_setstack_2:
  if (!val_is_vm(_SECOND_2) || !val_is_lst(_TOP_2)) E_BADTYPE;
  __val_dbg_destroy(_TOP_2); //lost since vm stack is direct valstruct pointer
  d = __vm_ptr(_SECOND_2)->v.vm;
  tv = __lst_ptr(_TOP_2);
  _val_lst_destroy_(&d->stack);
  d->stack = *tv; _valstruct_release(tv);
  NEXT;
op_vm_wsetstack_0: STATE_0TO1;
op_vm_wsetstack_1: STATE_1TO2;
op_vm_wsetstack_2:
  if (!val_is_vm(_SECOND_2) || !val_is_lst(_TOP_2)) E_BADTYPE;
  __val_dbg_destroy(_TOP_2); //lost since vm stack is direct valstruct pointer
  d = __vm_ptr(_SECOND_2)->v.vm;
  tv = __lst_ptr(_TOP_2);
  _val_lst_destroy_(&d->work);
  d->work = *tv; _valstruct_release(tv);
  NEXT;

op_vm_qstate_0: STATE_0TO1;
op_vm_qstate_1:
op_vm_qstate_2:
  if (!val_is_vm(_TOP_12)) E_BADTYPE;
  d = __vm_ptr(_TOP_12)->v.vm;
  VM_TRY(vm_qstate(d));
  NEXT;
op_vm_vstate_0: STATE_0TO1;
op_vm_vstate_1:
op_vm_vstate_2:
  if (!val_is_vm(_TOP_12)) E_BADTYPE;
  d = __vm_ptr(_TOP_12)->v.vm;
  VM_TRY(vm_vstate(d));
  NEXT;

op_catch_debug_0:
op_catch_debug_1:
op_catch_debug_2:
  FIXSTACKS;
  vm_perror(vm); //TODO: normalize error printing -- also, does catch_debug need to be opcode, or could it just be in dict???
  vm_drop(vm); //pop error we just printed
  vm_qstate(vm);
  VM_TRY(vm_debug_wrap(vm));
  RESTORESTACKS; //reset pointers
  NEXTW;

op_trycatch_0: STATE_0TO1;
op_trycatch_1: STATE_1TO2;
op_trycatch_2:
  t = _SECOND_2; //code to try
  val_clear(--stack);
  STATE_0;
  FIXSTACKS; //still have catch handler in top, try code in t

  VM_TRY(vm_trycatch(vm,t,top));

  RESTORESTACKS;
  NEXTW;

op_trydebug_0: STATE_0TO1;
op_trydebug_1:
op_trydebug_2:
  VM_TRY(_val_lst_rpush(&vm->cont,__op_val(OP_catch_debug)));

  WPUSH2(__op_val(OP__endtrydebug),_TOP_12);
  _POP_12;

  NEXT;

op__catch_0: STATE_0TO1;
op__catch_1:
op__catch_2:
  //everything did not go well, so restore state from cont
  //first save error from stack
  //e = __val_int(top); //exception already on top -- don't need to handle THROW
  //if (e == ERR_THROW || e == ERR_USER_THROW) {
  //  if (!HAVE(1)) E_FATAL(ERR_MISSINGARGS);
  //  t = _SECOND_2;
  //  val_clear(--stack);
  //} else {
  //  t = top;
  //}
  t = top; _POP_12;
  FIXSTACKS;

  VM_TRY(vm_catch(vm,t));

  workval = &vm->work;
  stackval = vm->open_list;
  RESTORESTACKS;

  NEXTW;

op__endtry_0:
op__endtry_1:
op__endtry_2:
  //everything went well, so just pop the 4 cont vals we pushed
  //TODO: rdrop4
  VM_TRY(_val_lst_rdrop(&vm->cont));
  VM_TRY(_val_lst_rdrop(&vm->cont));
  VM_TRY(_val_lst_rdrop(&vm->cont));
  VM_TRY(_val_lst_rdrop(&vm->cont));
  NEXT;

op__endtrydebug_0:
op__endtrydebug_1:
op__endtrydebug_2:
  //everything went well, so just pop the debugcatch handler
  VM_TRY(_val_lst_rdrop(&vm->cont));
  NEXT;

op_throw_0:
op_throw_1:
op_throw_2:
  e = ERR_USER_THROW;
  HANDLE_e;

op_perror_0: STATE_0TO1;
op_perror_1:
op_perror_2:
  VM_TRY(errval_fprintf(stdout,top));
  POP_12;
  NEXT;

op_open_code_0:
op_open_code_1:
op_open_code_2:
  FIXSTACKS;
  VM_TRY(_vm_open_code(vm));
  SET_NOEVAL_RETURN;
  NEXT;
//TODO: special handling for () and [] (so we don't need to allocate space for the empty lists)
op_open_list_0:
op_open_list_1:
op_open_list_2:
  PUSH(val_empty_list());
  vm->open_list = __list_ptr(_TOP_12);
  vm->groupi++;
  FIXSTACKS;
  stackval = vm->open_list;
  stack = stackbase = NULL;
  RESERVE;
  NEXT;
op_close_list_0:
op_close_list_1:
op_close_list_2:
  FIXSTACKS;
  VM_TRY(_vm_close_list(vm));
  stackval = vm->open_list; RESTORESTACK;
  NEXT;
op_catch_interactive_0:
op_catch_interactive_1:
op_catch_interactive_2:
  FIXSTACKS;
  vm_perror(vm);
  vm_drop(vm); //pop the error code we printed
  vm_reset(vm); //clear work and cont stacks
  RESTOREWSTACK; //reset work pointers
  vm_cpush(vm,__op_val(OP_catch_interactive)); //OP_catch_interactive is sticky
  WPUSH(val_file_stdin_ref()); //wpush stdin
  NEXTW;
op_quit_0:
op_quit_1:
op_quit_2:
  FIXSTACKS;
  exit(0); //TODO: or return???

//
//debug ops follow quit
//
op_debug_eval_0:
op_debug_eval_1:
op_debug_eval_2:
#ifdef DEBUG_VAL_EVAL
  vm->debug_val_eval = 1;
  debug_val_eval = 1;
#else
  E_NODEBUG;
#endif
  NEXT;

op_debug_noeval_0:
op_debug_noeval_1:
op_debug_noeval_2:
#ifdef DEBUG_VAL_EVAL
  vm->debug_val_eval = 0;
  debug_val_eval = 0;
#else
  E_NODEBUG;
#endif
  NEXT;

op_debug_set_0: STATE_0TO1;
op_debug_set_1: STATE_1TO2;
op_debug_set_2:
#ifdef DEBUG_VAL
  //set debug val on second to top (destroy debug val on top/second)
  __val_dbg_destroy(_TOP_2);
  __val_dbg_destroy(_SECOND_2);
  __val_dbg_val(_SECOND_2) = (val64_t)_TOP_2;
  _POP_2;
#else
  //POP_2;
  E_NODEBUG;
#endif
  NEXT;

op_debug_get_0: STATE_0TO1;
op_debug_get_1:
op_debug_get_2:
#ifdef DEBUG_VAL
  dbg = (val_t)__val_dbg_val(_TOP_12);
  _TOP_12 = __val_dbg_strip(_TOP_12);
  PUSH_12(dbg);
#else
  //PUSH_12(NULL);
  E_NODEBUG;
#endif
  NEXT;

  //ifdebug prob doesn't need to be a core opcode -- TODO: maybe we need some general config/features opcodes?
op_ifdebug_0: STATE_0TO1;
op_ifdebug_1:
op_ifdebug_2:
#ifdef DEBUG_VAL
  WPUSH(_TOP_12);
  _POP_12;
  NEXTW;
#else
  POP_12;
#endif
  NEXT;


//
//platform natives below here
//

op_fork_0:
op_fork_1:
op_fork_2:
  pid = fork();
  PUSH(__int_val(pid));
  NEXT;

op_socket_0:
op_socket_1:
op_socket_2:
  VM_TRY(val_fd_init_socket(&t,AF_INET,SOCK_STREAM,0));
  PUSH(t);
  NEXT;

op_socket_dgram_0:
op_socket_dgram_1:
op_socket_dgram_2:
  VM_TRY(val_fd_init_socket(&t,AF_INET,SOCK_DGRAM,0));
  PUSH(t);
  NEXT;

op_socket_listen_0: STATE_0TO1;
op_socket_listen_1:
op_socket_listen_2:
  if (!HAVE(2)) E_MISSINGARGS;
  if (!val_is_fd(_THIRD_2)) E_BADTYPE;
  if (!val_is_str(_SECOND_2)) E_BADTYPE;
  if (!val_is_int(_TOP_2)) E_BADTYPE;

  VM_TRY(_val_str_make_cstr(__str_ptr(_SECOND_2)));

  //TODO: decide on backlog value/parameter
  VM_TRY(_val_fd_listen(__val_ptr(_THIRD_2), _val_str_begin(__str_ptr(_SECOND_2)), __val_int(_TOP_2), 10));
  //POP2_3;
  POP2_2;
  NEXT;

op_socket_accept_0: STATE_0TO1;
op_socket_accept_2: if (state > 1 && stack == stackend) { RESERVE; } STATE_1;
op_socket_accept_1:
  if (!val_is_fd(_TOP_1)) E_BADTYPE;
  VM_TRY(_val_fd_accept(__val_ptr(_TOP_1), &t, (struct sockaddr*)NULL, NULL));
  PUSH_1(t);
  NEXT;

op_socket_connect_0: STATE_0TO1;
op_socket_connect_1:
op_socket_connect_2:
  if (!HAVE(2)) E_MISSINGARGS;
  if (!val_is_fd(_THIRD_2)) E_BADTYPE;
  if (!val_is_str(_SECOND_2)) E_BADTYPE;
  if (!val_is_int(_TOP_2)) E_BADTYPE;

  VM_TRY(_val_str_make_cstr(__str_ptr(_SECOND_2)));
  VM_TRY(_val_fd_connect(__val_ptr(_THIRD_2), _val_str_begin(__str_ptr(_SECOND_2)), __val_int(_TOP_2)));
  //POP2_3;
  POP2_2;
  NEXT;

op_effects_0: STATE_0TO1;
op_effects_1:
op_effects_2:
  // effects is the core to build static analysis tools from (incl. validating stack/stack-effect comments)
  //
  if (!val_is_op(_TOP_12)) E_BADTYPE;
  i = __val_op(_TOP_12);
  //val_destroy(&_TOP_12) //NOTE: not needed since just opcode (except for debugging)
  VM_TRY(val_string_init_cstr(&_TOP_12, op_effects[i], strlen(op_effects[i])));
  NEXT;


handle_err_t:
  val_destroy(t);
  HANDLE_e;

  return _fatal(ERR_FATAL); //should never get here

//if we don't care what line of C code the exception came from, we add a label to jump to that sets the exception there
#ifndef VM_DEBUG_ERR
err_null: e = _throw(ERR_NULL); HANDLE_e;
err_empty: e = _throw(ERR_EMPTY); HANDLE_e;
err_badtype: e = _throw(ERR_BADTYPE); HANDLE_e;
err_badop: e = _throw(ERR_BAD_OP); HANDLE_e;
err_badargs: e = _throw(ERR_BADARGS); HANDLE_e;
err_missingargs: e = _throw(ERR_MISSINGARGS); HANDLE_e;
err_undefined: e = _throw(ERR_UNDEFINED); HANDLE_e;
err_noimpl: e = _throw(ERR_NOT_IMPLEMENTED); HANDLE_e;
err_nodebug: e = _throw(ERR_NO_DEBUG); HANDLE_e;
//err_malloc: e = _throw(ERR_MALLOC); HANDLE_e;
err_fatal: e = _fatal(e); HANDLE_e;
#endif

handle_noeval_err: //TODO: allow recovery from noeval errors
  //RESTORESTACKS;
  FIXSTACKS;
  return e;
handle_err:
  if (err_isfatal(e)) return e;
  else {
    if (e != ERR_THROW && e != ERR_USER_THROW) { //if err not already on stack, push e to stack
      PUSH_fatal(__int_val(e));
      //e = ERR_THROW;
    }
    if (vm_hascont(vm)) {
      if ((e = _val_lst_rpop(&vm->cont,&t))) E_FATAL(e);
      WPUSH_fatal(t);
      //_op_return= vm->noeval ? &&noeval_return : &&loop_return;
      //NEXT;
      NEXTW;
    } else {
      FIXSTACKS;
      return e;
    }
  }

#ifdef VM_DEBUG_STEP
vm_debug_step: //print some debug state before each vm step
  //QSTATE;
  VSTATE;
  if (_op_return == &&loop_return) {
    if (work != workbase) {
      val_fprintf(stdout,"VM_STEP(w%d): %V\n",__int_val(state),work[-1]);
    }
  } else if (_op_return == &&code_return) {
    val_fprintf(stdout,"VM_STEP(c%d): %V\n",__int_val(state),*_val_lst_begin(v));
  } else if (_op_return == &&noeval_return) {
    if (work != workbase) {
      val_fprintf(stdout,"VM_STEP(n%d): %V\n",__int_val(state),work[-1]);
    }
  } else {
    fprintf(stdout,"VM_STEP: Invalid VM op return\n");
  }
  fflush(stdout);
  VALIDATE;
  goto *_op_return;
#endif

//FIXME: undef everything we should
#undef NEXT
#undef E_NULL
#undef E_EMPTY
#undef E_BADARGS
#undef E_BADOP
#undef E_BADTYPE
#undef RESERVE
#undef WRESERVE
#undef FIXSTACKS
#undef FIXSTACK
#undef FIXWSTACK
}
