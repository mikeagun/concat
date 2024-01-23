**concat is a concatenative stack-based (aka RPN-style / postfix) programming language and a matching cross-platform bytecode VM.**

# What is it?

This project started a couple years ago as a personal project for an powerful+friendly RPN-style terminal calculator.
It quickly evolved into a general-purpose concatenative stack-based programming language with a bytecode VM.

Beyond making a great calculator (and being useable for general scripting), one of the goals for concat was to enable pushing rule procressing out to the (low-CPU-power) IoT edge.
Toward that end it has some features making it more suitable for implementation on small MCUs compared to heavier languages (see below).

I've moved the code to GitHub to continue the development and make the language and a reference VM publicly available.

It is designed to be:
  - **lightweight** -- i.e. run on resource-constrained microcontrollers like 8bit AVR (e.g. Arduino)
    - simple language, stack-based VM, no GC (stack determines lifetime of val), minimal dependencies
  - **fast** -- token-threaded bytecode VM in C
  - **debuggable** -- exceptions with try/catch, debugging tools built in with the language+VM features to support new ones
    - easy to inspect VM state at any point in the concat or C code, with macros to throw SIGINT and trap out to gdb for debugging the C code
  - **cross platform** -- same language and bytecode from simple calculator application to running compiled bytecode on microcontrollers
    - simple concatenative programming language (eases embedded interpreters -- basically tokenizer + number, string, and list parsing)
    - 8 bit opcode with variable sized bytecode supporting compact programs
    - standard platform independent opcodes (scheduled for 1.0) to support transferring data/code between machines and platforms (e.g. push decision making to IoT edge)
      - can also transfer quotations/identifiers as strings anywhere we have an interpreter/compiler or need to resolve platform-specific natives
    - range of "native" (platform/build dependent) typecodes/opcodes for efficient implementation and code performance
  - **general purpose** -- multi-threading, shared (refcounted) objects, lock/wait/signal/broadcast, integer and floating point math, file IO
  - **extensible** -- documented C code interface to the VM state with consistent rules for new code to follow
    - macro helpers to define new types,ops,errors, and ease implementation -- plus friendly errors if you forget to define a mandatory op/type handler
  - **fully error checked** -- can catch and continue after all non-fatal exceptions
    - running on tiny MCU with limited resources, we really can't afford any leaks and would like not to need additional safety nets for VM
    - this has slid with the complete rewrite -- returning to this is one of the next tasks for concat

Features currently implemented in the VM/interpreter:
- Operations over **integer, float, string, list, and dictionary** data types
- **Control Flow** - both a sufficient set of combinators to implement anything else, and the most common control flow ops
- **Threading** - extremely easy to create/manage threads
- **Thread Synchronization** - thread-safe reference objects which handle locking and support wait/signal/broadcast
- **Exceptions** - try/catch, throw, and ability to break out into concat debugger on error
- **Debugging** - sufficient set of low-level ops to build powerful debugging tools
- **VM Debugging** - macro-based tools to help with debugging the VM (e.g. trap out to gdb on error, validate state at every step)
- **File IO** - file operations and ability to run scripts from files
- **Comprehensive printf** - with tweaks for stack-languages - very useful for debugging and general output

# What's new?

The old (very clean) implementation used switch+function dispatch, and every val was stored as a type+union struct.
This was great for VM debugging, but quite slow. Profiling with valgrind showed memory, dispatch, function call, and typechecking overheads dominated overall CPU time.

To address the above bottlenecks, I've completely rewritten the VM using token-threading and val boxing.
For now the implementation is just focused on 64bit CPUs (with 47 or less bits of canonical address space), but the new design is much lighter and faster, so also gets closer to fitting on small MCUs.

The new implementation uses a 3-state token-threaded VM with 64bit boxed vals.

- 3 states based on stack contents, which lets many opcodes have at least one state where they can skip all stack checks
  - huge reduction in checks for whether or not we have the arguments for an op
  - For unoptimized code we still need all the typechecks, but with the boxing (see below) these are also faster
  - see comments `vm.c` for a more complete description of the VM states
- all vals now boxed into 64bits using inverse pointer unboxing - either the val is directly contained in the 64 bits, or is a tagged pointer to a struct with the rest of the val information
  - opcodes, 32 bit integers (up to 47 bits supported), and doubles are stored directly in the 64bit field
    - greatly reduces both storage overhead and allocation/deallocation costs for simple types
  - extended types (lists, strings, dictionaries, references, ...) store a tagged pointer to a struct in the 64bit field
    - the tagging lets the VM do basic typechecking (string/list/other) without dereferencing the struct, and also point to several different struct types
  - relies on the fact that 64bit pointers don't (usually, currently) have 64 bits of usuable address space, and IEEE754 64bit double NaNs have enough spare bits we can play with to store a pointer along with some tag bits - in current x86\_64 a canonical pointer is just 47 bits
    - the current implementation is somewhat architecture-specific (assuming 47bit canonical pointers), so will need changing for other/future architectures
    - some CPUs are already up to 52+bits usable address space, so this will need to be revisited at some point (see val.h notes for some other tagging options)
    - on an 8bit MCU implementation we'll likely use smaller boxes and might either drop floats/doubles or make them extended (tagged pointer) types
  - see `val.h` for the details on how the bits are packed and some notes on other bit packing / pointer tagging options
