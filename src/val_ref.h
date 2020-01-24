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

#ifndef __VAL_REF_H__
#define __VAL_REF_H__ 1
#include "val.h"

//ref vals are threadsafe refcounted pointers to a single val
//  - used for inter-thread communication
//    - operations protected by lock and support wait/signal/broadcast
//    - unlock handled on cont to ensure as long as
//      a fatal doesn't occur ref will be unlocked on exit of the guarded code
//      (by exception or finishing eval)
//  - at concat level suports the ref/deref/refswap and guard and guard.* ops 

void val_ref_init_handlers(struct type_handlers *h);

err_t val_ref_wrap(val_t *val); //initialize ref by wrapping val in ref
err_t val_ref_unwrap(val_t *ref); //dereference val (so *ref becomes copy of reffered object)
err_t val_ref_swap(val_t *ref, val_t *val); //swap value of referred object with val

err_t val_ref_clone(val_t *ret, val_t *orig);
err_t val_ref_destroy(val_t *val);

int val_ref_fprintf(val_t *val,FILE *file, const fmt_t *fmt);
int val_ref_sprintf(val_t *val,val_t *buf, const fmt_t *fmt);

err_t val_ref_signal(val_t *val);
err_t val_ref_broadcast(val_t *val);
err_t val_ref_wait(val_t *val);

#endif
