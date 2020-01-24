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

#include "val_string_internal.h"
#include "val_num.h"
#include "val_cast.h"

#include "vm_err.h"
#include "vm_debug.h"
#include "helpers.h"
#include <string.h>
#include <stdlib.h>

//TODO: decide between exponential/requested allocation for different cases

void val_string_init_handlers(struct type_handlers *h) {
  h->destroy = val_string_destroy;
  h->clone = val_string_clone;
  h->fprintf = val_string_fprintf;
  h->sprintf = val_string_sprintf;
}

void val_string_init(val_t *val) {
  val->type = TYPE_STRING;
  val->val.string.offset = 0;
  val->val.string.len = 0;
  val->val.string.b = NULL;
  VM_DEBUG_VAL_INIT(val);
}
err_t val_string_init_(val_t *val, const char *str, unsigned int len) {
  val_string_init(val);
  return _val_string_set(val,str,len);
}
err_t val_string_init_cstr(val_t *val, const char *str) {
  val_string_init(val);
  return _val_string_set(val,str,strlen(str));
}
err_t val_string_init_dquoted(val_t *val, const char *str, unsigned int len) {
  throw_if(ERR_BADARGS,len<2);
  val_string_init(val);
  str++;
  len-=2; //assumes (without checking) that first and last char are '"'
  if (!len) return 0;
  
  err_t r;
  if ((r = val_string_rreserve(val,len))) return r;
  char *s = _val_string_str(val);
  
  unsigned int sn=0;
  for(;len;--len,++str) {
    if (*str == '\\') {
      if (len<2) goto out_badescape;
      --len;++str; //str now points to char after '\\'
      switch(*str) {
        //first handle \,", and /
        case '\\': s[sn++] = '\\'; break;
        case '"':  s[sn++] = '"'; break;
        case '/':  s[sn++] = ('/'); break;
        //now handle control characters with short representations
        case 'b':  s[sn++] = ('\b');  break;
        case 'f':  s[sn++] = ('\f');  break;
        case 'n':  s[sn++] = ('\n');  break;
        case 'r':  s[sn++] = ('\r');  break;
        case 't':  s[sn++] = ('\t');  break;
        case '\'': s[sn++] = ('\''); break;
        //case '?':  s[sn++] = ('?'); break;
        case 'a':  s[sn++] = ('\a');  break;
        case 'v':  s[sn++] = ('\v');  break;
        //now handle generic escapes.
        case 'u': //u then 4 hex chars -- 2 byte escape
          --len;++str;
          if (len>=4 && ishex2(str) && ishex2(str+2)) {
            s[sn++] = dehex2(str);
            s[sn++] = dehex2(str+2);
            len -= 3;
            str += 3;
          } else goto out_badescape;
          break;
        case 'U': //u then 8 hex chars -- 4 byte escape
          --len;++str;
          if (len>=8 && ishex2(str) && ishex2(str+2) && ishex2(str+4) && ishex2(str+6)) {
            s[sn++] = dehex2(str);
            s[sn++] = dehex2(str+2);
            s[sn++] = dehex2(str+4);
            s[sn++] = dehex2(str+6);
            len -= 7;
            str += 7;
          } else goto out_badescape;
          break;
        case 'x': //u then 2 hex chars -- 1 byte escape
          --len;++str;
          if (len>=2 && ishex2(str)) {
            s[sn++] = dehex2(str);
            len -= 1;
            str += 1;
          } else goto out_badescape;
          break;
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': //1-3 octal chars -- 1 byte escape
          if (len>=2 && isoctal(str[1])) { //at least 2 octal digits
            if (len>=3 && isoctal(str[2])) { //3 octal digits
              s[sn++] = (((str[0]-'0') << 6) | ((str[1]-'0') << 3) | (str[2]-'0'));
              len -= 2; str += 2;
            } else { //2 octal digits
              s[sn++] = (((str[0]-'0') << 3) | (str[1]-'0'));
              --len;++str;
            }
          } else { //1 octal digit
            s[sn++] = (str[0]-'0');
          }
          break;
        default:
          s[sn++] = *str;
          //goto out_badescape; //we let any char be backslash escaped
      }
    } else {
      s[sn++]=*str;
    }
  }
  val->val.string.len=sn;
  return 0;
out_badescape:
  val_destroy(val);
  return _throw(ERR_BADESCAPE);
}
err_t val_string_init_squoted(val_t *val, const char *str, unsigned int len) {
  err_t r;
  if ((r = val_string_init_(val,str+1,len-2))) return r;
  return 0;
}

err_t val_string_destroy(val_t *string) {
  argcheck(val_isstringtype(string));
  VM_DEBUG_VAL_DESTROY(string);
  string->type = VAL_INVALID;
  _stringbuf_destroy(string->val.string.b);
  return 0;
}
err_t val_string_clone(val_t *val, val_t *orig) {
  argcheck_r(val_isstringtype(orig));
  VM_DEBUG_VAL_CLONE(val,orig);
  *val = *orig;
  if (_val_string_buf(val)) refcount_inc(val->val.string.b->refcount);
  return 0;
}
int val_string_fprintf(val_t *string, FILE *file, const fmt_t *fmt) {
  argcheck(val_isstringtype(string));
  switch(fmt->conversion) {
    case 's': case 'v':
      return _val_string_fprintf(file,string,fmt->precision);
    case 'V':
      return _val_string_fprintf_quoted(file,string,fmt);
    default:
      return _throw(ERR_BADTYPE);
  }
}
int val_string_sprintf(val_t *string, val_t *buf, const fmt_t *fmt) {
  argcheck(val_isstringtype(string));
  switch(fmt->conversion) {
    case 's': case 'v':
      return _val_string_sprintf(buf,string,fmt->precision);
    case 'V':
      return _val_string_sprintf_quoted(buf,string,fmt);
    default:
      return _throw(ERR_BADTYPE);
  }
}

