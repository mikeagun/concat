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

#ifndef __VM_PARSER_H__
#define __VM_PARSER_H__ 1
#include "parser.h"
#include "val.h"

// vm_parser.h - concat tokenizer and val_t parser
// - uses parser.h/val.h to build parsing fsm and parse strings to concat vals


// Concat Parser Finite State Machine
//
// FIXME: vm_parser refactor - need to update vm_parser.h/c for recent parser.h/c changes (see FIXMEs in those 4 files)
//
// The concat parser uses the parser.h fsm parser interface.
// parse_class and parse_state represent the character classes and states of the fsm.
//
// High level parsing algorithm:
// 1. Each input char is mapped to a pclass
//    - reduces character to pclass to reduce the number of state transitions to handle
// 2. (op,next_state) is looked up by fsm[state][pclass]
//    - rules current described and defined in vm_parser.c in vm_new_parser()
// 3. fsm op controls tokenizing to split off concat tokens (string/number/op/ident/comment)
//    - each token split off is a complete concat token (number/ident could still have parse error)
//      - token is ultimately parsed into val_t in vm_parse_tok()
//      - the old tokenizer fully validated, but duplicated work with parse_tok
//        - tokenizing is decoupled from parsing to keep things cleaner
//        - to make parsing truly single-pass and reduce code size we could parse during tokenizing, but that will get messier
//        - TODO: consider single-pass concat parser (use spare parser.h opcodes to control sub-token/token parsing)
//    - whitespace between tokens is dropped/ignored
// 4. calls handler on each complete concate token to get a val_t
//    - first char determines d/s-quoted string, operator, escaped ident, or comment
//    - else if it parses as a number it is a number
//    - else if it is a valid parsed ident it is an ident
//    - else it throws an error
//    - vm_parse_input() just returns a list of tokens parsed to val_t (i.e. grouping ops just idents)
//    - vm_parse_code() handles grouping ops to build nested lists/quotations
//
// The rules are set using the parser_set_* functions
//

// parse_class - concat parsing character classes
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

// parse_state - concat parser FSM states
enum parse_state {
  PSTATE_INIT=0, //initial parser state (also for whitespace handling)

  PSTATE_DSTRING_ESCAPE, //escape sequence within double quotes
  PSTATE_DSTRING,        //double quoted string
  PSTATE_SSTRING,        //single quoted string
  PSTATE_IDENT,          //identifier
  PSTATE_DIGIT,          //like IDENT, but treats +/- as op instead of sign
  PSTATE_IDENT_ESCAPE,   //leading backslashes for escaped identifier
  PSTATE_OP,             //just parsed operator
  PSTATE_CLOSE_GROUP,    //just closed group

  PSTATE_COMMENT,        //in comment (until end of line)

  PSTATE_SIGN,           //+/- just seen (someplace that a number could start)

  PSTATE_COUNT,  //count of parse states
  PSTATE_FIN,    //termination state
};


struct vm_parse_code_state {
  valstruct_t *root_list;
  valstruct_t *open_list;
  unsigned int groupi;
};


int vm_parse_tok(const char *tok, int len, enum parse_state state, enum parse_state target, val_t *v);
int _vm_parse_input(struct parser_rules *p, const char *str, int len, valstruct_t *code);
int _vm_parse_code(struct parser_rules *p, const char *str, int len, valstruct_t *code);
int _vm_parse_code_(struct parser_rules *p, const char *str, int len, struct vm_parse_code_state *pstate);

struct parser_rules* vm_get_parser();
struct parser_rules* vm_new_parser();

const char* vm_get_classname(int pclass);
const char* vm_get_statename(int pstate);

#endif
