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

#include "val_list_internal.h"
#include "val_cast.h"
#include "val_compare.h"
#include "val_sort.h"
#include "val_string.h"
#include "val_alloc.h"
#include "vm_err.h"
#include "vm_debug.h"
#include "val_printf.h"
#include "helpers.h"
#include "stdlib.h"
#include "string.h"

const list_fmt_t _list_fmt_V = { //standard list format
  .reverse = 0,
  .max_bytes = -1,
  .max_els = -1,
  .trunc_tail = 0,
  .sep = " ", .seplen = 1,
  .braces = 1,
  .bracesep = " ", .braceseplen = 1,
  .trunc = "...", .trunclen = 3,
  .sublist_fmt = NULL
};
const list_fmt_t _list_fmt_lines = { //print 1 el per line (e.g. listing stack)
  .reverse = 0,
  .max_bytes = -1,
  .max_els = -1,
  .trunc_tail = 0,
  .sep = "\n", .seplen = 1,
  .braces = 0,
  .bracesep = NULL, .braceseplen = 0,
  .trunc = "...", .trunclen = 3,
  .sublist_fmt = NULL
};
const list_fmt_t _list_fmt_rlines = { //print 1 el per line in reverse order (e.g. listing work stack)
  .reverse = 1,
  .max_bytes = -1,
  .max_els = -1,
  .trunc_tail = 0,
  .sep = "\n", .seplen = 1,
  .braces = 0,
  .bracesep = NULL, .braceseplen = 0,
  .trunc = "...", .trunclen = 3,
  .sublist_fmt = NULL
};
const list_fmt_t _list_fmt_join = { //join (cat) list of strings
  .reverse = 0,
  .max_bytes = -1,
  .max_els = -1,
  .trunc_tail = 1,
  .sep = NULL, .seplen = 0,
  .braces = 0,
  .bracesep = NULL, .braceseplen = 0,
  .trunc = NULL, .trunclen = 0,
  .sublist_fmt = NULL
};
const list_fmt_t *list_fmt_V = &_list_fmt_V;
const list_fmt_t *list_fmt_lines = &_list_fmt_lines;
const list_fmt_t *list_fmt_rlines = &_list_fmt_rlines;
const list_fmt_t *list_fmt_join = &_list_fmt_join;

//generate list format from printf format
//  - internal list printing functions use list format (more flexible than just printf format)
//  - this function generates list format from printf format for standard printing options
//  - additional use cases are string join and generating tabular (e.g. csv/tsv) output
//Flags:
//  PRINTF_F_ALT    - print els in reverse order (i.e. order stack is processed in)
//  PRINTF_F_MINUS  - left justification (also controls truncated side)
//  PRINTF_F_SQUOTE - don't print open/close (just contents)
//  PRINTF_F_SPACE  - use newline as sep (instead of space)
err_t val_list_format_fmt(list_fmt_t *lfmt, const fmt_t *fmt) {
  if (fmt->conversion == 'V') {
    *lfmt = _list_fmt_V;
    lfmt->reverse = (fmt->flags & PRINTF_F_ALT) ? 1 : 0;
    lfmt->trunc_tail = (fmt->flags & PRINTF_F_MINUS);

    if (fmt->flags & PRINTF_F_SPACE) { //newline sep
      lfmt->sep = "\n";
      lfmt->max_els = fmt->precision;
    } else { //space sep (default)
      lfmt->max_bytes = fmt->precision;
    }

    if (fmt->flags & PRINTF_F_SQUOTE) { //suppress braces
      lfmt->braces = 0;
      lfmt->bracesep = NULL;
      lfmt->braceseplen = 0;
    } else { //print braces (default)
      lfmt->bracesep = lfmt->sep; //use same bracesep as sep
      lfmt->braceseplen = lfmt->seplen;
    }
  } else if (fmt->conversion == 'v') {
    *lfmt = _list_fmt_join;
  } else return _throw(ERR_BADTYPE);

  return 0;
}

//generate standard listing of tail of stack
void val_list_format_listn(list_fmt_t *lfmt, int n) {
  *lfmt = _list_fmt_lines;
  lfmt->max_els = n,
  lfmt->trunc = NULL;
  lfmt->trunclen = 0;
}

//generate list format to concatenate all elements (separated by sep)
void val_list_format_join2(list_fmt_t *lfmt, const char *sep, unsigned int seplen) {
  *lfmt = _list_fmt_join;
  lfmt->sep = sep;
  lfmt->seplen = seplen;
}



void val_list_init_handlers(struct type_handlers *h) {
  h->destroy = val_list_destroy;
  h->clone = val_list_clone;
  h->fprintf = val_list_fprintf;
  h->sprintf = val_list_sprintf;
}

void val_list_init(val_t *val) {
  val->type = TYPE_LIST;
  val->val.list.b = NULL;
  val->val.list.offset = 0;
  val->val.list.len = 0;
  VM_DEBUG_VAL_INIT(val);
}

err_t val_list_destroy(val_t *list) {
  argcheck_r(val_islisttype(list));
  VM_DEBUG_VAL_DESTROY(list);
  list->type = VAL_INVALID;
  _listbuf_destroy(list->val.list.b);
  return 0;
}
err_t val_list_clone(val_t *val, val_t *orig) {
  argcheck_r(val_islisttype(orig));
  VM_DEBUG_VAL_CLONE(val,orig);
  *val = *orig;
  if (_val_list_buf(val)) refcount_inc(val->val.list.b->refcount);
  return 0;
}

int val_list_fprintf(val_t *list, FILE *file, const fmt_t *fmt) {
  argcheck_r(val_islisttype(list));

  err_t r;
  list_fmt_t lfmt;
  if ((r = val_list_format_fmt(&lfmt,fmt))) return r;
  const fmt_t *el_fmt = (fmt->conversion == 'v' ? fmt_v : fmt_V);

  return val_list_fprintf_(list,file,&lfmt,el_fmt);
}
err_t val_list_sprintf(val_t *list, val_t *buf, const fmt_t *fmt) {
  argcheck_r(val_islisttype(list));

  err_t r;
  list_fmt_t lfmt;
  if ((r = val_list_format_fmt(&lfmt,fmt))) return r;
  const fmt_t *el_fmt = (fmt->conversion == 'v' ? fmt_v : fmt_V);

  return val_list_sprintf_(list,buf,&lfmt,el_fmt);
}

