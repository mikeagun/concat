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

//#include "val_cast.h"
//#include "val_list.h"
//#include "val_string.h"
////#include "val_include.h"
////#include "helpers.h"
//
////#include <string.h>
////#include <stdlib.h>
//
//int val_as_bool(val_t val) {
//  if (val_is_int(val)) {
//    return __val_int(val) != 0;
//  } else if (val_is_double(val)) {
//    return __val_dbl(val) != 0;
//  } else if (val_is_lst(val)) {
//    return !_val_lst_empty(__lst_ptr(val));
//  } else if (val_is_str(val)) {
//    return !_val_str_empty(__str_ptr(val));
//  } else {
//    return 0;
//  }
//}
//
//int val_as_int(val_t val) {
//  //if (val_isnumber(val)) {
//  //  return val_num_int(val);
//  if (val_is_int(val)) {
//    return __val_int(val) != 0;
//  } else if (val_is_double(val)) {
//    return __val_dbl(val) != 0;
//  } else if (val_is_lst(val)) {
//    return _val_lst_len(__lst_ptr(val));
//  } else if (val_is_str(val)) {
//    return _val_str_len(__lst_ptr(val));
//    //val_t t;
//    //int r = val_int_parse(&t,val_string_str(val),val_string_len(val));
//    //long v = val_int(&t);
//    //val_destroy(&t); //unecessary, except for later when implementing value tracing
//    //if (r) return 0;
//    //else return v;
//  } else {
//    return 0;
//  }
//}
//
//int val_to_int(val_t *ret, val_t *val) {
//  if (val_isstring(val)) {
//    return val_int_parse(ret,val_string_str(val),val_string_len(val));
//  } else {
//    val_int_init(ret,val_as_int(val));
//    return 0;
//  }
//}
//
