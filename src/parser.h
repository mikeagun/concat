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

#ifndef __PARSER_H__
#define __PARSER_H__
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

//FIXME: look for the other fixmes in parser.h/c to finish the refactor when we update vm_parser
// - changed a few function names, dropped some arguments, and removed handlerB

//parser.h - FSM parser

// Using Parser:
// 1. call parser_rules_init to init rules
// 2. use parser_set_* functions to define the rules
// 3. call parser_validate_rules on rules if not known good
//    - alternatively enable PARSER_SAFETY_CHECKS
// 4. call parser_validate to validate a string, and parser_eval to tokenize it

// Building Rules:
//
// call parser_set_* functions to configure the ruleset
//
// Suffix specifies what gets set (op and/or next state)
//   - set_*_op_next_state - set both op and next state of entry(s)
//   - set_*_op - set only op of entry(s)
//   - set_*_next_state - set only next state of entry(s)
// 
// Prefix specifies which entries are configured
// - set_all_* - set op/target/both for all entries
// - set_state_* - set op/target/both for state (for all pclasses)
// - set_global_* - set op/target/both for pclass (for all states)
//
// There are also a set of variadic functions to more easily configure lists of states/classes together
// - see variadic *_list_* set functions below
//

//parser safety checks doesn't assume that the fsm ruleset is well-formed
//  - i.e. with SAFETY_CHECKS the fsm checks for invalid pclass/state
//  - if you only use known-safe rules the checks can be skipped (to make the vm/loop even tighter)
//  - call parser_validate_rules on the parser rules before use to verify safety
//#define PARSER_SAFETY_CHECKS 1

// The general idea behind this parser is to generate a finite statemachine with the parsing rules, then eval the FSM for each char
// - short+fast FSM/VM to evaluate parsing rules
//   - general purpose (parsing is completely determined by ruleset, classifier, and supplied handlers)
//   - fsm ruleset can be hardcoded so it doesn't need to be generated at runtime (just a byte array)
// - fsm is just a lookup table that maps (state,pclass) to (op,newstate)
//   - every input char mapped to parse class (by supplied classifier)
//   - when op is a SPLIT op, splits off next token and calls tok handler
//
// Parser Ruleset
// - fsm rulset packed as a byte array of size nstates*nclasses
// - each state is a list of entries for dealing with each pclass
// - each entry contains op (what to do with this char) and next fsm state
// - entry packed into single byte (5 bits for pstate, 3 bits for parse_op)
//   - pstate - 5 bits - up to 32 states
//   - parse_op - 3 bits - index into 8 entry opcode table
//   - these are just current limits, state_entry_t could be swapped for larger
//
// - each fsm rule / vm instruction is 1 byte (max 32 states, 8 opcodes, only 5 currently used)
// - fsm state transitions looked up in ruleset byte array, which gives next state and opcode
//
// There is a separate function parser_validate to validate string without tokenizing
// - the validator runs a shorter/faster loop just to check if fsm reaches finish state


//TODO: add functions for saving/loading fsm (so at least in release mode we just load the static FSM string rather than build the FSM)
//TODO: better document how to build ruleset with examples (also refactor function names as appropriate)

//IDEAS:
// - list of FSMs and/or state stack to support more complicated rulesets and recursion
//   - use the spare ops for navigating between FSMs and the stack
// - 256-entry lookup array option for classifier
//   - more RAM for tiny MCUs, but reduces classifier to a single array lookup (no function call or conditionals)


// state_entry_t - fsm entry - fsm maps (state,class) -> entry, which contains (op,next_state)
// - entry is a single byte currently, so fsm ruleset takes (states*classes) bytes of memory
typedef unsigned char state_entry_t;

// parse_op - tells fsm what to do with a byte (based on pclass)
// - NOSPLIT             -> continue collecting bytes
// - SPLIT_BEFORE|AFTER  -> handler is called (split token before/after current)
// - ERR                 -> parse error (stops parsing)
// - TODO: decide what to do with remaining opcodes
enum parse_op {
  PARSE_NOSPLIT=0,     //don't split token (keep building)
//FIXME: remove A from SPLITA and update vm_parser to match (after parser refactor finished)
  PARSE_SPLITA_BEFORE,  //split token before this char (so char becomes part of next tok)
  PARSE_SPLITA_AFTER,   //split token after this char (so char included in this token)
  PARSE_SPLITA_SKIP,    //split token on this char (so char is thrown out)
  PARSE_NOOP_4,         // unused parse op -- figure out what to do with this
  PARSE_NOOP_5,         // unused parse op -- figure out what to do with this
  PARSE_NOOP_6,         // unused parse op -- figure out what to do with this
  PARSE_ERR             //parse error
};

//FIXME: remove SPLITB and update vm_parser to match (after parser refactor finished)
#define PARSE_SPLITB_AFTER (PARSE_SPLITA_AFTER)


// get parse op description (useful in debugging)
const char* parser_get_opname(enum parse_op op);

// fsm parse handler - the fsm calls a/b handler when it hits a SPLITA/B
typedef int (parse_handler_t)(const char *tok, int len, int state, int next_state, void* arg);

// fsm char classifier - this is used to map ascii char to pclass (to lookup next fsm state)
// - must return value in range 0 to nclasses-1
// FIXME: change return type to unsigned char and update vm_parser to match (after parser refactor finished)
typedef int (char_classifier_t)(char c);


