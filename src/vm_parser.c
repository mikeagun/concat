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

#include "vm_parser.h"
#include "vm_parser_internal.h"
#include "vm_err.h"
#include "val_string.h"
#include "val_list.h"
#include "val_math.h"
#include "helpers.h"

#include <ctype.h>


int vm_classify(char c);
const char* vm_get_classname(int pclass);
const char* vm_get_statename(int pstate);


//parse string token into val_t
// - doesn't parse lists, (but is used to parse grouping ops)
// - called during parse to build vals from tokens
// - currently ignores parse state, and just uses the first character to determine val
//   - except for number/ident where we try number first, then ident, then error
// FIXME: vm_parser refactor - clone from source val where possible (need extra argument)
err_t vm_parse_tok(const char *tok, int len, enum parse_state state, enum parse_state target, val_t *v) {
  if (len == 0) { //empty string
    *v = VAL_NULL;
    return 0;
  } else if (tok[0] == '"' || tok[0] == '\'') { //string val
    return val_string_init_quoted(v,tok,len);
  } else if (tok[0] == '#') { //comment
    *v = VAL_NULL;
    return 0;
  } else if (tok[0] == '\\') { //escaped ident val
    return val_ident_init_cstr(v,tok,len);
  } else if (is_op_str(tok,len)) { //operator (ident val)
    return val_ident_init_cstr(v,tok,len);
  } else if (0 == val_num_parse(v,tok,len)) { //number val
    return 0;
  } else if (is_identifier(tok,len)) { //ident val
    return val_ident_init_cstr(v,tok,len);
  } else { //invalid tok (or we could go wild and make anything else an ident)
    //FIXME: don't fatal or print error at the parse_tok level (move to higher level)
    fprintf(stderr,"invalid token '%.*s' with end state %d\n",len,tok,state);
    return -1;
  }
}

// vm_parse_input_handler - handles tokens for parse_input
// - just calls parse_tok and rpushes onto the list arg
int vm_parse_input_handler(const char *tok, int len, int state, int target, void* arg) {
  int ret;
  valstruct_t *code=arg;
  val_t t;

  if ((ret = vm_parse_tok(tok,len,state,target,&t))) return ret;
  if (!val_is_null(t)) {
    if ((ret = _val_lst_rpush(code,t))) return ret;
  }
  return ret;
}


// _vm_parse_input - parses string to code (treating parens/brackets like any other ident)
int _vm_parse_input(struct parser_rules *p, const char *str, int len, valstruct_t *code) {
  if (len == 0 || str[0] == '\0') return 0;
  return parser_eval(p,str,len,NULL,vm_parse_input_handler,code,NULL,NULL,vm_parse_input_handler,code);
}

// vm_parse_code_handler - handles tokens for parse_code, with grouping op handling
//
// This parsing function keeps track of nesting depth and looks for grouping operators: ()[]
// - handles list/code open/close and errors on mismatch
// FIXME: vm_parser refactor - don't return -1 (FATAL)
int vm_parse_code_handler(const char *tok, int len, int state, int target, void* arg) {
  struct vm_parse_code_state *pstate = arg;

  int r;

  val_t t;
  if ((r = vm_parse_tok(tok,len,state,target,&t))) return r;

  valstruct_t *v = __str_ptr(t); //need to check for val_is_str() before using

  if (val_is_str(t) && v->type == TYPE_IDENT && _val_str_len(v) == 1 && is_grouping_op(*_val_str_begin(v))) { //group ops
    unsigned int i;
    switch(*_val_str_begin(v)) {
      case '[':
        val_destroy(t);
        t = val_empty_code();
        if ((r = _val_lst_rpush(pstate->open_list,t))) return -1;
        pstate->groupi++;
        pstate->open_list = __lst_ptr(t);
        break;
      case ']':
        val_destroy(t);
        if (pstate->groupi<1) return -1;
        else if (pstate->open_list->type != TYPE_CODE) return -1;
        pstate->groupi--;
        pstate->open_list = pstate->root_list;
        for(i=0;i<pstate->groupi;++i) {
          pstate->open_list = __lst_ptr(_val_lst_end(pstate->open_list)[-1]);
        }
        break;
      case '(':
        val_destroy(t);
        t = val_empty_list();
        if ((r = _val_lst_rpush(pstate->open_list,t))) return -1;
        pstate->groupi++;
        pstate->open_list = __lst_ptr(t);
        break;
      case ')':
        val_destroy(t);
        if (pstate->groupi<1) return -1;
        else if (pstate->open_list->type != TYPE_LIST) return -1;
        pstate->groupi--;
        pstate->open_list = pstate->root_list;
        for(i=0;i<pstate->groupi;++i) {
          pstate->open_list = __lst_ptr(_val_lst_end(pstate->open_list)[-1]);
        }
        break;
      default: return -1; //unhandled case (should never happen)
    }
  } else if (!val_is_null(t)) {
    return _val_lst_rpush(pstate->open_list,t);
  } else { //skip null vals
  }
  return 0;
}

