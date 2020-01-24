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

#include "val_coll.h"
#include "val_list.h"
#include "val_string.h"
#include "vm_err.h"
#include "val_bytecode.h"

unsigned int val_coll_len(val_t *coll) {
  argcheck(val_iscoll(coll));
  if (val_islisttype(coll)) return val_list_len(coll);
  else return val_string_len(coll);
}
int val_coll_empty(val_t *coll) {
  argcheck(val_iscoll(coll));
  if (val_islisttype(coll)) return val_list_empty(coll);
  else return val_string_empty(coll);
}
int val_coll_small(val_t *coll) {
  argcheck(val_iscoll(coll));
  if (val_islisttype(coll)) return val_list_len(coll)<=1;
  else return val_string_len(coll)<=1;
}

int val_coll_any(val_t *coll) {
  argcheck(val_iscoll(coll));
  if (val_islisttype(coll)) return val_list_any(coll);
  else return val_string_any(coll);
}
int val_coll_all(val_t *coll) {
  argcheck(val_iscoll(coll));
  if (val_islisttype(coll)) return val_list_all(coll);
  else return val_string_all(coll);
}

err_t val_coll_deref(val_t *coll) {
  argcheck_r(val_iscoll(coll));
  if (val_islisttype(coll)) return val_list_deref(coll);
  else return val_string_deref(coll);
}
err_t val_coll_lpush(val_t *coll, val_t *el) {
  argcheck_r(val_iscoll(coll));
  if (val_islisttype(coll)) return val_list_lpush(coll,el);
  else return val_string_lcat(coll,el);
}
err_t val_coll_rpush(val_t *coll, val_t *el) {
  argcheck_r(val_iscoll(coll));
  if (val_islisttype(coll)) return val_list_rpush(coll,el);
  //else if (val_isbytecode(coll)) return val_bytecode_rpush(coll,el);
  else return val_string_cat(coll,el);
}
err_t val_coll_lpop(val_t *coll, val_t *el) {
  argcheck_r(val_iscoll(coll));
  if (val_islisttype(coll)) return val_list_lpop(coll,el);
  //else if (val_isbytecode(coll)) return val_bytecode_lpop(coll,el);
  else return val_string_lpop(coll,el);
}
err_t val_coll_rpop(val_t *coll, val_t *el) {
  argcheck_r(val_iscoll(coll));
  if (val_islisttype(coll)) return val_list_rpop(coll,el);
  else return val_string_rpop(coll,el);
}
err_t val_coll_cat(val_t *coll, val_t *val) {
  argcheck_r(val_iscoll(coll));
  if (val_islisttype(coll) && val_islisttype(val)) return val_list_cat(coll,val);
  //else if (val_isbytecode(coll)) return val_bytecode_cat(coll,val);
  else return val_string_cat(coll,val);
}
err_t val_coll_append(val_t *coll, val_t *val) {
  argcheck_r(val_iscoll(coll));
  if (val_islisttype(coll)) return val_list_append(coll,val);
  else return val_string_cat(coll,val);
}
err_t val_coll_splitn(val_t *coll, val_t *rhs, unsigned int i) {
  argcheck_r(val_iscoll(coll));
  if (val_islisttype(coll)) return val_list_rsplitn(coll,rhs,i);
  else return val_string_rsplitn(coll,rhs,i);
}

err_t val_coll_unwrap(val_t *coll) {
  argcheck_r(val_iscoll(coll));
  if (val_islisttype(coll)) return val_list_unwrap(coll);
  else return val_string_unwrap(coll);
}
void val_coll_clear(val_t *coll) {
  argcheck(val_iscoll(coll));
  if (val_islisttype(coll)) {
    val_list_clear(coll);
  } else { //else string
    val_string_clear(coll);
  }
}

err_t val_coll_ith(val_t *coll, unsigned int i) {
  if (val_islisttype(coll)) return val_list_ith(coll,i);
  else return val_string_ith(coll,i);
}
err_t val_coll_dith(val_t *coll, unsigned int i, val_t *el) {
  if (val_islisttype(coll)) return val_list_dith(coll,i,el);
  else return val_string_dith(coll,i,el);
}