int val_list_fprintf_(val_t *list, FILE *file, const list_fmt_t *lfmt, const fmt_t *el_fmt) {
  argcheck_r(val_islisttype(list));
  const char *open,*close;
  val_list_braces(list,&open,&close);
  return _val_list_fprintf(file,val_list_head(list),val_list_len(list),open,close,lfmt,el_fmt);
}
int val_list_sprintf_(val_t *list, val_t *buf, const list_fmt_t *lfmt, const fmt_t *el_fmt) {
  argcheck_r(val_islisttype(list));
  const char *open,*close;
  val_list_braces(list,&open,&close);
  return _val_list_sprintf(buf,val_list_head(list),val_list_len(list),open,close,lfmt,el_fmt);
}

int val_list_fprintf_open_(val_t *list, FILE *file, const list_fmt_t *lfmt, const fmt_t *el_fmt, unsigned int levels) {
  argcheck_r(val_islisttype(list));
  val_t buf;
  val_string_init(&buf);
  err_t r;
  if (0>(r = val_list_sprintf_open_(list,&buf,lfmt,el_fmt,levels))) goto bad;
  if (0>(r = val_fprintf_(&buf,file,fmt_v))) goto bad;
bad:
  val_destroy(&buf);
  return r;
}

//print list without close if levels>0, else standard print
//  - if levels>1, recursively calls printf_open_ on tail of list (must also be list)
//  - used for printing stack when in the process of building a list
int val_list_sprintf_open_(val_t *list, val_t *buf, const list_fmt_t *lfmt, const fmt_t *el_fmt, unsigned int levels) {
  argcheck_r(val_islisttype(list));
  const char *open,*close;
  val_list_braces(list,&open,&close);
  if (levels<=0) {
    return _val_list_sprintf(buf,val_list_head(list),val_list_len(list),open,close,lfmt,el_fmt);
  } else if (levels == 1) {
    close=NULL;
    return _val_list_sprintf(buf,val_list_head(list),val_list_len(list),open,close,lfmt,el_fmt);
  } else {
    close=NULL;
    throw_list_empty(list);
    throw_if(ERR_BADARGS,!val_islist(val_list_tail(list)));
    err_t r,rlen=0;
    if (0>(r = _val_list_sprintf(buf,val_list_head(list),val_list_len(list)-1,open,close,lfmt,el_fmt))) return r; rlen += r;
    if (0>(r = val_list_sprintf_open_(val_list_tail(list),buf,(lfmt ? lfmt->sublist_fmt : NULL),el_fmt,levels-1))) return r; rlen += r;
    return rlen;
  }
}

void val_list_clear(val_t *list) {
  argcheck(val_islisttype(list));
  list->val.list.len = 0;
}
unsigned int val_list_len(val_t *list) {
  argcheck(val_islisttype(list));
  return list->val.list.len;
}
int val_list_empty(val_t *list) {
  argcheck(val_islisttype(list));
  return list->val.list.len == 0;
}
int val_list_small(val_t *list) {
  argcheck(val_islisttype(list));
  return list->val.list.len <= 1;
}
int val_list_singleton(val_t *list) {
  struct val_list_buffer *buf = _val_list_buf(list);
  return !buf || buf->refcount == 1;
}

err_t val_list_deref(val_t *list) {
  argcheck_r(val_islisttype(list));
  struct val_list_buffer *buf = _val_list_buf(list);
  unsigned int len = val_list_len(list);

  if (!buf || buf->refcount == 1) {
    return 0;
  } else if (!len) {
    _listbuf_destroy(buf);
    list->val.list.b = NULL;
    return 0;
  } else {
    err_t r;
    if ((r=_listbuf_init(list,len))) return r;
    unsigned int off = _val_list_off(list);
    list->val.list.offset=0;
    return _listbuf_destroy_move(buf,off,len,_val_list_head(list));
  }
}

