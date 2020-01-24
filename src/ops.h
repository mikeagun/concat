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

#ifndef __OPS_H__
#define __OPS_H__ 1

#include "vmstate.h"
#include "ops_file.h"
#include "ops_ref.h"
#include "ops_comp.h"
#include "ops_hash.h"
#include "vm_debug.h"


err_t vm_init_ops(vm_t *vm);

//TODO: solution for lazy compilation of function definitions
//  - so rarely used builtins and library functions can load faster
//  - maybe split into sections, where all words in each section point to the section init native which then inits that section on first eval

err_t vm_init_op_(vm_t *vm, const char *tok, val_t *val);
err_t vm_init_op(vm_t *vm, const char *tok, vm_op_handler *op);
err_t vm_init_opcode(vm_t *vm, const char *tok, vm_op_handler *op, unsigned char opcode);
err_t vm_init_op_keep(vm_t *vm, const char *tok, vm_op_handler *op);
err_t vm_init_opcode_keep(vm_t *vm, const char *tok, vm_op_handler *op, unsigned char opcode);
err_t vm_init_compile_op(vm_t *vm, const char *tok, const char *code);


//vm operators
vm_op_handler _op_quit;    // quit ( -- )
vm_op_handler _op_nop;     // nop ( -- )
vm_op_handler _op_qstate;  // qstate ( -- ) prints 1-line vm state (top couple of stack and wstack, printed compactly)
vm_op_handler _op_vstate;  // vstate ( -- ) prints 1-line vm state (complete stack and wstack)

//exception handling
vm_op_handler _op_trycatch; // trycatch ( [try][catch] trycatch -- try ) OR ( [try][catch] trycatch -- err catch )
vm_op_handler _op_trydebug; // trydebug ( [try] trydebug -- try ) evals try, starting a debugger and at the point where an exception occurs if one does
vm_op_handler _op_throw;    // throw    ( errno -- ) print top of stack as exception (if it is integer, tries to interpret as standard error code)
vm_op_handler _op_perror;   // perror   ( err -- EXCEPTION )throw top of stack as exception

//type checking operators
vm_op_handler _op_isnum;      //isnum ( A -- B )
vm_op_handler _op_isint;      //isnum ( A -- B )
vm_op_handler _op_isfloat;    //isnum ( A -- B )
vm_op_handler _op_isstring;   //isstring ( A -- B )
vm_op_handler _op_isident;    //isident ( A -- B )
vm_op_handler _op_isescaped;  //isescaped ( A -- B )
vm_op_handler _op_isnative;   //isnative ( A -- B )
vm_op_handler _op_islist;     //isident ( A -- B )
vm_op_handler _op_iscode;     //iscode ( A -- B )
vm_op_handler _op_islisttype; //islisttype ( A -- B )
vm_op_handler _op_isfile;     //isfile ( A -- B )
vm_op_handler _op_ispush;     //ispush ( A -- B ) (whether val is simply pushed when eval)

//printing operators
vm_op_handler _op_list;       // list ( -- )
vm_op_handler _op_listn;      // listn ( n -- )
vm_op_handler _op_print;      // print ( A -- ) prints like printf %v
vm_op_handler _op_peek;       // peek ( A -- A ) prints top without popping
vm_op_handler _op_print_;     // print_ ( A -- ) skips newline
vm_op_handler _op_print_code; // print_code ( A -- ) prints concat code where possible (like printf %V)
vm_op_handler _op_print_code_; // print_code ( A -- ) prints concat code (without trailing newline)

