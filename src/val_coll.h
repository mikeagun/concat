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

#ifndef _VAL_COLL_H_
#define _VAL_COLL_H_ 1
#include "val.h"
unsigned  val_coll_len(val_t *coll);
int val_coll_any(val_t *coll);
int val_coll_all(val_t *coll);

err_t val_coll_deref(val_t *coll);
int val_coll_empty(val_t *coll);
int val_coll_small(val_t *coll);
err_t val_coll_lpush(val_t *coll, val_t *el);
err_t val_coll_rpush(val_t *coll, val_t *el);
err_t val_coll_lpop(val_t *coll, val_t *el);
err_t val_coll_rpop(val_t *coll, val_t *el);
err_t val_coll_cat(val_t *coll, val_t *val);
err_t val_coll_append(val_t *coll, val_t *val);
err_t val_coll_splitn(val_t *coll, val_t *rhs, unsigned int i);
err_t val_coll_unwrap(val_t *coll);
void val_coll_clear(val_t *coll);
err_t val_coll_ith(val_t *coll, unsigned int i);
err_t val_coll_dith(val_t *coll, unsigned int i, val_t *el);

#endif