void val_string_clear(val_t *string) {
  argcheck(val_isstringtype(string));
  string->val.string.len = 0;
}
unsigned int val_string_len(val_t *string) {
  argcheck(val_isstringtype(string));
  return string->val.string.len;
}
int val_string_empty(val_t *string) {
  argcheck(val_isstringtype(string));
  return string->val.string.len == 0;
}
int val_string_small(val_t *string) {
  argcheck(val_isstringtype(string));
  return string->val.string.len <= 1;
}

err_t val_string_deref(val_t *string) {
  argcheck_r(val_isstringtype(string));
  struct val_string_buffer *buf = _val_string_buf(string);
  if (buf && buf->refcount == 1) return 0;
  else {
    return _val_string_realloc(string,0,0); //TODO: should we keep some extra if we had extra?
  }
}

err_t val_string_lpushc_(val_t *string, char c) {
  err_t e;
  if ((e = val_string_lreserve(string,1))) return e;
  string->val.string.offset--;
  string->val.string.len++;
  *_val_string_str(string) = c;
  return 0;
}
err_t val_string_rpushc_(val_t *string, char c) {
  err_t e;
  if ((e = val_string_rreserve(string,1))) return e;
  string->val.string.len++;
  *_val_string_rget(string,0) = c;
  return 0;
}

err_t val_string_lpop(val_t *string, val_t *el) {
  argcheck_r(val_isstringtype(string) && !val_string_empty(string));
  if (el) return val_string_lsplitn(string,el,1);
  else return val_string_substring(string,1,-1);
}
err_t val_string_rpop(val_t *string, val_t *el) {
  argcheck_r(val_isstringtype(string) && !val_string_empty(string));
  if (el) return val_string_rsplitn(string,el,val_string_len(string)-1);
  else return val_string_substring(string,0,val_string_len(string)-1);
}

err_t val_string_cat(val_t *string, val_t *rhs) {
  argcheck_r(val_isstringtype(string));
  err_t r;
  if (!val_isstring(rhs) && (r = val_to_string(rhs))) { val_destroy(rhs); return r; }
  unsigned int len = val_string_len(rhs);
  if (!len) return 0;
  else if (val_string_empty(string)) {
    _stringbuf_destroy(_val_string_buf(string));
    val_move(string,rhs);
    return 0;
  } else {
    r = val_string_cat_(string,_val_string_str(rhs),len);
    val_string_destroy(rhs);
    return r;
  }
}
err_t val_string_lcat(val_t *string, val_t *lhs) {
  argcheck_r(val_isstringtype(string));
  err_t r;
  if (!val_isstring(lhs) && (r = val_to_string(lhs))) { val_destroy(lhs); return r; }
  unsigned int len = val_string_len(lhs);
  if (!len) return 0;
  else if (val_string_empty(string)) {
    _stringbuf_destroy(_val_string_buf(string));
    val_move(string,lhs);
    return 0;
  } else {
    r = val_string_lcat_(string,_val_string_str(lhs),len);
    val_string_destroy(lhs);
    return r;
  }
}
err_t val_string_cat_(val_t *string, const char *rhs, unsigned int len) {
  argcheck_r(val_isstringtype(string));
  err_t r;
  char *p;
  if ((r = val_string_rextend(string,len,&p))) return r;
  memcpy(p,rhs,len);
  return 0;
}
err_t val_string_lcat_(val_t *string, const char *lhs, unsigned int len) {
  argcheck_r(val_isstringtype(string));
  err_t r;
  char *p;
  if ((r = val_string_lextend(string,len,&p))) return r;
  memcpy(p,lhs,len);
  return 0;
}
err_t val_string_cat_copy(val_t *string, val_t *rhs) {
  argcheck_r(val_isstringtype(string) && val_isstringtype(rhs));
  unsigned int len = val_string_len(rhs);
  if (!len) return 0;
  else if (val_string_empty(string)) {
    _stringbuf_destroy(_val_string_buf(string));
    val_string_clone(string,rhs);
    return 0;
  } else {
    return val_string_cat_(string,_val_string_str(rhs),len);
  }
}
err_t val_string_lcat_copy(val_t *string, val_t *lhs) {
  argcheck_r(val_isstringtype(string) && val_isstringtype(lhs));
  unsigned int len = val_string_len(lhs);
  if (!len) return 0;
  else if (val_string_empty(string)) {
    _stringbuf_destroy(_val_string_buf(string));
    val_string_clone(string,lhs);
    return 0;
  } else {
    return val_string_lcat_(string,_val_string_str(lhs),len);
  }
}

