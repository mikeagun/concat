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

#ifndef __VM_DEBUG_INTERNAL_H__
#define __VM_DEBUG_INTERNAL_H__ 1
#include "vm.h"

struct vmdebug_data {
  int refcount;
};

int _vm_debug_init(vm_t *vm);
int vm_debuggee_arg(vm_t *vm, vm_t **debuggee, val_t **arg);
int vm_debuggee_popiarg(vm_t *vm, vm_t **debuggee, int *arg);

int vm_debug_pnext(vm_t *vm);

vm_op_handler _op_pnext;

vm_op_handler _op_debug;
vm_op_handler _op_end_debug;

vm_op_handler _op_dlist;
vm_op_handler _op_dlistn;
vm_op_handler _op_dwlist;
vm_op_handler _op_dwlistn;
vm_op_handler _op_dpeek;
vm_op_handler _op_dwpeek;
vm_op_handler _op_dstate;
vm_op_handler _op_dqstate;
vm_op_handler _op_dvstate;
vm_op_handler _op_clearstats;
vm_op_handler _op_dstats;

vm_op_handler _op_dstack;
vm_op_handler _op_dwstack;
vm_op_handler _op_dsetstack;
vm_op_handler _op_dwsetstack;

vm_op_handler _op_dpop;
vm_op_handler _op_dwpop;
vm_op_handler _op_dpush;
vm_op_handler _op_dwpush;
vm_op_handler _op_dtake;
vm_op_handler _op_dwtake;
vm_op_handler _op_ddup;
vm_op_handler _op_dwdup;
vm_op_handler _op_ddupnext;

vm_op_handler _op_deval;

vm_op_handler _op_step;
vm_op_handler _op_stepq;
vm_op_handler _op_stepn;
vm_op_handler _op_continue;

vm_op_handler _op_splitnext; //split first item off next work item if next work item is list
//vm_op_handler _op_slicenext; //slice next work item off into subvm
vm_op_handler _op_nextw; //run through top item of work stack (so if wtop = code, nextw = wtop)
vm_op_handler _op_hasnext;
vm_op_handler _op_next; //run through the next value (so if wtop = code, next = first(wtop))
vm_op_handler _op_nextq; //run through the next value without printing next
vm_op_handler _op_break;


#endif
