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

#ifndef __VM_PARSER_H__
#define __VM_PARSER_H__ 1

#include "val.h"
#include "parser.h"

enum parse_class {
  PCLASS_INIT=0,

  PCLASS_NULL,   //null string terminator
  PCLASS_BSLASH, //backslash
  PCLASS_HASH,   //hash/pound symbol
  PCLASS_NEWLINE,//newline
  PCLASS_SQUOTE, //single quote for string
  PCLASS_DQUOTE, //double quote for string
  PCLASS_SPACE,  //whitespace
  PCLASS_DIGIT,  //digit [0-9]
  PCLASS_SIGN,   //sign char [+-]
  PCLASS_IDENT,  //letters, underscore and dot
  PCLASS_OP,     //operator chars (except those that end expressions) [[(~!@#$%^&*=/;]
  PCLASS_CLOSE_GROUP,//operators that end groups (e.g. ')') -- triggers different sign handling
  PCLASS_OTHER,  //any characters we don't need to handle specially

  PCLASS_COUNT   //count of parse char classes
};

enum parse_state {
  PSTATE_INIT=0, //initial parser state (also for whitespace handling)

  PSTATE_DSTRING_ESCAPE, //escape sequence
  PSTATE_DSTRING, //double quoted string
  PSTATE_SSTRING, //single quoted string
  PSTATE_IDENT,
  PSTATE_DIGIT, //like IDENT, but treats +/- as op instead of sign
  PSTATE_IDENT_ESCAPE, //leading backslashes for escaped identifier
  PSTATE_OP,
  PSTATE_CLOSE_GROUP,

  PSTATE_COMMENT,

  PSTATE_SIGN,

  PSTATE_COUNT,  //count of parse states
  PSTATE_FIN,    //termination state
};



int vm_eval_tok(const char *tok, int len, enum parse_state state, enum parse_state target, val_t *v);
struct parser_rules* get_vm_parser();

#endif