err_t val_string_substring(val_t *string, unsigned int off, int len) {
  argcheck_r(val_isstringtype(string) && off<=val_string_len(string));
  unsigned int curlen = val_string_len(string);
  if (len < 0 || len+off>curlen) len = curlen - off;
  string->val.string.offset += off;
  string->val.string.len = len;
  return 0;
}
err_t val_string_rsplitn(val_t *string, val_t *rhs, unsigned int i) {
  argcheck_r(val_isstringtype(string) && (i<=val_string_len(string)));
  val_string_clone(rhs,string);
  val_string_substring(rhs,i,val_string_len(string)-i);
  val_string_substring(string,0,i);
  return 0;
}
err_t val_string_lsplitn(val_t *string, val_t *lhs, unsigned int i) {
  argcheck_r(val_isstringtype(string) && (i<=val_string_len(string)));
  val_string_clone(lhs,string);
  val_string_substring(lhs,0,i);
  val_string_substring(string,i,val_string_len(string)-i);
  return 0;
}
err_t val_string_unwrap(val_t *string) {
  return val_string_substring(string,0,1);
}

err_t val_string_ith(val_t *string, unsigned int i) {
  return val_string_substring(string,i,1);
}
err_t val_string_dith(val_t *string, unsigned int i, val_t *el) {
  argcheck_r(val_isstringtype(string) && i<val_string_len(string));
  err_t r;
  if ((r = val_string_clone(el,string))) return r;
  return val_string_substring(string,i,1);
}

err_t val_string_seti(val_t *string, unsigned int i, char c) {
  argcheck_r(val_isstringtype(string) && i<val_string_len(string));
  *_val_string_get(string,i) = c;
  return 0;
}


err_t val_string_split(val_t *string) {
  argcheck_r(val_isstringtype(string));
  val_t list,t;
  val_list_init(&list);
  err_t r;
  const char *s= val_string_str(string);
  unsigned int n = val_string_len(string);
  int i;
  while(0>=(i = _string_findi_whitespace(s,n))) {
    s+=i+1;
    n-=i+1;
    if (i) {
      if ((r = val_string_lsplitn(string,&t,i))) goto out_list;
      if ((r = val_list_rpush(&list,&t))) goto out_t;
    } else {
      val_string_substring(string,1,n); //ignore empty tok
    }
  }
  if (n) { //final tok
    if ((r = val_list_rpush(&list,string))) goto out_list;
  } else {
    val_destroy(string);
  }
  val_swap(string,&list);
  return 0;
out_t:
  val_destroy(&t);
out_list:
  val_destroy(&list);
  return r;
}
err_t val_string_split2_(val_t *string, const char *splitters, unsigned int nsplitters) {
  argcheck_r(val_isstringtype(string));
  val_t list,t;
  val_list_init(&list);
  err_t r;
  const char *s= val_string_str(string);
  unsigned int n = val_string_len(string);
  int i;
  if (nsplitters) {
    while(0>=(i = _string_findi_of(s,n, splitters,nsplitters))) {
      s+=i+1;
      n-=i+1;
      if ((r = val_string_lsplitn(string,&t,i))) goto out_list;
      if ((r = val_list_rpush(&list,&t))) goto out_t;
    }
    if (n) { //final tok
      if ((r = val_list_rpush(&list,string))) goto out_list;
    } else {
      val_destroy(string);
    }
  } else {
    while(n--) {
      if ((r = val_string_lsplitn(string,&t,1))) goto out_list;
      if ((r = val_list_rpush(&list,&t))) goto out_t;
    }
    val_destroy(string);
  }
  val_swap(string,&list);
  return 0;
out_t:
  val_destroy(&t);
out_list:
  val_destroy(&list);
  return r;
}
err_t val_string_join(val_t *list) {
  argcheck_r(val_islisttype(list));
  val_t buf;
  val_string_init(&buf);
  err_t r;
  if (0>(r = val_list_sprintf_(list,&buf,list_fmt_join,fmt_v))) {
    val_destroy(&buf);
    return r;
  }
  return 0;
}
err_t val_string_join2_(val_t *list, const char *sep, unsigned int seplen) {
  argcheck_r(val_islisttype(list));
  list_fmt_t lfmt;
  val_list_format_join2(&lfmt,sep,seplen);
  val_t buf;
  val_string_init(&buf);
  err_t r;
  if (0>(r = val_list_sprintf_(list,&buf,&lfmt,fmt_v))) {
    val_destroy(&buf);
    return r;
  }
  return 0;
}

void val_string_trim(val_t *string) {
  argcheck(val_isstringtype(string));
  unsigned int n = val_string_len(string);
  const char *p = val_string_str(string);
  int i;

  //trim right (find last non-space)
  i = _string_rfindi_notwhitespace(p,n);
  if (i>=0) {
    string->val.string.len = i+1;
    n = i+1;
  }

  //trim left (find first non-space)
  i = _string_findi_notwhitespace(p,n);
  if (i>=0) {
    string->val.string.offset += i;
    string->val.string.len -= i;
  } else {
    string->val.string.len = 0;
  }
}
err_t val_string_padleft(val_t *string,char c, int n) { //pad left with n chars of c
  argcheck_r(val_isstringtype(string));
  if (!n) return 0;
  err_t r;
  char *p;
  if ((r = val_string_lextend(string,n,&p))) return r;
  while(n--) *(p++) = c;
  return 0;
}
err_t val_string_padright(val_t *string,char c, int n) { //pad right with n chars of c
  argcheck_r(val_isstringtype(string));
  if (!n) return 0;
  err_t r;
  char *p;
  if ((r = val_string_rextend(string,n,&p))) return r;
  while(n--) *(p++) = c;
  return 0;
}

