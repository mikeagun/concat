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

#ifndef __VM_DEBUG_H__
#define __VM_DEBUG_H__ 1
#include "vm.h"

//#define VM_DEBUG 1

#ifdef VM_DEBUG //========vm debugging macros enabled========
#define VM_DEBUG_MEMBERS struct vmdebug_data *debug;

#define VM_DEBUG_VAL_INIT(val) vm_debug_val_init(val,__func__)
#define VM_DEBUG_VAL_DESTROY(val) vm_debug_val_destroy(val)
#define VM_DEBUG_VAL_CLONE(val,orig) vm_debug_val_clone(val,orig)
#define VM_DEBUG_VAL_MOVE(dst,src) vm_debug_val_move(dst,src)

#define VM_DEBUG_EVAL(val) vm_debug_val_eval(vm,val)

#define VM_DEBUG_STRBUF_INIT(string,size) do {} while(0)
#define VM_DEBUG_STRBUF_DESTROY(buf) do {} while(0)
#define VM_DEBUG_STRBUF_FREE(buf) do {} while(0)
#define VM_DEBUG_STR_REALLOC(string,lspace,rspace) do {} while(0)

#define VM_DEBUG_LISTBUF_INIT(list,size) do {} while(0)
#define VM_DEBUG_LISTBUF_DESTROY(buf) do {} while(0)
#define VM_DEBUG_LISTBUF_FREE(buf) do {} while(0)
#define VM_DEBUG_LIST_REALLOC(list,lspace,rspace) do {} while(0)

#else //========else all debug macros should be no-ops========
#define VM_DEBUG_MEMBERS

#define VM_DEBUG_VAL_INIT(val) do {} while(0)
#define VM_DEBUG_VAL_DESTROY(val) do {} while(0)
#define VM_DEBUG_VAL_CLONE(val,orig) do {} while(0)
#define VM_DEBUG_VAL_MOVE(dst,src) do {} while(0)

#define VM_DEBUG_EVAL(val) do {} while(0)

#define VM_DEBUG_STRBUF_INIT(string,size) do {} while(0)
#define VM_DEBUG_STRBUF_DESTROY(buf) do {} while(0)
#define VM_DEBUG_STRBUF_FREE(buf) do {} while(0)
#define VM_DEBUG_STR_REALLOC(string,lspace,rspace) do {} while(0)

#define VM_DEBUG_LISTBUF_INIT(list,size) do {} while(0)
#define VM_DEBUG_LISTBUF_DESTROY(buf) do {} while(0)
#define VM_DEBUG_LISTBUF_FREE(buf) do {} while(0)
#define VM_DEBUG_LIST_REALLOC(list,lspace,rspace) do {} while(0)
#endif


struct vmdebug_data;

//NOTE: debugger becomes responsible for debuggee, including freeing at end, so you should not call vm_destroy on debuggee anymore
//int vm_debug_set_target(vm_t *debugger, vm_t *debuggee);

//wrap vm in debugger (so vm now points to debugger, and vm->debuggee points to the old vm)
int vm_debug_wrap(vm_t *vm);

//end debugging session by destroying debugger and continuing in debuggee (so vm will be vm->debuggee at end)
int vm_debug_unwrap(vm_t *vm);

//initialize debugger ops
int ops_debug_init(vm_t *vm);

int vm_debuggee(vm_t *vm, vm_t **debuggee);

int vm_debug_val_init(val_t *val, const char *func); //called on val initialization (just before returning from init function)
int vm_debug_val_destroy(val_t *val); //called before destroying a val
int vm_debug_val_clone(val_t *val, val_t *orig);  //called when a val is cloned
int vm_debug_val_eval(vm_t *vm, val_t *val);  //called before trying to eval a val

int _vm_debug_catch(vm_t *vm); //for trapping out to debugger
int _vm_debug_fallback(vm_t *vm); //for trapping out to debugger as final fallback (don't push empty catch handler)

#endif
