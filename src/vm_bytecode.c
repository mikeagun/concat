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

#include "vm_bytecode.h"
#include "vm_internal.h"
#include "vm_err.h"
#include "val_coll.h"
#include "val_include.h"
#include "ops.h"
#include "val_bytecode_internal.h"


//threaded bytecode VM for concat
//  - currently implements a subset of the concat core functionality, and is very WIP
//  - we keep a table ops[] of labels to jump directly to code for vm ops which matches enum ordering in enum opcodes
//    - this makes opcodes indexes into the jump table so we can jump directly to the code for the next op when done with the current op

//TODO: right now this is WIP for testing, so very unoptimized, incomplete, and probably erroneous
//TODO: check all "return r" statements for memory leaks
err_t _vm_dobytecode(vm_t *vm) {
  return _throw(ERR_NOT_IMPLEMENTED);
//  const void *ops[] = { OPTABLE }; //OPTABLE comes from val_bytecode_internal, which also defines enum opcode to match
//
//  val_t *w;
//  fatal_if(ERR_FATAL,!(w = _vm_getnext(vm)) || !val_isbytecode(w));
//  const unsigned char *ip = (unsigned char *)val_string_str(w); //ip is our instruction pointer
//
////NEXT jumps immediately to the next opcode handler (i.e. the one at ip)
//#define NEXT goto *ops[*(ip++)]
////#define NEXT do { printf("Coming from %s:%d\n", __FILE__, __LINE__); goto *ops[*(ip++)]; } while(0)
//
//#define decode_t(type) *((type*)ip); ip += sizeof(type)
//#define decode_str(len) (char*)ip; ip += len
//
////FIX_CONT fixes up the bytecode val at the top of wstack before we do something that (may) jump us out of _vm_dobytecode()
////  - this is also where we pop the bytecode if next is OP_end (to support efficient tail calls)
////TODO: this is heavy and called in lots of places. tighten FIX_CONT
////  - per other commits wstack is guaranteed to be singleton, so we could store pointer to offset in bytecode val and update directly on FIX_CONT
//#define FIX_CONT do{ \
//  if (*ip == OP_end) { \
//    if ((r = _vm_popnext(vm,NULL))) return r;\
//  } else { \
//    int tip = ip-(unsigned char *)val_string_str(w); \
//    if ((r = _vm_popnext(vm,&t))) return r; \
//    if ((r = val_string_substring(&t,tip,-1))) return r; \
//    if ((r = vm_wpush(vm,&t))) return r; \
//  } \
//}while(0)
//
//err_t r;
//val_t t;
//vm_op_handler *op; //temp op_native/op_keep_native
//int64_t ti; //temp int
//double tf; //temp float
//unsigned int len; //temp list/string length
//val_t *x, *y; //temp vals
//const char *key; //temp ident
//
//NEXT;
//
////======== Ops for leaving bytecode ========
//op_end:
//  _vm_popnext(vm,NULL);
//  if (!(w = _vm_getnext(vm)) || !val_isbytecode(w)) return 0;
//  ip = (unsigned char *)val_string_str(w);
//  NEXT;
//op_break:
//  FIX_CONT;
//  return 0;
//
////======== Literal ops ========
//
////  - natives are not-portable, so code to transfer between even compiles of concat may not use them
//op_native:
//  op = decode_t(vm_op_handler*);
//
//  FIX_CONT;
//  if ((r = op(vm))) return r;
//  if (!(w = _vm_getnext(vm)) || !val_isbytecode(w)) return 0;
//  ip = (unsigned char *)val_string_str(w);
//  NEXT;
//op_keep_native:
//  op = decode_t(vm_op_handler*);
//
//  FIX_CONT;
//  if ((r = vm_wpush(vm,val_func_init_keep(&t,op,NULL)))) return r;
//  if ((r = op(vm))) return r;
//  if (!(w = _vm_getnext(vm)) || !val_isbytecode(w)) return 0;
//  ip = (unsigned char *)val_string_str(w);
//  NEXT;
//op_int8:
//  ti = decode_t(int8_t);
//  if ((r = vm_push(vm,val_int_init(&t,ti)))) return r;
//  NEXT;
//op_int64:
//  ti = decode_t(int64_t);
//  if ((r = vm_push(vm,val_int_init(&t,ti)))) return r;
//  NEXT;
//op_double:
//  tf = decode_t(double);
//  if ((r = vm_push(vm,val_float_init(&t,tf)))) return r;
//  NEXT;
//op_qstring:
//  len = decode_t(unsigned char);
//  ip = _val_bytecode_string(&t,w,ip,len);
//  if ((r = vm_push(vm,&t))) return r;
//
//  NEXT;
//op_string:
//  ip = _bytecode_read_vbyte32(ip,&len);
//  ip = _val_bytecode_string(&t,w,ip,len);
//  if ((r = vm_push(vm,&t))) return r;
//
//  NEXT;
//op_ident:
//  ip = _bytecode_read_vbyte32(ip,&len);
//  key = decode_str(len);
//
//  if (*key == '\\') {
//    key++;
//    len--;
//    _val_bytecode_ident(&t,w,(unsigned char*)key,len);
//    if ((r = vm_push(vm,&t))) return r;
//    NEXT;
//  } else {
//    throw_if(ERR_UNDEFINED,!(x = vm_dict_get_(vm,key,len)));
//
//    if (val_isbytecode(x)) { //if it is bytecode we can stay in dobytecode()
//      if (!val_string_empty(x)) {
//        FIX_CONT;
//        if ((r = val_clone(&t,x))) return r;
//        if ((r = vm_wpush(vm,&t))) return r;
//        w = vm_wtop(vm);
//        ip = (unsigned char *)val_string_str(w);
//      }
//      NEXT;
//    } else {
//      FIX_CONT;
//      if ((r = val_clone(&t,x))) return r;
//      return vm_wpush(vm,&t);
//    }
//  }
//op_bytecode:
//  ip = _bytecode_read_vbyte32(ip,&len);
//  ip = _val_bytecode_bytecode(&t,w,ip,len);
//  if ((r = vm_push(vm,&t))) return r;
//
//  NEXT;
//
//op_empty_list:
//  val_list_init(&t);
//  if ((r = vm_push(vm,&t))) return r;
//  NEXT;
//op_empty_code:
//  val_code_init(&t);
//  if ((r = vm_push(vm,&t))) return r;
//  NEXT;
//op_list:
//  val_list_init(&t);
//  throw_if(ERR_BADARGS,NULL == (ip = _val_bytecode_parse_listcontents(ip,w,&t)));
//  if ((r = vm_push(vm,&t))) return r;
//  NEXT;
//op_code:
//  val_code_init(&t);
//  throw_if(ERR_BADARGS,NULL == (ip = _val_bytecode_parse_listcontents(ip,w,&t)));
//  if ((r = vm_push(vm,&t))) return r;
//  NEXT;
//
////======== Stack ops ========
//op_pop:
//  vm_pop(vm,NULL);
//  NEXT;
//op_swap:
//  throw_if(ERR_MISSINGARGS,!vm_has(vm,2));
//
//  if ((r = vm_deref(vm))) return r;
//  x = val_list_rget(vm->open_list,0);
//  val_swap(x,x-1);
//  NEXT;
//op_dup:
//  throw_if(ERR_EMPTY,!(x = vm_top(vm)));
//  if ((r = val_clone(&t,x))) return r;
//  if ((r = vm_push(vm,&t))) return r;
//  NEXT;
//
////======== Collection ops ========
//op_empty:
//  if ((r = vm_topcoll_deref(vm,&x))) return r;
//  r = val_coll_empty(x);
//  val_destroy(x);
//  val_int_init(x,r);
//  NEXT;
//op_lpop:
//  if ((r = vm_topcoll_deref(vm,&x))) return r;
//  if ((r = val_coll_lpop(x,&t))) return r;
//  val_swap(x,&t);
//  if ((r = vm_push(vm,&t))) return r;;
//  NEXT;
//op_lpush:
//  if ((r = vm_top2_deref(vm,&y,&x))) return r;
//  if ((r = val_coll_lpush(x,y))) return r;
//  val_swap(x,y);
//  vm_pop(vm,NULL);
//  NEXT;
//op_rpop:
//  if ((r = vm_topcoll_deref(vm,&x))) return r;
//  if ((r = val_coll_rpop(x,&t))) return r;
//  val_swap(x,&t);
//  if ((r = vm_push(vm,&t))) return r;;
//  NEXT;
//op_rpush:
//  if ((r = vm_top2_deref(vm,&y,&x))) return r;
//  if ((r = val_coll_rpush(x,y))) return r;
//  val_swap(x,y);
//  vm_pop(vm,NULL);
//  NEXT;
//op_first:
//  if ((r = vm_topcoll_deref(vm,&x))) return r;
//  throw_coll_empty(x);
//
//  if ((r = val_coll_lpop(vm_top(vm),&t))) return r;
//  val_destroy(x);
//  val_move(x,&t);
//  NEXT;
//op_ith: //hardcoded ith (up to i=255)
//  len = *((unsigned char*)ip++);
//  if ((r = vm_topcoll_deref(vm,&x))) return r;
//  throw_if(ERR_BADARGS,len > val_coll_len(x));
//
//  r = val_coll_ith(x,len);
//  NEXT;
//
////======== Math Ops ========
//op_add:
//  if ((r = vm_top2num_deref(vm,&x,&y))) return r;
//  r = val_num_add(x,y);
//  vm_pop(vm,NULL);
//  NEXT;
//op_sub:
//  if ((r = vm_top2num_deref(vm,&x,&y))) return r;
//  r = val_num_sub(x,y);
//  vm_pop(vm,NULL);
//  NEXT;
//op_inc:
//  if ((r = vm_topnum_deref(vm,&x))) return r;
//  val_num_inc(x);
//  NEXT;
//op_dec:
//  if ((r = vm_topnum_deref(vm,&x))) return r;
//  val_num_dec(x);
//  NEXT;
//
////======== Printing Ops ========
//op_print:
//  if ((r = _op_print(vm))) return r;
//  NEXT;
//
////======== Comparison Ops ========
//op_eq:
//  if ((r = vm_top2_deref(vm,&x,&y))) return r;
//  r = val_lt(x,y);
//  val_destroy(x);
//  val_int_init(x,r);
//  vm_pop(vm,NULL);
//  NEXT;
//op_ne:
//  if ((r = vm_top2_deref(vm,&x,&y))) return r;
//  r = !val_eq(x,y);
//  val_destroy(x);
//  val_int_init(x,r);
//  vm_pop(vm,NULL);
//  NEXT;
//op_lt:
//  if ((r = vm_top2_deref(vm,&x,&y))) return r;
//  r = val_lt(x,y);
//  val_destroy(x);
//  val_int_init(x,r);
//  vm_pop(vm,NULL);
//  NEXT;
//op_le:
//  if ((r = vm_top2_deref(vm,&x,&y))) return r;
//  r = !val_lt(y,x);
//  val_destroy(x);
//  val_int_init(x,r);
//  vm_pop(vm,NULL);
//  NEXT;
//op_gt:
//  if ((r = vm_top2_deref(vm,&x,&y))) return r;
//  r = val_lt(y,x);
//  val_destroy(x);
//  val_int_init(x,r);
//  vm_pop(vm,NULL);
//  NEXT;
//op_ge:
//  if ((r = vm_top2_deref(vm,&x,&y))) return r;
//  r = !val_lt(x,y);
//  val_destroy(x);
//  val_int_init(x,r);
//  vm_pop(vm,NULL);
//  NEXT;
//
////======== Combinators ========
//op_eval:
//  FIX_CONT;
//  if ((r = vm_pushsw(vm))) return r;
//  if (!(w = _vm_getnext(vm)) || !val_isbytecode(w)) return 0;
//  ip = (unsigned char *)val_string_str(w);
//  NEXT;
//
//op_if:
//  throw_if(ERR_MISSINGARGS,!(x = vm_get(vm,1)));
//  if (!val_ispush(x)) { //if it is not a push type then we need to eval first
//    FIX_CONT;
//
//    if ((r = vm_wpush(vm,val_func_init(&t,_op_if_,"if_")))) return r; //TODO: push bytecode for if_ instead so we can stay in pure bytecode
//    if ((r = vm_hold1(vm))) return r; //hold body //TODO: figure out the bytecode way to do this
//    if ((r = vm_pushsw(vm))) return r; //push cond to work stack (so we will eval and then test again)
//
//    if (!(w = _vm_getnext(vm)) || !val_isbytecode(w)) return 0;
//    ip = (unsigned char *)val_string_str(w);
//    NEXT;
//  }
//op_if_:
//  throw_if(ERR_MISSINGARGS,!(x = vm_get(vm,1)));
//  if (val_as_bool(x)) {
//    FIX_CONT;
//    if ((r = vm_pushsw(vm))) return r;
//    vm_pop(vm,NULL);
//    if (!(w = _vm_getnext(vm)) || !val_isbytecode(w)) return 0;
//    ip = (unsigned char *)val_string_str(w);
//    NEXT;
//  } else {
//    vm_pop(vm,NULL);
//    vm_pop(vm,NULL);
//    NEXT;
//  }
//op_ifelse:
//  throw_if(ERR_MISSINGARGS,!(x = vm_get(vm,2)));
//  if (!val_ispush(x)) { //if it is code then we need to eval first
//    FIX_CONT;
//    if ((r = vm_wpush(vm,val_func_init(&t,_op_ifelse_,"ifelse_")))) return r; //TODO: figure out the pure bytecode way to do this
//    if ((r = vm_hold1(vm))) return r; //hold else-body
//    if ((r = vm_hold1(vm))) return r; //hold then-body
//    if ((r = vm_pushsw(vm))) return r; //push cond to work stack (so we will eval and then test again)
//
//    if (!(w = _vm_getnext(vm)) || !val_isbytecode(w)) return 0;
//    ip = (unsigned char *)val_string_str(w);
//    NEXT;
//  }
//op_ifelse_:
//  throw_if(ERR_MISSINGARGS,!(x = vm_get(vm,2)));
//  FIX_CONT;
//  if (val_as_bool(x)) { //pop else and pushw then
//    vm_pop(vm,NULL);
//    if ((r = vm_pushsw(vm))) return r;
//  } else { //pushw else, pop then
//    if ((r = vm_pushsw(vm))) return r;
//    vm_pop(vm,NULL);
//  }
//  vm_pop(vm,NULL); //pop condition
//
//  if (!(w = _vm_getnext(vm)) || !val_isbytecode(w)) return 0;
//  ip = (unsigned char *)val_string_str(w);
//  NEXT;
//op_only:
//  throw_if(ERR_MISSINGARGS,!(x = vm_get(vm,1)));
//  if (val_as_bool(x)) {
//    FIX_CONT;
//    if ((r = vm_pushsw(vm))) return r;
//    vm_pop(vm,NULL);
//    if (!(w = _vm_getnext(vm)) || !val_isbytecode(w)) return 0;
//    ip = (unsigned char *)val_string_str(w);
//  } else {
//    vm_pop(vm,NULL);
//  }
//  NEXT;
//op_unless:
//  throw_if(ERR_MISSINGARGS,!(x = vm_get(vm,1)));
//  if (!val_as_bool(x)) {
//    FIX_CONT;
//    if ((r = vm_pushsw(vm))) return r;
//    vm_pop(vm,NULL);
//    if (!(w = _vm_getnext(vm)) || !val_isbytecode(w)) return 0;
//    ip = (unsigned char *)val_string_str(w);
//  } else {
//    vm_pop(vm,NULL);
//  }
//  NEXT;
//}
//
//err_t _vm_rpush_bytecode(vm_t *vm, val_t *buf, val_t *val, int rec, int clone) {
//  err_t r;
//  val_t t,*def,*w;
//  unsigned int i;
//
//  switch(val->type) {
//    case TYPE_NATIVE_FUNC: case TYPE_INT: case TYPE_FLOAT: case TYPE_STRING: case TYPE_LIST:
//      if ((r = val_bytecode_rpush(buf,val))) goto out_val;
//      break;
//    case TYPE_IDENT:
//      if (!val_ident_escaped(val)) {
//        def = vm_dict_getv(vm,val);
//        while(def && val_isident(def)) {
//          def = vm_dict_getv(vm,def);
//        }
//        if (def && !val_iscode(def) && val_bytecode_canpush(def)) {
//          if ((r = val_bytecode_rpush(buf,def))) goto out_val;
//        } else {
//          if ((r = val_bytecode_rpush(buf,val))) goto out_val;
//        }
//      } else {
//        if ((r = val_bytecode_rpush(buf,val))) goto out_val;
//      }
//      break;
//    case TYPE_CODE:
//      for(i = val_list_len(val),w=val_list_get(val,0); i; --i,++w) {
//        switch(w->type) {
//          case TYPE_CODE:
//            if (rec) {
//              val_bytecode_init(&t);
//              if ((r = _vm_rpush_bytecode(vm,&t,w,rec,1))) goto out_t; //TODO: use lpop on list so we can optimize for clone/move case
//              if ((r = val_bytecode_rpush(buf,&t))) goto out_t;
//              val_destroy(&t);
//              break;
//            }
//          case TYPE_INT: case TYPE_FLOAT: case TYPE_STRING: case TYPE_NATIVE_FUNC: case TYPE_LIST:
//            if ((r = val_bytecode_rpush(buf,w))) goto out_val;
//            break;
//          case TYPE_IDENT:
//            if (!val_ident_escaped(w)) {
//              def = vm_dict_getv(vm,w);
//              while(def && val_isident(def)) {
//                def = vm_dict_getv(vm,def);
//              }
//              if (def && !val_iscode(def) && val_bytecode_canpush(def)) {
//                if ((r = val_bytecode_rpush(buf,def))) goto out_t;
//              } else {
//                if ((r = val_bytecode_rpush(buf,w))) goto out_t;
//              }
//            } else {
//              if ((r = val_bytecode_rpush(buf,w))) goto out_val;
//            }
//            break;
//          default:
//            return _throw(ERR_BADTYPE);
//        }
//      }
//      break;
//    default:
//      return _throw(ERR_BADTYPE);
//  }
//  if ((r = val_bytecode_rpush_opcode(buf,OP_end))) goto out_val;
//  if (!clone) val_destroy(val);
//  return 0;
//out_t:
//  val_destroy(&t);
//out_val:
//  if (!clone) val_destroy(val);
//  return r;
//#undef NEXT
//#undef FIX_CONT
}

err_t vm_compile_bytecode(vm_t *vm, val_t *buf, val_t *val, int rec) {
  return _throw(ERR_NOT_IMPLEMENTED);
  err_t r;
  //if ((r = _vm_rpush_bytecode(vm,buf,val,rec,0))) return r;
  return 0;
}

