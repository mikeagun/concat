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

#include "val_cast.h"
#include "val_include.h"
#include "helpers.h"

#include <string.h>
#include <stdlib.h>

int val_as_bool(val_t *val) {
  if (val_isint(val)) {
    return val_int(val) != 0;
  } else if (val_isfloat(val)) {
    return val_float(val) != 0;
  } else if (val_islisttype(val)) {
    return !val_list_empty(val);
  } else if (val_isstring(val)) {
    return !val_string_empty(val);
  } else if (val_isvm(val)) {
    return val_vm_finished(val);
  } else {
    return 0;
  }
}

valint_t val_as_int(val_t *val) {
  if (val_isnumber(val)) {
    return val_num_int(val);
  } else if (val_islisttype(val)) {
    return (valint_t)val_list_len(val);
  } else if (val_isstring(val)) {
    return (valint_t)val_string_len(val);
    //val_t t;
    //err_t r = val_int_parse(&t,val_string_str(val),val_string_len(val));
    //valint_t v = val_int(&t);
    //val_destroy(&t); //unecessary, except for later when implementing value tracing
    //if (r) return 0;
    //else return v;
  } else {
    return 0;
  }
}

err_t val_to_int(val_t *ret, val_t *val) {
  if (val_isstring(val)) {
    return val_int_parse(ret,val_string_str(val),val_string_len(val));
  } else {
    val_int_init(ret,val_as_int(val));
    return 0;
  }
}

err_t val_to_string(val_t *val) {
  if (val_isstring(val)) return 0;
  else if (val_isident(val)) {
    val_ident_to_string(val);
    return 0;
  } else {
    val_t str;
    val_string_init(&str);
    err_t r;
    if (0>(r = val_sprintf_(val,&str,fmt_v))) {
      val_destroy(&str);
      return r;
    }
    val_destroy(val);
    val_move(val,&str);
    return 0;
  }
}

