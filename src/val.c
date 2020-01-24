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

#include "val.h"
#include "val_num.h"
#include "val_cast.h"
#include "val_string.h"
#include "val_bytecode.h"
#include "val_file_internal.h" //for printing (should be moved to val_file.c)
#include "vm_debug.h"
#include "vm_err.h"
#include "val_printf.h"
#include "val_func.h"
#include "ops.h"

#include "string.h"
#include <stdlib.h>

val_t* valcpy(val_t *dst, val_t *src, size_t n) {
  return (val_t*)memcpy(dst,src,sizeof(val_t)*n);
}
val_t* valmove(val_t *dst, val_t *src, size_t n) {
  return (val_t*)memmove(dst,src,sizeof(val_t)*n);
}
err_t valcln(val_t *dst, val_t *src, size_t n) {
  size_t i;
  for(i=0;i<n;++i) {
    err_t r;
    if ((r=val_clone(dst+i,src+i))) return _fatal(r);
  }
  return 0;
}
err_t valdestroy(val_t *p, size_t n) {
  for(;n;p++,n--) {
    val_destroy(p);
  }
  return 0;
}
void val_clear(val_t *p) {
  p->type = VAL_INVALID;
}
void valclr(val_t *p, unsigned int len) {
  while(len--) {
    p->type = VAL_INVALID;
    ++p;
  }
}


err_t val_basic_clone(val_t *ret, val_t *orig) {
  *ret = *orig;
  return 0;
}
err_t val_basic_destroy(val_t *val) {
  val->type = VAL_INVALID;
  return 0;
}

err_t val_basic_print(val_t *val) { 
  switch(val->type) {
    case TYPE_NULL:
      printf("NULL");
      break;
    case TYPE_INT: //integer number (stored as valint_t)
      printf("%ld",val_int(val));
      break;
    case TYPE_FLOAT: //floating point number (stored as double)
      printf("%f",val_float(val));
      break;
    case TYPE_HASH:
      printf("{hash}");
      break;
    case TYPE_VM:
      printf("{hash}");
      break;
    case TYPE_NATIVE_FUNC: //built-in
      if (val->val.func.name) printf("native(%s)",val->val.func.name);
      else printf("native(%p)",val->val.func.f);
      break;
    case TYPE_FILE:
    case TYPE_REF:
    case TYPE_BYTECODE:
    case TYPE_STRING:
    case TYPE_IDENT:
    case TYPE_LIST:
    case TYPE_CODE:
    default:
      return -1;
  }
  return 0;
}

extern void val_list_init_handlers(struct type_handlers *h);
extern void val_code_init_handlers(struct type_handlers *h);
extern void val_string_init_handlers(struct type_handlers *h);
extern void val_ident_init_handlers(struct type_handlers *h);
extern void val_bytecode_init_handlers(struct type_handlers *h);
extern void val_int_init_handlers(struct type_handlers *h);
extern void val_float_init_handlers(struct type_handlers *h);
extern void val_file_init_handlers(struct type_handlers *h);
extern void val_hash_init_handlers(struct type_handlers *h);
extern void val_ref_init_handlers(struct type_handlers *h);
extern void val_vm_init_handlers(struct type_handlers *h);
extern void val_func_init_handlers(struct type_handlers *h);

int val_null_fprintf(val_t *val, FILE *file, const fmt_t *fmt) {
  return fprintf(file,"NULL");
}
int val_null_sprintf(val_t *val, val_t *buf, const fmt_t *fmt) {
  return val_sprint_cstr(buf,"NULL");
}
void val_null_init_handlers(struct type_handlers *h) {
  h->fprintf = val_null_fprintf;
  h->sprintf = val_null_sprintf;
}
int val_invalid_fprintf(val_t *val, FILE *file, const fmt_t *fmt) {
  return fprintf(file,"INVALID");
}
int val_invalid_sprintf(val_t *val, val_t *buf, const fmt_t *fmt) {
  return val_sprint_cstr(buf,"INVALID");
}
void val_invalid_init_handlers(struct type_handlers *h) {
  h->fprintf = val_invalid_fprintf;
  h->sprintf = val_invalid_sprintf;
}

int val_bad_fprintf(val_t *val, FILE *file, const fmt_t *fmt) {
  return fprintf(file,"BADTYPE(%d)",val->type);
}
int val_bad_sprintf(val_t *val, val_t *buf, const fmt_t *fmt) {
  err_t r,rlen=0;
  if (0>(r = val_sprint_cstr(buf,"BADTYPE("))) return r;
  rlen += r;
  if (0>(r = _val_int_sprintf(val->type,buf,fmt_v))) return r;
  rlen += r;
  if (0>(r = val_sprint_cstr(buf,")"))) return r;
  rlen += r;
  return rlen;
}

