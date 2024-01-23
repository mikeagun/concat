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

#include "val_printf.h"
#include "val_printf_helpers.h"
#include "val_math.h"
#include "val_string.h"
#include "val_list.h"
//#include "val_file.h"
#include "helpers.h"
#include "vm_err.h"
#include <ctype.h> //for character classes
#include <string.h> //for strlen
#include <stdarg.h> //for vararg

//standard print formats
const fmt_t _fmt_v = {
  .spec = "v",
  .speclen = 1,
  .field_width = -1,
  .precision = -1,
  .flags = 0,
  .length_mod = PRINTF_none,
  .conversion = 'v'
};

const fmt_t _fmt_V = {
  .spec = "V",
  .speclen = 1,
  .field_width = -1,
  .precision = -1,
  .flags = 0,
  .length_mod = PRINTF_none,
  .conversion = 'V'
};

const fmt_t *fmt_v = &_fmt_v; //standard print format
const fmt_t *fmt_V = &_fmt_V; //verbose/code print format

void fmt_clear(fmt_t *fmt) {
  fmt->spec=NULL;
  fmt->speclen=0;
  fmt->field_width = -1;
  fmt->precision = -1;
  fmt->flags = 0;
  fmt->length_mod = PRINTF_none;
  fmt->conversion = '\0';
}

void fmt_fprint(FILE *file,const fmt_t *fmt) { //for debugging
  fprintf(file,"{");
  if (fmt->flags & PRINTF_F_ALT)    fprintf(file,"#");
  if (fmt->flags & PRINTF_F_ZERO)   fprintf(file,"0");
  if (fmt->flags & PRINTF_F_MINUS)  fprintf(file,"-");
  if (fmt->flags & PRINTF_F_SPACE)  fprintf(file," ");
  if (fmt->flags & PRINTF_F_PLUS)   fprintf(file,"+");
  if (fmt->flags & PRINTF_F_SQUOTE) fprintf(file,"'");
  //if (fmt->flags & PRINTF_F_I)      fprintf(file,"I");
  if (fmt->flags & PRINTF_F_POP)    fprintf(file,"_");
  if (fmt->flags & PRINTF_F_BINARY) fprintf(file,"b");
  if (fmt->field_width >= 0) fprintf(file,"%d",fmt->field_width);
  if (fmt->precision >= 0) fprintf(file,".%d",fmt->precision);
  switch(fmt->length_mod) {
    case PRINTF_none: break;
    case PRINTF_hh: fprintf(file,"hh");
    case PRINTF_h:  fprintf(file,"h");
    case PRINTF_l:  fprintf(file,"l");
    case PRINTF_ll: fprintf(file,"ll");
    case PRINTF_L:  fprintf(file,"L");
    case PRINTF_j:  fprintf(file,"j");
    case PRINTF_z:  fprintf(file,"z");
    case PRINTF_t:  fprintf(file,"t");
    default: fprintf(file,"BADLEN");
  }
  fprintf(file,"%c}",fmt->conversion);
}

int isflag(char c) {
  return c=='#' || c=='0' || c=='-' || c==' ' || c=='+' || c=='b';
}


err_t val_printv(val_t val) {
  err_t e;
  if (0>(e = val_fprintf_(val,stdout,fmt_v))) return e;
  if (0>(e = val_fprint_ch(stdout,'\n'))) return e;
  return 0;
}
err_t val_printV(val_t val) {
  err_t e;
  if (0>(e = val_fprintf_(val,stdout,fmt_V))) return e;
  if (0>(e = val_fprint_ch(stdout,'\n'))) return e;
  return 0;
}
err_t val_printv_(val_t val) {
  err_t e;
  if (0>(e = val_fprintf_(val,stdout,fmt_v))) return e;
  return 0;
}
err_t val_printV_(val_t val) {
  err_t e;
  if (0>(e = val_fprintf_(val,stdout,fmt_V))) return e;
  return 0;
}

