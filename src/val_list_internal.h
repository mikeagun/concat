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

#ifndef __VAL_LIST_INTERNAL_H__
#define __VAL_LIST_INTERNAL_H__ 1
#include "val_list.h"

//refcounted list val type
//  - dup+test is common pattern, so dup MUST be O(1)
//  - ldrop/rdrop MUST be O(1) (done for every eval when evaluating code)
//  - call lreserve/rreserve whenever we need space to put stuff on left/right of existing contents
//  - planning for embedded implementation where malloc/free are even more expensive, so try to avoid that
//    - 1.5X growth algorithm when growing memory
//      - small enough to avoid hole filling problem
//      - enough to keep allocations rare as we keep pushing onto large lists
//    - 0.5X shrinking algorithm when sliding vals within existing buffer
//      - don't immediately shift vals all the way over in case we are pushing to both ends (and maybe unequally)
//      - if we have switched push directions, don't take too many steps (logn) to push vals all the way over
//Space reservation algorithm:
//  - immutable if more than one reference (need to clone before making changes)
//    - with threading other threads could have references, so unsafe to fill in INVALIDs unless we add locking
//    - TODO: use refcount or additional int for simple trylock -- clone if we aren't the only one trying
//      - then use reserve/unreserve pattern to safely fill unused vals in threadsafe way
//  - mutable if only one reference (and already have space for request)
//    - always slide instead of free/alloc if space
//    - shrink other side by 0.5X when sliding contents to make space
//      - avoid thrashing if shuffling vals around on both right and left side unequally
//  - if current buffer would have space, realloc same amount
//    - avoid thrashing if we dup, pop down to nothing, then rebuild in reverse
//  - TODO: IDEA: if current buffer would have space, then realloc same amount (with adjusted lspace/rspace)
//  - 1.5X growth algorithm (in direction of reservation) when growing buffer
//    - we preserve free-space on other side when growing and grow current-space+len by 1.5X
//    - if pushing to both ends we will alternately grow left and right sides
//    - if switching push direction we may leave some extra space to other side, but then will slide/fill-in once there is only 1 ref
//TODO: fix refcount == 1 race condition


//list buffer struct
struct val_list_buffer {
  val_t *p;
  unsigned int size;
  unsigned int refcount;
};

//initial allocation size when we realloc
#define LIST_INITIAL_SIZE 8

//reserve and extend list, returning pointer to newly reserved vals
err_t _val_list_lextend(val_t *list, unsigned int n, val_t **p);
err_t _val_list_rextend(val_t *list, unsigned int n, val_t **p);

//helpers to get list members
struct val_list_buffer* _val_list_buf(val_t *list);
unsigned int _val_list_off(val_t *list);

//(unsafe) functions to get elements of list (no checks)
val_t* _val_list_head(val_t *list);
val_t* _val_list_get(val_t *list, int i);
val_t* _val_list_rget(val_t *list, int i);

//alloc/free list buffer
err_t _listbuf_init(val_t *list, unsigned int size);
void _listbuf_destroy(struct val_list_buffer *buf);
err_t _val_list_realloc(val_t *list, unsigned int lspace, unsigned int rspace);
err_t _listbuf_destroy_move(struct val_list_buffer *buf, unsigned int off, unsigned int len, val_t *dest);

//slide a portion of an array of vals
//  - destroy vals we will overwrite
//  - move vals
//  - clear vals we moved that weren't overwritten
void _list_slide(val_t *buf, unsigned int off, unsigned int len, unsigned int newoff);

err_t _val_list_lpop(val_t *list, val_t *first);
err_t _val_list_rpop(val_t *list, val_t *last);
void _val_list_ldrop(val_t *list);
void _val_list_rdrop(val_t *list);

void val_list_braces(val_t *list, const char **open, const char **close);

//print single element of list
//  - handles forward/reverse indexing
//  - uses sublist_fmt for list child elements if sublist_fmt != NULL, val_fmt otherwise
int _val_list_sprintf_el(val_t *buf, val_t *p, int len, int i, int reverse, const list_fmt_t *sublist_fmt, const fmt_t *val_fmt);
int _val_list_fprintf(FILE *file, val_t *p, int len, const char *open, const char *close, const list_fmt_t *lfmt, const fmt_t *val_fmt);
int _val_list_sprintf(val_t *buf, val_t *p, int len, const char *open, const char *close, const list_fmt_t *lfmt, const fmt_t *val_fmt);

//whether a and b point to the same buffer and have the same start offset
int _val_list_samestart(val_t *a, val_t *b);
#endif