// _vm_parse_code_(p,str,len,pstate) - parses string to code (parsing grouping ops to build nested lsts)
// - parses list/code grouping ops to build list/code vals
// - takes pstate to allow continuation
// - can initialize pstate to start parsing with open list
// - if groupi = 0 at end, all open lists have been closed
int _vm_parse_code_(struct parser_rules *p, const char *str, int len, struct vm_parse_code_state *pstate) {
  if (len == 0 || str[0] == '\0') return 0;
  return parser_eval(p,str,len,NULL,vm_parse_code_handler,pstate,NULL,NULL,vm_parse_code_handler,pstate);
}

// _vm_parse_code(p,str,len,code) -  parses string to code (parsing grouping ops and verifying complete val)
int _vm_parse_code(struct parser_rules *p, const char *str, int len, valstruct_t *code) {
  struct vm_parse_code_state pstate = { .root_list = code, .open_list = code, .groupi = 0 };

  int r;
  if (0>(r = _vm_parse_code_(p,str,len,&pstate))) return _throw(ERR_BADPARSE);
  if (pstate.groupi!=0) return _throw(ERR_BADPARSE); //make sure we closed out any groups
  return 0;
}

// vm_get_parser() - get a pointer to the shared concat parser rules
// - initializes on first call
// TODO: eventually we should store this statically so we don't need to actually call the init functions every time
struct parser_rules* vm_get_parser() { //get shared parser
  static struct parser_rules *p = NULL;
  if (!p) p = vm_new_parser();
  return p;
}

// vm_new_parser() - construct concat parser rules
struct parser_rules* vm_new_parser() {
  struct parser_rules *p = NULL;

  // concat code parsing rules:
  //
  // The below comment block is out of date (I simplified the parser to reduce fsm size and reduce duplicated validation)
  // I'm leaving it for the moment for posterity, but just until the vm_parser refactor
  //
  // See the comments lower down where we actually build the fsm rules for details of the current parsing/tokenizing rules

  //FIXME: vm_parser refactor - remove/rewrite below comment block
  //rpn calc language:
  //TODO: this needs rewrite, out of date now (the language spec and code comments, the code should be correct)
  //  - the parser now treats idents and numbers the same (only special rules so we can differentiate sign vs op)
  //  - idents now keep ops (though op after number splits)
  //TODO: consider simpler parser (e.g. indent/number/op splitting just using space and "()[]", plus maybe "{}")
  //  - main difference is that we would change most op chars into ident chars
  //
  //symbols:
  //digits: '0123456789'
  //operators: '~!@#$%^&*()={}[]<>_,;+-' (note: '_' overriden by ident char currently)
  //letters: 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'
  //ident chars: '_0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz'
  //
  //regex:
  //digit = [0-9]
  //float_char = [-+.0-9]
  //exp_char = [-+e0-9]
  //number_char = [-+.e0-9]
  //ident_leadchar = [_A-Za-z]
  //ident_char = [._0-9A-Za-z]
  //op_char = [~!@#$%^&*()={}[]<>_,;]
  //sign_char = [-+]
  //any_char = any char
  //dstring_char = [^\\"]
  //sstring_char = [^']
  //
  //number = [-+]?([0-9]+(\.[0-9]*)?|\.[0-9]+)(e[-+]?[0-9]+)?
  //ident = [_A-Za-z][_0-9A-Za-z]*
  //operator = 
  //
  //BNF:
  //integer = digit | integer digit;
  //float = integer | integer '.' | '.' integer | integer '.' integer;
  //exp = 'e' integer;
  //number = float | float exp;
  //
  //ident_leadchar = letter | '_';
  //ident_char = letter | '_' | digit;
  //ident_tail = | ident_tail ident_char;
  //ident = ident_leadchar ident_tail;
  //
  //dstring = | dstring_char | dstring dstring_char | dstring '\\' anychar dstring;
  //dquoted_string = '"' dstring '"';
  //
  //sstring = | sstring_char | sstring sstring_char;
  //squoted_string = '\'' sstring '\'';
  //