//TODO: from old struct-val impl
//int val_sprintf_truncated_(val_t *val, val_t *buf, const fmt_t *fmt, int min_field_width, int max_chars, int left_align) {
//  //TODO: refactor -- at least some of the args are probably optional (pull from fmt, which we currently ignore)
//  if (max_chars==0) return 0;
//  else if (max_chars>0 && max_chars<3) return _fatal(ERR_BADARGS);
//
//  fmt_t tfmt = printf_fmt_V;
//  tfmt.field_width = min_field_width;
//
//  if (val_isstring(val)) tfmt.precision = max_chars;
//
//  int r,ret;
//  val_t tbuf;
//  val_string_init(&tbuf);
//  if (0>(r = val_sprintf_(val,&tbuf,&tfmt))) goto val_sprintf_truncated_bad;
//  ret = r;
//
//  if (max_chars > 0 && val_string_len(&tbuf) > (unsigned int)max_chars) { //need to truncate
//    ret = max_chars;
//    if (max_chars==3) { //if max_chars==3 (min valid arg), just sprint ...
//      val_string_clear(&tbuf);
//      r = val_sprint_cstr(&tbuf,"..."); 
//    } else {
//      //TODO: special case for VM?
//      if (val_iscoll(val) || val_isbytecode(val)) {
//        if ((r = val_string_substring(&tbuf,0,max_chars-4))) goto val_sprintf_truncated_bad; //remove 4 extra chars for ... and close
//        if (max_chars == 4) { 
//          switch(val->type) {
//            case TYPE_STRING: r = val_sprint_cstr(&tbuf,"\"..\""); break;
//            case TYPE_LIST:r = val_sprint_cstr(&tbuf,"(..)"); break;
//            case TYPE_CODE: r = val_sprint_cstr(&tbuf,"[..]"); break;
//            //case TYPE_BYTECODE: r = val_sprint_cstr(&tbuf,"(..)"); break;
//            default: return _fatal(ERR_FATAL); //shouldn't get here
//          }
//        } else {
//          switch(val->type) {
//            case TYPE_STRING: r = val_sprint_cstr(&tbuf,"\"..."); break;
//            case TYPE_LIST:r = val_sprint_cstr(&tbuf,"...)"); break;
//            case TYPE_CODE: r = val_sprint_cstr(&tbuf,"...]"); break;
//            //case TYPE_BYTECODE: r = val_sprint_cstr(&tbuf,"...)"); break;
//            default: return _fatal(ERR_FATAL); //shouldn't get here
//          }
//        }
//      } else {
//        val_string_substring(&tbuf,0,max_chars-3);
//        r = val_sprint_cstr(&tbuf,"...");
//      }
//    }
//  }
//  if (r<0) goto val_sprintf_truncated_bad;
//
//  if (min_field_width > 0 && val_string_len(&tbuf) < (unsigned int)min_field_width) { //need padding
//    if (left_align) {
//      r = val_sprintf_pad(&tbuf,&tfmt,val_string_len(&tbuf));
//    } else {
//      r = val_sprintf_padleft(&tbuf,&tfmt,val_string_len(&tbuf));
//    }
//    if (r<0) goto val_sprintf_truncated_bad;
//    ret = r;
//  }
//
//  if ((r = val_string_cat(buf,&tbuf))) goto val_sprintf_truncated_bad;
//  return ret;
//
//val_sprintf_truncated_bad:
//  val_destroy(&tbuf);
//  return r;
//}




int val_fprint_(FILE *file, const char *str, unsigned int len) {
  int r;
  throw_if(ERR_IO_ERROR,0>(r=fprintf(file,"%.*s",len,str)));
  return r;
}
int val_fprint_cstr(FILE *file, const char *str) {
  int r;
  throw_if(ERR_IO_ERROR,0>(r=fprintf(file,"%s",str)));
  return r;
}
int val_fprint_ch(FILE *file, char c) {
  int r;
  throw_if(ERR_IO_ERROR,0>(r=fprintf(file,"%c",c)));
  return r;
}
int val_fprint_ptr(FILE *file, void *p) {
  int r;
  throw_if(ERR_IO_ERROR,0>(r=fprintf(file,"%p",p)));
  return r;
}

int val_sprint_(valstruct_t *buf, const char *str, unsigned int len) {
  if (buf) {
    int r;
    if ((r = _val_str_cat_cstr(buf,str,len))) return r;
    else return len;
  } else {
    return len;
  }
}
int val_sprint_cstr(valstruct_t *buf, const char *str) {
  int len = strlen(str);
  if (buf) {
    int r;
    if ((r = _val_str_cat_cstr(buf,str,len))) return r;
    else return len;
  } else {
    return len;
  }
}


int val_sprint_ch(valstruct_t *buf, char c) {
  if (buf) {
    int r;
    if ((r = _val_str_cat_ch(buf,c))) return r;
  }
  return 1;
}
//int val_sprint_hex(valstruct_t *buf, unsigned char c) {
//  if (buf) {
//    int r;
//    if ((r = _val_str_cat_ch(buf,c))) return r;
//  }
//  return 1;
//}
int val_sprint_ptr(valstruct_t *buf, void *ptr) {
  int ptrsize = sizeof(ptr);
  int strsize = 2 * (ptrsize+1);
  if (buf) {
    char *p;
    int r;
    if ((r = _val_str_rextend(buf, strsize,&p))) return r;
    p[0] = '0'; p[1] = 'x';
    for(p += strsize; ; ptrsize--) {
      unsigned char b = (unsigned char)(uintptr_t)ptr;
      --p;
      if (b>>4<10) *p = '0' + (b>>4);
      else *p = 'a' + (b>>4);

      --p;
      if (b%16<10) *p = '0' + (b%16);
      else *p = 'a' + (b%16);
      ptr = (void*)((uintptr_t)ptr >> 8);
    }
  } else {
    return strsize;
  }
}