err_t val_string_find_(val_t *string, const char *substr, unsigned int substrn) {
  argcheck_r(val_isstringtype(string));
  int i = -1;
  unsigned int len = val_string_len(string);
  if (len) {
    i = _string_findi(_val_string_str(string),len,substr,substrn);
  }
  val_string_destroy(string);
  val_int_init(string,i);
  return 0;
}
err_t val_string_rfind_(val_t *string, const char *substr, unsigned int substrn) {
  argcheck_r(val_isstringtype(string));
  int i = -1;
  unsigned int len = val_string_len(string);
  if (len) {
    i = _string_rfindi(_val_string_str(string),len,substr,substrn);
  }
  val_string_destroy(string);
  val_int_init(string,i);
  return 0;
}
err_t val_string_firstof_(val_t *string, const char *accept, unsigned int naccept) {
  argcheck_r(val_isstringtype(string));
  int i = -1;
  unsigned int len = val_string_len(string);
  if (len) {
    i = _string_findi_of(_val_string_str(string),len,accept,naccept);
  }
  val_string_destroy(string);
  val_int_init(string,i);
  return 0;
}
err_t val_string_lastof_(val_t *string, const char *accept, unsigned int naccept) {
  argcheck_r(val_isstringtype(string));
  int i = -1;
  unsigned int len = val_string_len(string);
  if (len) {
    i = _string_rfindi_of(_val_string_str(string),len,accept,naccept);
  }
  val_string_destroy(string);
  val_int_init(string,i);
  return 0;
}
err_t val_string_firstnotof_(val_t *string, const char *reject, unsigned int nreject) {
  argcheck_r(val_isstringtype(string));
  int i = -1;
  unsigned int len = val_string_len(string);
  if (len) {
    i = _string_findi_notof(_val_string_str(string),len,reject,nreject);
  }
  val_string_destroy(string);
  val_int_init(string,i);
  return 0;
}
err_t val_string_lastnotof_(val_t *string, const char *reject, unsigned int nreject) {
  argcheck_r(val_isstringtype(string));
  int i = -1;
  unsigned int len = val_string_len(string);
  if (len) {
    i = _string_rfindi_notof(_val_string_str(string),len,reject,nreject);
  }
  val_string_destroy(string);
  val_int_init(string,i);
  return 0;
}


err_t val_string_lreserve(val_t *string, unsigned int n) {
  argcheck_r(val_isstringtype(string));
  struct val_string_buffer *buf = _val_string_buf(string);
  if (!buf) { //alloc new space at requested size
    string->val.string.offset = n;
    return _stringbuf_init(string,n);
  }
  const unsigned int len = val_string_len(string);
  const unsigned int off = _val_string_off(string);
  const unsigned int size = buf->size;
  if ( (buf->refcount==1) && (len+n<=size) ) { //move in-place
    if (!len) {
      string->val.string.offset = size;
      return 0;
    } else if (off>=n) {
      return 0; //already have space
    } else { //move mem without realloc (shift contents right)
      unsigned int newoff = _compute_exp_slide(off,len,size,n);
      memmove(buf->p+newoff,buf->p+off,len);
      string->val.string.offset = newoff;
      return 0;
    }
  } else {
    unsigned int lspace,rspace;
    _compute_exp_lreserve(off,len,size,n,STRING_INITIAL_SIZE,&lspace,&rspace);
    return _val_string_realloc(string,lspace,rspace);
  }
}
err_t val_string_rreserve(val_t *string, unsigned int n) {
  argcheck_r(val_isstringtype(string));
  struct val_string_buffer *buf = _val_string_buf(string);
  if (!buf) { //alloc new space at requested size
    string->val.string.offset = 0;
    return _stringbuf_init(string,n);
  }
  const unsigned int len = val_string_len(string);
  const unsigned int off = _val_string_off(string);
  const unsigned int size = buf->size;
  if ( (buf->refcount==1) && (len+n<=size) ) { //move in-place
    if (!len) {
      string->val.string.offset = 0;
      return 0;
    } else if ((size-len-off)>=n) {
      return 0; //already have space
    } else { //move mem without realloc (shift contents left)
      unsigned int newoff = size - len - _compute_exp_slide(size-len-off,len,size,n);
      memmove(buf->p+newoff,buf->p+off,len);
      string->val.string.offset = newoff;
      return 0;
    }
  } else {
    unsigned int rspace,lspace;
    //we flip lspace/rspace to make space on right
    _compute_exp_lreserve(size-len-off,len,size,n,STRING_INITIAL_SIZE,&rspace,&lspace);
    return _val_string_realloc(string,lspace,rspace);
  }
}
err_t val_string_lextend(val_t *string, unsigned int n, char **p) {
  err_t r;
  if ((r = val_string_lreserve(string,n))) return r;
  string->val.string.offset -= n;
  string->val.string.len += n;
  *p = _val_string_get(string,0);
  return 0;
}
err_t val_string_rextend(val_t *string, unsigned int n, char **p) {
  err_t r;
  if ((r = val_string_rreserve(string,n))) return r;
  *p = _val_string_get(string,string->val.string.len);
  string->val.string.len += n;
  return 0;
}


int val_string_any(val_t *string) {
  argcheck(val_isstringtype(string));
  if (val_string_empty(string)) return 0;
  const char *p = _val_string_str(string);
  unsigned int n = val_string_len(string);
  while(n) {
    if (p[--n]) return 1;
  }
  return 0;
}
int val_string_all(val_t *string) {
  argcheck(val_isstringtype(string));
  if (val_string_empty(string)) return 1;
  const char *p = _val_string_str(string);
  unsigned int n = val_string_len(string);
  while(n) {
    if (!p[--n]) return 0;
  }
  return 1;
}

