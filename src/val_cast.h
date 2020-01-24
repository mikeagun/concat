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

#ifndef __VAL_CAST_H__
#define __VAL_CAST_H__ 1
#include "val.h"

int val_as_bool(val_t *val);

valint_t val_as_int(val_t *val);
err_t val_to_int(val_t *ret, val_t *val);

err_t val_to_string(val_t *val);

#endif
