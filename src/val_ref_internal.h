//Copyright (C) 2024 D. Michael Agun
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

val_t* _val_ref_val(valstruct_t *val);

err_t _ref_lock(valstruct_t *ref);
err_t _ref_trylock(valstruct_t *ref);
err_t _ref_unlock(valstruct_t *ref);

err_t _ref_signal(valstruct_t *ref);
err_t _ref_broadcast(valstruct_t *ref);
err_t _ref_wait(valstruct_t *ref);

void _val_ref_swap(valstruct_t *ref, val_t *val);

err_t _val_ref_lock(valstruct_t *ref);
err_t _val_ref_trylock(valstruct_t *ref);
err_t _val_ref_unlock(valstruct_t *ref);

err_t _val_ref_signal(valstruct_t *val);
err_t _val_ref_broadcast(valstruct_t *val);
err_t _val_ref_wait(valstruct_t *val);

#endif
