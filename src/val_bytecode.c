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

#include "val_bytecode.h"
#include "val_string.h"
#include "val_list.h"
#include "val_printf.h"
#include "opcodes.h"

val_t val_empty_bytecode() { return _strval_alloc(TYPE_BYTECODE); }
err_t val_bytecode_init_empty(val_t *v) { return _strval_init(v,TYPE_BYTECODE); }

int val_bytecode_fprintf(valstruct_t *v,FILE *file, const struct printf_fmt *fmt) {
  int r,rlen = 0;
  if (0>(r = val_fprint_cstr(file,"bytecode("))) return r; rlen += r;
  if (0>(r = _val_str_fprint_quoted(v,file,fmt->precision))) return r; rlen += r;
  if (0>(r = val_fprint_cstr(file,")"))) return r; rlen += r;
  return rlen;
}

int val_bytecode_sprintf(valstruct_t *v,valstruct_t *buf, const struct printf_fmt *fmt) {
  int r,rlen = 0;
  if (0>(r = val_sprint_cstr(buf,"bytecode("))) return r; rlen += r;
  if (0>(r = _val_str_sprint_quoted(v,buf,fmt->precision))) return r; rlen += r;
  if (0>(r = val_sprint_cstr(buf,")"))) return r; rlen += r;
  return rlen;
}

err_t bytecode_rpush_int32(valstruct_t *b, int32_t i) {
  err_t e;
  char *p;
  if (i>=-128 && i<128) {
    const int nbytes = 2; //op + int8
    if ((e = _val_str_rextend(b,nbytes,&p))) return e;
    *(p++) = (char)TYPECODE_int8;
    *((int8_t*)p) = (int8_t)i;
    return nbytes;
  } else {
    const int nbytes = 5; //op + int8
    if ((e = _val_str_rextend(b,nbytes,&p))) return e;
    *(p++) = (char)TYPECODE_int32;
    *((int32_t*)p) = i;
    return nbytes;
  //} else { //TODO: add intermediate integers (8/16 probably mostly sufficient on avr) -- OR use varbyte for everything else
  //  const int nbytes = 9; //op + int64
  //  if ((e = _val_str_rextend(b,nbytes,&p))) return e;
  //  *(p++) = (char)TYPECODE_int64;
  //  *((int64_t*)p) = i;
  //  return nbytes;
  }

}

//TODO: normalize float bit layout for architectures where needed
err_t bytecode_rpush_dbl(valstruct_t *b, double f) {
  err_t e;
  const int nbytes = 1 + sizeof(f); //op+sizeof(double)
  char *p;
  if ((e = _val_str_rextend(b,nbytes,&p))) return e;
  *(p++) = (char)TYPECODE_float;
  *((double*)p) = f;
  return nbytes;
}

err_t _bytecode_rpush_qstr(valstruct_t *b, bytecode_t op, const char *str, unsigned int len) {
  err_t e;
  const int nbytes = 1 + 1 + len; //op+sizeof(double)
  char *p;
  if ((e = _val_str_rextend(b,nbytes,&p))) return e;
  *(p++) = (char)op;
  *((unsigned char*)p++) = (unsigned char)len;
  memcpy(p,str,len);
  return nbytes;
}

err_t _bytecode_rpush_str(valstruct_t *b, bytecode_t op, const char *str, unsigned int len) {
  err_t e;
  const int nbytes = 1 + _bytecode_write_vbyte32(NULL,len) + len; //op+sizeof(double)
  char *p;
  if ((e = _val_str_rextend(b,nbytes,&p))) return e;
  *(p++) = (char)op;
  p += _bytecode_write_vbyte32(p,len);
  memcpy(p,str,len);
  return nbytes;
}

err_t _bytecode_rpush_lst(valstruct_t *b, bytecode_t op, const val_t *vp, unsigned int len) {
  err_t e;
  int nbytes = 1 + _bytecode_write_vbyte32(NULL,len);
  char *p;
  if ((e = _val_str_rextend(b,nbytes,&p))) return e;
  *(p++) = (char)op;
  p += _bytecode_write_vbyte32(p,len);
  for(;len>0;++vp,--len) {
    if ((e = bytecode_rpush(b,*vp))) return e;
    else nbytes += e;
  }
  return nbytes;
}

