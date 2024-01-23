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

#ifndef __VAL_REF_H__
#define __VAL_REF_H__ 1

#include "val.h"

err_t val_ref_wrap(val_t *val); //initialize ref by wrapping val in ref
err_t val_ref_unwrap(val_t *ref); //dereference val (so *ref becomes copy of reffered object)
err_t val_ref_swap(valstruct_t *ref, val_t *val); //swap value of referred object with val

err_t _val_ref_clone(val_t *ret, valstruct_t *orig);
void _val_ref_destroy(valstruct_t *ref);

err_t val_ref_signal(valstruct_t *val);
err_t val_ref_broadcast(valstruct_t *val);
err_t val_ref_wait(valstruct_t *val);

void _val_ref_swap(valstruct_t *ref, val_t *val);
err_t _val_ref_lock(valstruct_t *ref);
err_t _val_ref_trylock(valstruct_t *ref);
err_t _val_ref_unlock(valstruct_t *ref);
err_t _val_ref_signal(valstruct_t *ref);
err_t _val_ref_broadcast(valstruct_t *ref);
err_t _val_ref_wait(valstruct_t *ref);

int val_ref_fprintf(valstruct_t *v,FILE *file, const struct printf_fmt *fmt);
int val_ref_sprintf(valstruct_t *v,valstruct_t *buf, const struct printf_fmt *fmt);

#endif