//string comparison functions
int val_string_compare(val_t *lhs, val_t *rhs) {
  argcheck(val_isstringtype(lhs) && val_isstringtype(rhs));

  const char *l = _val_string_str(lhs);
  const char *r = _val_string_str(rhs);
  unsigned int nl = val_string_len(lhs);
  unsigned int nr = val_string_len(rhs);

  for(; nl && nr; ++l,++r,--nl,--nr) {
    if (*l<*r) return -1;
    else if (*r<*l) return 1;
  }
  if (nl<nr) return -1;
  else if (nr<nl) return 1;
  else return 0;
}
int val_string_lt(val_t *lhs, val_t *rhs) {
  argcheck(val_isstringtype(lhs) && val_isstringtype(rhs));
  const char *l = val_string_str(lhs);
  const char *r = val_string_str(rhs);
  unsigned int nl = val_string_len(lhs);
  unsigned int nr = val_string_len(rhs);

  for(; nl && nr; ++l,++r,--nl,--nr) {
    if (*l<*r) return 1;
    else if (*r<*l) return 0;
  }
  return nl<nr;
}
int val_string_eq(val_t *lhs, val_t *rhs) {
  argcheck(val_isstringtype(lhs) && val_isstringtype(rhs));
  const char *l = val_string_str(lhs);
  const char *r = val_string_str(rhs);
  unsigned int nl = val_string_len(lhs);
  unsigned int nr = val_string_len(rhs);

  for(; nl && nr; ++l,++r,--nl,--nr) {
    if (*l!=*r) return 0;
  }
  return nl==nr;
}

int val_string_strcmp(val_t *string, const char *cstr) {
  argcheck(val_isstringtype(string));
  unsigned int nl = val_string_len(string);
  if (!nl) return *cstr ? -1 : 0;
  return strncmp_cstr(_val_string_str(string),nl,cstr);
}

//FNV (Fowler-Noll-Vo) hash function
//Constants:
//  32bit - 2166136261,16777619
//  64bit - 14695981039346656037,1099511628211
uint32_t val_string_hash32(val_t *string) {
  argcheck(val_isstringtype(string));
  register uint32_t h;
  unsigned int n=val_string_len(string);
  const char *s = val_string_str(string);
  for (h = 2166136261u; n; --n,++s) {
    h = (h*16777619) ^ *s;
  }
  return h;
}
uint64_t val_string_hash64(val_t *string) {
  argcheck(val_isstringtype(string));
  register uint64_t h;
  unsigned int n=val_string_len(string);
  const char *s = val_string_str(string);
  for (h = 14695981039346656037u; n; --n,++s) {
    h = (h*1099511628211) ^ *s;
  }
  return h;
}



const char* val_string_str(val_t *string) {
  argcheck(val_isstringtype(string));
  struct val_string_buffer *buf = _val_string_buf(string);
  if (!buf) return NULL;
  else return buf->p + _val_string_off(string);
}
const char* val_string_get(val_t *string, unsigned int i) {
  argcheck(val_isstringtype(string));
  struct val_string_buffer *buf = _val_string_buf(string);
  if (!buf || i>=val_string_len(string)) return NULL;
  else return buf->p + _val_string_off(string)+i;
}
const char* val_string_rget(val_t *string, unsigned int i) {
  argcheck(val_isstringtype(string));
  struct val_string_buffer *buf = _val_string_buf(string);
  if (!buf || i>=val_string_len(string)) return NULL;
  else return buf->p + _val_string_off(string) + val_string_len(string) - 1 - i;
}
const char* val_string_cstr(val_t *string) { //appends '\0' before returning str
  argcheck(val_isstringtype(string));
  unsigned int off = _val_string_off(string), len = val_string_len(string);
  struct val_string_buffer *buf = _val_string_buf(string);
  if (buf && (off+len)<buf->size && buf->p[off+len] == '\0') { //null already there
    return buf->p+off;
  } else { //need to append null after string
    err_t r;
    if ((r = val_string_rreserve(string,1))) return NULL; //fatal???
    *_val_string_rget(string,-1) = '\0';
    return _val_string_str(string);
  }
}


//ident functions
void val_ident_init_handlers(struct type_handlers *h) {
  h->destroy = val_string_destroy;
  h->clone = val_string_clone;
  h->fprintf = val_ident_fprintf;
  h->sprintf = val_ident_sprintf;
}
void val_ident_to_string(val_t *ident) {
  ident->type = TYPE_STRING;
}

