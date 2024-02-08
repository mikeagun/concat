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

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "parser.h"

//parser.c - single-pass finite-state-machine parser implementation


// BITS/MASK - used for entry (op,next_state) packing
#define _PSTATE_BITS_ (5)
static const int _PSTATE_MASK_ = ((1<<_PSTATE_BITS_)-1);

// gets friendly op description
const char* parser_get_opname(enum parse_op op) {
  switch(op) {
    case PARSE_NOSPLIT: return "keep";
    case PARSE_SPLITA_BEFORE: return "split before";
    case PARSE_SPLITA_AFTER: return "split after";
    case PARSE_SPLITA_SKIP: return "split skip";
    case PARSE_NOOP_4: return "ERROR: parser no-op 4";
    case PARSE_NOOP_5: return "ERROR: parser no-op 5";
    case PARSE_NOOP_6: return "ERROR: parser no-op 6";
    case PARSE_ERR: return "parse error";
    default: return NULL;
  }
}

//parser_state_init - initialize parser to start new parse
// - just sets state to -1 to init when we use it
void parser_state_init(struct parser_state *pstate) {
  pstate->state = -1; //if state<0, everything else ignored
}

//parser_rules_init - initialize parser rules
int parser_rules_init(struct parser_rules *p, int nstates, int nclasses, int init_state, int fin_state, int _err, char_classifier_t *classifier) {
  p->v = malloc(sizeof(state_entry_t) * nstates * nclasses);
  if (!(p->v)) return -1;
  p->nstates=nstates;
  p->nclasses=nclasses;
  p->init_state=init_state;
  p->fin_state=fin_state;
  p->classify = classifier;
  return 0;
}

void parser_rules_clear(struct parser_rules *p) {
  p->v=NULL;
  p->nstates=p->nclasses=0;
  p->classify=NULL;
}

void parser_rules_destroy(struct parser_rules *p) {
  free(p->v);
  p->v=NULL;
  p->nstates=p->nclasses=0;
  p->classify=NULL;
}

state_entry_t calc_state_entry(enum parse_op op, int next_state) {
  return (op<<_PSTATE_BITS_) | (next_state&_PSTATE_MASK_);
}

state_entry_t parser_get_entry(struct parser_rules *p, int state, int pclass) {
  return p->v[state*p->nclasses + pclass];
}

void parser_set_entry(struct parser_rules *p, int state, int pclass, state_entry_t e) {
  p->v[state*p->nclasses + pclass] = e;
}

void parser_set_op_target(struct parser_rules *p, int state, int pclass, enum parse_op op, int next_state) {
  p->v[state*p->nclasses + pclass] = calc_state_entry(op,next_state);
}

void parser_set_target(struct parser_rules *p, int state, int pclass, int next_state) {
  state_entry_t e = p->v[state*p->nclasses + pclass];
  e &= ~_PSTATE_MASK_; //clear next_state
  e |= next_state; //OR in next_state
  p->v[state*p->nclasses + pclass] = e;
}

void parser_set_op(struct parser_rules *p, int state, int pclass, enum parse_op op) {
  state_entry_t e = p->v[state*p->nclasses + pclass];
  e &= _PSTATE_MASK_; //clear everything but next_state
  e |= op<<_PSTATE_BITS_; //OR in op
  p->v[state*p->nclasses + pclass] = e;
}

//void parser_set_all_entry(struct parser_rules *p, state_entry_t e) {
//  int i;
//  for(i=0; i<(p->nstates*p->nclasses); ++i) {
//    p->v[i] = e;
//  }
//}
void parser_set_all_op_target(struct parser_rules *p, enum parse_op op, int next_state) {
  int i;
  state_entry_t e = calc_state_entry(op,next_state);
  for(i=0; i<(p->nstates*p->nclasses); ++i) {
    p->v[i] = e;
  }
}

void parser_set_state_op_target(struct parser_rules *p, int state, enum parse_op op, int next_state) {
  int i;
  state_entry_t e = calc_state_entry(op,next_state);
  for(i=0; i<p->nclasses; ++i) {
    parser_set_entry(p,state,i,e);
  }
}

//void parser_set_global_entry(struct parser_rules *p, int pclass, state_entry_t e) {
//  int i;
//  for(i=0;i<p->nstates;++i) {
//    parser_set_entry(p,i,pclass,e);
//  }
//}

void parser_set_global_op_target(struct parser_rules *p, int pclass, enum parse_op op, int next_state) {
  int i;
  state_entry_t e = calc_state_entry(op,next_state);
  for(i=0;i<p->nstates;++i) {
    parser_set_entry(p,i,pclass,e);
  }
}

