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

#include "val_string.h"
#include "val_string_internal.h"
#include "val_printf.h"
#include "vm_err.h"
#include "vm_debug.h"
#include "helpers.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <valgrind/helgrind.h>


inline int _val_str_empty(valstruct_t *str) { return str->v.str.len==0; }
inline int _val_str_small(valstruct_t *str) { return str->v.str.len<=1; }
inline char* _val_str_buf(valstruct_t *v) { return v->v.str.buf->p; }
inline char* _val_str_begin(valstruct_t *v) { return _val_str_buf(v) + v->v.str.off; }
inline char* _val_str_off(valstruct_t *v, int i) { return _val_str_begin(v)+i; }
inline char* _val_str_end(valstruct_t *v) { return _val_str_buf(v) + v->v.str.off + _val_str_len(v); }
inline unsigned int _val_str_len(valstruct_t *v) { return v->v.str.len; }
inline unsigned int _val_str_size(valstruct_t *v) { return v->v.str.buf->size; }


int _val_str_escaped(valstruct_t *str) { //whether char char of string is '\'
  return !_val_str_empty(str) && '\\' == *_val_str_begin(str);
}
unsigned int _val_str_escaped_levels(valstruct_t *str) { //how many leading '\' symbols are there
  if (_val_str_empty(str)) return 0;
  unsigned int len = _val_str_len(str);
  const char *p = _val_str_begin(str);
  unsigned int i;
  for(i = 0; i < len && p[i] == '\\'; ++i) ;
  return i;
}

void _val_str_unescape(valstruct_t *str) { //delete first char from string (ASSUMES non-empty)
  str->v.str.off++;
  str->v.str.len--;
}
err_t _val_str_escape(valstruct_t *str) { //insert '\\'
  return _val_str_rcat_ch(str,'\\');
}

