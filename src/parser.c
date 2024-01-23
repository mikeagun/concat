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


//idea behind this parser is to generate an FSM that can tokenize the input with zero custom logic (other than the char classification function -- which could be a lookup table)
//we assign every input char to a parse class using a mapping function (so there are fewer cases the parser must deal with)
//every state is simply a set of entries for how to deal with each parse_class (so each state is an array of parse_entry_t)
//  - for now we also pack each state_entry into a single byte (5 bits for pstate - so up to 32 states, 3 bits for parse_op - 3 splitting options each for two handlers plus nosplit and error)
//for each handler there is:
//  - a target state (which state we should be in next)
//  - an operation to perform for this char (either collect it into the existing token, throw an error, or split the current token before/after this char, or split the current token and skip this char)
//
//TODO: add functions for saving/loading fsm (so at least in release mode, we just load the static FSM string rather than build the FSM)
//  - use ifdef on macro with FSM string to either load static FSM or build in code
//
//IDEAS:
//  - could add a list of parser FSMs (and maybe a state stack to support recursion), where instead of the second handler we have some parse_ops for navigating between FSMs
//  - direct threaded version of parser
//    - jump directly to next op (skip loop and switch statement)
//    - drop len, just use op to finish (skip len checks)
//    - 256-entry lookup array option for classifier (more RAM for tiny MCUs, but on a real machine skips function call and return value checks)
//    - have separate op handlers based on handlerA/handlerB being set (skip 1 check when handler set, only need to update tok when just validating)
//

const int _PSTATE_MASK_ = (1<<_PSTATE_BITS_)-1;

const char* parser_get_opname(enum parse_op op) {
  switch(op) {
    case PARSE_NOSPLIT: return "no split";
    case PARSE_SPLITA_BEFORE: return "split-before on handlerA";
    case PARSE_SPLITA_AFTER: return "split-after on handlerA";
    case PARSE_SPLITA_SKIP: return "split-skip on handlerA";
    case PARSE_SPLITB_BEFORE: return "split-before on handlerB";
    case PARSE_SPLITB_AFTER: return "split-after on handlerB";
    case PARSE_SPLITB_SKIP: return "split-skip on handlerB";
    case PARSE_ERR: return "parse error";
    default: return NULL;
  }
}

void parser_state_init(struct parser_state *pstate) {
  pstate->state = -1; //if state<0, everything else ignored
}