- token-threaded dispatch (with space to implement direct-threading later for compiled/optimized bytecode)
  - in the VM core we jump directly to the next opcode with a computed goto (based on opcode and VM state) instead of the old switch+function dispatch
  - the loop iteration, switch statement, and function call of the old impl. made up most of overall CPU time for computation-heavy code, so this shaves a lot of cycles off the minimum cycles-per-instruction
  - with the 3 VM states, the computed goto skips unecessary stack checks and can jump straight to the type checks or operation


# Language Features:
below are some of the features of the language (and VM):

### concat is a language (and VM) of 4 stacks:
  - each stack contains vals (everything in concat is a val, including code and the VM itself)
  - **the stack** -- this is where we keep data and/or code
    - ints,strings,quotations being built, threads, VMs being debugged, etc -- everything goes on the stack
  - **the work stack** -- this contains the work to be evaluated
    - supports control flow, protecting local data, anonymous/named recursion (automatic tail-recursion-elimination)
    - the VM "runs" a program by evaluating the top of the work stack until it is empty
    - some combinators also hide data on the work stack to protect local data and make programs easier to reason about
    - at the language level programs can only push onto the top of the work stack (for the VM to eval)
      - no random jumps/gotos in concat, control flow is by repeated/conditional evaluation using combinators (simpler than it may sound)
      - using dup+eval pattern, any type of looping/recursion can be implemented, with the common patterns already implemented as combinators
  - **the dictionary stack** -- this is where the VM looks up identifiers
    - much of the concat language maps identifiers directly to VM opcodes/natives via the dictionary
    - supports scoping, user defined functions, named-recursion, and general key-value mapping
    - local scopes can either be discarded at the end or popped to the stack as dictionary vals for reuse
  - **the continuation stack** -- this supports try-catch, debug-on-error, and cleanup handlers to e.g. release locks on shared data
    - this could also be handled via the work stack (e.g. insert flag in work stack at continuation point),
      but has given a nice clean separation between work to be evaluated and exception handling/cleanup code

### no explicit memory management
  - no GC -- all data lives on one of the stacks (and is freed when popped or the last reference is popped)
  - smart-pointer style reference objects
    - these also handle locking and thread synchronization primitives like wait/signal/broadcast
    - referred object freed when last reference destroyed
  - core list/string types currently default to exponential growth in both directions so no L/R affinity for building up lists
    - reserve functions allow you to pre-reserve the space you will need if you know up front
  - core list/string types support "views" (refcounted buffers with offset+len for each view)
    - concatenative stack-based languages tend to incur lots of stack item duplication (at least at the language level)
    - in particular useful for recursion, since recursion duplicates the code onto the work stack for each level of recursion (also see tail recursion elimination below)

### exceptions and try/catch
- all non-fatal exceptions can be caught -- still getting back to this since VM rewrite
- integer error codes from the C core with macro helpers for nice error messages
    - all error codes have associated short name and description (via a single macro list)
    - macro options to print extra info (e.g. line of code for an exception)
    - macro options to trap out to gdb on exception when debugging the VM
    - macro options for comprehensive argument checks to library functions
        - lib functions normally make some assumptions on arguments for performance
        - when debugging the C code, we can validate all args to every lib function to help find bugs easier
- concat code (or C code) can throw any val as an exception (though integers normally reserved for error codes)
    - e.g. string error message, stack contents

### debugging
- some tools built in, with the language and VM support for better ones
  - one of the basic types is vm, so you can put a concat VM with the code to debug on the stack, then run/inspect/manipulate that VM using concat
  - it is easy to dump and inspect the entire state of the VM at any point (or trace VM state over time)
  - no breakpoints built-in, but you can easily simulate one with a one-liner to step until some condition is met
    - by inserting a debug call into the code (or writing a script to insert one at the desired location in the work stack), you can start in normal mode and then start a debugger just before the interesting part without any performance penalty

### comprehensive printf implementation
 - full printf format parser (with some adjustments to argument interpretation for stack-based languages like concat)
 - built-in types implement fprintf and sprintf handlers
   - flexible formatting options (e.g. print lists as concat code, or concatenate contents, optionally truncate contents using precision)