void val_ident_init(val_t *val) {
  val->type = TYPE_IDENT;
  val->val.string.offset = 0;
  val->val.string.len = 0;
  val->val.string.b = NULL;
  VM_DEBUG_VAL_INIT(val);
}
err_t val_ident_init_(val_t *val, const char *ident, unsigned int len) {
  val_ident_init(val);
  return _val_string_set(val,ident,len);
}
int val_ident_fprintf(val_t *ident, FILE *file, const fmt_t *fmt) {
  argcheck(val_isident(ident));
  switch(fmt->conversion) {
    case 's': case 'v': case 'V':
      return _val_string_fprintf(file,ident,fmt->precision);
    default:
      return _throw(ERR_BADTYPE);
  }
}
int val_ident_sprintf(val_t *ident, val_t *buf, const fmt_t *fmt) {
  argcheck(val_isstringtype(ident));
  switch(fmt->conversion) {
    case 's': case 'v': case 'V':
      return _val_string_sprintf(buf,ident,fmt->precision);
    default:
      return _throw(ERR_BADTYPE);
  }
}
err_t val_ident_escape(val_t *ident) {
  argcheck_r(val_isident(ident));
  struct val_string_buffer *buf = _val_string_buf(ident);
  if (!buf || !buf->size) {
    err_t r;
    if ((r = _val_string_realloc(ident,1,0))) return r;
  }
  ident->val.string.offset--;
  ident->val.string.len++;
  *_val_string_str(ident) = '\\';
  return 0;
}
err_t val_ident_unescape(val_t *ident) {
  argcheck_r(val_ident_escaped(ident));
  ident->val.string.offset++;
  ident->val.string.len--;
  return 0;
}
int val_ident_escaped(val_t *ident) {
  argcheck(val_isident(ident));
  return val_string_empty(ident) ? 0 : '\\' == *_val_string_str(ident);
}
unsigned int val_ident_escaped_levels(val_t *ident) {
  argcheck(val_isident(ident));
  if (val_string_empty(ident)) return 0;
  unsigned int len = val_string_len(ident);
  const char *p = _val_string_str(ident);
  unsigned int i;
  for(i = 0; i < len && p[i] == '\\'; ++i) ;
  return i;
}

//========internal functions=============


struct val_string_buffer* _val_string_buf(val_t *string) {
  debug_assert(val_isstringtype(string));
  return string->val.string.b;
}
unsigned int _val_string_off(val_t *string) {
  debug_assert(val_isstringtype(string));
  return string->val.string.offset;
}


//(unsafe) functions to get elements of string (no checks except debug asserts)
char* _val_string_str(val_t *string) {
  debug_assert(val_isstringtype(string));
  return string->val.string.b->p + string->val.string.offset;
}
char* _val_string_get(val_t *string, int i) {
  debug_assert(val_isstringtype(string));
  return _val_string_str(string) + i;
}
char* _val_string_rget(val_t *string, int i) {
  debug_assert(val_isstringtype(string));
  return _val_string_str(string) + val_string_len(string) - 1 - i;
}
char* _val_string_bget(val_t *string, int i) {
  debug_assert(val_isstringtype(string));
  return string->val.string.b->p + i;
}

//set contents of string (and deref if needed)
err_t _val_string_set(val_t *string, const char *s, unsigned int len) {
  val_string_clear(string);
  err_t r;
  if ((r = val_string_rreserve(string,len))) return r;
  memcpy(_val_string_str(string),s,len);
  string->val.string.len = len;
  return 0;
}

//alloc/free list buffer
err_t _stringbuf_init(val_t *string, unsigned int size) {
  throw_if(ERR_MALLOC,!(string->val.string.b = malloc(sizeof(struct val_string_buffer))));
  VM_DEBUG_STRBUF_INIT(string,size);
  struct val_string_buffer *buf = _val_string_buf(string);
  buf->size=size;
  buf->refcount=1;
  cleanup_throw_if(ERR_MALLOC,free(buf),!(buf->p = malloc(size)));
  return 0;
}
void _stringbuf_destroy(struct val_string_buffer *buf) {
  VM_DEBUG_STRBUF_DESTROY(buf);
  if (buf) {
    VM_DEBUG_STRBUF_FREE(buf);
    if (0 == refcount_dec(buf->refcount)) {
      free(buf->p);
      free(buf);
    }
  }
}
err_t _val_string_realloc(val_t *string, unsigned int lspace, unsigned int rspace) {
  argcheck_r(val_isstringtype(string));
  VM_DEBUG_STR_REALLOC(string,lspace,rspace);
  err_t r;
  struct val_string_buffer *buf = _val_string_buf(string);
  const unsigned int len = val_string_len(string);
  const unsigned int off = _val_string_off(string);
  string->val.list.offset = lspace;
  if (!len) {
    _stringbuf_destroy(buf);
    if ((r = _stringbuf_init(string,lspace+rspace))) return r;
    return 0;
  } else {
    if ((r = _stringbuf_init(string,lspace+len+rspace))) return r;
    memcpy(_val_string_str(string),buf->p+off,len);
    _stringbuf_destroy(buf);
    return 0;
  }
}


//internal string printing functions

int _val_string_fprintf(FILE *file, val_t *string, int precision) {
  argcheck_r(val_isstringtype(string));
  int len=val_string_len(string);
  if (precision>=0 && precision<len) len=precision;
  if (!len) return 0;
  else return val_fprint_(file,_val_string_str(string),len);
}
int _val_string_sprintf(val_t *buf, val_t *string, int precision) {
  argcheck_r((buf==NULL || val_isstringtype(buf)) && val_isstringtype(string));
  int len=val_string_len(string);
  if (precision>=0 && precision<len) len=precision;
  if (!buf) return len;
  else if (!len) return 0;
  else return val_sprint_(buf,_val_string_str(string),len);
}