err_t _val_str_simpleprint(valstruct_t *v) {
  if (_val_str_len(v)) printf("%.*s",_val_str_len(v),_val_str_begin(v));
  return 0;
}
int _val_str_fprint_quoted(valstruct_t *v, FILE *file,int precision) {
  int len = _val_str_len(v);
  if (precision >= 0 && len > precision) len = precision;
  int r=0;
  if (len) {
    r+=fprintf(file,"\"");
    //printf("\"%.*s\"",len,val_string_str(val));
    const char *s=_val_str_begin(v);
    unsigned int n=len;
    unsigned int i=0;
    unsigned int tok=0;
    for(i=0;i<n;++i) {
      switch(s[i]) {
       case '"': r+=fprintf(file,"%.*s\\\"",i-tok,s+tok); tok=i+1; break;
       case '\\': r+=fprintf(file,"%.*s\\\\",i-tok,s+tok); tok=i+1; break;
       case '\a': r+=fprintf(file,"%.*s\\a",i-tok,s+tok); tok=i+1; break;
       case '\b': r+=fprintf(file,"%.*s\\b",i-tok,s+tok); tok=i+1; break;
       //case '\c': r+=fprintf(file,"%.*s\\c",i-tok,s+tok); tok=i+1; break;
       case '\e': r+=fprintf(file,"%.*s\\e",i-tok,s+tok); tok=i+1; break;
       case '\f': r+=fprintf(file,"%.*s\\f",i-tok,s+tok); tok=i+1; break;
       case '\n': r+=fprintf(file,"%.*s\\n",i-tok,s+tok); tok=i+1; break;
       case '\r': r+=fprintf(file,"%.*s\\r",i-tok,s+tok); tok=i+1; break;
       case '\t': r+=fprintf(file,"%.*s\\t",i-tok,s+tok); tok=i+1; break;
       case '\v': r+=fprintf(file,"%.*s\\v",i-tok,s+tok); tok=i+1; break;
       default:
         if (s[i] < 32) {
           unsigned char c = s[i];
           r+=fprintf(file,"%.*s\\x%c%c",i-tok,s+tok,( (c>0x9f) ? 'a'+((c>>4) - 10) : '0'+(c>>4)),( ((c&0x0f)>9) ? 'a'+((c&0x0f) - 10) : '0'+(c&0x0f)));
           tok=i+1;
         } else {
           //r+=fprintf(file,"%c",*s); break;
           break; //accumulate
         }
      }
    }
    r+=fprintf(file,"%.*s\"",i-tok,s+tok);
  } else {
    r+=fprintf(file,"\"\"");
  }
  return r;
}
int _val_str_sprint_quoted(valstruct_t *v, valstruct_t *buf, int precision) {
  int len = _val_str_len(v);
  if (precision >= 0 && len > precision) len = precision;
  int r=0;
  int rlen=0;
  if (len) {
    if (buf && (r = _val_str_cat_ch(buf,'"'))) return r;
    else rlen+=1;
    //printf("\"%.*s\"",strlen,val_string_str(val));
    const char *s=_val_str_begin(v);
    unsigned int n=len;
    unsigned int i=0;
    unsigned int tok=0;
    const char *escape;
    for(i=0;i<n;++i) {
      escape=NULL;
      switch(s[i]) {
       case '"': escape = "\""; break;
       case '\\': escape = "\\\\"; break;
       case '\a': escape = "\\a"; break;
       case '\b': escape = "\\b"; break;
       //case '\c': escape = "\\c"; break;
       case '\e': escape = "\\e"; break;
       case '\f': escape = "\\f"; break;
       case '\n': escape = "\\n"; break;
       case '\r': escape = "\\r"; break;
       case '\t': escape = "\\t"; break;
       case '\v': escape = "\\v"; break;
       default:
         if (s[i] < 32) {
           if (i>tok) {
             if (buf && (r = _val_str_cat_cstr(buf,_val_str_begin(v)+tok,i-tok))) return r;
             rlen+=i-tok;
           }
           if (buf) {
             char b[3];
             unsigned char c = s[i];
             b[0] = 'x';
             b[1] = ( (c>0x9f) ? 'a'+((c>>4) - 10) : '0'+(c>>4));
             b[2] = ( ((c&0x0f)>9) ? 'a'+((c&0x0f) - 10) : '0'+(c&0x0f));
             if ((r = _val_str_cat_cstr(v,b,3))) return r;
           }
           rlen+=3;
           tok=i+1;
         } else {
           //r+=fprintf(file,"%c",*s); break;
           break; //accumulate
         }
      }
      if (escape) {
        if (i>tok) {
          if (buf && (r = _val_str_cat_cstr(buf,_val_str_begin(v)+tok,i-tok))) return r;
          rlen+=i-tok;
        }
        unsigned int elen = strlen(escape);
        if (buf && (r = _val_str_cat_cstr(buf,escape,elen))) return r;
        rlen += elen;
        tok=i+1;
      }
    }
    if (i>tok) {
      if (buf && (r = _val_str_cat_cstr(buf,_val_str_begin(v)+tok,i-tok))) return r;
      rlen+=i-tok;
    }
    if (buf && (r = _val_str_cat_ch(buf,'"'))) return r;
    rlen+=1;
  } else {
    if (buf && (r = _val_str_cat_cstr(buf,"\"\"",2))) return r;
    rlen+=2;
  }
  return rlen;
}

val_t _strval_alloc(enum val_type type) {
  valstruct_t *t = _valstruct_alloc();
  t->type = type;
  t->v.str.buf=NULL;
  t->v.str.off=0;
  t->v.str.len=0;
  return __str_val(t);
}

err_t _strval_init(val_t *v, enum val_type type) {
  valstruct_t *t;
  if (!(t = _valstruct_alloc())) return _fatal(ERR_MALLOC);
  t->type = type;
  t->v.str.buf=NULL;
  t->v.str.off=0;
  t->v.str.len=0;
  *v = __str_val(t);
  return 0;
}

err_t val_string_init_cstr(val_t *val, const char *str, unsigned int n) {
  valstruct_t *t;
  if (!(t = _valstruct_alloc())) return _fatal(ERR_MALLOC);
  t->type = TYPE_STRING;
  if (!(t->v.str.buf=_sbuf_alloc(n))) _fatal(ERR_MALLOC);
  t->v.str.off=0;
  t->v.str.len=n;
  memcpy(_val_str_buf(t),str,n);
  *val = __str_val(t);
  VM_DEBUG_VAL_INIT(val);
  return 0;
}