int parser_rules_init(struct parser_rules *p, int nstates, int nclasses, int init_state, int fin_state, int err_state, char_classifier_t *classifier/*, parse_handler_t *handlerA, parse_handler_t *handlerB*/) {
  p->v = malloc(sizeof(state_entry_t) * nstates * nclasses);
  if (!(p->v)) return -1;
  p->nstates=nstates;
  p->nclasses=nclasses;
  p->init_state=init_state;
  p->fin_state=fin_state;
  p->err_state=err_state;
  p->classify = classifier;
  //p->handlerA = handlerA;
  //p->handlerB = handlerB;
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

state_entry_t calc_state_entry(enum parse_op op, int target) {
  return (op<<_PSTATE_BITS_) | (target&_PSTATE_MASK_);
}

state_entry_t parser_get_entry(struct parser_rules *p, int state, int pclass) {
  return p->v[state*p->nclasses + pclass];
}

void parser_set_entry(struct parser_rules *p, int state, int pclass, state_entry_t e) {
  p->v[state*p->nclasses + pclass] = e;
}

void parser_set_op_target(struct parser_rules *p, int state, int pclass, enum parse_op op, int target) {
  p->v[state*p->nclasses + pclass] = calc_state_entry(op,target);
}

void parser_set_target(struct parser_rules *p, int state, int pclass, int target) {
  state_entry_t e = p->v[state*p->nclasses + pclass];
  e &= ~_PSTATE_MASK_; //clear target
  e |= target; //OR in target state
  p->v[state*p->nclasses + pclass] = e;
}

void parser_set_op(struct parser_rules *p, int state, int pclass, enum parse_op op) {
  state_entry_t e = p->v[state*p->nclasses + pclass];
  e &= _PSTATE_MASK_; //clear everything but target
  e |= op<<_PSTATE_BITS_; //OR in op
  p->v[state*p->nclasses + pclass] = e;
}

//void parser_set_all_entry(struct parser_rules *p, state_entry_t e) {
//  int i;
//  for(i=0; i<(p->nstates*p->nclasses); ++i) {
//    p->v[i] = e;
//  }
//}
void parser_set_all_op_target(struct parser_rules *p, enum parse_op op, int target) {
  int i;
  state_entry_t e = calc_state_entry(op,target);
  for(i=0; i<(p->nstates*p->nclasses); ++i) {
    p->v[i] = e;
  }
}

void parser_set_state_op_target(struct parser_rules *p, int state, enum parse_op op, int target) {
  int i;
  state_entry_t e = calc_state_entry(op,target);
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

void parser_set_global_op_target(struct parser_rules *p, int pclass, enum parse_op op, int target) {
  int i;
  state_entry_t e = calc_state_entry(op,target);
  for(i=0;i<p->nstates;++i) {
    parser_set_entry(p,i,pclass,e);
  }
}

void parser_set_global_target(struct parser_rules *p, int pclass, int target) {
  int i;
  for(i=0;i<p->nstates;++i) {
    parser_set_target(p,i,pclass,target);
  }
}

void parser_set_global_op(struct parser_rules *p, int pclass, enum parse_op op) {
  int i;
  for(i=0;i<p->nstates;++i) {
    parser_set_op(p,i,pclass,op);
  }
}

void parser_set_list_target(struct parser_rules *p, int state, int target, int nclasses, ...) {
  va_list ap;
  va_start(ap, nclasses);
  for(;nclasses;--nclasses) {
    parser_set_target(p,state,va_arg(ap,int),target);
  }
  va_end(ap);
}

void parser_list_set_target(struct parser_rules *p, int pclass, int target, int nstates, ...) {
  va_list ap;
  va_start(ap, nstates);
  for(;nstates;--nstates) {
    parser_set_target(p,va_arg(ap,int),pclass,target);
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

void parser_set_list_op_target(struct parser_rules *p, int state, int op, int target, int nclasses, ...) {
  va_list ap;
  va_start(ap, nclasses);
  for(;nclasses;--nclasses) {
    parser_set_op_target(p,state,va_arg(ap,int),op,target);
  }
  va_end(ap);
}

void parser_list_set_op_target(struct parser_rules *p, int pclass, int op, int target, int nstates, ...) {
  va_list ap;
  va_start(ap, nstates);
  for(;nstates;--nstates) {
    parser_set_op_target(p,va_arg(ap,int),pclass,op,target);
  }
  va_end(ap);
}

void parser_set_state_list_op_target(struct parser_rules *p, enum parse_op op, int target, int nstates, ...) {
  int i;
  state_entry_t e = calc_state_entry(op,target);
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

//validate a string using a parser ruleset
//  - tokenizing is ignored, and the parser simply determines whether we reach the FIN state or error out before then
//  - if (!state || *state <= 0), then state and iterator initialized to init state and str
//    - otherwise state and iterator initialized to *state and *ptr (so str is ignored)
// - len sets the max number of chars the parser will look at
//   - so long as string termination is handled in fsm for all possible values of str (e.g. null-terminated string with cases to handle '\0'), can just pass -1 as len
int parser_validate(struct parser_rules *p, const char *str, int len, int *state, const char **ptr) {
  int mystate = (state && *state <= 0) ? *state : p->init_state;
  const char *s = (state && *state <= 0) ? *ptr : str;
  int ret=-1;
  while(len--) {
    char ch=*s;
    const int pclass = p->classify(ch);
    if (pclass<0) { ret = pclass; goto parser_validate_return; }
    state_entry_t e = parser_get_entry(p,mystate,pclass);
    if (e<0) { ret = e; goto parser_validate_return; }
    const enum parse_op op = e >> _PSTATE_BITS_;
    const int target = e & _PSTATE_MASK_;

    //if (target == p->fin_state) {
      //ret = 0;
      //goto parser_validate_return;
    /*} else*/ if (target == p->err_state) {
      fprintf(stderr,"parse failure\n");
      goto parser_validate_return;
    } else {
      switch(op) {
        case PARSE_NOSPLIT: //don't split token (keep building)
        case PARSE_SPLITA_BEFORE: //split token before this char (so char becomes part of next tok)
        case PARSE_SPLITA_AFTER: //split token after this char (so char included in this token)
        case PARSE_SPLITA_SKIP: //split token on this char (so char is thrown out)
        case PARSE_SPLITB_BEFORE: //split token before this char (so char becomes part of next tok)
        case PARSE_SPLITB_AFTER: //split token after this char (so char included in this token)
        case PARSE_SPLITB_SKIP: //split token on this char (so char is thrown out)
          break;
        case PARSE_ERR: //parse error
        default:
          goto parser_validate_return;
      }
      if (target == p->fin_state) {
        ret = 0; //fin state
        goto parser_validate_return;
      } else if (target >= p->nstates) {
        ret = target; //invalid target state
        goto parser_validate_return;
      }
      mystate=target;
    }
    ++s;
  }
parser_validate_return:
  if (state) *state = mystate;
  if (ptr) *ptr = s;
  return ret;
}

//extern const char *rpn_get_classname(int pclass);
//extern const char *rpn_get_statename(int pstate);

//evaluate a string using a parser ruleset, calling handlerA/B as needed during the parse
// - any time the parser reaches a SPLIT rule, it calls one of the two handlers (A or B), except when both the previous and current op are SPLIT_SKIP ops
// - if there is any remaining text when len is reached, tail_handler is called with that
// - if a handler is NULL, the token is silently skipped over (this can be used as a way to "drop" tokens)
// - if (!pstate || pstate->state <= 0) then parser initialized to init state at start of string
//   - otherwise parser is initialized using pstate and starts from there (so str is ignored)
// - len sets the max number of chars the parser will look at
//   - so long as string termination is handled in fsm for all possible values of str (e.g. null-terminated string with cases to handle '\0'), can just pass -1 as len
int parser_eval(struct parser_rules *p, const char *str, int len, struct parser_state *pstate, parse_handler_t *handlerA, void *argA, parse_handler_t *handlerB, void *argB, parse_handler_t *tail_handler, void *arg_tail) {
  int state;
  const char *tok;
  const char *s;
  enum parse_op prev_op;

  if (pstate && !str) {
    state = pstate->state;
    tok = pstate->tok;
    s = pstate->ptr;
    len = pstate->len;
    prev_op = pstate->prev_op;
  } else {
    state = p->init_state;
    tok = str;
    s = str;
    prev_op = PARSE_SPLITA_SKIP; //if previous op was skip, and current op is skip, don't call handler. We initialize to skip in case first char is skip char
  }

  int ret = -1;
  while(len < 0 || len--) {
    char ch=*s;
    const int pclass = p->classify(ch);
    if (pclass<0) { ret = pclass; goto parser_eval_return; }
    state_entry_t e = parser_get_entry(p,state,pclass);
    if (e<0) { ret = e; goto parser_eval_return; }
    const enum parse_op op = e >> _PSTATE_BITS_;
    const int target = e & _PSTATE_MASK_;
    //printf("s:%d c:%d o:%d t:%d\n",state,pclass,op,target);
    //printf("s:'%s' c:'%s' o:'%s' t:'%s'\n",rpn_get_statename(state),rpn_get_classname(pclass),parser_get_opname(op),rpn_get_statename(target));
    //if (target == p->fin_state) {
      //ret = 0;
      //goto parser_eval_return;
    /*} else*/ if (target == p->err_state) {
      fprintf(stderr,"parse failure\n");
      goto parser_eval_return;
    } else {
      int hret=0; //TODO: use hret??? (or get rid of it)
      switch(op) {
        case PARSE_NOSPLIT: //don't split token (keep building)
          break;
        case PARSE_SPLITA_BEFORE: //split token before this char (so char becomes part of next tok)
          if (handlerA && prev_op!=PARSE_SPLITA_SKIP && prev_op != PARSE_SPLITB_SKIP) {
            hret=handlerA(tok,s-tok,state,target,argA);
          }
          tok=s;
          break;
        case PARSE_SPLITA_AFTER: //split token after this char (so char included in this token)
          if (handlerA) {
            hret=handlerA(tok,s+1-tok,state,target,argA);
          }
          tok=s+1;
          break;
        case PARSE_SPLITA_SKIP: //split token on this char (so char is thrown out)
          if (handlerA && prev_op!=PARSE_SPLITA_SKIP && prev_op != PARSE_SPLITB_SKIP) {
            hret=handlerA(tok,s-tok,state,target,argA);
          }
          tok=s+1;
          break;
        case PARSE_SPLITB_BEFORE: //split token before this char (so char becomes part of next tok)
          if (handlerB && prev_op!=PARSE_SPLITA_SKIP && prev_op != PARSE_SPLITB_SKIP) {
            hret=handlerB(tok,s-tok,state,target,argB);
          }
          tok=s;
          break;
        case PARSE_SPLITB_AFTER: //split token after this char (so char included in this token)
          if (handlerB) {
            hret=handlerB(tok,s+1-tok,state,target,argB);
          }
          tok=s+1;
          break;
        case PARSE_SPLITB_SKIP: //split token on this char (so char is thrown out)
          if (handlerB && prev_op!=PARSE_SPLITA_SKIP && prev_op != PARSE_SPLITB_SKIP) {
            hret=handlerB(tok,s-tok,state,target,argB);
          }
          tok=s+1;
          break;
        case PARSE_ERR: //parse error
        default:
          goto parser_eval_return;
      }
      if (target == p->fin_state) {
        ret = 0; //fin state
        goto parser_eval_return;
      } else if (target >= p->nstates) {
        ret = target; //invalid target state
        goto parser_eval_return;
      }
      state=target;
      prev_op = op;
    }
    ++s;
  }
  if (s != tok && tail_handler) {
    /*int hret = */tail_handler(tok,s-tok,state,p->fin_state,arg_tail);
    len -= s-tok;
    tok=s;
    ret = 0;
  } else if (s == tok) {
    ret = 0; //TODO: use different return code for end-of-string?
  }
parser_eval_return:
  if (pstate) {
    pstate->state = state;
    pstate->tok = tok;
    pstate->ptr = s;
    pstate->len = len;
    pstate->prev_op = prev_op;
  }
  return ret;
}