int val_printf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  int r = val_vfprintf(stdout, format, args);
  va_end(args);
  return r;
}

int val_fprintf(FILE *file, const char *format, ...) {
  va_list args;
  va_start(args, format);
  int r = val_vfprintf(file,format,args);
  va_end(args);
  return r;
}

int val_sprintf(valstruct_t *buf, const char *format, ...) {
  va_list args;
  va_start(args, format);
  int r = val_vsprintf(buf,format,args);
  va_end(args);
  return r;
}

int val_vfprintf(FILE *file, const char *format, va_list args) {
  int r;
  int len = strlen(format);
  int rlen=0;
  int prevn = rlen;

  fmt_t fmt;
  const char *str;
  int vali,precision_arg,field_width_arg;

  while(len && 0<(r = _val_printf_parse(&format,&len,&str,&fmt,&vali,&precision_arg,&field_width_arg))) {
    if (str) {
      if (0>(r = val_fprint_(file,str,r))) goto bad;
    } else {
      throw_if_goto(ERR_BADESCAPE,bad,vali>=0 || precision_arg>0 || field_width_arg>0); //doesn't support indexed args currently
      val_t ignore;
      if (field_width_arg == 0) fmt.field_width = va_arg(args,int);
      if (precision_arg == 0) fmt.precision = va_arg(args,int);
      if (fmt.conversion == '_') {
        ignore = va_arg(args,val_t);
      } else if (fmt.conversion == 'n') { //get num bytes printed so far (or since last 'n')
        throw_if_goto(ERR_BADESCAPE,bad,vali>=0); //doesn't support indexed args currently
        int n;
        if (fmt.flags & PRINTF_F_MINUS) n = rlen-prevn;
        else n = rlen;
        *va_arg(args,int*) = n;
        prevn = rlen;
      } else if (fmt.conversion == '%') {
        if (0>(r = val_fprint_(file,"%",1))) goto bad;
        rlen += r;
      } else {
        val_t val = va_arg(args,val_t);

        if (fmt.field_width > 0 && !(fmt.flags & PRINTF_F_MINUS)) { //pad left
          if (0>(r = _val_fprintf_padding(file,&fmt,val_sprintf_(val,NULL,&fmt),0))) return r; //first sprintf to NULL to get length for padding
          rlen += r;
        }

        if (0>(r = val_fprintf_(val,file,&fmt))) {
          if (0>(r=val_fprint_(file,fmt.spec-1,fmt.speclen+1))) return r; //print failed conversion spec
          fprintf(stderr,"\nprintf conversion error for \"%.*s\"\n",fmt.speclen+1,fmt.spec-1);
          return r;
        } else {
          rlen += r;

          if (0>(r = _val_fprintf_padding(file,&fmt,r,1))) return r;
          rlen += r;

          //throw_if_goto(ERR_BADESCAPE,bad,fmt.flags & PRINTF_F_POP); //or ignore???
        }
      }
    }

  }
bad:
  return r;
}

int val_vsprintf(valstruct_t *buf, const char *format, va_list args) {
  int r;
  int len = strlen(format);
  int rlen=0;
  int prevn = rlen;

  fmt_t fmt;
  const char *str;
  int vali,precision_arg,field_width_arg;

  while(len && 0<(r = _val_printf_parse(&format,&len,&str,&fmt,&vali,&precision_arg,&field_width_arg))) {
    if (str) {
      if(0>(r = val_sprint_(buf,str,r))) goto bad;
    } else {
      throw_if_goto(ERR_BADESCAPE,bad,vali>=0 || precision_arg>0 || field_width_arg>0); //doesn't support indexed args currently
      val_t ignore;
      if (field_width_arg == 0) fmt.field_width = va_arg(args,int);
      if (precision_arg == 0) fmt.precision = va_arg(args,int);
      if (fmt.conversion == '_') {
        ignore = va_arg(args,val_t);
      } else if (fmt.conversion == 'n') { //get num bytes printed so far (or since last 'n')
        throw_if_goto(ERR_BADESCAPE,bad,vali>=0); //doesn't support indexed args currently
        int n;
        if (fmt.flags & PRINTF_F_MINUS) n = rlen-prevn;
        else n = rlen;
        *va_arg(args,int*) = n;
        prevn = rlen;
      } else if (fmt.conversion == '%') {
        if(0>(r = val_sprint_(buf,"%",1))) goto bad;
        rlen += r;
      } else {
        val_t val = va_arg(args,val_t);

        if (fmt.field_width > 0 && !(fmt.flags & PRINTF_F_MINUS)) { //pad left
          if (0>(r = _val_sprintf_padding(buf,&fmt,val_sprintf_(val,NULL,&fmt),0))) goto bad; //first sprintf to NULL to get length for padding
          rlen += r;
        }

        if (0>(r = val_sprintf_(val,buf,&fmt))) {
          if (0>(r=val_sprint_(buf,fmt.spec-1,fmt.speclen+1))) goto bad; //print failed conversion spec
          fprintf(stdout,"\nprintf conversion error for \"%.*s\"\n",fmt.speclen+1,fmt.spec-1);
          return r;
        } else {
          rlen += r;

          if (0>(r = _val_sprintf_padding(buf,&fmt,r,1))) goto bad;
          rlen += r;

          throw_if_goto(ERR_BADESCAPE,bad,fmt.flags & PRINTF_F_POP); //or ignore?
        }
      }
    }
  }
bad:
  return r;
}

