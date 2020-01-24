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

#ifndef __VAL_STRING_INTERNAL_H__
#define __VAL_STRING_INTERNAL_H__ 1

#include "val_string.h"
#include "val_printf.h"


//TODO: fix refcount == 1 race condition

struct val_string_buffer {
  char *p;
  unsigned int size;
  unsigned int refcount;
};

//TODO: select a value
#define STRING_INITIAL_SIZE 8

//helpers to get string members
struct val_string_buffer* _val_string_buf(val_t *string);
unsigned int _val_string_off(val_t *string);

//(unsafe) functions to get elements of string (no checks)
char* _val_string_str(val_t *string);
char* _val_string_get(val_t *string, int i);
char* _val_string_rget(val_t *string, int i);
char* _val_string_bget(val_t *string, int i);

//set contents of string (deref/reserve as needed)
err_t _val_string_set(val_t *string, const char *s, unsigned int len);

//alloc/free string buffer
err_t _stringbuf_init(val_t *string, unsigned int size);
void _stringbuf_destroy(struct val_string_buffer *buf);
err_t _val_string_realloc(val_t *string, unsigned int lspace, unsigned int rspace);

int _val_string_fprintf(FILE *file, val_t *string, int precision);
int _val_string_sprintf(val_t *buf, val_t *string, int precision);
int _val_string_fprintf_quoted(FILE *file, val_t *string, const fmt_t *fmt);
int _val_string_sprintf_quoted(val_t *buf, val_t *string, const fmt_t *fmt);

//helpers for printing quoted (and escaped) strings
const char* _string_find_dq_special(const char *str, unsigned int len); //find first char that needs escaping
const char* _string_rfind_dq_special(const char *str, unsigned int len); //find first char (from end) that needs escaping
int _string_findi(const char *str,unsigned int len, const char *substr,unsigned int substrn);
int _string_rfindi(const char *str,unsigned int len, const char *substr,unsigned int substrn);
int _string_findi_whitespace(const char *str,unsigned int len);
int _string_findi_notwhitespace(const char *str,unsigned int len);
int _string_rfindi_notwhitespace(const char *str,unsigned int len);
int _string_findi_of(const char *str,unsigned int len, const char *chars,unsigned int nchars);
int _string_rfindi_of(const char *str,unsigned int len, const char *chars,unsigned int nchars);
int _string_findi_notof(const char *str,unsigned int len, const char *chars,unsigned int nchars);
int _string_rfindi_notof(const char *str,unsigned int len, const char *chars,unsigned int nchars);
unsigned int _string_escape_ch(unsigned char c, char *buf); //generate escape for char to insert in quoted string

#endif
