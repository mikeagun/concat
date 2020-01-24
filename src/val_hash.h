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

#ifndef _VAL_HASH_H_
#define _VAL_HASH_H_ 1
#include "val.h"
#include "valhash.h"

int val_isdict(val_t *val);
err_t val_hash_init(val_t *val, struct hashtable *next);
err_t val_hash_init_(val_t *val, struct hashtable *hash);
err_t val_hash_setnext(val_t *hash, val_t *next);
void val_hash_clearnext(val_t *hash);

err_t val_dict_init(val_t *dict);
err_t val_dict_destroy(val_t *dict);
err_t val_dict_pushnew(val_t *dict);
err_t val_dict_push(val_t *dict, val_t *hash);
err_t val_dict_pop(val_t *dict, val_t *hash);
err_t val_dict_pop_to1(val_t *dict); //pop to the root dictionary (e.g. for error handling)
struct hashtable* val_dict_h(val_t *dict);

//err_t val_hash_open_scope(val_t *val);
//err_t val_hash_close_scope(val_t *val);

//struct hashtable *val_hash_destroy_steal(val_t *hash, struct hashtable *next);


void val_hash_init_handlers(struct type_handlers *h);
err_t val_hash_clone(val_t *ret, val_t *orig);
err_t val_hash_destroy(val_t *val);
int val_hash_fprintf(val_t *val, FILE *file, const fmt_t *fmt);
int val_hash_sprintf(val_t *val, val_t *buf, const fmt_t *fmt);

#endif