int val_printfv(valstruct_t *format, valstruct_t *args, int isstack) {
  return val_fprintfv(stdout,format,args,isstack,args,isstack);
}
int val_printfv_(const char *format, int len, valstruct_t *args, int isstack) {
  return val_fprintfv_(stdout,format,len,args,isstack,args,isstack);
}

int val_fprintfv(FILE *file, valstruct_t *format, valstruct_t *v_args, int v_isstack, valstruct_t *f_args, int f_isstack) {
  //throw_if(ERR_BADARGS,!(format->type == TYPE_STRING) || !val_is_lst(v_args) || !val_is_lst(f_args)); //TODO: argcheck

  return val_fprintfv_(file,_val_str_begin(format),_val_str_len(format),v_args,v_isstack,f_args,f_isstack);
}
int val_sprintfv(valstruct_t *buf, valstruct_t *format, valstruct_t *v_args, int v_isstack, valstruct_t *f_args, int f_isstack) {
  //throw_if(ERR_BADARGS,!val_isstring(format) || !val_islist(v_args) || !val_islist(f_args));
  return val_sprintfv_(buf,_val_str_begin(format),_val_str_len(format),v_args,v_isstack,f_args,f_isstack);
}

int val_fprintfv_(FILE *file, const char *format, int len, valstruct_t *v_args, int v_isstack, valstruct_t *f_args, int f_isstack) {
  int r;
  int rlen=0;
  int prevn = rlen;

  fmt_t fmt;
  const char *str;
  int vali,precision_arg,field_width_arg;

  while(len && 0<(r = _val_printf_parse(&format,&len,&str,&fmt,&vali,&precision_arg,&field_width_arg))) {
    if (str) {
      if(0>(r = val_fprint_(file,str,r))) return r;
    } else {
      if ((r = _val_printf_fmt_takeargs(&fmt,precision_arg,field_width_arg,f_args,f_isstack))) return r;

      if ((r = _val_printf_special(&fmt,vali,f_args,f_isstack,rlen,&prevn))) {
        if (r<0) return r;
        else r=0;
      } else if (fmt.conversion == '%') {
        if (0>(r = val_fprint_(file,"%",1))) return r;
        rlen += r;
      } else {
        val_t *val;
        if ((r = _val_printf_takeval(v_args,v_isstack,vali,&val))) return r;

        if (fmt.field_width > 0 && !(fmt.flags & PRINTF_F_MINUS)) { //pad left
          if (0>(r = _val_fprintf_padding(file,&fmt,val_sprintf_(*val,NULL,&fmt),0))) return r; //first sprintf to NULL to get length for padding
          rlen += r;
        }

        if (0>(r = val_fprintf_(*val,file,&fmt))) {
          int tr;
          if (0>(tr=val_fprint_(file,fmt.spec-1,fmt.speclen+1))) return tr; //print failed conversion spec
          fprintf(stdout,"\nprintf conversion error for \"%.*s\"\n",fmt.speclen+1,fmt.spec-1);
          return r;
        } else {
          rlen += r;

          if (0>(r = _val_fprintf_padding(file,&fmt,r,1))) return r;
          rlen += r;

          if (fmt.flags & PRINTF_F_POP) { if ((r = val_arglist_drop(v_args,v_isstack))) return r; }
        }
      }
    }
  }
  return r;
}
int val_sprintfv_(valstruct_t *buf, const char *format, int len, valstruct_t *v_args, int v_isstack, valstruct_t *f_args, int f_isstack) {
  int r;
  int rlen=0;
  int prevn = rlen;

  fmt_t fmt;
  const char *str;
  int vali,precision_arg,field_width_arg;

  while(len && 0<(r = _val_printf_parse(&format,&len,&str,&fmt,&vali,&precision_arg,&field_width_arg))) {
    if (str) {
      if(0>(r = val_sprint_(buf,str,r))) return r;
      rlen += r;
    } else {
      if ((r = _val_printf_fmt_takeargs(&fmt,precision_arg,field_width_arg,f_args,f_isstack))) return r;

      if ((r = _val_printf_special(&fmt,vali,f_args,f_isstack,rlen,&prevn))) {
        if (r<0) return r;
        else r=0;
      } else if (fmt.conversion == '%') {
        if (0>(r = val_sprint_(buf,"%",1))) return r;
        rlen += r;
      } else {
        val_t *val;
        if ((r = _val_printf_takeval(v_args,v_isstack,vali,&val))) return r;

        if (fmt.field_width > 0 && !(fmt.flags & PRINTF_F_MINUS)) { //pad left
          if (0>(r = _val_sprintf_padding(buf,&fmt,val_sprintf_(*val,NULL,&fmt),0))) return r; //first sprintf to NULL to get length for padding
          rlen += r;
        }

        if (0>(r = val_sprintf_(*val,buf,&fmt))) {
          int tr;
          if (0>(tr=val_sprint_(buf,fmt.spec-1,fmt.speclen+1))) return tr; //print failed conversion spec
          fprintf(stdout,"\nprintf conversion error for \"%.*s\"\n",fmt.speclen+1,fmt.spec-1);
          return r;
        } else {
          rlen += r;

          if (0>(r = _val_sprintf_padding(buf,&fmt,r,1))) return r;
          rlen += r;

          if (fmt.flags & PRINTF_F_POP) { if ((r = val_arglist_drop(v_args,v_isstack))) return r; }
        }
      }
    }
  }
  return r;
}

