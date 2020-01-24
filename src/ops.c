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

#include "ops_internal.h"
#include "ops.h"
#include "val_include.h"
#include "val_printf.h"
#include "vm_unsafe.h"
#include "vm_err.h"
#include "val_bytecode_internal.h"
#include "val_coll.h"
#include "val_hash.h"

#include "ops_vm.h"
#include "ops_system.h"
#include "ops_bytecode.h"
#include "ops_printf.h"

#include <math.h>
#include <stdlib.h> //only needed for srandom and random


//This is where all the ops started, and where the majority of the ops remain

//op structure:
//- take args and reserve stack space (and wstack space) first
//- can validate immediately after or while processing arguments
//  - e.g. pop1, validate n, pop/get n
//  - e.g. pop3, validate off+len, take substring
//- goal is op handling/validation and stack space reservation go first, then the actual work (with minimal/no checks)
//  - later when we are are compiling, we can more easily split the unchecked work from the checks (wherever possible check at compile time and skip at eval time)
//  - maybe even separate bytecodes for checks/ops depending on how often the checks are skipped
//- in a couple places we still use convenience functions like vm_pushsw where it helps readability
//  - this also may translate well to bytecodes though if it is the tail of an op


//TODO:
//  - TODO: most of these should probably be moved out to their type-specific files
//    - just leave the core/random stuff here
//  - TODO: work stack combinator optimizations
//    - right now all ops are "pure" wrt work stack (they only ever pop themselves, and otherwise only push)
//      - for getting everything correct initially this made things cleaner, but I think now we can optimize this
//    - for many ops (e.g. each/map/times) we could reduce unecessary shuffles by hiding args below op inbetween iters
//      - just need to pop them all on done (when we popnext)
//      - then don't need to protect+wpush, then unprotect+push between each iteration
//    - does complicate ops inside quotations a bit (and need 2 versions), but earlier profiling showed trivial stack shuffling as our bottleneck
//      - some ops (e.g. while) already need 2 versions, so in those cases may just simplify things

//val_t _opval_eval;