err_t bytecode_rpush(valstruct_t *b, val_t val) {
  valstruct_t *v;
  if (val_is_double(val)) {
    //err_t e;
    //char *p;
    //const int nbytes=1 + sizeof(double);
    //if ((e = _val_str_rextend(b,nbytes,&p))) return e;
    //*(p++) = (char)TYPECODE_float;
    //*((double*)p) = __val_dbl(val);
    return bytecode_rpush_dbl(b,__val_dbl(val));
  } else {
    err_t e;
    const char *s;
    bytecode_t op;
    //char *p;
    //int nbytes;
    unsigned int len;
    switch(__val_tag(val)) {
      case _OP_TAG:
        if ((e = _val_str_cat_ch(b,__val_op(val)))) return e;
        else return 1;
        break;
      case _INT_TAG:
        return bytecode_rpush_int32(b,__val_int(val));
        break;
      case _STR_TAG:
        v = __str_ptr(val);
        len = v->v.str.len;
        s = _val_str_begin(v);

        switch(v->type) {
          case TYPE_STRING:
            if (_val_str_len(v) < 256) {
              return _bytecode_rpush_qstr(b,TYPECODE_qstring,s,len);
            } else {
              return _bytecode_rpush_str(b,TYPECODE_string,s,len);
            }
            return 0;
          case TYPE_IDENT:
            if (_val_str_len(v) < 256) {
              return _bytecode_rpush_qstr(b,TYPECODE_qident,s,len);
            } else {
              return _bytecode_rpush_str(b,TYPECODE_ident,s,len);
            }
            return 0;
          case TYPE_BYTECODE:
            return _bytecode_rpush_str(b,TYPECODE_bytecode,s,len);
          default:
            return _throw(ERR_BADTYPE);
        }
        break;
      case _LST_TAG:
        v = __lst_ptr(val);
        len = v->v.lst.len;

        switch(v->type) {
          case TYPE_LIST:
            op = TYPECODE_list;
            break;
          case TYPE_CODE:
            op = TYPECODE_code;
            break;
          default:
            return _throw(ERR_BADTYPE);
        }
        return _bytecode_rpush_lst(b,op,_val_lst_begin(v),len);
      case _VAL_TAG:
        v = __val_ptr(val);
        return _throw(ERR_NOT_IMPLEMENTED);

        switch(v->type) {
          case TYPE_DICT:
          case TYPE_REF:
          case TYPE_FILE:
          case TYPE_VM:
            return _throw(ERR_NOT_IMPLEMENTED);
          default:
            return _throw(ERR_BADTYPE);
        }
      //case _TAG5:
      default:
        return _throw(ERR_BADTYPE);
    }
    return _throw(ERR_NOT_IMPLEMENTED);
  }
}
err_t bytecode_lpop(valstruct_t *b, val_t *val) {
  if (_val_str_empty(b)) return _throw(ERR_EMPTY);
  char *p = _val_str_begin(b);
  bytecode_t op = (bytecode_t)(*p);
  if (op < N_OPS) {
    *val = __op_val(op);
    b->v.str.off += 1;
    b->v.str.len -= 1;
    return 0;
  } else {
    err_t e;
    uint32_t len;
    unsigned int n;
    valstruct_t *v;
    val_t *vp;
    //TODO: should I use computed goto with array of labels, or switch statment?
    ++p;
    switch(op) {
      case TYPECODE_int8:
        *val = __int_val((int8_t)*p);
        b->v.str.off += 2;
        b->v.str.len -= 2;
        break;
      case TYPECODE_int32:
        *val = __int_val((int32_t)*p);
        b->v.str.off += 5;
        b->v.str.len -= 5;
        break;
      case TYPECODE_float:
        *val = __dbl_val((double)*p);
        b->v.str.off += 9;
        b->v.str.len -= 9;
        break;
      case TYPECODE_string:
        n = _bytecode_read_vbyte32(p,&len);
        if ((e = _val_str_substr_clone(val,b,n+1,len))) return e;
        __str_ptr(*val)->type = TYPE_STRING;
        b->v.str.off += 1+n+len;
        b->v.str.len -= 1+n+len;
        break;
      case TYPECODE_qstring:
        len = (unsigned char)*p;
        if ((e = _val_str_substr_clone(val,b,2,len))) return e;
        __str_ptr(*val)->type = TYPE_STRING;
        b->v.str.off += 2+len;
        b->v.str.len -= 2+len;
        break;
      case TYPECODE_ident:
        n = _bytecode_read_vbyte32(p,&len);
        if ((e = _val_str_substr_clone(val,b,n+1,len))) return e;
        __str_ptr(*val)->type = TYPE_IDENT;
        b->v.str.off += 1+n+len;
        b->v.str.len -= 1+n+len;
        break;
      case TYPECODE_qident:
        len = (unsigned char)*p;
        if ((e = _val_str_substr_clone(val,b,2,len))) return e;
        __str_ptr(*val)->type = TYPE_IDENT;
        b->v.str.off += 2+len;
        b->v.str.len -= 2+len;
        break;
      case TYPECODE_bytecode:
        n = _bytecode_read_vbyte32(p,&len);
        if ((e = _val_str_substr_clone(val,b,n+1,len))) return e;
        b->v.str.off += 1+n+len;
        b->v.str.len -= 1+n+len;
        break;
      case TYPECODE_list:
        return _throw(ERR_NOT_IMPLEMENTED);
        n = _bytecode_read_vbyte32(p,&len);
        *val = val_empty_list();
        v = __val_ptr(*val);
        if ((e = _val_lst_rreserve(v,len))) return e;

        b->v.str.off += 1+n;
        b->v.str.len -= 1+n;
        while(len--) {
          if ((e = bytecode_lpop(b,vp++))) return e;
        }
        return 0;
      case TYPECODE_code:
        return _throw(ERR_NOT_IMPLEMENTED);
        n = _bytecode_read_vbyte32(p,&len);
        *val = val_empty_code();
        v = __val_ptr(*val);
        if ((e = _val_lst_rextend(v,len,&vp))) return e;

        b->v.str.off += 1+n;
        b->v.str.len -= 1+n;
        while(len--) {
          if ((e = bytecode_lpop(b,vp++))) return e;
        }
        return 0;
      case TYPECODE_dict:
      case TYPECODE_ref:
      case TYPECODE_file:
      case TYPECODE_vm:
        return _throw(ERR_NOT_IMPLEMENTED);
      default:
        return _throw(ERR_BADTYPE);
    }
    return 0;
  }
}