err_t val_list_lpush(val_t *list, val_t *el) {
  argcheck_r(val_islisttype(list));
  err_t r;
  if ((r = val_list_lreserve(list,1))) return r;
  list->val.list.offset--;
  list->val.list.len++;
  val_move(_val_list_get(list,0),el);
  return 0;
}
err_t val_list_rpush(val_t *list, val_t *el) {
  argcheck_r(val_islisttype(list));
  err_t r;
  if ((r = val_list_rreserve(list,1))) { val_destroy(el); return r; }
  list->val.list.len++;
  val_move(_val_list_rget(list,0),el);
  return 0;
}
err_t val_list_rpush2(val_t *list, val_t *a, val_t *b) {
  argcheck_r(val_islisttype(list));
  err_t r;
  val_t *p;
  if ((r = _val_list_rextend(list,2,&p))) goto bad;
  val_move(p++,a);
  val_move(p++,b);
  return 0;
bad:
  val_destroy(a);
  val_destroy(b);
  return r;
}
err_t val_list_rpush3(val_t *list, val_t *a, val_t *b, val_t *c) {
  argcheck_r(val_islisttype(list));
  err_t r;
  val_t *p;
  if ((r = _val_list_rextend(list,3,&p))) goto bad;
  val_move(p++,a);
  val_move(p++,b);
  val_move(p++,c);
  return 0;
bad:
  val_destroy(a);
  val_destroy(b);
  val_destroy(c);
  return r;
}
err_t val_list_vrpushn(val_t *list, int n, va_list ap) {
  err_t r;
  val_t *p;
  if ((r = _val_list_rextend(list,n,&p))) goto bad;
  for(;n;--n) {
    val_move(p++,va_arg(ap,val_t*));
  }
  return 0;
bad:
  for(;n;--n) {
    val_destroy(va_arg(ap,val_t*));
  }
  return r;
}
err_t val_list_rpushn(val_t *list, int n, ...) {
  argcheck_r(val_islisttype(list));
  va_list ap;
  va_start(ap, n);
  err_t r = val_list_vrpushn(list,n,ap);
  va_end(ap);
  return r;
}
err_t val_list_append(val_t *list, val_t *val) {
  if (val_islisttype(val)) return val_list_cat(list,val);
  else return val_list_rpush(list,val);
}
err_t val_list_lpop(val_t *list, val_t *el) {
  argcheck_r(val_islisttype(list) && !val_list_empty(list));
  val_t *head = _val_list_get(list,0);

  if (list->val.list.b->refcount == 1) { //we are the only ref, so we can do a move
    if (el) val_move(el,head);
    else val_destroy(head);
  } else { //do a clone, someone else may need the head element in this list
    if (el) val_clone(el,head);
  }
  list->val.list.offset++;
  list->val.list.len--;
  return 0;
}
err_t val_list_rpop(val_t *list, val_t *el) {
  argcheck_r(val_islisttype(list) && !val_list_empty(list));
  val_t *tail = _val_list_rget(list,0);

  if (list->val.list.b->refcount == 1) { //we are the only ref, so we can do a move
    if (el) val_move(el,tail);
    else val_destroy(tail);
  } else { //do a clone, someone else may need the head element in this list
    if (el) val_clone(el,tail);
  }
  list->val.list.len--;
  return 0;
}
err_t val_list_rpop2(val_t *list, val_t *a, val_t *b) {
  argcheck_r(val_islisttype(list) && val_list_len(list)>=2);
  list->val.list.len-=2;
  val_t *p = _val_list_get(list,list->val.list.len);
  if (list->val.list.b->refcount == 1) { //we are the only ref, so we can do a move
    if (a) val_move(a,p++);
    else val_destroy(p++);
    if (b) val_move(b,p++);
    else val_destroy(p++);
  } else { //do a clone, someone else may need the head element in this list
    if (a) val_clone(a,p++);
    if (b) val_clone(b,p++);
  }
  return 0;
}
err_t val_list_rpop3(val_t *list, val_t *a, val_t *b, val_t *c) {
  argcheck_r(val_islisttype(list) && val_list_len(list)>=3);
  list->val.list.len-=3;
  val_t *p = _val_list_get(list,list->val.list.len);
  if (list->val.list.b->refcount == 1) { //we are the only ref, so we can do a move
    if (a) val_move(a,p++);
    else val_destroy(p++);
    if (b) val_move(b,p++);
    else val_destroy(p++);
    if (c) val_move(c,p);
    else val_destroy(p);
  } else { //do a clone, someone else may need the head element in this list
    if (a) val_clone(a,p++);
    if (b) val_clone(b,p);
    if (c) val_clone(c,p);
  }
  return 0;
}
err_t val_list_vrpopn_(val_t *list, int n, va_list ap) {
  argcheck_r(val_islisttype(list) && val_list_len(list)>=n);
  list->val.list.len-=n;
  val_t *p = _val_list_get(list,list->val.list.len);
  for(;n;--n) {
    val_move(va_arg(ap,val_t*),p++);
  }
  return 0;
}
err_t val_list_rpopn_(val_t *list, int n, ...) {
  va_list ap;
  va_start(ap, n);
  err_t r = val_list_vrpopn_(list,n,ap);
  va_end(ap);
  return r;
}

err_t val_list_cat(val_t *list, val_t *rhs) {
  argcheck_r(val_islisttype(list) && val_islisttype(rhs));
  err_t r;
  unsigned int len = val_list_len(rhs);
  if (!len) {
    val_list_destroy(rhs);
    return 0;
  } else if (val_list_empty(list)) {
    val_list_destroy(list);
    val_move(list,rhs);
    return 0;
  }
  val_t *p;
  if ((r = _val_list_rextend(list,len,&p))) { val_list_destroy(rhs); return r; }
  if ((r = _listbuf_destroy_move(_val_list_buf(rhs),_val_list_off(rhs),len,p))) return r;
  VM_DEBUG_VAL_DESTROY(rhs);
  val_clear(rhs);
  return 0;
}
err_t val_list_lcat(val_t *list, val_t *lhs) {
  argcheck_r(val_islisttype(list) && val_islisttype(lhs));
  err_t r;
  unsigned int len = val_list_len(lhs);
  if (!len) {
    val_list_destroy(lhs);
    return 0;
  } else if (val_list_empty(list)) {
    val_list_destroy(list);
    val_move(list,lhs);
    return 0;
  }
  val_t *p;
  if ((r = _val_list_lextend(list,len,&p))) return r;
  if ((r = _listbuf_destroy_move(_val_list_buf(lhs),_val_list_off(lhs),len,p))) return r;
  VM_DEBUG_VAL_DESTROY(lhs);
  val_clear(lhs);
  return 0;
}

err_t val_list_sublist(val_t *list, unsigned int off, int len) {
  argcheck_r(val_islisttype(list) && off<=val_list_len(list));
  //bound request in [0,len-offset]
  unsigned int curlen = val_list_len(list);
  if (len < 0 || len+off>curlen) len = curlen - off;

  list->val.list.offset += off;
  list->val.list.len = len;
  return 0;
}
err_t val_list_rsplitn(val_t *list, val_t *rhs, unsigned int i) {
  argcheck_r(val_islisttype(list) && i<=val_list_len(list));
  err_t r;

  //TODO: special case for refcount==1 (memcpy to rhs, clear in list)
  if (!_val_list_buf(list)) {
    val_list_init(rhs);
    return 0;
  //} else if (list->val.list.b->refcount == 1) {
  //  return _fatal(ERR_NOT_IMPLEMENTED);
  } else {
    unsigned int len = val_list_len(list);

    if ((r = val_clone(rhs,list))) return r;
    val_list_sublist(rhs,i,len-i);
    val_list_sublist(list,0,i);
    return 0;
  }
}
err_t val_list_lsplitn(val_t *list, val_t *lhs, unsigned int i) {
  argcheck_r(val_islisttype(list) && i<=val_list_len(list));
  err_t r;

  //TODO: special case for refcount==1 (memcpy to lhs, clear in list)
  if (!_val_list_buf(list)) {
    val_list_init(lhs);
    return 0;
  //} else if (list->val.list.b->refcount == 1) {
  //  return _fatal(ERR_NOT_IMPLEMENTED);
  } else {
    unsigned int len = val_list_len(list);

    if ((r = val_clone(lhs,list))) return r;
    val_list_sublist(lhs,0,i);
    val_list_sublist(list,i,len-i);
    return 0;
  }
}

