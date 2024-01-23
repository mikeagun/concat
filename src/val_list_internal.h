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

#ifndef __VAL_LIST_INTERNAL_H__
#define __VAL_LIST_INTERNAL_H__ 1
#include "val_list.h"

void _val_lst_clean(valstruct_t *v); //destroy all vals outside of list in buffer
void _val_lst_cleanclear(valstruct_t *v); //destroy (and clear to VAL_NULL) all vals outside of list in buffer
void val_list_braces(valstruct_t *lst, const char **open, const char **close);
int _val_list_sprintf_el(valstruct_t *buf, val_t *p, int len, int i, int reverse, const list_fmt_t *sublist_fmt, const fmt_t *val_fmt);

int _val_list_fprintf(FILE *file, val_t *p, int len, const char *open, const char *close, const list_fmt_t *lfmt, const fmt_t *val_fmt);
int _val_list_sprintf(valstruct_t *buf, val_t *p, int len, const char *open, const char *close, const list_fmt_t *lfmt, const fmt_t *val_fmt);

#endif
