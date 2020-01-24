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

#include "helpers.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char *_strdup(const char *s) {
  size_t len = strlen(s);
  char *r = malloc(len+1);
  if (!r) return NULL;
  memcpy(r,s,len+1);
  return r;
}
char *_strdupn(const char *s, size_t size) {
  size_t len = strlen(s);
  if (len<size) size = len;
  char *r = malloc(size+1);
  if (!r) return NULL;
  memcpy(r,s,size);
  r[size] = '\0';
  return r;
}
char *_memdup(const char *s, size_t size) {
  char *r = malloc(size);
  if (!r) return NULL;
  memcpy(r,s,size);
  return r;
}
char *_strduprange(const char *begin, const char *end) {
  char *r = malloc(end-begin+1);
  if (!r) return NULL;
  memcpy(r,begin,end-begin);
  r[end-begin]='\0';
  return r;
}

int strncmp_cstr(const char *str, unsigned int n, const char *cstr) {
  for(; *cstr && n; --n, ++str, ++cstr) {
    if (*str < *cstr) return -1;
    else if (*cstr < *str) return 1;
  }
  if (*cstr) return -1;
  else if (n) return 1;
  else return 0;
}

char *_strcat(const char *lhs, const char *rhs) {
  size_t llen = strlen(lhs),
         rlen = strlen(rhs);
  char *r = malloc(llen+rlen+1);
  if (!r) return NULL;
  memcpy(r,lhs,llen);
  memcpy(r+llen,rhs,rlen+1);
  return r;
}

char *strtoke(char *str,const char *delim) { //acts like strtok, but also returns empty strings
  static char *_state;
  if (str) _state = str;
  if (!_state) return NULL;

  char *r = _state;
  _state = strpbrk(_state,delim);
  if (_state) *(_state++)='\0';
  return r;
}

const char *strnchr(const char *haystack, unsigned int haystackn, char c) {
  while(haystackn--) {
    if (*haystack==c) return haystack;
    else haystack++;
  }
  return NULL;
}

const char *strnstrn(const char *haystack, unsigned int haystackn, const char *needle, unsigned int needlen) {
  if (!needlen) return haystack;
  else if (needlen > haystackn) return NULL;
  unsigned int matchlen=0;
  while(haystackn--) {
    if (*haystack == needle[matchlen]) {
      matchlen++;
      if (matchlen==needlen) return haystack-needlen+1;
    } else {
      matchlen=0;
    }
    haystack++;
  }
  return NULL;
}

const char *rstrnstrn(const char *haystack, unsigned int haystackn, const char *needle, unsigned int needlen) {
  if (!needlen) return haystack+haystackn;
  else if (needlen > haystackn) return NULL;
  unsigned int matchi=needlen-1;
  haystack+=haystackn-1;
  while(haystackn--) {
    if (*haystack == needle[matchi]) {
      if (matchi==0) return haystack;
      else matchi--;
    } else {
      matchi=needlen-1;
    }
    haystack--;
  }
  return NULL;
}

const char *strnfirstof(const char *haystack, unsigned int haystackn, const char *chars, unsigned int charsn) {
  if (!charsn) return NULL;
  while(haystackn--) {
    if (strnchr(chars,charsn,*haystack)) return haystack;
    else haystack++;
  }
  return NULL;
}
const char *strnlastof(const char *haystack, unsigned int haystackn, const char *chars, unsigned int charsn) {
  if (!charsn) return NULL;
  haystack+=haystackn-1;
  while(haystackn--) {
    if (strnchr(chars,charsn,*haystack)) return haystack;
    else haystack--;
  }
  return NULL;
}
const char *strnfirstnotof(const char *haystack, unsigned int haystackn, const char *chars, unsigned int charsn) {
  if (!charsn) return NULL;
  while(haystackn--) {
    if (!strnchr(chars,charsn,*haystack)) return haystack;
    else haystack++;
  }
  return NULL;
}
const char *strnlastnotof(const char *haystack, unsigned int haystackn, const char *chars, unsigned int charsn) {
  if (!charsn) return NULL;
  haystack+=haystackn-1;
  while(haystackn--) {
    if (!strnchr(chars,charsn,*haystack)) return haystack;
    else haystack--;
  }
  return NULL;
}

int is_op(const char c) {
  const char *all_ops = "~!@$%^*()={}[]<>_,;+-/&|"; //# removed compared to rpn calc
  return strchr(all_ops,c) != NULL;
}

int is_space(const char c) {
  return isspace(c);
}

int is_op_str(const char *s, unsigned int len) { //single char string consisting of op
  return len==1 && is_op(*s);
}

