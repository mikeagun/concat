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

#include "ops_ref.h"

#include "val_ref_internal.h"
#include "val_func.h"
#include "val_list.h"
#include "val_stack.h"
#include "val_cast.h"
#include "ops_internal.h"
#include "vm_err.h"

err_t ops_ref_init(vm_t *vm) {
  err_t r=0;
  if ((r = vm_init_op(vm,"ref",_op_ref))) return r;
  if ((r = vm_init_op(vm,"deref",_op_deref))) return r;
  if ((r = vm_init_op(vm,"refswap",_op_refswap))) return r;

  if ((r = vm_init_op(vm,"guard",_op_guard))) return r;
  if ((r = vm_init_op(vm,"guard.sig",_op_guard_sig))) return r;
  if ((r = vm_init_op(vm,"guard.bcast",_op_guard_bcast))) return r;
  if ((r = vm_init_op(vm,"guard.waitwhile",_op_guard_waitwhile))) return r;
  if ((r = vm_init_op(vm,"guard.sigwaitwhile",_op_guard_sigwaitwhile))) return r;

  if ((r = vm_init_op(vm,"wait",_op_wait))) return r;
  if ((r = vm_init_op(vm,"signal",_op_signal))) return r;
  if ((r = vm_init_op(vm,"broadcast",_op_broadcast))) return r;
  return r;
}

err_t _op_ref(vm_t *vm) {
  __op_get1;
  return val_ref_wrap(p);
}

err_t _op_deref(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_isref(p));
  return val_ref_unwrap(p);
}

err_t _op_refswap(vm_t *vm) {
  __op_get2;
  throw_if(ERR_BADARGS,!val_isref(p));
  return val_ref_swap(p,p+1);
}

err_t _op_guard(vm_t *vm) {
  __op_popget; // ref [body]
  throw_if(ERR_BADARGS,!val_isref(p));
  __op_wpush2;
  __op_cpush2;
  val_func_init(cp+1,_op_unguard_catch,"unguard_catch");
  val_move(cp,p);

  err_t e;
  if ((e = _val_ref_lock(p))) goto bad_nolock;
  val_func_init(q,_op_unguard,"unguard");
  _val_ref_swap(cp,p); //swap ref val to top of stack
  val_move(q+1,p+1); //eval body next
  return 0;

bad_nolock: //no lock, so need to pop cont
  vm_cpopn_(vm,2,&cp);
  val_destroy(cp);
  val_destroy(cp+1);
  return e;
}

err_t _op_guard_sig(vm_t *vm) {
  __op_popget; // ref [body]
  throw_if(ERR_BADARGS,!val_isref(p));
  __op_wpush2;
  __op_cpush2;
  val_func_init(cp+1,_op_unguard_catch,"unguard_catch");
  val_move(cp,p);

  err_t e;
  if ((e = _val_ref_lock(p))) goto bad_nolock;
  val_func_init(q,_op_unguard_sig,"unguard.sig");
  _val_ref_swap(cp,p); //swap ref val to top of stack
  val_move(q+1,p+1); //eval body next
  return 0;

bad_nolock: //no lock, so need to pop cont
  vm_cpopn_(vm,2,&cp);
  val_destroy(cp);
  val_destroy(cp+1);
  return e;
}

err_t _op_guard_bcast(vm_t *vm) {
  __op_popget; // ref [body]
  throw_if(ERR_BADARGS,!val_isref(p));
  __op_wpush2;

  err_t e;

  __op_cpush2;
  val_func_init(cp+1,_op_unguard_catch,"unguard_catch");
  val_move(cp,p);
  if ((e = _val_ref_lock(p))) goto bad_nolock;

  val_func_init(q,_op_unguard_bcast,"unguard.bcast");
  _val_ref_swap(cp,p); //swap ref val to top of stack
  val_move(q+1,p+1); //eval body next
  return 0;

bad_nolock: //no lock, so need to pop cont
  vm_cpopn_(vm,2,&cp);
  val_destroy(cp);
  val_destroy(cp+1);
  return e;
}