err_t val_ident_init_cstr(val_t *val, const char *str, unsigned int n) {
  valstruct_t *t;
  if (!(t = _valstruct_alloc())) return -1;
  t->type = TYPE_IDENT;
  if (!(t->v.str.buf=_sbuf_alloc(n))) return -1;
  t->v.str.off=0;
  t->v.str.len=n;
  memcpy(_val_str_buf(t),str,n);
  *val = __str_val(t);
  VM_DEBUG_VAL_INIT(val);
  return 0;
}

err_t val_string_init_quoted(val_t *val, const char *str, unsigned int len) {
  //we only validate the leading quote, and assume the trailing one matches (since it's been through parser)
  if (*str == '\'') {
    return val_string_init_cstr(val,str+1,len-2);
  } else if (*str != '"') {
    return _throw(ERR_BADARGS);
  } else {
    str++; len-=2; //skip first/last char
    *val = val_empty_string();
    if (!len) return 0;
    err_t e;
    if ((e = _val_str_rreserve(__str_ptr(*val),len))) goto out_val;
    char *s = _val_str_begin(__str_ptr(*val));

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
            //NOTE: we assume string has already been validated by jstok
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
            //goto out_badescape; //invalid escape
        }
      } else {
        s[sn++]=*str;
      }
    }
    __str_ptr(*val)->v.str.len=sn;
    return 0;
out_badescape:
    val_destroy(*val);
    return _throw(ERR_BADESCAPE);
out_val:
    val_destroy(*val);
    return e;
  }
}


val_t val_string_temp_cstr(const char *str, unsigned int n) {
  val_t v;
  int r;
  if ((r = val_string_init_cstr(&v,str,n))) return VAL_NULL;
  return v;
}

sbuf_t* _sbuf_alloc(unsigned int size) {
  sbuf_t *p;
  if (!(p = malloc(sizeof(sbuf_t)+size))) return NULL;
  p->size=size;
  p->refcount=1;
  VM_DEBUG_STRBUF_INIT(p,size);
  return p;
}

inline void _sbuf_release(sbuf_t* buf) {
  VM_DEBUG_STRBUF_DESTROY(buf);
  if (0 == (refcount_dec(buf->refcount))) {
    ANNOTATE_HAPPENS_AFTER(buf);
    ANNOTATE_HAPPENS_BEFORE_FORGET_ALL(buf);
    free(buf);
  } else {
    ANNOTATE_HAPPENS_BEFORE(buf);
  }
}

void _val_str_clone(valstruct_t *ret, valstruct_t *str) {
  *ret = *str;
  if (ret->v.str.buf) refcount_inc(ret->v.str.buf->refcount);
}

void _val_str_destroy(valstruct_t *str) {
  if (str->v.str.buf) _sbuf_release(str->v.str.buf);
  _valstruct_release(str);
}
void _val_str_destroy_(valstruct_t *str) {
  if (str->v.str.buf) _sbuf_release(str->v.str.buf);
}

val_t val_empty_string() { return _strval_alloc(TYPE_STRING); }
val_t val_empty_ident() { return _strval_alloc(TYPE_IDENT); }

err_t val_string_init_empty(val_t *v) { return _strval_init(v,TYPE_STRING); }
err_t val_ident_init_empty(val_t *v) { return _strval_init(v,TYPE_IDENT); }

#define _str_lspace(val) ((val)->v.str.off)
#define _str_rspace(val) ((val)->v.str.buf->size - (val)->v.str.len - (val)->v.str.off)
#define _str_lrspace(val) ((val)->v.str.buf->size - (val)->v.str.len)
#define _str_mutable(val) ((val)->v.str.buf->refcount==1)

err_t _val_str_realloc(valstruct_t *v, unsigned int left, unsigned int right) {
  sbuf_t *newbuf;
  if (!(newbuf = _sbuf_alloc(left + _val_str_len(v) + right))) return _fatal(ERR_MALLOC);
  memcpy((char*)newbuf->p + left, _val_str_begin(v), _val_str_len(v));
  _sbuf_release(v->v.str.buf);
  v->v.str.buf = newbuf;
  v->v.str.off = left;
  return 0;
}

