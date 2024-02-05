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

#include "val_list.h"
#include "val_list_internal.h"
#include "val_string.h"
#include "val_printf.h"
#include "vm_err.h"
#include "helpers.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <valgrind/helgrind.h>

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

//TODO: SIMPLE_POOL for small lists -- under small buf threshold alloc small buf from pool, else malloc buf
//  - this should greatly improve all the small list creation we do (e.g. wrap,wrap2,wrap3)


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



const list_fmt_t *list_fmt_V = &_list_fmt_V;
const list_fmt_t *list_fmt_lines = &_list_fmt_lines;
const list_fmt_t *list_fmt_rlines = &_list_fmt_rlines;
const list_fmt_t *list_fmt_join = &_list_fmt_join;


inline int _val_lst_empty(valstruct_t *v) { return v->v.lst.len==0; }
inline int _val_lst_small(valstruct_t *v) { return v->v.lst.len<=1; }
inline val_t* _val_lst_buf(valstruct_t *v) { return v->v.lst.buf->p; }
inline val_t* _val_lst_begin(valstruct_t *v) { return _val_lst_buf(v) + v->v.lst.off; }
inline val_t* _val_lst_off(valstruct_t *v, int i) { return _val_lst_begin(v)+i; }
inline val_t* _val_lst_end(valstruct_t *v) { return _val_lst_buf(v) + v->v.lst.off + _val_lst_len(v); }
inline val_t* _val_lst_bufend(valstruct_t *v) { return _val_lst_buf(v) + _val_lst_size(v); }
inline unsigned int _val_lst_offset(valstruct_t *v) { return v->v.lst.off; }
inline unsigned int _val_lst_len(valstruct_t *v) { return v->v.lst.len; }
inline unsigned int _val_lst_size(valstruct_t *v) { return v->v.lst.buf->size; }

//err_t val_list_print(val_t val) {
//  err_t e;
//  //if (!val_is_lst(v)) return _throw(ERR_BAD_TYPE);
//  valstruct_t *v = __lst_ptr(val);
//  int iscode = v->type == TYPE_CODE;
//  printf("%c",iscode ? '[' : '(');
//  if (0>(e = _val_lst_print_contents(v))) return e;
//  printf("%s",iscode ? " ]" : " )");
//  return e;
//}
//
//err_t val_list_print_open(val_t val, unsigned int levels) {
//  //if (!val_is_lst(v)) return _throw(ERR_BAD_TYPE);
//  valstruct_t *v = __lst_ptr(val);
//  if (!levels) return val_list_print(val);
//
//  if (v->type == TYPE_CODE) printf("[");
//  else printf("(");
//
//  unsigned int i,n = _val_lst_len(v);
//  for(i=0;i+1<n;++i) {
//    printf(" ");
//    err_t e;
//    if ((e = val_print_code(_val_lst_begin(v)[i]))) return e;
//  }
//  if(n) { //if there was at least one item, now print the last one
//    printf(" ");
//    val=_val_lst_begin(v)[i];
//    if (val_is_lst(val)) return val_list_print_open(val,levels-1);
//    else return val_print_code(val);
//  }
//
//  return 0;
//}
//
//err_t _val_lst_print_contents(valstruct_t *v) {
//  if (_val_lst_empty(v)) return 0;
//  val_t *p,*end;
//  err_t e;
//  for(p=_val_lst_begin(v),end=_val_lst_end(v); p!=end; ++p) {
//    printf(" ");
//    if ((e = val_print_code(*p))) return e;
//  }
//  return 0;
//}
//err_t _val_lst_sprint_contents(valstruct_t *buf,valstruct_t *v) {
//  if (_val_lst_empty(v)) return 0;
//  val_t *p,*end;
//  err_t e;
//  for(p=_val_lst_begin(v),end=_val_lst_end(v); p!=end; ++p) {
//    if ((e = _val_str_cat_ch(buf,' '))) return e;
//    if ((e = val_sprint_code(buf,*p))) return e;
//  }
//  return 0;
//}
//err_t _val_list_print(valstruct_t *v) {
//  printf("(");
//  err_t e;
//  if ((e = _val_lst_print_contents(v))) return e;
//  printf(" )");
//  return 0;
//}
//err_t _val_code_print(valstruct_t *v) {
//  printf("[");
//  err_t e;
//  if ((e = _val_lst_print_contents(v))) return e;
//  printf(" ]");
//  return 0;
//}
//err_t _val_list_sprint(valstruct_t *buf, valstruct_t *v) {
//  _val_str_cat_cstr(buf,"(",1);
//  err_t e;
//  if ((e = _val_lst_sprint_contents(buf,v))) return e;
//  _val_str_cat_cstr(buf," )",2);
//  return 0;
//}
//err_t _val_code_sprint(valstruct_t *buf, valstruct_t *v) {
//  _val_str_cat_cstr(buf,"[",1);
//  err_t e;
//  if ((e = _val_lst_sprint_contents(buf,v))) return e;
//  _val_str_cat_cstr(buf," ]",2);
//  return 0;
//}

val_t _lstval_alloc(enum val_type type) {
  valstruct_t *t = _valstruct_alloc();
  t->type = type;
  t->v.lst.buf=0;
  t->v.lst.off=0;
  t->v.lst.len=0;
  return __lst_val(t);
}
void _val_lst_init(valstruct_t *v, enum val_type type) {
  v->type = type;
  v->v.lst.buf=0;
  v->v.lst.off=0;
  v->v.lst.len=0;
}



lbuf_t* _lbuf_alloc(unsigned int size) {
  lbuf_t *p;
  if (!(p = malloc(sizeof(lbuf_t)+sizeof(val_t)*size))) return NULL;
  p->size=size;
  p->dirty=0;
  p->refcount=1;
  memset(p->p,0,sizeof(val_t)*size);
  return p;
}

inline void _lst_release(valstruct_t *v) {
  if (0 == (refcount_dec(v->v.lst.buf->refcount))) {
    ANNOTATE_HAPPENS_AFTER(v);
    ANNOTATE_HAPPENS_BEFORE_FORGET_ALL(v);
    val_t *p,*end;
    if (v->v.lst.buf->dirty) { //dirty -- need to destroy every val in buffer
      for(p=_val_lst_buf(v),end=p+_val_lst_size(v); p!=end; ++p) {
        val_destroy(*p);
      }
    } else { //clean -- only need to destroy vals in list v
      for(p=_val_lst_begin(v),end=p+_val_lst_len(v); p!=end; ++p) {
        val_destroy(*p);
      }
    }
    free(v->v.lst.buf);
  } else {
    ANNOTATE_HAPPENS_BEFORE(v);
  }
}