err_t vm_init_ops(vm_t *vm) {
  err_t r=0;
  val_t t;

  //if (!r) r = val_func_init(&_opval_eval,_op_eval,"_eval");
  
  //vm ops
  if ((r = vm_init_op(vm,"quit",_op_quit))) goto out;
  if ((r = vm_init_op(vm,"nop",_op_nop))) goto out;

  if ((r = vm_init_op(vm,"qstate",_op_qstate))) goto out;
  if ((r = vm_init_op(vm,"vstate",_op_vstate))) goto out;

  //exception handling
  if ((r = vm_init_op(vm,"trycatch",_op_trycatch))) goto out;
  if ((r = vm_init_op(vm,"trydebug",_op_trydebug))) goto out;
  if ((r = vm_init_op(vm,"throw",_op_throw))) goto out;
  if ((r = vm_init_op(vm,"perror",_op_perror))) goto out;

  //type checking operators
  if ((r = vm_init_op(vm,"isnum",_op_isnum))) goto out;
  if ((r = vm_init_op(vm,"isint",_op_isint))) goto out;
  if ((r = vm_init_op(vm,"isfloat",_op_isfloat))) goto out;
  if ((r = vm_init_op(vm,"isstring",_op_isstring))) goto out;
  if ((r = vm_init_op(vm,"isident",_op_isident))) goto out;
  if ((r = vm_init_op(vm,"isescaped",_op_isescaped))) goto out;
  if ((r = vm_init_op(vm,"isnative",_op_isnative))) goto out;
  if ((r = vm_init_op(vm,"islist",_op_islist))) goto out;
  if ((r = vm_init_op(vm,"iscode",_op_iscode))) goto out;
  if ((r = vm_init_op(vm,"islisttype",_op_islisttype))) goto out;
  if ((r = vm_init_op(vm,"isfile",_op_isfile))) goto out;
  if ((r = vm_init_op(vm,"ispush",_op_ispush))) goto out;

  //printing ops
  if ((r = vm_init_op(vm,"list",_op_list))) goto out;
  if ((r = vm_init_op(vm,"listn",_op_listn))) goto out;
  if ((r = vm_init_opcode(vm,"print",_op_print,OP_print))) goto out;
  if ((r = vm_init_op(vm,"print_",_op_print_))) goto out;
  if ((r = vm_init_op(vm,"print_code",_op_print_code))) goto out;
  if ((r = vm_init_op(vm,"print_code_",_op_print_code_))) goto out;
  if ((r = vm_init_op(vm,"peek",_op_peek))) goto out;

  //math ops (some are overloaded, e.g. + is also string concatenation)
  if ((r = vm_init_opcode(vm,"+",_op_add,OP_add))) goto out;
  if ((r = vm_init_opcode(vm,"-",_op_sub,OP_sub))) goto out;
  if ((r = vm_init_op(vm,"*",_op_mul))) goto out;
  if ((r = vm_init_op(vm,"/",_op_div))) goto out;
  if ((r = vm_init_op(vm,"%",_op_mod))) goto out;
  if ((r = vm_init_op(vm,"^",_op_pow))) goto out;
  if ((r = vm_init_op(vm,"&",_op_bit_and))) goto out;
  if ((r = vm_init_op(vm,"|",_op_bit_or))) goto out;
  if ((r = vm_init_op(vm,"xor",_op_bit_xor))) goto out;
  if ((r = vm_init_op(vm,"~",_op_bit_not))) goto out;

  if ((r = vm_init_op(vm,"_",_op_negate))) goto out;


  //if (!r) r = vm_init_op(vm,"add",_op_add);
  //if (!r) r = vm_init_op(vm,"sub",_op_sub);
  //if (!r) r = vm_init_op(vm,"mul",_op_mul);
  //if (!r) r = vm_init_op(vm,"div",_op_div);
  //if (!r) r = vm_init_op(vm,"mod",_op_mod);
  //if (!r) r = vm_init_op(vm,"pow",_op_pow);
  //if (!r) r = vm_init_op(vm,"neg",_op_negate);
  
  if ((r = vm_init_opcode(vm,"inc",_op_inc,OP_inc))) goto out;
  if ((r = vm_init_opcode(vm,"dec",_op_dec,OP_dec))) goto out;
  if ((r = vm_init_op(vm,"sqrt",_op_sqrt))) goto out;
  if ((r = vm_init_op(vm,"log",_op_log))) goto out;
  if ((r = vm_init_op(vm,"abs",_op_abs))) goto out;

  if ((r = vm_init_op(vm,"sin",_op_sin))) goto out;
  if ((r = vm_init_op(vm,"cos",_op_cos))) goto out;
  if ((r = vm_init_op(vm,"tan",_op_tan))) goto out;
  if ((r = vm_init_op(vm,"asin",_op_sin))) goto out;
  if ((r = vm_init_op(vm,"acos",_op_cos))) goto out;
  if ((r = vm_init_op(vm,"atan",_op_tan))) goto out;

  if ((r = vm_init_op(vm,"atan2",_op_atan2))) goto out;

  if ((r = vm_init_op(vm,"rand",_op_rand))) goto out;
  if ((r = vm_init_op(vm,"randf",_op_randf))) goto out;
  if ((r = vm_init_op_(vm,"RAND_MAX",val_int_init(&t,RAND_MAX)))) goto out;

  if ((r = vm_init_op_(vm,"pi",val_float_init(&t,3.14159265358979323846)))) goto out;
  if ((r = vm_init_op_(vm,"e",val_float_init(&t,2.71828182845904523536)))) goto out;

  if ((r = vm_init_op(vm,"toint",_op_toint))) goto out;
  if ((r = vm_init_op(vm,"tofloat",_op_tofloat))) goto out;
  if ((r = vm_init_op(vm,"parsenum",_op_parsenum))) goto out;
  if ((r = vm_init_op(vm,"tryparsenum",_op_tryparsenum))) goto out;

  //general collection ops
  if ((r = vm_init_op(vm,"size",_op_size))) goto out;
  if ((r = vm_init_op(vm,"empty",_op_empty))) goto out;
  if ((r = vm_init_op(vm,"small",_op_small))) goto out;
  if ((r = vm_init_opcode(vm,"lpop",_op_lpop,OP_lpop))) goto out;
  if ((r = vm_init_opcode(vm,"lpush",_op_lpush,OP_lpush))) goto out;
  if ((r = vm_init_opcode(vm,"rpop",_op_rpop,OP_rpop))) goto out;
  if ((r = vm_init_opcode(vm,"rpush",_op_rpush,OP_rpush))) goto out;
  if ((r = vm_init_op(vm,"cat",_op_cat))) goto out;
  if ((r = vm_init_op(vm,"rappend",_op_rappend))) goto out;
  if ((r = vm_init_op(vm,"splitn",_op_splitn))) goto out;

  if ((r = vm_init_op(vm,"nth",_op_nth))) goto out;
  if ((r = vm_init_opcode(vm,"first",_op_first,OP_first))) goto out;
  if ((r = vm_init_op(vm,"rest",_op_rest))) goto out;
  if ((r = vm_init_op(vm,"second",_op_second))) goto out;
  if ((r = vm_init_op(vm,"third",_op_third))) goto out;
  if ((r = vm_init_compile_op(vm,"fourth","4 nth"))) goto out;
  if ((r = vm_init_compile_op(vm,"fifth","5 nth"))) goto out;
  if ((r = vm_init_compile_op(vm,"sixth","6 nth"))) goto out;
  if ((r = vm_init_compile_op(vm,"seventh","7 nth"))) goto out;
  if ((r = vm_init_compile_op(vm,"eigth","8 nth"))) goto out;
  if ((r = vm_init_compile_op(vm,"ninth","9 nth"))) goto out;
  if ((r = vm_init_compile_op(vm,"tenth","10 nth"))) goto out;
  if ((r = vm_init_op(vm,"last",_op_last))) goto out;

  if ((r = vm_init_op(vm,"dnth",_op_dnth))) goto out;
  if ((r = vm_init_op(vm,"dfirst",_op_dfirst))) goto out;
  if ((r = vm_init_op(vm,"dsecond",_op_dsecond))) goto out;
  if ((r = vm_init_op(vm,"dthird",_op_dthird))) goto out;
  if ((r = vm_init_compile_op(vm,"dfourth","4 dnth"))) goto out;
  if ((r = vm_init_compile_op(vm,"dfifth","5 dnth"))) goto out;
  if ((r = vm_init_compile_op(vm,"dsixth","6 dnth"))) goto out;
  if ((r = vm_init_compile_op(vm,"dseventh","7 dnth"))) goto out;
  if ((r = vm_init_compile_op(vm,"deigth","8 dnth"))) goto out;
  if ((r = vm_init_compile_op(vm,"dninth","9 dnth"))) goto out;
  if ((r = vm_init_compile_op(vm,"dtenth","10 dnth"))) goto out;
  if ((r = vm_init_op(vm,"dlast",_op_dlast))) goto out;


  //string ops
  if ((r = vm_init_op(vm,"tostring",_op_tostring))) goto out;
  if ((r = vm_init_op(vm,"toident",_op_toident))) goto out;
  if ((r = vm_init_op(vm,"substr",_op_substr))) goto out;
  if ((r = vm_init_op(vm,"trim",_op_trim))) goto out;
  if ((r = vm_init_op(vm,"split",_op_split))) goto out;
  if ((r = vm_init_op(vm,"split2",_op_split2))) goto out;
  if ((r = vm_init_op(vm,"join",_op_join))) goto out;
  if ((r = vm_init_op(vm,"join2",_op_join2))) goto out;

  if ((r = vm_init_op(vm,"strhash",_op_strhash))) goto out;
  if ((r = vm_init_op(vm,"getbyte",_op_getbyte))) goto out;
  if ((r = vm_init_op(vm,"setbyte",_op_setbyte))) goto out;

  if ((r = vm_init_op(vm,"find",_op_find))) goto out;
  if ((r = vm_init_op(vm,"rfind",_op_rfind))) goto out;
  if ((r = vm_init_op(vm,"firstof",_op_firstof))) goto out;
  if ((r = vm_init_op(vm,"lastof",_op_lastof))) goto out;
  if ((r = vm_init_op(vm,"firstnotof",_op_firstnotof))) goto out;
  if ((r = vm_init_op(vm,"lastnotof",_op_lastnotof))) goto out;


  //list operators
  if ((r = vm_init_op(vm,"expand",_op_expand))) goto out;
  if ((r = vm_init_op(vm,"clearlist",_op_clearlist))) goto out;

  if ((r = vm_init_op(vm,"tocode",_op_tocode))) goto out;
  if ((r = vm_init_op(vm,"tolist",_op_tolist))) goto out;
  if ((r = vm_init_op(vm,"parsecode",_op_parsecode))) goto out;
  if ((r = vm_init_op(vm,"parsecode_",_op_parsecode_))) goto out;

  if ((r = vm_init_op(vm,"setnth",_op_setnth))) goto out;
  if ((r = vm_init_op(vm,"swapnth",_op_swapnth))) goto out;
  if ((r = vm_init_op(vm,"rsetnth",_op_rsetnth))) goto out;
  if ((r = vm_init_op(vm,"mapnth",_op_mapnth))) goto out;

  if ((r = vm_init_op(vm,"sort",_op_sort))) goto out;
  if ((r = vm_init_op(vm,"rsort",_op_rsort))) goto out;
  //if (!r) r = vm_init_op(vm,"sort2",_op_sort2);

  //stack manipulators
  if ((r = vm_init_op(vm,"clear",_op_clear))) goto out;
  if ((r = vm_init_op(vm,"collapse",_op_collapse))) goto out;
  if ((r = vm_init_op(vm,"restore",_op_restore))) goto out;
  if ((r = vm_init_op(vm,"stacksize",_op_stacksize))) goto out;

  if ((r = vm_init_opcode(vm,"swap",_op_swap,OP_swap))) goto out;
  if ((r = vm_init_opcode(vm,"dup",_op_dup,OP_dup))) goto out;
  if ((r = vm_init_op(vm,"dup2",_op_dup2))) goto out;
  if ((r = vm_init_op(vm,"dup3",_op_dup3))) goto out;
  if ((r = vm_init_op(vm,"2dup",_op_2dup))) goto out;
  if ((r = vm_init_op(vm,"3dup",_op_3dup))) goto out;
  if ((r = vm_init_op(vm,"dupn",_op_dupn))) goto out;
  if ((r = vm_init_opcode(vm,"pop",_op_pop,OP_pop))) goto out;

  if ((r = vm_init_op(vm,"dig2",_op_dig2))) goto out;
  if ((r = vm_init_op(vm,"dig3",_op_dig3))) goto out;
  if ((r = vm_init_op(vm,"bury2",_op_bury2))) goto out;
  if ((r = vm_init_op(vm,"bury3",_op_bury3))) goto out;
  if ((r = vm_init_op(vm,"flip3",_op_flip3))) goto out;
  if ((r = vm_init_op(vm,"flip4",_op_flip4))) goto out;
  if ((r = vm_init_op(vm,"dign",_op_dign))) goto out;
  if ((r = vm_init_op(vm,"buryn",_op_buryn))) goto out;
  if ((r = vm_init_op(vm,"flipn",_op_flipn))) goto out;

  //combinators
  if ((r = vm_init_opcode(vm,"eval",_op_eval,OP_eval))) goto out;
  if ((r = vm_init_op_keep(vm,"evalr",_op_evalr))) goto out;
  if ((r = vm_init_op(vm,"quote",_op_quote))) goto out;
  if ((r = vm_init_op(vm,"wrap",_op_wrap))) goto out;
  if ((r = vm_init_op(vm,"wrap2",_op_wrap2))) goto out;
  if ((r = vm_init_op(vm,"wrap3",_op_wrap3))) goto out;
  if ((r = vm_init_op(vm,"wrapn",_op_wrapn))) goto out;
  if ((r = vm_init_op(vm,"protect",_op_protect))) goto out;
  if ((r = vm_init_op(vm,"dip",_op_dip))) goto out;
  if ((r = vm_init_op(vm,"dip2",_op_dip2))) goto out;
  if ((r = vm_init_op(vm,"dip3",_op_dip3))) goto out;
  if ((r = vm_init_op(vm,"dipn",_op_dipn))) goto out;
  if ((r = vm_init_op(vm,"dipe",_op_dipe))) goto out;
  if ((r = vm_init_op(vm,"sip",_op_sip))) goto out;
  if ((r = vm_init_op(vm,"sip2",_op_sip2))) goto out;
  if ((r = vm_init_op(vm,"sip3",_op_sip3))) goto out;
  if ((r = vm_init_op(vm,"sipn",_op_sipn))) goto out;
  if ((r = vm_init_op(vm,"0apply",_op_0apply))) goto out;
  if ((r = vm_init_op(vm,"apply",_op_1apply))) goto out;
  if ((r = vm_init_op(vm,"2apply",_op_2apply))) goto out;
  if ((r = vm_init_op(vm,"3apply",_op_3apply))) goto out;
  if ((r = vm_init_op(vm,"napply",_op_napply))) goto out;

  //conditional operators
  if ((r = vm_init_opcode(vm,"if",_op_if,OP_if))) goto out;
  if ((r = vm_init_opcode(vm,"ifelse",_op_ifelse,OP_ifelse))) goto out;
  if ((r = vm_init_opcode(vm,"if_",_op_if_,OP_if_))) goto out;
  if ((r = vm_init_opcode(vm,"ifelse_",_op_ifelse_,OP_ifelse_))) goto out;
  if ((r = vm_init_op(vm,"only",_op_only))) goto out;
  if ((r = vm_init_op(vm,"unless",_op_unless))) goto out;

  //loop operators
  if ((r = vm_init_op_keep(vm,"each",_op_each))) goto out;
  if ((r = vm_init_op_keep(vm,"eachr",_op_eachr))) goto out;
  if ((r = vm_init_op_keep(vm,"times",_op_times))) goto out;
  if ((r = vm_init_op(vm,"while",_op_while))) goto out;
  //if (!r) r = vm_init_op_keep(vm,"spread",_op_spread);

  if ((r = vm_init_compile_op(vm,"popd","swap pop"))) goto out;
  if ((r = vm_init_compile_op(vm,"dupd","\\dup dip"))) goto out;
  if ((r = vm_init_compile_op(vm,"swapd","\\swap dip"))) goto out;
  //if (!r) r = vm_init_compile_op(vm,"dipd","\\dip dip");
  //if (!r) r = vm_init_compile_op(vm,"evald","\\dip dip");
  //if (!r) r = vm_init_compile_op(vm,"ddup","dup dip");
  //if (!r) r = vm_init_compile_op(vm,"ddip","dup dip");
  //if (!r) r = vm_init_compile_op(vm,"deval","dup eval");

  if ((r = vm_init_compile_op(vm,"filter","dup2 clearlist flip3 [ flip3 dup3 flip3 dup dip3 [ dig2 \\rpush \\popd ifelse] dip ] each pop"))) goto out;
  if ((r = vm_init_compile_op(vm,"filter2","[ dup clearlist dup flip3 ] dip swap [ bury3 \\dup dip3 dup 4 dipn 4 dign [ \\rpush dip2 ] [ [ swapd rpush ] dip ] ifelse ] each pop"))) goto out;
  if ((r = vm_init_compile_op(vm,"map","dup2 clearlist flip3 [ bury2 dup dip2 \\rpush dip ] each pop"))) goto out;
  if ((r = vm_init_compile_op(vm,"mapr","dup2 clearlist flip3 [ bury2 dup dip2 \\lpush dip ] eachr pop"))) goto out;
  if ((r = vm_init_compile_op(vm,"mmap","dup2 clearlist flip3 [ bury2 dup dip2 \\rappend dip ] each pop"))) goto out;
  if ((r = vm_init_compile_op(vm,"cleave","dup clearlist swap [ swap [sip swap] dip rpush ] each swap pop"))) goto out;
  if ((r = vm_init_compile_op(vm,"spread","dup clearlist swap dup size swap [ dup2 inc dipn dup inc dign dig2 rpush swap dec ] each pop"))) goto out;
  if ((r = vm_init_compile_op(vm,"bi","[ dup2 \\eval dip ] dip eval"))) goto out;
  if ((r = vm_init_compile_op(vm,"tri","[ dup2 \\eval dip ] dip2 [ dup2 \\eval dip ] dip eval"))) goto out;
  
  if ((r = vm_init_compile_op(vm,"findp","0 1 [ [ inc dup3 empty [ pop 0 0 ] 1 ifelse ] 0 ifelse ] [ \\lpop dip2 dup2 dip3 dig3 not ] while popd popd dec"))) goto out;

  if ((r = vm_init_compile_op(vm,"linrec","dig2 [ [ 4 dupn 4 dipn 4 dign not [ inc dup3 4 dipn dup2 eval ] if ] 0 dup2 eval [ pop pop pop ] dip ] dip2 dip2 times"))) goto out;
  if ((r = vm_init_compile_op(vm,"binrec","[ 5 dupn 5 dipn 5 dign [ 4 dupn 5 dipn ] [ dup3 5 dipn 5 dign dup2 dip 5 buryn dup eval dup2 5 dipn ] ifelse ] dup eval pop pop pop pop pop"))) goto out;




 // if (!r) r = vm_init_op(vm,"true",_op_true);
 // if (!r) r = vm_init_op(vm,"false",_op_false);
 // if (!r) r = vm_init_op(vm,"not",_op_not);
 // if (!r) r = vm_init_op(vm,"and",_op_and);
 // if (!r) r = vm_init_op(vm,"or",_op_or);

 // if (!r) r = vm_init_op(vm,"toint",_op_toint);
 // if (!r) r = vm_init_op(vm,"tofloat",_op_tofloat);
 // if (!r) r = vm_init_op(vm,"tostring",_op_tostring);


 // //stack ops
 // if (!r) r = vm_init_op(vm,"top",_op_top);
 // //if (!r) r = vm_init_op(vm,".",_op_ppop); //forth
 // if (!r) r = vm_init_op(vm,"swap",_op_swap);
 // if (!r) r = vm_init_op(vm,"dup",_op_dup);
 // if (!r) r = vm_init_op(vm,"dup2",_op_dup2);
 // //if (!r) r = vm_init_op(vm,"over",_op_dup2); //forth
 // if (!r) r = vm_init_op(vm,"dup3",_op_dup3);
 // if (!r) r = vm_init_op(vm,"dupn",_op_dupn);
 // if (!r) r = vm_init_op(vm,"pop",_op_pop);
 // //if (!r) r = vm_init_op(vm,"drop",_op_pop); //forth
 // //if (!r) r = vm_init_op(vm,"del",_op_pop);
 // if (!r) r = vm_init_op(vm,"has",_op_has);
 // if (!r) r = vm_init_op(vm,"rlist",_op_rlist);
 // if (!r) r = vm_init_op(vm,"stacksize",_op_stacksize);
 // if (!r) r = vm_init_op(vm,"clear",_op_clear);

 // if (!r) r = vm_init_op(vm,"dign",_op_dign);
 // if (!r) r = vm_init_op(vm,"buryn",_op_buryn);
 // if (!r) r = vm_init_op(vm,"flipn",_op_flipn);
 // //if (!r) r = vm_init_op(vm,"dig1",_op_swap);
 // if (!r) r = vm_init_op(vm,"dig2",_op_dign);
 // if (!r) r = vm_init_op(vm,"dig3",_op_dign);
 // //if (!r) r = vm_init_op(vm,"bury1",_op_swap);
 // if (!r) r = vm_init_op(vm,"bury2",_op_bury2);
 // if (!r) r = vm_init_op(vm,"bury3",_op_bury3);
 // //if (!r) r = vm_init_op(vm,"flip2",_op_swap);
 // if (!r) r = vm_init_op(vm,"flip3",_op_flip3);

 // if (!r) r = vm_init_op(vm,"getdef",_op_getdef);
 // if (!r) r = vm_init_op(vm,"def",_op_def);

 // //list ops -- For now we handle these as special cases instead of ops (so they are still processed in delayed-expand code)
 // //if (!r) r = vm_init_op(vm,"[",_op_open_code);
 // //if (!r) r = vm_init_op(vm,"]",_op_close_code);
 // //if (!r) r = vm_init_op(vm,"(",_op_open_code);
 // //if (!r) r = vm_init_op(vm,")",_op_close_code);

 // //vm ops
 // if (!r) r = vm_init_op(vm,"quit",_op_quit);
 // 
 // if (!r) r = vm_init_op(vm,"break",_op_break);
 // if (!r) r = vm_init_op(vm,"breakif",_op_breakif);
 // if (!r) r = vm_init_op(vm,"eval",_op_eval);
 // if (!r) r = vm_init_op(vm,"rec",_op_rec);
 // if (!r) r = vm_init_op(vm,"recif",_op_recif);
 // if (!r) r = vm_init_op(vm,"loop",_op_loop);
 // if (!r) r = vm_init_op(vm,"loopif",_op_loopif);
 // if (!r) r = vm_init_op(vm,"if",_op_if);
 // if (!r) r = vm_init_op(vm,"ifelse",_op_ifelse);

 // if (!r) r = vm_init_op(vm,"include",_op_include);

 // if (!r) r = vm_init_op(vm,"quote",_op_quote);
 // if (!r) r = vm_init_op(vm,"cat",_op_cat);
 // if (!r) r = vm_init_op(vm,"cons",_op_cons);
 // if (!r) r = vm_init_op(vm,"dip",_op_dip);

 // //list ops
 // if (!r) r = vm_init_op(vm,"append",_op_append);
 // if (!r) r = vm_init_op(vm,"uncons",_op_uncons);
 // if (!r) r = vm_init_op(vm,"unappend",_op_unappend);

 // if (!r) r = vm_init_compile_op(vm,"swons","swap cons");
 // if (!r) r = vm_init_compile_op(vm,"swappend","swap append");

 // //if (!r) r = vm_init_compile_op(vm,"each","dup2 [[uncons]dip dup[dip]dip 1] 0 ifelse loopif pop pop"); //not working yet (nested groups issue)
 // char *t;

 // //some extra (maybe not needed) interconstructed combinators
 // if (!r) r = vm_evalraw(vm,t = _strdup("[ not if ] \\unless def")); free(t);
 // 

 // //swap      -- swap top two items   A B swap == B A
 // //dup       -- duplicate top item   A dup == A A
 // //zap (pop) -- delete top item      A B zap == A
 // //unit      -- quotes the top item  A unit == [A]
 // //cat       -- concatenate top two items on stack (i.e. concatenate two quotations) [A] [B] == [A B]
 // //cons      -- same as cat, but bottom item remains quoted                          [A] [B] == [[A] B]
 // //i         -- dequotes (i.e. executes) top stack item                              [A] i == A
 // //dip       -- executes top after removing top 2, then restores second item         A [B] == B A
 // //if (!r) r = vm_init_op(vm,"swap",_op_swap);
 // //if (!r) r = vm_init_op(vm,"dup",_op_dup);
 // //if (!r) r = vm_init_op(vm,"zap",_op_pop);
 // //if (!r) r = vm_init_op(vm,"unit",_op_quote);
 // //if (!r) r = vm_init_op(vm,"cat",_op_cat);
 // //if (!r) r = vm_init_op(vm,"cons",_op_cons);
 // //if (!r) r = vm_init_op(vm,"i",_op_eval);
 // //if (!r) r = vm_init_op(vm,"dip",_op_dip);
 // 


 // //some interconstructions
 

  if ((r = ops_comp_init(vm))) goto out;
  if ((r = ops_printf_init(vm))) goto out;
  if ((r = ops_file_init(vm))) goto out;
  if ((r = ops_hash_init(vm))) goto out;
  if ((r = ops_ref_init(vm))) goto out;
  if ((r = ops_debug_init(vm))) goto out;
  if ((r = ops_unsafe_init(vm))) goto out;
  if ((r = ops_vm_init(vm))) goto out;
  if ((r = ops_system_init(vm))) goto out;
  if ((r = ops_bytecode_init(vm))) goto out;
  
out:
  return r;
}