err_t val_list_wrap(val_t *val) {
  err_t r;
  val_t t;
  val_list_init(&t);
  if ((r = val_list_lpush(&t,val))) { val_destroy(&t); return r; }
  val_move(val,&t);
  return 0;
}
err_t val_list_wrap2(val_t *dest, val_t *a, val_t *b) {
  err_t r;
  val_list_init(dest);
  if ((r = val_list_rpush2(dest,a,b))) {
    val_destroy(dest);
  }
  return r;
}
err_t val_list_wrap3(val_t *dest, val_t *a, val_t *b, val_t *c) {
  err_t r;
  val_list_init(dest);
  if ((r = val_list_rpush3(dest,a,b,c))) {
    val_destroy(dest);
  }
  return r;
}
err_t val_list_wrap_arr(val_t *dest, val_t *p, unsigned int n) {
  argcheck_r(n>=1);
  err_t e;
  val_list_init(dest);
  if ((e = _listbuf_init(dest,n))) goto bad;
  dest->val.list.offset = 0;
  dest->val.list.len = n;
  valcpy(_val_list_head(dest),p,n);
  valclr(p,n);
  return 0;
bad:
  val_list_destroy(dest);
  valdestroy(p,n);
  return e;
}
err_t val_list_clone_wrap_arr(val_t *dest, val_t *p, unsigned int n) {
  argcheck_r(n>=1);
  err_t e;
  val_list_init(dest);
  if ((e = _listbuf_init(dest,n))) return e;
  dest->val.list.offset = 0;
  dest->val.list.len = n;
  val_t *d = _val_list_head(dest);
  for(;n;--n) {
    if ((e = val_clone(d++,p++))) goto bad;
  }
  return 0;
bad:
  val_list_destroy(dest);
  return e;
}
err_t val_list_wrapn(val_t *dest, int n, ...) {
  val_list_init(dest);
  va_list ap;
  va_start(ap, n);
  err_t r = val_list_vrpushn(dest,n,ap);
  va_end(ap);
  if (r) val_destroy(dest);
  return r;
}
err_t val_list_unwrap(val_t *list) {
  argcheck_r(val_islisttype(list) && !val_list_empty(list));
  err_t r;
  val_t t;
  if ((r = val_list_lpop(list,&t))) return r;
  val_destroy(list);
  val_move(list,&t);
  return 0;
}

err_t val_list_ith(val_t *list, unsigned int i) {
  argcheck_r(val_islisttype(list) && i<val_list_len(list));
  err_t r;
  val_t t;
  if (list->val.list.b->refcount == 1) {
    val_move(&t,_val_list_get(list,i));
  } else {
    if ((r = val_clone(&t,_val_list_get(list,i)))) return r;
  }
  val_destroy(list);
  val_move(list,&t);
  return 0;
}
err_t val_list_dith(val_t *list, unsigned int i, val_t *el) {
  argcheck_r(val_islisttype(list) && i<val_list_len(list));
  return val_clone(el,_val_list_get(list,i));
}
err_t val_list_seti(val_t *list, unsigned int i, val_t *val) {
  err_t r;
  if ((r = val_list_swapi(list,i,val))) return r;
  val_destroy(val);
  return 0;
}
err_t val_list_swapi(val_t *list, unsigned int i, val_t *val) {
  argcheck_r(val_islisttype(list) && i<val_list_len(list));
  err_t r;
  if ((r = val_list_deref(list))) return r;
  val_swap(_val_list_get(list,i),val);
  return 0;
}

err_t val_list_dign(val_t *list, unsigned int n) {
  throw_if(ERR_BADARGS,n>=val_list_len(list));
  err_t r;
  if ((r = val_list_deref(list))) return r;
  val_t *nth = _val_list_rget(list,n);
  val_t t;
  val_move(&t,nth);
  valmove(nth,nth+1,n);
  val_move(_val_list_rget(list,0),&t);
  return 0;
}
err_t val_list_buryn(val_t *list, unsigned int n) {
  throw_if(ERR_BADARGS,n>=val_list_len(list));
  err_t r;
  if ((r = val_list_deref(list))) return r;
  val_t *nth = _val_list_rget(list,n);
  val_t t = *_val_list_rget(list,0);
  valmove(nth+1,nth,n);
  *nth = t;
  return 0;
}
err_t val_list_flipn(val_t *list, unsigned int n) {
  throw_if(ERR_BADARGS,n>=val_list_len(list));
  err_t r;
  if ((r = val_list_deref(list))) return r;
  unsigned int i;
  for(i=0;i<n/2;++i) {
    val_swap(_val_list_rget(list,i),_val_list_rget(list,n-i-1));
  }
  return 0;
}

