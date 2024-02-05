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

#include "val.h"

#include "val_list.h"
#include "val_string.h"
#include "val_dict.h"
#include "val_ref.h"
#include "val_file.h"
#include "val_fd.h"
#include "val_vm.h"
#include "val_printf.h"
#include "vm_err.h"
#include "opcodes.h"
#include "defpool.h"
#include "helpers.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>



inline double __val_dbl(val_t val) { *(uint64_t*)&val = ~(uint64_t)val; return *((double*)(&val)); }
inline val_t __dbl_val(const double f) { return (val_t)(~( *((val64_t*)&f) )); }

//valstruct_t* __val_ptr(val_t val) { return val_ptr(val); }
//int32_t __val_int(val_t val) { return __val_int(val); }
//double __val_dbl(val_t val) { return __val_dbl(val); }
//val_t __str_val(valstruct_t *v) { return __str_val(v); }
//val_t __lst_val(valstruct_t *v) { return __lst_val(v); }
//val_t __val_val(valstruct_t *v) { return __val_val(v); }
//val_t __int_val(int32_t i) { return __int_val(i); }
//val_t __dbl_val(double v) { return __dbl_val(v); }

//TODO: we need a new thread-safe pool allocator.
//DEFINE_SIMPLE_POOL(valstruct_t,4096,_valstruct_alloc,_valstruct_release)
DEFINE_NO_POOL(valstruct_t,4096,_valstruct_alloc,_valstruct_release)


void val_destroy(val_t val) {
#ifdef DEBUG_VAL
  val_t dbg = __val_dbg_val(val);
  if (!val_is_op(dbg)) val_destroy((val_t)dbg);
#endif
  valstruct_t *v;
  switch(__val_tag(val)) {
    case _STR_TAG:
      v = __str_ptr(val);
      if (v->v.str.buf) { _sbuf_release(v->v.str.buf); }
      break;
    case _LST_TAG:
      v = __lst_ptr(val);
      if (v->v.lst.buf) { _lst_release(v); }
      break;
    case _VAL_TAG:
      v = __val_ptr(val);
      switch(v->type) {
        case TYPE_DICT:
          _val_dict_destroy_(v);
          _valstruct_release(v);
          break;
        case TYPE_REF:
          _val_ref_destroy(v);
          break;
        case TYPE_FILE:
          _val_file_destroy(v);
          break;
        case TYPE_FD:
          _val_fd_destroy(v);
          break;
        case TYPE_VM:
          _val_vm_destroy(v);
          break;
        default:
          _fatal(ERR_NOT_IMPLEMENTED);
      }
      return;
    default:
      return; //skip releasing valstruct
  }
  _valstruct_release(v);
}
void val_destroyn(val_t *p, size_t n) {
  for(;n;p++,n--) {
    val_destroy(*p);
  }
}