val_t val_empty_list() {
  return _lstval_alloc(TYPE_LIST);
}
val_t val_empty_code() {
  return _lstval_alloc(TYPE_CODE);
}

void _val_list_init(valstruct_t *v) {
  _val_lst_init(v,TYPE_LIST);
}

void _val_code_init(valstruct_t *v) {
  _val_lst_init(v,TYPE_CODE);
}

void _val_lst_destroy_(valstruct_t *v) {
  if (v->v.lst.buf) { _lst_release(v); }
}

//TODO: consider alternate memory reserve algorithms
//
#define _lst_lspace(val) ((val)->v.lst.off)
#define _lst_rspace(val) ((val)->v.lst.buf->size - (val)->v.lst.len - (val)->v.lst.off)
#define _lst_lrspace(val) ((val)->v.lst.buf->size - (val)->v.lst.len)
#define _lst_singleref(val) ((val)->v.lst.buf->refcount==1)
#define _lst_dirty(val) ((val)->v.lst.buf->dirty)
#define _lst_samestart(lhs,rhs) ((lhs)->v.lst.buf == (rhs)->v.lst.buf && (lhs)->v.lst.off == (rhs)->v.lst.off)

void _val_lst_clear(valstruct_t *v) {
  if (v->v.lst.len) {
    if (_lst_singleref(v)) {
      v->v.lst.len = 0;
      _val_lst_cleanclear(v);
    } else {
      _lst_release(v);
      v->v.lst.len = 0;
      v->v.lst.off = 0; //TODO: needed???
      v->v.lst.buf = NULL;
    }
  }
}

inline void _val_lst_swap(valstruct_t *a, valstruct_t *b) {
  lst_t t = a->v.lst;
  a->v.lst = b->v.lst;
  b->v.lst = t;
}
void _val_lst_clone(valstruct_t *ret, valstruct_t *orig) {
  *ret = *orig;
  if (ret->v.lst.buf) refcount_inc(ret->v.lst.buf->refcount);
}

err_t _val_lst_deep_clone(valstruct_t *ret, valstruct_t *orig) {
  return _throw(ERR_NOT_IMPLEMENTED); //TODO: IMPLEMENTME -- only deep clones the current list, not child lists/strings
  if (!_val_lst_buf(orig) || _val_lst_empty(orig)) {
    ret->v.lst.len = 0;
    ret->v.lst.off = 0; //TODO: needed???
    ret->v.lst.buf = NULL;
    return 0;
  } else {
    lbuf_t *newbuf;
    int len = _val_lst_len(orig);
    if (!(newbuf = _lbuf_alloc(len))) return _throw(ERR_MALLOC);

    err_t e;
    //TODO: collect bufs used by children (and maybe min off, max len) to only deep clone each buf once
    // - sbufs,lbufs, probably use hashtable of deep cloned buffers (keyed on orig buffer address)
    // - this same code will be used/needed for creating self-contained bytecode vals
    if ((e = val_clonen(newbuf->p,_val_lst_begin(orig),len))) {
      free(newbuf);
      return e;
    }
    ret->v.lst.buf = newbuf;
    ret->v.lst.off = 0;
    ret->v.lst.len = len;
    return 0;
  }
}

err_t val_list_wrap(val_t *val) {
  val_t t = val_empty_list();
  err_t e;
  if ((e = _val_lst_rpush(__lst_ptr(t),*val))) {
    val_destroy(t);
    return e;
  }
  *val = t;
  return 0;
}
err_t val_code_wrap(val_t *val) {
  val_t t = val_empty_code();
  err_t e;
  if ((e = _val_lst_rpush(__lst_ptr(t),*val))) {
    val_destroy(t);
    return e;
  }
  *val = t;
  return 0;
}

err_t val_list_wrap_arr(val_t *dest, val_t *p, int n) {
  *dest = val_empty_list();
  valstruct_t *tv = __lst_ptr(*dest);
  err_t e;
  val_t *q;
  if ((e = _val_lst_rextend(tv,n,&q))) { val_destroy(*dest); return e; }
  valcpy(q,p,n);
  val_clearn(p,n);
  return 0;
}

err_t val_list_wrap_arr_clone(val_t *dest, val_t *p, int n) {
  *dest = val_empty_list();
  valstruct_t *tv = __lst_ptr(*dest);
  err_t e;
  val_t *q;
  if ((e = _val_lst_rextend(tv,n,&q))) goto bad;
  if ((e = val_clonen(q,p,n))) goto bad;
  return 0;
bad:
  val_destroy(*dest);
  return e;
}

err_t val_list_wrap2(val_t *dst, val_t a, val_t b) {
  *dst = val_empty_list();
  valstruct_t *tv = __lst_ptr(*dst);
  err_t e;
  val_t *q;
  if ((e = _val_lst_rextend(tv,2,&q))) goto bad;
  q[0] = a;
  q[1] = b;
  return 0;
bad:
  val_destroy(*dst);
  val_destroy(a);
  val_destroy(b);
  return e;
}
err_t val_list_wrap3(val_t *dst, val_t a, val_t b, val_t c) {
  *dst = val_empty_list();
  valstruct_t *tv = __lst_ptr(*dst);
  err_t e;
  val_t *q;
  if ((e = _val_lst_rextend(tv,3,&q))) goto bad;
  q[0] = a;
  q[1] = b;
  q[2] = c;
  return 0;
bad:
  val_destroy(*dst);
  val_destroy(a);
  val_destroy(b);
  val_destroy(c);
  return e;
}
err_t val_list_wrap3_clone(val_t *dst, val_t a, val_t b, val_t c) {
  *dst = val_empty_list();
  valstruct_t *tv = __lst_ptr(*dst);
  err_t e;
  val_t *q;
  if ((e = _val_lst_rextend(tv,3,&q))) goto bad;
  if ((e = val_clone(q,a))) goto bad;
  if ((e = val_clone(q+1,b))) goto bad;
  if ((e = val_clone(q+2,c))) goto bad;
  return 0;
bad:
  val_destroy(*dst);
  return e;
}
err_t val_list_wrapn(val_t *dst, int n, ...) {
  *dst = val_empty_list();
  va_list vals;
  va_start(vals, n);
  err_t e = _val_lst_vrpushn(__list_ptr(*dst),n,vals);
  va_end(vals);
  if (e) val_destroy(*dst);
  return e;
}