//Algorithm:
// Idea is to allow patterns that add to both sides, so we use exponential growth algorithm to slide/grow
//  - if buf not set just reserve requested amount
//  - if refcount==1 and there is space in the current buffer
//    - if there is already space to left just destroy off-n vals (we don't track what vals were valid before)
//      - then return 0
//    - else we need to slide vals over (use _compute_slide_lreserve)
//      - destroy vals we will slide over to the right (off+len to newoff+len)
//      - destroy vals between newoff-n and off (for reservation)
//      - move existing vals from off to newoff
//      - clear vals between off and newoff that we moved
//  - if off>=n, request same amount of memory in new buffer
//  - else we use 1.5X growth in memory to the left until we have enough space
//    - if result < LIST_INITIAL_SIZE, then use that
//    - calculate lspace and rspace and call _val_list_realloc
err_t val_list_lreserve(val_t *list, unsigned int n) {
  argcheck_r(val_islisttype(list));
  struct val_list_buffer *buf = _val_list_buf(list);
  if (!buf) { //alloc new space at requested size
    list->val.list.offset = n;
    return _listbuf_init(list,n);
  }
  const unsigned int len = val_list_len(list);
  const unsigned int off = _val_list_off(list);
  const unsigned int size = buf->size;
  if ( (buf->refcount==1) && (len+n<=size) ) { //move in-place
    if (!len) {
      list->val.list.offset = size;
      valdestroy(_val_list_get(list,-n),n);
      return 0;
    } else if (off>=n) {
      valdestroy(_val_list_get(list,-n),n);
      return 0; //already have space
    } else { //move mem without realloc (shift contents right)
      unsigned int newoff = _compute_exp_slide(off,len,size,n);
      list->val.list.offset = newoff;

      //destroy vals in reserved section (that aren't part of slide)
      if (off>(newoff-n)) valdestroy(buf->p+(newoff-n),off-(newoff-n));
      //slide vals over to make space
      _list_slide(buf->p,off,len,newoff);
      return 0;
    }
  } else {
    unsigned int lspace,rspace;
    _compute_exp_lreserve(off,len,size,n,LIST_INITIAL_SIZE,&lspace,&rspace);
    return _val_list_realloc(list,lspace,rspace);
  }
}
err_t val_list_rreserve(val_t *list, unsigned int n) {
  argcheck_r(val_islisttype(list));
  struct val_list_buffer *buf = _val_list_buf(list);
  if (!buf) { //alloc new space at requested size
    list->val.list.offset = 0;
    return _listbuf_init(list,n);
  }
  const unsigned int len = val_list_len(list);
  const unsigned int off = _val_list_off(list);
  const unsigned int size = buf->size;
  if ( (buf->refcount==1) && (len+n<=size) ) { //move in-place
    if (!len) {
      list->val.list.offset = 0;
      valdestroy(_val_list_get(list,0),n);
      return 0;
    } else if ((size-len-off)>=n) {
      valdestroy(_val_list_get(list,len),n);
      return 0; //already have space
    } else { //move mem without realloc (shift contents left)
      unsigned int newoff = size - len - _compute_exp_slide(size-len-off,len,size,n);
      list->val.list.offset = newoff;

      //destroy vals in reserved section (that aren't part of slide)
      if (n+newoff>off) valdestroy(buf->p+off+len,n+newoff-off);
      //slide vals over to make space
      _list_slide(buf->p,off,len,newoff);
      return 0;
    }
  } else {
    unsigned int rspace,lspace;
    //we flip lspace/rspace to make space on right
    _compute_exp_lreserve(size-len-off,len,size,n,LIST_INITIAL_SIZE,&rspace,&lspace);
    return _val_list_realloc(list,lspace,rspace);
  }
}

//whether any/all values are true values
int val_list_any(val_t *list) {
  argcheck(val_islisttype(list));
  if (val_list_empty(list)) return 0;

  val_t *p = _val_list_get(list,0);
  unsigned int n = val_list_len(list);
  while(n--) {
    if (val_as_bool(p + n)) return 1;
  }
  return 0;
}
int val_list_all(val_t *list) {
  argcheck(val_islisttype(list));
  if (val_list_empty(list)) return 1;

  val_t *p = _val_list_get(list,0);
  unsigned int n = val_list_len(list);
  while(n--) {
    if (val_as_bool(p + n)) return 0;
  }
  return 1;
}

//list comparison functions
int val_list_compare(val_t *lhs, val_t *rhs) {
  argcheck(val_islisttype(lhs) && val_islisttype(rhs));
  unsigned int llen = val_list_len(lhs), rlen = val_list_len(rhs);
  if (!llen || !rlen || _val_list_samestart(lhs,rhs)) {
    if (llen < rlen) return -1;
    else if (rlen < llen) return 1;
    else return 0;
  } else {
    unsigned int i;
    unsigned int n = (llen < rlen) ? llen : rlen;
    val_t *lp = _val_list_get(lhs,0), *rp = _val_list_get(rhs,0);
    for(i = 0; i < n; ++i) {
      int c;
      if ((c = val_compare(lp+i, rp+i))) return c;
    }
    if (i < llen) return 1;
    else if (i < rlen) return -1;
    else return 0;
  }
}
int val_list_lt(val_t *lhs, val_t *rhs) {
  argcheck(val_islisttype(lhs) && val_islisttype(rhs));
  unsigned int llen = val_list_len(lhs), rlen = val_list_len(rhs);
  if (!llen || !rlen || _val_list_samestart(lhs,rhs)) {
    return llen < rlen;
  } else {
    unsigned int i;
    unsigned int n = (llen < rlen) ? llen : rlen;
    val_t *lp = _val_list_get(lhs,0), *rp = _val_list_get(rhs,0);
    for(i = 0; i < n; ++i) {
      int c = val_compare(lp+i, rp+i);
      if (c < 0) return 1;
      else if (c > 0) return 0;
    }
    return i < rlen;
  }
}
int val_list_eq(val_t *lhs, val_t *rhs) {
  argcheck(val_islisttype(lhs) && val_islisttype(rhs));
  unsigned int llen = val_list_len(lhs), rlen = val_list_len(rhs);
  if (!llen || !rlen || _val_list_samestart(lhs,rhs)) {
    return llen == rlen;
  } else if (llen != rlen) {
    return 0;
  } else {
    unsigned int i;
    unsigned int n = (llen < rlen) ? llen : rlen;
    val_t *lp = _val_list_get(lhs,0), *rp = _val_list_get(rhs,0);
    for(i = 0; i < n; ++i) {
      if (!val_eq(lp+i, rp+i)) return 0;
    }
    return 1;
  }
}

//uint32_t val_list_hash32(val_t *str) {
//}
//uint64_t val_list_hash64(val_t *str) {
//}

err_t val_list_qsort(val_t *list) {
  argcheck_r(val_islisttype(list));
  if (val_list_small(list)) return 0; //already sorted (zero or one item)
  err_t r;
  if ((r = val_list_deref(list))) return r;
  val_qsort(_val_list_get(list,0),val_list_len(list));
  return 0;
}
err_t val_list_rqsort(val_t *list) {
  argcheck_r(val_islisttype(list));
  if (val_list_small(list)) return 0; //already sorted (zero or one item)
  err_t r;
  if ((r = val_list_deref(list))) return r;
  val_rqsort(_val_list_get(list,0),val_list_len(list));
  return 0;
}



