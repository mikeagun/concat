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

//hash.h -- chained hashtable implementation
//Implements a chained-hashtable which supports get,put,delete,walk, and keeps next pointers to implement scoped hashtables
//
//NOTES:
//  - uses string keys with FNV hash function
//  - TODO: make sure no key leaks
#include "helpers.h"
#include "valhash.h"
#include "val_string.h"


#include<stdlib.h>
#include<string.h>


//FNV (Fowler-Noll-Vo) hash function
//Constants:
//  32bit - 2166136261,16777619
//  64bit - 14695981039346656037,1099511628211
uint32_t hash_string(const char* s)
{
  register uint32_t h;
  for (h = 2166136261; *s; ++s) {
    h = (h*16777619) ^ *s;
  }
  return h;
}

uint32_t hash_string_(const char* s, unsigned int len)
{
  register uint32_t h;
  for (h = 2166136261; len--; ++s) {
    h = (h*16777619) ^ *s;
  }
  return h;
}


unsigned int hash_bucket(const unsigned int khash, const unsigned int nbuckets) { return khash & (nbuckets-1); }
//inline unsigned int hash_bucket(const unsigned int khash, const unsigned int nbuckets) { return khash % nbuckets; }


struct hashentry* alloc_hashentry_kcopy(const char *key, val_t *value, unsigned int khash, struct hashentry *next) {
  char *k=_strdup(key);
  if (!k) return NULL;
  return alloc_hashentry(k,value,khash,next,1);
}

struct hashentry* alloc_hashentry_kval(val_t *key, val_t *value, unsigned int khash, struct hashentry *next) {
  char *k=_strdup(val_string_cstr(key)); //TODO: ???
  if (!k) return NULL;
  return alloc_hashentry(k,value,khash,next,1);
}

struct hashentry* alloc_hashentry(char *key, val_t *value, unsigned int khash, struct hashentry *next, int free_key) {
  struct hashentry *e = malloc(sizeof(struct hashentry));
  if (!e) return NULL;
  e->next  = next;
  e->khash = khash;
  e->k     = key;
  val_move(&e->v,value);
  e->free_key = free_key;
  return e;
}

void free_hashentry(struct hashentry *e) {
  if (e) {
    if (e->free_key) free(e->k);
    val_destroy(&e->v);
    free(e);
  }
}

void dispose_hashentry(struct hashentry *e) {
  while(e) { //loop through entries, freeing each one
    struct hashentry *n=e->next;
    free_hashentry(e);
    e=n;
  }
}

struct hashtable* alloc_hashtable(unsigned int nbuckets, struct hashtable *next) {
  struct hashtable *h = malloc(sizeof(struct hashtable));
  if (!h) return NULL;
  h->buckets = malloc(sizeof(struct hashentry*) * nbuckets);
  if (!h->buckets) { free(h); return NULL; }
  memset(h->buckets,'\0',sizeof(struct hashentry*) * nbuckets); //zero bucket pointers
  h->nbuckets = nbuckets;
  h->next = next;
  h->nelements = 0;
  return h;
}
struct hashtable* free_hashtable(struct hashtable *h) {
  if (!h) return NULL;
  struct hashtable *next=h->next;
  free(h->buckets);
  free(h);
  return next;
}

struct hashtable* dispose_hashtable(struct hashtable *h) {
  if (!h) return NULL;
  unsigned int i;
  for(i=0;i<h->nbuckets;++i) {
    dispose_hashentry(h->buckets[i]);
  }
  free(h->buckets);
  struct hashtable *next=h->next;
  free(h);
  return next;
}

struct hashtable* dispose_merge_hashtable(struct hashtable *h) {
  if (!h) return NULL;
  unsigned int i;
  for(i=0;i<h->nbuckets;++i) {
    struct hashentry *e = h->buckets[i];
    while(e) { //loop through entries, copying each one to h->next and then freeing
      e=_hash_putfree(h->next,e,1);
    }
  }
  free(h->buckets);
  struct hashtable *next=h->next;
  free(h);
  return next;
}