err_t vm_init_op_(vm_t *vm, const char *word, val_t *val) {
  fatal_if(ERR_FATAL,-1 == hash_put_kcopy(vm->scope,word,val,0));
  return 0;
}

err_t vm_init_op(vm_t *vm, const char *word, vm_op_handler *op) {
  val_t t;
  fatal_if(ERR_FATAL,-1 == hash_put_kcopy(vm->scope,word,val_func_init(&t,op,word),0));
  return 0;
}
err_t vm_init_opcode(vm_t *vm, const char *word, vm_op_handler *op, unsigned char opcode) {
  val_t t;
  fatal_if(ERR_FATAL,-1 == hash_put_kcopy(vm->scope,word,val_func_init_(&t,op,word,opcode),0));
  return 0;
}

err_t vm_init_op_keep(vm_t *vm, const char *word, vm_op_handler *op) {
  val_t t;
  fatal_if(ERR_FATAL,-1 == hash_put_kcopy(vm->scope,word,val_func_init_keep(&t,op,word),0));
  return 0;
}
err_t vm_init_opcode_keep(vm_t *vm, const char *word, vm_op_handler *op, unsigned char opcode) {
  val_t t;
  fatal_if(ERR_FATAL,-1 == hash_put_kcopy(vm->scope,word,val_func_init_keep_(&t,op,word,opcode),0));
  return 0;
}