val_t* val_list_head(val_t *list) {
  argcheck(val_islisttype(list));
  if (val_list_empty(list)) return NULL;
  else return _val_list_head(list);
}
val_t* val_list_tail(val_t *list) {
  argcheck(val_islisttype(list));
  if (val_list_empty(list)) return NULL;
  else return _val_list_rget(list,0);
}
val_t* val_list_get(val_t *list, unsigned int i) {
  argcheck(val_islisttype(list) && i<val_list_len(list));
  return _val_list_get(list,i);
}
val_t* val_list_rget(val_t *list, unsigned int i) {
  argcheck(val_islisttype(list) && i<val_list_len(list));
  return _val_list_rget(list,i);
}

//========code functions========

void val_code_init_handlers(struct type_handlers *h) {
  val_list_init_handlers(h);
}
void val_code_init(val_t *val) {
  val->type = TYPE_CODE;
  val->val.list.b = NULL;
  val->val.list.offset = 0;
  val->val.list.len = 0;
  VM_DEBUG_VAL_INIT(val);
}
err_t val_code_wrap(val_t *val) {
  err_t r;
  val_t t;
  val_code_init(&t);
  if ((r = val_list_lpush(&t,val))) { val_destroy(&t); return r; }
  val_move(val,&t);
  return 0;
}

//========internal list functions========

err_t _val_list_lextend(val_t *list, unsigned int n, val_t **p) {
  argcheck_r(val_islisttype(list));
  err_t r;
  if ((r = val_list_lreserve(list,n))) return r;
  list->val.list.offset -= n;
  list->val.list.len += n;
  *p = _val_list_get(list,0);
  return 0;
}
err_t _val_list_rextend(val_t *list, unsigned int n, val_t **p) {
  argcheck_r(val_islisttype(list));
  err_t r;
  if ((r = val_list_rreserve(list,n))) return r;
  *p = _val_list_get(list,list->val.string.len);
  list->val.list.len += n;
  return 0;
}

//int _val_list_findi(val_t *list, val_t *el) {
//  argcheck_r(val_islisttype(list));
//  return _fatal(ERR_NOT_IMPLEMENTED);
//}
//int val_list_rfindi(val_t *list, val_t *el) {
//  argcheck_r(val_islisttype(list));
//  return _fatal(ERR_NOT_IMPLEMENTED);
//}
//int val_list_findip(val_t *list, int(pred)(val_t* el)) {
//  argcheck_r(val_islisttype(list));
//  return _fatal(ERR_NOT_IMPLEMENTED);
//}
//int val_list_rfindip(val_t *list, int(pred)(val_t* el)) {
//  argcheck_r(val_islisttype(list));
//  return _fatal(ERR_NOT_IMPLEMENTED);
//}

//helpers to get list members
struct val_list_buffer* _val_list_buf(val_t *list) {
  debug_assert(val_islisttype(list));
  return list->val.list.b;
}
unsigned int _val_list_off(val_t *list) {
  debug_assert(val_islisttype(list));
  return list->val.list.offset;
}

//(unsafe) functions to get elements of list (no checks)
val_t* _val_list_head(val_t *list) {
  debug_assert(val_islisttype(list));
  return list->val.list.b->p + list->val.list.offset;
}
val_t* _val_list_get(val_t *list, int i) {
  debug_assert(val_islisttype(list));
  return _val_list_head(list) + i;
}
val_t* _val_list_rget(val_t *list, int i) {
  debug_assert(val_islisttype(list));
  return _val_list_head(list) + val_list_len(list) - 1 - i;
}


//alloc/free list buffer
err_t _listbuf_init(val_t *list, unsigned int size) {
  throw_if(ERR_MALLOC,!(list->val.list.b = malloc(sizeof(struct val_list_buffer))));
  VM_DEBUG_LISTBUF_INIT(list,size);
  struct val_list_buffer *buf = _val_list_buf(list);
  buf->size=size;
  buf->refcount=1;
  cleanup_throw_if(ERR_MALLOC,free(buf),!(buf->p = val_alloc(size)));
  valclr(buf->p,size);
  return 0;
}
void _listbuf_destroy(struct val_list_buffer *buf) {
  VM_DEBUG_LISTBUF_DESTROY(buf);
  if (buf) {
    VM_DEBUG_LISTBUF_FREE(buf);
    if (0 == refcount_dec(buf->refcount)) {
      val_t *p = buf->p;
      unsigned int n = buf->size;
      while(n--) {
        if (p[n].type != VAL_INVALID) {
          val_destroy(&p[n]);
        }
      }
      val_free(p);
      free(buf);
    }
  }
}
err_t _val_list_realloc(val_t *list, unsigned int lspace, unsigned int rspace) {
  argcheck_r(val_islisttype(list));
  VM_DEBUG_LIST_REALLOC(list,lspace,rspace);
  err_t r;
  struct val_list_buffer *buf = _val_list_buf(list);
  const unsigned int len = val_list_len(list);
  const unsigned int off = _val_list_off(list);
  list->val.list.offset = lspace;
  if (!len) {
    _listbuf_destroy(buf);
    if ((r = _listbuf_init(list,lspace+rspace))) return r;
    return 0;
  } else {
    if ((r = _listbuf_init(list,lspace+len+rspace))) return r;
    return _listbuf_destroy_move(buf,off,len,_val_list_head(list));
    return 0;
  }
}
err_t _listbuf_destroy_move(struct val_list_buffer *buf, unsigned int off, unsigned int len, val_t *dest) {
  if (buf->refcount == 1) {
    unsigned int newrefcount = refcount_dec(buf->refcount);
    debug_assert(newrefcount == 0);

    valcpy(dest,buf->p+off,len);

    unsigned int i;
    //destroy to the left&right of vals we moved
    for(i=0;i<off;++i) {
      if (buf->p[i].type != VAL_INVALID) {
        val_destroy(&buf->p[i]);
      }
    }
    for(i=off+len;i<buf->size;++i) {
      if (buf->p[i].type != VAL_INVALID) {
        val_destroy(&buf->p[i]);
      }
    }
    val_free(buf->p);
    free(buf);
    return 0;
  } else {
    err_t r;
    if ((r = valcln(dest,buf->p+off,len))) return r;
    _listbuf_destroy(buf);
    return 0;
  }
}

