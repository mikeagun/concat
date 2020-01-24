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

#include "val_hash.h"
#include "val_hash_internal.h"
#include "val_stack.h"
#include "val_printf.h"
#include "vm_err.h"

int val_isdict(val_t *val) {
  if (!val_islist(val)) return 0;
  else if (val_list_empty(val)) return 0;

  unsigned int i;
  for(i = 0; i < val_stack_len(val); ++i) {
    if (!val_ishash(_val_stack_get(val,i))) return 0;
  }
  return 1;
}

err_t val_hash_init(val_t *val, struct hashtable *next) {
  struct hashtable *hash;
  throw_if(ERR_MALLOC,!(hash = alloc_hashtable(DEFAULT_HASH_BUCKETS,next)));
  cleanup_throw_if(ERR_MALLOC,dispose_hashtable(hash),!(val->val.hash = malloc(sizeof(struct val_hash_struct))));
  val->type = TYPE_HASH;
  val->val.hash->refcount=1;
  val->val.hash->depth=1;
  val->val.hash->hash=hash;
  return 0;
}

err_t val_hash_init_(val_t *val, struct hashtable *hash) {
  throw_if(ERR_MALLOC,!(val->val.hash = malloc(sizeof(struct val_hash_struct))));
  val->type = TYPE_HASH;
  val->val.hash->refcount=1;
  val->val.hash->depth=1;
  val->val.hash->hash = hash;
  return 0;
}

err_t val_dict_init(val_t *dict) {
  val_stack_init(dict);
  return val_dict_pushnew(dict);
}
err_t val_dict_destroy(val_t *dict) {
  argcheck_r(val_isdict(dict));
  while(!val_stack_empty(dict)) val_dict_pop(dict,NULL);
  return val_destroy(dict);
}
err_t val_dict_pushnew(val_t *dict) {
  err_t r;
  val_t t;
  if ((r = val_hash_init(&t,NULL))) return r;
  return val_dict_push(dict,&t);
}
err_t val_dict_push(val_t *dict, val_t *hash) {
  argcheck_r(val_islist(dict) && val_ishash(hash));
  err_t r;
  if (val_stack_empty(dict)) { r = 0; val_hash_clearnext(hash); }
  else r = val_hash_setnext(hash,val_stack_top(dict));
  if (r) return r;

  if ((r = val_stack_push(dict,hash))) return r;
  return 0;
}
err_t val_dict_pop(val_t *dict, val_t *hash) {
  argcheck_r(val_isdict(dict));
  val_t top;
  err_t r;
  if ((r = val_stack_pop(dict,&top))) return r;
  val_hash_clearnext(&top);
  if (hash) val_move(hash,&top);
  else val_destroy(&top);
  return 0;
}

err_t val_dict_pop_to1(val_t *dict) { //pop all but bottom scope
  argcheck_r(val_isdict(dict));
  err_t r;
  while(val_stack_len(dict) > 1) {
    if ((r = val_dict_pop(dict,NULL))) return r;
  }
  return 0;
}

struct hashtable* val_dict_h(val_t *dict) {
  argcheck(val_islist(dict) && (val_stack_empty(dict) || val_ishash(val_stack_top(dict))));
  if (val_stack_empty(dict)) return NULL;
  else return val_stack_top(dict)->val.hash->hash;
}

err_t val_hash_setnext(val_t *hash, val_t *next) {
  argcheck_r(val_ishash(hash) && val_ishash(next));
  int depth=hash->val.hash->depth;
  struct hashtable* tail=hash->val.hash->hash;
  while(--depth) tail=tail->next;
  fatal_if(ERR_FATAL,tail->next); //can only set when null
  tail->next = next->val.hash->hash;
  return 0;
}
void val_hash_clearnext(val_t *hash) {
  argcheck(val_ishash(hash));
  int depth=hash->val.hash->depth;
  struct hashtable* tail=hash->val.hash->hash;
  while(--depth) tail=tail->next;
  tail->next = NULL;
}


void val_hash_init_handlers(struct type_handlers *h) {
  h->destroy = val_hash_destroy;
  h->clone = val_hash_clone;
  h->fprintf = val_hash_fprintf;
  h->sprintf = val_hash_sprintf;
}
err_t val_hash_clone(val_t *ret, val_t *orig) {
  *ret = *orig;
  refcount_inc(ret->val.hash->refcount);
  return 0;
}
err_t val_hash_destroy(val_t *val) {
  if (0 == refcount_dec(val->val.hash->refcount)) {
    while(val->val.hash->depth--) val->val.hash->hash = dispose_hashtable(val->val.hash->hash);
    free(val->val.hash);
  }
  val->type = VAL_INVALID;
  return 0;
}
int val_hash_fprintf(val_t *val, FILE *file, const fmt_t *fmt) {
  return val_fprint_cstr(file,"{hash}");
}
int val_hash_sprintf(val_t *val, val_t *buf, const fmt_t *fmt) {
  return val_sprint_cstr(buf,"{hash}");
}