//int val_sprintf_truncated_(val_t *val, val_t *buf, const fmt_t *fmt, const fmt_t *trunc_fmt) {
//  int max_chars = trunc_fmt;
//  if (trunc_fmt->precision==0) return 0;
//  else if (max_chars>0 && max_chars<3) return _fatal(ERR_BADARGS);
//
//  //fmt_t tfmt = printf_fmt_V;
//  //tfmt.field_width = min_field_width;
//
//  //if (val_isstring(val)) tfmt.precision = max_chars;
//  switch(val->type) {
//    case TYPE_NULL:
//    case TYPE_INT: //integer number (stored as valint_t)
//    case TYPE_FLOAT: //floating point number (stored as double)
//    case TYPE_HASH:
//    case TYPE_VM:
//    case TYPE_NATIVE_FUNC: //built-in
//    case TYPE_FILE:
//    case TYPE_REF:
//    case TYPE_BYTECODE:
//    case TYPE_STRING:
//    case TYPE_IDENT:
//    case TYPE_LIST:
//    case TYPE_CODE:
//    default:
//      return -1;
//  }
//  return 0;
//}

//int val_print_truncated(val_t *val, int min_field_width, int max_chars, int left_align) {
//  int r;
//  if (0>(r= val_fprintf_truncated_(val,stdout,fmt_V,min_field_width,max_chars,left_align))) return r;
//  return 0;
//}
//
//
//int val_fprintf_truncated_(val_t *val, FILE *file, const fmt_t *fmt, int min_field_width, int max_chars, int left_align) {
//  val_t buf;
//  int r;
//  val_string_init(&buf);
//  if (0>(r = val_sprintf_truncated_(val,&buf,fmt,min_field_width,max_chars,left_align))) return r;
//
//  if (0>(r = val_fprintf_(&buf,file,fmt_v))) {
//    val_destroy(&buf);
//    return r;
//  } else {
//    val_destroy(&buf);
//    return r;
//  }
//}
//int val_sprintf_truncated_(val_t *val, val_t *buf, const fmt_t *fmt, int min_field_width, int max_chars, int left_align) {
//  //TODO: refactor -- at least some of the args are probably optional (pull from fmt, which we currently ignore)
//  if (max_chars==0) return 0;
//  else if (max_chars>0 && max_chars<3) return _fatal(ERR_BADARGS);
//
//  fmt_t tfmt = printf_fmt_V;
//  tfmt.field_width = min_field_width;
//
//  if (val_isstring(val)) tfmt.precision = max_chars;
//
//  int r,ret;
//  val_t tbuf;
//  val_string_init(&tbuf);
//  if (0>(r = val_sprintf_(val,&tbuf,&tfmt))) goto val_sprintf_truncated_bad;
//  ret = r;
//
//  if (max_chars > 0 && val_string_len(&tbuf) > (unsigned int)max_chars) { //need to truncate
//    ret = max_chars;
//    if (max_chars==3) { //if max_chars==3 (min valid arg), just sprint ...
//      val_string_clear(&tbuf);
//      r = val_sprint_cstr(&tbuf,"..."); 
//    } else {
//      //TODO: special case for VM?
//      if (val_iscoll(val) || val_isbytecode(val)) {
//        if ((r = val_string_substring(&tbuf,0,max_chars-4))) goto val_sprintf_truncated_bad; //remove 4 extra chars for ... and close
//        if (max_chars == 4) { 
//          switch(val->type) {
//            case TYPE_STRING: r = val_sprint_cstr(&tbuf,"\"..\""); break;
//            case TYPE_LIST:r = val_sprint_cstr(&tbuf,"(..)"); break;
//            case TYPE_CODE: r = val_sprint_cstr(&tbuf,"[..]"); break;
//            //case TYPE_BYTECODE: r = val_sprint_cstr(&tbuf,"(..)"); break;
//            default: return _fatal(ERR_FATAL); //shouldn't get here
//          }
//        } else {
//          switch(val->type) {
//            case TYPE_STRING: r = val_sprint_cstr(&tbuf,"\"..."); break;
//            case TYPE_LIST:r = val_sprint_cstr(&tbuf,"...)"); break;
//            case TYPE_CODE: r = val_sprint_cstr(&tbuf,"...]"); break;
//            //case TYPE_BYTECODE: r = val_sprint_cstr(&tbuf,"...)"); break;
//            default: return _fatal(ERR_FATAL); //shouldn't get here
//          }
//        }
//      } else {
//        val_string_substring(&tbuf,0,max_chars-3);
//        r = val_sprint_cstr(&tbuf,"...");
//      }
//    }
//  }
//  if (r<0) goto val_sprintf_truncated_bad;
//
//  if (min_field_width > 0 && val_string_len(&tbuf) < (unsigned int)min_field_width) { //need padding
//    if (left_align) {
//      r = val_sprintf_pad(&tbuf,&tfmt,val_string_len(&tbuf));
//    } else {
//      r = val_sprintf_padleft(&tbuf,&tfmt,val_string_len(&tbuf));
//    }
//    if (r<0) goto val_sprintf_truncated_bad;
//    ret = r;
//  }
//
//  if ((r = val_string_cat(buf,&tbuf))) goto val_sprintf_truncated_bad;
//  return ret;
//
//val_sprintf_truncated_bad:
//  val_destroy(&tbuf);
//  return r;
//}
//





