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

#ifndef __VAL_DICT_H__
#define __VAL_DICT_H__ 1
// val_dict.h - concat dictionary val interface


#include "val.h"

// NOTES:
// - copy-on-write - cloning dict just adds ref, writing to dict creates full clone
// - scoped - child dictionaries write to current layer, and read through to parent/next when key not found
//   - dict keeps next pointer to parent dict
// - built on top of val_hash hashtable implementation

// Interface:
//   _val_dict_* functions operate on dictionary valstructs
//
//   Management:
//   - init    - create new dictionary
//   - clone   - clone dictionary (just adds reference)
//   - destroy - destroy dictionary (and free resources)
//   - deref   - clones underlying hashtable and releases old so dict has sole ownership
//   
//   Scoping:
//   - newscope  - creates new dictionary layer (scope) to isolate writes (current dict then refers to child)
//   - canpop    - whether dictionary is a child scope (has parent dict)
//   - popscope  - pop scope off into new val (current dict then refers to parent)
//   - dropscope - drop (pop and delete) child scope (current dict then refers to parent)
//   - pushscope - push existing dict onto current as child scope (current dict then refers to child)
//
//   Read/Write:
//   - get  - lookup key and get value in current/parent dict, or NULL if not found (NOTE: value is NOT cloned)
//   - put  - write key-value pair into dict
//   - put_ - like put but takes char pointer and length (automatically creates val to hold key string)
//   - swap - swap value in dict (if in parent scope, clones instead of swaps)
//

void _val_dict_destroy_(valstruct_t *dict);
err_t _val_dict_init(valstruct_t *dict);
err_t _val_dict_clone(val_t *ret, valstruct_t *orig);
void _val_dict_clone_(valstruct_t *ret, valstruct_t *orig);
err_t _val_dict_deref(valstruct_t *dict);

err_t _val_dict_newscope(valstruct_t *dict);
int _val_dict_canpop(valstruct_t *dict);
void _val_dict_popscope(valstruct_t *dict, val_t *scope);
void _val_dict_dropscope(valstruct_t *dict);
void _val_dict_pushscope(valstruct_t *dict, valstruct_t *scope);

val_t _val_dict_get(valstruct_t *dict, valstruct_t *key);
int _val_dict_put(valstruct_t *dict, valstruct_t *key, val_t val);
int _val_dict_put_(valstruct_t *dict, const char *key, unsigned int klen, val_t val);

//swap val out of dict -- only actually swaps when def is in dict->h, otherwise clones
err_t _val_dict_swap(valstruct_t *dict, valstruct_t *key, val_t *val);


int val_dict_fprintf(valstruct_t *v,FILE *file, const struct printf_fmt *fmt);
int val_dict_sprintf(valstruct_t *v,valstruct_t *buf, const struct printf_fmt *fmt);

#endif