err_t val_lst_ith(val_t *lst, unsigned int i) {
  valstruct_t *v = __lst_ptr(*lst);
  if (i>=_val_lst_len(v)) return _throw(ERR_BADARGS);
  val_t *ith = _val_lst_begin(v)+i;
  val_t t;
  if (_lst_singleref(v)) {
    t = *ith;
    val_clear(ith);
  } else {
    err_t e;
    if ((e = val_clone(&t,*ith))) return e;
  }
  val_destroy(*lst);
  *lst = t;
  return 0;
}
err_t _val_lst_rdign(valstruct_t *lst, unsigned int n) {
  throw_if(ERR_BADARGS,n>=_val_lst_len(lst));
  err_t e;
  if ((e = _val_lst_deref(lst))) return e;
  val_t *nth = _val_lst_end(lst) - 1 - n;
  val_t t = *nth;
  valmove(nth,nth+1,n);
  _val_lst_end(lst)[-1] = t;
  return 0;
}
err_t _val_lst_rburyn(valstruct_t *lst, unsigned int n) {
  throw_if(ERR_BADARGS,n>=_val_lst_len(lst));
  err_t e;
  if ((e = _val_lst_deref(lst))) return e;
  val_t *nth = _val_lst_end(lst) - 1 - n;
  val_t t = _val_lst_end(lst)[-1];
  valmove(nth+1,nth,n);
  *nth = t;
  return 0;
}
err_t _val_lst_rflipn(valstruct_t *lst, unsigned int n) {
  throw_if(ERR_BADARGS,n>_val_lst_len(lst));
  err_t r;
  if ((r = _val_lst_deref(lst))) return r;
  val_t *tail = _val_lst_end(lst) - 1;
  unsigned int i;
  for(i=0;i<n/2;++i) {
    val_swap(tail-i,tail-(n-i-1));
  }
  return 0;
}

void _val_lst_clean(valstruct_t *v) {
  val_t *p,*end;
  for(p=_val_lst_buf(v),end=p+v->v.lst.off; p!=end; ++p) {
    val_destroy(*p);
  }
  for(p=_val_lst_end(v),end=_val_lst_buf(v)+_val_lst_size(v); p!=end; ++p) {
    val_destroy(*p);
  }
  v->v.lst.buf->dirty=0;
}

void _val_lst_cleanclear(valstruct_t *v) {
  val_t *p,*end;
  for(p=_val_lst_buf(v),end=p+v->v.lst.off; p!=end; ++p) {
    val_destroy(*p);
    val_clear(p);
  }
  for(p=_val_lst_end(v),end=_val_lst_buf(v)+_val_lst_size(v); p!=end; ++p) {
    val_destroy(*p);
    val_clear(p);
  }
  v->v.lst.buf->dirty=0;
}


void _val_lst_slide(valstruct_t *lst, unsigned int newoff) {
  if (_lst_dirty(lst)) _val_lst_cleanclear(lst);
  valmove(_val_lst_buf(lst)+newoff,_val_lst_begin(lst),_val_lst_len(lst));
  if (newoff<lst->v.lst.off) {
    lst->v.lst.off = newoff;
    val_clearn(_val_lst_end(lst),lst->v.lst.off-newoff);
  } else {
    val_clearn(_val_lst_begin(lst),newoff-lst->v.lst.off);
    lst->v.lst.off = newoff;
  }
}

err_t _val_lst_move(valstruct_t *lst, val_t *dst) { //moves contents of list v to location dst, and releases lbuf from lst (doesn't release lst though)
  err_t e;
  if (_lst_singleref(lst)) {
    valcpy(dst,_val_lst_begin(lst),_val_lst_len(lst));
    if (_lst_dirty(lst)) { //dirty singleton -- destroy entries AROUND lst and the free buffer
      //TODO: what if empty???
      val_t *p,*end;
      for(p=_val_lst_buf(lst),end=p+lst->v.lst.off; p!=end; ++p) {
        val_destroy(*p);
      }
      for(p=_val_lst_end(lst),end=_val_lst_bufend(lst); p!=end; ++p) {
        val_destroy(*p);
      }
      free(lst->v.lst.buf); //FIXME: was _lst_release
    } else { //clean singleton -- just free buffer
      free(lst->v.lst.buf);
    }
    return 0;
  } else {
    if ((e = val_clonen(dst,_val_lst_begin(lst),_val_lst_len(lst)))) return e;
    _lst_release(lst);
    return 0;
  }
}

err_t _val_lst_deref(valstruct_t *lst) {
  //argcheck_r(val_islisttype(list));
  if (!_val_lst_buf(lst) || _lst_singleref(lst)) {
    return 0;
  } else if (_val_lst_empty(lst)) {
    _lst_release(lst);
    lst->v.lst.buf = NULL;
    lst->v.lst.off = 0; //TODO: needed???
    return 0;
  } else {
    return _val_lst_realloc(lst,0,0); //TODO: how much lspace/rspace??? (keep old value, or reset to 0)
  }
}
err_t _val_lst_cleanderef(valstruct_t *lst) {
  //argcheck_r(val_islisttype(list));
  if (!_val_lst_buf(lst)) {
    return 0;
  } else if(_lst_singleref(lst)) {
    _val_lst_cleanclear(lst);
    return 0;
  } else if (_val_lst_empty(lst)) {
    _lst_release(lst);
    lst->v.lst.buf = NULL;
    lst->v.lst.off = 0; //TODO: needed???
    return 0;
  } else {
    return _val_lst_realloc(lst,0,0); //TODO: how much lspace/rspace??? (keep old value, or reset to 0)
  }
}

err_t _val_lst_realloc(valstruct_t *v, unsigned int left, unsigned int right) {
  lbuf_t *newbuf;
  int len = _val_lst_len(v);
  if (!(newbuf = _lbuf_alloc(left + len + right))) return _throw(ERR_MALLOC);

  err_t e;
  if (len) {
    if ((e = _val_lst_move(v, newbuf->p + left))) {
      free(newbuf);
      return e;
    }
  }
  v->v.lst.buf = newbuf;
  v->v.lst.off = left;
  return 0;
}