err_t vm_init_compile_op(vm_t *vm, const char *word, const char *code) {
  val_t t;
  err_t r;
  val_code_init(&t);
  if ((r = vm_compile_code(vm,code,strlen(code),&t,OPT_NATIVEFUNC))) goto bad;
  throw_if_goto(ERR_DICT,bad,(-1 == hash_put_kcopy(vm->scope,word,&t,0)));

bad:
  val_destroy(&t);
  return r;
}



err_t _op_quit(vm_t *vm) {
  exit(0); //TODO: get exit code (or add second quit operator for exit code)
}

err_t _op_nop(vm_t *vm) {
  return 0;
}

err_t _op_qstate(vm_t *vm) {
  err_t r;
  if ((r = vm_qstate(vm))) return r;
  return 0;
}

err_t _op_vstate(vm_t *vm) {
  err_t r;
  if ((r = vm_vstate(vm))) return r;
  return 0;
}

err_t _op_trycatch(vm_t *vm) {
  return _vm_try(vm);
}

err_t _op_trydebug(vm_t *vm) {
  return _vm_trydebug(vm);
}


err_t _op_throw(vm_t *vm) {
  return vm_user_throw(vm);
}

err_t _op_perror(vm_t *vm) {
  return vm_perror(vm);
}


//type checking operators
err_t _op_isnum (vm_t *vm) { __op_get1; val_int_replace(p,val_isnumber(p)); return 0; }
err_t _op_isint (vm_t *vm) { __op_get1; val_int_replace(p,val_isint(p)); return 0; }
err_t _op_isfloat (vm_t *vm) { __op_get1; val_int_replace(p,val_isfloat(p)); return 0; }

err_t _op_isstring (vm_t *vm) { __op_get1; val_int_replace(p,val_isstring(p)); return 0; }
err_t _op_isident (vm_t *vm) { __op_get1; val_int_replace(p,val_isident(p)); return 0; }
err_t _op_isescaped (vm_t *vm) { __op_get1; val_int_replace(p,val_isident(p) && val_ident_escaped(p)); return 0; }

err_t _op_isnative (vm_t *vm) { __op_get1; val_int_replace(p,val_isfunc(p)); return 0; }

err_t _op_islist (vm_t *vm) { __op_get1; val_int_replace(p,val_islist(p)); return 0; }
err_t _op_iscode (vm_t *vm) { __op_get1; val_int_replace(p,val_iscode(p)); return 0; }
err_t _op_islisttype (vm_t *vm) { __op_get1; val_int_replace(p,val_islisttype(p)); return 0; }

err_t _op_isfile (vm_t *vm) { __op_get1; val_int_replace(p,val_isfile(p)); return 0; }

err_t _op_ispush (vm_t *vm) { __op_get1; val_int_replace(p,val_ispush(p)); return 0; }


err_t _op_list(vm_t *vm) { return vm_list(vm); }

err_t _op_listn(vm_t *vm) {
  __op_pop1;
  __op_take_iarg(p,n,n>=0 && vm_has(vm,n));
  return vm_listn(vm,n);
}

err_t _op_print(vm_t *vm) { __op_pop1; return val_printv(p); }

err_t _op_print_(vm_t *vm) { __op_pop1; return val_printv_(p); }

err_t _op_print_code(vm_t *vm) { __op_pop1; return val_printV(p); }

err_t _op_print_code_(vm_t *vm) { __op_pop1; return val_printV_(p); }

err_t _op_peek(vm_t *vm) { __op_get1; return val_peekV(p); }


//TODO: is there a cleaner/more efficient way to do these?
//  - right now since all the math ops follow one of a couple patterns, each of those patterns has a macro
//    - this makes the actual ops all one-liners
//  - maybe one higher-level function, and each of these macros can specialize that?
//leaves p+1 above stack on throw
#define __binmath_num_op(func) \
  __op_popget; \
  throw_if(ERR_BADARGS,!val_isnumber(p) || !val_isnumber(p+1)); \
  err_t e = func(p,p+1); \
  val_clear(p+1); \
  return e;

#define __binmath_float_op(func) \
  __op_popget; \
  throw_if(ERR_BADARGS,!val_isnumber(p) || !val_isnumber(p+1)); \
  val_float_init(p,func(val_num_float(p),val_num_float(p+1))); \
  val_clear(p+1); \
  return 0;

#define __binmath_int_binop(func) \
  __op_popget; \
  throw_if(ERR_BADARGS,!val_isint(p) || !val_isint(p+1)); \
  val_int_set(p,val_int(p) func val_int(p+1)); \
  val_clear(p+1); \
  return 0;

#define __umath_num_op(func) __op_get1; throw_if(ERR_BADARGS,!val_isnumber(p)); return func(p);
#define __umath_float_op(func) __op_get1; throw_if(ERR_BADARGS,!val_isnumber(p)); val_float_init(p,func(val_num_float(p))); return 0;
#define __umath_int_op(func) __op_get1; throw_if(ERR_BADARGS,!val_isint(p)); val_int_set(p,func val_int(p)); return 0;


err_t _op_add(vm_t *vm) { __binmath_num_op(val_num_add) }
err_t _op_sub(vm_t *vm) { __binmath_num_op(val_num_sub) }
err_t _op_mul(vm_t *vm) { __binmath_num_op(val_num_mul) }
err_t _op_div(vm_t *vm) { __binmath_num_op(val_num_div) }
err_t _op_mod(vm_t *vm) { __binmath_num_op(val_num_mod) }

err_t _op_pow(vm_t *vm) { __binmath_float_op(pow) }

err_t _op_bit_and(vm_t *vm) { __binmath_int_binop(&) }
err_t _op_bit_or(vm_t *vm) { __binmath_int_binop(|) }
err_t _op_bit_xor(vm_t *vm) { __binmath_int_binop(^) }

err_t _op_bit_not(vm_t *vm) { __umath_int_op(~) }

err_t _op_negate(vm_t *vm) { __umath_num_op(val_num_neg) }
err_t _op_inc(vm_t *vm) { __umath_num_op(val_num_inc) }
err_t _op_dec(vm_t *vm) { __umath_num_op(val_num_dec) }
err_t _op_sqrt(vm_t *vm) { __umath_num_op(val_num_sqrt) }
err_t _op_log(vm_t *vm) { __umath_num_op(val_num_log) }
err_t _op_abs(vm_t *vm) { __umath_num_op(val_num_abs) }

err_t _op_sin(vm_t *vm) { __umath_float_op(sin) }
err_t _op_cos(vm_t *vm) { __umath_float_op(cos) }
err_t _op_tan(vm_t *vm) { __umath_float_op(tan) }
err_t _op_asin(vm_t *vm) { __umath_float_op(asin) }
err_t _op_acos(vm_t *vm) { __umath_float_op(acos) }
err_t _op_atan(vm_t *vm) { __umath_float_op(atan) }