void val_init_type_handlers() {
  int i;
  for(i=0; i <= VAL_NTYPES; ++i) {
    type_handlers[i].destroy = val_basic_destroy;
    type_handlers[i].clone = val_basic_clone;
    type_handlers[i].fprintf = val_bad_fprintf;
    type_handlers[i].sprintf = val_bad_sprintf;
  }

  val_null_init_handlers(&type_handlers[TYPE_NULL]);
  val_invalid_init_handlers(&type_handlers[VAL_INVALID]);

  val_list_init_handlers(&type_handlers[TYPE_LIST]);
  val_code_init_handlers(&type_handlers[TYPE_CODE]);
  val_string_init_handlers(&type_handlers[TYPE_STRING]);
  val_ident_init_handlers(&type_handlers[TYPE_IDENT]);
  val_bytecode_init_handlers(&type_handlers[TYPE_BYTECODE]);
  val_int_init_handlers(&type_handlers[TYPE_INT]);
  val_float_init_handlers(&type_handlers[TYPE_FLOAT]);
  val_file_init_handlers(&type_handlers[TYPE_FILE]);
  val_hash_init_handlers(&type_handlers[TYPE_HASH]);
  val_ref_init_handlers(&type_handlers[TYPE_REF]);
  val_vm_init_handlers(&type_handlers[TYPE_VM]);
  val_func_init_handlers(&type_handlers[TYPE_NATIVE_FUNC]);
}

err_t val_clone(val_t *ret, val_t *orig) {
  err_t r = type_handlers[orig->type].clone(ret,orig);
  VM_DEBUG_VAL_CLONE(ret,orig);
  return r;
}
err_t val_destroy(val_t *val) {
  VM_DEBUG_VAL_DESTROY(val);
  if (val_isvalid(val)) return type_handlers[val->type].destroy(val);
  else return 0; //destroy no-op for INVALID
}
void val_move(val_t *ret, val_t *orig) {
  *ret = *orig;
  //set type to an invalid value (to make sure we don't try to use this val again)
  orig->type = VAL_INVALID;
}

void val_null_init(val_t *val) {
  val->type = TYPE_NULL;
}



err_t val_wprotect(val_t *val) {
  if (val_ispush(val)) return 0;
  else if (val_iscode(val)) return val_code_wrap(val);
  else if (val_isident(val)) return val_ident_escape(val);
  else {
    err_t r;
    if ((r = val_list_wrap(val))) return r;
    if ((r = val_code_wrap(val))) return r;
    val_t t;
    r = val_list_rpush(val,val_func_init(&t,_op_expand,"expand"));
    return 0;
  }
}
err_t val_protect(val_t *val) {
  if (val_ispush(val) || val_iscode(val)) return 0;
  //else if (val_iscode(val)) return val_code_wrap(val);
  else if (val_isident(val)) return val_ident_escape(val);
  else {
    err_t r;
    if ((r = val_list_wrap(val))) return r;
    if ((r = val_code_wrap(val))) return r;
    val_t t;
    r = val_list_rpush(val,val_func_init(&t,_op_expand,"expand"));
    return 0;
  }
}

int val_isvalid(val_t *val) {
  return val->type != VAL_INVALID;
}

int val_isnull(val_t *val) {
  return val->type == TYPE_NULL;
}

int val_islisttype(val_t *val) {
  return val->type == TYPE_LIST || val->type == TYPE_CODE;
}

int val_islist(val_t *val) {
  return val->type == TYPE_LIST;
}

int val_iscode(val_t *val) {
  return val->type == TYPE_CODE;
}

int val_isnumber(val_t *val) {
  return val->type == TYPE_INT || val->type == TYPE_FLOAT;
}

int val_isint(val_t *val) {
  return val->type == TYPE_INT;
}

int val_isfloat(val_t *val) {
  return val->type == TYPE_FLOAT;
}

int val_isstringtype(val_t *val) {
  return val->type == TYPE_STRING || val->type == TYPE_IDENT;
}

int val_isstring(val_t *val) {
  return val->type == TYPE_STRING;
}

int val_isident(val_t *val) {
  return val->type == TYPE_IDENT;
}

int val_isbytecode(val_t *val) {
  return val->type == TYPE_BYTECODE;
}

int val_isfile(val_t *val) {
  return val->type == TYPE_FILE;
}

int val_isfunc(val_t *val) {
  return val->type == TYPE_NATIVE_FUNC;
}

int val_ishash(val_t *val) {
  return val->type == TYPE_HASH;
}

int val_isref(val_t *val) {
  return val->type == TYPE_REF;
}

int val_isvm(val_t *val) {
  return val->type == TYPE_VM;
}

//whether this val is 
int val_ispush(val_t *val) {
  switch(val->type) {
    case TYPE_NULL:
    case TYPE_INT:
    case TYPE_FLOAT:
    case TYPE_STRING:
    case TYPE_LIST:
    case TYPE_HASH:
    case TYPE_REF:
      return 1;
    default:
      return 0;
  }
}

int val_iscoll(val_t *val) {
  switch(val->type) {
    case TYPE_LIST:
    case TYPE_CODE:
    case TYPE_STRING:
      return 1;
    default:
      return 0;
  }
}

