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

#include "val_compare.h"
#include "val_string.h"
#include "val_list.h"
#include "val_num.h"

int val_compare(val_t *lhs, val_t *rhs) {
  if (val_isnumber(lhs) && val_isnumber(rhs)) {
    return val_num_compare(lhs,rhs);
  } else if ( lhs->type == rhs->type && (val_isstring(lhs) || val_isident(lhs)) ) { //string compare
    return val_string_compare(lhs,rhs);
  } else if ( lhs->type == rhs->type && val_islisttype(lhs) && val_islisttype(rhs) ) { //list compare
    return val_list_compare(lhs,rhs);
  } else {
    return -1; //not comparable
  }
}
int val_lt(val_t *lhs, val_t *rhs) {
  if (val_isnumber(lhs) && val_isnumber(rhs)) {
    return val_num_lt(lhs,rhs);
  } else if ( lhs->type == rhs->type && (val_isstring(lhs) || val_isident(lhs)) ) { //string compare
    return val_string_lt(lhs,rhs);
  } else if ( lhs->type == rhs->type && val_islisttype(lhs) && val_islisttype(rhs) ) { //list compare
    return val_list_lt(lhs,rhs);
  } else {
    return 0; //not comparable
  }
}
int val_gt(val_t *lhs, val_t *rhs) {
  return val_lt(rhs,lhs);
}
int val_eq(val_t *lhs, val_t *rhs) {
  if (val_isnumber(lhs) && val_isnumber(rhs)) {
    return val_num_eq(lhs,rhs);
  } else if ( lhs->type == rhs->type && (val_isstring(lhs) || val_isident(lhs)) ) { //string compare
    return val_string_eq(lhs,rhs);
  } else if ( lhs->type == rhs->type && val_islisttype(lhs) ) { //list compare
    return val_list_eq(lhs,rhs);
  } else {
    return 0; //not comparable
  }
}
