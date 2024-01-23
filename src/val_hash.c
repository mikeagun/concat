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

#include "val_hash.h"
#include "val_hash_internal.h"
#include "val_string.h"
#include "defpool.h"
#include "helpers.h"

#include <stdlib.h>
#include <string.h>
#include <valgrind/helgrind.h>

inline unsigned int hash_bucket(const uint32_t khash, const unsigned int nbuckets) { return khash & (nbuckets-1); } //currently assumes nbuckets is a power of 2
//inline unsigned int hash_bucket(const uint32_t khash, const unsigned int nbuckets) { return khash % nbuckets; } //doesn't assume nbuckets is a power of 2

struct hashtable* alloc_hashtable() {
  unsigned int nbuckets = DEFAULT_HASH_BUCKETS;
  struct hashtable *h;
  if (!(h = malloc(sizeof(struct hashtable) + (sizeof(struct hashentry*) * nbuckets)))) return NULL;
  memset(h->buckets,0,sizeof(struct hashentry*) * nbuckets); //zero bucket pointers
  h->nbuckets = nbuckets;
  h->refcount=1;
  h->size = 0;
  return h;
}
void release_hashtable(struct hashtable *h) {
  if (0 == (refcount_dec(h->refcount))) {
    ANNOTATE_HAPPENS_AFTER(h);
    ANNOTATE_HAPPENS_BEFORE_FORGET_ALL(h);
    unsigned int i;
    for(i=0;i<h->nbuckets;++i) {
      _hashchain_dispose(h->buckets[i]);
    }
    free(h);
  } else {
    ANNOTATE_HAPPENS_BEFORE(h);
  }
}

err_t hash_clone(struct hashtable **ret, struct hashtable *orig) {
  *ret = alloc_hashtable();
  unsigned int i;
  struct hashentry *e;
  for(i=0;i<orig->nbuckets;++i) {
    if ((e = orig->buckets[i])) {
      struct hashentry *re;
      if (!(re = alloc_hashentry_clone(&e->k,e->v,e->khash,NULL))) goto err_malloc;
      (*ret)->buckets[i] = re;
      for(e=e->next; e; e=e->next) {
        if (!(re->next = alloc_hashentry_clone(&e->k,e->v,e->khash,NULL))) goto err_malloc;
        re = re->next;
      }
    }
  }
  return 0;
err_malloc:
  return _throw(ERR_MALLOC);
}



//we use a pool for hashentries, since we tend to need a lot of these when we need a few, and good-practice is that you aren't freeing many of these anyways
//TODO: needs a threadsafe / thread-local pool allocator
//DEFINE_SIMPLE_POOL(struct hashentry,4096,_hashentry_alloc,_hashentry_free)
DEFINE_NO_POOL(struct hashentry,4096,_hashentry_alloc,_hashentry_free)

void _hashentry_release(struct hashentry *e) {
  _val_str_destroy_(&e->k);
  val_destroy(e->v);
  _hashentry_free(e);
}
void _hashchain_dispose(struct hashentry *e) {
  while(e) { //loop through entries, freeing each one
    struct hashentry *n=e->next;
    _hashentry_release(e);
    e=n;
  }
}

struct hashentry* alloc_hashentry(valstruct_t *key, val_t value, uint32_t khash, struct hashentry *next) {
  struct hashentry *e;
  if (!(e = _hashentry_alloc())) return NULL;
  e->next  = next;
  e->khash = khash;
  e->k     = *key;
  _valstruct_release(key);
  e->v     = value;
  return e;
}

struct hashentry* alloc_hashentry_clone(valstruct_t *key, val_t value, uint32_t khash, struct hashentry *next) {
  struct hashentry *e;
  if (!(e = _hashentry_alloc())) return NULL;
  val_t vcln;
  int r;
  _val_str_clone(&e->k,key);
  if ((r = val_clone(&vcln,value))) goto out_kentry;
  e->next  = next;
  e->khash = khash;
  e->v     = vcln;
  return e;
out_kentry:
  _val_str_destroy_(&e->k);
  _hashentry_free(e);
  return NULL;
}


val_t hash_get(struct hashtable *h, valstruct_t *key) {
  uint32_t khash=_val_str_hash32(key);
  struct hashentry *e;
  for (e = h->buckets[hash_bucket(khash,h->nbuckets)]; e; e=e->next) { //loop through bucket list
    if (e->khash==khash && _val_str_eq(key,&e->k)) { //found it
      return e->v;
    } else if (khash<e->khash || (e->khash==khash && _val_str_compare(key,&e->k)<0)) { //past it. search failed
      break;
    }
  }
  return VAL_NULL;
}
val_t* _hash_get(struct hashtable *h, valstruct_t *key) {
  uint32_t khash=_val_str_hash32(key);
  struct hashentry *e;
  for (e = h->buckets[hash_bucket(khash,h->nbuckets)]; e; e=e->next) { //loop through bucket list
    if (e->khash==khash && _val_str_eq(key,&e->k)) { //found it
      return &e->v;
    } else if (khash<e->khash || (e->khash==khash && _val_str_compare(key,&e->k)<0)) { //past it. search failed
      break;
    }
  }
  return NULL;
}

