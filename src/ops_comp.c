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

#include "ops_comp.h"
#include "ops_internal.h"
#include "val_include.h"
#include "val_bytecode_internal.h"
#include "ops.h"
#include "vm_err.h"

err_t ops_comp_init(vm_t *vm) {
  err_t e; //TODO: proper error checks
  val_t t;
  //comparison operators
  if ((e = vm_init_opcode(vm,"<",_op_lt,OP_lt))) goto out;
  if ((e = vm_init_opcode(vm,">",_op_gt,OP_gt))) goto out;
  if ((e = vm_init_opcode(vm,"=",_op_eq,OP_eq))) goto out;
  if ((e = vm_init_opcode(vm,"lt",_op_lt,OP_lt))) goto out;
  if ((e = vm_init_opcode(vm,"gt",_op_gt,OP_gt))) goto out;
  if ((e = vm_init_opcode(vm,"eq",_op_eq,OP_eq))) goto out;
  if ((e = vm_init_opcode(vm,"le",_op_le,OP_le))) goto out;
  if ((e = vm_init_opcode(vm,"ge",_op_ge,OP_ge))) goto out;
  if ((e = vm_init_opcode(vm,"ne",_op_ne,OP_ne))) goto out;
  if ((e = vm_init_op(vm,"compare",_op_compare))) goto out;

  //boolean operators
  if ((e = vm_init_op_(vm,"true",val_int_init(&t,1)))) goto out;
  if ((e = vm_init_op_(vm,"false",val_int_init(&t,0)))) goto out;
  if ((e = vm_init_op(vm,"bool",_op_bool))) goto out;
  if ((e = vm_init_op(vm,"not",_op_not))) goto out;
  if ((e = vm_init_op(vm,"and_",_op_and_))) goto out;
  if ((e = vm_init_op(vm,"or_",_op_or_))) goto out;
  if ((e = vm_init_op(vm,"and",_op_and))) goto out;
  if ((e = vm_init_op(vm,"or",_op_or))) goto out;

  if ((e = vm_init_op(vm,"swaplt",_op_swaplt))) goto out;
  if ((e = vm_init_op(vm,"swapgt",_op_swapgt))) goto out;

out:
  return e;
}

err_t _op_compare(vm_t *vm) { // compare ( A B -- compare(A,B) )
  __op_popget;
  val_int_replace(p,val_compare(p,p+1));
  val_destroy(p+1);
  return 0;
}
err_t _op_lt(vm_t *vm) {      // lt ( A B -- A<B )
  __op_popget;
  val_int_replace(p,val_lt(p,p+1));
  val_destroy(p+1);
  return 0;
}
err_t _op_le(vm_t *vm) {      // le ( A B -- A<=B )
  __op_popget;
  val_int_replace(p,!val_lt(p+1,p));
  val_destroy(p+1);
  return 0;
}
err_t _op_gt(vm_t *vm) {      // gt ( A B -- A>B )
  __op_popget;
  val_int_replace(p,val_lt(p+1,p));
  val_destroy(p+1);
  return 0;
}
err_t _op_ge(vm_t *vm) {      // ge ( A B -- A>=B )
  __op_popget;
  val_int_replace(p,!val_lt(p,p+1));
  val_destroy(p+1);
  return 0;
}
err_t _op_eq(vm_t *vm) {      // eq ( A B -- A=B )
  __op_popget;
  val_int_replace(p,val_eq(p,p+1));
  val_destroy(p+1);
  return 0;
}
err_t _op_ne(vm_t *vm) {      // ne ( A B -- A!=B )
  __op_popget;
  val_int_replace(p,!val_eq(p,p+1));
  val_destroy(p+1);
  return 0;
}

//boolean operators
err_t _op_bool(vm_t *vm) {     // bool ( A -- bool(A) )
  __op_get1;
  val_int_replace(p,0 != val_as_bool(p));
  return 0;
}
err_t _op_not(vm_t *vm) {     // not ( A -- not(A) )
  __op_get1;
  val_int_replace(p,0 == val_as_bool(p));
  return 0;
}
err_t _op_and_(vm_t *vm) {     // and_ ( A B -- and(A,B) )
  __op_popget;
  val_int_replace(p,val_as_bool(p) && val_as_bool(p+1));
  val_destroy(p+1);
  return 0;
}
err_t _op_or_(vm_t *vm) {      // or_ ( A -- or(A,B) )
  __op_popget;
  val_int_replace(p,val_as_bool(p) || val_as_bool(p+1));
  val_destroy(p+1);
  return 0;
}
err_t _op_and(vm_t *vm) {      // and ( [A] [B] -- A [B] only )
  __op_popget; //A B
  if (val_ispush(p) && val_ispush(p+1)) { //both vals are simple, so we can answer now
    val_int_replace(p,val_as_bool(p) && val_as_bool(p+1));
    val_destroy(p+1);
  } else { //else 1 or both require eval, so eval A, then use only+bool to conditionally eval B and get 1/0
    vm_pop1_(vm,&p); //pop A too
    __op_wpush4;

    err_t e;
    val_move(q+3,p); //eval A first
    val_move(q+2,p);
    if ((e = val_wprotect(q+2))) return e; //protect B (for only)
    val_func_init(q+1,_op_unless,"only"); //pop A and eval B if A
    val_func_init(q,_op_bool,"bool"); //cast final result to bool (1/0)
  }
  return 0;
}
err_t _op_or(vm_t *vm) {       // or ( [A] [B] -- A [B] unless )
  __op_popget; //A B
  if (val_ispush(p) && val_ispush(p+1)) { //both vals are simple, so we can answer now
    val_int_replace(p,val_as_bool(p) || val_as_bool(p+1));
    val_destroy(p+1);
  } else { //else 1 or both require eval, so eval A, then use unless+bool to conditionally eval B and get 1/0
    vm_pop1_(vm,&p); //pop A too
    __op_wpush4;

    err_t e;
    val_move(q+3,p); //eval A first
    val_move(q+2,p);
    if ((e = val_wprotect(q+2))) return e; //protect B (for unless)
    val_func_init(q+1,_op_unless,"unless"); //pop A and eval B if not A
    val_func_init(q,_op_bool,"bool"); //cast final result to bool (1/0)
  }
  return 0;
}

err_t _op_swaplt(vm_t *vm) {
  __op_get2;
  if (val_lt(p,p+1)) val_swap(p,p+1);
  return 0;
}

err_t _op_swapgt(vm_t *vm) {
  __op_get2;
  if (val_lt(p+1,p)) val_swap(p,p+1);
  return 0;
}