//slide a portion of an array of vals
//  - destroy vals we will overwrite
//  - move vals
//  - clear vals we moved that weren't overwritten
void _list_slide(val_t *buf, unsigned int off, unsigned int len, unsigned int newoff) {
  if (off<newoff) {
//    - slide existing vals over to make space
//      - destroy vals we will slide over to the right (off+len to newoff+len)
      valdestroy(buf+(off+len),newoff-off);
//      - move vals from off to newoff
      valmove(buf+newoff,buf+off,len);
//      - clear vals between off and newoff that we moved
      valclr(buf+off,newoff-off);
  } else if (off>newoff) {
//    - slide existing vals over to make space
//      - destroy vals we will slide over to the LEFT (newoff to off)
      valdestroy(buf+newoff,off-newoff);
//      - move vals from off to newoff
      valmove(buf+newoff,buf+off,len);
//      - clear vals between newoff+len and off+len that we moved
      valclr(buf+newoff+len,off-newoff);
  } else return;
}

err_t _val_list_lpop(val_t *list, val_t *first) {
  debug_assert(val_islisttype(list) && !val_list_empty(list) && first != NULL);
  err_t r = 0;
  if (_val_list_buf(list)->refcount == 1) val_move(first,_val_list_head(list));
  else r = val_clone(first,_val_list_head(list));
  list->val.list.len--;
  list->val.list.offset++;
  return r;
}
err_t _val_list_rpop(val_t *list, val_t *last) {
  debug_assert(val_islisttype(list) && !val_list_empty(list) && last != NULL);
  err_t r = 0;
  if (_val_list_buf(list)->refcount == 1) val_move(last,_val_list_rget(list,0));
  else r = val_clone(last,_val_list_rget(list,0));
  list->val.list.len--;
  return r;
}
void _val_list_ldrop(val_t *list) {
  debug_assert(val_islisttype(list) && !val_list_empty(list));
  struct val_list_buffer *buf = _val_list_buf(list);
  if (buf->refcount == 1) val_destroy(_val_list_head(list));
  list->val.list.len--;
  list->val.list.offset++;
}
void _val_list_rdrop(val_t *list) {
  debug_assert(val_islisttype(list) && !val_list_empty(list));
  struct val_list_buffer *buf = _val_list_buf(list);
  if (buf->refcount == 1) val_destroy(_val_list_rget(list,0));
  list->val.list.len--;
}


void val_list_braces(val_t *list, const char **open, const char **close) {
  debug_assert(val_islisttype(list));
  *open = (list->type == TYPE_CODE) ? "[" : "(";
  *close = (list->type == TYPE_CODE) ? "]" : ")";
}

