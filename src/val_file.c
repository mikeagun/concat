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

#include "vm_err.h"
#include "val_file.h"
#include "val_file_internal.h"
#include "val_string.h"
#include "val_printf.h"
//#include "val_math.h"
#include "helpers.h"

//FIXME: threadsafe file IO

valstruct_t _val_file_stdin = {
  .type = TYPE_FILE,
  .v.file = {
    .f = NULL,
    .flags = NONE,
    .refcount = 1
#ifdef DEBUG_FILENAME
    , .fname = "STDOUT"
#endif
  }
};
valstruct_t _val_file_stdout = {
  .type = TYPE_FILE,
  .v.file = {
    .f = NULL,
    .flags = NONE,
    .refcount = 1
#ifdef DEBUG_FILENAME
    , .fname = "STDIN"
#endif
  }
};
valstruct_t _val_file_stderr = {
  .type = TYPE_FILE,
  .v.file = {
    .f = NULL,
    .flags = NONE,
    .refcount = 1
#ifdef DEBUG_FILENAME
    , .fname = "STDERR"
#endif
  }
};



err_t concat_file_init() {
  _val_file_stdin.v.file.f = stdin;
  _val_file_stdout.v.file.f = stdout;
  _val_file_stderr.v.file.f = stderr;
  return 0;
}

val_t val_file_stdin_ref() {
  return __file_val(_val_file_stdin_ref());
}
val_t val_file_stdout_ref() {
  return __file_val(_val_file_stdout_ref());
}
val_t val_file_stderr_ref() {
  return __file_val(_val_file_stderr_ref());
}

valstruct_t* _val_file_stdin_ref() {
  refcount_inc(_val_file_stdin.v.file.refcount);
  return &_val_file_stdin;
}
valstruct_t* _val_file_stdout_ref() {
  refcount_inc(_val_file_stdout.v.file.refcount);
  return &_val_file_stdout;
}
valstruct_t* _val_file_stderr_ref() {
  refcount_inc(_val_file_stderr.v.file.refcount);
  return &_val_file_stderr;
}



int val_file_init(val_t *val, const char *fname, const char *mode) {
  valstruct_t *v;
  if (!(v = _valstruct_alloc())) return _throw(ERR_MALLOC);
  v->type = TYPE_FILE;


  if (!(v->v.file.f = fopen(fname,mode))) {
    _valstruct_release(v);
    return _throw(ERR_IO_ERROR);
  } else {
#ifdef DEBUG_FILENAME
    v->v.file.fname = _strdup(fname);
#endif
    v->v.file.flags = DOCLOSE;
    v->v.file.refcount = 1;
    *val = __file_val(v);
    return 0;
  }
}


int val_file_print(val_t val) {
#ifdef DEBUG_FILENAME
  printf("file(%s)",__val_ptr(val)->v.file.fname);
#else
  printf("file(%p)",__val_ptr(val)->v.file.f);
#endif
  return 0;
}

void _val_file_destroy(valstruct_t *f) {
  if (0 == refcount_dec(f->v.file.refcount)) {
#ifdef DEBUG_FILENAME
    free(f->v.file.fname);
    //f->v.file.fname = (char*)9;
#endif
    if (f->v.file.flags & DOCLOSE) fclose(f->v.file.f);
    _valstruct_release(f);
  }
}

err_t _val_file_clone(val_t *ret, valstruct_t *orig) {
  *ret = __file_val(orig);
  refcount_inc(orig->v.file.refcount);
  return 0;
}

#ifdef DEBUG_FILENAME
const char *_val_file_fname(valstruct_t *file) {
  return file->v.file.fname;
}
#endif

FILE* _val_file_f(valstruct_t *file) {
  return file->v.file.f;
}


//int val_file_readline(val_t file, char *buffer, int buflen) {
//  return _val_file_readline(file->val.file,buffer,buflen);
//}
//int val_file_writeline_val(val_t *file, val_t *str) {
//  int r = val_string_cat(str,val_string_temp("\n"));
//  if (!r) r = val_file_write_val(file,str);
//  return r;
//}
//int val_file_write_val(val_t *file, val_t *str) {
//  int len = fwrite(val_string_str(str), val_string_len(str),1,file->val.file->f);
//  if (len>0) len = val_string_len(str);
//  val_destroy(str);
//  val_int_init(str,len);
//  
//  return 0;
//}

