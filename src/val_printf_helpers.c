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

#include "val_printf_helpers.h"
#include "val_printf.h"
#include "val_string.h"
#include "val_list.h"
#include "val_file.h"
#include "val_fd.h"
#include "val_dict.h"
#include "val_ref.h"
#include "val_op.h"
#include "val_num.h"
#include "val_vm.h"

#include "vm_err.h"
#include <string.h>


void val_printf_clear(struct printf_fmt *fmt) {
  fmt->spec=NULL;
  fmt->speclen=0;
  fmt->field_width = -1;
  fmt->precision = -1;
  fmt->flags = 0;
  fmt->length_mod = PRINTF_none;
  fmt->conversion = '\0';
}

int val_fprintf_(val_t val, FILE *file, const struct printf_fmt *fmt) {
  //TODO: type_handlers array like old impl??? -- probably faster, more compact/robust for the handle-all-types functions (but requires typecode)
  //  - see val.h for typecode discussion
  int r;
  switch(__val_tag(val)) {
    case _OP_TAG:
      r = val_op_fprintf(__val_op(val),file,fmt);
      break;
    case _STR_TAG:
      switch(__str_ptr(val)->type) {
        case TYPE_IDENT:
          r = val_ident_fprintf(__ident_ptr(val),file,fmt);
          break;
        case TYPE_STRING:
          r = val_string_fprintf(__string_ptr(val),file,fmt);
          break;
        //case TYPE_BYTECODE:
        default:
          return _throw(ERR_NOT_IMPLEMENTED);
      }
      break;
    case _LST_TAG:
      //return val_list_fprintf_simple(__lst_ptr(val),file,fmt);
      r = val_list_fprintf(__lst_ptr(val),file,fmt);
      break;
    case _VAL_TAG:
      switch(__val_ptr(val)->type) {
        case TYPE_DICT:
          r = val_dict_fprintf(__dict_ptr(val),file,fmt);
          break;
        case TYPE_REF:
          r = val_ref_fprintf(__ref_ptr(val),file,fmt);
          break;
        case TYPE_FILE:
          r = val_file_fprintf(__file_ptr(val),file,fmt);
          break;
        case TYPE_FD:
          r = val_fd_fprintf(__fd_ptr(val),file,fmt);
          break;
        case TYPE_VM:
          r = val_vm_fprintf(__vm_ptr(val),file,fmt);
          break;
        default:
          return _throw(ERR_NOT_IMPLEMENTED);
      }
      break;
    case _TAG5:
      return _fatal(ERR_BADTYPE);
    case _INT_TAG:
      r = val_int32_fprintf(__val_int(val),file,fmt);
      break;
    default: //double
      r = val_double_fprintf(__val_dbl(val),file,fmt);
      break;
  }

  if (r < 0) return r;

#ifdef DEBUG_VAL
  val64_t dbg = __val_dbg_val(val);
  if (!val_is_null(dbg)) {
    err_t e;
    if (0>(e = val_fprint_cstr(file,"+debug("))) return e;
    else r += e;
    if (0>(e = val_fprintf_(dbg,file,fmt_V))) return e;
    else r += e;
    if (0>(e = val_fprint_ch(file,')'))) return e;
    else r += e;
  }
#endif
  return r;
}

int val_sprintf_(val_t val, valstruct_t *buf, const struct printf_fmt *fmt) {
  int r;
  switch(__val_tag(val)) {
    case _OP_TAG:
      r = val_op_sprintf(__val_op(val),buf,fmt);
      break;
    case _STR_TAG:
      switch(__str_ptr(val)->type) {
        case TYPE_IDENT:
          r = val_ident_sprintf(__ident_ptr(val),buf,fmt);
          break;
        case TYPE_STRING:
          r = val_string_sprintf(__string_ptr(val),buf,fmt);
          break;
        //case TYPE_BYTECODE:
        default:
          return _throw(ERR_NOT_IMPLEMENTED);
      }
      break;
    case _LST_TAG:
      //r = val_list_sprintf_simple(__lst_ptr(val),buf,fmt);
      r = val_list_sprintf(__lst_ptr(val),buf,fmt);
      break;
    case _VAL_TAG:
      switch(__val_ptr(val)->type) {
        case TYPE_DICT:
          r = val_dict_sprintf(__dict_ptr(val),buf,fmt);
          break;
        case TYPE_REF:
          r = val_ref_sprintf(__ref_ptr(val),buf,fmt);
          break;
        case TYPE_FILE:
          r = val_file_sprintf(__file_ptr(val),buf,fmt);
          break;
        case TYPE_FD:
          r = val_fd_sprintf(__fd_ptr(val),buf,fmt);
          break;
        case TYPE_VM:
          r = val_vm_sprintf(__vm_ptr(val),buf,fmt);
          break;
        default:
          return _throw(ERR_NOT_IMPLEMENTED);
      }
      break;
    case _TAG5:
      r = _fatal(ERR_BADTYPE);
      break;
    case _INT_TAG:
      r = val_int32_sprintf(__val_int(val),buf,fmt);
      break;
    default: //double
      r = val_double_sprintf(__val_dbl(val),buf,fmt);
      break;
  }

#ifdef DEBUG_VAL
  if (r < 0) goto skip_debug;
  val64_t dbg = __val_dbg_val(val);
  if (!val_is_null(dbg)) {
    err_t e;
    if (0>(e = val_sprint_cstr(buf,"+debug("))) return e;
    else r += e;
    if (0>(e = val_sprintf_(dbg,buf,fmt_V))) return e;
    else r += e;
    if (0>(e = val_sprint_ch(buf,')'))) return e;
    else r += e;
  }
skip_debug:
#endif

#ifdef SPRINTF_CHECK_RLEN
  if (buf) {
    int rb = val_sprintf_(val,NULL,fmt);
    if (rb != r) return _throw(ERR_ASSERT);
  }
#endif
  return r;
}



val_t* val_arglist_get(valstruct_t *list, int isstack, int i) {
  if ((unsigned int)i >= _val_lst_len(list)) return NULL;

  if (isstack) return &_val_lst_end(list)[-1-i];
  else return &_val_lst_begin(list)[i];
}
//int val_arglist_pop(valstruct_t *list, int isstack, val_t *el) {
//  if (isstack) return _val_lst_rpop(list,el);
//  else return _val_lst_lpop(list,el);
//}

int val_arglist_drop(valstruct_t *list, int isstack) {
  if (isstack) return _val_lst_rdrop(list);
  else return _val_lst_ldrop(list);
}