int hash_put_kcast(struct hashtable *h, const char *key, val_t *value, int overwrite) {
  return hash_put(h,(char *)key,value, overwrite);
}

int hash_put_kcopy(struct hashtable *h, const char *key, val_t *value, int overwrite) {
  if (!h) return -1;

  unsigned int khash=hash_string(key);
  struct hashentry *e;
  e = h->buckets[hash_bucket(khash,h->nbuckets)];

  if (!e || khash<e->khash || (khash==e->khash && strcmp(key,e->k) < 0)) { //no bucket list in this table, or our key goes first
    struct hashentry *t = alloc_hashentry_kcopy(key,value,khash,e);
    if (!t) return -1;
    h->buckets[hash_bucket(khash,h->nbuckets)] = t;
    h->nelements++;
    return 1;
  } else if (e->khash==khash && !strcmp(key,e->k)) { //found key at head of list
    if (!overwrite) return 0;
    val_destroy(&e->v);
    val_move(&e->v,value);
    return 2;
  } else { //else, search for key in list
    for(; e; e=e->next) { //scan bucket entries
      if (e->next && e->next->khash==khash && !strcmp(key,e->next->k)) { //found it
        if (!overwrite) return -1;
        val_destroy(&e->next->v);
        val_move(&e->next->v,value);
        return 2;
      } else if (!e->next || khash<e->next->khash || (e->next->khash==khash && strcmp(key,e->next->k)<0)) { //past it (or this is the tail of the list). insert here
        //now insert our k,v pair
        struct hashentry *t = alloc_hashentry_kcopy(key,value,khash,e->next);
        if (!t) return -1;
        e->next = t;
        h->nelements++;
        return 1;
      }
    }
    return -1;
  }
}

int hash_putv(struct hashtable *h, val_t *key, val_t *value, int overwrite) {
  if (!h) return -1;

  unsigned int khash=val_string_hash32(key);
  struct hashentry *e;
  e = h->buckets[hash_bucket(khash,h->nbuckets)];

  if (!e || khash<e->khash || (khash==e->khash && val_string_strcmp(key,e->k) < 0)) { //no bucket list in this table, or our key goes first
    struct hashentry *t = alloc_hashentry_kval(key,value,khash,e);
    if (!t) return -1;
    h->buckets[hash_bucket(khash,h->nbuckets)] = t;
    h->nelements++;
    return 1;
  } else if (e->khash==khash && !val_string_strcmp(key,e->k)) { //found key at head of list
    if (!overwrite) return 0;
    val_destroy(&e->v);
    val_move(&e->v,value);
    return 2;
  } else { //else, search for key in list
    for(; e; e=e->next) { //scan bucket entries
      if (e->next && e->next->khash==khash && !val_string_strcmp(key,e->next->k)) { //found it
        if (!overwrite) return -1;
        val_destroy(&e->next->v);
        val_move(&e->next->v,value);
        return 2;
      } else if (!e->next || khash<e->next->khash || (e->next->khash==khash && val_string_strcmp(key,e->next->k)<0)) { //past it (or this is the tail of the list). insert here
        //now insert our k,v pair
        struct hashentry *t = alloc_hashentry_kval(key,value,khash,e->next);
        if (!t) return -1;
        e->next = t;
        h->nelements++;
        return 1;
      }
    }
    return -1;
  }
}