//math operators
//- binary math ops
vm_op_handler _op_add;     // rot ( A B -- A+B )
vm_op_handler _op_sub;     // sub ( A B -- A-B )
vm_op_handler _op_mul;     // mul ( A B -- A*B )
vm_op_handler _op_div;     // div ( A B -- A/B )
vm_op_handler _op_mod;     // mod ( A B -- A mod B )
vm_op_handler _op_pow;     // pow ( A B -- A^B )
//- bitwise math ops
vm_op_handler _op_bit_and; // mod ( A B -- A & B )
vm_op_handler _op_bit_or;  // mod ( A B -- A | B )
vm_op_handler _op_bit_xor; // mod ( A B -- A xor B )
vm_op_handler _op_bit_not; // mod ( A B -- ~A )
//- unary math ops
vm_op_handler _op_negate;  // neg ( A -- -A )
vm_op_handler _op_inc;     // inc ( A -- A+1 )
vm_op_handler _op_dec;     // dec ( A -- A-1 )
vm_op_handler _op_sqrt;    // sqrt ( A -- sqrt(A) )
vm_op_handler _op_log;     // log ( A -- log(A) )
vm_op_handler _op_abs;     // abs ( A -- abs(A) )
vm_op_handler _op_sin;     // sin ( A -- sin(A) )
vm_op_handler _op_cos;     // cos ( A -- cos(A) )
vm_op_handler _op_tan;     // tan ( A -- tan(A) )
vm_op_handler _op_asin;    // asin ( A -- asin(A) )
vm_op_handler _op_acos;    // acos ( A -- acos(A) )
vm_op_handler _op_atan;    // atan ( A -- atan(A) )
vm_op_handler _op_atan2;   // atan2 ( y x -- atan2(y,x) )
//- other math ops
vm_op_handler _op_rand;    // rand ( -- rand_int )
vm_op_handler _op_randf;   // randf ( -- rand_float )
vm_op_handler _op_srand;   // srand ( A -- )
//- type conversion
vm_op_handler _op_toint;   // toint ( A -- int(A) )
vm_op_handler _op_tofloat; // tofloat ( A -- float(A) )
vm_op_handler _op_parsenum; // parsenum ( A -- num(A) )
vm_op_handler _op_tryparsenum; // tryparsenum ( A -- num(A) ); ("x" -- "x")

//general collection operators (string/list/code)
vm_op_handler _op_size;   //size   ( "ABCDE" -- 5 ); ( [A B C] -- 3)
vm_op_handler _op_empty;  //empty  ( "" -- 1 ); ( "A" -- 0 ); ( [ ] -- 1 ); ( [A B C] -- 0)
vm_op_handler _op_small;  //small  ( "" -- 1 ); ( "A" -- 1 ); ( "AB" -- 0 );
vm_op_handler _op_lpop;      //lpop    ( [A B] -- A [B] )
vm_op_handler _op_lpush;     //lpush   ( A [B] -- [A B] )
//vm_op_handler _op_cons;      //cons    ( A [B] -- [A B] ); ( A B -- [A B] ); ( [A] [B] -- [A B] ); (A) [B] -- [(A) B] ??? something like this to prepend code, or make code if needed and then prepend
vm_op_handler _op_rpop;      //rpop    ( [A B] -- B [A] )
vm_op_handler _op_rpush;     //rpush   ( A [B] -- [B A] )
vm_op_handler _op_cat;    //cat    ( "AB" "CDE" -- "ABCDE" ); ( [A] [B] -- [A B] )
vm_op_handler _op_rappend;   //rappend ( [A] [B] -- [B A] ); ( A [B] -- [B A] )
vm_op_handler _op_splitn; //splitn ( "ABCDE" 2 -- "AB" "CDE" )

