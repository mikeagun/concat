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

#include "val_bytecode.h"
#include "val_bytecode_internal.h"
#include "val_string_internal.h"
#include "vm.h"
#include "vm_err.h"
#include "vm_debug.h"
#include "val_include.h"

#include "ops.h"
#include <string.h> //memcpy


void val_bytecode_init_handlers(struct type_handlers *h) {
  h->destroy = val_string_destroy;
  h->clone = val_string_clone;
  h->fprintf = val_bytecode_fprintf;
  h->sprintf = val_bytecode_sprintf;
}

int val_bytecode_fprintf(val_t *val,FILE *file, const fmt_t *fmt) {
  err_t r;
  int rlen=0;
  switch(fmt->conversion) {
    case 's': case 'v':
    case 'V':
      if (0 > (r = val_fprint_cstr(file,"bytecode("))) return r; rlen+=r;
      if (0 > (r = _val_string_fprintf_quoted(file,val,fmt))) return r; rlen += r;
      if (0 > (r = val_fprint_cstr(file,")"))) return r; rlen+=r;
      return rlen;
    default:
      return _throw(ERR_BADTYPE);
  }
}
int val_bytecode_sprintf(val_t *val,val_t *buf, const fmt_t *fmt) {
  err_t r;
  int rlen=0;
  switch(fmt->conversion) {
    case 's': case 'v':
    case 'V':
      if (0 > (r = val_sprint_cstr(buf,"bytecode("))) return r; rlen+=r;
      if (0 > (r = _val_string_sprintf_quoted(buf,val,fmt))) return r; rlen += r;
      if (0 > (r = val_sprint_cstr(buf,")"))) return r; rlen+=r;
      return rlen;
    default:
      return _throw(ERR_BADTYPE);
  }
}

void val_bytecode_val_to_string(val_t *val) {
  val->type = TYPE_STRING;
}

void val_bytecode_init(val_t *val) {
  val->type = TYPE_BYTECODE;
  val->val.string.offset = 0;
  val->val.string.len = 0;
  val->val.string.b = NULL;
  VM_DEBUG_VAL_INIT(val);
}

int _bprintf_int(val_t *code, valint_t i) {
  err_t e;
  char *p;
  if (i>=-128 && i<128) {
    const int nbytes = 2; //op + int8
    if ((e = val_string_rextend(code,nbytes,&p))) return e;
    *(p++) = OP_int8;
    *((int8_t*)p) = (int8_t)i;
    return nbytes;
  } else { //TODO: add intermediate integers (8/16 probably mostly sufficient on avr)
    const int nbytes = 9; //op + int64
    if ((e = val_string_rextend(code,nbytes,&p))) return e;
    *(p++) = OP_int64;
    *((int64_t*)p) = i;
    return nbytes;
  }
}

err_t _bprintf_float(val_t *code, valfloat_t f) {
  err_t e;
  const int nbytes = 1 + sizeof(f); //op+sizeof(valfloat_t)
  char *p;
  if ((e = val_string_rextend(code,nbytes,&p))) return e;
  *(p++) = OP_double;
  *((valfloat_t*)p) = f;
  return nbytes;
}

int _bprintf_qstring(val_t *buf, const char *str, uint8_t len) {
  err_t e;
  const int nbytes = 2 + len;
  char *p;
  if ((e = val_string_rextend(buf,nbytes,&p))) return e;
  *(p++) = OP_qstring;
  *((unsigned char*)p++) = (unsigned char)len;
  memcpy(p,str,len); //p += len
  return nbytes;
}

int _bprintf_string(val_t *buf, opcode_t type, const char *str, unsigned int len) {
  err_t e;
  const int nbytes = 1 + _bsprintf__vbyte32(NULL,len) + len;
  char *p;
  if ((e = val_string_rextend(buf,nbytes,&p))) return e;
  *(p++) = type;

  *((unsigned char*)p++) = (unsigned char)len;
  memcpy(p,str,len); //p += len
  return nbytes;
}

//int _bprintf__byte(val_t *buf, uint8_t v) {
//  return val_string_rpushc_(buf,v);
//}

int _bsprintf__str(char *buf, const char *str, unsigned int len) {
  if (!str) return len;
  else {
    memcpy(buf,str,len); //p += len
    return len;
  }
}


int _bprintf__opcode(val_t *buf, opcode_t op) {
  return val_string_rpushc_(buf,op);
}