  p = malloc(sizeof(struct parser_rules));
  if (!p) return NULL;
  if (parser_rules_init(p,
      PSTATE_COUNT+1, //number of states, +1 for init
      PCLASS_COUNT, //number of classes
      PSTATE_INIT, //initial state
      PSTATE_FIN, //final state (to terminate fsm)
      -1, //err_state (not used here, we can just set err flag)
      vm_classify //char classifier
      //,rpn_handler, //token handlerA
      //NULL //no handlerB
      )
  ) {
    return NULL; //failed to init parser
  }


  // == base rule -- error ==
  //   fsm[all][all] = (ERR, FIN)
  parser_set_all_op_target(p,                 PARSE_ERR,           PSTATE_FIN); //initialize everything to errors

  //
  // == baseline rules to start/end tokens ==
  //

  // if '\0' handle final token and stop (null terminated string)
  //* NULL splits before and goto FIN
  //   fsm[all][NULL] = (SPLIT before, FIN)
  parser_set_global_op_target(p,PCLASS_NULL,  PARSE_SPLITA_BEFORE, PSTATE_FIN); //null char always handles last token and terminates

  // whitespace (SPACE|NEWLINE) skip splits and goto INIT
  // TODO: collect whitespace tokens? we can then ignore by state (or add opcode to drop before/after, or replace skip with drop)
  //   - drop before/after instead of second handler to make dropping whitespace very cheap
  //
  // -- these rules split-skip on whitespace and goto INIT --
  //   fsm[all][SPACE] = (SPLIT skip, INIT)
  //   fsm[all][NEWLINE] = (SPLIT skip, INIT)
  parser_set_global_op_target(p,PCLASS_SPACE, PARSE_SPLITA_SKIP,   PSTATE_INIT); //whitespace splits token and sends us back to init
  parser_set_global_op_target(p,PCLASS_NEWLINE, PARSE_SPLITA_SKIP,   PSTATE_INIT); //whitespace splits token and sends us back to init

  // These are the rules for pclasses that start a new token of their own type 
  // - split before and goto state with same name as pclass
  //
  // -- these rules all split before and start a new token based on current char --
  //   fsm[all][DIGIT] = (SPLIT before, DIGIT)
  //   fsm[all][IDENT] = (SPLIT before, IDENT)
  //   fsm[all][IDENT_ESCAPE] = (SPLIT before, IDENT_ESCAPE)
  //   fsm[all][OP] = (SPLIT before, OP)
  //   fsm[all][SIGN] = (SPLIT before, SIGN)
  //   fsm[all][CLOSE_GROUP] = (SPLIT before, CLOSE_GROUP)
  //   fsm[all][COMMENT] = (SPLIT before, COMMENT)
  //   fsm[all][SSTRING] = (SPLIT before, SSTRING)
  //   fsm[all][DSTRING] = (SPLIT before, DSTRING)
  parser_set_global_op_target(p,PCLASS_DIGIT, PARSE_SPLITA_BEFORE, PSTATE_DIGIT); //general case is that digits switch us to ident mode
  parser_set_global_op_target(p,PCLASS_IDENT, PARSE_SPLITA_BEFORE, PSTATE_IDENT); //remaining letters all send us to ident case
  parser_set_global_op_target(p,PCLASS_BSLASH, PARSE_SPLITA_BEFORE, PSTATE_IDENT_ESCAPE); //leading backslashes to escape identifier
  parser_set_global_op_target(p,PCLASS_OP,    PARSE_SPLITA_BEFORE, PSTATE_OP); //operator chars send us to op state
  parser_set_global_op_target(p,PCLASS_SIGN,  PARSE_SPLITA_BEFORE, PSTATE_SIGN); //sign chars send us to sign state
  parser_set_global_op_target(p,PCLASS_CLOSE_GROUP,    PARSE_SPLITA_BEFORE, PSTATE_CLOSE_GROUP); //close group chars always close the current group
  parser_set_global_op_target(p,PCLASS_HASH, PARSE_SPLITA_BEFORE, PSTATE_COMMENT); //# sends us to comment
  parser_set_global_op_target(p,PCLASS_SQUOTE, PARSE_SPLITA_BEFORE, PSTATE_SSTRING); //dquote sends us to single quoted string case
  parser_set_global_op_target(p,PCLASS_DQUOTE, PARSE_SPLITA_BEFORE, PSTATE_DSTRING); //dquote sends us to double quoted string case

