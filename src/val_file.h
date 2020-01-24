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

#ifndef _VAL_FILE_H_
#define _VAL_FILE_H_ 1

#include "val.h"
#include "vm.h"

#define FILE_ERR -1
#define FILE_EOF -2

//TODO: come up with buffer/block manager for files
#define FILE_BUFLEN 500
char FILE_BUF[FILE_BUFLEN];

val_t* val_file_init_stdin(val_t *val);
val_t* val_file_init_stdout(val_t *val);
val_t* val_file_init_stderr(val_t *val);
err_t val_file_init(val_t *val, const char *fname, const char *mode);

err_t val_file_destroy(val_t *val);
err_t val_file_clone(val_t *ret, val_t *orig);

int val_file_fprintf(val_t *val, FILE *file, const fmt_t *fmt);
int val_file_sprintf(val_t *val, val_t *buf, const fmt_t *fmt);
void val_file_init_handlers(struct type_handlers *h);

const char *val_file_fname(val_t *file);

int val_file_readline_(val_t *file, char *buffer, int buflen);
err_t val_file_readline(val_t *file, val_t *line);
int val_file_read(val_t *file, val_t *buf, unsigned int nbytes);

err_t val_file_writeline(val_t *file, val_t *str);
err_t val_file_write(val_t *file, val_t *str);

err_t val_file_close(val_t *file);

#endif
