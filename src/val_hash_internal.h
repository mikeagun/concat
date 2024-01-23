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

#ifndef __VAL_HASH_INTERNAL_H__
#define __VAL_HASH_INTERNAL_H__ 1
#include "val_hash.h"

void _hashchain_dispose(struct hashentry *e);

struct hashentry* alloc_hashentry(valstruct_t *key, val_t value, uint32_t khash, struct hashentry *next);
struct hashentry* alloc_hashentry_clone(valstruct_t *key, val_t value, uint32_t khash, struct hashentry *next);

#endif