  //
  // == ident parsing rules ==
  //

  //   These rules handle ident and number continuation
  //
  // The current ident/number fsm states have a simplified ruleset (with final validation in vm_parse_tok).
  // - this probably needs a redesign, but is a simplified version of the old complete fsm number parser
  // - currently not precise enough to tag tokens for parsing number vs ident (but smaller fsm)
  // FIXME: vm_parser refactor - review and possibly update these rules
  //
  // DIGIT stays in DIGIT for DIGIT chars
  // SIGN goes to DIGIT for DIGIT chars   -- handles sign prefix on number (the only time sign isn't an op)
  //
  // IDENT stays in IDENT for (IDENT|OP|SIGN) chars
  // DIGIT goes to IDENT for IDENT chars -- lets things like 2dup work
  // IDENT goes to DIGIT for DIGIT chars
  //
  // IDENT_ESCAPE stays in IDENT_ESCAPE for BSLASH char
  // IDENT_ESCAPE goes to IDENT for IDENT chars
  // IDENT_ESCAPE goes to DIGIT for DIGIT chars
  // IDENT_ESCAPE goes to OP for (OP|SIGN) chars
  //
  //
  // -- these rules all do NOSPLIT (continue building the current token) --
  //   fsm[SIGN][DIGIT] = (NOSPLIT, DIGIT)
  //   fsm[IDENT][IDENT,NOSPLIT,OP,SIGN] = (NOSPLIT, IDENT)
  //   fsm[DIGIT][IDENT] = (NOSPLIT, IDENT)
  //   fsm[IDENT][DIGIT] = (NOSPLIT, DIGIT)
  //   fsm[DIGIT][DIGIT] = (NOSPLIT, DIGIT)
  //   fsm[IDENT_ESCAPE][IDENT] = (NOSPLIT, IDENT)
  //   fsm[IDENT_ESCAPE][DIGIT] = (NOSPLIT, DIGIT)
  //   fsm[IDENT_ESCAPE][PCLASS_BSLASH] = (NOSPLIT, IDENT_ESCAPE)
  //   fsm[IDENT_ESCAPE][OP,PCLASS_SIGN] = (NOSPLIT, OP)
  parser_set_list_op_target(p,PSTATE_SIGN,           PARSE_NOSPLIT,       PSTATE_DIGIT,   1, PCLASS_DIGIT); //sign goes to digit (if digit follows)
  parser_set_list_op_target(p,PSTATE_IDENT,          PARSE_NOSPLIT,       PSTATE_IDENT,   3, PCLASS_IDENT, PCLASS_OP, PCLASS_SIGN); //idents keep ident chars (ident,digits, 'e', and '.')
  parser_set_list_op_target(p,PSTATE_DIGIT,          PARSE_NOSPLIT,       PSTATE_IDENT,   1, PCLASS_IDENT); //idents keep ident chars (ident,digits, 'e', and '.')
  parser_set_list_op_target(p,PSTATE_IDENT,          PARSE_NOSPLIT,       PSTATE_DIGIT,   1, PCLASS_DIGIT); //idents keep ident chars (ident,digits, 'e', and '.')
  parser_set_list_op_target(p,PSTATE_DIGIT,          PARSE_NOSPLIT,       PSTATE_DIGIT,   1, PCLASS_DIGIT); //idents keep ident chars (ident,digits, 'e', and '.')
  parser_set_list_op_target(p,PSTATE_IDENT_ESCAPE,   PARSE_NOSPLIT,       PSTATE_IDENT,   1, PCLASS_IDENT); //idents keep ident chars (ident,digits, 'e', and '.')
  parser_set_list_op_target(p,PSTATE_IDENT_ESCAPE,   PARSE_NOSPLIT,       PSTATE_DIGIT,   1, PCLASS_DIGIT); //idents keep ident chars (ident,digits, 'e', and '.')
  parser_set_op_target(p,PSTATE_IDENT_ESCAPE,PCLASS_BSLASH,PARSE_NOSPLIT,PSTATE_IDENT_ESCAPE); //can have multiple leading backslashes to escape ident
  parser_set_list_op_target(p,PSTATE_IDENT_ESCAPE,PARSE_SPLITA_AFTER, PSTATE_OP, 2, PCLASS_OP, PCLASS_SIGN); //op chars can also be escaped with bslash

