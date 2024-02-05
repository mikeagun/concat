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

#ifndef _VAL_FD_H_
#define _VAL_FD_H_ 1
// val_fd.h - concat fd (file descriptor) val interface (holds int file descriptor)

#include "val.h"
#include <sys/socket.h>
//#include "vm.h"


//FIXME: document fd val interface


//TODO: come up with buffer/block manager for files
//TODO: support threadsafe file io
#define FD_BUFLEN 500
char FD_BUF[FD_BUFLEN];

extern valstruct_t _val_fd_stdin;
extern valstruct_t _val_fd_stdout;
extern valstruct_t _val_fd_stderr;

val_t val_fd_stdin_ref();
val_t val_fd_stdout_ref();
val_t val_fd_stderr_ref();

valstruct_t * _val_fd_stdin_ref();
valstruct_t * _val_fd_stdout_ref();
valstruct_t * _val_fd_stderr_ref();


err_t val_fd_init_socket(val_t *val, int domain, int type, int protocol);
err_t _val_fd_listen(valstruct_t *f, const char *host, int port, int backlog);
err_t _val_fd_accept(valstruct_t *f, val_t *conn, struct sockaddr* addr, socklen_t *addrlen);
err_t _val_fd_connect(valstruct_t *f, const char *host, int port);

err_t val_fd_open(val_t *val, const char *fname, int flags, mode_t mode);

int val_fd_print(val_t val);
void _val_fd_destroy(valstruct_t *f);
err_t _val_fd_clone(val_t *ret, valstruct_t *orig);

#ifdef DEBUG_FILENAME
const char *val_fd_fname(val_t *fd);
#endif

FILE* _val_fd_f(valstruct_t *fd);

//close fd
int _val_fd_close(valstruct_t *f);

int _val_fd_readline(valstruct_t *f, valstruct_t *buf);
int _val_fd_read(valstruct_t *f, valstruct_t *buf, int nbytes);
int _val_fd_write(valstruct_t *f, valstruct_t *buf);

err_t _val_fd_seek(valstruct_t *f, long offset, int whence);
long _val_fd_pos(valstruct_t *f);

//read one line from fd
int _val_fd_readline_(valstruct_t *f, char *buffer, int buflen);

//read n bytes from fd
int _val_fd_read_(valstruct_t *f, char *buffer, int nbytes);

//write n bytes to fd
int _val_fd_write_(valstruct_t *f, const char *buffer, int nbytes);


//int val_fd_readline(val_t *fd, char *buffer, int buflen);
//int val_fd_readline_val(val_t *fd, val_t *line);
//int val_fd_read_val(val_t *fd, val_t *str, unsigned int nbytes);
//int val_fd_writeline_val(val_t *fd, val_t *str);
//int val_fd_write_val(val_t *fd, val_t *str);
//int _val_fd_readline_val(val_t *fd, val_t *line);
//int _val_fd_close(valstruct_t *fd);

int val_fd_fprintf(valstruct_t *v,FILE *file, const struct printf_fmt *fmt);
int val_fd_sprintf(valstruct_t *v,valstruct_t *buf, const struct printf_fmt *fmt);

#endif
