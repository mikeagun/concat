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

#include "val_alloc.h"

#include <stdlib.h>

val_t* val_alloc(size_t n) {
  return malloc(sizeof(val_t)*n);
}
val_t* val_realloc(val_t *p, size_t n) {
  return realloc(p,sizeof(val_t)*n);
}
val_t* val_realloc_inplace(val_t **pp, size_t n) {
  val_t *t = realloc(*pp,sizeof(val_t)*n);
  if (t) {
    *pp = t;
    return t;
  } else {
    return NULL;
  }
}

void val_free(val_t *p) {
  free(p);
}