err_t _val_lst_cat(valstruct_t *lst, valstruct_t *suffix) {
  unsigned int n = _val_lst_len(suffix);
  if (!n) {
    if (suffix->v.lst.buf) _lst_release(suffix);
  } else if (_val_lst_empty(lst)) { //TODO: need option (or another func) to still move suffix into lst if there is space (optimize for e.g. reused buffer)
    if (lst->v.lst.buf) _lst_release(lst);
    lst->v.lst = suffix->v.lst;
  } else {
    err_t e;
    //if lst has space we pick lst to append to, else if suffix has space we prepend to suffix, else we extend and append to lst
    //FIXME: update list cases
    if ((_lst_singleref(lst) && _lst_lrspace(lst)>=n) || !(_lst_singleref(suffix) && _lst_lrspace(suffix)>=n)) {
      if ((e = _val_lst_rreserve(lst,n))) return e;
      if ((e = _val_lst_move(suffix,_val_lst_end(lst)))) return e;
      lst->v.lst.len+=n;
    } else {
      if ((e = _val_lst_lreserve(suffix,_val_lst_len(lst)))) return e;
      suffix->v.lst.off -= _val_lst_len(lst);
      if ((e = _val_lst_move(lst,_val_lst_begin(suffix)))) return e;

      lst->v.lst.buf = suffix->v.lst.buf;
      lst->v.str.off = suffix->v.str.off;
      lst->v.str.len+=n;
    }
  }
  _valstruct_release(suffix);
  return 0;
}

err_t _val_lst_lpush(valstruct_t *lst, val_t el) {
  //if (lst->v.lst.buf && lst->v.lst.off > 0 && _val_lst_begin(lst)[-1] == el) {
  //  //we are re-adding val that is already in buf, just expand view of buffer
  //  //TODO: should we have this special case???
  //  lst->v.lst.off--;
  //  lst->v.lst.len++;
  //  val_destroy(el); //cleanup el
  //  return 0;
  //} else {
    err_t e;
    unsigned int n;
    if (!lst->v.lst.buf) n=LST_MIN_ALLOC; //initial allocation for lists
    else if (_lst_singleref(lst) && _lst_lrspace(lst)>=1) n=1;
    else if (_val_lst_len(lst) < LST_MIN_ALLOC*2) n=LST_MIN_ALLOC;
    else n = _val_lst_len(lst)/2;

    if ((e = _val_lst_lreserve(lst,n))) { val_destroy(el); return e; }

    lst->v.lst.off--;
    lst->v.lst.len++;
    *_val_lst_begin(lst) = el;
    return 0;
  //}
}
err_t _val_lst_rpush(valstruct_t *lst, val_t el) {
  //if (lst->v.lst.buf && _val_lst_end(lst)<_val_lst_bufend(lst) && *_val_lst_end(lst) == el) {
  //  //we are re-adding val that is already in buf, just expand view of buffer
  //  //TODO: should we have this special case???
  //  lst->v.lst.len++;
  //  val_destroy(el); //cleanup el (since we didn't need it)
  //  return 0;
  //} else {
    err_t e;
    unsigned int n;
    if (!lst->v.lst.buf) n=LST_MIN_ALLOC; //initial allocation for lists
    else if (_lst_singleref(lst) && _lst_lrspace(lst)>=1) n=1;
    else if (_val_lst_len(lst) < LST_MIN_ALLOC*2) n=LST_MIN_ALLOC;
    else n = _val_lst_len(lst)/2;

    if ((e = _val_lst_rreserve(lst,n))) { val_destroy(el); return e; }

    *_val_lst_end(lst) = el;
    lst->v.lst.len++;
    return 0;
  //}
}
err_t _val_lst_rpush2(valstruct_t *lst, val_t a, val_t b) {
  err_t e;
  val_t *p;
  if ((e = _val_lst_rextend(lst,2,&p))) goto bad;
  p[0] = a;
  p[1] = b;
  return 0;
bad:
  val_destroy(a);
  val_destroy(b);
  return e;
}
err_t _val_lst_rpush3(valstruct_t *lst, val_t a, val_t b, val_t c) {
  err_t e;
  val_t *p;
  if ((e = _val_lst_rextend(lst,3,&p))) goto bad;
  p[0] = a;
  p[1] = b;
  p[2] = c;
  return 0;
bad:
  val_destroy(a);
  val_destroy(b);
  val_destroy(c);
  return e;
}
err_t _val_lst_vrpushn(valstruct_t *lst, int n, va_list vals) {
  err_t e;
  val_t *p;
  if ((e = _val_lst_rextend(lst,3,&p))) goto bad;
  for(;n;--n) {
    *(p++) = va_arg(vals,val_t);
  }
  return 0;
bad:
  for(;n;--n) {
    val_destroy(*(p++));
  }
  return e;
}
err_t _val_lst_rpushn(valstruct_t *lst, int n, ...) {
  va_list vals;
  va_start(vals, n);
  err_t r = _val_lst_vrpushn(lst,n,vals);
  va_end(vals);
  return r;
}

err_t _val_lst_rpush_fromr(valstruct_t *lst, valstruct_t *src_lst) {
  if (_val_lst_empty(src_lst)) return _throw(ERR_EMPTY);
  err_t e;
  val_t el;
  if ((e = _val_lst_rpop(lst,&el))) return e;
  if ((e = _val_lst_rpush(lst,el))) {
    val_destroy(el);
    return e;
  }
  return 0;
}

err_t _val_lst_lpop(valstruct_t *lst, val_t *head) {
  if (_val_lst_empty(lst)) return _throw(ERR_EMPTY);

  val_t *lhead = _val_lst_begin(lst);
  lst->v.lst.off++;
  lst->v.lst.len--;
  if(_lst_singleref(lst)) {
    *head = *lhead;
    val_clear(lhead);
  } else {
    lst->v.lst.buf->dirty=1;
    err_t e;
    if ((e = val_clone(head,*lhead))) return e;
  }
  return 0;
}

