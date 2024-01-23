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

#include "val_op.h"
#include "val_string.h"
#include "val_printf.h"
#include "val_num.h"
#include "opcodes.h"

#include <string.h>

int val_op_fprintf(int op,FILE *file, const struct printf_fmt *fmt) {
  switch(fmt->conversion) {
    case 'v':
      if (op>=0 && op < N_OPS) {
        const char * opstr = opstrings[op];
        return fprintf(file,"%s",opstr);
      } else {
        return fprintf(file,"op(%i)",op);
      }
    case 'V':
      if (op>=0 && op < N_OPS) {
        const char * opstr = opstrings[op];
        return fprintf(file,"op(%s)",opstr);
      } else {
        return fprintf(file,"op(%i)",op);
      }
    default:
      return _throw(ERR_BADTYPE);
  }
}
int val_op_sprintf(int op,valstruct_t *buf, const struct printf_fmt *fmt) {
  switch(fmt->conversion) {
    case 'v':
      if (op>=0 && op < N_OPS) {
        const char * opstr = opstrings[op];
        int len = strlen(opstr);
        return val_sprint_(buf,opstr,len);
      } else {
        if (buf) {
          int r,rlen=0;
          if (0>(r=val_sprint_(buf,"op(",3))) return r;
          rlen += r;
          if (0>(r=val_int32_sprintf(op,buf,fmt_v))) return r;
          rlen += r;
          if (0>(r=val_sprint_ch(buf,')'))) return r;
          rlen += r;
          return rlen;
        } else {
          return 4 + val_int32_sprintf(op,NULL,fmt_v);
        }
      }
    case 'V':
      if (op>=0 && op < N_OPS) {
        const char * opstr = opstrings[op];
        int len = strlen(opstr);
        if (buf) {
          int r;
          if ((r = _val_str_rreserve(buf,4+len))) return r;
          //we don't need to error check since we reserved
          _val_str_cat_cstr(buf,"op(",3);
          _val_str_cat_cstr(buf,opstr,len);
          _val_str_cat_ch(buf,')');
        }
        return 4+len;
      } else {
        if (buf) {
          int r,rlen=0;
          if (0>(r=val_sprint_(buf,"op(",3))) return r;
          rlen += r;
          if (0>(r=val_int32_sprintf(op,buf,fmt_v))) return r;
          rlen += r;
          if (0>(r=val_sprint_ch(buf,')'))) return r;
          rlen += r;
          return rlen;
        } else {
          return 4 + val_int32_sprintf(op,NULL,fmt_v);
        }
      }
    default:
      return _throw(ERR_BADTYPE);
  }
}


