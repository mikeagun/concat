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

#include "val_ref_internal.h"
#include "val_string.h"
#include "val_printf.h"
#include "vm_err.h"
#include "helpers.h"
#include <errno.h>
#include <stdlib.h>

void val_ref_init_handlers(struct type_handlers *h) {
  h->destroy = val_ref_destroy;
  h->clone = val_ref_clone;
  h->sprintf = val_ref_sprintf;
  h->fprintf = val_ref_fprintf;
}

struct val_ref_struct* _val_ref_ref(val_t *val) {
  return val->val.ref;
}
val_t* _val_ref_val(val_t *val) {
  return &val->val.ref->val;
}

err_t val_ref_wrap(val_t *val) {
  val_t t;
  t.type = TYPE_REF;
  fatal_if(ERR_FATAL,NULL==(t.val.ref = malloc(sizeof(struct val_ref_struct))));
  t.val.ref->nwait=0;
  fatal_if(ERR_LOCK,0 != sem_init(&t.val.ref->lock,0,1));
  fatal_if(ERR_LOCK,0 != sem_init(&t.val.ref->wait,0,0));
  t.val.ref->refcount=1;
  val_move(&t.val.ref->val,val);
  val_move(val,&t);
  return 0;
}
err_t val_ref_unwrap(val_t *ref) {
  err_t r;
  if ((r = _val_ref_lock(ref))) return r;
  val_t t;
  if (ref->val.ref->refcount == 1) { //we are the last reference
    val_move(&t,&ref->val.ref->val);
  } else {
    val_clone(&t,&ref->val.ref->val);
  }
  r = _val_ref_unlock(ref);
  val_destroy(ref);
  val_move(ref,&t);
  return r;
}

err_t val_ref_swap(val_t *ref, val_t *val) {
  err_t r;
  if ((r = _val_ref_lock(ref))) return r;
  val_swap(&ref->val.ref->val,val);
  if ((r = _val_ref_unlock(ref))) return r;
  return r;
}

err_t _ref_lock(struct val_ref_struct *ref) {
  err_t r;
  throw_if(ERR_LOCK,(r = sem_wait(&ref->lock)));
  return 0;
}
err_t _ref_trylock(struct val_ref_struct *ref) {
  err_t r;
  if (!(r = sem_trywait(&ref->lock))) return 0;
  else if (errno == EAGAIN) return ERR_LOCKED;
  else return _throw(ERR_LOCK);
}
err_t _ref_unlock(struct val_ref_struct *ref) {
  err_t r;
  throw_if(ERR_LOCK,(r = sem_post(&ref->lock)));
  return 0;
}

err_t _ref_signal(struct val_ref_struct *ref) {
  if (ref->nwait) { //post once if there are waiter(s)
    --ref->nwait;
    throw_if(ERR_LOCK,sem_post(&ref->wait));
  }
  return 0;
}
err_t _ref_broadcast(struct val_ref_struct *ref) {
  while (ref->nwait) { //post once for each waiter
    --ref->nwait;
    throw_if(ERR_LOCK,sem_post(&ref->wait));
  }
  return 0;
}
err_t _ref_wait(struct val_ref_struct *ref) {
  err_t r;
  ++ref->nwait;
  if ((r = _ref_unlock(ref))) return r;

  throw_if(ERR_LOCK,sem_wait(&ref->wait));
  return 0;
}

err_t val_ref_signal(val_t *val) {
  err_t r;
  struct val_ref_struct *ref = val->val.ref;
  if ((r = _ref_lock(ref))) return r;
  if ((r = _ref_signal(ref))) {
    _ref_unlock(ref);
    return r;
  } 
  return _ref_unlock(ref);
}
err_t val_ref_broadcast(val_t *val) {
  err_t r;
  struct val_ref_struct *ref = val->val.ref;
  if ((r = _ref_lock(ref))) return r;
  if ((r = _ref_broadcast(ref))) {
    _ref_unlock(ref);
    return r;
  } 
  return _ref_unlock(ref);
}
err_t val_ref_wait(val_t *val) {
  err_t r;
  struct val_ref_struct *ref = val->val.ref;
  if ((r = _ref_lock(ref))) return r;
  return _ref_wait(ref);
}

void _val_ref_swap(val_t *ref, val_t *val) {
  val_swap(&ref->val.ref->val,val);
}
err_t _val_ref_lock(val_t *val) {
  return _ref_lock(val->val.ref);
}
err_t _val_ref_trylock(val_t *val) {
  return _ref_trylock(val->val.ref);
}
err_t _val_ref_unlock(val_t *val) {
  return _ref_unlock(val->val.ref);
}
err_t _val_ref_signal(val_t *val) {
  return _ref_signal(val->val.ref);
}
err_t _val_ref_broadcast(val_t *val) {
  return _ref_broadcast(val->val.ref);
}
err_t _val_ref_wait(val_t *val) {
  debug_assert(ERR_LOCKED == _val_ref_trylock(val)); //we should already have lock before waiting
  return _ref_wait(val->val.ref);
}


err_t val_ref_clone(val_t *ret, val_t *orig) {
  *ret = *orig;
  refcount_inc(ret->val.ref->refcount);
  return 0;
}
err_t val_ref_destroy(val_t *val) {
  if (0 == refcount_dec(val->val.ref->refcount)) {
    //last reference, free resources
    err_t r;
    fatal_if(r,(r = _val_ref_trylock(val)));
    sem_destroy(&val->val.ref->lock);
    sem_destroy(&val->val.ref->wait);
    val_destroy(&val->val.ref->val);
    free(val->val.ref);
  }
  val->type = VAL_INVALID;
  return 0;
}

int val_ref_fprintf(val_t *val,FILE *file, const fmt_t *fmt) {
  err_t r;
  if (ERR_LOCKED == (r = _val_ref_trylock(val))) {
    return fprintf(file,"ref(REF LOCKED)");
  } else if (r) return r;
  int rlen = 0;
  if (fmt->conversion == 'V') {
    throw_if(ERR_IO_ERROR,0>(r = fprintf(file,"ref(")));
    rlen += r;
  }
  if (0>(r = val_fprintf_(&val->val.ref->val,file,fmt))) goto out_bad;
  rlen += r;
  if ((r = _val_ref_unlock(val))) return r;
  if (fmt->conversion == 'V') {
    throw_if(ERR_IO_ERROR,0>(r = fprintf(file,")")));
    rlen += r;
  }
  return rlen;
out_bad:
  _val_ref_unlock(val);
  return r;
}
int val_ref_sprintf(val_t *val,val_t *buf, const fmt_t *fmt) {
  err_t r;
  if (ERR_LOCKED == (r = _val_ref_trylock(val))) {
    return val_sprint_cstr(buf,"ref(REF LOCKED)");
  } else if (r) return r;
  int rlen = 0;
  if (fmt->conversion == 'V') {
    if (0>(r = val_sprint_cstr(buf,"ref("))) return r; rlen += r;
  }
  if (0>(r = val_sprintf_(&val->val.ref->val,buf,fmt))) goto out_bad; rlen += r;
  if ((r = _val_ref_unlock(val))) return r;
  if (fmt->conversion == 'V') {
    if (0>(r = val_sprint_cstr(buf,")"))) return r; rlen += r;
  }
  return rlen;
out_bad:
  _val_ref_unlock(val);
  return r;
}

