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

#include "ops_printf.h"
#include "val_printf.h"
#include "val_string.h"
#include "ops_internal.h"

//TODO: consider simplifying and treating all lists for printf as stack (remove isstack, avoid stacklist_*)

err_t ops_printf_init(vm_t *vm) {
  err_t e;
  //if (!r) r = vm_init_compile_op(vm,"printf","[ dup '%' find dup 0 ge ] [ splitn swap print_ 2 splitn swap second dup '%vV_' swap find inc dup [ ( [ print_ ] [ pop \\print_ dip ] [ pop \\print_code_ dip ] [ pop popd ] ) swap nth eval ] [ pop '%' print_ print_ ] ifelse ] while pop print_");
  if ((e = vm_init_op(vm,"printf",_op_printf))) goto bad;
  if ((e = vm_init_op(vm,"printf_",_op_printf_))) goto bad;
  if ((e = vm_init_op(vm,"sprintf",_op_sprintf))) goto bad;
  if ((e = vm_init_op(vm,"printlf",_op_printlf))) goto bad;
  if ((e = vm_init_op(vm,"printlf_",_op_printlf_))) goto bad;
  if ((e = vm_init_op(vm,"sprintlf",_op_sprintlf))) goto bad;
  if ((e = vm_init_op(vm,"printlf2",_op_printlf2))) goto bad;
  if ((e = vm_init_op(vm,"printlf2_",_op_printlf2_))) goto bad;
  if ((e = vm_init_op(vm,"sprintlf2",_op_sprintlf2))) goto bad;

bad:
  return e;
}

err_t _op_printf(vm_t *vm) {
  __op_pop1;
  throw_if(ERR_BADARGS,!val_isstring(p));
  val_t fmt;
  val_move(&fmt,p); //need to move fmt out in case printf pushes (e.g. %n)

  err_t r;
  r = val_fprintfv(stdout, &fmt, vm->open_list,1, vm->open_list,1);
  if (r>0) r=0;
  val_destroy(&fmt);
  return r;
}

err_t _op_printf_(vm_t *vm) {
  __op_pop1;
  throw_if(ERR_BADARGS,!val_isstring(p));
  val_t fmt;
  val_move(&fmt,p);

  err_t r;
  r = val_sprintfv(NULL, &fmt,vm->open_list,1, vm->open_list,1);
  if (r>0) r=0;
  val_destroy(&fmt);
  return r;
}

err_t _op_sprintf(vm_t *vm) {
  __op_pop1;
  throw_if(ERR_BADARGS,!val_isstring(p));
  val_t fmt,buf;
  val_move(&fmt,p);

  err_t r;
  val_string_init(&buf);
  if (0 > (r = val_sprintfv(&buf, &fmt,vm->open_list,1, vm->open_list,1))) goto out_buf;
  if ((r = vm_push(vm,&buf))) goto out_buf;
  goto out_fmt;

out_buf:
  val_destroy(&buf);
out_fmt:
  val_destroy(&fmt);
  return r;
}

err_t _op_printlf(vm_t *vm) {
  __op_popget;
  throw_if(ERR_BADARGS,!val_isstring(p+1) || !val_islisttype(p));

  err_t r;
  if (0>(r = val_printfv(p+1,p,0))) return r;
  val_destroy(p+1);
  return 0;
}

err_t _op_printlf_(vm_t *vm) {
  __op_popget;
  throw_if(ERR_BADARGS,!val_isstring(p+1) || !val_islisttype(p));

  err_t r;
  if (0>(r = val_sprintfv(NULL,p+1,p,0,p,0))) return r;
  val_destroy(p+1);
  return 0;
}

err_t _op_sprintlf(vm_t *vm) {
  __op_get2;
  throw_if(ERR_BADARGS,!val_isstring(p+1) || !val_islisttype(p));

  err_t r;
  val_t buf;
  val_string_init(&buf);
  if (0 > (r = val_sprintfv(&buf,p+1,p,0,p,0))) goto bad_buf;
  val_swap(p+1,&buf);
  val_destroy(&buf);
  return 0;

bad_buf:
  val_destroy(&buf);
  return r;
}

err_t _op_printlf2(vm_t *vm) {
  __op_popget2;
  throw_if(ERR_BADARGS,!val_isstring(p+2) || !val_islisttype(p+1) || !val_islisttype(p));

  err_t r;
  if (0 > (r = val_fprintfv(stdout,p+2,p,0,p+1,0))) return r;
  val_destroy(p+2);
  return 0;
}

err_t _op_printlf2_(vm_t *vm) {
  __op_popget2;
  throw_if(ERR_BADARGS,!val_isstring(p+2) || !val_islisttype(p+1) || !val_islisttype(p));

  err_t r;
  if (0 > (r = val_sprintfv(NULL,p+2,p,0,p+1,0))) return r;
  val_destroy(p+2);
  return 0;
}

err_t _op_sprintlf2(vm_t *vm) {
  __op_get3;
  throw_if(ERR_BADARGS,!val_isstring(p+2) || !val_islisttype(p+1) || !val_islisttype(p));

  err_t r;
  val_t buf;
  val_string_init(&buf);
  if (0 > (r = val_sprintfv(&buf,p+2,p,0,p+1,0))) goto bad_buf;
  val_swap(p+2,&buf);
  val_destroy(&buf);
  return 0;

bad_buf:
  val_destroy(&buf);
  return r;
}