//err_t val_file_close(val_t *file) {
//  return _val_file_close(file->val.file);
//}

//TODO: we need a way to tag vals with the file and line number they came from in debug mode
int _val_file_readline(valstruct_t *f, valstruct_t *buf) {
  //FIXME: handle case where line is longer than FILE_BUF
  int n;
  err_t e;
  if(0>(n = _val_file_readline_(f,FILE_BUF,FILE_BUFLEN))) return n;
  if ((e = _val_str_rreserve(buf,n))) return e;

  _val_str_cat_cstr(buf,FILE_BUF,n);
  return n;
}
int _val_file_read(valstruct_t *f, valstruct_t *buf, int nbytes) {
  int r;
  if ((r = _val_str_rreserve(buf,nbytes))) return r;
  if (0>(r = _val_file_read_(f,_val_str_end(buf),FILE_BUFLEN))) return r;

  buf->v.str.len += r;
  return r;
}
int _val_file_write(valstruct_t *f, valstruct_t *buf) {
  return _val_file_write_(f,_val_str_begin(buf),_val_str_len(buf));
}

//whence: SEEK_SET, SEEK_CURE, SEEK_END (from stdio.h)
//TODO: handle files over 4GB (32bit)
err_t _val_file_seek(valstruct_t *f, long offset, int whence) {
  if ((0 != fseek(f->v.file.f,offset,whence))) {
    //TODO: process errno
    return ERR_IO_ERROR;
  } else return 0;
  //alternatively (with only SEEK_SET support): fsetpos(f->v.file.f,offset)
}

long _val_file_pos(valstruct_t *f) {
  return ftell(f->v.file.f); //TODO: handle errors
  //alternatively: long pos; if ((0 != fgetpos(f->v.file.f,&pos))) { return ERR_IO_ERROR; } else return pos;
}

// ==== struct val_file helpers ====

err_t _val_file_close(valstruct_t *f) {
  if (f->v.file.flags & DOCLOSE) {
    if (EOF == fclose(f->v.file.f)) return _throw(ERR_IO_ERROR);
    f->v.file.flags &= ~DOCLOSE;
  }
  return 0;
}

int _val_file_readline_(valstruct_t *f, char *buffer, int buflen) {
  if (NULL == fgets(buffer,buflen,f->v.file.f)) {
    //if (errno == 0) return ERR_EOF;
    //else return _throw(ERR_IO_ERROR);
    if (feof(f->v.file.f)) return ERR_EOF;
    else return _throw(ERR_IO_ERROR);
  }
  int len = strlen(buffer);
  return len;
}
int _val_file_read_(valstruct_t *f, char *buffer, int nbytes) {
  ssize_t n = fread(buffer,1,nbytes,f->v.file.f);
  if (n == 0) {
    if (feof(f->v.file.f)) return ERR_EOF;
    else return _throw(ERR_IO_ERROR);
  } else {
    return (int)n;
  }
}
int _val_file_write_(valstruct_t *f, const char *buffer, int nbytes) {
  ssize_t n = fwrite(buffer, 1,nbytes,f->v.file.f);
  if (!n) {
    if (feof(f->v.file.f)) return _throw(ERR_EOF);
    else if (ferror(f->v.file.f)) return _throw(ERR_IO_ERROR);
  }
  return (int)n;
}


int val_file_fprintf(valstruct_t *v,FILE *file, const struct printf_fmt *fmt) {
  int r;
#ifdef DEBUG_FILENAME
  r = fprintf(file,"file(%s)",v->v.file.fname);
#else
  r = fprintf(file,"file(%p)",v->v.file.f);
#endif
  return r >= 0 ? r : _throw(ERR_IO_ERROR);
}
int val_file_sprintf(valstruct_t *v,valstruct_t *buf, const struct printf_fmt *fmt) {
  int r,rlen=0;
  if (0>(r = val_sprint_cstr(buf,"file("))) return r;
  rlen += r;
#ifdef DEBUG_FILENAME
  if (0>(r = val_sprint_cstr(buf,v->v.file.fname))) return r;
#else
  if (0>(r = val_sprint_ptr(buf,v->v.file.f))) return r;
#endif
  rlen += r;
  if (0>(r = val_sprint_cstr(buf,")"))) return r;
  rlen += r;
  return rlen;
}