int _val_fprintf_padding(FILE *file, const fmt_t *fmt, int len, int right) {
  throw_if(ERR_BADARGS,len < 0);
  if (len >= fmt->field_width) return 0;
  if (!right) { //left side
    if ((fmt->flags & PRINTF_F_MINUS)) return 0; //padding is right
  } else { //right side
    if (!(fmt->flags & PRINTF_F_MINUS)) return 0; //padding is left
  }

  char padc = ' ';
  if (fmt->flags & PRINTF_F_ZERO && !(fmt->flags & PRINTF_F_MINUS)) {
    //per printf(3) on my system
    // - 0 means value should be zero padded.
    // - for d i o u x X a A e E f F g G, left padded with zeros instead of blanks
    // - if both 0 and - flags, ignore 0
    // - if precision given with numeric conversion (d i o u x X), ignore 0
    // - for other conversions, undefined (we pad with zeroes here)
    switch(fmt->conversion) {
      case 'd': case 'i': case 'o': case 'u': case 'x': case 'X': if (fmt->precision>=0) break;
      default: padc = '0'; break;
    }
  }
  int r;
  int rlen=0;
  while(len<fmt->field_width) {
    throw_if(ERR_IO_ERROR,0>(r=fprintf(file,"%c",padc)));
    rlen += r;
    len++;
  }
  return rlen;
}
int _val_sprintf_padding(valstruct_t *buf, const fmt_t *fmt, int len, int right) {
  throw_if(ERR_BADARGS,len < 0);
  if (len >= fmt->field_width) return 0;
  if (!right) { //left side
    if ((fmt->flags & PRINTF_F_MINUS)) return 0; //padding is right
  } else { //right side
    if (!(fmt->flags & PRINTF_F_MINUS)) return 0; //padding is left
  }

  if (buf) {
    char padc = ' ';
    if (fmt->flags & PRINTF_F_ZERO && !(fmt->flags & PRINTF_F_MINUS)) {
      //zero left-padding (unless numeric and precision set) (see fprintf_padding)
      switch(fmt->conversion) {
        case 'd': case 'i': case 'o': case 'u': case 'x': case 'X': if (fmt->precision>=0) break;
        default: padc = '0'; break;
      }
    }
    int r;
    if ((r = _val_str_padright(buf,padc,fmt->field_width-len))) return r;
  }
  return fmt->field_width-len;
}



