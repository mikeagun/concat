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

#ifndef __VAL_REF_INTERNAL_H__
#define __VAL_REF_INTERNAL_H__ 1
#include "val_ref.h"
#include <semaphore.h>

struct val_ref_struct {
  val_t val;
  unsigned int refcount;
  sem_t lock;
  sem_t wait;
  unsigned int nwait;
};

struct val_ref_struct* _val_ref_ref(val_t *val);
val_t* _val_ref_val(val_t *val);

err_t _ref_lock(struct val_ref_struct *ref);
err_t _ref_trylock(struct val_ref_struct *ref);
err_t _ref_unlock(struct val_ref_struct *ref);

err_t _ref_signal(struct val_ref_struct *ref);
err_t _ref_broadcast(struct val_ref_struct *ref);
err_t _ref_wait(struct val_ref_struct *ref);

void _val_ref_swap(val_t *ref, val_t *val);

err_t _val_ref_lock(val_t *ref);
err_t _val_ref_trylock(val_t *ref);
err_t _val_ref_unlock(val_t *ref);

err_t _val_ref_signal(val_t *val);
err_t _val_ref_broadcast(val_t *val);
err_t _val_ref_wait(val_t *val);

#endif
