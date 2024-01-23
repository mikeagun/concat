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

#include "val_ref.h"
#include "val_ref_internal.h"
#include "val_printf.h"
#include "helpers.h"
#include <valgrind/helgrind.h>
#include <errno.h>

inline val_t* _val_ref_val(valstruct_t *ref) { return &ref->v.ref.val; }

err_t val_ref_wrap(val_t *val) {
  valstruct_t *ref;
  if (!(ref = _valstruct_alloc())) return ERR_MALLOC;
  ANNOTATE_RWLOCK_CREATE(ref);
  ref->type = TYPE_REF;
  ref->v.ref.nwait = 0;
  fatal_if(ERR_LOCK,0 != sem_init(&ref->v.ref.lock,0,1));
  fatal_if(ERR_LOCK,0 != sem_init(&ref->v.ref.wait,0,0));
  ref->v.ref.refcount = 1;
  ref->v.ref.val = *val;
  *val = __ref_val(ref);
  return 0;
}
err_t val_ref_unwrap(val_t *ref) {
  err_t e;
  valstruct_t *rv = __val_ptr(*ref);
  if ((e = _val_ref_lock(rv))) return e;
  val_t t;
  if (rv->v.ref.refcount == 1) { //we are the last reference
    t = *_val_ref_val(rv);
    val_clear(_val_ref_val(rv));
  } else {
    if ((e = val_clone(&t,rv->v.ref.val))) goto out_e;
  }
  e = _val_ref_unlock(rv);
  val_destroy(*ref);
  *ref = t;
  return e;
out_e:
  _val_ref_unlock(rv);
  return e;
}

err_t val_ref_swap(valstruct_t *ref, val_t *val) {
  err_t e;
  if ((e = _val_ref_lock(ref))) return e;
  val_swap(&ref->v.ref.val,val);
  if ((e = _val_ref_unlock(ref))) return e;
  return 0;
}

err_t _ref_lock(valstruct_t *ref) {
  err_t e;
  throw_if(ERR_LOCK,(e = sem_wait(&ref->v.ref.lock)));
  ANNOTATE_RWLOCK_ACQUIRED(ref,1);
  return 0;
}
err_t _ref_trylock(valstruct_t *ref) {
  err_t e;
  if (!(e = sem_trywait(&ref->v.ref.lock))) {
    ANNOTATE_RWLOCK_ACQUIRED(ref,1);
    return 0;
  }
  else if (errno == EAGAIN) return ERR_LOCKED;
  else return _throw(ERR_LOCK);
}
err_t _ref_unlock(valstruct_t *ref) {
  err_t e;
  ANNOTATE_RWLOCK_RELEASED(ref,1);
  throw_if(ERR_LOCK,(e = sem_post(&ref->v.ref.lock)));
  return 0;
}

err_t _ref_signal(valstruct_t *ref) {
  if (ref->v.ref.nwait) { //post once if there are waiter(s)
    --ref->v.ref.nwait;
    throw_if(ERR_LOCK,sem_post(&ref->v.ref.wait));
  }
  return 0;
}
err_t _ref_broadcast(valstruct_t *ref) {
  while (ref->v.ref.nwait) { //post once for each waiter
    --ref->v.ref.nwait;
    throw_if(ERR_LOCK,sem_post(&ref->v.ref.wait));
  }
  return 0;
}
err_t _ref_wait(valstruct_t *ref) {
  err_t r;
  ++ref->v.ref.nwait;
  if ((r = _ref_unlock(ref))) return r;

  throw_if(ERR_LOCK,sem_wait(&ref->v.ref.wait));
  return 0;
}

err_t val_ref_signal(valstruct_t *ref) {
  err_t r;
  if ((r = _ref_lock(ref))) return r;
  if ((r = _ref_signal(ref))) {
    _ref_unlock(ref);
    return r;
  } 
  return _ref_unlock(ref);
}
err_t val_ref_broadcast(valstruct_t *ref) {
  err_t r;
  if ((r = _ref_lock(ref))) return r;
  if ((r = _ref_broadcast(ref))) {
    _ref_unlock(ref);
    return r;
  } 
  return _ref_unlock(ref);
}
err_t val_ref_wait(valstruct_t *ref) {
  err_t r;
  if ((r = _ref_lock(ref))) return r;
  return _ref_wait(ref);
}