err_t _op_atan2(vm_t *vm) { __binmath_float_op(atan2) }

err_t _op_rand(vm_t *vm) {
  val_t t;
  return vm_push(vm,val_int_init(&t,random()));
}
err_t _op_randf(vm_t *vm) {
  val_t t;
  return vm_push(vm,val_float_init(&t,((valfloat_t)random())/RAND_MAX));
}
err_t _op_srand(vm_t *vm) {
  __op_pop1;
  throw_if(ERR_BADARGS,!val_isint(p));
  srandom(val_int(p));
  val_destroy(p);
  return 0;
}

err_t _op_toint(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_isnumber(p));
  return val_num_toint(p);
}
err_t _op_tofloat(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_isnumber(p));
  return val_num_tofloat(p);
}
err_t _op_parsenum(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_isstring(p));

  err_t e;
  val_t t;
  if((e = val_num_parse(&t,val_string_str(p),val_string_len(p)))) return e;
  val_swap(p,&t);
  val_destroy(&t);
  return 0;
}
err_t _op_tryparsenum(vm_t *vm) {
  __op_get1;
  if (val_isstring(p)) {
    val_t t;
    int r;
    if (!(r = _val_num_parse(&t,val_string_str(p),val_string_len(p)))) {
      val_swap(p,&t);
      val_destroy(&t);
    }
  }
  return 0;
}

err_t _op_size(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_iscoll(p));
  val_int_replace(p,val_coll_len(p));
  return 0;
}

err_t _op_empty(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_iscoll(p));
  val_int_replace(p,val_coll_empty(p));
  return 0;
}

err_t _op_small(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_iscoll(p));
  val_int_replace(p,val_coll_small(p));
  return 0;
}

err_t _op_lpop(vm_t *vm) {
  __op_getpush;
  throw_if(ERR_BADARGS,!val_iscoll(p)); //leaves INVALID on top of stack

  err_t e;
  if ((e = val_coll_lpop(p,p+1))) return e;
  val_swap(p,p+1); //swap coll back to top
  return 0;
}

err_t _op_lpush(vm_t *vm) {
  __op_popget;
  throw_if(ERR_BADARGS,!val_iscoll(p+1));

  err_t e = val_coll_lpush(p+1,p);
  val_swap(p,p+1);
  return e;
}

err_t _op_rpop(vm_t *vm) {
  __op_getpush;
  throw_if(ERR_BADARGS,!val_iscoll(p)); //leaves INVALID on top of stack

  err_t e;
  if ((e = val_coll_rpop(p,p+1))) return e;
  val_swap(p,p+1); //swap coll back to top
  return 0;
}

err_t _op_rpush(vm_t *vm) {
  __op_popget;
  throw_if(ERR_BADARGS,!val_iscoll(p+1));

  err_t e = val_coll_rpush(p+1,p);
  val_swap(p,p+1);
  return e;
}

err_t _op_cat(vm_t *vm) {
  __op_popget;
  throw_if(ERR_BADARGS,!val_iscoll(p) || (!val_isstringtype(p) && !val_islisttype(p+1))); //leaves p+1 above stack
  return val_coll_cat(p,p+1);
}

err_t _op_rappend(vm_t *vm) {
  __op_popget;
  throw_if(ERR_BADARGS,!val_iscoll(p+1)); //leaves p+1 above stack
  err_t e = val_coll_append(p+1,p);
  val_swap(p,p+1);
  return e;
}

err_t _op_splitn(vm_t *vm) {
  __op_get2;
  throw_if(ERR_BADARGS,!val_iscoll(p));
  __op_take_iarg(p+1,i,i>=0 && i<=val_coll_len(p));
  return val_coll_splitn(p,p+1,i);
}

err_t _op_nth(vm_t *vm) {
  __op_popget;
  throw_if(ERR_BADARGS,!val_iscoll(p));
  __op_take_iarg(p+1,n,n>=1 && n<=val_coll_len(p));
  return val_coll_ith(p,n-1);
}

err_t _op_first(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_iscoll(p) || val_coll_empty(p));
  val_t t;
  err_t e;
  if ((e = val_coll_lpop(p,&t))) return e;
  val_swap(p,&t);
  val_destroy(&t);
  return 0;
}

err_t _op_rest(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_iscoll(p) || val_coll_empty(p));

  return val_coll_lpop(p,NULL);
}

err_t _op_second(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_iscoll(p) || val_coll_len(p)<2);
  return val_coll_ith(p,1);
}

err_t _op_third(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_iscoll(p) || val_coll_len(p)<3);
  return val_coll_ith(p,2);
}

err_t _op_last(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_iscoll(p) || val_coll_empty(p));
  val_t t;
  err_t e;
  if ((e = val_coll_rpop(p,&t))) return e;
  val_swap(p,&t);
  val_destroy(&t);
  return 0;
}

err_t _op_dnth(vm_t *vm) {
  __op_get2;
  throw_if(ERR_BADARGS,!val_iscoll(p));
  __op_take_iarg(p+1,n,n>=1 && n<=val_coll_len(p));
  return val_coll_dith(p,n-1,p+1);
}

err_t _op_dfirst(vm_t *vm) {
  __op_getpush;
  throw_if(ERR_BADARGS,!val_iscoll(p) || val_coll_empty(p)); //leaves INVALID on top
  return val_coll_dith(p,0,p+1);
}

err_t _op_dsecond(vm_t *vm) {
  __op_getpush;
  throw_if(ERR_BADARGS,!val_iscoll(p) || val_coll_empty(p)); //leaves INVALID on top
  return val_coll_dith(p,1,p+1);
}

err_t _op_dthird(vm_t *vm) {
  __op_getpush;
  throw_if(ERR_BADARGS,!val_iscoll(p) || val_coll_empty(p)); //leaves INVALID on top
  return val_coll_dith(p,2,p+1);
}

err_t _op_dlast(vm_t *vm) {
  __op_getpush;
  throw_if(ERR_BADARGS,!val_iscoll(p) || val_coll_empty(p)); //leaves INVALID on top
  return val_coll_dith(p,val_coll_len(p)-1,p+1);
}


err_t _op_split(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_isstring(p));
  return val_string_split(p);
}

err_t _op_split2(vm_t *vm) {
  __op_popget;
  throw_if(ERR_BADARGS,!val_isstring(p) || !val_isstring(p+1));

  err_t e = val_string_split2_(p,val_string_str(p+1),val_string_len(p+1));
  val_destroy(p+1);
  return e;
}

err_t _op_join(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_islist(p));
  return val_string_join(p);
}

err_t _op_join2(vm_t *vm) {
  __op_popget;
  throw_if(ERR_BADARGS,!val_islist(p) || !val_isstring(p+1));

  err_t e = val_string_join2_(p,val_string_str(p+1),val_string_len(p+1));
  val_destroy(p+1);
  return e;
}

err_t _op_strhash(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_isstring(p));
  val_int_replace(p,val_string_hash32(p));
  return 0;
}

err_t _op_getbyte(vm_t *vm) {
  __op_get2;
  throw_if(ERR_BADARGS,!val_isstring(p) || !val_isint(p+1) || val_int(p+1)<0 || val_int(p+1)>=val_string_len(p));
  val_int_replace(p+1,val_string_str(p)[val_int(p+1)]);
  return 0;
}

err_t _op_setbyte(vm_t *vm) {
  __op_pop2get;
  throw_if(ERR_BADARGS,!val_isstring(p) || !val_isint(p+1));
  __op_take_iarg(p+2,i,i>=0 && i<val_string_len(p));
  char v = (char)val_int(p+1);
  val_clear(p+1);

  return val_string_seti(p,i,v);
}

err_t _op_find(vm_t *vm) {
  __op_popget;
  throw_if(ERR_BADARGS,!val_isstring(p) || !val_isstring(p+1));

  err_t e = val_string_find_(p,val_string_str(p+1),val_string_len(p+1));
  val_destroy(p+1);
  return e;
}

err_t _op_rfind(vm_t *vm) {
  __op_popget;
  throw_if(ERR_BADARGS,!val_isstring(p) || !val_isstring(p+1));

  err_t e = val_string_rfind_(p,val_string_str(p+1),val_string_len(p+1));
  val_destroy(p+1);
  return e;
}

err_t _op_firstof(vm_t *vm) {
  __op_popget;
  throw_if(ERR_BADARGS,!val_isstring(p) || !val_isstring(p+1));

  err_t e = val_string_firstof_(p,val_string_str(p+1),val_string_len(p+1));
  val_destroy(p+1);
  return e;
}

err_t _op_lastof(vm_t *vm) {
  __op_popget;
  throw_if(ERR_BADARGS,!val_isstring(p) || !val_isstring(p+1));

  err_t e = val_string_lastof_(p,val_string_str(p+1),val_string_len(p+1));
  val_destroy(p+1);
  return e;
}

err_t _op_firstnotof(vm_t *vm) {
  __op_popget;
  throw_if(ERR_BADARGS,!val_isstring(p) || !val_isstring(p+1));

  err_t e = val_string_firstnotof_(p,val_string_str(p+1),val_string_len(p+1));
  val_destroy(p+1);
  return e;
}