err_t val_clone(val_t *val, val_t orig) {
  err_t e;
#ifdef DEBUG_VAL
  val64_t dbg = __val_dbg_val(orig);
  if (!val_is_op(dbg)) {
    val_t dbg_clone;
    if ((e = val_clone(&dbg_clone,(val_t)dbg))) return e;
    dbg = (val64_t)dbg_clone;
  }
  //after this point (when DEBUG_VAL), need to destroy dbg if err (goto bad_e)
#endif
  valstruct_t *p,*origp;
  //if (val_is_double(val) || val_is_ptr(val)) {
  //  return val;
  //} else {
  switch(__val_tag(orig)) {
    case _STR_TAG:
      if (!(p = _valstruct_alloc())) return _fatal(ERR_MALLOC);

      origp = __str_ptr(orig);
      *p=*origp;
      if (p->v.str.buf) refcount_inc(p->v.str.buf->refcount);
      *val = __val_dbg(__str_val(p),dbg);
      return 0;
    case _LST_TAG:
      if (!(p = _valstruct_alloc())) return _fatal(ERR_MALLOC);

      origp = __lst_ptr(orig);
      *p=*origp;
      if (p->v.lst.buf) refcount_inc(p->v.lst.buf->refcount);
      *val = __val_dbg(__lst_val(p),dbg);
      return 0;
    case _VAL_TAG:
      origp = __val_ptr(orig);

      //type clone function
      switch(origp->type) {
        case TYPE_DICT:
          if ((e = _val_dict_clone(val,origp))) goto bad_e;
          break;
        case TYPE_REF:
          if ((e = _val_ref_clone(val,origp))) goto bad_e;
          break;
        case TYPE_FILE:
          if ((e = _val_file_clone(val,origp))) goto bad_e;
          break;
        case TYPE_FD:
          if ((e = _val_fd_clone(val,origp))) goto bad_e;
          break;
        case TYPE_VM:
          if ((e = val_vm_clone(val,origp->v.vm))) goto bad_e;
          break;
        default:
          _fatal(ERR_NOT_IMPLEMENTED);
          //*p=*origp;
      }
#ifdef DEBUG_VAL
      __val_dbg_val(*val) = dbg;
#endif
      return 0;
    case _TAG5:
      return _fatal(ERR_BADTYPE);
    default: //base case (for inlined val)
#ifdef DEBUG_VAL
      *val= __val_dbg(__val_dbg_strip(orig),dbg); //equivalent to below when DEBUG_VAL not defined
#else
      *val=orig;
#endif
      return 0;
  }
  //}
bad_e:
#ifdef DEBUG_VAL
  //if we already cloned dbg, need to destroy
  if (!val_is_op(dbg)) {
    val_destroy(dbg);
  }
#endif
  return e;
}

err_t val_clonen(val_t *val, val_t *orig, unsigned int n) {
  unsigned int i=n;
  err_t e;
  while(i--) {
    if ((e = val_clone(val++,*(orig++)))) { //if we hit an error we need to destroy the already cloned values
      for(++i;i<n;++i) {
        val_destroy(*(++val));
      }
      return e;
    }
  }
  return 0;
}

err_t val_validate(val_t val) {
#ifdef DEBUG_VAL
  val64_t dbg = __val_dbg_val(val);
  if (!val_is_null(dbg)) {
    err_t e;
    if ((e = val_validate(dbg))) return e;
  }
#endif

  valstruct_t *v;
  if (val_is_double(val)) {
    return 0; //TODO: validate double (e.g. supported NaN values)
  }
  int op;
  unsigned int off,len,size;
  switch(__val_tag(val)) {
    case _OP_TAG:
      op = __val_op(val);
      if (op < 0 || op >= N_OPS) return _throw(ERR_BADTYPE);
      return 0;
    case _INT_TAG:
      if (__val_int47(val) >= (1UL << 32)) return _throw(ERR_BADTYPE);
      return 0;
    case _STR_TAG:
      v = __str_ptr(val);
      off = v->v.str.off;
      len = v->v.str.len;

      if (v->v.str.buf) {
        if (v->v.str.buf->refcount < 1) return _throw(ERR_BADTYPE);
        if (v->v.str.buf->refcount > 10000) return _throw(ERR_BADTYPE); //NOTE: this doesn't actually guarantee val is bad, but seems highly unlikely during VM debugging
        size = v->v.str.buf->size;
        if (off > size || len > size || off+len > size) return _throw(ERR_BADTYPE);
      } else {
        if (len != 0) return _throw(ERR_BADTYPE);
      }
      switch(v->type) {
        case TYPE_STRING:
          return 0;
        case TYPE_IDENT:
          return 0;
        //case TYPE_BYTECODE:
        default:
          return _throw(ERR_BADTYPE);
      }
    case _LST_TAG:
      v = __lst_ptr(val);
      off = v->v.lst.off;
      len = v->v.lst.len;

      if (v->v.lst.buf) {
        if (v->v.lst.buf->refcount < 1) return _throw(ERR_BADTYPE);
        if (v->v.lst.buf->refcount > 10000) return _throw(ERR_BADTYPE); //NOTE: this doesn't actually guarantee val is bad, but seems highly unlikely during VM debugging
        size = v->v.lst.buf->size;
        if (off > size || len > size || off+len > size) return _throw(ERR_BADTYPE);
      } else {
        if (len != 0) return _throw(ERR_BADTYPE);
      }
      switch(v->type) {
        case TYPE_LIST:
          return 0;
        case TYPE_CODE:
          return 0;
        default:
          return _throw(ERR_BADTYPE);
      }
    case _VAL_TAG:
      v = __val_ptr(val);

      switch(v->type) {
        case TYPE_DICT:
          if (!v->v.dict.h) return _throw(ERR_BADTYPE);
          //TODO: validate hashtable
          if (v->v.dict.next) return val_validate(__val_val(v->v.dict.next));
          return 0;
        case TYPE_REF:
          if (v->v.ref.refcount < 1) return _throw(ERR_BADTYPE);
          if (v->v.ref.refcount > 10000) return _throw(ERR_BADTYPE); //NOTE: this doesn't actually guarantee val is bad, but seems highly unlikely during VM debugging
          return 0;
        case TYPE_FILE:
          if (!v->v.file.f) return _throw(ERR_BADTYPE);
          if (v->v.file.refcount < 1) return _throw(ERR_BADTYPE);
          if (v->v.file.refcount > 10000) return _throw(ERR_BADTYPE); //NOTE: this doesn't actually guarantee val is bad, but seems highly unlikely during VM debugging
        case TYPE_FD:
          if (v->v.fd.fd < 0) return _throw(ERR_BADTYPE);
          if (v->v.fd.refcount < 1) return _throw(ERR_BADTYPE);
          if (v->v.fd.refcount > 10000) return _throw(ERR_BADTYPE); //NOTE: this doesn't actually guarantee val is bad, but seems highly unlikely during VM debugging
          return 0;
        case TYPE_VM:
          return vm_validate(v->v.vm);
        default:
          return _throw(ERR_BADTYPE);
      }
    //case _TAG5:
    default:
      return _throw(ERR_BADTYPE);
  }
  return _throw(ERR_BADTYPE);
}
err_t val_validaten(val_t *p, unsigned int n) {
  err_t e;
  for(;n--;p++) {
    if ((e = val_validate(*p))) return e;
  }
  return 0;
}