void parser_set_global_target(struct parser_rules *p, int pclass, int next_state) {
  int i;
  for(i=0;i<p->nstates;++i) {
    parser_set_target(p,i,pclass,next_state);
  }
}

void parser_set_global_op(struct parser_rules *p, int pclass, enum parse_op op) {
  int i;
  for(i=0;i<p->nstates;++i) {
    parser_set_op(p,i,pclass,op);
  }
}

void parser_set_list_target(struct parser_rules *p, int state, int next_state, int nclasses, ...) {
  va_list ap;
  va_start(ap, nclasses);
  for(;nclasses;--nclasses) {
    parser_set_target(p,state,va_arg(ap,int),next_state);
  }
  va_end(ap);
}

void parser_list_set_target(struct parser_rules *p, int pclass, int next_state, int nstates, ...) {
  va_list ap;
  va_start(ap, nstates);
  for(;nstates;--nstates) {
    parser_set_target(p,va_arg(ap,int),pclass,next_state);
  }
  va_end(ap);
}

void parser_set_list_op(struct parser_rules *p, int state, int op, int nclasses, ...) {
  va_list ap;
  va_start(ap, nclasses);
  for(;nclasses;--nclasses) {
    parser_set_op(p,state,va_arg(ap,int),op);
  }
  va_end(ap);
}

void parser_list_set_op(struct parser_rules *p, int pclass, int op, int nstates, ...) {
  va_list ap;
  va_start(ap, nstates);
  for(;nstates;--nstates) {
    parser_set_op(p,va_arg(ap,int),pclass,op);
  }
  va_end(ap);
}

void parser_set_list_op_target(struct parser_rules *p, int state, int op, int next_state, int nclasses, ...) {
  va_list ap;
  va_start(ap, nclasses);
  for(;nclasses;--nclasses) {
    parser_set_op_target(p,state,va_arg(ap,int),op,next_state);
  }
  va_end(ap);
}

void parser_list_set_op_target(struct parser_rules *p, int pclass, int op, int next_state, int nstates, ...) {
  va_list ap;
  va_start(ap, nstates);
  for(;nstates;--nstates) {
    parser_set_op_target(p,va_arg(ap,int),pclass,op,next_state);
  }
  va_end(ap);
}

void parser_set_state_list_op_target(struct parser_rules *p, enum parse_op op, int next_state, int nstates, ...) {
  int i;
  state_entry_t e = calc_state_entry(op,next_state);
  va_list ap;
  va_start(ap, nstates);
  for(;nstates;--nstates) {
    int state=va_arg(ap,int);
    for(i=0; i<p->nclasses; ++i) {
      parser_set_entry(p,state,i,e);
    }
  }
  va_end(ap);
}

// parser_validate_rules - verify that rules are safe (no invalid next states)
// - returns 0 if rules are safe, -1 for invalid class, and 1 for no fin state targets
// - doesn't check correctness or termination, but checks for:
//   - invalid init state
//   - at least one fin_state
//   - no invalid next_states (must be < nstates or = fin_state)
// - if all parser rules used are validated (or known valid), then PARSER_SAFETY_CHECKS can be disabled
int parser_validate_rules(struct parser_rules *p) {
  //invalid init rule
  if (p->init_state >= p->nstates) return -1;

  int no_fin = 1;

  int i;
  int size = p->nclasses * p->nstates;
  for (i=0; i < size; ++i) {
    state_entry_t entry = p->v[i];
    int next_state = entry & _PSTATE_MASK_;
    if (next_state >= p->nstates && next_state != p->fin_state) return -1;
    else if (next_state == p->fin_state) no_fin = 0;
  }
  return no_fin;
}

//parser_validate - validate a string
//  - tokenizing is ignored, and the parser simply determines whether we reach the FIN state or error
//  - if (!state || *state <= 0), then state and iterator initialized to init state and str
//    - otherwise state and iterator initialized to *state and *ptr (so str is ignored)
// - len sets the max number of chars the parser will look at
//   - can just set arbitrarily high (e.g. -1) if FSM termination is guaranteed (e.g. null-terminated string)
int parser_validate(struct parser_rules *p, const char *str, int len, int *state, const char **ptr) {
  int pclass;
  state_entry_t entry;
  enum parse_op op;

  int ret=0, _state = p->init_state;
  //use state if already initialized
  if (state && *state >= 0) {
    _state = *state;
    str = *ptr;
  }

#ifdef PARSER_SAFETY_CHECKS
  // check for invalid start state
  if (_state < 0 || _state >= p->nstates) goto parse_error;
#endif

  //if we restrict to only terminating FSM we can skip this check (while(1) or goto loop)
  // - but in that case we need some additional return values to distinguish between parse error and end of string
while (len--) {
  pclass = p->classify(*str++);
#ifdef PARSER_SAFETY_CHECKS
  if (pclass >= p->nclasses) goto parse_error; // invalid pclass
#endif
  entry = parser_get_entry(p,_state,pclass); // lookup entry
  op = entry >> _PSTATE_BITS_; // unpack op/next_state
  _state = entry & _PSTATE_MASK_;

  if (op > PARSE_SPLITA_SKIP) goto parse_error; //error/invalid op
  else if (_state == p->fin_state) goto parser_exit; //fin state -- done
#ifdef PARSER_SAFETY_CHECKS
  else if (_state >= p->nstates) goto parse_error; //invalid state
#endif
}

parse_error: //end of loop with return value = -1
  ret = -1;
parser_exit: // save parser state and return
  if (state) *state = _state;
  if (ptr) *ptr = str;
  return ret;
}

