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

#include "ops_system.h"
#include "ops.h"
#include "val_num.h"
#include "vm_err.h"
#include "ops_internal.h"

#include <unistd.h> //sleep
#include <time.h> //time
#include <sys/time.h> //gettimeofday

err_t ops_system_init(vm_t *vm) {
  err_t r;
  if ((r = vm_init_op(vm,"time",_op_time))) return r;
  if ((r = vm_init_op(vm,"time.us",_op_time_us))) return r;
  if ((r = vm_init_op(vm,"sleep",_op_sleep))) return r;
  if ((r = vm_init_op(vm,"usleep",_op_usleep))) return r;
  return 0;
}

err_t _op_time(vm_t *vm) {
  __op_push1;
  time_t t = time(NULL);
  val_int_init(p,t);
  return 0;
}
err_t _op_time_us(vm_t *vm) {
  __op_push1;
  struct timeval tv;
  if (gettimeofday(&tv,NULL)) return _throw(ERR_SYSTEM); //check errno for error
  val_float_init(p,(double)tv.tv_sec + ((double)tv.tv_usec)/1000000);
  return 0;
}
err_t _op_sleep(vm_t *vm) {
  __op_pop1;
  throw_if(ERR_BADARGS,!val_isint(p));
  sleep(val_int(p)); //TODO: check return code
  return 0;
}
err_t _op_usleep(vm_t *vm) {
  __op_pop1;
  throw_if(ERR_BADARGS,!val_isint(p));
  usleep(val_int(p));
  return 0;
}
