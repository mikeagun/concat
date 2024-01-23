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

#include "val_dict.h"
#include "val_hash.h"
#include "val_string.h"
#include "val_printf.h"
#include "val.h"
#include "helpers.h"
#include "vm_debug.h"

#include <stdlib.h>

//TODO: validate the dictionary refcount managment and scoping semantics
//TODO: make dict threadsafe (or ensure we deep copy when we could break threadsafety)
//TODO: evaluate dict code structure
//  - we currently preserve valstruct_t pointers to fit with other type implementations
//  - the means a lot more data movement required on push/pop
//  - faster implementation might be swappign valstruct ptr on push/pop -- but then need more unique cases in other code
void _val_dict_destroy_(valstruct_t *dict) {
  //VM_DEBUG_VAL_DESTROY(dict);
  if (dict->v.dict.next) {
    _val_dict_destroy_(dict->v.dict.next);
    _valstruct_release(dict->v.dict.next);
  }
  release_hashtable(dict->v.dict.h);
}
err_t _val_dict_init(valstruct_t *dict) {
  if (!(dict->v.dict.h = alloc_hashtable())) return _throw(ERR_MALLOC);
  dict->type = TYPE_DICT;
  dict->v.dict.next = NULL;
  return 0;
}
err_t _val_dict_clone(val_t *ret, valstruct_t *orig) {
  //*ret = __dict_val(orig);
  //refcount_inc(orig->v.dict.h->refcount);
  valstruct_t *v;
  if (!(v = _valstruct_alloc())) return _throw(ERR_MALLOC);
  *v = *orig;
  refcount_inc(orig->v.dict.h->refcount);
  if (orig->v.dict.next) refcount_inc(orig->v.dict.next->v.dict.h->refcount);
  *ret = __dict_val(v);
  return 0;
}
void _val_dict_clone_(valstruct_t *ret, valstruct_t *orig) { //FIXME: bug here -- also contains ref to next, will end up double-releasing next
  *ret = *orig;
  refcount_inc(orig->v.dict.h->refcount);
  if (orig->v.dict.next) refcount_inc(orig->v.dict.next->v.dict.h->refcount);
}

err_t _val_dict_newscope(valstruct_t *dict) {
  //dict becomes new scope, old dict pushed into next
  valstruct_t *parent;
  struct hashtable *h;
  if (!(parent = _valstruct_alloc())) goto out_malloc;
  *parent = *dict;
  if (!(h = alloc_hashtable())) goto out_parent;
  dict->v.dict.next = parent;
  dict->v.dict.h = h;
  return 0;
out_parent:
  _valstruct_release(parent);
out_malloc:
  return _throw(ERR_MALLOC);
}

int _val_dict_canpop(valstruct_t *dict) {
  return dict->v.dict.next!=NULL;
}

void _val_dict_popscope(valstruct_t *dict, val_t *scope) {
  //dict becomes old next, old dict returned as scope in old next valstruct
  valstruct_t *n = dict->v.dict.next;
  struct hashtable *h = dict->v.dict.h;
  dict->v.dict = n->v.dict;

  n->v.dict.h = h;
  n->v.dict.next=NULL;

  *scope = __dict_val(n);
}

void _val_dict_dropscope(valstruct_t *dict) {
  //dict becomes old next, free old dict
  valstruct_t *n = dict->v.dict.next;
  release_hashtable(dict->v.dict.h);
  dict->v.dict = n->v.dict;
  _valstruct_release(n);
}

void _val_dict_pushscope(valstruct_t *dict, valstruct_t *scope) {
  struct hashtable *h = scope->v.dict.h;
  scope->v.dict.h = dict->v.dict.h;
  dict->v.dict.h = h;
  dict->v.dict.next = scope;
}

val_t _val_dict_get(valstruct_t *dict, valstruct_t *key) {
  for(;dict; dict=dict->v.dict.next) {
    val_t ret = hash_get(dict->v.dict.h,key);
    if (!val_is_null(ret)) return ret;
  }
  return VAL_NULL;
}

err_t _val_dict_deref(valstruct_t *dict) {
  struct hashtable *h;
  err_t e;
  if ((e = hash_clone(&h,dict->v.dict.h))) return e;
  release_hashtable(dict->v.dict.h);
  dict->v.dict.h = h;
  return 0;
}
int _val_dict_put(valstruct_t *dict, valstruct_t *key, val_t val) {
  if (dict->v.dict.h->refcount>1) {
    err_t e;
    if ((e = _val_dict_deref(dict))) return e;
  }
  return hash_put(dict->v.dict.h,key,val,1);
}

int _val_dict_put_(valstruct_t *dict, const char *key, unsigned int klen, val_t val) {
  if (dict->v.dict.h->refcount>1) {
    err_t e;
    if ((e = _val_dict_deref(dict))) return e;
  }
  int r;
  val_t keystr;
  if ((r = val_ident_init_cstr(&keystr,key,klen))) return r;
  r = hash_put(dict->v.dict.h,__str_ptr(keystr),val,1);
  if (r <= 0) { val_destroy(keystr); }
  return r;
}

err_t _val_dict_swap(valstruct_t *dict, valstruct_t *key, val_t *val) {
  val_t *def;
  if (dict->v.dict.h->refcount==1 && NULL!=(def = _hash_get(dict->v.dict.h,key))) {
    val_swap(def,val);
    return 0;
  } else {
    for(dict=dict->v.dict.next;dict; dict=dict->v.dict.next) {
      if (NULL != (def = _hash_get(dict->v.dict.h,key))) {
        //FIXME: write current val to dict before filling with cloned val
        return val_clone(val,*def);
      }
    }
    return _throw(ERR_UNDEFINED);
  }
}

int val_dict_fprintf(valstruct_t *v,FILE *file, const struct printf_fmt *fmt) {
  return val_fprint_cstr(file,"{DICT}"); //TODO: IMPLEMENTME
}

int val_dict_sprintf(valstruct_t *v,valstruct_t *buf, const struct printf_fmt *fmt) {
  return val_sprint_cstr(buf,"{DICT}"); //TODO: IMPLEMENTME
}

