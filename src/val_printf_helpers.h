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

#ifndef __VAL_PRINTF_HELPERS__
#define __VAL_PRINTF_HELPERS__ 1

#include "val.h"
#include "val_printf_fmt.h"
#include <stdio.h>

void val_printf_clear(struct printf_fmt *fmt);
int val_fprintf_(val_t val, FILE *file, const struct printf_fmt *fmt);
int val_sprintf_(val_t val, valstruct_t *buf, const struct printf_fmt *fmt);

val_t* val_arglist_get(valstruct_t *list, int isstack, int i);
//int val_arglist_pop(valstruct_t *list, int isstack, val_t *el);
int val_arglist_drop(valstruct_t *list, int isstack);

#endif
