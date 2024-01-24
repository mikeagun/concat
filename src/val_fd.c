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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

//#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "vm_err.h"
#include "val_fd.h"
#include "val_num.h"
#include "val_fd_internal.h"
#include "val_string.h"
#include "val_printf.h"
//#include "val_math.h"
#include "helpers.h"

//FIXME: threadsafe fd IO

valstruct_t _val_fd_stdin = {
  .type = TYPE_FD,
  .v.fd = {
    .fd = STDIN_FILENO,
    .flags = FD_NONE,
    .refcount = 1
#ifdef DEBUG_FILENAME
    , .fname = "STDIN"
#endif
  }
};
valstruct_t _val_fd_stdout = {
  .type = TYPE_FD,
  .v.fd = {
    .fd = STDOUT_FILENO,
    .flags = FD_NONE,
    .refcount = 1
#ifdef DEBUG_FILENAME
    , .fname = "STDOUT"
#endif
  }
};
valstruct_t _val_fd_stderr = {
  .type = TYPE_FD,
  .v.fd = {
    .fd = STDERR_FILENO,
    .flags = FD_NONE,
    .refcount = 1
#ifdef DEBUG_FILENAME
    , .fname = "STDERR"
#endif
  }
};


val_t val_fd_stdin_ref() {
  return __fd_val(_val_fd_stdin_ref());
}
val_t val_fd_stdout_ref() {
  return __fd_val(_val_fd_stdout_ref());
}
val_t val_fd_stderr_ref() {
  return __fd_val(_val_fd_stderr_ref());
}

valstruct_t* _val_fd_stdin_ref() {
  refcount_inc(_val_fd_stdin.v.fd.refcount);
  return &_val_fd_stdin;
}
valstruct_t* _val_fd_stdout_ref() {
  refcount_inc(_val_fd_stdout.v.fd.refcount);
  return &_val_fd_stdout;
}
valstruct_t* _val_fd_stderr_ref() {
  refcount_inc(_val_fd_stderr.v.fd.refcount);
  return &_val_fd_stderr;
}


err_t val_fd_init_socket(val_t *val, int domain, int type, int protocol) {
  valstruct_t *v;
  if (!(v = _valstruct_alloc())) return _throw(ERR_MALLOC);
  v->type = TYPE_FD;
  if (-1 == (v->v.fd.fd = socket(domain, type, protocol))) {
    _valstruct_release(v);
    return _throw(ERR_IO_ERROR);
  } else {
#ifdef DEBUG_FILENAME
    v->v.fd.fname = NULL;
#endif
    v->v.fd.flags = FD_DOCLOSE;
    v->v.fd.refcount = 1;
    *val = __fd_val(v);
    return 0;
  }

}

