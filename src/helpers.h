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

#ifndef __HELPERS_H__
#define __HELPERS_H__ 1

#include <stddef.h>

//======= refcount functions ========
// - these are used to atomically increment, decrement, and check refcounts
//
//#define refcount_inc(refcount) (++(refcount))
//#define refcount_dec(refcount) (--(refcount))
//#define refcount_singleton(refcount) (1 == (refcount))
//
//TODO: replace these with the newer atomic_load_n, atomic_add_fetch, atomic_sub_fetch
#define refcount_inc(refcount) __sync_add_and_fetch(&(refcount),1)
#define refcount_dec(refcount) __sync_sub_and_fetch(&(refcount),1)
//if singleton returns true then we are currently looking at the only copy, so we can safely modify it in-place
#define refcount_singleton(refcount) (1 == __sync_fetch_and_add(&(refcount),0))

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

char *_strdup(const char *s);
char *_strdupn(const char *s, size_t size);
char *_memdup(const char *s, size_t size);
char *_strduprange(const char *begin, const char *end);

int strncmp_cstr(const char *str, unsigned int len, const char *cstr);

char *_strcat(const char *lhs, const char *rhs);

char *strtoke(char *str,const char *delim);

const char *strnchr(const char *haystack, unsigned int haystackn, char c);
const char *strnstrn(const char *haystack, unsigned int haystackn, const char *needle, unsigned int needlen);
const char *rstrnstrn(const char *haystack, unsigned int haystackn, const char *needle, unsigned int needlen);
const char *strnfirstof(const char *haystack, unsigned int haystackn, const char *chars, unsigned int charsn);
const char *strnlastof(const char *haystack, unsigned int haystackn, const char *chars, unsigned int charsn);
const char *strnfirstnotof(const char *haystack, unsigned int haystackn, const char *chars, unsigned int charsn);
const char *strnlastnotof(const char *haystack, unsigned int haystackn, const char *chars, unsigned int charsn);

int is_op(const char c);
int is_space(const char c);
int is_op_str(const char *s, unsigned int len);
int is_grouping_op(const char c);
int is_close_group(const char c);

int is_number(const char *s, unsigned int len);
int is_identifier(const char *s, unsigned int len);

int parseint_(const char *s, unsigned int len); //parse positive integer from string (assumes chars have already been validated and len>0)


int isoctal(char o);
int ishex(char h);
char dehex(char h);
int ishex2(const char *h);
char dehex2(const char *h);

//helpers for exponential buffer allocation
//computes newoff so lspace >= n
unsigned int _compute_exp_slide(unsigned int off, unsigned int len, unsigned int size, unsigned int n);
//computes lspace,rspace so lspace >= n
void _compute_exp_lreserve(unsigned int off, unsigned int len, unsigned int size, unsigned int n, unsigned int min_size, unsigned int *lspace, unsigned int *rspace);
#endif
