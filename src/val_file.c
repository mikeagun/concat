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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "val_file.h"
#include "val_file_internal.h"
#include "val_string.h"
#include "val_num.h"
#include "helpers.h"
#include "ops.h"
#include "vm_err.h"
#include "val_printf.h"

val_t* val_file_init_stdin(val_t *val) {
  val->val.file = _val_file_stdin();
  val->type = TYPE_FILE;
  return val;
}
val_t* val_file_init_stdout(val_t *val) {
  val->val.file = _val_file_stdout();
  val->type = TYPE_FILE;
  return val;
}
val_t* val_file_init_stderr(val_t *val) {
  val->val.file = _val_file_stderr();
  val->type = TYPE_FILE;
  return val;
}



err_t val_file_init(val_t *val, const char *fname, const char *mode) {
  throw_if(ERR_IO_ERROR,!(val->val.file = _val_file_open(fname,mode)));
  val->type = TYPE_FILE;
  return 0;
}

err_t val_file_destroy(val_t *val) {
  val->type = VAL_INVALID;
  return val_file_close(val); 
}

err_t val_file_clone(val_t *ret, val_t *orig) {
  *ret = *orig;
  refcount_inc(ret->val.file->refcount);
  return 0;
}

int val_file_fprintf(val_t *val, FILE *file, const fmt_t *fmt) {
  fprintf(file,"file(%s)",val->val.file->fname);
  return 0;
}
int val_file_sprintf(val_t *val, val_t *buf, const fmt_t *fmt) {
  int r,rlen=0;
  if (0>(r = val_sprint_cstr(buf,"file("))) return r;
  rlen += r;
  if (0>(r = val_sprint_cstr(buf,val->val.file->fname))) return r;
  rlen += r;
  if (0>(r = val_sprint_cstr(buf,")"))) return r;
  rlen += r;
  return rlen;
}

void val_file_init_handlers(struct type_handlers *h) {
  h->destroy = val_file_destroy;
  h->clone = val_file_clone;
  h->fprintf = val_file_fprintf;
  h->sprintf = val_file_sprintf;
}

const char *val_file_fname(val_t *file) {
  return file->val.file->fname;
}

int val_file_readline_(val_t *file, char *buffer, int buflen) {
  return _val_file_readline(file->val.file,buffer,buflen);
}
int val_file_readline(val_t *file, val_t *line) {
  int n = _val_file_readline(file->val.file,FILE_BUF,FILE_BUFLEN);
  if (n<0) return n;

  int ret = val_string_init_(line,FILE_BUF,n); 
  if (ret) return ret;
  else return n;
}
int val_file_read(val_t *file, val_t *str, unsigned int nbytes) {
  if (nbytes > FILE_BUFLEN) return -1; //TODO: support large reads, and read directly into str buffer
  int n = _val_file_read(file->val.file,FILE_BUF,nbytes);
  if (n<0) return n;

  int ret = val_string_init_(str,FILE_BUF,n); 
  if (ret) return ret;
  else return n;
}
err_t val_file_writeline(val_t *file, val_t *str) {
  val_t nl;
  err_t r;
  if ((r = val_string_init_(&nl,"\n",1))) return r; //TODO: special case for nl
  if ((r = val_string_cat(str,&nl))) return r;
  if ((r = val_file_write(file,str))) return r;
  return 0;
}
err_t val_file_write(val_t *file, val_t *str) {
  int len = fwrite(val_string_str(str), val_string_len(str),1,file->val.file->f);
  if (len>0) len = val_string_len(str);
  val_destroy(str);
  val_int_init(str,len);
  
  return 0;
}

int val_file_close(val_t *file) {
  return _val_file_close(file->val.file);
}


// ==== struct val_file helpers ====

struct val_file_struct * _val_file_open(const char *fname, const char *mode) {
  struct val_file_struct *fs = malloc(sizeof(struct val_file_struct));
  if (!fs) return NULL;

  //fs->fd = open(fname,O_RDONLY);
  fs->f = fopen(fname,mode);
  //if (fs->fd != -1) {
  if (fs->f != NULL) {
    fs->fname = _strdup(fname);
    fs->flags = DOCLOSE;
    fs->refcount = 1;
    return fs;
  } else {
    free(fs);
    return NULL;
  }
}

struct val_file_struct* _val_file_stdin() {
  static int init = 0;
  static struct val_file_struct fs;
  if (!init) {
    fs.f = stdin;
    fs.fname = "STDIN";
    fs.flags = NONE;
    fs.refcount=1; //since refcount starts from 1, it will never be freed
    init=1;
  }
  refcount_inc(fs.refcount);
  return &fs;
}
struct val_file_struct * _val_file_stdout() {
  static int init = 0;
  static struct val_file_struct fs;
  if (!init) {
    fs.f = stdout;
    fs.fname = "STDOUT";
    fs.flags = NONE;
    fs.refcount=1; //since refcount starts from 1, it will never be freed
    init=1;
  }
  refcount_inc(fs.refcount);
  return &fs;
}
struct val_file_struct * _val_file_stderr() {
  static int init = 0;
  static struct val_file_struct fs;
  if (!init) {
    fs.f = stderr;
    fs.fname = "STDERR";
    fs.flags = NONE;
    fs.refcount=1; //since refcount starts from 1, it will never be freed
    init=1;
  }
  refcount_inc(fs.refcount);
  return &fs;
}

err_t _val_file_close(struct val_file_struct *f) {
  if (0 == refcount_dec(f->refcount)) {
    int r=0;
    free(f->fname);
    f->fname = NULL;
    if (f->flags & DOCLOSE) {
      if (EOF == (r = fclose(f->f))) {
        // handle errno here if we want to know why
        r = _throw(ERR_IO_ERROR);
      }
    }
    free(f);
    return r;
  } else {
    return 0;
  }
}

int _val_file_readline(struct val_file_struct *f, char *buffer, int buflen) {
  if (fgets(buffer,buflen,f->f)) return strlen(buffer);
  else if (feof(f->f)) return FILE_EOF;
  else return FILE_ERR;
}
int _val_file_read(struct val_file_struct *f, char *buffer, int nbytes) {
  int r;
  if ((r = fread(buffer,1,nbytes,f->f))) return r;
  else if (feof(f->f)) return FILE_EOF;
  else return FILE_ERR;
}
int _val_file_write(struct val_file_struct *f, const char *buffer, int nbytes) {
  int r;
  if ((r = fwrite(buffer,1,nbytes,f->f))) return r;
  else if (feof(f->f)) return FILE_EOF;
  else return FILE_ERR;
}
