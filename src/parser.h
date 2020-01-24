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

#ifndef __PARSER_H__
#define __PARSER_H__
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

//This is some code hacked over an old FSM-based language parser I wrote for another project
//  - TODO: code-review, should we replace, refactor, or keep???

//idea behind this parser is to generate a parser FSM, and then just run the FSM for each char, outputing tokens as it goes
//we assign every input char to a parse class using a mapping function (so there are fewer FSM cases to deal with)
//every state is simply a set of entries for how to deal with each parse_class (so each state is an array of parse_entry_t)
//  - for now we also pack each state_entry into a single byte (5 bits for pstate - so up to 32 states, 3 bits for parse_op - 3 splitting options each for two handlers plus nosplit and error)
//for each class there is:
//  - a target state (which state we should be in next)
//  - an operation to perform for this char (either collect it into the existing token, throw an error, or split the current token before/after this char, or split the current token and skip this char)
//
//IDEAS:
//  - could add a list of parser FSMs (and maybe a state stack to support recursion), where instead of the second handler we have some parse_ops for navigating between FSMs

typedef unsigned char state_entry_t;
#define _PSTATE_BITS_ (5)
const int _PSTATE_MASK_;

enum parse_op { //TODO: figure out how to support sub-fsms
  PARSE_NOSPLIT=0,     //don't split token (keep building)
  PARSE_SPLITA_BEFORE,  //split token before this char (so char becomes part of next tok)
  PARSE_SPLITA_AFTER,   //split token after this char (so char included in this token)
  PARSE_SPLITA_SKIP,    //split token on this char (so char is thrown out)
  PARSE_SPLITB_BEFORE,  //split token before this char (so char becomes part of next tok)
  PARSE_SPLITB_AFTER,   //split token after this char (so char included in this token)
  PARSE_SPLITB_SKIP,    //split token on this char (so char is thrown out)
  PARSE_ERR             //parse error
};

const char* parser_get_opname(enum parse_op op);

typedef int (parse_handler_t)(const char *tok, int len, int state, int target, void* arg);
typedef int (char_classifier_t)(char c);

//parser_rules describes a parser (including fsm, classifier, and handlers)
struct parser_rules {
  state_entry_t *v;
  int nstates;
  int nclasses;
  int init_state;
  int fin_state;
  int err_state;
  char_classifier_t *classify;
  //parse_handler_t *handlerA;
  //parse_handler_t *handlerB;
};

//parser_state describes the state of a parsing job (to allow for continuations)
struct parser_state {
  int state;
  const char *tok;
  const char *ptr;
  int len;
  enum parse_op prev_op;
};

void parser_state_init(struct parser_state *pstate);

int parser_rules_init(struct parser_rules *p, int nstates, int nclasses, int init_state, int fin_state, int err_state, char_classifier_t *classifier/*, parse_handler_t *handlerA, parse_handler_t *handlerB*/);

void parser_rules_clear(struct parser_rules *p);

void parser_rules_destroy(struct parser_rules *p);

state_entry_t calc_state_entry(enum parse_op op, int target);

state_entry_t parser_get_entry(struct parser_rules *p, int state, int pclass);

void parser_set_entry(struct parser_rules *p, int state, int pclass, state_entry_t e);

void parser_set_op_target(struct parser_rules *p, int state, int pclass, enum parse_op op, int target);

void parser_set_target(struct parser_rules *p, int state, int pclass, int target);

void parser_set_op(struct parser_rules *p, int state, int pclass, enum parse_op op);

//void parser_set_all_entry(struct parser_rules *p, state_entry_t e);
void parser_set_all_op_target(struct parser_rules *p, enum parse_op op, int target);

void parser_set_state_op_target(struct parser_rules *p, int state, enum parse_op op, int target);

//void parser_set_global_entry(struct parser_rules *p, int pclass, state_entry_t e);

void parser_set_global_op_target(struct parser_rules *p, int pclass, enum parse_op op, int target);

void parser_set_global_target(struct parser_rules *p, int pclass, int target);

void parser_set_global_op(struct parser_rules *p, int pclass, enum parse_op op);

void parser_set_list_target(struct parser_rules *p, int state, int target, int nclasses, ...);

void parser_list_set_target(struct parser_rules *p, int pclass, int target, int nstates, ...);

void parser_set_list_op(struct parser_rules *p, int state, int op, int nclasses, ...);

void parser_list_set_op(struct parser_rules *p, int pclass, int op, int nstates, ...);

void parser_set_list_op_target(struct parser_rules *p, int state, int op, int target, int nclasses, ...);

void parser_list_set_op_target(struct parser_rules *p, int pclass, int op, int target, int nstates, ...);

void parser_set_state_list_op_target(struct parser_rules *p, enum parse_op op, int target, int nstates, ...);

//validate a string using a parser ruleset
//  - tokenizing is ignored, and the parser simply determines whether we reach the FIN state or error out before then
//  - if (!state || *state <= 0), then state and iterator initialized to init state and str
//    - otherwise state and iterator initialized to *state and *ptr (so str is ignored)
// - len sets the max number of chars the parser will look at
//   - so long as string termination is handled in fsm for all possible values of str (e.g. null-terminated string with cases to handle '\0'), can just pass -1 as len
int parser_validate(struct parser_rules *p, const char *str, int len, int *state, const char **ptr);

//extern const char *rpn_get_classname(int pclass);
//extern const char *rpn_get_statename(int pstate);

//evaluate a string using a parser ruleset, calling handlerA/B as needed during the parse
// - any time the parser reaches a SPLIT rule, it calls one of the two handlers (A or B), except when both the previous and current op are SPLIT_SKIP ops
// - if a handler is NULL, the token is silently skipped over (this can be used as a way to "drop" tokens)
// - if (!pstate || pstate->state <= 0) then parser initialized to init state at start of string
//   - otherwise parser is initialized using pstate and starts from there (so str is ignored)
// - len sets the max number of chars the parser will look at
//   - so long as string termination is handled in fsm for all possible values of str (e.g. null-terminated string with cases to handle '\0'), can just pass -1 as len
int parser_eval(struct parser_rules *p, const char *str, int len, struct parser_state *pstate, parse_handler_t *handlerA, void *argA, parse_handler_t *handlerB, void *argB, parse_handler_t *tail_handler, void *arg_tail);
#endif
