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

#ifndef __HASH_H__
#define __HASH_H__
//hash.h -- chained hashtable implementation
//Implements a chained-hashtable which supports get,put,delete,walk, and keeps next pointers to implement scoped hashtables
//
//NOTES:
//  - uses string keys with FNV hash function
//  - FIXME: make sure no key leaks
//  - TODO: add alloc()/free() param to core put()/del() calls, then can just pass identity function for no alloc, or malloc for simple solution
#include "helpers.h"
#include "val.h"


#include<stdlib.h>
#include<string.h>

//default number of hash buckets (# hash buckets must always be power of 2, or you need to replace hash_bucket() function)
#define DEFAULT_HASH_BUCKETS 128

#define ASSERT_CONCAT_(a, b) a##b
#define ASSERT_CONCAT(a, b) ASSERT_CONCAT_(a, b)
/* These can't be used after statements in c89. */
#ifdef __COUNTER__
  #define STATIC_ASSERT(e,m) \
    ;enum { ASSERT_CONCAT(static_assert_, __COUNTER__) = 1/(int)(!!(e)) }
#else
  /* This can't be used twice on the same line so ensure if using in headers
   * that the headers are not included twice (by wrapping in #ifndef...#endif)
   * Note it doesn't cause an issue when used on same line of separate modules
   * compiled with gcc -combine -fwhole-program.  */
  #define STATIC_ASSERT(e,m) \
    ;enum { ASSERT_CONCAT(assert_line_, __LINE__) = 1/(int)(!!(e)) }
#endif

STATIC_ASSERT(sizeof(int)==4,"code assumes 32bit integers. Update source");

//FNV (Fowler-Noll-Vo) hash function
//Constants:
//  32bit - 2166136261,16777619
//  64bit - 14695981039346656037,1099511628211
unsigned int hash_string(const char* s);

struct hashentry {
  char *k; //key
  val_t v; //value
  unsigned int khash; //we save the key hash to speed up hash searches
  struct hashentry *next;
  int free_key;
  //free_func free_k; //if we want per-entry behaviour for freeing keys/values
  //free_func free_v;
};

struct hashtable {
  struct hashentry **buckets; //array of bucket pointers
  unsigned int nbuckets; //number of bucket pointers
  unsigned int nelements; //number of inserted elements
  struct hashtable *next; //parent hashtable
  //free_func free_k; //if we want per-table behaviour for freeing keys/values
  //free_func free_v;
};

unsigned int hash_bucket(const unsigned int khash, const unsigned int nbuckets);
//inline unsigned int hash_bucket(const unsigned int khash, const unsigned int nbuckets);

typedef int (hash_walker)(struct hashentry *e, void *arg);

struct hashentry* alloc_hashentry(char *key, val_t *value, unsigned int khash, struct hashentry *next, int free_key); //alloc new hashentry
struct hashentry* alloc_hashentry_kcopy(const char *key, val_t *value, unsigned int khash, struct hashentry *next); //alloc new hashentry, copying key
struct hashentry* alloc_hashentry_kval(val_t *key, val_t *value, unsigned int khash, struct hashentry *next); //alloc new hashentry
void              free_hashentry(struct hashentry *e); //free hashentry struct and destroy e->v
void              dispose_hashentry(struct hashentry *e); //free hashentry struct and all entries in chain (and destroys values) //FIXME should this destroy also???
struct hashtable* alloc_hashtable(unsigned int nbuckets, struct hashtable *next); //alloc new hashtable
struct hashtable* free_hashtable(struct hashtable *h); //free hashtable struct and return h->next
struct hashtable* dispose_hashtable(struct hashtable *h); //free hashtable struct and all entries in hashtable (including values) and return h->next
struct hashtable* dispose_merge_hashtable(struct hashtable *h); //copy all values from h to h->next and free all entries, then return h->next

//returns -1 on error, 0 if key already exists and overwrite==0, 1 if inserts new item, 2 if updates existing item -- key is not copied but directly stored
int hash_put(struct hashtable *h, char *key, val_t *value, int overwrite);

//returns -1 on error, 0 if key already exists and overwrite==0, 1 if inserts new item, 2 if updates existing item -- copies the key (if insert)
int hash_put_kcopy(struct hashtable *h, const char *key, val_t *value, int overwrite);
int hash_putv(struct hashtable *h, val_t *key, val_t *value, int overwrite);
//returns -1 on error, 0 if key already exists and overwrite==0, 1 if inserts new item, 2 if updates existing item -- casts away the constness of the key (then have to be careful not to try to modify/free the key later)
int hash_put_kcast(struct hashtable *h, const char *key, val_t *value, int overwrite);

val_t* hash_get(struct hashtable *h, const char *key);
val_t* hash_get_(struct hashtable *h, const char *key, unsigned int len);
val_t* hash_getv(struct hashtable *h, val_t *key);
int hash_delete(struct hashtable *h, const char *key);

//clone a hashtable, flattening the chain where shallower table values take priority
//  - note that all keys are marked non-free in the flattened table, so freeing h may break the returned table (this is meant to be a temporary copy for iteration)
struct hashtable* hash_flatten(struct hashtable *h);

//walks every entry in current hashtable (but not ancestors), calling func() on every entry.
//  - on negative return value from func(), returns that value
//  - if every call to func returns >= 0, in the end returns last return value from func()
//  - arg is passed unmodified to each call of func (to e.g. track state)
int   hash_walk(struct hashtable *h, hash_walker *func, void *arg);

//same as hash_walk, but also walks all entries in ancestors (even if they were also walked in descendants)
int   hash_walk_all(struct hashtable *h, hash_walker *func, void *arg);

//same as hash_walk, but also walks entries in ancestors (where they aren't shadowed by descendants)
int   hash_walk_unique(struct hashtable *h, hash_walker *func, void *arg);

struct hashentry* _hash_putfree(struct hashtable *h, struct hashentry *entry, int overwrite);

#endif