void _val_str_slide(valstruct_t *v, unsigned int newoff) {
  memmove(_val_str_buf(v)+newoff,_val_str_begin(v),_val_str_len(v));
  v->v.str.off = newoff;
}

err_t _val_str_lreserve(valstruct_t *v, unsigned int n) {
  if (!v->v.str.buf) {
    if (!(v->v.str.buf = _sbuf_alloc(n))) return _fatal(ERR_MALLOC);
    v->v.str.off = n;
  } else if (_str_mutable(v) && _str_lrspace(v)>=n) { //unique ref and have space
    if(_str_lspace(v)>=n) { //already have space
      return 0;
    } else { //have space with shuffling
      _val_str_slide(v,n);
    }
  } else {
    return _val_str_realloc(v,n,_str_rspace(v));
  }
  return 0;
}

err_t _val_str_rreserve(valstruct_t *v, unsigned int n) {
  if (!v->v.str.buf) {
    if (!(v->v.str.buf = _sbuf_alloc(n))) return _fatal(ERR_MALLOC);
    v->v.str.off = 0;
  } else if (_str_mutable(v) && _str_lrspace(v)>=n) {
    if(_str_rspace(v)>=n) { //already have space
      return 0;
    } else { //have space with shuffling
      _val_str_slide(v,_val_str_size(v) - _val_str_len(v) - n);
    }
  } else {
    return _val_str_realloc(v,_str_lspace(v),n);
  }
  return 0;
}

err_t _val_str_lextend(valstruct_t *v, unsigned int n, char **p) {
  err_t e;
  if ((e = _val_str_lreserve(v,n))) return e;
  v->v.str.off -= n;
  v->v.str.len += n;
  *p = _val_str_begin(v);
  return 0;
}
err_t _val_str_rextend(valstruct_t *v, unsigned int n, char **p) {
  err_t e;
  if ((e = _val_str_rreserve(v,n))) return e;
  *p = _val_str_end(v);
  v->v.str.len += n;
  return 0;
}

err_t _val_str_cat(valstruct_t *str, valstruct_t *suffix) {
  unsigned int n = _val_str_len(suffix);
  if (!n) {
    _val_str_destroy_(suffix);
  } else if (_val_str_empty(str)) {
    _val_str_destroy_(str);
    str->v.str = suffix->v.str;
  } else {
    err_t e;
    //if str has space we pick str to append to, else if suffix has space we prepend to suffix, else we extend and append to str
    if ((_str_mutable(str) && _str_lrspace(str)>=n) || !(_str_mutable(suffix) && _str_lrspace(suffix)>=n)) {
      if ((e = _val_str_rreserve(str,n))) return e;
      memcpy(_val_str_end(str), _val_str_begin(suffix), n);
      str->v.str.len+=n;
      _sbuf_release(suffix->v.str.buf);
    } else {
      if ((e = _val_str_lreserve(suffix,n))) return e;
      memcpy(_val_str_begin(suffix)-_val_str_len(str), _val_str_begin(str), _val_str_len(str));
      _sbuf_release(str->v.str.buf);
      str->v.str.off=suffix->v.str.off-_val_str_len(str);
      str->v.str.len+=n;
      str->v.str.buf = suffix->v.str.buf;
    }
  }
  _valstruct_release(suffix);
  return 0;
}

err_t _val_str_rcat(valstruct_t *str, valstruct_t *prefix) {
  unsigned int n = _val_str_len(prefix);
  if (!n) {
      _val_str_destroy_(prefix);
  } else if (_val_str_empty(str)) {
    _val_str_destroy_(str);
    str->v.str = prefix->v.str;
  } else {
    err_t e;
    //if str has space we pick str to append to, else if suffix has space we prepend to suffix, else we extend and append to str
    if ((_str_mutable(str) && _str_lrspace(str)>=n) || !(_str_mutable(prefix) && _str_lrspace(prefix)>=n)) {
      if ((e = _val_str_lreserve(str,n))) return e;
      str->v.str.off-=n;
      str->v.str.len+=n;
      memcpy(_val_str_begin(str), _val_str_begin(prefix), n);
      _sbuf_release(prefix->v.str.buf);
    } else {
      if ((e = _val_str_rreserve(prefix,n))) return e;
      memcpy(_val_str_end(prefix), _val_str_begin(str), _val_str_len(str));
      _sbuf_release(str->v.str.buf);
      str->v.str.buf = prefix->v.str.buf;
      str->v.str.off=prefix->v.str.off-_val_str_len(str);
      str->v.str.len+=n;
    }
  }
  _valstruct_release(prefix);
  return 0;
}

