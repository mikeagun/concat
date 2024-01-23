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

#include "vm_err.h"
#include "vm.h"
#include "val_list.h"
#include "val_printf.h"
#include "helpers.h"

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#define ERR_STRING(e,s) #e
const char *errcodes[] =  { ERRCODES(ERR_STRING) };
#undef ERR_STRING

#define ERR_STRING(e,s) s
const char *errstrings[] =  { ERRCODES(ERR_STRING) };
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
    return "Not an error";
  } else if (err<LAST_ERROR) {
    return "Unknown error number";
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
  fprintf(file,"FATAL: %s\n", err_string(err));
}
err_t errval_fprintf(FILE *file, val_t err) {
  if (val_is_int(err)) {
    vm_perrornum(__val_int(err));
    return 0;
  } else {
    return val_fprintf(file,"ERROR: %V\n",err);
    //printf("ERROR: ");
    //int r = val_print(err);
    //printf("\n");
  }
}


void vm_perrornum(err_t err) {
  const char *estring = err_string(err);
  if (estring) {
    printf("ERROR: %s\n",estring);
  } else {
    printf("ERROR: %d\n",err);
  }
}
void vm_pfatal(err_t err) {
  const char *estring = err_string(err);
  if (estring) {
    printf("FATAL: %s\n",estring);
  } else {
    printf("FATAL: %d\n",err);
  }
}

err_t vm_perror(vm_t *vm) {
  if (vm_empty(vm)) return _throw(ERR_MISSINGARGS);
  else if (val_is_int(vm_top(vm))) {
    //vm_perrornum(val_as_int(vm_top(vm)));
    val_t t = vm_top(vm);
    vm_perrornum(__val_int(t));
  } else {
    int r = vm_stack_printf(vm,"ERROR: %V\n");
    //printf("ERROR: ");
    //int r = val_print(vm_top(vm));
    //printf("\n");
    if (r < 0) return r;
    else return 0;
  }
  //return vm_drop(vm);
  return 0;
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

int vm_fatal_str(err_t err,const char *msg) {
#ifdef INTERRUPT_ON_FATAL
  raise(SIGINT);
#endif
  fprintf(stderr,"FATAL: %s",msg);
  return ERR_FATAL;
}

err_t vm_throw(vm_t *vm, err_t err) {
//#ifdef INTERRUPT_ON_THROW //assumes we already called _throw() on err
//  raise(SIGINT);
//#endif
  if (err != ERR_THROW && err != ERR_USER_THROW) { //throw top of stack
    int r;
    fatal_if(r,(r=vm_push(vm,__int_val(err))));
    return ERR_THROW;
  } else { //else exception already on stack
    return err;
  }
}

//forward error from debuggee to debugger
err_t vm_debug_throw(vm_t *vm, err_t err) {
  //don't need to interrupt since debuggee already should have

  return _fatal(ERR_NOT_IMPLEMENTED);
  //if (err == ERR_THROW || err == ERR_USER_THROW) {
  //  err_t fatal = _val_lst_rpush_fromr(vm->open_list,vm->debuggee->open_list);
  //  if (fatal) return _fatal(fatal);
  //  else return err;
  //} else if (!err_isfatal(err)) {
  //  vm_push(vm,__int_val(err));
  //  return ERR_THROW;
  //} else return err;
}

err_t vm_user_throw(vm_t *vm) {
#ifdef INTERRUPT_ON_THROW
  raise(SIGINT);
#endif
  if (vm_empty(vm)) return _throw(ERR_EMPTY);
  val_t top = vm_top(vm);
  if (val_is_int(top) && __val_int(top)==0) return vm_pop(vm,NULL);
  else return ERR_USER_THROW;
}

int err_isfatal(int err) {
  return err == ERR_FATAL;
}