//parse an extent of a printf spec
//  - returns <0 for err, 0 for end-of-string, >0 is the length of the extent just parsed
//  - if *str != NULL, extent is a string literal (length in return value)
//  - if *str == NULL
//    - fmt contains the next print spec
//    - vali is which value to print (or -1 for pop first)
//    - precision_arg is arg index for precision (or -1)
//    - field_width_arg is arg index for precision (or -1)
//
//ret = num chars consumed. 0 means end of string, <0 means err
//  - if *str != NULL, then tok is string literal
//  - else fmt contains a format token
//  - updates *format, len for next call
int _val_printf_parse(
    const char **format, int *len,
    const char **str, fmt_t *fmt,
    int *vali, int *precision_arg, int *field_width_arg) {
  const char *f = *format;
  int i=0;
  int n=*len;
  while(i < n && f[i] != '%') i++;
  if (i+1 < n && f[i+1] == '%') i++; //literal % -- include at tail of current literal

  if (!n || i>0) { //string literal (or empty string)
    *str = f;
    *len -= i;
    *format += i;
    if (i && f[i-1] == '%') { //% escape
      (*format)++;
      (*len)--;
    }
    return i;
  } else { //printf format to return
    *str = NULL;
    int t;
    throw_if(ERR_BADARGS,++i >= n); //bad % at end of string
    int rlen=i;
    f += i; n -= i;
    fmt_clear(fmt);
    *vali=-1;
    *precision_arg=-1;
    *field_width_arg=-1;
    fmt->spec=f;

    //shortcut for % escape -- we don't use this currently because we catch %% above
    //if (f[i] == '%') { fmt->speclen=1; fmt->conversion='%'; return 1; }

    fmt->flags = PRINTF_F_POP;

    //conversion format
    //  - flags (also %m$ notation)
    //  - min field width (also * and *m$ notation)
    //  - precision (also * and *m$ notation)
    //  - length modifier
    //  - conversion type
    //

    int skip_flags=0; //skip flags because we jumped straight to min field width
    if (isdigit(*f) && *f != '0') { //could be either min field width or %m$ notation, need to scan for what follows end of digits
      i=1; while(i<n && isdigit(f[i])) i++;
      throw_if(ERR_BADESCAPE,i==n); //there should be more

      if (f[i] == '$') { //%m$ format
        t = parseint_(f,i);
        *vali=t;
        fmt->flags &= ~PRINTF_F_POP; //no pop arg (since we are using %m$ arg specifier)
        f+=i+1;rlen+=i+1;n-=i+1;
      } else { //else is min field width, skip to there and don't update spec
        skip_flags=1;
      }
    }

    throw_if(ERR_BADESCAPE,!n);

    if (!skip_flags) {
      int flag=1;
      do {
        switch(*f) {
          case '#':  f++;rlen++;n--; fmt->flags |= PRINTF_F_ALT; break;
          case '0':  f++;rlen++;n--; fmt->flags |= PRINTF_F_ZERO; break;
          case '-':  f++;rlen++;n--; fmt->flags |= PRINTF_F_MINUS; break;
          case ' ':  f++;rlen++;n--; fmt->flags |= PRINTF_F_SPACE; break;
          case '+':  f++;rlen++;n--; fmt->flags |= PRINTF_F_PLUS; break;
          case '\'': f++;rlen++;n--; fmt->flags |= PRINTF_F_SQUOTE; break;
          case 'b':  f++;rlen++;n--; fmt->flags |= PRINTF_F_BINARY; break;
          default:  flag=0; break;
        }
      } while (n&&flag);
    }

    throw_if(ERR_BADESCAPE,!n);

    if (*f == '*') { //grab arg for min field width - either * or *m$
      f++;rlen++;n--;
      throw_if(ERR_BADESCAPE,!n); //there should be more
      if (isdigit(*f)) { //*m$ format
        i=1; while(i<n && isdigit(f[i])) i++;
        throw_if(ERR_BADESCAPE,i==n || f[i]!='$'); //improper *m$ format

        t = parseint_(f,i);
        throw_if(ERR_BADARGS,t<1);
        *field_width_arg = t;

        f+=i+1;rlen+=i+1;n-=i+1;
      } else { //* format, pop top of stack to get min field width. must be int val
        *field_width_arg = 0;
      }
    } else if (isdigit(*f)) { //min field width
      i=1; while(i<n && isdigit(f[i])) i++;
      throw_if(ERR_BADESCAPE,i==n); //there should be more
      t = parseint_(f,i);
      fmt->field_width=t;
      f+=i;rlen+=i;n-=i;
    }

    throw_if(ERR_BADESCAPE,!n);
    
    if (*f == '.') { //precision - either .n or .* or .*m$ (or . with missing precision value)
      f++;rlen++;n--;
      throw_if(ERR_BADESCAPE,!n); //there should be more
      if (isdigit(*f)) { //.n format
        i=1; while(i<n && isdigit(f[i])) i++;
        throw_if(ERR_BADESCAPE,i==n); //there should be more
        t = parseint_(f,i);
        fmt->precision=t;
        f+=i;rlen+=i;n-=i;
      } else if (*f=='*') { //.* or .*m$
        f++;rlen++;n--;
        throw_if(ERR_BADESCAPE,!n); //there should be more
        if (isdigit(*f)) { //.*m$ format
          i=1; while(i<n && isdigit(f[i])) i++;
          throw_if(ERR_BADESCAPE,i==n || f[i]!='$'); //invalid .*m$ format

          throw_if(ERR_BADARGS,0>= (t = parseint_(f,i)));
          *precision_arg = t;
          f+=i+1;rlen+=i+1;n-=i+1;
        } else { //.* format
          *precision_arg = 0;
        }
      } else {
        //do nothing -- missing precision
      }
    }

    throw_if(ERR_BADESCAPE,!n);

    switch(*f) {
      case 'h':
        throw_if(ERR_BADESCAPE,n<2);
        if (f[1] == 'h') {
          fmt->length_mod = PRINTF_hh;
          f+=2;rlen+=2;n-=2;
        } else {
          fmt->length_mod = PRINTF_h;
          f++;rlen++;n--;
        }
        break;
      case 'l':
        throw_if(ERR_BADESCAPE,n<2);
        if (f[1] == 'l') {
          fmt->length_mod = PRINTF_ll;
          f+=2;rlen+=2;n-=2;
        } else {
          fmt->length_mod = PRINTF_l;
          f++;rlen++;n--;
        }
        break;
      case 'L': f++;rlen++;n--; fmt->length_mod = PRINTF_L; break;
      case 'j': f++;rlen++;n--; fmt->length_mod = PRINTF_j; break;
      case 'z': f++;rlen++;n--; fmt->length_mod = PRINTF_z; break;
      case 't': f++;rlen++;n--; fmt->length_mod = PRINTF_t; break;
      default: break;
    }
    throw_if(ERR_BADESCAPE,!n);

    //whats left should be conversion type
    fmt->conversion = *f;
    rlen++; //for conversion type
    fmt->speclen=f-fmt->spec+1;
    //if ((unsigned int)*vali >= val_list_len(args)) return -1;
    *format += rlen;
    *len -= rlen;

    return rlen;
  }
}