err_t val_tostring(val_t *str, val_t val) {
  err_t e;
  if (val_is_str(val)) {
    if ((e = val_clone(str,val))) return e;
    __str_ptr(*str)->type = TYPE_STRING;
    return 0;
  } else {
    if ((e = val_string_init_empty(str))) goto out_e;
    //if ((e = val_sprint(__str_ptr(*str),val))) goto out_str;
    if (0>(e = val_sprintf_(val,__str_ptr(*str),fmt_v))) goto out_str;
    return 0;
out_str:
    val_destroy(*str);
out_e:
    return e;
  }
}

int val_ispush(val_t val) {
  return val_is_int(val) || val_is_double(val) || val_is_list(val) || val_is_string(val);
}

//used when you want the evaluation of a val INSIDE A QUOTATION to result in the original val
// - distinguish from protect, for when you want the evaluation of a BARE val directly on the work stack to result in the original val
//
//wrap a value so it's evaluation (inside a quotation) is the original value
err_t val_qprotect(val_t *val) {
  if (val_ispush(*val) || val_is_code(*val) || val_is_file(*val)) return 0;
  //else if (val_is_code(*val)) return val_code_wrap(val);
  else if (val_is_ident(*val)) return _val_str_rcat_ch(__str_ptr(*val),'\\');
  else {
    err_t e;
    if ((e = val_list_wrap(val))) return e;
    if ((e = val_code_wrap(val))) return e;
    if ((e = _val_lst_rpush(__lst_ptr(*val),__op_val(OP_first)))) return e;
    return 0;
  }
}
//wrap a value so its evaluation (directly on the work stack) is the original value
err_t val_protect(val_t *val) {
  if (val_ispush(*val)) return 0;
  else if (val_is_code(*val) || val_is_file(*val)) return val_code_wrap(val);
  else if (val_is_ident(*val)) return _val_str_rcat_ch(__str_ptr(*val),'\\');
  else {
    err_t e;
    if ((e = val_list_wrap(val))) return e;
    if ((e = val_code_wrap(val))) return e;
    if ((e = _val_lst_rpush(__lst_ptr(*val),__op_val(OP_first)))) return e;
    return 0;
  }
}

