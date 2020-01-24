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

#ifndef __OPS_REF_H__
#define __OPS_REF_H__ 1
#include "vmstate.h"

//TODO: review and determine minimum comprehensive operator set

err_t ops_ref_init(vm_t *vm);

vm_op_handler _op_ref;      //A ref -- ref(A)
vm_op_handler _op_deref;    //ref(A) -- A
vm_op_handler _op_refswap;  //ref(A) B -- ref(B) A

vm_op_handler _op_guard;           //ref(x) [A] guard -- lock(ref(x)) x A unlock(ref(x))
vm_op_handler _op_guard_sig;       //ref(x) [A] guard.sig -- lock(ref(x)) x A signal(ref(x)) unlock(ref(x))
vm_op_handler _op_guard_bcast;     //ref(x) [A] guard.bcast -- lock(ref(x)) x A bcast (ref(x)) unlock(ref(x))
vm_op_handler _op_guard_waitwhile; //ref(x) [prewait] [postwait] guard.waitwhile -- lock x [prewait] (ref() [prewait] [postwait]) waitwhile_ unlock
vm_op_handler _op_guard_sigwaitwhile; //ref(x) [prewait] [postwait] guard.waitwhile -- lock x [prewait] signal (ref() [prewait] [postwait]) waitwhile_ unlock
vm_op_handler _op_waitwhile_;      //x bool (ref() [prewait] [postwait]) waitwhile_ -- x postwait ref()  OR  wait x (ref() [prewait] [postwait]) waitwhile_
vm_op_handler _op_sigwaitwhile_;      //x bool (ref() [prewait] [postwait]) waitwhile_ -- x postwait ref()  OR  wait x (ref() [prewait] [postwait]) waitwhile_
vm_op_handler _op_unguard_catch; //internal: continuation handler to ensure refs get unlocked on exception
vm_op_handler _op_unguard;       //internal: unlock reference
vm_op_handler _op_unguard_sig;   //internal: signal and unlock reference
vm_op_handler _op_unguard_bcast; //internal: broadcast and unlock reference

vm_op_handler _op_signal;  //ref{A} -- ref{A}     signal one thread waiting on this ref
vm_op_handler _op_broadcast;  //ref{A} -- ref{A}  signal all threads waiting on this ref
vm_op_handler _op_wait;    //ref{A} -- ref{A}     locks, waits for a signal, unlocks -- you probably want guard.waitwhile instead

#endif