err_t _val_str_cat_cstr(valstruct_t *str, const char *s, unsigned int len) {
  err_t e;
  if ((e = _val_str_rreserve(str,len))) return e;
  memcpy(_val_str_end(str), s, len);
  str->v.str.len+=len;
  return 0;
}
err_t _val_str_rcat_cstr(valstruct_t *str, const char *s, unsigned int len) {
  err_t e;
  char *p;
  if ((e = _val_str_lextend(str,len,&p))) return e;
  memcpy(p, s, len);
  return 0;
}

err_t _val_str_cat_ch(valstruct_t *str, char c) {
  err_t e;
  if ((e = _val_str_rreserve(str,1))) return e;
  *_val_str_end(str) = c;
  str->v.str.len++;
  return 0;
}
err_t _val_str_rcat_ch(valstruct_t *str, char c) {
  err_t e;
  if ((e = _val_str_lreserve(str,1))) return e;
  str->v.str.off--;
  str->v.str.len++;
  *_val_str_begin(str) = c;
  return 0;
}

err_t _val_str_cat_copy(valstruct_t *str, valstruct_t *suffix) {
  if (suffix->v.str.buf) return _val_str_cat_cstr(str,_val_str_begin(suffix),_val_str_len(suffix));
  else return 0;
}

err_t _val_str_rcat_copy(valstruct_t *str, valstruct_t *prefix) {
  if (prefix->v.str.buf) return _val_str_rcat_cstr(str,_val_str_begin(prefix),_val_str_len(prefix));
  else return 0;
}

err_t _val_str_make_cstr(valstruct_t *str) {
  //don't need to do anything if we already have a proper cstr
  if (!str->v.str.buf || _str_rspace(str)<1 || *_val_str_end(str) != '\0') {
    err_t e;
    if ((e = _val_str_rreserve(str,1))) return e;
    *_val_str_end(str) = '\0';
  }
  return 0;
}


#undef _str_lspace
#undef _str_rspace
#undef _str_lrspace
#undef _str_mutable

void _val_str_clear(valstruct_t *str) {
  str->v.str.len = 0;
}
void _val_str_substr(valstruct_t *str, unsigned int off, unsigned int len) {
  unsigned int strlen=_val_str_len(str);
  if (off>strlen) off=strlen;
  strlen-=off;
  if (len>strlen) len=strlen;
  str->v.str.off += off;
  str->v.str.len = len;
}
void _val_str_trim(valstruct_t *str) {
  unsigned int n = _val_str_len(str);
  if (n) {
    const char *p = _val_str_begin(str);
    int i;

    //trim right (find last non-space)
    i = _string_rfindi_notwhitespace(p,n);
    if (i>=0) {
      str->v.str.len = i+1;
      n = i+1;
    }

    //trim left (find first non-space)
    i = _string_findi_notwhitespace(p,n);
    if (i>=0) {
      str->v.str.off += i;
      str->v.str.len -= i;
    } else {
      str->v.str.len = 0;
    }
  }
}

err_t _val_str_splitn(valstruct_t *str, val_t *rhs, unsigned int off) {
  if (off>_val_str_len(str)) return _throw(ERR_BADARGS);
  valstruct_t *ret;
  if (!(ret = _valstruct_alloc())) return _fatal(ERR_MALLOC);
  *ret = *str;
  if (ret->v.str.buf) {
    refcount_inc(ret->v.str.buf->refcount);
    str->v.str.len=off;
    ret->v.str.len-=off;
    ret->v.str.off+=off;
  } //else both sides are of length zero anyways so there is nothing to do
  *rhs = __str_val(ret);
  return 0;
}

