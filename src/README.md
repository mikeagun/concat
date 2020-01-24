Copyright (C) 2020 D. Michael Agun

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

# IMPLEMENTATION README
- this has important info you need to understand the concat source code and/or to add new/custom features to the VM
- important rules to follow in the source code should all be in this document
- all of this is from the persepective of the C VM code (not the concat language)

# NOTE: refactor in-progress
- currently re-implementing concat to token-threaded-bytecode, so the below info is correct for the new implementation, the code is still catching up


# DEPENDENCIES
  - pretty much just pthread.h,semaphore.h, and standard libraries
    - pthread needed for vm, semaphores for vm and ref types
  - goal is to get+keep the core light enough to run on 8-bit AVR, so I want to keep dependencies light
    - For a couple of features (i.e. threading) there will obviously be some differences, but eventually those should all be macro switches

# READING THIS DOCUMENT
  - defined == the flag -DSOMEOPTION was passed to gcc, or `#define SOMEOPTION 1` is used somewhere before the option is first checked
  - argcheck -- call one of the argchecks macros to validate all incoming arguments (that aren't guaranteed to be argchecked later)
  - `_throw(e)`/`_fatal(e)` -- the throw exception/fatal exception macros


# ARGCHECKS
argcheck macros throw a `_fatal(BADARGS)` exception (and print an ARGCHECK error) when `DEBUG_CHECKS` is defined, and are a no-op otherwise
  - all library functions should argcheck all assumptions about arguments (e.g. type, length), and no caller should rely on argchecks
    - you are allowed to skip checks if the first thing you do is call a function which does argcheck
    - be very careful about removing argchecks (find every function which calls that one and verify they are checked)
  - checking every argument to every library function catches c bugs a lot faster, but adds overhead in duplicate checks
  - if you don't already know your arguments are valid for a call, you must check them first
  - seeing an ARGCHECK in the output (on stderr) indicates a bug, so they are always `_fatal()`
    - you can hit it again with `INTERRUPT_ON_FATAL` defined in gdb to jump into the C code there
      - e.g. $ make concat-debug && gdb --args ./concat-debug -f somefailingscript.cat

# EXCEPTION HANDLING
exception handling is done via `_throw(e)`/`_fatal(e)` macros and return codes
- it is actually the return code e itself that is the exception (from err.h), but ALWAYS wrap the creation of a new exception in one of the
  throw/fatal functions from err.h
  - these macros give extra output during debugging
1. only functions that can never fail should return void
  - if any assumptions are made in "can never fail", those must be argchecked
2. functions that can throw exceptions should return `err_t`
3. all `err_t` return values (or ints that act as `err_t` if <0) must be checked
  - must either return the same error received or an overriding one (e.g. MISSINGARGS could override EMPTY since it is more useful)
  - other option is to return `_fatal(e)` on the exception to override exception with FATAL, but print the original during debugging
  - we should minimize the number of fatal exceptions in the code, since those are unrecoverable (and make trycatch useless)
    - eventually would like to eliminate all the fatals, so we could just trycatch+scope some code and be completely protected if it is bad
      - so as long as fatals exist we will need some extra safety nets (esp. on MCU build)
4. must clean up / destroy all temporary variables
  - don't need to destroy vals that are already on the stack since that will happen when the VM sees the exception
  - nothing should be left in an inconsistent state such that resources might leak
    - this is very important (on AVR with a couple KB spare, we can't afford to leak much)


# ADDING NEW OPS/TYPES
This is all driven by the opcodes.h macros
- TODO: we really need some appropriate blocking out of ranges to ensure standard cross-compile/version opcodes for the basic stuff
  - as long as we can agree on the basic calls, we can push (generally useful) code between VMs on different compiles/machines/architectures
If you add a new type (by adding an entry to `TYPECODES`), it will generate:
  - In opcodes.h:
    - typecode enum value `TYPE_mytype`
    - entry in `ALL_OPCODES`
    - opcode enum value `OP_mytype`
  - In val.h
    - typecheck function `int val_ismytype(val_t *val)`
      - definition also generated in val.c
  - In vm.c
    - reference to VM op label `op_mytype:`
      - **YOU NEED TO IMPLEMENT THIS**
  - The typehandler calls for clone/destroy/printf functions
    - `val_mytype_destroy(val_t *val)`
    - `val_mytype_clone(val_t *dst, val_t *src)`
    - `val_mytype_fprintf(val_t *val, FILE *file, const fmt_t *fmt)`
    - `val_mytype_sprintf(val_t *val, val_t *buf, const fmt_t *fmt) //where buf is NULL or a stringtype val`
    - **YOU NEED TO IMPLEMENT ALL OF THESE**
  - by convention we write the opcodes in lowercase (to match concat standard dictionary, since much of it is 1to1)
  - you will get compiler errors if you are missing anything, which is kind of nice

If you add a new op to OPCODES, it will generate:
  - In opcodes.h:
    - entry in `ALL_OPCODES`
    - opcode enum value `OP_myop`
  - In vm.c
    - reference to VM op label `op_myop:`
      - **YOU NEED TO IMPLEMENT THIS**