err_t _val_printf_fmt_takeargs(fmt_t *fmt, int precision_arg, int field_width_arg, valstruct_t *args, int isstack) {
  if (field_width_arg>0) {
    throw_if(ERR_BADARGS,(unsigned int)field_width_arg > _val_lst_len(args));
    val_t *v = val_arglist_get(args,isstack,field_width_arg-1);
    throw_if(ERR_BADARGS,!val_is_int(*v));
    fmt->field_width = __val_int(*v);
  } else if (field_width_arg==0) {
    throw_lst_empty(args);
    val_t *v = val_arglist_get(args,isstack,0);
    throw_if(ERR_BADARGS,!val_is_int(*v));
    fmt->field_width = __val_int(*v);
    val_arglist_drop(args,isstack);
  }

  if (precision_arg>0) {
    throw_if(ERR_BADARGS,(unsigned int)precision_arg > _val_lst_len(args));
    val_t *v = val_arglist_get(args,isstack,precision_arg-1);
    throw_if(ERR_BADARGS,!val_is_int(*v));
    fmt->precision = __val_int(*v);
  } else if (precision_arg==0) { //* format, pop top of stack to get min field width. must be int val
    throw_lst_empty(args);
    val_t *v = val_arglist_get(args,isstack,0);
    throw_if(ERR_BADARGS,!val_is_int(*v));
    fmt->precision = __val_int(*v);
    val_arglist_drop(args,isstack);
  }

  return 0;
}

err_t _val_printf_takeval(valstruct_t *args, int isstack, int vali, val_t **val) {
  if (vali == -1) vali = 0;
  else if (vali > 0) vali--;
  throw_if(ERR_BADARGS,(unsigned int)vali >= _val_lst_len(args));
  *val = val_arglist_get(args,isstack,vali);
  return 0;
}

int _val_printf_special(fmt_t *fmt, int vali, valstruct_t *args, int isstack, int rlen, int *prevn) {
  int r;
  if (fmt->conversion == '_') {
    if ((r = val_arglist_drop(args,isstack))) return r;
  } else if (fmt->conversion == 'n') {
    throw_if(ERR_BADARGS,vali > 0 && ((unsigned int)vali > _val_lst_len(args)));

    //we use stack push instead of isstack so field widths can be pushed on right and args popped from left
    if (fmt->flags & PRINTF_F_MINUS) r = _val_lst_rpush(args,__int_val(rlen-*prevn));
    else r = _val_lst_rpush(args,__int_val(rlen));
    if (r) return r;

    if (vali > 0) {
      //if ((r = val_stack_buryn(args,vali))) return r;
      return _throw(ERR_NOT_IMPLEMENTED); //FIXME: IMPLEMENTME
    }
    *prevn = rlen;
    return 1;
  }
  return 0;
}