err_t _op_lastnotof(vm_t *vm) {
  __op_popget;
  throw_if(ERR_BADARGS,!val_isstring(p) || !val_isstring(p+1));

  err_t e = val_string_lastnotof_(p,val_string_str(p+1),val_string_len(p+1));
  val_destroy(p+1);
  return e;
}

err_t _op_tostring(vm_t *vm) {
  __op_get1;
  return val_to_string(p);
}

err_t _op_toident(vm_t *vm) {
  __op_get1;
  err_t e = val_to_string(p);
  if (!e) p->type = TYPE_IDENT;
  return e;
}


err_t _op_substr(vm_t *vm) {
  __op_pop2get;
  throw_if(ERR_BADARGS,!val_isstring(p) || !val_isint(p+2));
  __op_take_iarg(p+1,off,off>=0 && off<=val_string_len(p));
  valint_t len = val_int(p+2);
  val_clear(p+2);

  return val_string_substring(p,off,len);
}

err_t _op_trim(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_isstring(p));
  val_string_trim(p);
  return 0;
}

err_t _op_expand(vm_t *vm) {
  __op_pop1;
  throw_if(ERR_BADARGS,!val_islisttype(p));
  val_t list;
  val_move(&list,p);
  return val_stack_pusheach(vm->open_list,&list); //TODO: verify list destroyed in all cases
}

err_t _op_clearlist(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_iscoll(p));
  val_coll_clear(p);
  return 0;
}

err_t _op_tocode(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_islisttype(p));
  p->type = TYPE_CODE;
  return 0;
}

err_t _op_tolist(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_islisttype(p));
  p->type = TYPE_LIST;
  return 0;
}

err_t _op_parsecode(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_isstring(p));

  err_t e;
  val_t t;
  val_code_init(&t);
  if ((e = vm_compile_code(vm,val_string_str(p),val_string_len(p),&t,OPT_NONE))) goto out;
  val_swap(p,&t);
out:
  val_destroy(&t);
  return e;
}

err_t _op_parsecode_(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_isstring(p));

  err_t e;
  val_t t;
  val_code_init(&t);
  if ((e = vm_compile_input(vm,val_string_str(p),val_string_len(p),&t))) goto out;
  val_swap(p,&t);
out:
  val_destroy(&t);
  return 0;
}


err_t _op_setnth(vm_t *vm) {
  __op_pop2get;
  throw_if(ERR_BADARGS,!val_islisttype(p));
  __op_take_iarg(p+2,n,n>=1 && n<=val_list_len(p));

  return val_list_seti(p,n-1,p+1);
}

err_t _op_swapnth(vm_t *vm) {
  __op_popget2;
  throw_if(ERR_BADARGS,!val_islisttype(p));
  __op_take_iarg(p+2,n,n>=1 && n<=val_list_len(p));

  return val_list_swapi(p,n-1,p+1);
}

err_t _op_rsetnth(vm_t *vm) {
  __op_pop2get;
  throw_if(ERR_BADARGS,!val_islisttype(p+1));
  __op_take_iarg(p+2,n,n>=1 && n<=val_list_len(p+1));

  err_t e = val_list_seti(p+1,n-1,p);
  val_swap(p,p+1);
  return e;
}

err_t _op_mapnth(vm_t *vm) { //(list) [map] n -- nth_el <|> map (list) n rsetnth
  __op_pop2get; //(list) [map] n
  __op_wpush4;
  throw_if(ERR_BADARGS,!val_islisttype(p) || !val_isint(p+2) || val_int(p+2)<0 || val_int(p+2)>val_list_len(p));

  err_t e;
  valint_t n = val_int(p+2);
  val_null_init(q+2); //init q+2 with NULL (to swap into list)
  if ((e = val_list_swapi(p,n-1,q+2))) return e; //swap el out of list
  if ((e = val_wprotect(p))) return e; //protect list for the work stack
  val_swap(q+2,p); //swap list with el (so el on stack, list in work)
  val_move(q+1,p+2); //move n over to work stack for rsetnth
  val_func_init(q,_op_rsetnth,"rsetnth"); //to put el back in list at end
  val_move(q+3,p+1); //mapping val to top of wstack
  return 0;
}

err_t _op_sort(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_islisttype(p));
  return val_list_qsort(p);
}

err_t _op_rsort(vm_t *vm) {
  __op_get1;
  throw_if(ERR_BADARGS,!val_islisttype(p));
  return val_list_rqsort(p);
}


err_t _op_clear(vm_t *vm) {
  val_stack_clear(vm->open_list);
  return 0;
}

err_t _op_collapse(vm_t *vm) {
  return val_list_wrap(vm->open_list);
}

err_t _op_restore(vm_t *vm) {
  __op_pop1;
  throw_if(ERR_BADARGS,!val_islist(p));
  val_t list;
  val_move(&list,p);
  return val_list_lcat(vm->open_list,&list); //TODO: verify list destroyed
}

err_t _op_stacksize(vm_t *vm) {
  val_t t;
  return vm_push(vm,val_int_init(&t,val_stack_len(vm->open_list)));
}


err_t _op_swap(vm_t *vm) {
  __op_get2;
  val_swap(p,p+1);
  return 0;
}
err_t _op_dup(vm_t *vm) {
  __op_getpush;
  return val_clone(p+1,p);
}
err_t _op_dup2(vm_t *vm) {
  throw_if(ERR_MISSINGARGS,!vm_has(vm,2));
  __op_push1;

  return val_clone(p,p-2);
}
err_t _op_dup3(vm_t *vm) {
  throw_if(ERR_MISSINGARGS,!vm_has(vm,3));
  __op_push1;

  return val_clone(p,p-3);
}
err_t _op_2dup(vm_t *vm) {
  throw_if(ERR_MISSINGARGS,!vm_has(vm,2));
  __op_push2;

  err_t e;
  if ((e = val_clone(p,p-2))) return e;
  return val_clone(p+1,p-1);
}
err_t _op_3dup(vm_t *vm) {
  throw_if(ERR_MISSINGARGS,!vm_has(vm,2));
  __op_push3;

  err_t e;
  if ((e = val_clone(p,p-3))) return e;
  if ((e = val_clone(p+1,p-2))) return e;
  return val_clone(p+2,p-1);
}
err_t _op_dupn(vm_t *vm) {
  __op_get1;
  __op_take_iarg(p,n,n>=1 && vm_has(vm,n));
  return val_clone(p,p-n);
}
err_t _op_pop(vm_t *vm) {
  __op_pop1;
  val_destroy(p);
  return 0;
}
err_t _op_2pop(vm_t *vm) {
  __op_pop2;
  val_destroy(p);
  val_destroy(p+1);
  return 0;
}
err_t _op_3pop(vm_t *vm) {
  __op_pop3;
  val_destroy(p);
  val_destroy(p+1);
  val_destroy(p+2);
  return 0;
}

err_t _op_dig2(vm_t *vm) {
  __op_get3;       // A B C
  val_swap(p,p+2); // C B A
  val_swap(p,p+1); // B C A
  return 0;
}
err_t _op_dig3(vm_t *vm) {
  return val_stack_dign(vm->open_list,3);
}
err_t _op_bury2(vm_t *vm) {
  __op_get3;         // A B C
  val_swap(p,p+2);   // C B A
  val_swap(p+1,p+2); // C A B
  return 0;
}
err_t _op_bury3(vm_t *vm) {
  return val_stack_buryn(vm->open_list,3);
}
err_t _op_flip3(vm_t *vm) {
  __op_get3;       // A B C
  val_swap(p,p+2); // C B A
  return 0;
}

err_t _op_flip4(vm_t *vm) {
  __op_get3;         // A B C D
  val_swap(p,p+3);   // D B C A
  val_swap(p+1,p+2); // D C B A
  return 0;
}

err_t _op_dign(vm_t *vm) {
  __op_pop1;
  __op_take_iarg(p,n,n>=1 && vm_has(vm,n+1));
  return val_stack_dign(vm->open_list,n);
}
err_t _op_buryn(vm_t *vm) {
  __op_pop1;
  __op_take_iarg(p,n,n>=1 && vm_has(vm,n+1));
  return val_stack_buryn(vm->open_list,n);
}
err_t _op_flipn(vm_t *vm) {
  __op_pop1;
  __op_take_iarg(p,n,n>=1 && vm_has(vm,n));
  return val_stack_flipn(vm->open_list,n);
}

