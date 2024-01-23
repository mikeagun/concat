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

//Macros for generating typed memory pools
//  - each of the macros defines a (de)allocator with a type, a unit of allocation, an allocation function name, and a free function name
//  - DEFINE_NO_POOL - uses malloc directory (no memory pool) and ignores alloc_size
//  - DEFINE_SIMPLE_POOL - allocates chunks, keeping free entries in a linked list stack (never frees memory, just returns to stack for reuse)
//DEFINE_SIMPLE_POOL:
//    - uses a linked-list stack to keep free entries
//      - good locality side-effect of this is that we always take the most-recently freed value
//      - each entry is a union of the allocation type and a next pointer
//    - the union used for the pool entries is named after the allocation function
//    - current implementation never returns memory to system
//      - with this linked-list stack implementation, to return system memory you would identify allocation blocks and count how many free entries we have in each block (e.g. by sorting-by-block), and then we can free blocks where we have all the entries for that block in the free pool
//      - compaction when we don't have full free blocks is more difficult, but since everything is in one of the stacks is still possible (and probably never worth it)
//TODO: need threadsafe version of the memory pool
//  - we need either a separate pool per thread (and then either merge or free all when thread finished), or appropriate locking
//    - per-thread pools
//      - faster (obviously, no locks)
//      - need a mechanism to pass around thread-specific pool pointer (probably thread local var, but that may be harder to port everywhere vs locking)
//      - on thread close need to make sure to clean everything up -- free all (problematic with inter-thread comms), or add to pool of thread calling join (need pool pointer in vm val)
//    - locking -- slower, but means process just needs one pool for each type, no possibly messy cleanup cases

//TODO: add helgrind annotations so helgrind (valgrind) knows reused pool entries are "new" memory
//  ANNOTATE_NEW_MEMORY(v,sizeof(type));
//TODO: add appropriate memcheck annotations (possibly VALGRIND_CREATE_BLOCK/VALGRIND_DISCARD, probably VALGRIND_MEMPOOL_*

//#define DEFINE_DEFAULT_POOL(type,alloc_size,alloc_name,free_name) DEFINE_NO_POOL(type,alloc_size,alloc_name,free_name)

#define DEFINE_SIMPLE_POOL(type,alloc_size,alloc_name,free_name) \
union ualloc_##alloc_name { \
  type v; \
  union ualloc_##alloc_name *next; \
} *nextfree_##alloc_name=NULL; \
type* alloc_name() { \
  type *v; \
  if (nextfree_##alloc_name) { \
    v = &nextfree_##alloc_name->v; \
    nextfree_##alloc_name = nextfree_##alloc_name->next; \
    return v; \
  } else { \
    union ualloc_##alloc_name  *chunk; \
    if (!(chunk = malloc(alloc_size))) return NULL; \
    v = &chunk->v; \
    if (((alloc_size)/sizeof(union ualloc_##alloc_name))>1) { \
      union ualloc_##alloc_name  *last = chunk+((alloc_size)/sizeof(union ualloc_##alloc_name))-1; \
      last->next=NULL; \
      ++chunk; \
      nextfree_##alloc_name=chunk; \
      for(;chunk!=last;++chunk) { \
        chunk->next=chunk+1; \
      } \
    } \
    return v; \
  } \
} \
void free_name(type *v) { \
  ((union ualloc_##alloc_name *)v)->next = nextfree_##alloc_name; \
  nextfree_##alloc_name = (union ualloc_##alloc_name *)v; \
}

#define DEFINE_NO_POOL(type,alloc_size,alloc_name,free_name) \
  type* alloc_name() { return malloc(sizeof(type)); } \
  void free_name(type *v) { free(v); }
