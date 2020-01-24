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

#ifndef _VAL_ALLOC_H_
#define _VAL_ALLOC_H_ 1
#include "val.h"

val_t* val_alloc(size_t n);
val_t* val_realloc(val_t *dst, size_t n);
void val_free(val_t *p);







#define FRAME_SIZE 64
#define PAGE_SIZE 16


struct _alloc_frame {
  struct _alloc_frame *next;
  union _alloc_entry {
    union _alloc_entry *next;
    val_t p[PAGE_SIZE];
  } p[FRAME_SIZE];
};

#endif