int hash_put(struct hashtable *h,char *key, val_t *value, int overwrite) {
  if (!h) return -1;

  unsigned int khash=hash_string(key);
  struct hashentry *e;
  e = h->buckets[hash_bucket(khash,h->nbuckets)];

  if (!e || khash<e->khash || (khash==e->khash && strcmp(key,e->k) < 0)) { //no bucket list in this table, or our key goes first
    struct hashentry *t = alloc_hashentry(key,value,khash,e, 0);
    if (!t) return -1;
    h->buckets[hash_bucket(khash,h->nbuckets)] = t;
    h->nelements++;
    return 1;
  } else if (e->khash==khash && !strcmp(key,e->k)) { //found key at head of list
    if (!overwrite) return -1;
    val_destroy(&e->v);
    val_move(&e->v,value);
    return 2;
  } else { //else, search for key in list
    for(; e; e=e->next) { //scan bucket entries
      if (e->next && e->next->khash==khash && !strcmp(key,e->next->k)) { //found it
        if (!overwrite) return -1;
        val_destroy(&e->next->v);
        val_move(&e->next->v,value);
        return 2;
      } else if (!e->next || khash<e->next->khash || (e->next->khash==khash && strcmp(key,e->next->k)<0)) { //past it (or this is the tail of the list). insert here
        //now insert our k,v pair
        struct hashentry *t = alloc_hashentry(key,value,khash,e->next, 0);
        if (!t) return -1;
        e->next = t;
        h->nelements++;
        return 1;
      }
    }
    return -1;
  }
}

//used to put an existing hashentry into h and then destroy the hashentry and return entry->next (for merging hashtables)
struct hashentry* _hash_putfree(struct hashtable *h, struct hashentry *entry, int overwrite) {
  struct hashentry *enext=entry->next;
  entry->next = NULL;
  if (!h) {
    free_hashentry(entry);
    return enext;
  }

  unsigned int khash=entry->khash;
  struct hashentry *e;
  e = h->buckets[hash_bucket(khash,h->nbuckets)];

  if (!e || khash<e->khash || (khash==e->khash && strcmp(entry->k,e->k) < 0)) { //no bucket list in this table, or our key goes first
    entry->next = e;
    h->buckets[hash_bucket(khash,h->nbuckets)] = entry;
    h->nelements++;
  } else if (e->khash==khash && !strcmp(entry->k,e->k)) { //found key at head of list
    if (overwrite) {
      val_destroy(&e->v);
      val_move(&e->v,&entry->v);
    }
    free_hashentry(entry);
  } else { //else, search for key in list
    for(; e; e=e->next) { //scan bucket entries
      if (e->next && e->next->khash==khash && !strcmp(entry->k,e->next->k)) { //found it
        if (overwrite) {
          val_destroy(&e->next->v);
          val_move(&e->next->v,&entry->v);
        }
        free_hashentry(entry);
        return enext;
      } else if (!e->next || khash<e->next->khash || (e->next->khash==khash && strcmp(entry->k,e->next->k)<0)) { //past it (or this is the tail of the list). insert here
        //now insert our k,v pair
        e->next = entry;
        h->nelements++;
        return enext;
      }
    }
  }
  return enext;
}

val_t* hash_get(struct hashtable *h, const char *key) {
  unsigned int khash=hash_string(key);
  struct hashentry *e;
  for(;h;h=h->next) { //loop through parent scopes
    for (e = h->buckets[hash_bucket(khash,h->nbuckets)]; e; e=e->next) { //loop through bucket list
      if (e->khash==khash && !strcmp(key,e->k)) { //found it
        return &e->v;
      } else if (khash<e->khash || (e->khash==khash && strcmp(key,e->k)<0)) { //past it. move on to parent scope
        break;
      }
    }
  }
  return NULL;
}

val_t* hash_get_(struct hashtable *h, const char *key, unsigned int len) {
  unsigned int khash=hash_string_(key,len);
  struct hashentry *e;
  for(;h;h=h->next) { //loop through parent scopes
    for (e = h->buckets[hash_bucket(khash,h->nbuckets)]; e; e=e->next) { //loop through bucket list
      if (e->khash==khash && !strncmp_cstr(key,len,e->k)) { //found it
        return &e->v;
      } else if (khash<e->khash) { //past it. move on to parent scope
        const char *cstr = key;
        unsigned int n = len;
        const char *s = e->k;
        for(; *cstr && n; --n, ++s, ++cstr) {
          if (*s < *cstr) break;
          else if (*cstr < *s) continue;
        }
        if (*cstr) break;
        else continue;
      }
    }
  }
  return NULL;
}

