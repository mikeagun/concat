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

#include "vm.h"
#include "val_file.h"
#include "val_list.h"
#include "vm_err.h"
#include "vm_debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

//This file describes the main interpreter for concat
//  - takes command line args
//  - sets up debugging
//  - sets interactive mode (read from stdin, restart VM on exception)
//  - takes -e "expression" args for commandline concat code to eval (append onto work stack)
//  - takes -f filename args for files to append onto the work stack
//concat interpreter
//- TODO: finish bytecode, and implement ephemeral/persistant bytecode -- light embedded vms may only read vals/bytecode (not parse text)
//- TODO: optimization (code optimization tools and controls for when optimization performed)
//  - figure out natives needed, rest can be implemented as concat scripts/libraries
//  - already have natives for inlining, need tools for typical use cases
//- TODO: proper getline (or other pretty repl) interface to the concat interpreter

//int readline(char *buffer, int size) {
//  if (!fgets(buffer,size,stdin)) {
//    if (errno != 0) {
//      perror("Read failed");
//    }
//    return -1;
//  }
//  int len = strlen(buffer);
//  return len;
//}

int main(int argc, char *argv[]) {
  vm_t vm;
  err_t r;
  val_t t;

  if (vm_init(&vm)) {
    fprintf(stderr,"Failed to initialize vm\n");
    return 1;
  }

  //char buffer[512];
  //int n;
  //int eof=0;
  //while(!eof && 0 <= (n = readline(buffer,512))) {
  //  if (buffer[n-1] == '\n') buffer[--n] = '\0';
  //  else eof=1;
  //  //calc_eval(c,buffer);
  //  err_t ret = vm_evalraw(&vm,buffer);
  //  if (err_t) {
  //    printf("parse error: %d\n", ret);
  //  }

  //  //if (stack_size(c)>0) fprintf(stdout,"top=%f\n",s_top());
  //  //else fprintf(stdout,"top=empty\n");
  //}

  int expr=0;
  int argi=1;

  int interactive=0; //interactive means instead of stopping on error, clear oustanding work and grab a new stdin (retains stack and dictionary)

  while(argi<argc) {
    const char *arg = argv[argi++];
    if (arg[0] == '-' && arg[1] != '\0') { //first char is -, and there are at least two chars
      if (!strcmp(arg,"-e")) {
        if (argi < argc) {
          arg = argv[argi++];
          expr=1;
          val_code_init(&t);
          r = vm_compile_input(&vm,arg,strlen(arg),&t);
          if (r) {
            printf("ERROR: failed to compile expression\n"); return 1;
          }
          r = vm_wappend(&vm,&t);
          if (r) {
            printf("ERROR: failed to append expr to work\n"); return 1;
          }
        } else {
          printf("ERROR: missing argument to -e (should be code to run)\n"); return 1;
        }
      } else if (!strcmp(arg,"-f")) {
        if (argi < argc) {
          arg = argv[argi++];
          expr=1;
          val_t file;
          if ((r = val_file_init(&file,arg,"r"))
             || (r = vm_wappend(&vm,&file))) {
            printf("failed to open file '%s'\n",arg);
            return 1;
          }
        } else {
          printf("ERROR: missing argument to -f (should be file to run)\n");
          return 1;
        }
      } else if (!strcmp(arg,"-d")) {
        r = vm_debug_wrap(&vm);
        if (r) {
          printf("ERROR: could not set debugger\n");
          return 1;
        }
        expr=0; //reset expr so final debugger will get stdin (unless we give it expr)
      } else if (!strcmp(arg,"-x")) { //evaluate all queued work up to this point
        if ((r = vm_dowork(&vm,-1))) {
          err_fprintf(stderr,r);
          printf("ERROR in pre-evaluation -x\n");
          return r;
        }
      } else if (!strcmp(arg,"-de")) { //trap out to debugger on unhandled exception
        r = vm_cpush_debugfallback(&vm);
        if (r) {
          printf("ERROR: failed to set debug-catch mode");
          return 1;
        }
      } else if (!strcmp(arg,"--")) {
        break;
      } else {
        printf("ERROR: Unknown argument '%s'\n", arg);
        return 1;
      }
    } else if (arg[0] == '-') { //single -, grab stdin
      r = vm_wappend(&vm,val_file_init_stdin(&t));
      interactive=1;
      if (r) {
        printf("ERROR: could load stdin\n");
        return 1;
      }
    } else {
      argi--;
      break;
    }
  }

  if (argi<argc) {
    while(argi<argc) {
      const char *arg = argv[argi++];
      if (!strcmp(arg,"-")) {
        r = vm_wappend(&vm,val_file_init_stdin(&t));
        interactive=1;
        if (r) {
          printf("ERROR: could load stdin\n");
          return 1;
        }
      } else {
        val_t tfile;
        if ((r = val_file_init(&tfile,arg,"r"))) {
          printf("ERROR: failed to open file '%s'\n",arg);
          return 1;
        } else if ((r = vm_wappend(&vm,&tfile))) {
          val_destroy(&tfile);
          printf("ERROR appending work\n");
          return 1;
        }
      }
    }
  } else if (!expr) { //if no args just read stdin
    r = vm_wappend(&vm,val_file_init_stdin(&t));
    interactive=1;
    if (r) {
      printf("ERROR: could load stdin\n");
      return 1;
    }
  }
  if (interactive) {
    r = vm_set_interactive_debug(&vm);
    if (r) {
      vm_pfatal(r);
      return r;
    }
  }

  r = vm_dowork(&vm,-1);
  if (err_isfatal(r)) {
    vm_pfatal(r);
    return -1;
  } else if (r) vm_perror(&vm);

  vm_destroy(&vm);
  return r;
}