  //   fsm[IDENT_ESCAPE][OP] = (NOSPLIT, OP, PCLASS_CLOSE_GROUP)
  //parser_set_list_op_target(p,PSTATE_IDENT_ESCAPE,   PARSE_NOSPLIT,       PSTATE_OP,   2, PCLASS_OP, PCLASS_CLOSE_GROUP); //idents keep ident chars (ident,digits, and 'e')
  
  //
  // == comment parsing rules ==
  //

  // COMMENT goes to COMMENT for all chars except (NEWLINE|NULL)
  // COMMENT goes to INIT for (NEWLINE|NULL) -- comment ends at end of line or string
  //
  //
  //   fsm[COMMENT][all] = (NOSPLIT, COMMENT)
  //   fsm[COMMENT][PCLASS_NEWLINE] = (SPLITB_AFTER, INIT)
  //   fsm[COMMENT][PCLASS_NULL] = (SPLITB_AFTER, INIT)
  parser_set_state_op_target(p,PSTATE_COMMENT, PARSE_NOSPLIT,       PSTATE_COMMENT); //comment always goes to comment (except '\n' or '\0')
  parser_set_op_target(p,PSTATE_COMMENT,PCLASS_NEWLINE,PARSE_SPLITB_AFTER,PSTATE_INIT); //newline ends comment
  parser_set_op_target(p,PSTATE_COMMENT,PCLASS_NULL,PARSE_SPLITB_AFTER,PSTATE_INIT); //newline ends comment

  //
  // == single quoted string parsing rules ==
  //

  // SSTRING goes to INIT for SQUOTE char -- single quoted string ends on '\''
  // SSTRING stays in SSTRING for all chars except SQUOTE -- single-quoted string has no escapes
  //
  //   fsm[SSTRING][all] = (NOSPLIT, SSTRING)
  //   fsm[SSTRING][PCLASS_SQUOTE] = (SPLITA_AFTER, INIT)
  parser_set_state_op_target(p,PSTATE_SSTRING, PARSE_NOSPLIT,       PSTATE_SSTRING); //string always goes to string (except '\'')
  parser_set_op_target(p,PSTATE_SSTRING,PCLASS_SQUOTE,PARSE_SPLITA_AFTER,PSTATE_INIT); //dquote ends string

  //
  // == double quoted string parsing rules ==
  //

  //   DSTRING goes to INIT for DQUOTE char -- end of string '"'
  //   DSTRING stays in DSTRING for all except (DQUOTE|BSLASH)
  //   DSTRING goes to DSTRING_ESCAPE for BSLASH -- bslash starts escape
  //   DSTRING_ESCAPE goes back to DSTRING for any char -- bslash escapes any char into string
  //   - note that for parsing, escapes can be more than 1 char
  //   - the tokenizer just knows that the next character is in the string no matter what
  //
  //   fsm[DSTRING][all] = (NOSPLIT, DSTRING)
  //   fsm[DSTRING][PCLASS_DQUOTE] = (SPLITA_AFTER, INIT)
  //   fsm[DSTRING][PCLASS_BSLASH] = (NOSPLIT, DSTRING_ESCAPE)
  //   fsm[DSTRING_ESCAPE][all] = (NOSPLIT, DSTRING)
  parser_set_state_op_target(p,PSTATE_DSTRING, PARSE_NOSPLIT,       PSTATE_DSTRING); //string always goes to string (except '"' and '\\')
  parser_set_op_target(p,PSTATE_DSTRING,PCLASS_DQUOTE,PARSE_SPLITA_AFTER,PSTATE_INIT); //dquote ends string
  parser_set_op_target(p,PSTATE_DSTRING,PCLASS_BSLASH,PARSE_NOSPLIT,PSTATE_DSTRING_ESCAPE); //in escape state we keep all chars in string
  parser_set_state_op_target(p,PSTATE_DSTRING_ESCAPE, PARSE_NOSPLIT,       PSTATE_DSTRING); //after escape always go back to string