### flexible and simple thread interface
  - you create a thread from an initial stack and work stack (data and what to do with it)
    - this pushes a (locked and running) vm val onto the stack
  - `pop`ping a VM which is running a thread cancels it
  - `eval`ing a VM which is running waits for termination, then either evaluates to the final stack contents or rethrows an exception
    - if the vm is stopped, `eval` runs the vm to completion in the current thread
  - putting reference objects in the thread's initial stack supports flexible inter-thread communication
    - ref handles mutual exclusion, also supports wait/signal/broadcast for easy implementation of higher-level comms like async-queue

### automatic tail recursion elimination
  - at language level we duplicate code onto the workstack for recursion
    - this dup eval pattern is how all looping is implemented in concat
  - tail-recursive functions push themselves onto the work stack as the last val is popped from the work stack
    - no increase in stack depth for recursion when function is tail-recursive
  - efficient recursion supported via refcounted views on list buffers (no actual copy of contents)

### dictionary stack supporting scoping
  - as long as dictionary isn't modified, concatenative code has nice property that we can freely inline or factor out anything/everything
  - scopes allow code to constrain "impure" effects of dictionary mutation, and be more flexible with programing patterns
  - scopes (as dictionary vals) can be popped to the stack instead of discarded for later re-use
  - current implementation uses a hashtable (FNV hash function) with linked list buckets and a linked list of hashtables for the dictionary stack.

### compact bytecode
  - 8bit opcode leading variable length bytecode
  - native (platform dependent) types are also valid bytecode (this is where I am working right now)
    - first 8bits are opcode, remainder is fixed-length pointer-sized field with either value or pointer to struct


# What does it look like?

Concat will look familiar if you have seen other concatenative programming languages like joy, cat, or postscript.
If not, it can be a bit confusing at first, but just takes some getting used to.
When I first started concat as a full-featured programming language it was fun brain-exercise just writing (or reading) a simple function,
but once you get used to reading+writing stack code (and writing good stack comments) skimming concat code can become an intuitive process.

## Thinking stack-wise
Concat is entirely postfix -- as we encounter simple push types (e.g. string,int,float,list,dictionary,quotation) we push them onto the stack.
When we see an eval type (e.g. identifier) we evaluate it using the current contents of the stack.

```
#A simple example
#Comments start with "#"
1 2 3 #this pushes, one then two then 3 onto the stack
  #| 1 2 3  # this is a stack comment (starts with "#|")
# stack comments tell you the contents of the (top of the) stack at that point in the code
# they are your most basic tool for avoiding mental stack juggling
```

If you are used to most common programming languages with prefix functions (i.e. the function name comes before the list of arguments) and infix math (binary operators between arguments), this can seem backwards and hard to think your way through. That is the wrong way to think about it. Instead you need to get used to thinking in terms of the stack (we are building up the stack from top-to-bottom, left-to-right), and then consuming/mutating the top of the stack (from the right) when we call an operator.

From this perspective concat has a refreshing elegance. `1 2 3` means `1 2 3` will be on the stack, and if we then call `+`, `1 5` will be on the stack.

When you are writing concat code you need to answer 3 questions: **What do I have on the stack? What do I want on the stack? How do I get there?**

concat operators are all described in terms of their stack effects.

We write stack effects like `( initial stack -- final stack )`.
The stack effect for `+` looks like `( a b -- a+b )`. So if you see `1 3 +` you know that the stack will end up being `#| 4`.

If we start thinking of the stack from left-to-right (which is the way we write code), with operators consuming args from the right things start to make more sense.
```
1 2 3 #| 1 2 3
+     #| 1 5   #2+3 = 5
swap  #| 5 1
```

Concat doesn't care about whitespace, but navigating well-indented and thoughtfully grouped code is much easier than a wall of code.
Even more importantly, having stack comments at all the key points in the code lets you skim code and understand what it does without having
to go through any mental stack juggling yourself.

Up till now what we have described is a basic RPN calculator. To get to the interesting parts of concat we need to talk about quotations.
Quotations act as anonymous functions, and let us pass around/lazily evaluate code.
In concat quotations are surrounded in square brackets (`[]`), as opposed to lists, which are surrounded in parenthesis (`()`).
The evaluation of a quotation is the evaluation of its contents from left-to-right.
```
4     #| 4
[3 +] #| 4 [3 +]
eval  #| 7
```

Quotations act as anonymous functions, and let us pass around/lazily evaluate code. This is also how we implement control flow.
`ifelse` has the stack effect `( [cond] [then] [else] -- then)` if `[cond]` evaluates to true, and `( [cond] [then] [else] -- else)` otherwise.
Said differently, ifelse pops the top 3 stack vals, evals the 3rd one, then evals either the second or first depending on the results.

concat doesn't use an explicit bool type, but checks the boolean value of a val when evaluating a conditional.
Empty lists and zero-valued numbers are false, while non-empty lists and numbers other than zero are true.
Boolean operators like `<`/`=` return an integer one or zero (for true or false).