unsigned int _bytecode_write_vbyte32(char *buf, uint32_t v) {
  if (buf) {
    unsigned int len;
    if (v < (1<<7)) {
      buf[0] = (uint8_t)v;
      len=1;
    } else if (v < ((uint32_t)1<<14)) {
      buf[0] = (uint8_t)(v|128); v >>= 7;
      buf[1] = (uint8_t)v;
      len=2;
    } else if (v < ((uint32_t)1<<21)) {
      buf[0] = (uint8_t)(v|128); v >>= 7;
      buf[1] = (uint8_t)(v|128); v >>= 7;
      buf[2] = (uint8_t)v;
      len=3;
    } else if (v < ((uint32_t)1<<28)) {
      buf[0] = (uint8_t)(v|128); v >>= 7;
      buf[1] = (uint8_t)(v|128); v >>= 7;
      buf[2] = (uint8_t)(v|128); v >>= 7;
      buf[3] = (uint8_t)v;
      len=4;
    } else {
      buf[0] = (uint8_t)(v|128); v >>= 7;
      buf[1] = (uint8_t)(v|128); v >>= 7;
      buf[2] = (uint8_t)(v|128); v >>= 7;
      buf[3] = (uint8_t)(v|128); v >>= 7;
      buf[4] = (uint8_t)v;
      len=5;
    }
    return len;
  } else {
    if (v < (1<<7)) return 1;
    else if (v < ((uint32_t)1<<14)) return 2;
    else if (v < ((uint32_t)1<<21)) return 3;
    else if (v < ((uint32_t)1<<28)) return 4;
    else return 5;
  }
}

unsigned int _bytecode_read_vbyte32(const char *ip, uint32_t *v) {
  uint8_t t;
  unsigned int n=1;
  t=(unsigned char)*ip;
  *v = (uint32_t)(t&127);
  if (t & 0x80) {
    t=(unsigned char)ip[n++];
    *v |= (uint32_t)(t&127) << 7;
    if (t & 0x80) {
      t=(unsigned char)ip[n++];
      *v |= (uint32_t)(t&127) << 14;
      if (t & 0x80) {
        t=(unsigned char)ip[n++];
	*v |= (uint32_t)(t&127) << 21;
	if (t & 0x80) {
          t=(unsigned char)ip[n++];
	  *v |= (uint32_t)(t&127) << 28;
	}
      }
    }
  }
  return n;
}