int is_grouping_op(const char c) {
  const char *grouping_ops = "()[]";
  return strchr(grouping_ops,c) != NULL;
}

int is_close_group(const char c) { //removed } since that is the scopestack pop operator
  const char *end_expr = ")]";
  return strchr(end_expr,c) != NULL;
}

int is_number(const char *s, unsigned int len) {
  int state = 0; //TODO: clean this up with enum
  // 0    1          2          3       4      5    6   7    8      9
  //init +/-  leading_decimal digits decimal digits e  +/- digits space
  for(; len; --len,++s) {
    if (isspace(*s)) {
      switch (state) {
        case 0: break;
        case 3: case 4: case 5: case 8: state=9; break;
        case 9: break;
        default: return 0;
      }
    } else if (*s == '+' || *s == '-') {
      switch (state) {
        case 0: state=1; break;
        case 6: state=7; break;
        default: return 0;
      }
    } else if (isdigit(*s)) {
      switch(state) {
        case 0: state=3; break;
        case 1: state=3; break;
        case 2: state=5; break;
        case 3: break;
        case 4: state=5; break;
        case 5: break;
        case 6: state=8; break;
        case 7: state=8; break;
        case 8: break;
        default: return 0;
      }
    } else if (*s == '.') {
      switch(state) {
        case 0: case 1: state=2; break;
        case 3: state=4; break;
        default: return 0;
      }
    } else if (*s == 'e' || *s == 'E') {
      switch(state) {
        case 3: case 4: case 5: state=6; break;
        default: return 0;
      }
    } else return 0;
  }
  switch(state) {
    case 3: case 4: case 5: case 8: case 9: return 1;
    default: return 0;
  }
}

int is_identifier(const char *s, unsigned int len) {
  if (!len) return 0;
  while(len && *s == '\\') { //skip over leading backslashes
    ++s;
    --len;
  }
  if (!(isalnum(*s) || *s == '_' || *s == '.')) return 0; //first check is to make sure not empty string
  for(--len;len>0;--len) {
    if (!(isalnum(s[len]) || s[len] == '_' || s[len] == '.' || is_op(s[len]))) return 0;
  }
  return 1;
}

int parseint_(const char *s, unsigned int len) {
  int i=*s-'0'; s++; len--;
  while(len--) i = i * 10 + *s - '0';
  return i;
}

int isoctal(char o) {
  return (o >= '0' && o <= '7');
}
int ishex(char h) {
  return (h>='0' && h<='9') || (h>='a' && h<='f') || (h>='F' && h<='F');
}

char dehex(char h) {
  return (h > '9') ? 9 + (h&0xf) : h-'0';
}

int ishex2(const char *h) {
  return
    ((h[0]>='0' && h[0]<='9') || (h[0]>='a' && h[0]<='f') || (h[0]>='F' && h[0]<='F')) &&
    ((h[1]>='0' && h[1]<='9') || (h[1]>='a' && h[1]<='f') || (h[1]>='F' && h[1]<='F'));
}

char dehex2(const char *h) {
  unsigned char hc = h[0] - '0';
  unsigned char lc = h[1] - '0';
  if (hc > 9) hc += 9;
  if (lc > 9) lc += 9;
  return (hc << 4) + (lc & 0xf);
}

unsigned int _compute_exp_slide(
    unsigned int off, unsigned int len, unsigned int size,
    unsigned int n) {
  if (off >= n) { //already have space
    return off;
  }
  //     (              a    b    c    d    e    f    g                )
  //     0             off                         off+len          size
  //                                                      lside    rspace
  //                                         destroy(off+len to lside)
  //     0                 newoff                      newoff+len   size
  //           clear(off to newoff)
  unsigned int lside = len+off;
  unsigned int rspace = size-lside;
//    - calculate newoff by shinking rspace by /=2 until lspace>=n
  while(lside - len < n) {
    lside += (rspace+1)/2;
    rspace /= 2;
  }
  return lside-len;
}

//add option for maximum exponential growth
void _compute_exp_lreserve(
    unsigned int off, unsigned int len, unsigned int size,
    unsigned int n, unsigned int min_size,
    unsigned int *lspace, unsigned int *rspace) {
  *lspace=off;
  *rspace=size-len-off;
  if (off >= n) return; //if we already had enough space realloc same
  //multiply len+lspace on left by 3/2 (round up) until we get there
  unsigned int lside = len+off;
  if (lside < 1) lside = 1;
  while (lside - len < n) {
    lside = lside + (lside+1)/2;
  }
  //min alloc is LIST_INITIAL_SIZE
  if (lside+*rspace < min_size) {
    lside=min_size-*rspace; //put extra space to left
  }
  *lspace=lside-len;
  return;
}