int _val_string_fprintf_quoted(FILE *file, val_t *string, const fmt_t *fmt) {
  argcheck_r(val_isstringtype(string));
  int quotes = !(fmt->flags & PRINTF_F_SQUOTE);
  //int squotes = !(fmt->flags & PRINTF_F_ALT); //FIXME: support this
  unsigned int len = val_string_len(string);
  if (!len) return (quotes ? val_fprint_(file,"\"\"",2) : 0);

  //int trunc_tail = (fmt->flags & PRINTF_F_MINUS);
  err_t r,rlen = 0;
  const char *str = _val_string_str(string);

  if (fmt->precision < 0) {
    if (quotes) { if (0>(r = val_fprint_(file,"\"",1))) return r; rlen += r; }
    const char *special;
    while ((special = _string_find_dq_special(str,len))) { //need to escape special chars
      unsigned int toklen = special-str;
      if (toklen) {
        if (0>(r = val_fprint_(file,str,toklen))) return r; rlen += r;
      }
      char special_buf[4];
      unsigned int specialn = _string_escape_ch(*special,special_buf);
      if (0>(r = val_fprint_(file,special_buf,specialn))) return r; rlen += r;

      len -= toklen+1;
      str += toklen+1;
    }
    if (len) {
      if (0>(r = val_fprint_(file,str,len))) return r; rlen += r;
    }
    if (quotes) { if (0>(r = val_fprint_(file,"\"",1))) return r; rlen += r; }
    return rlen;
  } else {
    val_t tbuf;
    val_string_init(&tbuf);
    r = _val_string_sprintf_quoted(&tbuf,string,fmt);
    if (r>=0) r = val_fprintf_(&tbuf,file,fmt_v);
    val_destroy(&tbuf);
    return r;
  }
}

int _val_string_sprint_trunc(val_t *buf, const char *str, unsigned int len, unsigned int rlen, unsigned int rlim, int trunc_tail) {
  debug_assert(rlen <= rlim);
  err_t r;
  if (trunc_tail) {
    if (rlen+len+3 >= rlim) {
      if (buf) {
        if (rlen+3 > rlim) { //need to truncate what we already printed
          val_string_substring(buf,0,val_string_len(buf)-rlen+rlim-3); //cut it off at 3 chars before limit
        } else { //truncate in current token
          if ((r = val_string_cat_(buf,str,rlim-rlen-3))) return r;
        }
        if ((r = val_string_cat_(buf,"...",3))) return r;
      }
      return 1;
    } else {
      if (buf) return val_string_cat_(buf,str,len);
      else return 0;
    }
  } else {
    if (rlen+len+3 >= rlim) {
      if (buf) {
        if (rlen+3 > rlim) { //need to truncate what we already printed
          //FIXME: off-by-one???
          val_string_substring(buf,val_string_len(buf)-rlen+rlim-3,-1); //cut it off at 3 chars before limit
        } else { //truncate in current token
          if ((r = val_string_lcat_(buf,str+len-(rlim-rlen-3),rlim-rlen-3))) return r;
        }
        if ((r = val_string_lcat_(buf,"...",3))) return r;
      }
      return 1;
    } else {
      if (buf) return val_string_lcat_(buf,str,len);
      else return 0;
    }
  }
}

int _val_string_sprintf_quoted(val_t *buf, val_t *string, const fmt_t *fmt) {
  val_t tbuf; //used for truncated head printing
  argcheck_r((buf==NULL || val_isstringtype(buf)) && val_isstringtype(string));
  unsigned int quotes = !(fmt->flags & PRINTF_F_SQUOTE) ? 1 : 0;
  //int squotes = !(fmt->flags & PRINTF_F_ALT); //FIXME: support this
  if (val_string_empty(string)) return (quotes ? val_sprint_(buf,"\"\"",2) : 0);

  //int trunc_tail = (fmt->flags & PRINTF_F_MINUS);
  err_t r,rlen = 0;
  const char *str = _val_string_str(string);
  unsigned int len = val_string_len(string);
  if (quotes) { if (0>(r = val_sprint_(buf,"\"",1))) return r; rlen += r; }
  if (fmt->precision < 0) {
    const char *special;
    while ((special = _string_find_dq_special(str,len))) { //need to escape special chars
      unsigned int toklen = special-str;
      if (toklen) {
        if (0>(r = val_sprint_(buf,str,toklen))) return r; rlen += r;
      }
      char special_buf[4];
      unsigned int specialn = _string_escape_ch(*special,special_buf);
      if (0>(r = val_sprint_(buf,special_buf,specialn))) return r; rlen += r;

      len -= toklen+1;
      str += toklen+1;
    }
    if (len) {
      if (0>(r = val_sprint_(buf,str,len))) return r; rlen += r;
    }
  } else {
    unsigned int prec = fmt->precision;
    int trunc_tail = (fmt->flags & PRINTF_F_MINUS);
    if (prec >= 0 && prec < (quotes+3+quotes)) prec = quotes+3+quotes; //minimum is "..."
    const char *special;
    if (trunc_tail) {
      while ((special = _string_find_dq_special(str,len))) { //need to escape special chars
        unsigned int toklen = special-str;
        if (toklen) {
          if (0>(r = _val_string_sprint_trunc(buf,str,toklen,rlen+quotes,prec,trunc_tail))) return r;
          else if (r>0) { //truncation occured
            rlen = prec-quotes;
            len=0; break;
          } else {
            rlen += toklen;
          }
        }
        char special_buf[4];
        unsigned int specialn = _string_escape_ch(*special,special_buf);
        if (0>(r = _val_string_sprint_trunc(buf,special_buf,specialn,rlen+quotes,prec,trunc_tail))) return r;
        else if (r>0) { //truncation occured
          rlen = prec-quotes;
          len=0; break;
        } else {
          rlen += specialn;
        }

        len -= toklen+1;
        str += toklen+1;
      }
      if (len) {
        if (0>(r = _val_string_sprint_trunc(buf,str,len,rlen+quotes,prec,trunc_tail))) return r;
        else if (r>0) { //truncation occured
          rlen = prec-quotes;
        } else {
          rlen += len;
        }
      }
    } else { //trunc head, print from right end
      val_string_init(&tbuf);
      const char *tok = str+len-1;
      while ((special = _string_rfind_dq_special(str,len))) { //need to escape special chars
        unsigned int toklen = tok-special;
        tok = special+1;
        if (toklen) {
          if (0>(r = _val_string_sprint_trunc(&tbuf,tok,toklen,rlen+quotes,prec,trunc_tail))) goto bad_tbuf;
          else if (r>0) { //truncation occured
            rlen = prec-quotes;
            len=0; break;
          } else {
            rlen += toklen;
          }
        }
        tok = special-1; //for next iter
        char special_buf[4];
        unsigned int specialn = _string_escape_ch(*special,special_buf);
        if (0>(r = _val_string_sprint_trunc(&tbuf,special_buf,specialn,rlen+quotes,prec,trunc_tail))) goto bad_tbuf;
        else if (r>0) { //truncation occured
          rlen = prec-quotes;
          len=0; break;
        } else {
          rlen += specialn;
        }

        len -= toklen+1;
      }
      if (len) {
        if (0>(r = _val_string_sprint_trunc(&tbuf,str,len,rlen+quotes,prec,trunc_tail))) goto bad_tbuf;
        else if (r>0) { //truncation occured
          rlen = prec-quotes;
        } else {
          rlen += len;
        }
      }
      if ((r = val_string_cat(buf,&tbuf))) goto bad_tbuf;
    }
  }
  if (quotes) { if (0>(r = val_sprint_(buf,"\"",1))) return r; rlen += r; }
  return rlen;
bad_tbuf:
  val_destroy(&tbuf);
  return r;
}