vm_op_handler _op_nth;       //nth      ( [A B] 1 -- A ); ( [A B] 2 -- B )
vm_op_handler _op_first;     // first   ( (A B C D E F G H I J) -- A )
vm_op_handler _op_rest;      // rest    ( (A B C D E F G H I J) -- (B C D E F G H I J) )
vm_op_handler _op_last;      // last    ( (A B C D E F G H I J) -- J )
vm_op_handler _op_second;    // second  ( (A B C D E F G H I J) -- B )
vm_op_handler _op_third;     // third   ( (A B C D E F G H I J) -- C )
//vm_op_handler _op_fourth;    // fourth  ( (A B C D E F G H I J) -- D )
//vm_op_handler _op_fifth;     // fifth   ( (A B C D E F G H I J) -- E )
//vm_op_handler _op_sixth;     // sixth   ( (A B C D E F G H I J) -- F )
//vm_op_handler _op_seventh;   // seventh ( (A B C D E F G H I J) -- G )
//vm_op_handler _op_eighth;    // eight   ( (A B C D E F G H I J) -- H )
//vm_op_handler _op_ninth;     // ninth   ( (A B C D E F G H I J) -- I )
//vm_op_handler _op_tenth;     // tenth   ( (A B C D E F G H I J) -- J )

vm_op_handler _op_dnth;      //dnth     ( [A B] 1 -- [A B] A ); ( [A B] 2 -- [A B] B )
vm_op_handler _op_dfirst;    //dfirst   ( (A B C D E F G H I J) -- (A B C D E F G H I J) A )
vm_op_handler _op_dlast;     //dlast    ( (A B C D E F G H I J) -- (A B C D E F G H I J) J )
vm_op_handler _op_dsecond;   //dsecond  ( (A B C D E F G H I J) -- (A B C D E F G H I J) B )
vm_op_handler _op_dthird;    //dthird   ( (A B C D E F G H I J) -- (A B C D E F G H I J) C )
//vm_op_handler _op_dfourth;   //dfourth  ( (A B C D E F G H I J) -- (A B C D E F G H I J) D )
//vm_op_handler _op_dfifth;    //dfifth   ( (A B C D E F G H I J) -- (A B C D E F G H I J) E )
//vm_op_handler _op_dsixth;    //dsixth   ( (A B C D E F G H I J) -- (A B C D E F G H I J) F )
//vm_op_handler _op_dseventh;  //dseventh ( (A B C D E F G H I J) -- (A B C D E F G H I J) G )
//vm_op_handler _op_deighth;   //deight   ( (A B C D E F G H I J) -- (A B C D E F G H I J) H )
//vm_op_handler _op_dninth;    //dninth   ( (A B C D E F G H I J) -- (A B C D E F G H I J) I )
//vm_op_handler _op_dtenth;    //dtenth   ( (A B C D E F G H I J) -- (A B C D E F G H I J) J )


//string operators
vm_op_handler _op_tostring;//tostring ( "A" -- "A" ); ( 1 -- "1" )
vm_op_handler _op_toident; //tostring ( "A" -- A )
vm_op_handler _op_substr;  //substr   ( "ABCDE" 1 2 -- "BC" )
vm_op_handler _op_trim;    //trim     ( "  A " -- "A" )
vm_op_handler _op_split;   //split    ( "A B C" -- ("A" "B" "C") )
vm_op_handler _op_split2;  //split2   ( "A,,B, C" ", " -- ("A" "" "B" " C") )
vm_op_handler _op_find;    //find     ( "A B C" " " -- 1 ); ( "A B C" " C" -- 3 )
vm_op_handler _op_rfind;   //rfind    ( "A B C" " " -- 3 ); ( "A B C" "C " -- -1 )
vm_op_handler _op_firstof; //firstof  ( "A,B C" ", " -- 1 ); ( "A;B;C" ", " -- -1 )
vm_op_handler _op_lastof;  //lastof   ( "A,B C" ", " -- 3 ); ( "A;B;C" ", " -- -1 )
vm_op_handler _op_firstnotof; //firstnotof  ( " ,B C ," " ," -- 2 ); ( "A;B;C" "ABC;" -- -1 )
vm_op_handler _op_lastnotof;  //lastnotof   ( "A,B C ," " ," -- 4 ); ( "A;B;C" "ABC;" -- -1 )
vm_op_handler _op_join;    //join     ( ("A" "B" "C") -- "ABC" )
vm_op_handler _op_join2;   //join2    ( ("A" "B" "C") " " -- "A B C" )