err_t _val_str_substr_clone(val_t *ret, valstruct_t *str, unsigned int off, unsigned int len) {
  valstruct_t *v;
  if (!(v = _valstruct_alloc())) return _fatal(ERR_MALLOC);
  *v = *str;
  if (v->v.str.buf) {
    refcount_inc(v->v.str.buf->refcount);
    v->v.str.off+=off;
    v->v.str.len=len;
  } //else both sides are of length zero anyways so there is nothing to do
  *ret = __str_val(v);
  return 0;
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
  unsigned int i;
  if (len) {
    for(i=len;i>0;--i) {
      if (!is_space(str[i-1])) return i-1;
    }
  }
  return -1;
}
int _string_findi_of(const char *str,unsigned int len, const char *chars,unsigned int nchars) {
  unsigned int i;
  for(i=0;i<len;++i) {
    if (strnchr(chars,nchars,str[i])) return i;
  }
  return -1;
}
int _string_rfindi_of(const char *str,unsigned int len, const char *chars,unsigned int nchars) {
  unsigned int i;
  for(i=len;i>0;--i) {
    if (strnchr(chars,nchars,str[i-1])) return i-1;
  }
  return -1;
}
int _string_findi_notof(const char *str,unsigned int len, const char *chars,unsigned int nchars) {
  unsigned int i;
  for(i=0;i<len;++i) {
    if (!strnchr(chars,nchars,str[i])) return i;
  }
  return -1;
}
int _string_rfindi_notof(const char *str,unsigned int len, const char *chars,unsigned int nchars) {
  unsigned int i;
  for(i=len;i>0;--i) {
    if (!strnchr(chars,nchars,str[i-1])) return i-1;
  }
  return -1;
}

int _val_str_compare(valstruct_t *lhs, valstruct_t *rhs) {
  const char *l = _val_str_begin(lhs);
  const char *r = _val_str_begin(rhs);
  unsigned int nl = _val_str_len(lhs);
  unsigned int nr = _val_str_len(rhs);

  for(; nl && nr; ++l,++r,--nl,--nr) {
    if (*l<*r) return -1;
    else if (*r<*l) return 1;
  }
  if (nl<nr) return -1;
  else if (nr<nl) return 1;
  else return 0;
}
int _val_str_lt(valstruct_t *lhs, valstruct_t *rhs) {
  const char *l = _val_str_begin(lhs);
  const char *r = _val_str_begin(rhs);
  unsigned int nl = _val_str_len(lhs);
  unsigned int nr = _val_str_len(rhs);

  for(; nl && nr; ++l,++r,--nl,--nr) {
    if (*l<*r) return 1;
    else if (*r<*l) return 0;
  }
  return nl<nr;
}
int _val_str_eq(valstruct_t *lhs, valstruct_t *rhs) {
  const char *l = _val_str_begin(lhs);
  const char *r = _val_str_begin(rhs);
  unsigned int nl = _val_str_len(lhs);
  unsigned int nr = _val_str_len(rhs);

  for(; nl && nr; ++l,++r,--nl,--nr) {
    if (*l!=*r) return 0;
  }
  return nl==nr;
}

int _val_str_cstr_compare(valstruct_t *str, const char *cstr,unsigned int len) {
  const char *l = _val_str_begin(str);
  const char *r = cstr;
  unsigned int nl = _val_str_len(str);
  unsigned int nr = len;

  for(; nl && nr; ++l,++r,--nl,--nr) {
    if (*l<*r) return -1;
    else if (*r<*l) return 1;
  }
  if (nl<nr) return -1;
  else if (nr<nl) return 1;
  else return 0;
}
int _val_str_strcmp(valstruct_t *str, const char *cstr) {
  return _val_str_cstr_compare(str,cstr,strlen(cstr));
}

int _val_str_find(valstruct_t *str, const char *substr, unsigned int sslen) {
  unsigned int slen = _val_str_len(str);
  if (sslen == 0) return 0;
  else if (slen == 0) return -1;
  else return _string_findi(_val_str_begin(str),slen,substr,sslen);
}
int _val_str_findstr(valstruct_t *str, valstruct_t *substr) {
  unsigned int slen = _val_str_len(str);
  unsigned int sslen = _val_str_len(substr);
  if (sslen == 0) return 0;
  else if (slen == 0) return -1;
  else return _string_findi(_val_str_begin(str),slen,_val_str_begin(substr),sslen);
}


