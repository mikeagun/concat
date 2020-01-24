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

int test_int_init(val_t *val, valint_t i) {
  val_int_init(val,i);
  printf("int(%ld) = %ld = ",i,val_int(val)); fflush(stdout);
  val_print(val);
  printf("\n");

  return 0;
}

int test_int() {
  val_t v1,v2,v3;
  test_int_init(&v1,0);
  test_int_init(&v2,3);
  test_int_init(&v3,5);

  val_destroy(&v1);
  val_destroy(&v2);
  val_destroy(&v3);

  return 0;
}

int test_float_init(val_t *val, double f) {
  val_float_init(val,f);
  printf("float(%f) = %f = ",f,val_float(val)); fflush(stdout);
  val_print(val);
  printf("\n");

  return 0;
}

int test_float() {
  val_t v1,v2,v3;
  test_float_init(&v1,1);
  test_float_init(&v2,3.0);
  test_float_init(&v3,1.0/3.0);

  val_destroy(&v1);
  val_destroy(&v2);
  val_destroy(&v3);

  return 0;
}

int test_string_init_dquote(val_t *val, const char *str) {
  val_string_init_dquoted(val,str,strlen(str));
  printf("string(%s) = ",str); fflush(stdout);
  val_print_code(val);
  printf("\n");

  return 0;
}

int test_string_cat(val_t *val1, val_t *val2) {
  printf("cat \""); fflush(stdout);
  val_print(val1); fflush(stdout);
  printf("\" + \""); fflush(stdout);
  val_print(val2); fflush(stdout);
  printf("\" = \""); fflush(stdout);
  val_string_cat(val1,val2); fflush(stdout);
  val_print(val1);
  printf("\"\n");

  return 0;
}

int test_string() {
  val_t v1,v2,v3;
  test_string_init_dquote(&v1,"\"hello\"");
  test_string_init_dquote(&v2,"\" world\"");
  test_string_init_dquote(&v3,"\"\\x41\\n\\x07\\x00\\0\\02\\000\"");
  test_string_cat(&v1,&v2);

  val_destroy(&v1);
  val_destroy(&v2);
  val_destroy(&v3);

  return 0;
}

int test_list_init(val_t *val) {
  printf("empty list: "); fflush(stdout);
  val_list_init(val);
  val_print(val);
  printf("\n");

  return 0;
}

int test_list_lpush(val_t *list, val_t *el) {
  printf("lpush "); fflush(stdout);
  val_print(el); fflush(stdout);
  printf(" onto "); fflush(stdout);
  val_print(list); fflush(stdout);
  printf(" = "); fflush(stdout);
  val_list_lpush(list,el); fflush(stdout);
  val_print(list);
  printf("\n");

  return 0;
}

int test_list_lpushes(val_t *list) {
  val_t t;
  test_int_init(&t,1);
  test_list_lpush(list,&t);

  test_int_init(&t,2);
  test_list_lpush(list,&t);

  test_list_init(&t);
  test_list_lpush(list,&t);

  test_int_init(&t,3);
  test_list_lpush(list,&t);

  return 0;
}

int test_list_lpop(val_t *list) {
  printf("lpop "); fflush(stdout);
  val_print(list); fflush(stdout);
  printf(" into "); fflush(stdout);
  val_t t;
  val_list_lpop(list,&t); fflush(stdout);
  val_print(&t); fflush(stdout);
  val_destroy(&t);
  printf(" , "); fflush(stdout);
  val_print(list);
  printf("\n");

  return 0;
}

int test_list_rpush(val_t *list, val_t *el) {
  printf("rpush "); fflush(stdout);
  val_print(el); fflush(stdout);
  printf(" onto "); fflush(stdout);
  val_print(list); fflush(stdout);
  printf(" = "); fflush(stdout);
  val_list_rpush(list,el); fflush(stdout);
  val_print(list); fflush(stdout);
  printf("\n");

  return 0;
}

int test_list_rpushes(val_t *list) {
  val_t t;
  test_int_init(&t,1);
  test_list_rpush(list,&t);

  test_int_init(&t,2);
  test_list_rpush(list,&t);

  test_list_init(&t);
  test_list_rpush(list,&t);

  test_int_init(&t,3);
  test_list_rpush(list,&t);

  return 0;
}

int test_list_rpop(val_t *list) {
  printf("rpop "); fflush(stdout);
  val_print(list); fflush(stdout);
  printf(" into "); fflush(stdout);
  val_t t;
  val_list_rpop(list,&t);
  val_print(list); fflush(stdout);
  printf(" , "); fflush(stdout);
  val_print(&t); fflush(stdout);
  val_destroy(&t);
  printf("\n");

  return 0;
}


int test_list() {
  val_t v1,v2;

  test_list_init(&v1);

  test_list_lpushes(&v1);
  test_list_lpop(&v1);
  test_list_lpop(&v1);
  test_list_lpop(&v1);
  test_list_lpop(&v1);

  test_list_rpushes(&v1);
  test_list_rpop(&v1);
  test_list_rpop(&v1);
  test_list_rpop(&v1);
  test_list_rpop(&v1);

  test_list_lpushes(&v1);
  test_list_rpop(&v1);
  test_list_rpop(&v1);
  test_list_rpop(&v1);
  test_list_rpop(&v1);

  test_list_rpushes(&v1);
  test_list_lpop(&v1);
  test_list_lpop(&v1);
  test_list_lpop(&v1);
  test_list_lpop(&v1);

  test_int_init(&v2,1);
  test_list_lpush(&v1,&v2);

  test_int_init(&v2,2);
  test_list_rpush(&v1,&v2);

  test_list_init(&v2);
  test_list_lpush(&v1,&v2);

  test_int_init(&v2,3);
  test_list_rpush(&v1,&v2);

  test_list_lpop(&v1);
  test_list_rpop(&v1);
  test_list_lpop(&v1);
  test_list_rpop(&v1);

  val_destroy(&v1);

  return 0;
}

int test_file() {
  val_t file;
  printf("read test.txt:\n");
  val_file_init(&file,"test.txt","r");

  char buffer[500];
  int r;
  while (0 <= (r = val_file_readline(&file,buffer,500))) {
    printf("line: \"%s\"\n",buffer);
  }
  if (r == FILE_EOF) {
    printf("done reading\n");
  } else {
    printf("ERROR: file read error\n");
  }
  val_destroy(&file);

  return 0;
}


int main(int argc, char *argv[]) {
  val_init_type_handlers();

  test_int();
  test_float();
  test_string();
  test_list();
  test_file();

  return 0;
}