//print single element of list
//  - handles forward/reverse indexing
//  - uses sublist_fmt for list child elements if sublist_fmt != NULL, val_fmt otherwise
int _val_list_sprintf_el(val_t *buf, val_t *p, int len, int i, int reverse, const list_fmt_t *sublist_fmt, const fmt_t *val_fmt) {
  debug_assert_r(i<len);
  val_t *val;
  if (reverse) val = p + len - 1 - i;
  else val = p + i;

  if (sublist_fmt && val_islisttype(val)) {
    return val_list_sprintf_(val,buf,sublist_fmt,val_fmt);
  } else {
    return val_sprintf_(val,buf,val_fmt);
  }
}
int _val_list_fprintf(FILE *file, val_t *p, int len, const char *open, const char *close, const list_fmt_t *lfmt, const fmt_t *val_fmt) {
  val_t buf;
  val_string_init(&buf);
  err_t r;
  if (0>(r = _val_list_sprintf(&buf,p,len,open,close,lfmt,val_fmt))) goto cleanup;
  if (0>(r = val_fprintf_(&buf,file,fmt_v))) goto cleanup;
cleanup:
  val_destroy(&buf);
  return r;
}
//core function for printing a list of values
int _val_list_sprintf(val_t *buf, val_t *p, int len, const char *open, const char *close, const list_fmt_t *lfmt, const fmt_t *val_fmt) {
  err_t r,rlen=0;
  int i=0;
  int trunc=0;
  int closelen = (close && lfmt->braces ? strlen(close) : 0); //close-list string
  int reverse = lfmt->reverse;
  const list_fmt_t *sublist_fmt = lfmt->sublist_fmt;
  int opensep = open && strlen(open) && lfmt->braces && lfmt->braceseplen; //whether we need bracesep before contents
  int closesep = closelen && lfmt->braceseplen; //whether we need bracesep before close


  if (open && lfmt->braces) {
    if (0 > (r = val_sprint_cstr(buf,open))) return r; rlen += r;
  }

  if (lfmt->max_els>=0 && len>lfmt->max_els) {
    if (lfmt->trunc_tail) {
      if (reverse) p += (len-lfmt->max_els+1);
    } else {
      if (!reverse) p += (len-lfmt->max_els+1);
    }
    len = lfmt->max_els;
    if (lfmt->trunclen) len--; //count trunc as one of our els
    trunc=1;
  }
  if (!len) goto out_close; //so we can assume len>=1 below

  if (lfmt->max_bytes >= 0) { //need to handle truncation
    int rlim = lfmt->max_bytes - closelen;
    if (rlim<lfmt->trunclen) rlim=lfmt->trunclen;
    if (lfmt->trunc_tail) { //truncate right side if we run out of space
      if (opensep) { //print open sep
        if (0 > (r = val_sprint_(buf,lfmt->bracesep,lfmt->braceseplen))) return r; rlen += r;
      }

      //skip sep before first el
      if (0 > (r = _val_list_sprintf_el(buf,p,len,i++,reverse,sublist_fmt,val_fmt))) return r; rlen += r;
      //print remaining els
      for(;i<len && rlen<=rlim;++i) { //print elements (in l-to-r order)
        if (lfmt->seplen) { if (0 > (r = val_sprint_(buf,lfmt->sep,lfmt->seplen))) return r; rlen += r; }
        if (0 > (r = _val_list_sprintf_el(buf,p,len,i,reverse,sublist_fmt,val_fmt))) return r; rlen += r;
      }
      if (closesep) { //print close sep
        if (0 > (r = val_sprint_(buf,lfmt->bracesep,lfmt->braceseplen))) return r; rlen += r;
        closesep=0;
      }

      if (rlen>rlim) { //trunc tail chars
        if (!buf) return lfmt->max_bytes;
        else {
          if ((r = val_string_substring(buf,0,val_string_len(buf)-rlen+rlim-lfmt->trunclen))) goto bad_tbuf;
          if (0 > (r = val_sprint_(buf,lfmt->trunc,lfmt->trunclen))) return r; rlen += r;
          rlen=rlim;
        }
      } else if (trunc && lfmt->trunclen) { //trunc tail els
        if (lfmt->seplen) { if (0 > (r = val_sprint_(buf,lfmt->sep,lfmt->seplen))) return r; rlen += r; }
        if (0 > (r = val_sprint_(buf,lfmt->trunc,lfmt->trunclen))) return r; rlen += r;
      } //else have space -- standard print
    } else { //truncate left -- build string right-to-left in temp buffer so we can stop when we hit lim
      val_t t,tbuf;
      int tlen=0;
      int tlim = lfmt->max_bytes - rlen - (closelen ? lfmt->braceseplen + closelen : 0);
      if (tlim<lfmt->trunclen) tlim=lfmt->trunclen;
      val_string_init(&tbuf);
      reverse = !reverse; //print list backwards (by prepending) till we hit limit

      //skip sep before first (last) el
      if (0 > (r = _val_list_sprintf_el(&tbuf,p,len,i++,reverse,sublist_fmt,val_fmt))) goto bad_tbuf; tlen += r;
      debug_assert_r(tlen == val_string_len(&tbuf)); //verify tlen after first el
      //print remaining els to tbuf (in reversed order)
      val_string_init(&t); //we init+destroy t only once so we can reuse it for each el (clear and refill)
      for(;i<len && tlen<=tlim;++i) { //print elements (in l-to-r order) //+1 is so we know if we are only 1 over
        val_string_clear(&t);
        if (0 > (r = _val_list_sprintf_el(&t,p,len,i,reverse,sublist_fmt,val_fmt))) goto bad_t; tlen += r;
        if (lfmt->seplen) { if (0 > (r = val_sprint_(&t,lfmt->sep,lfmt->seplen))) goto bad_t; tlen += r; }
        if ((r = val_string_lcat_copy(&tbuf,&t))) goto bad_t; //copy so we can re-use t for each el
        debug_assert_r(tlen == val_string_len(&tbuf)); //verify tlen after each el
      }

      if (opensep && tlen<=tlim) { //print open sep to tbuf
        val_string_clear(&t);
        if (0 > (r = val_sprint_(&t,lfmt->bracesep,lfmt->braceseplen))) goto bad_t; tlen += r;
        if ((r = val_string_lcat_copy(&tbuf,&t))) goto bad_t; //copy so we can re-use t for each el
      }

      if (tlen>tlim) { //trunc head chars
        if (!buf) { r = lfmt->max_bytes; goto bad_t; }
        else {
          if ((r = val_string_substring(&tbuf,tlen-(tlim-lfmt->trunclen),-1))) goto bad_t;
          tlen = tlim-lfmt->trunclen;
          val_string_clear(&t);
          if (0 > (r = val_sprint_(&t,lfmt->trunc,lfmt->trunclen))) goto bad_t; tlen += r;
          if ((r = val_string_lcat_copy(&tbuf,&t))) goto bad_t;
        }
      } else if (trunc && lfmt->trunclen) { //trunc head els
        if (0 > (r = val_sprint_(buf,lfmt->trunc,lfmt->trunclen))) goto bad_t; rlen += r;
        if (lfmt->seplen) { if (0 > (r = val_sprint_(buf,lfmt->sep,lfmt->seplen))) goto bad_t; rlen += r; }
      } //else have space -- standard print

      val_destroy(&t);
      debug_assert_r(tlen == val_string_len(&tbuf)); //verify tlen after each el
      if (0>(r = val_sprintf_(&tbuf,buf,fmt_v))) goto bad_tbuf; rlen += r; //+= truncated contents
      if (closesep) { //print close sep on right
        if (0 > (r = val_sprint_(buf,lfmt->bracesep,lfmt->braceseplen))) return r; rlen += r;
        closesep=0;
      }
      goto out_close;

bad_t:
      val_destroy(&t);
bad_tbuf:
      val_destroy(&tbuf);
      return r;
    }
  } else {

    if (opensep) {
      if (0 > (r = val_sprint_(buf,lfmt->bracesep,lfmt->braceseplen))) return r; rlen += r;
    }
    if (trunc && lfmt->trunclen && !lfmt->trunc_tail) {
      if (0>(r = val_sprint_(buf,lfmt->trunc,lfmt->trunclen))) return r; rlen += r;
    } else {
      //skip sep before first el (unless we printed trunc already, then that is first el)
      if (0 > (r = _val_list_sprintf_el(buf,p,len,i++,reverse,sublist_fmt,val_fmt))) return r; rlen += r;
    }

    //print remaining els
    for(;i<len;++i) {
      if (lfmt->seplen) { if (0 > (r = val_sprint_(buf,lfmt->sep,lfmt->seplen))) return r; rlen += r; }
      if (0 > (r = _val_list_sprintf_el(buf,p,len,i,reverse,sublist_fmt,val_fmt))) return r; rlen += r;
    }

    if (trunc && lfmt->trunclen && lfmt->trunc_tail) { //el truncated list
      if (lfmt->seplen) { if (0 > (r = val_sprint_(buf,lfmt->sep,lfmt->seplen))) return r; rlen += r; }
      if (0 > (r = val_sprint_(buf,lfmt->trunc,lfmt->trunclen))) return r; rlen += r;
    }
  }

out_close:
  if (lfmt->braces) {
    //space before close if we have space -- handled above right now
    if (closesep && (lfmt->max_bytes < 0 || rlen+lfmt->braceseplen+closelen<=lfmt->max_bytes)) {
      if (0 > (r = val_sprint_(buf,lfmt->bracesep,lfmt->braceseplen))) return r; rlen += r;
    }
    if (0 > (r = val_sprint_(buf,close,closelen))) return r; rlen += r;
  }
  return rlen;
}

//whether a and b point to the same buffer and have the same start offset
int _val_list_samestart(val_t *a, val_t *b) {
  //so we can skip comparing lists when buffers are the same and start index is the same
  return (_val_list_buf(a) == _val_list_buf(b)) && (_val_list_off(a) == _val_list_off(b));
}