err_t _val_lst_rpop(valstruct_t *lst, val_t *tail) {
  if (_val_lst_empty(lst)) return _throw(ERR_EMPTY);

  lst->v.lst.len--;
  val_t *ltail = _val_lst_end(lst);
  if(_lst_singleref(lst)) {
    *tail = *ltail;
    val_clear(ltail);
  } else {
    lst->v.lst.buf->dirty=1;
    err_t e;
    if ((e = val_clone(tail,*ltail))) return e;
  }
  return 0;
}
err_t _val_lst_ldrop(valstruct_t *lst) {
  if (_val_lst_empty(lst)) return _throw(ERR_EMPTY);
  lst->v.lst.off++;
  lst->v.lst.len--;
  lst->v.lst.buf->dirty=1; //TODO: should we destroy+clear instead???
  return 0;
}
err_t _val_lst_rdrop(valstruct_t *lst) {
  if (_val_lst_empty(lst)) return _throw(ERR_EMPTY);
  lst->v.lst.len--;
  lst->v.lst.buf->dirty=1; //TODO: should we destroy+clear instead???
  return 0;
}

err_t _val_lst_lreserve(valstruct_t *v, unsigned int n) {
  if (!v->v.lst.buf) {
    if (!(v->v.lst.buf = _lbuf_alloc(n))) return _fatal(ERR_MALLOC);
    v->v.lst.off = n;
  } else if (_lst_singleref(v) && _lst_lrspace(v)>=n) { //unique ref and have space
    if(_lst_lspace(v)>=n) { //already have space
      if (_lst_dirty(v)) _val_lst_cleanclear(v);
    } else { //have space with shuffling
      _val_lst_slide(v,n);
    }
  } else {
    return _val_lst_realloc(v,n,_lst_rspace(v));
  }
  return 0;
}

err_t _val_lst_rreserve(valstruct_t *v, unsigned int n) {
  if (!v->v.lst.buf) {
    if (!(v->v.lst.buf = _lbuf_alloc(n))) return _fatal(ERR_MALLOC);
    v->v.lst.off = 0;
  } else if (_lst_singleref(v) && _lst_lrspace(v)>=n) {
    if(_lst_rspace(v)>=n) { //already have space
      if (_lst_dirty(v)) _val_lst_cleanclear(v);
    } else { //have space with shuffling
      _val_lst_slide(v,_val_lst_size(v) - _val_lst_len(v) - n);
    }
  } else {
    return _val_lst_realloc(v,_lst_lspace(v),n);
  }
  return 0;
}

err_t _val_lst_lextend(valstruct_t *v, unsigned int n, val_t **p) {
  err_t e;
  if ((e = _val_lst_lreserve(v,n))) return e;
  v->v.lst.off -= n;
  v->v.lst.len += n;
  *p = _val_lst_begin(v);
  return 0;
}
err_t _val_lst_rextend(valstruct_t *v, unsigned int n, val_t **p) {
  err_t e;
  if ((e = _val_lst_rreserve(v,n))) return e;
  *p = _val_lst_end(v);
  v->v.lst.len += n;
  return 0;
}

//int _val_lst_qrreserve(valstruct_t *v, unsigned int n) { //quick reserve used by vm (assumes sigleref and buf != NULL)
//  if (!v->v.lst.buf) {
//    if (!(v->v.lst.buf = _lbuf_alloc(n))) return _fatal(ERR_MALLOC);
//    v->v.lst.off = 0;
//  } else if (_lst_singleref(v) && _lst_lrspace(v)>=n) {
//    if(_lst_rspace(v)>=n) { //already have space
//      if (_lst_dirty(v)) _val_lst_clean(v);
//    } else { //have space with shuffling
//      _val_lst_slide(v,_val_lst_size(v) - _val_lst_len(v) - n);
//    }
//  } else {
//    return _val_lst_realloc(v,_lst_lspace(v),n);
//  }
//  return 0;
//}


void _val_lst_sublst(valstruct_t *lst, unsigned int off, unsigned int len) {
  unsigned int lstlen=_val_lst_len(lst);
  if (off>lstlen) off=lstlen;
  lstlen-=off;
  if (len>lstlen) len=lstlen;
  lst->v.lst.off += off;
  lst->v.lst.len = len;
}

int _val_lst_splitn(valstruct_t *lst, val_t *rhs, unsigned int off) {
  if (off>_val_lst_len(lst)) return _throw(ERR_BADARGS);
  valstruct_t *ret;
  if (!(ret = _valstruct_alloc())) return _throw(ERR_MALLOC);
  *ret = *lst;
  if (ret->v.lst.buf) {
    refcount_inc(ret->v.lst.buf->refcount);
    lst->v.lst.len=off;
    ret->v.lst.len-=off;
    ret->v.lst.off+=off;
  } //else both sides are of length zero anyways so there is nothing to do
  *rhs = __lst_val(ret);
  return 0;
}

int _val_lst_compare(valstruct_t *lhs, valstruct_t *rhs) {
  unsigned int llen = _val_lst_len(lhs), rlen = _val_lst_len(rhs);
  if (!llen || !rlen || _lst_samestart(lhs,rhs)) {
    if (llen < rlen) return -1;
    else if (rlen < llen) return 1;
    else return 0;
  } else {
    unsigned int i;
    unsigned int n = (llen < rlen) ? llen : rlen;
    val_t *lp = _val_lst_begin(lhs), *rp = _val_lst_begin(rhs);
    for(i = 0; i < n; ++i) {
      int c;
      if ((c = val_compare(lp[i], rp[i]))) return c;
    }
    if (i < llen) return 1;
    else if (i < rlen) return -1;
    else return 0;
  }
}
int _val_lst_lt(valstruct_t *lhs, valstruct_t *rhs) {
  unsigned int llen = _val_lst_len(lhs), rlen = _val_lst_len(rhs);
  if (!llen || !rlen || _lst_samestart(lhs,rhs)) {
    return llen < rlen;
  } else {
    unsigned int i;
    unsigned int n = (llen < rlen) ? llen : rlen;
    val_t *lp = _val_lst_begin(lhs), *rp = _val_lst_begin(rhs);
    for(i = 0; i < n; ++i) {
      int c = val_compare(lp[i], rp[i]);
      if (c < 0) return 1;
      else if (c > 0) return 0;
    }
    return i < rlen;
  }
}
int _val_lst_eq(valstruct_t *lhs, valstruct_t *rhs) {
  unsigned int llen = _val_lst_len(lhs), rlen = _val_lst_len(rhs);
  if (!llen || !rlen || _lst_samestart(lhs,rhs)) {
    return llen == rlen;
  } else if (llen != rlen) {
    return 0;
  } else {
    unsigned int i;
    unsigned int n = (llen < rlen) ? llen : rlen;
    val_t *lp = _val_lst_begin(lhs), *rp = _val_lst_begin(rhs);
    for(i = 0; i < n; ++i) {
      if (!val_eq(lp[i], rp[i])) return 0;
    }
    return 1;
  }
}