//parser_rules describes a parser (including fsm, classifier, and handlers)
struct parser_rules {
  state_entry_t *v;
  int nstates;
  int nclasses;
  int init_state;
  int fin_state;
//FIXME: remove _err and update vm_parser to match (after parser refactor finished)
  int _err;
  char_classifier_t *classify;
  //parse_handler_t *handler;
};

//parser_state describes the state of a parsing job (to allow for continuations)
struct parser_state {
  int state;
  const char *tok;
  const char *ptr;
  int len;
  int skip;
};

// create empty parser
void parser_state_init(struct parser_state *pstate);

// init parser fsm rules with n states, n classes, init/fin/err states, and classifier
//FIXME: remove _err and update vm_parser to match (after parser refactor finished)
int parser_rules_init(struct parser_rules *p, int nstates, int nclasses, int init_state, int fin_state, int _err, char_classifier_t *classifier/*, parse_handler_t *handler*/);

// clear parser rules (NOTE: doesn't destroy)
void parser_rules_clear(struct parser_rules *p);

// destroy parser and release resources
void parser_rules_destroy(struct parser_rules *p);

// build entry from (op,next_state)
state_entry_t calc_state_entry(enum parse_op op, int next_state);

// get state,pclass entry
state_entry_t parser_get_entry(struct parser_rules *p, int state, int pclass);

// for state: set pclass -> entry
void parser_set_entry(struct parser_rules *p, int state, int pclass, state_entry_t e);

// for state: set pclass -> (op,target)
void parser_set_op_target(struct parser_rules *p, int state, int pclass, enum parse_op op, int next_state);

// for state: set pclass target
//FIXME: rename target -> next_state everywhere and update vm_parser to match (after parser refactor finished)
void parser_set_target(struct parser_rules *p, int state, int pclass, int next_state);

// for state: set pclass op
void parser_set_op(struct parser_rules *p, int state, int pclass, enum parse_op op);

//// set all states/classes -> entry
//void parser_set_all_entry(struct parser_rules *p, state_entry_t e);

// set all states/classes -> (op,target)
void parser_set_all_op_target(struct parser_rules *p, enum parse_op op, int next_state);

// for state: set all pclasses -> (op, target)
void parser_set_state_op_target(struct parser_rules *p, int state, enum parse_op op, int next_state);

//void parser_set_global_entry(struct parser_rules *p, int pclass, state_entry_t e);

// for all states: set pclass -> (op,target)
void parser_set_global_op_target(struct parser_rules *p, int pclass, enum parse_op op, int next_state);

// for all states: set pclass target
void parser_set_global_target(struct parser_rules *p, int pclass, int next_state);

// for all states: set pclass op
void parser_set_global_op(struct parser_rules *p, int pclass, enum parse_op op);

// for state: set target for classes...
void parser_set_list_target(struct parser_rules *p, int state, int next_state, int nclasses, ...);

// for states...: set pclass target
void parser_list_set_target(struct parser_rules *p, int pclass, int next_state, int nstates, ...);

// for state: set op for classes...
void parser_set_list_op(struct parser_rules *p, int state, int op, int nclasses, ...);

// for states...: set pclass op
void parser_list_set_op(struct parser_rules *p, int pclass, int op, int nstates, ...);

// for state: set (op,next_state) for classes...
void parser_set_list_op_target(struct parser_rules *p, int state, int op, int next_state, int nclasses, ...);

// for states...: set pclass -> (op,next_state)
void parser_list_set_op_target(struct parser_rules *p, int pclass, int op, int next_state, int nstates, ...);

// for states...: set all pclasses -> (op,next_state)
void parser_set_state_list_op_target(struct parser_rules *p, enum parse_op op, int next_state, int nstates, ...);

//validate a string using a parser ruleset
//  - tokenizing is ignored, and the parser simply determines whether we reach the FIN state or error out before then
//  - if (!state || *state <= 0), then state and iterator initialized to init state and str
//    - otherwise state and iterator initialized to *state and *ptr (so str is ignored)
// - len sets the max number of chars the parser will look at
//   - can just set arbitrarily high (e.g. -1) if FSM termination is guaranteed (e.g. null-terminated string)
int parser_validate(struct parser_rules *p, const char *str, int len, int *state, const char **ptr);

//extern const char *rpn_get_classname(int pclass);
//extern const char *rpn_get_statename(int pstate);

//evaluate a string using a parser ruleset, calling handler as needed during the parse
// - any time the parser reaches a SPLIT rule, it calls handler, skipping empty tokens
// - if (!pstate || pstate->state <= 0) then parser initialized to init state at start of string
//   - otherwise parser is initialized using pstate and starts from there (so str is ignored)
// - len sets the max number of chars the parser will look at
//   - can just set arbitrarily high (e.g. -1) if FSM termination is guaranteed (e.g. null-terminated string)
//FIXME: remove _h/_arg arguments and update vm_parser to match (after parser refactor finished)
int parser_eval(struct parser_rules *p, const char *str, int len, struct parser_state *pstate, parse_handler_t *handler, void *arg, parse_handler_t *_h, void *_arg, parse_handler_t *tail_handler, void *arg_tail);
#endif
