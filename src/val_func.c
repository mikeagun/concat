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

#include "val_func.h"
#include "val_printf.h"
#include "vm_err.h"

val_t* val_func_init(val_t *val, int (*func)(struct vm_state *vm), const char *name) {
  return val_func_init_(val,func,name,0);
}
val_t* val_func_init_(val_t *val, int (*func)(struct vm_state *vm), const char *name, unsigned char opcode) {
  val->type = TYPE_NATIVE_FUNC;
  val->val.func.f = func;
  val->val.func.name = name;
  val->val.func.keep = 0;
  val->val.func.opcode = opcode;
  return val;
}

val_t* val_func_init_keep(val_t *val, int (*func)(struct vm_state *vm), const char *name) {
  return val_func_init_keep_(val,func,name,0);
}
val_t* val_func_init_keep_(val_t *val, int (*func)(struct vm_state *vm), const char *name, unsigned char opcode) {
  val->type = TYPE_NATIVE_FUNC;
  val->val.func.f = func;
  val->val.func.name = name;
  val->val.func.keep = 1;
  val->val.func.opcode = opcode;
  return val;
}


vm_op_handler* val_func_f(val_t *f) {
  return f->val.func.f;
}
const char* val_func_name(val_t *f) {
  return f->val.func.name;
}


int val_func_keep(val_t *f) {
  return f->val.func.keep;
}

err_t val_func_exec(val_t *f, vm_t *vm) {
  return f->val.func.f(vm);
}

void val_func_init_handlers(struct type_handlers *h) {
  h->fprintf = val_func_fprintf;
  h->sprintf = val_func_sprintf;
}
int val_func_fprintf(val_t *val, FILE *file, const fmt_t *fmt) {
  switch(fmt->conversion) {
    case 'v':
      if (val->val.func.name) return fprintf(file,"%s",val->val.func.name);
      else return fprintf(file,"%p",val->val.func.f);
    case 'V':
      if (val->val.func.name) return fprintf(file,"native(%s)",val->val.func.name);
      else return fprintf(file,"native(%p)",val->val.func.f);
    default:
      return _throw(ERR_BADTYPE);
  }
}
int val_func_sprintf(val_t *val, val_t *buf, const fmt_t *fmt) {
  err_t r;
  int rlen=0;
  switch(fmt->conversion) {
    case 'v':
      if (val->val.func.name) {
        if (0>(r = val_sprint_cstr(buf,val->val.func.name))) return r; rlen += r;
      }
      break;
    case 'V':
      if (0>(r = val_sprint_cstr(buf,"native("))) return r; rlen+=r;

      if (val->val.func.name) {
        if (0>(r = val_sprint_cstr(buf,val->val.func.name))) return r; rlen += r;
      }

      if (0>(r = val_sprint_cstr(buf,")"))) return r; rlen+=r;
  }
  return rlen;
}