val_t* hash_getv(struct hashtable *h, val_t *key) {
  unsigned int khash=val_string_hash32(key);
  struct hashentry *e;
  for(;h;h=h->next) { //loop through parent scopes
    for (e = h->buckets[hash_bucket(khash,h->nbuckets)]; e; e=e->next) { //loop through bucket list
      if (e->khash==khash && !val_string_strcmp(key,e->k)) { //found it
        return &e->v;
      } else if (khash<e->khash || (e->khash==khash && val_string_strcmp(key,e->k)<0)) { //past it. move on to parent scope
        break;
      }
    }
  }
  return NULL;
}


int hash_delete(struct hashtable *h, const char *key) {
  if (!h) return -1;
  unsigned int khash=hash_string(key);
  struct hashentry *e;
  for(;h;h=h->next) { //loop through parent scopes
    e = h->buckets[hash_bucket(khash,h->nbuckets)];
    if (!e) { //no bucket list in this table
      continue;
    } else if (e->khash==khash && !strcmp(key,e->k)) { //found key at head of list
      h->buckets[hash_bucket(khash,h->nbuckets)] = e->next;
      h->nelements--;
      free_hashentry(e);
      return 1;
    } else { //else, search for key in list
      for(; e->next; e=e->next) { //scan bucket entries
        if (e->next->khash==khash && !strcmp(key,e->next->k)) { //found it
          struct hashentry *t=e->next;
          e->next=t->next;
          h->nelements--;
          free_hashentry(t);
          return 1;
        } else if (khash<e->next->khash || (e->next->khash==khash && strcmp(key,e->next->k)<0)) { //past it. move on to parent scope
          break;
        }
      }
    }
  }
  return 0;
}

int hash_walk(struct hashtable *h, hash_walker *func, void *arg) {
  if (!h) return 0;
  if (!func) return 0;
  unsigned int i;
  struct hashentry *e;
  int r=0;
  for(i=0;i<h->nbuckets;++i) {
    for(e=h->buckets[i]; e; e=e->next) {
      if ((r=func(e,arg)) < 0) return r;
    }
  }
  return r;
}

struct walk_set_info {
  struct hashtable *h;
  hash_walker *func;
  void *arg;
};

hash_walker walk_set_;
int walk_set_(struct hashentry *e, void *arg) {
  struct walk_set_info *info = arg;
  val_t t;
  int r;
  if ((r = val_clone(&t,&e->v))) return r;
  if (1 == hash_put(info->h,e->k,&t, 0)) {
    if (info->func) info->func(e,info->arg);
  }
  return 0;
}

int hash_walk_chain(struct hashtable *h, hash_walker *func, void *arg) {
  int r=0;
  for(;h;h=h->next) {
    r = hash_walk(h,func,arg);
    if (r < 0) return r;
  }
  return r;
}
int hash_walk_unique(struct hashtable *h, hash_walker *func, void *arg) {
  struct hashtable *hflat = alloc_hashtable(h->nbuckets,NULL);
  if (!hflat) return -1;
  struct walk_set_info info;
  info.h = hflat;
  info.func = func;
  info.arg = arg;

  int r=0;
  for(;h;h=h->next) { //loop through parent scopes, walking over variables we haven't seen before
    r = hash_walk(h,walk_set_,&info);
    if (r < 0) return r;
  }
  dispose_hashtable(hflat);
  return r;
}

struct hashtable* hash_flatten(struct hashtable *h) {
  struct hashtable *hflat = alloc_hashtable(h->nbuckets,NULL);
  if (!hflat) return NULL;
  struct walk_set_info info;
  info.h = hflat;
  info.func = NULL;
  info.arg = NULL;
  int r=0;
  for(;h;h=h->next) { //loop through parent scopes, walking over variables we haven't seen before
    r = hash_walk(h,walk_set_,&info);
    if (r < 0) {
      dispose_hashtable(hflat);
      return NULL;
    }
  }
  return hflat;
}
