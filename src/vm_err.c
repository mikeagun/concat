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

#include "vmstate.h"
#include "vm_err.h"
#include "val_num.h"
#include "ops_internal.h"
#include "val_printf.h"
#include "helpers.h"
#include <signal.h> //for raise()

#define ERR_STRING(e,s) #e
const char *errcodes[] =  { "SUCCESS", ERRCODES(ERR_STRING) };
#undef ERR_STRING

#define ERR_STRING(e,s) #s
const char *errstrings[] =  { "Success", ERRCODES(ERR_STRING) };
#undef ERR_STRING

const char *err_code(err_t err) {
  if (err>0) {
    return "NOTERROR";
  } else if (err<LAST_ERROR) {
    return "UNKNOWN";
  } else {
    return errcodes[-err];
  }
}
const char *err_string(err_t err) {
  if (err>0) {
    return "NOTERROR";
  } else if (err<LAST_ERROR) {
    return "UNKNOWN";
  } else {
    return errstrings[-err];
  }
}
err_t err_parse(const char *str, unsigned int len) {
  err_t i;
  for(i = 0; i <= N_ERRORS; ++i) {
    if (!strncmp_cstr(str,len,errcodes[i])) return -i;
  }
  return 1;
}

void err_fprintf(FILE *file, err_t err) {
  fprintf(file,"ERROR: %s\n", err_string(err));
}
void fatal_fprintf(FILE *file, err_t err) {
  fprintf(file,"FATAL ERROR: %s\n", err_string(err));
}

void vm_pfatal(err_t err) {
  fatal_fprintf(stderr,err);
}

err_t vm_perror(vm_t *vm) {
  __op_pop1;

  if (val_isint(p) && val_int(p)>=LAST_ERROR) {
    err_fprintf(stderr,val_int(p));
  } else {
    int r;
    if (0>(r = val_fprint_cstr(stdout,"ERROR: "))) return r;
    if (0>(r = val_fprintf_(p,stdout,fmt_V))) return r;
    if (0>(r = val_fprint_(stdout,"\n",1))) return r;
  }
  return vm_pop(vm,NULL);
}

#ifdef INTERRUPT_ON_THROW
err_t _throw_(err_t err) {
  raise(SIGINT);
  return err;
}
#endif

#ifdef INTERRUPT_ON_FATAL
err_t _fatal_(err_t err) {
  raise(SIGINT);
  return ERR_FATAL;
}
#endif

//int vm_fatal_cstr(int ret,const char *msg) {
//#ifdef INTERRUPT_ON_FATAL
//  raise(SIGINT);
//#endif
//  fprintf(stderr,"FATAL ERROR: %s",msg);
//  return ERR_FATAL;
//}

err_t vm_throw(vm_t *vm, err_t err) {
#ifdef INTERRUPT_ON_THROW
  raise(SIGINT);
#endif
  if (err != ERR_THROW && err != ERR_USER_THROW) { //throw top of stack
    val_t t;
    int r;
    fatal_if(r,(r=vm_push(vm,val_int_init(&t,err))));
    return ERR_THROW;
  } else { //else exception already on stack
    return err;
  }
}

//forward error from debuggee to debugger
err_t vm_debug_throw(vm_t *vm, vm_t *debuggee, err_t e) {
  //don't need to interrupt since debuggee already should have
  if (e == ERR_THROW || e == ERR_USER_THROW) { //throw top of stack
    __op_push1;
    if (vm_empty(debuggee)) {
      val_int_init(p,_throw(ERR_EMPTY));
    } else {
      err_t ret;
      if ((ret = vm_pop(debuggee,p))) return _fatal(ret);
    }
    return e;
  } else if (!err_isfatal(e)) { //else exception in e
    __op_push1;
    val_int_init(p,e);
    return ERR_THROW;
  } else return e; //fatal error
}

//throw from code
err_t vm_user_throw(vm_t *vm) {
#ifdef INTERRUPT_ON_THROW
  raise(SIGINT);
#endif
  __op_get1;
  if (val_isint(p) && val_int(p)==0) return vm_pop(vm,NULL); //0 throw is no-op
  else return ERR_USER_THROW;
}

err_t err_isfatal(err_t err) {
  return err == ERR_FATAL;
}
