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

#include "opcodes.h"
#include "helpers.h"

STATIC_ASSERT(N_BYTECODES<256,"number of basic bytecode ops doesn't fit into byte");

#define OP_STRING(op,opstr,effects) opstr
#define TYPE_STRING(op,opstr,effects) ("type_" opstr)
const char *opstrings[] =  { OPCODES(OP_STRING), TYPECODES(TYPE_STRING),"invalid" };
#undef TYPE_STRING
#undef OP_STRING

#define EFFECT_STRING(op,opstr,effects) effects
const char *op_effects[] =  { OPCODES(EFFECT_STRING), TYPECODES(EFFECT_STRING),"invalid" };
#undef EFFECT_STRING