const char* _string_find_dq_special(const char *str, unsigned int len) {
  for(;len;++str,--len) {
    if( *str<32 || *str == '"') return str;
  }
  return NULL;
}
const char* _string_rfind_dq_special(const char *str, unsigned int len) {
  for(str += len-1;len;--str,--len) {
    if( *str<32 || *str == '"') return str;
  }
  return NULL;
}

int _string_findi(const char *str,unsigned int len, const char *substr,unsigned int substrn) {
  const char *match = strnstrn(str,len, substr,substrn);
  if (match) return match-str;
  else return -1;
}
int _string_rfindi(const char *str,unsigned int len, const char *substr,unsigned int substrn) {
  const char *match = rstrnstrn(str,len, substr,substrn);
  if (match) return match-str;
  else return -1;
}
int _string_findi_whitespace(const char *str,unsigned int len) {
  unsigned int i;
  for(i=0;i<len;++i) {
    if (is_space(str[i])) return i;
  }
  return -1;
}
int _string_findi_notwhitespace(const char *str,unsigned int len) {
  unsigned int i;
  for(i=0;i<len;++i) {
    if (!is_space(str[i])) return i;
  }
  return -1;
}
int _string_rfindi_notwhitespace(const char *str,unsigned int len) {
  unsigned int i=-1;
  if (len) {
    for(i=len-1;i>=0;--i) {
      if (!is_space(str[i])) break;
    }
  }
  return i;
}
int _string_findi_of(const char *str,unsigned int len, const char *chars,unsigned int nchars) {
  unsigned int i;
  for(i=0;i<len;++i) {
    if (strnchr(chars,nchars,str[i])) return i;
  }
  return -1;
}
int _string_rfindi_of(const char *str,unsigned int len, const char *chars,unsigned int nchars) {
  unsigned int i=-1;
  if (len) {
    for(i=len-1;i>=0;--i) {
      if (strnchr(chars,nchars,str[i])) break;
    }
  }
  return i;
}
int _string_findi_notof(const char *str,unsigned int len, const char *chars,unsigned int nchars) {
  unsigned int i;
  for(i=0;i<len;++i) {
    if (!strnchr(chars,nchars,str[i])) return i;
  }
  return -1;
}
int _string_rfindi_notof(const char *str,unsigned int len, const char *chars,unsigned int nchars) {
  unsigned int i=-1;
  if (len) {
    for(i=len-1;i>=0;--i) {
      if (!strnchr(chars,nchars,str[i])) break;
    }
  }
  return i;
}
//backslash escape char for use in string
//  - buf must be at least 4 chars: "\\x00"
//  - for standard escapes (e.g. \n \r) the one letter escape is used
//  - for other chars it is escaped as hex using the format \x00
unsigned int _string_escape_ch(unsigned char c, char *buf) {
  buf[0] = '\\';
  switch(c) {
    case '"':  buf[1] = '\"'; break;
    case '\\': buf[1] = '\\'; break;
    case '\a': buf[1] = 'a'; break;
    case '\b': buf[1] = 'b'; break;
    //case '\c': buf[1] = 'c'; break;
    case '\e': buf[1] = 'e'; break;
    case '\f': buf[1] = 'f'; break;
    case '\n': buf[1] = 'n'; break;
    case '\r': buf[1] = 'r'; break;
    case '\t': buf[1] = 't'; break;
    case '\v': buf[1] = 'v'; break;
    default:
      buf[1] = 'x';
      buf[2] = ( (c>0x9f) ? 'a'+((c>>4)-10) : '0'+(c>>4) );
      buf[3] = ( ((c&0x0f)>9) ? 'a'+((c&0x0f)-10) : '0'+(c&0x0f) );
      return 4;
  }
  return 2;
}