int val_as_bool(val_t val) {
  if (val_is_int(val)) return __val_int(val) != 0;
  else if (val_is_double(val)) return __val_dbl(val) != 0;
  else if (val_is_lst(val)) return !_val_lst_empty(__lst_ptr(val));
  else if (val_is_str(val)) return !_val_str_empty(__str_ptr(val));
  //else if (val_is_vm(val)) return !_val_vm_finished(__val_ptr(val));
  else return 0;
}
int val_compare(val_t lhs,val_t rhs) {
  if (val_is_int(lhs)) {
    if (val_is_int(rhs)) {
      return __val_int(lhs) - __val_int(rhs);
    } else if (val_is_double(rhs)) {
      double c = (double)__val_int(lhs) - __val_dbl(rhs);
      return c == 0 ? 0 : (c > 0 ? 1 : -1);
    } else {
      return -1; //type mismatch
    }
  } else if (val_is_double(lhs)) {
    double c = __val_dbl(lhs);
    if (val_is_double(rhs)) {
      c -= __val_dbl(rhs);
    } else if (val_is_int(rhs)) {
      c -= (double)__val_int(rhs);
    } else { //type mismatch
      c = -1;
    }
    return c == 0 ? 0 : (c > 0 ? 1 : -1);
  } else if (val_is_str(lhs) && val_is_str(rhs)) {
    return _val_str_compare(__str_ptr(lhs),__str_ptr(rhs));
  } else if (val_is_lst(lhs) && val_is_lst(rhs)) {
    return _val_lst_compare(__lst_ptr(lhs),__lst_ptr(rhs));
  } else {
    return -1;
  }
}
int val_eq(val_t lhs,val_t rhs) {
  if (val_is_int(lhs)) {
    if (val_is_int(rhs)) {
      return __val_int(lhs) == __val_int(rhs);
    } else if (val_is_double(rhs)) {
      return (double)__val_int(lhs) == __val_dbl(rhs);
    } else {
      return 0; //type mismatch
    }
  } else if (val_is_double(lhs)) {
    if (val_is_double(rhs)) {
      return  __val_dbl(lhs) == __val_dbl(rhs);
    } else if (val_is_int(rhs)) {
      return  __val_dbl(lhs) == (double)__val_int(rhs);
    } else { //type mismatch
      return 0;
    }
  } else if (val_is_str(lhs) && val_is_str(rhs)) {
    return __str_ptr(lhs)->type == __str_ptr(rhs)->type && _val_str_eq(__str_ptr(lhs),__str_ptr(rhs));
  } else if (val_is_lst(lhs) && val_is_lst(rhs)) {
    return __lst_ptr(lhs)->type == __lst_ptr(rhs)->type && _val_lst_eq(__lst_ptr(lhs),__lst_ptr(rhs));
  } else {
    return 0;
  }
}
int val_lt(val_t lhs,val_t rhs) {
  if (val_is_int(lhs)) {
    if (val_is_int(rhs)) {
      return __val_int(lhs) < __val_int(rhs);
    } else if (val_is_double(rhs)) {
      return (double)__val_int(lhs) < __val_dbl(rhs);
    } else {
      return 0; //type mismatch
    }
  } else if (val_is_double(lhs)) {
    if (val_is_double(rhs)) {
      return  __val_dbl(lhs) < __val_dbl(rhs);
    } else if (val_is_int(rhs)) {
      return  __val_dbl(lhs) < (double)__val_int(rhs);
    } else { //type mismatch
      return 0;
    }
  } else if (val_is_str(lhs) && val_is_str(rhs)) {
    return _val_str_lt(__str_ptr(lhs),__str_ptr(rhs));
  } else if (val_is_lst(lhs) && val_is_lst(rhs)) {
    return _val_lst_lt(__lst_ptr(lhs),__lst_ptr(rhs));
  } else {
    return 0;
  }
}