```
#One way to format ifelse
5 3 > [          #5>3
  "5>3\n" print
] [              #5<=3
  "5<=3\n" print
] ifelse         #|  #stack empty here
```

You do have to skip to the bottom to confirm what op we are calling (because concat is entirely postfix),
but with consistent formatting you get used to finding the op trailing the set of
arguments (usually quotations or escaped identifiers).

In the future I plan to add some automated analysis tools to verify stack effect comments, starting from the stack effects given in opcodes.h.

## Local variables
concat doesn't have explicit support for local variables (though I'm not completely opposed to adding some simple sugar later).
There are 2 main options for handling named locals in code -- local dictionary scopes and code-rewriting.
See locals.cat in the examples directory for examples of each.
Since quotations are really just standard lists with special evaluation rules, we can inspect/modify quotations just like we would a list.
This power allows for a variety of things that would be hard in many other languages --
for example in locals.cat we replace all references to local variables with stack manipulations and `sip`/`dip`.

## sip, dip, and apply
sip/dip (and apply) are the main tools for keeping the stack manageable.
Without them your stack (and code) can become a bloated mess of stack shuffling.
With them you can make it so at any point in the code the only things on the stack are the vals relevant to that code
`dip` under vals you don't need right now, and `sip` vals that you will need again later.

I'll add more on this to the documentation later,
but these work like sip and dip in other stack languages, and you can see many examples of their use in the examples/ directory.


# Goals:
One of the goals for this project is a cross-platform bytecode supporting resource-constrained microcontrollers
- with code as a first-class object this allows for some compelling use cases pushing code out to IoT edge
- I've implemented parts of concat on AVR (e.g. arduino), and eventually plan for a full implementation supporting both desktop CPUs and embedded MCU (except for some obvious differences like threading and other platform-specific libraries)
- one motivating use case: pushing rules for home IoT controller out to the relevant devices
    - controller is no longer single-point-of-failure, better response latency
    - e.g. light switch is supposed to turn on room light, push out rule so switch directly talks to light
        - as long as the switch can talk to the light everything should work as expected, even if internet or even controller are down/unavailable
    - e.g. alarm to turn on room lights at 6am, push out rule to light
        - light turns on at 6am, even with no internet or controller connectivity
    - e.g. push out rule(s) linking motion sensor to light
        - now light turns on in direct response to motion sensor (instead of sensor->controller->light, or even worse sensor->ctrlr->internet->ctrlr->light)

# Where is it now?
- The language is stable right now (less some additional, possibly breaking, changes on the way to 1.0)
- new 3-state VM using token-threaded dispatch
  - functionally complete other than a few standard library functions from the previous version that need to be reimplemented
  - still needs thorough testing/debugging, but is usable and passes all the basic tests
- sufficient and useful set of combinators - allows powerful control flow with the common patterns built in
- comprehensive printf
- threading and thread synchronization primitives
  - these work correctly (see various thread examples) with the exception that there are some shared global buffers for IO in the existing implementation
  - we still need a threadsafe memory pool allocator (so we can use the new memory pool code and cut down on malloc/free)
- example programs
    - quite a few already in examples/ directory
- simple debugging tools exist
    - existing tools are just a set of words added to the dictionary to manipulate/run/debug/inspect concat VMs
    - set up to easily support writing new debugging tools in concat, and/or script debugging sessions
        - e.g. we don't have breakpoints, but you can write a one-liner to step through a program until a condition is met


# Where is it going?

The next goals for concat are:
1. Finish the new VM core (cleaner/more maintainable/smaller/faster)
  - token-threaded bytecode VM instead of stack of heavy structs using function-dispatch (was easy to debug, but lots of overhead) *DONE*
  - macros to help avoid repitition of the important lists (opcodes, exceptions, type handlers) *DONE*
  - reduce redundant argument checking, and confirm all arguments are checked once *WIP, mostly done*
  - new round of profiling
  - break the "purity" of the loop natives to allow hiding args under loop on the work-stack instead of shuffling to the stack and back on every loop
2. Finish comprehensive test suite
  - enough C tests to cover all the interesting cases for the type library (especially lists/strings/idents since they drive everything)
  - concat scripts to test everything else
3. Review+finish documentation on all standard language features (stack effects, side effects)
  - also review instruction set for round of (if needed) breaking changes
4. Document all implementation rules (i.e. document how to add types/opcodes/natives)
5. proper REPL for interactive coding
6. Improve default implementation of debugging tools
  - also look into compile/debug-time checks of stack comments
7. Complete AVR implementation of core VM
8. Standardize core opcode set
9. Document design and design decisions
  - e.g. err and opcode macros, token-threading
10. version 1.0

# License

Copyright (C) 2024 D. Michael Agun

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
