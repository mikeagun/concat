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

#ifndef __VAL_HASH_H__
#define __VAL_HASH_H__ 1

#include "val.h"

//#define ASSERT_CONCAT_(a, b) a##b
//#define ASSERT_CONCAT(a, b) ASSERT_CONCAT_(a, b)
///* These can't be used after statements in c89. */
//#ifdef __COUNTER__
//  #define STATIC_ASSERT(e,m) \
//    ;enum { ASSERT_CONCAT(static_assert_, __COUNTER__) = 1/(int)(!!(e)) }
//#else
//  /* This can't be used twice on the same line so ensure if using in headers
//   * that the headers are not included twice (by wrapping in #ifndef...#endif)
//   * Note it doesn't cause an issue when used on same line of separate modules
//   * compiled with gcc -combine -fwhole-program.  */
//  #define STATIC_ASSERT(e,m) \
//    ;enum { ASSERT_CONCAT(assert_line_, __LINE__) = 1/(int)(!!(e)) }
//#endif
//
//STATIC_ASSERT(sizeof(int)==4,"code assumes 32bit integers. Update source");


//default number of hash buckets (# hash buckets must always be power of 2, or you need to replace hash_bucket() function)
#define DEFAULT_HASH_BUCKETS 128




struct hashentry {
  valstruct_t k; //key string
  val_t v; //value
  uint32_t khash; //we save the key hash to speed up hash searches
  struct hashentry *next;
};

struct hashtable {
  unsigned int nbuckets; //number of bucket pointers
  unsigned int size; //number of inserted elements
  unsigned int refcount; //number of references that exist to this ht
  struct hashentry *buckets[]; //array of bucket pointers
};

unsigned int hash_bucket(const uint32_t khash, const unsigned int nbuckets);

struct hashtable* alloc_hashtable();
void release_hashtable(struct hashtable *h);

err_t hash_clone(struct hashtable **ret, struct hashtable *orig);

typedef int (hash_visitor)(struct hashentry *e, void *arg);

val_t hash_get(struct hashtable *h, valstruct_t *key);
val_t* _hash_get(struct hashtable *h, valstruct_t *key);
val_t hash_get_(struct hashtable *h, const char *key, unsigned int len);

//returns -1 on error, 0 if key already exists and overwrite==0, 1 if inserts new item, 2 if updates existing item -- key and value both consumed when r>0 (otherwise left alone)
int hash_put(struct hashtable *h, valstruct_t *key, val_t value, int overwrite);
//returns -1 on error, 0 if key already exists and overwrite==0, 1 if inserts new item, 2 if updates existing item -- key and value cloned as needed
int hash_put_copy(struct hashtable *h, valstruct_t *key, val_t value, int overwrite);


//visits every entry in current hashtable, calling vistor visit() on every entry.
//  - on negative return value from visit(), returns that value
//  - if every call to func returns >= 0, in the end returns last return value from visit()
//  - arg is passed unmodified to each call of func (to e.g. track state)
int hash_visit(struct hashtable *h, hash_visitor *visit, void *arg);

#endif