err_t _op_guard_waitwhile(vm_t *vm) {
  // ref(x) [prewait] [postwait] guard.waitwhile  --  <|> guard x [prewait] (ref() [prewait] [postwait]) waitwhile_
  // x prewait -- x bool
  
  __op_pop2get; // ref [prewait] [postwait]
  throw_if(ERR_BADARGS,!val_isref(p));
  __op_wpush3;
  err_t e;
  if ((e = val_clone(q+2,p+1))) return e; //next thing to do is prewait (after the unlock)
  if ((e = val_clone(q+1,p))) return e; //temp clone ref to q+1

  __op_cpush2; //set up unlock catch on cont
  val_func_init(cp+1,_op_unguard_catch,"unguard_catch");
  val_move(cp,q+1); //ref clone moved under unguard_catch
  if ((e = _val_ref_lock(p))) goto bad_nolock;

  val_move(q,_val_ref_val(p)); //steal val from ref (temporarily in q where waitwhile_ will be later)
  if ((e = val_list_wrap_arr(q+1,p,3))) return e; //catch handler will unlock
  val_move(p,q); //move ref-val to top of stack
  val_func_init_keep(q,_op_waitwhile_,"waitwhile_");
  return 0;

bad_nolock: //no lock, so need to pop cont
  vm_cpopn_(vm,2,&cp);
  val_destroy(cp);
  val_destroy(cp+1);
  return e;
}

err_t _op_waitwhile_(vm_t *vm) {
  // x bool (ref() [prewait] [postwait]) waitwhile_  --  x postwait unguard  OR  wait(ref(x)) x prewait (ref() [prewait] [postwait]) waitwhile_
  
  __op_pop2get; //x bool (ref() [prewait] [postwait])
  debug_assert_r(val_islist(p+2) && val_list_len(p+2)==3 && val_isref(val_list_head(p+2)));
  int c = val_as_bool(p+1);
  val_destroy(p+1);
  if (c) { //need to wait
    val_t *ref = val_list_head(p+2);
    __op_wpush2;
    err_t e;
    _val_ref_swap(ref,p);
    if ((e = _val_ref_wait(ref))) return e;
    if ((e = _val_ref_lock(ref))) goto bad_nolock;
    _val_ref_swap(ref,p);
    debug_assert_r(val_isvalid(p));
    val_move(q,p+2);
    return val_list_dith(q,1,q+1);

  bad_nolock: //no lock, so need to pop cont
    __op_cpop2;
    val_destroy(cp);
    val_destroy(cp+1);
    return e;
  } else { //else we are ready for postwait and unlock
    _vm_popnext(vm,NULL); //pop waitwhile_ to end loop
    __op_wpush2;
    val_list_ith(p+2,2); //should never fail here (because of debug assert above)
    val_move(q+1,p+2); //post-wait now on top of wstack
    val_func_init(q,_op_unguard,"unguard"); //unguard after postwait
    return 0;
  }
}