#undef _lst_lspace
#undef _lst_rspace
#undef _lst_lrspace
#undef _lst_singleref
#undef _lst_dirty

int val_list_fprintf_simple(valstruct_t *v, FILE *file, const struct printf_fmt *fmt) {
  switch(fmt->conversion) {
    case 'v': case 'V':
      break;
    default:
      return _throw(ERR_BADTYPE);
  }
  int rlen=0;
  int r;
  unsigned int i,n;

  int iscode = v->type == TYPE_CODE;

  if (0 > (r = val_fprint_ch(file,iscode ? '[' : '('))) return r;
  rlen += r;

  for(i=0,n = _val_lst_len(v);i<n;++i) {
    if (0 > (r = val_fprint_ch(file,' '))) return r;
    rlen += r;
    if (0 > (r = val_fprintf_(_val_lst_begin(v)[i],file,fmt_V))) return r;
    rlen += r;
  }

  if (0>(r = val_fprint_(file,iscode ? " ]" : " )",2))) return r;
  rlen += r;
  return rlen;
}

int val_list_sprintf_simple(valstruct_t *v, valstruct_t *buf, const struct printf_fmt *fmt) {
  switch(fmt->conversion) {
    case 'v': case 'V':
      break;
    default:
      return _throw(ERR_BADTYPE);
  }
  int rlen=0;
  int r;
  unsigned int i,n;

  int iscode = v->type == TYPE_CODE;

  if (0 > (r = val_sprint_ch(buf,iscode ? '[' : '('))) return r;
  rlen += r;

  for(i=0,n = _val_lst_len(v);i<n;++i) {
    if (0 > (r = val_sprint_ch(buf,' '))) return r;
    rlen += r;
    if (0 > (r = val_sprintf_(_val_lst_begin(v)[i],buf,fmt_V))) return r;
    rlen += r;
  }

  if (0 > (r = val_sprint_(buf,iscode ? " ]" : " )",2))) return r;
  rlen += r;

  return rlen;
}





//TODO: these all need refactoring (converted from old vm)
int val_list_fprintf(valstruct_t *lst, FILE *file, const fmt_t *fmt) {
  //argcheck_r(val_islisttype(list));

  int r;
  list_fmt_t lfmt;
  if ((r = val_list_format_fmt(&lfmt,fmt))) return r;
  const fmt_t *el_fmt = (fmt->conversion == 'v' ? fmt_v : fmt_V);

  return val_list_fprintf_(lst,file,&lfmt,el_fmt);
}
int val_list_sprintf(valstruct_t *list, valstruct_t *buf, const fmt_t *fmt) {
  //argcheck_r(val_islisttype(list));

  int r;
  list_fmt_t lfmt;
  if ((r = val_list_format_fmt(&lfmt,fmt))) return r;
  const fmt_t *el_fmt = (fmt->conversion == 'v' ? fmt_v : fmt_V);

  return val_list_sprintf_(list,buf,&lfmt,el_fmt);
}

int val_list_fprintf_(valstruct_t *lst, FILE *file, const list_fmt_t *lfmt, const fmt_t *el_fmt) {
  //argcheck_r(val_islisttype(list));
  const char *open,*close;
  val_list_braces(lst,&open,&close);
  unsigned int len = _val_lst_len(lst);
  return _val_list_fprintf(file,len ? _val_lst_begin(lst) : NULL,len,open,close,lfmt,el_fmt);
}
int val_list_sprintf_(valstruct_t *lst, valstruct_t *buf, const list_fmt_t *lfmt, const fmt_t *el_fmt) {
  //argcheck_r(val_islisttype(list));
  const char *open,*close;
  val_list_braces(lst,&open,&close);
  unsigned int len = _val_lst_len(lst);
  return _val_list_sprintf(buf,len ? _val_lst_begin(lst) : NULL,len,open,close,lfmt,el_fmt);
}

int val_list_fprintf_open_(valstruct_t *lst, FILE *file, const list_fmt_t *lfmt, const fmt_t *el_fmt, unsigned int levels) {
  //argcheck_r(val_islisttype(list));
  val_t buf = val_empty_string();
  int r;
  if (0>(r = val_list_sprintf_open_(lst,__str_ptr(buf),lfmt,el_fmt,levels))) goto bad;
  if (0>(r = val_fprintf_(buf,file,fmt_v))) goto bad;
bad:
  val_destroy(buf);
  return r;
}

//print list without close if levels>0, else standard print
//  - if levels>1, recursively calls printf_open_ on tail of list (must also be list)
//  - used for printing stack when in the process of building a list
int val_list_sprintf_open_(valstruct_t *lst, valstruct_t *buf, const list_fmt_t *lfmt, const fmt_t *el_fmt, unsigned int levels) {
  //argcheck_r(val_islisttype(list));
  const char *open,*close;
  val_list_braces(lst,&open,&close);
  unsigned int len = _val_lst_len(lst);
  if (levels<=0) {
    return _val_list_sprintf(buf,len ? _val_lst_begin(lst) : NULL,len,open,close,lfmt,el_fmt);
  } else if (levels == 1) {
    close=NULL;
    return _val_list_sprintf(buf,len ? _val_lst_begin(lst) : NULL,len,open,close,lfmt,el_fmt);
  } else {
    close=NULL;
    throw_lst_empty(lst);
    //throw_if(ERR_BADARGS,!val_islist(val_list_tail(list)));
    int r,rlen=0;
    if (0>(r = _val_list_sprintf(buf,len ? _val_lst_begin(lst) : NULL,len-1,open,close,lfmt,el_fmt))) return r; rlen += r;
    if (0>(r = val_list_sprintf_open_(__lst_ptr(_val_lst_end(lst)[-1]),buf,(lfmt ? lfmt->sublist_fmt : NULL),el_fmt,levels-1))) return r; rlen += r;
    return rlen;
  }
}