val_t hash_get_(struct hashtable *h, const char *key, unsigned int len) {
  uint32_t khash=_val_cstr_hash32(key,len);
  struct hashentry *e;
  for (e = h->buckets[hash_bucket(khash,h->nbuckets)]; e; e=e->next) { //loop through bucket list
    if (e->khash==khash && !_val_str_cstr_compare(&e->k,key,len)) { //found it
      return e->v;
    } else if (khash<e->khash || (e->khash==khash && _val_str_cstr_compare(&e->k,key,len)>0)) { //past it. search failed
      break;
    }
  }
  return VAL_NULL;
}

int hash_put(struct hashtable *h, valstruct_t *key, val_t value, int overwrite) {
  uint32_t khash=_val_str_hash32(key);
  struct hashentry *e;
  e = h->buckets[hash_bucket(khash,h->nbuckets)];

  if (!e || khash<e->khash || (khash==e->khash && _val_str_compare(key,&e->k) < 0)) { //no bucket list in this table, or our key goes first
    struct hashentry *t;
    if (!(t = alloc_hashentry(key,value,khash,e))) return -1;
    h->buckets[hash_bucket(khash,h->nbuckets)] = t;
    h->size++;
    return 1;
  } else if (e->khash==khash && _val_str_eq(key,&e->k)) { //found key at head of list
    if (!overwrite) return 0;
    _val_str_destroy(key);
    val_destroy(e->v);
    e->v = value;
    return 2;
  } else { //else, search for key in list
    for(; e; e=e->next) { //scan bucket entries
      if (e->next && e->next->khash==khash && _val_str_eq(key,&e->next->k)) { //found it
        if (!overwrite) return 0;
        _val_str_destroy(key);
        val_destroy(e->next->v);
        e->next->v = value;
        return 2;
      } else if (!e->next || khash<e->next->khash || (e->next->khash==khash && _val_str_compare(key,&e->next->k)<0)) { //past it (or this is the tail of the list). insert here
        //now insert our k,v pair
        struct hashentry *t;
        if (!(t = alloc_hashentry(key,value,khash,e->next))) return -1;
        e->next = t;
        h->size++;
        return 1;
      }
    }
    return -1;
  }
}

int hash_put_copy(struct hashtable *h, valstruct_t *key, val_t value, int overwrite) {
  uint32_t khash=_val_str_hash32(key);
  struct hashentry *e;
  int r;
  e = h->buckets[hash_bucket(khash,h->nbuckets)];

  if (!e || khash<e->khash || (khash==e->khash && _val_str_compare(key,&e->k) < 0)) { //no bucket list in this table, or our key goes first
    struct hashentry *t;
    if (!(t = alloc_hashentry_clone(key,value,khash,e))) return -1;
    h->buckets[hash_bucket(khash,h->nbuckets)] = t;
    h->size++;
    return 1;
  } else if (e->khash==khash && _val_str_eq(key,&e->k)) { //found key at head of list
    if (!overwrite) return 0;
    val_destroy(e->v);
    if ((r = val_clone(&e->v,value))) return r;
    return 2;
  } else { //else, search for key in list
    for(; e; e=e->next) { //scan bucket entries
      if (e->next && e->next->khash==khash && _val_str_eq(key,&e->next->k)) { //found it
        if (!overwrite) return 0;
        val_destroy(e->next->v);
        if ((r = val_clone(&e->next->v,value))) return r;
        return 2;
      } else if (!e->next || khash<e->next->khash || (e->next->khash==khash && _val_str_compare(key,&e->next->k)<0)) { //past it (or this is the tail of the list). insert here
        //now insert our k,v pair
        struct hashentry *t;
        if (!(t = alloc_hashentry_clone(key,value,khash,e->next))) return -1;
        e->next = t;
        h->size++;
        return 1;
      }
    }
    return -1;
  }
}

int hash_delete(struct hashtable *h, valstruct_t *key) {
  if (!h) return -1;
  uint32_t khash=_val_str_hash32(key);
  struct hashentry *e;

  e = h->buckets[hash_bucket(khash,h->nbuckets)];
  if (!e) { //no bucket list in this table
    return 0;
  } else if (e->khash==khash && _val_str_eq(key,&e->k)) { //found key at head of list
    h->buckets[hash_bucket(khash,h->nbuckets)] = e->next;
    h->size--;
    _hashentry_release(e);
    return 1;
  } else { //else, search for key in list
    for(; e->next; e=e->next) { //scan bucket entries
      if (e->next->khash==khash && _val_str_eq(key,&e->next->k)) { //found it
        struct hashentry *t=e->next;
        e->next=t->next;
        h->size--;
        _hashentry_release(t);
        return 1;
      } else if (khash<e->next->khash || (e->next->khash==khash && _val_str_compare(key,&e->next->k)<0)) { //past it. move on to parent scope
        break;
      }
    }
  }

  return 0;
}

int hash_visit(struct hashtable *h, hash_visitor *visit, void *arg) {
  unsigned int i;
  struct hashentry *e;
  int r=0;
  for(i=0;i<h->nbuckets;++i) {
    for(e=h->buckets[i]; e; e=e->next) {
      if ((r=visit(e,arg)) < 0) return r;
    }
  }
  return r;
}