int _bsprintf__vbyte32(char *buf, uint32_t v) {
  if (buf) {
    int len;
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
















////======== Old Implementation ========
//
//void val_bytecode_init_handlers(struct type_handlers *h) {
//  h->destroy = val_string_destroy;
//  h->clone = val_string_clone;
//  h->fprintf = val_bytecode_fprintf;
//  h->sprintf = val_bytecode_sprintf;
//}
//
//int val_bytecode_fprintf(val_t *val,FILE *file, const fmt_t *fmt) {
//  err_t r;
//  int rlen=0;
//  switch(fmt->conversion) {
//    case 's': case 'v':
//    case 'V':
//      if (0 > (r = val_fprint_cstr(file,"bytecode("))) return r; rlen+=r;
//      if (0 > (r = _val_string_fprintf_quoted(file,val,fmt))) return r; rlen += r;
//      if (0 > (r = val_fprint_cstr(file,")"))) return r; rlen+=r;
//      return rlen;
//    default:
//      return _throw(ERR_BADTYPE);
//  }
//}
//int val_bytecode_sprintf(val_t *val,val_t *buf, const fmt_t *fmt) {
//  err_t r;
//  int rlen=0;
//  switch(fmt->conversion) {
//    case 's': case 'v':
//    case 'V':
//      if (0 > (r = val_sprint_cstr(buf,"bytecode("))) return r; rlen+=r;
//      if (0 > (r = _val_string_sprintf_quoted(buf,val,fmt))) return r; rlen += r;
//      if (0 > (r = val_sprint_cstr(buf,")"))) return r; rlen+=r;
//      return rlen;
//    default:
//      return _throw(ERR_BADTYPE);
//  }
//}
//
//void val_bytecode_val_to_string(val_t *val) {
//  val->type = TYPE_STRING;
//}
//
//void val_bytecode_init(val_t *val) {
//  val->type = TYPE_BYTECODE;
//  val->val.string.offset = 0;
//  val->val.string.len = 0;
//  val->val.string.b = NULL;
//  VM_DEBUG_VAL_INIT(val);
//}
//
//err_t val_bytecode_init_cstr(val_t *val, const char *str) {
//  //err_t r;
//  //if ((r = val_string_init_cstr(val,str))) return r;
//  //val->type = TYPE_BYTECODE;
//  //VM_DEBUG_VAL_INIT(val);
//  //return 0;
//  return _fatal(ERR_FATAL);
//}
////int val_bytecode_init_(val_t *val, char *str, unsigned int len, unsigned int size) {
////  err_t r;
////  if ((r = val_string_init_(val,str,len,size))) return r;
////  val->type = TYPE_BYTECODE;
////  return 0;
////}
//
//err_t val_bytecode_rpush(val_t *code, val_t *val) {
//  err_t r;
//  switch(val->type) {
//    case TYPE_INT: r = _val_bytecode_rpush_int(code,val); break;
//    case TYPE_FLOAT: r = _val_bytecode_rpush_float(code,val); break;
//    case TYPE_NATIVE_FUNC: r = _val_bytecode_rpush_native(code,val); break;
//    case TYPE_STRING:
//    case TYPE_IDENT:
//    case TYPE_BYTECODE: r = _val_bytecode_rpush_stringtype(code,val); break;
//    case TYPE_LIST:
//    case TYPE_CODE: r = _val_bytecode_rpush_listtype(code,val); break;
//
//    default: r = _throw(ERR_NOT_IMPLEMENTED);
//  }
//  return r;
//}
//
//err_t val_bytecode_lpop(val_t *code, val_t *val) {
//  if (val_string_empty(code)) return _throw(ERR_EMPTY);
//  const char *str = _val_string_str(code);
//  const char *ip = str;
//  switch(*(ip++)) {
//    case OP_end:
//    case OP_break:
//      return _throw(ERR_BADESCAPE);
//    case OP_native:
//    case OP_keep_native:
//
//    case OP_int8:
//    case OP_int64:
//    case OP_double:
//    case OP_qstring:
//    case OP_string:
//    case OP_ident:
//    case OP_bytecode:
//    case OP_empty_list:
//    case OP_empty_code:
//    case OP_list:
//    case OP_code:
//
//    case OP_pop:
//    case OP_swap:
//    case OP_dup:
//
//    case OP_empty:
//    case OP_lpop:
//    case OP_lpush:
//    case OP_rpop:
//    case OP_rpush:
//    case OP_first:
//    case OP_ith:
//
//    case OP_add:
//    case OP_sub:
//    case OP_inc:
//    case OP_dec:
//    case OP_print:
//    case OP_eval:
//      return _throw(ERR_NOT_IMPLEMENTED);
//  }
//  code->val.string.offset += (ip-str);
//  code->val.string.len -= (ip-str);
//  return 0;
//}
//
//err_t val_bytecode_cat(val_t *code, val_t *suffix) {
//  return _throw(ERR_NOT_IMPLEMENTED);
//}
//
//
//const unsigned char* _val_bytecode_string(val_t *val, val_t *code, const unsigned char *ip, unsigned int len) {
//  *val = *code;
//  if (1 >= refcount_inc(val->val.string.b->refcount)) return NULL;
//  val->type = TYPE_STRING;
//  val->val.string.offset = (const char*)ip - val->val.string.b->p;
//  val->val.string.len = len;
//  VM_DEBUG_VAL_INIT(val);
//  return ip+len;
//}
//
//const unsigned char* _val_bytecode_ident(val_t *val, val_t *code, const unsigned char *ip, unsigned int len) {
//  *val = *code;
//  if (1 >= refcount_inc(val->val.string.b->refcount)) return NULL;
//  val->type = TYPE_IDENT;
//  val->val.string.offset = (const char*)ip - val->val.string.b->p;
//  val->val.string.len = len;
//  VM_DEBUG_VAL_INIT(val);
//  return ip+len;
//}
//
//const unsigned char* _val_bytecode_bytecode(val_t *val, val_t *code, const unsigned char *ip, unsigned int len) {
//  *val = *code;
//  if (1 >= refcount_inc(val->val.string.b->refcount)) return NULL;
//  val->type = TYPE_BYTECODE;
//  val->val.string.offset = (const char*)ip - val->val.string.b->p;
//  val->val.string.len = len;
//  VM_DEBUG_VAL_INIT(val);
//  return ip+len;
//}
//
//
//int val_bytecode_canpush(val_t *val) {
//  switch(val->type) {
//    case TYPE_INT:
//    case TYPE_FLOAT:
//    case TYPE_IDENT:
//    case TYPE_STRING:
//    case TYPE_BYTECODE:
//    case TYPE_LIST:
//    case TYPE_CODE:
//    case TYPE_NATIVE_FUNC:
//      return 1;
//    default:
//      return 0;
//  }
//}
//
//err_t val_bytecode_rpush_opcode(val_t *code, opcode_t op) {
//  err_t r;
//  char *p;
//  if ((r = val_string_rextend(code,1,&p))) return r;
//  *p = op;
//  return 0;
//}
//
//err_t _val_bytecode_rpush_native(val_t *code, val_t *native) {
//  opcode_t opcode = native->val.func.opcode;
//  
//  if (opcode != OP_end) {
//    return val_bytecode_rpush_opcode(code,opcode);
//  } else {
//    return val_bytecode_rpush_native(code,val_func_f(native),val_func_keep(native));
//  }
//}
//
//err_t val_bytecode_rpush_native(val_t *code, vm_op_handler *op, int keep) {
//  err_t r;
//  const int len = 1 + sizeof(vm_op_handler*);
//  char *p;
//  if ((r = val_string_rextend(code,len,&p))) return r;
//  if (keep) *(p++) = OP_keep_native;
//  else *(p++) = OP_native;
//  *((vm_op_handler**)p) = op; //p += sizeof(op);
//  return 0;
//}
//
//err_t _val_bytecode_rpush_int(val_t *code, val_t *val) {
//  err_t r;
//  valint_t i = val_int(val);
//  if (i>=-128 && i<128) {
//    char *p;
//    if ((r = val_string_rextend(code,2,&p))) return r;
//    *(p++) = OP_int8;
//    *(p++) = (int8_t)i;
//    return 0;
//  } else {
//    const int len = 1 + sizeof(i);
//    char *p;
//    if ((r = val_string_rextend(code,len,&p))) return r;
//    *(p++) = OP_int64;
//    *((int64_t*)p) = i; //p += sizeof(i);
//    return 0;
//  }
//}
//
//err_t _val_bytecode_rpush_float(val_t *code, val_t *val) {
//  err_t r;
//  double f = val_float(val);
//  const int len = 1 + sizeof(f);
//  char *p;
//  if ((r = val_string_rextend(code,len,&p))) return r;
//  *(p++) = OP_double;
//  *((double*)p) = f; //p += sizeof(f);
//  return 0;
//}
//
//err_t _val_bytecode_rpush_listtype(val_t *code, val_t *val) {
//  err_t r;
//  if (val_list_empty(val)) {
//    if (val_iscode(val)) return val_bytecode_rpush_opcode(code,OP_empty_code);
//    else return val_bytecode_rpush_opcode(code,OP_empty_list);
//  } else {
//    if (val_iscode(val)) {
//      if ((r = val_bytecode_rpush_opcode(code,OP_code))) return r;
//    } else {
//      if ((r = val_bytecode_rpush_opcode(code,OP_list))) return r;
//    }
//    unsigned int i,len = val_list_len(val);
//    if ((r = _val_bytecode_rpush_vbyte32(code,len))) return r;
//    for(i=0;i<len;i++) {
//      val_t *v = val_list_get(val,i);
//      switch(v->type) {
//        case TYPE_INT: case TYPE_FLOAT: case TYPE_STRING: case TYPE_IDENT: case TYPE_BYTECODE: case TYPE_LIST: case TYPE_CODE: case TYPE_NATIVE_FUNC:
//          if ((r = val_bytecode_rpush(code,v))) return r;
//          break;
//        default:
//          return _throw(ERR_BADTYPE);
//      }
//    }
//    return 0;
//  }
//}
//
//const unsigned char* _val_bytecode_parse_listtype(unsigned char *ip, val_t *code, val_t *val) {
//  opcode_t opcode = *(ip++);
//  err_t r;
//  switch (opcode) {
//    case OP_code:
//      val_code_init(val);
//      return _val_bytecode_parse_listcontents(ip,code,val);
//    case OP_list:
//      val_list_init(val);
//      return _val_bytecode_parse_listcontents(ip,code,val);
//    default:
//      r = _throw(ERR_BADARGS); //throw for debugging
//      return NULL;
//  }
//}
//
//const unsigned char* _val_bytecode_parse_listcontents(const unsigned char *ip, val_t *code, val_t *val) { //TODO: use stack-manip only version of dobytecode instead
//  err_t r=0;
//  val_t t;
//  unsigned int llen,len; //llen - list len, len - temp var
//  ip = _bytecode_read_vbyte32(ip,&llen);
//
//  while(llen--) {
//    switch(*(ip++)) {
//      case OP_int8: val_int_init(&t,*((int8_t*)ip++)); break;
//      case OP_int64: val_int_init(&t,*((int64_t*)ip)); ip += sizeof(int64_t); break;
//      case OP_double: val_float_init(&t,*((double*)ip)); ip += sizeof(double); break;
//      case OP_qstring: 
//        len = *((unsigned char*)ip++);
//        ip = _val_bytecode_string(&t,code,ip,len);
//        break;
//      case OP_string:
//        ip = _bytecode_read_vbyte32(ip,&len);
//        ip = _val_bytecode_string(&t,code,ip,len);
//        break;
//      case OP_ident:
//        ip = _bytecode_read_vbyte32(ip,&len);
//        ip = _val_bytecode_ident(&t,code,ip,len);
//        break;
//      case OP_bytecode:
//        ip = _bytecode_read_vbyte32(ip,&len);
//        ip = _val_bytecode_bytecode(&t,code,ip,len);
//        break;
//      case OP_empty_list: val_list_init(&t); break;
//      case OP_empty_code: val_list_init(&t); break;
//      case OP_list:
//	val_list_init(&t);
//        ip = _val_bytecode_parse_listcontents(ip,code,&t);
//        break;
//      case OP_code:
//	val_code_init(&t);
//        ip = _val_bytecode_parse_listcontents(ip,code,&t);
//        break;
//      case OP_native: ip = _bytecode_read_op2native(ip,&t,0); break;
//      case OP_keep_native: ip = _bytecode_read_op2native(ip,&t,1); break;
//      case OP_pop: val_func_init_(&t,_op_pop,"pop",OP_pop); break; //TODO: there should be a reverse loopkup table for all of these
//      case OP_swap: val_func_init_(&t,_op_swap,"swap",OP_swap); break;
//      case OP_dup: val_func_init_(&t,_op_dup,"dup",OP_dup); break;
//      case OP_empty: val_func_init_(&t,_op_empty,"empty",OP_empty); break;
//      case OP_lpop: val_func_init_(&t,_op_lpop,"lpop",OP_lpop); break;
//      case OP_lpush: val_func_init_(&t,_op_lpush,"lpush",OP_lpush); break;
//      case OP_rpop: val_func_init_(&t,_op_rpop,"rpop",OP_rpop); break;
//      case OP_rpush: val_func_init_(&t,_op_rpush,"rpush",OP_rpush); break;
//      case OP_first: val_func_init_(&t,_op_first,"first",OP_first); break;
//      case OP_add: val_func_init_(&t,_op_add,"add",OP_add); break;
//      case OP_sub: val_func_init_(&t,_op_sub,"sub",OP_sub); break;
//      case OP_inc: val_func_init_(&t,_op_inc,"inc",OP_inc); break;
//      case OP_dec: val_func_init_(&t,_op_dec,"dec",OP_dec); break;
//      case OP_print: val_func_init_(&t,_op_print,"print",OP_print); break;
//      case OP_eq: val_func_init_(&t,_op_eq,"eq",OP_eq); break;
//      case OP_ne: val_func_init_(&t,_op_ne,"ne",OP_ne); break;
//      case OP_lt: val_func_init_(&t,_op_lt,"lt",OP_lt); break;
//      case OP_le: val_func_init_(&t,_op_le,"le",OP_le); break;
//      case OP_gt: val_func_init_(&t,_op_gt,"gt",OP_gt); break;
//      case OP_ge: val_func_init_(&t,_op_ge,"ge",OP_ge); break;
//      case OP_eval: val_func_init_(&t,_op_eval,"eval",OP_eval); break;
//      case OP_if: val_func_init_(&t,_op_if,"if",OP_if); break;
//      case OP_if_: val_func_init_(&t,_op_if_,"if_",OP_if_); break;
//      case OP_ifelse: val_func_init_(&t,_op_ifelse,"ifelse",OP_ifelse); break;
//      case OP_ifelse_: val_func_init_(&t,_op_ifelse_,"ifelse_",OP_ifelse_); break;
//      case OP_only: val_func_init_(&t,_op_only,"only",OP_only); break;
//      case OP_unless: val_func_init_(&t,_op_unless,"unless",OP_unless); break;
//      default:
//        r = _throw(ERR_BADARGS);
//        return NULL;
//    }
//    if (r || (r = val_list_rpush(val,&t))) { val_destroy(&t); return NULL; }
//  }
//  return ip;
//}
//
//const unsigned char* _bytecode_read_op2native(const unsigned char *ip, val_t *val, int keep) {
//  val->type = TYPE_NATIVE_FUNC;
//  val->val.func.f = *((vm_op_handler**)ip);
//  val->val.func.name = NULL;
//  val->val.func.keep = keep;
//  val->val.func.opcode = OP_end;
//  return ip+sizeof(vm_op_handler*);
//}
//
//const unsigned char* _bytecode_read_op(const unsigned char *ip, vm_op_handler **op) {
//  *op = *((vm_op_handler**)ip);
//  return ip+sizeof(vm_op_handler*);
//}
//
//
//err_t _val_bytecode_rpush_stringtype(val_t *code, val_t *val) {
//  err_t r;
//  unsigned int len = val_string_len(val);
//  if (len == 0) {
//    char *p;
//    if ((r = val_string_rextend(code,2,&p))) return r;
//    switch(val->type) {
//      case TYPE_STRING: *(p++) = OP_qstring; break;
//      case TYPE_IDENT: *(p++) = OP_ident; break;
//      case TYPE_BYTECODE: *(p++) = OP_bytecode; break;
//      default: return _throw(ERR_BADARGS);
//    }
//    *(p++) = 0;
//  } else if (val_isstring(val) && len < 256) {
//    const int nbytes = 2 + len;
//    char *p;
//    if ((r = val_string_rextend(code,nbytes,&p))) return r;
//    *(p++) = OP_qstring;
//    *((unsigned char*)p++) = (unsigned char)len;
//    memcpy(p,_val_string_str(val),len); //p += len
//  } else {
//    const int nbytes = 6 + len; //5 for len is over-estimate, but that way we don't have to do any extra math (and we are only asking for 0-4 extra bytes, so ...)
//    char *p;
//    if ((r = val_string_rextend(code,nbytes,&p))) return r;
//    switch(val->type) {
//      case TYPE_STRING: *(p++) = OP_string; break;
//      case TYPE_IDENT: *(p++) = OP_ident; break;
//      case TYPE_BYTECODE: *(p++) = OP_bytecode; break;
//      default: return _throw(ERR_BADARGS);
//    }
//    p = (char*)_bytecode_write_vbyte32((unsigned char*)p,len);
//    memcpy(p,_val_string_str(val),len); p += len;
//    code->val.string.len = p - _val_string_str(code); //fixup length (since we overestimated for vbyte)
//  }
//  return 0;
//}
//
//err_t val_bytecode_rpush_ith(val_t *code, int i) {
//  if (i < 0 || i > 255) return _throw(ERR_BADARGS);
//  err_t r;
//  char *p;
//  if ((r = val_string_rextend(code,2,&p))) return r;
//  *(p++) = OP_ith;
//  *(p++) = (unsigned char)i;
//  return 0;
//}
//
//err_t _val_bytecode_rpush_char(val_t *code, char c) {
//  err_t r;
//  char *p;
//  if ((r = val_string_rextend(code,1,&p))) return r;
//  *p = c;
//  return 0;
//}
//err_t _val_bytecode_rpush_int32(val_t *code, int32_t i) {
//  err_t r;
//  char *p;
//  if ((r = val_string_rextend(code,4,&p))) return r;
//  *((int32_t*)p) = i;
//  return 0;
//}
//err_t _val_bytecode_rpush_vbyte32(val_t *code, uint32_t v) {
//  char buf[5];
//  int len;
//  if (v < (1<<7)) {
//    buf[0] = (uint8_t)v;
//    len=1;
//  } else if (v < ((uint32_t)1<<14)) {
//    buf[0] = (uint8_t)(v|128); v >>= 7;
//    buf[1] = (uint8_t)v;
//    len=2;
//  } else if (v < ((uint32_t)1<<21)) {
//    buf[0] = (uint8_t)(v|128); v >>= 7;
//    buf[1] = (uint8_t)(v|128); v >>= 7;
//    buf[2] = (uint8_t)v;
//    len=3;
//  } else if (v < ((uint32_t)1<<28)) {
//    buf[0] = (uint8_t)(v|128); v >>= 7;
//    buf[1] = (uint8_t)(v|128); v >>= 7;
//    buf[2] = (uint8_t)(v|128); v >>= 7;
//    buf[3] = (uint8_t)v;
//    len=4;
//  } else {
//    buf[0] = (uint8_t)(v|128); v >>= 7;
//    buf[1] = (uint8_t)(v|128); v >>= 7;
//    buf[2] = (uint8_t)(v|128); v >>= 7;
//    buf[3] = (uint8_t)(v|128); v >>= 7;
//    buf[4] = (uint8_t)v;
//    len=5;
//  }
//  err_t r;
//  char *p;
//  if ((r = val_string_rextend(code,len,&p))) return r;
//  memcpy(p,buf,len);
//  return 0;
//}
//
//const unsigned char* _bytecode_read_vbyte32(const unsigned char *ip, uint32_t *v) {
//  uint8_t t;
//  t=*(ip++);
//  *v = (uint32_t)(t&127);
//  if (t & 0x80) {
//    t=*(ip++);
//    *v |= (uint32_t)(t&127) << 7;
//    if (t & 0x80) {
//      t=*(ip++);
//      *v |= (uint32_t)(t&127) << 14;
//      if (t & 0x80) {
//	t=*(ip++);
//	*v |= (uint32_t)(t&127) << 21;
//	if (t & 0x80) {
//	  t=*(ip++);
//	  *v |= (uint32_t)(t&127) << 28;
//	}
//      }
//    }
//  }
//  return ip;
//}
//
//unsigned char* _bytecode_write_vbyte32(unsigned char *ip, uint32_t v) {
//  if (v < (1<<7)) {
//    *(ip++) = (uint8_t)v;
//  } else if (v < ((uint32_t)1<<14)) {
//    *(ip++) = (uint8_t)(v|128); v >>= 7;
//    *(ip++) = (uint8_t)v;
//  } else if (v < ((uint32_t)1<<21)) {
//    *(ip++) = (uint8_t)(v|128); v >>= 7;
//    *(ip++) = (uint8_t)(v|128); v >>= 7;
//    *(ip++) = (uint8_t)v;
//  } else if (v < ((uint32_t)1<<28)) {
//    *(ip++) = (uint8_t)(v|128); v >>= 7;
//    *(ip++) = (uint8_t)(v|128); v >>= 7;
//    *(ip++) = (uint8_t)(v|128); v >>= 7;
//    *(ip++) = (uint8_t)v;
//  } else {
//    *(ip++) = (uint8_t)(v|128); v >>= 7;
//    *(ip++) = (uint8_t)(v|128); v >>= 7;
//    *(ip++) = (uint8_t)(v|128); v >>= 7;
//    *(ip++) = (uint8_t)(v|128); v >>= 7;
//    *(ip++) = (uint8_t)v;
//  }
//  return ip;
//}
//