//list fprintf using pointer+len instead of list val
int val_listp_fprintf(FILE *file, val_t *p, int len, const list_fmt_t *lfmt, const fmt_t *val_fmt) {
  return _val_list_fprintf(file,p,len,NULL,NULL,lfmt,val_fmt);
}
//list sprintf using pointer+len instead of list val
int val_listp_sprintf(valstruct_t *buf, val_t *p, int len, const list_fmt_t *lfmt, const fmt_t *val_fmt) {
  return _val_list_sprintf(buf,p,len,NULL,NULL,lfmt,val_fmt);
}
//list fprintf using va_list
int val_vlist_fprintf(FILE *file, const list_fmt_t *lfmt, const fmt_t *val_fmt, int len, va_list vals) {
  int r;
  if (lfmt->max_bytes >= 0) { //have specified max length -- need to use sprintf so we can truncate before printing
    val_t buf = val_empty_string();
    if (0>(r = val_vlist_sprintf(__str_ptr(buf),lfmt,val_fmt,len,vals))) goto cleanup;
    if (0>(r = val_fprintf_(buf,file,fmt_v))) goto cleanup;
cleanup:
    val_destroy(buf);
    return r;
  } else {
    int rlen = 0;
    while(len--) {
      if (0>(r = val_fprintf_(va_arg(vals,val_t),file,val_fmt))) return r; rlen += r;
      if (len) {
        if (0>(r = val_fprint_(file,lfmt->sep,lfmt->seplen))) return r; rlen += r;
      }
    }
    return rlen;
  }
}
int val_vlist_sprintf(valstruct_t *buf, const list_fmt_t *lfmt, const fmt_t *val_fmt, int len, va_list vals) {
  int r,rlen=0;
  if (lfmt->max_els >= 0 && lfmt->max_els < len) len = lfmt->max_els;
  int maxlen = lfmt->max_bytes;
  if (maxlen >= 0 && maxlen < lfmt->trunclen) maxlen = lfmt->trunclen; //space for ... (or whatever trunc is set to)
  while(len--) {
    if (0>(r = val_sprintf_(va_arg(vals,val_t),buf,val_fmt))) return r; rlen += r;
    if (len) {
      if (0>(r = val_sprint_(buf,lfmt->sep,lfmt->seplen))) return r; rlen += r;
    }
  }
  if (maxlen >= 0 && maxlen < rlen) {
    if (buf) {
      if (lfmt->trunc_tail) {
        _val_str_substr(buf,0,_val_str_len(buf)-rlen+maxlen-lfmt->trunclen);
        _val_str_cat_cstr(buf,lfmt->trunc,lfmt->trunclen); //guaranteed because we trunc'd from more than this
      } else {
        char *bufstart = _val_str_end(buf)-rlen;
        char *truncstart = _val_str_end(buf)-maxlen-lfmt->trunclen;
        memmove(bufstart+lfmt->trunclen,truncstart,maxlen-lfmt->trunclen);
        memcpy(bufstart,lfmt->trunc,lfmt->trunclen);
        _val_str_substr(buf,0,_val_str_len(buf)-rlen+maxlen);
      }
    }
    return maxlen;
  } else {
    return rlen;
  }
}

//list fprintf using pointer+len, appending extra_vals to contents
int val_listp_fprintf_extra(FILE *file, val_t *p, int len, const list_fmt_t *lfmt, const fmt_t *val_fmt, int extran, va_list extra) {
  int r;
  if (lfmt->max_bytes >= 0) { //have specified max length -- need to use sprintf so we can truncate before printing
    val_t buf = val_empty_string();
    if (0>(r = val_listp_sprintf_extra(__str_ptr(buf),p,len,lfmt,val_fmt,extran,extra))) goto cleanup;
    if (0>(r = val_fprintf_(buf,file,fmt_v))) goto cleanup;
cleanup:
    val_destroy(buf);
    return r;
  } else {
    int rlen=0;
    if (!lfmt->reverse) {
      if (0>(r = val_listp_fprintf(file,p,len,lfmt,val_fmt))) return r; rlen += r;
      if (rlen && extran) {
        if (0>(r = val_fprint_(file,lfmt->sep,lfmt->seplen))) return r; rlen += r;
      }
    }

    while(extran--) {
      if (0>(r = val_fprintf_(va_arg(extra,val_t),file,val_fmt))) return r; rlen += r;
      if (extran) {
        if (0>(r = val_fprint_(file,lfmt->sep,lfmt->seplen))) return r; rlen += r;
      }
    }

    if (lfmt->reverse) {
      if (rlen) {
        if (0>(r = val_fprint_(file,lfmt->sep,lfmt->seplen))) return r; rlen += r;
      }
      if (0>(r = val_listp_fprintf(file,p,len,lfmt,val_fmt))) return r; rlen += r;
    }
    return rlen;
  }
}

//list sprintf using pointer+len, appending extra_vals to contents
int val_listp_sprintf_extra(valstruct_t *buf, val_t *p, int len, const list_fmt_t *lfmt, const fmt_t *val_fmt, int extran, va_list extra) {
  if (lfmt->max_els >= 0 && lfmt->max_els < len) len = lfmt->max_els;

  int r,rlen=0;
  if (!lfmt->reverse) {
    if (0>(r = val_listp_sprintf(buf,p,len,lfmt,val_fmt))) return r; rlen += r;
    if (rlen && extran) {
      if (0>(r = val_sprint_(buf,lfmt->sep,lfmt->seplen))) return r; rlen += r;
    }
  }

  while(extran--) {
    if (0>(r = val_sprintf_(va_arg(extra,val_t),buf,val_fmt))) return r; rlen += r;
    if (extran) {
      if (0>(r = val_sprint_(buf,lfmt->sep,lfmt->seplen))) return r; rlen += r;
    }
  }

  if (lfmt->reverse) {
    if (rlen) {
      if (0>(r = val_sprint_(buf,lfmt->sep,lfmt->seplen))) return r; rlen += r;
    }
    if (0>(r = val_listp_sprintf(buf,p,len,lfmt,val_fmt))) return r; rlen += r;
  }


  int maxlen = lfmt->max_bytes;
  if (maxlen >= 0 && maxlen < lfmt->trunclen) maxlen = lfmt->trunclen; //space for ... (or whatever trunc is set to)
  if (maxlen >=0 && maxlen < rlen) { //need to truncate
    if (buf) {
      if (lfmt->trunc_tail) {
        _val_str_substr(buf,0,_val_str_len(buf)-rlen+maxlen-lfmt->trunclen);
        _val_str_cat_cstr(buf,lfmt->trunc,lfmt->trunclen); //guaranteed space because we trunc'd from more than this
      } else {
        char *bufstart = _val_str_end(buf)-rlen;
        char *truncstart = _val_str_end(buf)-maxlen-lfmt->trunclen;
        memmove(bufstart+lfmt->trunclen,truncstart,maxlen-lfmt->trunclen);
        memcpy(bufstart,lfmt->trunc,lfmt->trunclen);
        _val_str_substr(buf,0,_val_str_len(buf)-rlen+maxlen);
      }
    }
    return maxlen;
  } else { //just print it (no max length)
    return rlen;
  }
}