err_t _op_eval(vm_t *vm) {
  __op_pop1;
  return vm_wpush(vm,p);
}
err_t _op_evalr(vm_t *vm) { // keep evaling until we have a simple val -- one way to implement looping/recursion
  __op_get1;
  if (!val_ispush(p)) return vm_pushsw(vm); //push cond to work stack (so we will eval and then test again)
  else return _vm_popnext(vm,NULL); //else we are done, so pop evalr
}
err_t _op_quote(vm_t *vm) {
  __op_get1;
  return val_code_wrap(p);
}
err_t _op_wrap(vm_t *vm) {
  __op_get1;
  return val_list_wrap(p);
}
err_t _op_wrap2(vm_t *vm) {
  __op_popget;
  err_t e;
  val_t l;
  if ((e = val_list_wrap2(&l,p,p+1))) return e;
  val_move(p,&l);
  return 0;
}
err_t _op_wrap3(vm_t *vm) {
  __op_pop2get;
  err_t e;
  val_t l;
  if ((e = val_list_wrap_arr(&l,p,3))) return e;
  val_move(p,&l);
  return 0;
}
err_t _op_wrapn(vm_t *vm) {
  __op_pop1;
  __op_take_iarg(p,n,n>=1 && vm_has(vm,n));

  err_t e;
  val_t t;
  if ((e = vm_popngetn_(vm,n-1,1,&p))) return e; //take n-1, get the last one (to replace with list)
  if ((e = val_list_wrap_arr(&t,p,n))) return e; //wrap p to p+n-1 in list
  val_move(p,&t);
  return 0;
}
err_t _op_protect(vm_t *vm) {
  __op_get1;
  return val_protect(p);
}
err_t _op_dip(vm_t *vm) {
  __op_pop2;
  __op_wpush2;
  err_t e;
  if ((e = val_wprotect(p))) return e;
  val_move(q,p);
  val_move(q+1,p+1);
  return 0;
}

err_t _op_dip2(vm_t *vm) { // A B [C] -- C A B
  __op_pop3;
  __op_wpush3;
  err_t e;
  if ((e = val_wprotect(p))) return e;
  if ((e = val_wprotect(p+1))) return e;
  val_move(q,p+1);
  val_move(q+1,p);
  val_move(q+2,p+2);
  return 0;
}

err_t _op_dip3(vm_t *vm) { // A B C [D] -- D A B C
  __op_pop(4);
  __op_wpush3;

  err_t e;
  if ((e = vm_wpush(vm,val_func_init(q,_op_expand,"expand")))) return e;
  if ((e = val_list_wrap_arr(q+1,p,3))) return e; //wrap p,p+1,p+2 in list to expand after
  val_move(q+2,p+3);
  return 0;
}

err_t _op_dipn(vm_t *vm) {
  __op_pop2;
  __op_take_iarg(p+1,n,n>=0 && vm_has(vm,n));
  __op_wpush3;

  val_func_init(q,_op_expand,"expand");
  val_move(q+2,p);
  err_t e;
  if ((e = vm_popn_(vm,n,&p))) return e; //verified above
  if ((e = val_list_wrap_arr(q+1,p,n))) return e; //wrap p to p+n-1 in list on work stack
  return 0;
}

err_t _op_dipe(vm_t *vm) {
  __op_pop2;
  __op_wpush2;
  val_move(q+1,p+1);
  val_move(q,p);
  return 0;
}

err_t _op_sip(vm_t *vm) {
  __op_popget;
  __op_wpush2;

  err_t e;
  if ((e = val_clone(q,p))) return e;
  if ((e = val_wprotect(q))) return e;
  val_move(q+1,p+1);
  return 0;
}

err_t _op_sip2(vm_t *vm) {
  __op_popget2;
  __op_wpush3;

  err_t e;
  if ((e = val_clone(q,p))) return e;
  if ((e = val_clone(q+1,p+1))) return e;
  if ((e = val_wprotect(q))) return e;
  if ((e = val_wprotect(q+1))) return e;
  val_move(q+2,p+2);
  return 0;
}

err_t _op_sip3(vm_t *vm) {
  __op_popngetn(1,3);
  __op_wpush3;

  err_t e;
  if ((e = vm_wpush(vm,val_func_init(q,_op_expand,"expand")))) return e;
  if ((e = val_list_clone_wrap_arr(q+1,p,3))) return e; //wrap p,p+1,p+2 in list to expand after
  val_move(q+2,p+3);
  return 0;
}

err_t _op_sipn(vm_t *vm) {
  __op_pop2;
  __op_take_iarg(p+1,n,n>=0 && vm_has(vm,n));
  __op_wpush3;
  err_t e;
  if ((e = vm_getn_(vm,n+1,&p))) return e;
  val_func_init(q,_op_expand,"expand");
  if ((e = val_list_clone_wrap_arr(q+1,p,n))) return e; //wrap p to p+n-1 in list on work stack
  val_move(q+2,p+n);
  return 0;
}

err_t _op_0apply(vm_t *vm) {
  __op_pop1;
  __op_wpush3;

  val_func_init(q,_op_restore,"restore");
  val_move(q+2,p);
  err_t e;
  unsigned int size = vm_stacksize(vm);
  if ((e = val_list_lsplitn(vm->open_list,q+1,size))) return e;
  return 0;
}
err_t _op_1apply(vm_t *vm) { //FIXME: this is broken
  throw_if(ERR_MISSINGARGS,!vm_has(vm,2));
  __op_pop1;
  __op_wpush3;

  val_func_init(q,_op_restore,"restore");
  val_move(q+2,p);
  err_t e;
  unsigned int size = vm_stacksize(vm);
  if ((e = val_list_lsplitn(vm->open_list,q+1,size-1))) return e;
  return 0;
}
err_t _op_2apply(vm_t *vm) { //FIXME: this is broken
  throw_if(ERR_MISSINGARGS,!vm_has(vm,3));
  __op_pop1;
  __op_wpush3;

  val_func_init(q,_op_restore,"restore");
  val_move(q+2,p);
  err_t e;
  unsigned int size = vm_stacksize(vm);
  if ((e = val_list_lsplitn(vm->open_list,q+1,size-2))) return e;
  return 0;
}
err_t _op_3apply(vm_t *vm) {
  throw_if(ERR_MISSINGARGS,!vm_has(vm,4));
  __op_pop1;
  __op_wpush3;

  val_func_init(q,_op_restore,"restore");
  val_move(q+2,p);
  err_t e;
  unsigned int size = vm_stacksize(vm);
  if ((e = val_list_lsplitn(vm->open_list,q+1,size-3))) return e;
  return 0;
}

err_t _op_napply(vm_t *vm) {
  __op_pop2;
  __op_take_iarg(p+1,n,n>=0 && vm_has(vm,n));
  __op_wpush3;

  val_func_init(q,_op_restore,"restore");
  val_move(q+2,p);
  err_t e;
  unsigned int size = vm_stacksize(vm);
  if ((e = val_list_lsplitn(vm->open_list,q+1,size-n))) return e;
  return 0;
}

//conditional operators
err_t _op_if(vm_t *vm) { //version of if that evals its condition if needed
  __op_pop2; // [cond] [then]
  if (val_ispush(p)) {
    int c = val_as_bool(p);
    val_destroy(p);
    if (c) {
      return vm_wpush(vm,p+1);
    } else {
      val_destroy(p+1);
    }
  } else { //else wpush cond for eval
    __op_wpush3;
    err_t e;
    val_func_init(q,_op_if_,"if_"); //check again with if_ after eval of cond
    if ((e = val_wprotect(p+1))) return e;
    val_move(q+1,p+1); //hold body
    val_move(q+2,p); //eval cond
  }
  return 0;
}

err_t _op_if_(vm_t *vm) { //version of if that doesn't eval its condition (just cast to bool)
  __op_pop2; // cond [then]
  int c = val_as_bool(p);
  val_destroy(p);
  if (c) {
    return vm_wpush(vm,p+1);
  } else {
    val_destroy(p+1);
    return 0;
  }
}

err_t _op_ifelse(vm_t *vm) {
  __op_pop3; // [cond] [then] [else]
  if (val_ispush(p)) { //simple cond -- wpush 1 and destroy the other
    __op_wpush1;
    int c = val_as_bool(p);
    val_destroy(p);
    if (c) {
      val_destroy(p+2);
      val_move(q,p+1);
    } else {
      val_destroy(p+1);
      val_move(q,p+2);
    }
    return 0;
  } else { //else wpush cond for eval
    __op_wpush4;
    err_t e;
    val_func_init(q,_op_ifelse_,"ifelse_"); //check again with ifelse_ after eval of cond
    if ((e = val_wprotect(p+1))) return e; //protect then for hold
    if ((e = val_wprotect(p+2))) return e; //protect else for hold
    val_move(q+2,p+1); //hold then
    val_move(q+1,p+2); //hold else
    val_move(q+3,p); //eval cond
  }
  return 0;
}

err_t _op_ifelse_(vm_t *vm) {
  __op_pop3;
  __op_wpush1;
  int c = val_as_bool(p);
  val_destroy(p);
  if (c) {
    val_destroy(p+2);
    val_move(q,p+1);
  } else {
    val_destroy(p+1);
    val_move(q,p+2);
  }
  return 0;
}

err_t _op_only(vm_t *vm) {
  __op_popget; // cond [then]
  if (val_as_bool(p)) { //need to pop cond and eval [then]
    val_destroy(p);
    val_move(p,p+1);
    return vm_pushsw(vm);
  } else { //just destroy [then]
    val_destroy(p+1);
    return 0;
  }
}

err_t _op_unless(vm_t *vm) {
  __op_popget; // cond [else]
  if (!val_as_bool(p)) { //need to pop cond and eval [else]
    val_destroy(p);
    val_move(p,p+1);
    return vm_pushsw(vm);
  } else { //just destroy [else]
    val_destroy(p+1);
    return 0;
  }
}