  //+/- special cases -- trailing number (where it becomes an op), or trailing group (where it becomes an op)
  //
  // DIGIT,CLOSEGROUP go to OP on SIGN -- +/- following digit or )/] is an op and not a sign before a number
  //
  //   fsm[DIGIT][SIGN] = (SPLITA_BEFORE, OP)
  //   fsm[CLOSE_GROUP][SIGN] = (SPLITA_BEFORE, OP)
  parser_set_op_target(p,PSTATE_DIGIT,PCLASS_SIGN,PARSE_SPLITA_BEFORE,PSTATE_OP); //sign after digit is op
  parser_set_op_target(p,PSTATE_CLOSE_GROUP,PCLASS_SIGN,PARSE_SPLITA_BEFORE,PSTATE_OP); //sign after end-of-group is op TODO: ??? for concat

  // all states go to FIN on NULL char
  // COMMENT goes to INIT on NULL -- previously we used separate handler for comments, so this doesn't make sense anymore
  // FIXME: vm_parser refactor - drop extra comment rule
  //
  //   fsm[all][NULL] = (SPLIT before, FIN)
  //   fsm[COMMENT][NULL] = (SPLITB_AFTER, INIT)
  parser_set_global_op_target(p,PCLASS_NULL,  PARSE_SPLITA_BEFORE, PSTATE_FIN); //repeated rule to make sure it is last and always terminates the FSM
  parser_set_op_target(p,PSTATE_COMMENT,PCLASS_NULL,PARSE_SPLITB_AFTER,PSTATE_INIT); //make sure we don't try to process comments (repeated rule)
  return p;
}

//parser internals

int vm_classify(char c) {
  //printf("rpn_classify '%c'\n",c);
  switch(c) {
    case '\0': return PCLASS_NULL;
    case '\\': return PCLASS_BSLASH;
    case '\n': return PCLASS_NEWLINE;
    case '#': return PCLASS_HASH;
    case '\'': return PCLASS_SQUOTE;
    case '"': return PCLASS_DQUOTE;
    case '+':
    case '-': return PCLASS_SIGN;
    case '_':
    case '.': return PCLASS_IDENT;
    default:
      if (isspace(c)) return PCLASS_SPACE;
      else if (isdigit(c)) return PCLASS_DIGIT;
      else if (isalpha(c)) return PCLASS_IDENT;
      else if (is_close_group(c)) return PCLASS_CLOSE_GROUP;
      else if (is_op(c)) return PCLASS_OP;
      else return PCLASS_OTHER;
  }
}

const char* vm_get_classname(int pclass) {
  switch(pclass) {
    case PCLASS_NULL: return "null-terminator";
    case PCLASS_BSLASH: return "backslash";
    case PCLASS_NEWLINE: return "newline";
    case PCLASS_HASH: return "hash symbol";
    case PCLASS_SQUOTE: return "single-quote";
    case PCLASS_DQUOTE: return "double-quote";
    case PCLASS_SPACE: return "whitespace";
    case PCLASS_DIGIT: return "digit";
    case PCLASS_SIGN: return "sign char";
    case PCLASS_IDENT: return "identifier";
    case PCLASS_OP: return "operator";
    case PCLASS_CLOSE_GROUP: return "end-of-group operator";
    case PCLASS_OTHER: return "invalid char";
    //default: return NULL;
  }
  return NULL; //just for complainy compilers
}

const char* vm_get_statename(int pstate) {
  switch(pstate) {
    case PSTATE_INIT: return "init/whitespace";
    //case PSTATE_SIGN: return "sign";
    //case PSTATE_NUMBER: return "number";
    case PSTATE_DSTRING_ESCAPE: return "escaped char";
    case PSTATE_SSTRING: return "single quoted string";
    case PSTATE_DSTRING: return "double quoted string";
    case PSTATE_IDENT_ESCAPE: return "escaped identifier";
    case PSTATE_IDENT: return "identifier";
    case PSTATE_OP: return "operator";
    case PSTATE_CLOSE_GROUP: return "end of group";
    case PSTATE_COMMENT: return "code comment";
    case PSTATE_SIGN: return "sign";
    default: return NULL;
  }
  return NULL; //just for complainy compilers
}
