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

#include "val_include.h"
#include "val_stack.h"

int main(int argc, char *argv[]) {
  val_init_type_handlers();
  val_t stack;
  val_stack_init(&stack);
  val_t t;
  val_stack_push(&stack,val_int_init(&t,5));
  int r;
  const char *hello = "hello world";
  if ((r = val_string_init_cstr(&t,hello,strlen(hello)))) {
    printf("error string init\n");
    return 1;
  }
  val_stack_push(&stack,&t);
  val_stack_push(&stack,val_int_init(&t,6));
  val_stack_rprint(&stack);
  return 0;
}