//loop operators
err_t _op_each(vm_t *vm) {
  __op_popget; // (list) [body]
  throw_if(ERR_BADARGS,!val_iscoll(p));
  unsigned int n = val_coll_len(p);
  if (n == 0) { //empty list -- pop everything (and end loop)
    vm_pop1_(vm,&p); //don't need to check since we already got it with popget
    val_destroy(p);
    val_destroy(p+1);
    return _vm_popnext(vm,NULL); //drop loop
  } else if (n == 1) { //last iter -- just unwrap list and wpush work (this shortcut check is optional)
    _vm_popnext(vm,NULL); //we don't check since we are just destroying
    __op_wpush1;
    val_move(q,p+1);
    return val_coll_unwrap(p);
  } else { //multiple els -- set up next iter
    __op_wpush3;
    err_t e;
    val_move(q+2,p+1); //move body to top of w
    if ((e = val_clone(q,q+2))) return e; //clone body for next iter
    if ((e = val_wprotect(q))) return e; //protect body for next iter
    if ((e = val_coll_lpop(p,q+1))) return e; //pop next el to work on
    if ((e = val_wprotect(p))) return e; //protect list for next iter
    val_swap(p,q+1); //now el on stack, list on wstack
    return 0;
  }
}

err_t _op_eachr(vm_t *vm) {
  __op_popget; // (list) [body]
  throw_if(ERR_BADARGS,!val_iscoll(p));
  unsigned int n = val_coll_len(p);
  if (n == 0) { //empty list -- pop everything (and end loop)
    vm_pop1_(vm,&p); //don't need to check since we already got it with popget
    val_destroy(p);
    val_destroy(p+1);
    return _vm_popnext(vm,NULL); //drop loop
  } else if (n == 1) { //(optional shortcut) last iter -- just unwrap list and wpush work (this shortcut check is optional)
    _vm_popnext(vm,NULL); //we don't check since we are just destroying
    __op_wpush1;
    val_move(q,p+1);
    return val_coll_unwrap(p);
  } else { //multiple els -- set up next iter
    __op_wpush3;

    err_t e;
    val_move(q+2,p+1); //move body to top of w
    if ((e = val_clone(q,q+2))) return e; //clone body for next iter
    if ((e = val_wprotect(q))) return e; //protect body for next iter
    if ((e = val_coll_rpop(p,q+1))) return e; //pop next el to work on
    if ((e = val_wprotect(p))) return e; //protect list for next iter
    val_swap(p,q+1); //now el on stack, list on wstack
    return 0;
  }
}

err_t _op_times(vm_t *vm) {
  __op_pop2; // count [body]
  throw_if(ERR_BADARGS,!val_isint(p));
  valint_t count = val_int(p);
  if (count>0) { //need to loop
    __op_wpush3;

    err_t e;
    val_int_dec_(p);
    val_move(q+1,p); //hold count-1
    val_move(q+2,p+1); //move body to top of w
    if ((e = val_clone(q,q+2))) return e; //clone body for next iter
    if ((e = val_wprotect(q))) return e; //protect body for next iter
  } else { //done, pop everything
    _vm_popnext(vm,NULL); //we don't check since we are just destroying
    val_destroy(p);
    val_destroy(p+1);
  }
  return 0;
}

err_t _op_while(vm_t *vm) {
  __op_pop2; // [cond] [body]
  if (val_ispush(p)) { //cond is simple val, either no-op or infinite loop
    int c = val_as_bool(p);
    val_destroy(p);
    if (c) { //infinite loop
      __op_wpush2;
      val_func_init_keep(q,_op_loop_,"loop_");
      val_move(q+1,p+1); //we just set up loop_ (iteration will actually start after first call to loop_)
    } else { //no-op
      val_destroy(p+1);
    }
    return 0;
  } else { //need to set up loop
    __op_wpush4;

    err_t e;
    val_func_init_keep(q,_op_while_,"while_");
    val_move(q+3,p); //cond to eval now before first iter
    val_move(q+1,p+1); //body (for if cond passes)
    if ((e = val_clone(q+2,q+3))) return e; //clone cond for next iter
    if ((e = val_wprotect(q+2))) return e; //protect cond for next iter
    if ((e = val_wprotect(q+1))) return e; //protect body for while_
    return 0;
  }
}

err_t _op_loop_(vm_t *vm) { //infinite loop
  __op_pop1; // [body]
  __op_wpush2;
  val_move(q+1,p);
  err_t e;
  if ((e = val_clone(q,q+1))) return e;
  if ((e = val_wprotect(q))) return e; //clone+protect [body] for next iter
  return 0;
}

err_t _op_while_(vm_t *vm) {
  __op_pop3; // cond [cond] [body]
  int c = val_as_bool(p);
  val_destroy(p);
  if (c) { //cond TRUE, eval body and re-check
    __op_wpush4;
    val_move(q+3,p+2); //body to eval
    val_move(q+2,p+1); //cond to eval for next

    err_t e;
    if ((e = val_clone(q+1,q+2))) return e; //cond for next
    if ((e = val_clone(q,q+3))) return e; //body for next
    if ((e = val_wprotect(q+1))) return e; //protect cond for next
    if ((e = val_wprotect(q))) return e; //protect body for next
  } else { //cond FALSE, done with loop
    val_destroy(p+1);
    val_destroy(p+2);
    _vm_popnext(vm,NULL);
  }
  return 0;
}

err_t _op_linrec(vm_t *vm) {
  return _throw(ERR_NOT_IMPLEMENTED);
  //#| [cond] [base] [rec1] [rec2] 
  //      3      2      1      0
  val_t *cond;
  //throw_if(ERR_MISSINGARGS, !(cond = vm_get(vm,3)));
  err_t r=-1;
  if (!val_ispush(cond)) { //if it is code then we need to setup _op_linrec_() so we can eval first
    //|# 
    //r = vm_deref(vm); if (r) return r; //so we can modify things (i.e. replace while with while_)
    //val_t *op = _vm_getnext(vm);
    //if (op->type != TYPE_NATIVE_FUNC || val_func_f(op) != _op_while) return _throw(BADARGS); //sanity check (since this should ALWAYS be true when _op_while is called)
    //val_destroy(op); //not necc. but if we implement value tracing needed to avoid leaks
    //val_func_init_keep(op,_op_while_,"while_"); //now next time we get to the while loop we will run _op_while_ instead

    //r = vm_hold1(vm); //hold body
    //val_t c;
    //if (!r) r = val_clone(&c,vm_top(vm));
    //if (!r) r = vm_hold1(vm); //hold cond
    //if (!r) r = vm_wpush(vm,&c); //wpush cloned cond for evaluation
  } else {
    if (val_as_bool(cond)) { //just eval then
      //val_t body;
      //r = val_clone(&body,vm_top(vm));
      //if (!r) r = vm_hold1(vm); //dup body (top) onto work stack
      //if (!r) r = vm_pushsw(vm); //push cond over
      //if (!r) r = vm_wpush(vm,&body); //push body on top of work stack
      //return r;
    } else { //else pop everything
      //r = _vm_popnext(vm,NULL); //pop while
      //if (!r) r = vm_pop(vm,NULL); //pop body
      //if (!r) r = vm_pop(vm,NULL); //pop cond
      //return r;
    }
  }
  return r;
}

//err_t _op_?(vm_t *vm) { //spread without producing output list
//  val_t *funcs = vm_top(vm);
//  if (!funcs || !val_islisttype(funcs)) return _throw(BADARGS);
//  unsigned int len = val_list_len(funcs);
//  if (len==0) { //empty list so we are done
//    vm_pop(vm,NULL); //pop list
//    vm_popnext(vm,NULL); //pop spread
//    return 0;
//  } else if (len==1) { //last iteration
//    vm_popnext(vm,NULL); //pop spread
//    err_t r = val_list_unwrap(funcs);
//    if (!r) r = vm_pushsw(vm);
//    return r;
//  } else {
//    val_t w;
//    err_t r = val_list_lpop(funcs,&w);
//    if (!r) r = vm_holdn(vm,len);
//    if (!r) r = vm_wpush(vm,&w);
//    return r;
//  }
//}

//TODO: native implementation
//err_t _op_spread(vm_t *vm) {
//  val_t *funcs = vm_top(vm);
//  if (!funcs || !val_islisttype(funcs)) return _throw(BADARGS);
//  unsigned int len = val_list_len(funcs);
//  if (len==0) { //empty list so we are done
//    vm_pop(vm,NULL); //pop list
//    vm_popnext(vm,NULL); //pop spread
//    return 0;
//  //} else if (len==1) { //last iteration
//  //  vm_popnext(vm,NULL); //pop spread
//  //  err_t r = val_list_unwrap(funcs);
//  //  if (!r) r = vm_pushsw(vm);
//  //  return r;
//  } else {
//    val_t w;
//    err_t r = val_list_lpop(funcs,&w);
//    if (!r) r = vm_holdn(vm,len);
//    if (!r) r = vm_wpush(vm,&w);
//    return r;
//  }
//}
//
//err_t _op_spread_(vm_t *vm) {
//}

