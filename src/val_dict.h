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
#include "val.h"

void _val_dict_destroy_(valstruct_t *dict);
int _val_dict_init(valstruct_t *dict);
int _val_dict_clone(val_t *ret, valstruct_t *orig);
void _val_dict_clone_(valstruct_t *ret, valstruct_t *orig);

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
