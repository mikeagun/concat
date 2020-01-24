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

#include "ops_file.h"
#include "ops_internal.h"

#include "val_file.h"
#include "val_string.h"
#include "val_num.h"
#include "vm_err.h"

#include "ops.h"

err_t ops_file_init(vm_t *vm) {
  err_t r=0;
  if ((r = vm_init_op(vm,"include",_op_include))) return r;
  if ((r = vm_init_op(vm,"openr",_op_openr))) return r;
  if ((r = vm_init_op(vm,"open",_op_open))) return r;
  if ((r = vm_init_op(vm,"stdin",_op_stdin))) return r;
  if ((r = vm_init_op(vm,"stdout",_op_stdout))) return r;
  if ((r = vm_init_op(vm,"stderr",_op_stderr))) return r;
  if ((r = vm_init_op(vm,"readline",_op_readline))) return r;
  if ((r = vm_init_op(vm,"stdin.readline",_op_stdin_readline))) return r;
  if ((r = vm_init_op(vm,"read",_op_read))) return r;
  if ((r = vm_init_op(vm,"writeline",_op_writeline))) return r;
  if ((r = vm_init_op(vm,"write",_op_write))) return r;
  if ((r = vm_init_op_keep(vm,"eachline",_op_eachline))) return r;
  if ((r = vm_init_compile_op(vm,"isfeof","-2 ="))) return r;
  if ((r = vm_init_compile_op(vm,"isferr","0 <"))) return r;
  //if ((r = vm_init_compile_op(vm,"eachline","[readline 0 ge] swap while"))) return r;
  return r;
}

err_t _op_include(vm_t *vm) { //`openr eval` for strings, or `eval` for file vals
  __op_pop1; // "filename" OR file
  __op_wpush1;
  if (val_isstring(p)) {
    err_t e;
    if ((e = val_file_init(q,val_string_cstr(p),"r"))) return e;
    val_destroy(p);
    return 0;
  } else if (val_isfile(p)) {
    val_move(q,p);
    return 0;
  } else return _throw(ERR_BADARGS);
}

err_t _op_openr(vm_t *vm) {
  __op_get1; // "filename"
  throw_if(ERR_BADARGS,!val_isstring(p));
  val_t t;
  err_t e;

  if ((e = val_file_init(&t,val_string_cstr(p),"r"))) goto bad;
  val_swap(p,&t);
bad:
  val_destroy(&t);
  return e;
}

err_t _op_open(vm_t *vm) {
  __op_popget; // "filename" "mode"
  throw_if(ERR_BADARGS,!val_isstring(p) || !val_isstring(p+1));
  val_t t;
  err_t e;

  if ((e = val_file_init(&t,val_string_cstr(p),val_string_cstr(p+1)))) return e;
  val_destroy(p);
  val_destroy(p+1);
  val_move(p,&t);
  return e;
}

err_t _op_stdin(vm_t *vm) { __op_push1; val_file_init_stdin(p); return 0; }
err_t _op_stdout(vm_t *vm) { __op_push1; val_file_init_stdout(p); return 0; }
err_t _op_stderr(vm_t *vm) { __op_push1; val_file_init_stderr(p); return 0; }

err_t _op_read(vm_t *vm) {
  __op_get2; // "file" nbytes
  throw_if(ERR_BADARGS,!val_isfile(p));
  __op_take_iarg(p,n,n>=0);
  int len;
  if (0>(len = val_file_read(p,p+1,n))) {
    val_int_init(p+1,len);
  }
  return 0;
}

err_t _op_readline(vm_t *vm) {
  __op_getpush; // "file"
  throw_if(ERR_BADARGS,!val_isfile(p));
  int len;
  if (0>(len = val_file_readline(p,p+1))) {
    val_int_init(p+1,len);
  }
  return 0;
}

err_t _op_stdin_readline(vm_t *vm) {
  __op_push1;
  val_t t;
  int len = val_file_readline(val_file_init_stdin(&t),p);
  //val_destroy(&t); //not needed because stdin doesn't close normally
  if (0>len) {
    val_int_init(p,len);
  }
  return 0;
}

err_t _op_writeline(vm_t *vm) {
  __op_popget; // file "line"
  return val_file_writeline(p,p+1);
}

err_t _op_write(vm_t *vm) {
  __op_popget; // file "string"
  return val_file_write(p,p+1);
}

err_t _op_eachline(vm_t *vm) {
  __op_popget; // file [body]
  throw_if(ERR_BADARGS,!val_isfile(p));

  err_t e;
  val_t t;
  int len = val_file_readline(p,&t);
  if (0>len) { //EOF or err, need cleanup
    val_destroy(p+1); //destroy body
    val_int_replace(p,len); //replace file with return code
  } else {
    val_t *q;
    if ((e = vm_wextend_(vm,3,&q))) goto bad;
    val_move(q,p); //move file to w
    val_move(p,&t); //put line on stack <-- done with t after here
    val_move(q+2,p+1); //wpush body for eval
    if ((e = val_clone(q+1,q+2))) return e; //clone body for next
    if ((e = val_wprotect(q+1))) return e; //protect body for next
  }
  return 0;
bad:
  val_destroy(&t);
  return e;
}