err_t _op_guard_sigwaitwhile(vm_t *vm) {
  // ref(x) [prewait] [postwait] guard.waitwhile  --  <|> guard x [prewait] (ref() [prewait] [postwait]) waitwhile_
  // x prewait -- x bool
  
  __op_pop2get; // ref [prewait] [postwait]
  throw_if(ERR_BADARGS,!val_isref(p));
  __op_wpush3;
  err_t e;
  if ((e = val_clone(q+2,p+1))) return e; //next thing to do is prewait (after the unlock)
  if ((e = val_clone(q+1,p))) return e; //temp clone ref to q+1

  __op_cpush2; //set up unlock catch on cont
  val_func_init(cp+1,_op_unguard_catch,"unguard_catch");
  val_move(cp,q+1); //ref clone moved under unguard_catch
  if ((e = _val_ref_lock(p))) goto bad_nolock;

  val_move(q,_val_ref_val(p)); //steal val from ref (temporarily in q where waitwhile_ will be later)
  if ((e = val_list_wrap_arr(q+1,p,3))) return e; //catch handler will unlock
  val_move(p,q); //move ref-val to top of stack
  val_func_init_keep(q,_op_sigwaitwhile_,"sigwaitwhile_");
  return 0;

bad_nolock: //no lock, so need to pop cont
  vm_cpopn_(vm,2,&cp);
  val_destroy(cp);
  val_destroy(cp+1);
  return e;
}
err_t _op_sigwaitwhile_(vm_t *vm) {
  // x bool (ref() [prewait] [postwait]) waitwhile_  --  x postwait unguard  OR  wait(ref(x)) x prewait (ref() [prewait] [postwait]) waitwhile_
  
  __op_pop2get; //x bool (ref() [prewait] [postwait])
  debug_assert_r(val_islist(p+2) && val_list_len(p+2)==3 && val_isref(val_list_head(p+2)));
  int c = val_as_bool(p+1);
  val_destroy(p+1);
  if (c) { //need to wait
    val_t *ref = val_list_head(p+2);
    __op_wpush2;
    err_t e;
    _val_ref_swap(ref,p);
    if ((e = _val_ref_signal(ref))) return e;
    if ((e = _val_ref_wait(ref))) return e;
    if ((e = _val_ref_lock(ref))) goto bad_nolock;
    _val_ref_swap(ref,p);
    val_move(q,p+2);
    return val_list_dith(q,1,q+1);

  bad_nolock: //no lock, so need to pop cont
    __op_cpop2;
    val_destroy(cp);
    val_destroy(cp+1);
    return e;
  } else { //else we are ready for postwait and unlock
    _vm_popnext(vm,NULL); //pop waitwhile_ to end loop
    __op_wpush2;
    val_list_ith(p+2,2); //should never fail here (because of debug assert above)
    val_move(q+1,p+2); //post-wait now on top of wstack
    val_func_init(q,_op_unguard,"unguard"); //unguard after postwait
    return 0;
  }
}


err_t _op_signal(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_isref(p));
  return val_ref_signal(p);
}
err_t _op_broadcast(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_isref(p));
  return val_ref_broadcast(p);
}
err_t _op_wait(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_isref(p));
  return val_ref_wait(p);
}

err_t _op_unguard_catch(vm_t *vm) {
  __op_cpop1; // ref to unlock
  debug_assert_r(val_isref(cp));
  err_t e;
  if ((e = _val_ref_unlock(cp))) return e;
  val_ref_destroy(cp);
  return _throw(ERR_THROW); //since we are a catch handler, original exception on top of stack already
}

err_t _op_unguard(vm_t *vm) {
  __op_get1; //newval
  __op_cpop2; // ref unguard_catch
  debug_assert_r(val_isfunc(cp+1) && val_isref(cp) && _val_ref_trylock(cp) == ERR_LOCKED);
  val_destroy(cp+1);
  _val_ref_swap(cp,p);
  val_move(p,cp);
  return _val_ref_unlock(p);
}

err_t _op_unguard_sig(vm_t *vm) {
  __op_get1; //newval
  __op_cpop2; // ref unguard_catch
  debug_assert_r(val_isfunc(cp+1) && val_isref(cp) && _val_ref_trylock(cp) == ERR_LOCKED);
  val_destroy(cp+1);
  _val_ref_swap(cp,p);
  val_move(p,cp);
  err_t e;
  if ((e = _val_ref_signal(p))) return e;
  return _val_ref_unlock(p);
}
err_t _op_unguard_bcast(vm_t *vm) {
  __op_get1; //newval
  __op_cpop2; // ref unguard_catch
  debug_assert_r(val_isfunc(cp+1) && val_isref(cp) && _val_ref_trylock(cp) == ERR_LOCKED);
  val_destroy(cp+1);
  _val_ref_swap(cp,p);
  val_move(p,cp);
  err_t e;
  if ((e = _val_ref_broadcast(p))) return e;
  return _val_ref_unlock(p);
}
