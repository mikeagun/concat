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

#ifndef __VAL_FILE_INTERNAL_H__
#define __VAL_FILE_INTERNAL_H__ 1

#include "vm_err.h"

#include <stdio.h>

//typedef enum file_flags_ { NONE=0,DOCLOSE=1,FAILSTOP=2 } file_flags;

struct val_file_struct {
  //int fd;
  FILE *f;
  char *fname; //name of the file (nice for debugging and interactive coding, not actually needed, so maybe should go in debugging
  enum {NONE=0,DOCLOSE=1} flags;
  int refcount;
};

// ==== val_file_struct helpers ====

//open file in read mode
struct val_file_struct * _val_file_open(const char *fname, const char *mode);

//get file corresponding to stdin
struct val_file_struct * _val_file_stdin();

//get file corresponding to stdout
struct val_file_struct * _val_file_stdout();

//get file corresponding to stderr
struct val_file_struct * _val_file_stderr();

err_t _val_file_close(struct val_file_struct *f);
err_t _val_file_readline(struct val_file_struct *f, char *buffer, int buflen);
err_t _val_file_read(struct val_file_struct *f, char *buffer, int nbytes);
err_t _val_file_write(struct val_file_struct *f, const char *buffer, int nbytes);

#endif