vm_op_handler _op_strhash; //strhash  ( "abc" -- 1134309195 ); ( "abcd" -- 3118363509 )
vm_op_handler _op_getbyte; //getbyte  ( "ABC" 0 -- "ABC" 65 ); ( "ABC" 1 -- "ABC" 66 )
vm_op_handler _op_setbyte; //setbyte  ( "XBC" 65 0 -- "ABC" ); ( "ABC" 66 2 -- "ABB" )


//list operators
vm_op_handler _op_expand;    //expand  ( [A B C] -- A B C );
vm_op_handler _op_clearlist; //clearlist ( [A B] -- [] ); ( (A) -- () )

vm_op_handler _op_tocode;    // tocode ( (A) -- [A] ); ( [A] -- [A] )
vm_op_handler _op_tolist;    // tolist ( [A] -- (A) ); ( (A) -- (A) )
vm_op_handler _op_parsecode; // parsecode ( "A B" -- [A B] )
vm_op_handler _op_parsecode_; // parsecode ( "[A B]" -- [ ident([) A B ident(]) ] ) parses code without handling grouping operators -- should only be used if you know what you are doing

vm_op_handler _op_setnth;    // setnth   ( (a b c d e) f 2 -- (a f c d e) ); ( (a b c d e) f 4 -- (a b c f e) )
vm_op_handler _op_swapnth;   // swapnth  ( (a b c d e) f 2 -- (a f c d e) b ); ( (a b c d e) f 4 -- (a b c f e) d )
vm_op_handler _op_rsetnth;   // rsetnth  ( f (a b c d e) 2 -- (a f c d e) ); ( f (a b c d e) 4 -- (a b c f e) )
vm_op_handler _op_mapnth;    // mapnth   ( (a b c d e) [X] 2 -- (a bX c d e) ); ( (a b c d e) [X] 4 -- (a b c dX e) )

vm_op_handler _op_sort;      //sort     ( (4 1 3 5 2) -- (1 2 3 4 5) )
vm_op_handler _op_rsort;     //sort     ( (4 1 3 5 2) -- (5 4 3 2 1) )
//vm_op_handler _op_sortp;     //sortp    ( (4 1 3 5 2) [>] -- (5 4 3 2 1) )

//stack manipulators
vm_op_handler _op_clear;     //clear ( ... -- )
vm_op_handler _op_collapse;  //collapse ( ... A B C -- ( ... A B C ) )
vm_op_handler _op_restore;   //restore  ( ... (A B C) -- A B C ... )
vm_op_handler _op_stacksize; //stacksize ( -- stacksize )

vm_op_handler _op_swap;    // swap  ( A B -- B A )
vm_op_handler _op_dup;     // dup   ( A -- A A )
vm_op_handler _op_dup2;    // dup2  ( A B -- A B A )
vm_op_handler _op_dup3;    // dup3  ( A B C -- A B C A )
vm_op_handler _op_2dup;    // 2dup  ( A B C -- A B C B C)
vm_op_handler _op_3dup;    // 3dup  ( A B C -- A B C A B C)
vm_op_handler _op_dupn;    // dupn  ( A B C 2 -- A B A )
vm_op_handler _op_pop;     // pop   ( A -- )
vm_op_handler _op_2pop;    // 2pop  ( A B -- )
vm_op_handler _op_3pop;    // 3pop  ( A B C -- )

vm_op_handler _op_dig2;
vm_op_handler _op_dig3;
vm_op_handler _op_bury2;
vm_op_handler _op_bury3;
vm_op_handler _op_flip3;
vm_op_handler _op_flip4;
vm_op_handler _op_dign;
vm_op_handler _op_buryn;
vm_op_handler _op_flipn;