err_t _val_fd_listen(valstruct_t *f, const char *host, int port, int backlog) {
   struct sockaddr_in serv_addr; 
   memset(&serv_addr, '0', sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_port = htons(port); 
   if (host == NULL) {
     serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   } else {
     if(inet_pton(AF_INET, host, &serv_addr.sin_addr)<=0) {
       return _throw(ERR_IO_ERROR); //TODO: differentiate err cases
     } 
   }
   if (-1 == bind(f->v.fd.fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) {
     return _throw(ERR_IO_ERROR); //TODO: differentiate err cases
   }

   if (-1 == listen(f->v.fd.fd, backlog)) {
     return _throw(ERR_IO_ERROR); //TODO: differentiate err cases
   }

   return 0;
}

//TODO: save sockaddr to val_t (prob string)
err_t _val_fd_accept(valstruct_t *f, val_t *conn, struct sockaddr* addr, socklen_t *addrlen) {
  valstruct_t *v;
  if (!(v = _valstruct_alloc())) return _throw(ERR_MALLOC);
  v->type = TYPE_FD;

  if (-1 == (v->v.fd.fd = accept(f->v.fd.fd, addr,addrlen))) {
    _valstruct_release(v);
    return _throw(ERR_IO_ERROR);
  } else {
#ifdef DEBUG_FILENAME
    v->v.fd.fname = NULL;
#endif
    v->v.fd.flags = FD_DOCLOSE;
    v->v.fd.refcount = 1;
    *conn = __fd_val(v);
    return 0;
  }
}

err_t _val_fd_connect(valstruct_t *f, const char *host, int port) {
  struct sockaddr_in serv_addr; 
  memset(&serv_addr, '0', sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port); 

  if(inet_pton(AF_INET, host, &serv_addr.sin_addr)<=0) {
    return _throw(ERR_IO_ERROR); //TODO: differentiate err cases
  } 

  if (-1 == connect(f->v.fd.fd, (struct sockaddr *)&serv_addr,sizeof(serv_addr))) {
    return _throw(ERR_IO_ERROR);
  } else {
    return 0;
  }
}

err_t val_fd_open(val_t *val, const char *fname, int flags, mode_t mode) {
  valstruct_t *v;
  if (!(v = _valstruct_alloc())) return _throw(ERR_MALLOC);
  v->type = TYPE_FD;


  if (-1 == (v->v.fd.fd = open(fname,flags, mode))) {
    _valstruct_release(v);
    return _throw(ERR_IO_ERROR);
  } else {
#ifdef DEBUG_FILENAME
    v->v.fd.fname = _strdup(fname);
#endif
    v->v.fd.flags = FD_DOCLOSE;
    v->v.fd.refcount = 1;
    *val = __fd_val(v);
    return 0;
  }
}


int val_fd_print(val_t val) {
#ifdef DEBUG_FILENAME
  if (__val_ptr(val)->v.fd.fname != NULL) {
    printf("fd(%s)",__val_ptr(val)->v.fd.fname);
  } else {
    printf("fd(%i)",__val_ptr(val)->v.fd.fd);
  }
#else
  printf("fd(%i)",__val_ptr(val)->v.fd.fd);
#endif
  return 0;
}

void _val_fd_destroy(valstruct_t *f) {
  if (0 == refcount_dec(f->v.fd.refcount)) {
#ifdef DEBUG_FILENAME
    free(f->v.fd.fname);
    //f->v.fd.fname = (char*)9;
#endif
    if (f->v.fd.flags & FD_DOCLOSE) close(f->v.fd.fd);
    _valstruct_release(f);
  }
}

err_t _val_fd_clone(val_t *ret, valstruct_t *orig) {
  *ret = __fd_val(orig);
  refcount_inc(orig->v.fd.refcount);
  return 0;
}

#ifdef DEBUG_FILENAME
const char *_val_fd_fname(valstruct_t *fd) {
  return fd->v.fd.fname;
}
#endif

int _val_fd_fd(valstruct_t *fd) {
  return fd->v.fd.fd;
}


//int val_fd_readline(val_t fd, char *buffer, int buflen) {
//  return _val_fd_readline(fd->val.fd,buffer,buflen);
//}
//int val_fd_writeline_val(val_t *fd, val_t *str) {
//  int r = val_string_cat(str,val_string_temp("\n"));
//  if (!r) r = val_fd_write_val(fd,str);
//  return r;
//}
//int val_fd_write_val(val_t *fd, val_t *str) {
//  int len = fwrite(val_string_str(str), val_string_len(str),1,fd->val.fd->f);
//  if (len>0) len = val_string_len(str);
//  val_destroy(str);
//  val_int_init(str,len);
//  
//  return 0;
//}

//err_t val_fd_close(val_t *fd) {
//  return _val_fd_close(fd->val.fd);
//}

//TODO: we need a way to tag vals with the fd and line number they came from in debug mode
int _val_fd_readline(valstruct_t *f, valstruct_t *buf) {
  //FIXME: handle case where line is longer than FD_BUF
  int n;
  err_t e;
  if(0>(n = _val_fd_readline_(f,FD_BUF,FD_BUFLEN))) return n;
  if ((e = _val_str_rreserve(buf,n))) return e;

  _val_str_cat_cstr(buf,FD_BUF,n);
  return n;
}
int _val_fd_read(valstruct_t *f, valstruct_t *buf, int nbytes) {
  int r;
  if ((r = _val_str_rreserve(buf,nbytes))) return r;
  if (0>(r = _val_fd_read_(f,_val_str_end(buf),FD_BUFLEN))) return r;

  buf->v.str.len += r;
  return r;
}
int _val_fd_write(valstruct_t *f, valstruct_t *buf) {
  return _val_fd_write_(f,_val_str_begin(buf),_val_str_len(buf));
}

//whence: SEEK_SET, SEEK_CURE, SEEK_END (from stdio.h)
//TODO: handle files over 4GB (32bit)
err_t _val_fd_seek(valstruct_t *f, long offset, int whence) {
  if ((0 != lseek(f->v.fd.fd,offset,whence))) {
    //TODO: process errno
    return ERR_IO_ERROR;
  } else return 0;
  //alternatively (with only SEEK_SET support): fsetpos(f->v.fd.fd,offset)
}

long _val_fd_pos(valstruct_t *f) {
  return lseek(f->v.fd.fd,0,SEEK_CUR); //TODO: handle errors
  //alternatively: long pos; if ((0 != fgetpos(f->v.fd.fd,&pos))) { return ERR_IO_ERROR; } else return pos;
}

// ==== struct val_fd helpers ====

err_t _val_fd_close(valstruct_t *f) {
  if (f->v.fd.flags & FD_DOCLOSE) {
    if (-1 == close(f->v.fd.fd)) return _throw(ERR_IO_ERROR);
    f->v.fd.flags &= ~FD_DOCLOSE;
  }
  return 0;
}

int _val_fd_readline_(valstruct_t *f, char *buffer, int buflen) {
  return _throw(ERR_NOT_IMPLEMENTED);
  //if (NULL == fgets(buffer,buflen,f->v.fd.fd)) {
  //  //if (errno == 0) return ERR_EOF;
  //  //else return _throw(ERR_IO_ERROR);
  //  if (feof(f->v.fd.fd)) return ERR_EOF;
  //  else return _throw(ERR_IO_ERROR);
  //}
  //int len = strlen(buffer);
  //return len;
}
int _val_fd_read_(valstruct_t *f, char *buffer, int nbytes) {
  ssize_t n = read(f->v.fd.fd,buffer,nbytes);
  if (n == -1) {
    return _throw(ERR_IO_ERROR);
  } else if (n == 0) {
    return ERR_EOF;
  } else {
    return (int)n;
  }
}
int _val_fd_write_(valstruct_t *f, const char *buffer, int nbytes) {
  ssize_t n = write(f->v.fd.fd,buffer,nbytes);
  if (n == -1) {
    return _throw(ERR_IO_ERROR);
  } else {
    return (int)n;
  }
}


int val_fd_fprintf(valstruct_t *v,FILE *file, const struct printf_fmt *fmt) {
  int r;
#ifdef DEBUG_FILENAME
  if (v->v.fd.fname != NULL) {
    r = fprintf(file,"fd(%s)",v->v.fd.fname);
  } else {
    r = fprintf(file,"fd(%i)",v->v.fd.fd);
  }
#else
  r = fprintf(file,"fd(%i)",v->v.fd.fd);
#endif
  return r >= 0 ? r : _throw(ERR_IO_ERROR);
}
int val_fd_sprintf(valstruct_t *v,valstruct_t *buf, const struct printf_fmt *fmt) {
  int r,rlen=0;
  if (0>(r = val_sprint_cstr(buf,"fd("))) return r;
  rlen += r;
#ifdef DEBUG_FILENAME
  if (v->v.fd.fname != NULL) {
    if (0>(r = val_sprint_cstr(buf,v->v.fd.fname))) return r;
  } else {
    if (0>(r = val_int32_sprintf(v->v.fd.fd,buf,fmt))) return r;
  }
#else
  if (0>(r = val_int32_sprintf(v->v.fd.fd,buf,fmt))) return r;
#endif
  rlen += r;
  if (0>(r = val_sprint_cstr(buf,")"))) return r;
  rlen += r;
  return rlen;
}