// parser_eval - tokenize a string using parser ruleset, calling handler on each token
//
// - returns all tokens through handler
//   - skips handler call for empty tokens
// - pstate is optional and can be used to preserve state between parses
//   - by saving state parse can be continued from exactly where it left off (e.g. with new bytes)
// - any time the parser reaches a SPLIT rule, it calls handlers with the finished tok, except when both the previous and current op are SPLIT_SKIP ops
// - if there is any remaining text when len is reached, tail_handler is called with that
// - if (!pstate || str != null) then parser is initialized to init state
//   - otherwise parser is initialized using pstate and starts from there (so str is ignored)
// - len sets the max number of chars the parser will look at
//   - can just set arbitrarily high (e.g. -1) if FSM termination is guaranteed (e.g. null-terminated string)
// - stops on fin state, end of string, or error
//
// The code looks very convoluted, but the core "loop" is only around 30 lines of code (without comments).
// Since it does look like a mess at first I've thoroughly documented each of the jump labels.
//
// The vm that handles tokenizing and updates state dispatches by jumping directly to opcode handler by table lookup.
// Instead of ordering op handlers by opcode, they're placed to maximize fall through cases (minimize jumps).
// 
//
int parser_eval(struct parser_rules *p, const char *str, int len, struct parser_state *pstate, parse_handler_t *handler, void *arg, parse_handler_t *_h, void *_arg, parse_handler_t *tail_handler, void *arg_tail) {
  int state, next_state, pclass, len_off, tok_off;
  int ret = 0;
  enum parse_op op;
  const char *tok; //keeps start of token
  state_entry_t entry;

  // First load or initialize parser state
  // - if empty string
  //we set next_state instead of state because main loop loads next_state
  if (pstate && !str) { //if continuing parse, load parse state
    next_state = pstate->state; //we set next_state instead of state because main loop loads next_state
    tok = pstate->tok;
    str = pstate->ptr;
    len = pstate->len;
  } else { //else initilize parse state
    next_state = p->init_state;
    tok = str;
  }

  // ops - parser op jump table - one for each op (3 bits - 8 ops)
  const void * const ops[] = {
    &&begin_loop, &&op_PARSE_SPLITA_BEFORE, &&op_PARSE_SPLITA_AFTER, &&op_PARSE_SPLITA_SKIP,
    &&parse_error, &&parse_error, &&parse_error, &&parse_error
  };

#ifdef PARSER_SAFETY_CHECKS
  //check for invalid initial state
  if (next_state < 0 || next_state >= p->nstates) goto parse_error;
#endif

  goto begin_loop; //go to start of loop

// ================================ VM/FSM Core ================================
// 1. use classifier from fsm rules to map input byte -> pclass
// 2. get (opcode,next_state) from fsm table lookup fsm[state][pclass]
//   - opcode decides what to do with char, next_state provides the state transition
// 3. jump directly to the next op handler (goto ops[opcode])
//   - op handlers are ordered to fall through instead of jump where possible
// 4. call token handler callback on every token split (drop empty tokens)
// 5. terminate on fin state, end of string, or error

// ================ Error Handler ================
//   1. set ret = -1
//   2. jump to Parser Exit
parse_error: //end of loop with return value = -1
  ret = -1;
  goto parser_exit;

// ================ Main Parser Loop ================
//   // main loop classifies char, looks up entry, and jumps to op (via lookup table)
//   -- NOSPLIT op jumps directly here
//   1. if end of string jump to End of String code
//   2. get pclass for next char (call classifier)
//   ## if pclass invalid jump to Error Handler (only if if PARSER_SAFETY_CHECKS enabled)
//   3. lookup entry = fsm[state][pclass]
//   4. unpack entry into (op, next_state)
//   5. jump to ops[op] Op Handler (NOOP through ERR jump to Error Handler)
begin_loop: //loop starts here
  //printf("begin loop: state=%d, len=%d\n",state,len);
  state=next_state;
  if (len == 0) goto string_end; //if we restrict to only terminating FSM we can skip this check
  --len;
  pclass = p->classify(*str++);
#ifdef PARSER_SAFETY_CHECKS
  if (pclass >= p->nstates) goto parse_error;
#endif
  entry = parser_get_entry(p,state,pclass);
  op = entry >> _PSTATE_BITS_;
  next_state = entry & _PSTATE_MASK_;
  //printf("loop: state=%d, toklen=%d, char='%c', pclass=%d, op(%d)=%s, next_state=%d\n",state, (int)(str-tok), str[-1], pclass, op, parser_get_opname(op), next_state);
  //loop: state=0, char='p', pclass=10, op(1)=split before, next_state=4
  goto *ops[op];

// ================ Split Before/After Op Handlers ================
//   // split off a token (before/after current char) and jump to Token Handler
//   1. set len_off - 0/1: whether current token excludes/includes this char
//   2. set tok_off - 0/1: whether next token excludes/includes this char
//   3. jump to Token Handler (split before jumps to length check before handler)
op_PARSE_SPLITA_BEFORE: len_off = 1; tok_off = 1; goto len_check;
op_PARSE_SPLITA_AFTER:  len_off = 0; tok_off = 0; goto tok_handle;

// ================ Split Skip Op Handler ================
//   // split off a token (drop this char), and if skip<2 jump to Token Handler
//   1. set len_off=1 - current token ends before current char
//   1. set tok_off=1 - next token starts after current char
//   2. If tok empty jump to Next Token Handler (offset into Token Handler)
//   -- fall through to Token Handler
op_PARSE_SPLITA_SKIP: // split token on this char (so char is thrown out)
  len_off = 1; tok_off = 0;
len_check: //split before jumps here to confirm tok not empty
  if (str-tok == 1) goto tok_next; //NOTE: we already incremented str above
//// fall through
//
// ================ Token Handler ================
//   // call Token Handler callback and jump to error or fall through
//   1. call handler function on token we just split off
//   2. if handler ret != 0 jump to parser exit
//   -- fall through to Next Token Handler
tok_handle: //call handler on finished token
  if (0 != (ret = handler(tok, str-tok-len_off, state, next_state, arg))) goto parser_exit;
//// fall through
//
// ================ Next Token Handler ================
//   // inits next token and jumps to exit/error/loop
//   1. initialize next token to point to current char + tok_off
//   2. if next_state = fin_state jump to Parser Exit
//   ## if next_state invalid jump to Error Handler (only if if PARSER_SAFETY_CHECKS enabled)
//   3. jump to start of loop
tok_next: // if we are skipping handler (empty tok) jump to here
  //printf("tok_next: state=%d, char='%c', pclass=%d, op=%d, next_state=%d, tok_off=%d, len_off=%d\n",state,str[-1], pclass, op, next_state,tok_off,len_off);
  tok = str - tok_off; // next token starts after/before current char if tok_off is 0/1
  if (next_state == p->fin_state) goto parser_exit;
#ifdef PARSER_SAFETY_CHECKS
  else if (next_state >= p->nstates) goto parse_error;
#endif
  else goto begin_loop;

// ================ End of String ================
//   //reached end of string (and not in fin state)
//   1. if current tok empty or tail handler null, jump to parser exit
//   2. if tail handler null jump to error handler
//   2. call tail handler with rest of string
//   3. if tail handler ret !=0 jump to parser exit
//   4. update token to end of string (since we handled it)
//   -- fall through to parser exit
string_end:
  //printf("string_end: state=%d, char='%c', pclass=%d, op=%d, next_state=%d\n",state,str[-1], pclass, op, next_state);
  if (tok==str) goto parser_exit; //good exit, no more chars
  else if (tail_handler == NULL) goto parse_error; //trailing chars
  if ((0 != (ret = tail_handler(tok,str-tok,state,p->fin_state,arg_tail)))) goto parser_exit;
  tok=str; // since we handled tail, set tok to end of string
//// fall through
//
// ================  Parser Exit ================
//   // save parser state and return ret
//   1. save parser state (if pstate not null)
//   2. return ret
parser_exit: // save parser state and return
  //printf("parser_exit: state=%d, char='%c', pclass=%d, op=%d, next_state=%d\n",state,str[-1], pclass, op, next_state);
  if (pstate) {
    pstate->state = state;
    pstate->tok = tok;
    pstate->ptr = str;
    pstate->len = len;
  }
  return ret;
}