//general combinators
vm_op_handler _op_eval;     //eval   ( [A] -- A )
vm_op_handler _op_evalr;    //eval recursively (as long as top of stack isn't simple value)
vm_op_handler _op_quote;    //quote  ( A -- [A] )
vm_op_handler _op_wrap;     //quote  ( A -- (A) )
vm_op_handler _op_wrap2;    //quote  ( A B -- (A B) )
vm_op_handler _op_wrap3;    //quote  ( A B C -- (A B C) )
vm_op_handler _op_wrapn;    //quote  ( A B C 2 -- A (B C) ); ( A B C 3 -- (A B C) )
vm_op_handler _op_protect;  //protect( 1 -- 1 ); ( "a" -- "a" ); ( [A] -- [[A]] ); ( A -- \A );
vm_op_handler _op_dip;      //dip    ( [A] [B] -- B [A] )
vm_op_handler _op_dip2;     //dip2   ( [A] [B] [C] -- C [A] [B] )
vm_op_handler _op_dip3;     //dip3   ( [A] [B] [C] [D] -- D [A] [B] [C] )
vm_op_handler _op_dipn;     //dipn   ( [A] [B] [C] 2 -- C A B ); ( [A] [B] [C] [D] 3 -- D [A] [B] [C] )
vm_op_handler _op_dipe;     //dipe   ( [A] [B] -- B A ) -- sap is the name I have seen for this before, but [dip eval] makes sense to me
vm_op_handler _op_sip;      //sip    ( [A] [B] -- [A] B [A] )
vm_op_handler _op_sip2;     //sip2   ( [A] [B] [C] -- [A] [B] C [A] [B] )
vm_op_handler _op_sip3;     //sip3   ( [A] [B] [C] [D] -- [A] [B] [C] D [A] [B] [C] )
vm_op_handler _op_sipn;     //sipn   ( [A] [B] [C] 2 -- [A] [B] C [A] [B] ); ( [A] [B] [C] [D] 3 -- D [A] [B] [C] )
vm_op_handler _op_0apply;   //0apply ( ... [A] -- A (...) restore -- ... A ) eval quotation with entire stack hidden (then restore old stack under)
vm_op_handler _op_1apply;   //apply ( ... x [A] -- x A (...) restore -- ... x A ) apply quotation to only the top item of the stack (hide and then restore the rest)
vm_op_handler _op_2apply;   //2apply ( ... x y [A] -- x y A (...) restore -- ... x y A ) apply quotation to only top 2 items of the stack (hide and then restore the rest)
vm_op_handler _op_3apply;   //3apply ( ... x y z [A] -- x y z A (...) restore -- ... x y z A ) apply quotation to only top 3 items of the stack (hide and then restore the rest)
vm_op_handler _op_napply;   //napply ( ... x y z [A] 3 -- x y z A (...) restore -- ... x y z A ) apply quotation to only the top n items of the stack (hide and then restore the rest)
vm_op_handler _op_spread;   //spread ( x y z ( [A] [B] [C] ) -- ( xA yB zC ) )

//conditional operators
vm_op_handler _op_if;       // if ( [true] [A] -- A ); if ( [false] [A] -- )
vm_op_handler _op_ifelse;   // if ( [true] [A] [B] -- A ); if ( [false] [A] [B] -- B )
vm_op_handler _op_if_;      // if ( true [A] -- A ); if ( false [A] -- ) -- does not eval conditional
vm_op_handler _op_ifelse_;  // if ( true [A] [B] -- A ); if ( false [A] [B] -- B ) -- does not eval conditional
vm_op_handler _op_only;     // if ( true [A] -- A ); if ( false [A] -- false ) -- 
vm_op_handler _op_unless;   // if ( false [A] -- A ); if ( true [A] -- true ) -- 

//loop operators
vm_op_handler _op_each;
vm_op_handler _op_eachr;
vm_op_handler _op_times;
vm_op_handler _op_while;
vm_op_handler _op_loop_;
vm_op_handler _op_while_;

//recursion operators -- TODO: for now we are using concat quotations for these
vm_op_handler _op_linrec;
vm_op_handler _op_binrec;

#endif