err_t _val_str_padleft(valstruct_t *str,char c, int n) { //pad left with n chars of c
  //argcheck_r(val_is_str(string));
  if (!n) return 0;
  err_t e;
  char *p;
  if ((e = _val_str_lextend(str,n,&p))) return e;
  while(n--) *(p++) = c;
  return 0;
}
err_t _val_str_padright(valstruct_t *str,char c, int n) { //pad right with n chars of c
  //argcheck_r(val_is_str(string));
  if (!n) return 0;
  err_t e;
  char *p;
  if ((e = _val_str_rextend(str,n,&p))) return e;
  while(n--) *(p++) = c;
  return 0;
}


//FNV (Fowler-Noll-Vo) hash function
//Constants:
//  32bit - 2166136261,16777619
//  64bit - 14695981039346656037,1099511628211
uint32_t _val_str_hash32(valstruct_t *str) {
  register uint32_t h = 2166136261u;
  if (_val_str_empty(str)) return h;
  const char *s = _val_str_begin(str);
  unsigned int n;
  for (n=_val_str_len(str); n; --n,++s) {
    h = (h*16777619) ^ *s;
  }
  return h;
}
uint64_t _val_str_hash64(valstruct_t *str) {
  register uint64_t h = 14695981039346656037u;
  if (_val_str_empty(str)) return h;
  const char *s = _val_str_begin(str);
  unsigned int n;
  for (n=_val_str_len(str); n; --n,++s) {
    h = (h*1099511628211) ^ *s;
  }
  return h;
}

uint32_t _val_cstr_hash32(const char *s, unsigned int n) {
  register uint32_t h = 2166136261u;
  for (; n; --n,++s) {
    h = (h*16777619) ^ *s;
  }
  return h;
}

uint64_t _val_cstr_hash64(const char *s, unsigned int n) {
  register uint64_t h = 14695981039346656037u;
  for (; n; --n,++s) {
    h = (h*1099511628211) ^ *s;
  }
  return h;
}

int val_string_fprintf(valstruct_t *v,FILE *file, const struct printf_fmt *fmt) {
  int len;
  switch(fmt->conversion) {
    case 's': case 'v':
      len=_val_str_len(v);
      if (fmt->precision>=0 && fmt->precision<len) len=fmt->precision;
      return val_fprint_(file,_val_str_begin(v),len);
    case 'V':
      return _val_str_fprint_quoted(v,file,fmt->precision);
    default:
      return _throw(ERR_BADTYPE);
  }
}

int val_string_sprintf(valstruct_t *v,valstruct_t *buf, const struct printf_fmt *fmt) {
  err_t e;
  switch(fmt->conversion) {
    case 's': case 'v':
      if (buf) {
        int len = _val_str_len(v);
        if (fmt->precision>=0 && fmt->precision < len) len=fmt->precision;
        if ((e = _val_str_cat_cstr(buf,_val_str_begin(v), len))) return e;
        return len;
      } else {
        int len = _val_str_len(v);
        if (fmt->precision>=0 && fmt->precision < len) {
          return fmt->precision;
        } else {
          return len;
        }
      }
    case 'V':
      return _val_str_sprint_quoted(v,buf,fmt->precision);
    default:
      return _throw(ERR_BADTYPE);
  }
}

int val_ident_fprintf(valstruct_t *v,FILE *file, const struct printf_fmt *fmt) {
  switch(fmt->conversion) {
    case 's': case 'v':
    case 'V':
      if (!_val_str_empty(v)) {
        return (0>fprintf(file,"%.*s",_val_str_len(v),_val_str_begin(v)) ? _throw(ERR_IO_ERROR) : 0);
      } else {
        return 0;
      }
    default:
      return _throw(ERR_BADTYPE);
  }
}

int val_ident_sprintf(valstruct_t *v,valstruct_t *buf, const struct printf_fmt *fmt) {
  switch(fmt->conversion) {
    case 's': case 'v':
    case 'V':
      if (buf) {
        err_t e;
        if ((e = _val_str_cat_copy(buf,v))) return e;
      }
      return _val_str_len(v);
    default:
      return _throw(ERR_BADTYPE);
  }
}