//TODO: which, if any of these do we need???
void _val_ref_swap(valstruct_t *ref, val_t *val) {
  debug_assert(ERR_LOCKED == _val_ref_trylock(ref)); //we should already have lock before swapping
  val_swap(&ref->v.ref.val,val);
}
err_t _val_ref_lock(valstruct_t *ref) {
  return _ref_lock(ref);
}
err_t _val_ref_trylock(valstruct_t *ref) {
  return _ref_trylock(ref);
}
err_t _val_ref_unlock(valstruct_t *ref) {
  return _ref_unlock(ref);
}
err_t _val_ref_signal(valstruct_t *ref) {
  return _ref_signal(ref);
}
err_t _val_ref_broadcast(valstruct_t *ref) {
  return _ref_broadcast(ref);
}
err_t _val_ref_wait(valstruct_t *ref) {
  debug_assert(ERR_LOCKED == _val_ref_trylock(ref)); //we should already have lock before waiting
  return _ref_wait(ref);
}


err_t _val_ref_clone(val_t *ret, valstruct_t *orig) {
  *ret = __val_val(orig);
  refcount_inc(orig->v.ref.refcount);
  return 0;
}
void _val_ref_destroy(valstruct_t *ref) {
  if (0 == refcount_dec(ref->v.ref.refcount)) {
    ANNOTATE_HAPPENS_AFTER(ref);
    ANNOTATE_HAPPENS_BEFORE_FORGET_ALL(ref);
    //last reference, free resources
    //err_t e; //TODO: trylock as a debug check
    //if ((e = _val_ref_trylock(ref))) { _fatal(e); }
    sem_destroy(&ref->v.ref.lock);
    sem_destroy(&ref->v.ref.wait);
    val_destroy(ref->v.ref.val);
    //ANNOTATE_RWLOCK_RELEASED(ref,1);
    ANNOTATE_RWLOCK_DESTROY(ref);
    _valstruct_release(ref);
  } else {
    ANNOTATE_HAPPENS_BEFORE(ref);
  }
}

int val_ref_fprintf(valstruct_t *ref,FILE *file, const fmt_t *fmt) {
  err_t e;
  if (ERR_LOCKED == (e = _val_ref_trylock(ref))) {
    return val_fprint_cstr(file,"ref(REF LOCKED)");
  } else if (e) return e;
  int rlen = 0;
  if (fmt->conversion == 'V') {
    if(0>(e = val_fprint_cstr(file,"ref("))) return e;
    rlen += e;
  }
  if (0>(e = val_fprintf_(ref->v.ref.val,file,fmt))) goto out_bad;
  rlen += e;
  if ((e = _val_ref_unlock(ref))) return e;
  if (fmt->conversion == 'V') {
    if(0>(e = val_fprint_ch(file,')'))) return e;
    rlen += e;
  }
  return rlen;
out_bad:
  _val_ref_unlock(ref);
  return e;
}
int val_ref_sprintf(valstruct_t *ref,valstruct_t *buf, const fmt_t *fmt) {
  //TODO: use fmt
  err_t e;
  if (ERR_LOCKED == (e = _val_ref_trylock(ref))) {
    return val_sprint_cstr(buf,"ref(REF LOCKED)");
  } else if (e) return e;
  int rlen = 0;
  if (fmt->conversion == 'V') {
    if(0>(e = val_sprint_cstr(buf,"ref("))) return e;
    rlen += e;
  }
  if (0>(e = val_sprintf_(ref->v.ref.val,buf,fmt))) goto out_bad;
  rlen += e;
  if ((e = _val_ref_unlock(ref))) return e;
  if (fmt->conversion == 'V') {
    if(0>(e = val_sprint_ch(buf,')'))) return e;
    rlen += e;
  }
  return rlen;
out_bad:
  _val_ref_unlock(ref);
  return e;
}
