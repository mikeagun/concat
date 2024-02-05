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

#ifndef _VAL_FILE_H_
#define _VAL_FILE_H_ 1
// val_file.h - concat file val interface (holds c FILE*)

#include "val.h"
//#include "vm.h"


//FIXME: document file val interface

//TODO: come up with buffer/block manager for files
//TODO: support threadsafe file io
#define FILE_BUFLEN 500
char FILE_BUF[FILE_BUFLEN];

extern valstruct_t _val_file_stdin;
extern valstruct_t _val_file_stdout;
extern valstruct_t _val_file_stderr;

err_t concat_file_init();

val_t val_file_stdin_ref();
val_t val_file_stdout_ref();
val_t val_file_stderr_ref();

valstruct_t * _val_file_stdin_ref();
valstruct_t * _val_file_stdout_ref();
valstruct_t * _val_file_stderr_ref();


int val_file_init(val_t *val, const char *fname, const char *mode);

int val_file_print(val_t val);
void _val_file_destroy(valstruct_t *f);
err_t _val_file_clone(val_t *ret, valstruct_t *orig);

#ifdef DEBUG_FILENAME
const char *val_file_fname(val_t *file);
#endif

FILE* _val_file_f(valstruct_t *file);

//close file
int _val_file_close(valstruct_t *f);

int _val_file_readline(valstruct_t *f, valstruct_t *buf);
int _val_file_read(valstruct_t *f, valstruct_t *buf, int nbytes);
int _val_file_write(valstruct_t *f, valstruct_t *buf);

err_t _val_file_seek(valstruct_t *f, long offset, int whence);
long _val_file_pos(valstruct_t *f);

//read one line from file
int _val_file_readline_(valstruct_t *f, char *buffer, int buflen);

//read n bytes from file
int _val_file_read_(valstruct_t *f, char *buffer, int nbytes);

//write n bytes to file
int _val_file_write_(valstruct_t *f, const char *buffer, int nbytes);


//int val_file_readline(val_t *file, char *buffer, int buflen);
//int val_file_readline_val(val_t *file, val_t *line);
//int val_file_read_val(val_t *file, val_t *str, unsigned int nbytes);
//int val_file_writeline_val(val_t *file, val_t *str);
//int val_file_write_val(val_t *file, val_t *str);
//int _val_file_readline_val(val_t *file, val_t *line);
//int _val_file_close(valstruct_t *file);

int val_file_fprintf(valstruct_t *v,FILE *file, const struct printf_fmt *fmt);
int val_file_sprintf(valstruct_t *v,valstruct_t *buf, const struct printf_fmt *fmt);

#endif