void val_list_braces(valstruct_t *lst, const char **open, const char **close) {
  //debug_assert(val_islisttype(list));
  *open = (lst->type == TYPE_CODE) ? "[" : "(";
  *close = (lst->type == TYPE_CODE) ? "]" : ")";
}

//print single element of list
//  - handles forward/reverse indexing
//  - uses sublist_fmt for list child elements if sublist_fmt != NULL, val_fmt otherwise
int _val_list_sprintf_el(valstruct_t *buf, val_t *p, int len, int i, int reverse, const list_fmt_t *sublist_fmt, const fmt_t *val_fmt) {
  debug_assert_r(i<len);
  val_t val;
  if (reverse) val = p[len - 1 - i];
  else val = p[i];

  if (sublist_fmt && val_is_lst(val)) {
    return val_list_sprintf_(__lst_ptr(val),buf,sublist_fmt,val_fmt);
  } else {
    return val_sprintf_(val,buf,val_fmt);
  }
}

//TODO: at least basic fprintf options without going through sprintf
int _val_list_fprintf(FILE *file, val_t *p, int len, const char *open, const char *close, const list_fmt_t *lfmt, const fmt_t *val_fmt) {
  val_t buf = val_empty_string();
  err_t r;
  if (0>(r = _val_list_sprintf(__str_ptr(buf),p,len,open,close,lfmt,val_fmt))) goto cleanup;
  if (0>(r = val_fprintf_(buf,file,fmt_v))) goto cleanup;
cleanup:
  val_destroy(buf);
  return r;
}
//core function for printing a list of values
int _val_list_sprintf(valstruct_t *buf, val_t *p, int len, const char *open, const char *close, const list_fmt_t *lfmt, const fmt_t *val_fmt) {
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
          _val_str_substr(buf,0,_val_str_len(buf)-rlen+rlim-lfmt->trunclen);
          if (0 > (r = val_sprint_(buf,lfmt->trunc,lfmt->trunclen))) return r; rlen += r; //TODO: or bad_tbuf??? -- previous line errored to that before
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
      tbuf = val_empty_string();
      valstruct_t *tbv = __str_ptr(tbuf);
      reverse = !reverse; //print list backwards (by prepending) till we hit limit

      //skip sep before first (last) el
      if (0 > (r = _val_list_sprintf_el(tbv,p,len,i++,reverse,sublist_fmt,val_fmt))) goto bad_tbuf; tlen += r;
      //debug_assert_r(tlen == val_string_len(&tbuf)); //verify tlen after first el
      //print remaining els to tbuf (in reversed order)
      t = val_empty_string(); //we init+destroy t only once so we can reuse it for each el (clear and refill)

      valstruct_t *tv = __str_ptr(t);
      for(;i<len && tlen<=tlim;++i) { //print elements (in l-to-r order) //+1 is so we know if we are only 1 over
        _val_str_clear(tv);
        if (0 > (r = _val_list_sprintf_el(tv,p,len,i,reverse,sublist_fmt,val_fmt))) goto bad_t; tlen += r;
        if (lfmt->seplen) { if (0 > (r = val_sprint_(tv,lfmt->sep,lfmt->seplen))) goto bad_t; tlen += r; }
        if ((r = _val_str_rcat_copy(tbv,tv))) goto bad_t; //copy so we can re-use t for each el
        //debug_assert_r(tlen == val_string_len(&tbuf)); //verify tlen after each el
      }

      if (opensep && tlen<=tlim) { //print open sep to tbuf
        _val_str_clear(tv);
        if (0 > (r = val_sprint_(tv,lfmt->bracesep,lfmt->braceseplen))) goto bad_t; tlen += r;
        if ((r = _val_str_rcat_copy(tbv,tv))) goto bad_t; //copy so we can re-use t for each el
      }

      if (tlen>tlim) { //trunc head chars
        if (!buf) { r = lfmt->max_bytes; goto bad_t; }
        else {
          _val_str_substr(tbv,tlen-(tlim-lfmt->trunclen),-1);
          tlen = tlim-lfmt->trunclen;
          _val_str_clear(tv);
          if (0 > (r = val_sprint_(tv,lfmt->trunc,lfmt->trunclen))) goto bad_t; tlen += r;
          if ((r = _val_str_rcat_copy(tbv,tv))) goto bad_t;
        }
      } else if (trunc && lfmt->trunclen) { //trunc head els
        if (0 > (r = val_sprint_(buf,lfmt->trunc,lfmt->trunclen))) goto bad_t; rlen += r;
        if (lfmt->seplen) { if (0 > (r = val_sprint_(buf,lfmt->sep,lfmt->seplen))) goto bad_t; rlen += r; }
      } //else have space -- standard print

      val_destroy(t);
      //debug_assert_r(tlen == val_string_len(&tbuf)); //verify tlen after each el
      if (0>(r = val_sprintf_(tbuf,buf,fmt_v))) goto bad_tbuf; rlen += r; //+= truncated contents
      if (closesep) { //print close sep on right
        if (0 > (r = val_sprint_(buf,lfmt->bracesep,lfmt->braceseplen))) return r; rlen += r;
        closesep=0;
      }
      goto out_close;

bad_t:
      val_destroy(t);
bad_tbuf:
      val_destroy(tbuf);
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

