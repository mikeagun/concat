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

#ifndef __OPCODES_H__
#define __OPCODES_H__ 1

//OPCODES - list of concat VM opcodes
// - takes macro function with 4 arguments (C code opcode, concat opcode string, stack effects string)
// TODO: standardize stack/stack-effects comments to enable parsing/generation/validation
// TODO: stack-effect parsing and composition (so we can generate/validate/analyze stack effects comments)
//   - at least for a high level of abstraction/widening, but support for different levels of analysis would be good
//   - could be either natives or implemented in concat
// TODO: set fixed order for core opcodes, and set fixed id for quit opcode so stripped down VMs can use subset of core opcode range (plus interconstructions)
#define OPCODES(opcode) \
  opcode(NULL,"NULL","--"), \
  opcode(end,"end","--"), \
  opcode(break,"break","--"), \
  opcode(eval,"eval","[A] -- A"), \
  opcode(parsecode,"parsecode","\"[A B]\" -- [A B]"), \
  opcode(parsecode_,"parsecode_","\"[A B]\" -- [ ident([) A B ident(]) ]"), \
  opcode(pop,"pop","A B -- A"), \
  opcode(swap,"swap","A B -- B A"), \
  opcode(dup,"dup","A -- A A"), \
  opcode(dup2,"dup2","A B -- A B A"), \
  opcode(dup3,"dup3","A B C -- A B C A"), \
  opcode(dupn,"dupn","A B C 3 -- A B C A"), \
  opcode(dign,"dign","A B C 2 -- B C A"), \
  opcode(buryn,"buryn","A B C 2 -- C A B"), \
  opcode(flipn,"flipn","A B C 3 -- C B A"), \
  opcode(empty,"empty","() -- 1 | (A) -- 0"), \
  opcode(small,"small","() -- 1 | (A) -- 1 | (A B) -- 0"), \
  opcode(size,"size","() -- 0 | (A) -- 1 | (A B) -- 2"), \
  opcode(lpop,"lpop","(A B C) -- A (B C)"), \
  opcode(lpush,"lpush","A (B C) -- (A B C)"), \
  opcode(rpop,"rpop","(A B C) -- C (A B)"), \
  opcode(rpush,"rpush","C (A B) -- (A B C)"), \
  opcode(cat,"cat","(A B) (C D) -- (A B C D) | \"AB\" \"CD\" -- \"ABCD\""), \
  opcode(rappend,"rappend","(A) (B) -- (B A) | A (B) (B A)"), \
  opcode(splitn,"splitn","(A B C D) 2 -- (A B) (C D) | \"ABCD\" 2 -- \"AB\" \"CD\""), \
  opcode(strhash,"strhash","\"abc\" -- 1134309195 | \"abcd\" -- 3118363509"), \
  opcode(getbyte,"getbyte","\"ABC\" 0 -- \"ABC\" 65 | \"ABC\" 1 -- \"ABC\" 66"), \
  opcode(setbyte,"setbyte","\"XBC\" 65 0 -- \"ABC\" | \"ABC\" 66 2 -- \"ABB\""), \
  opcode(first,"first","(A B C) -- A | \"ABC\" -- \"A\""), \
  opcode(last,"last","(A B C) -- C | \"ABC\" -- \"C\""), \
  opcode(nth,"nth","(A B C) 2 -- B"), \
  opcode(rest,"rest","(A B C) -- (B C) | \"ABC\" -- \"BC\""), \
  opcode(dfirst,"dfirst","(A B C) -- (A B C) A | \"ABC\" -- \"ABC\" \"A\""), \
  opcode(dlast,"dlast","(A B C) -- (A B C) C | \"ABC\" -- \"ABC\" \"C\""), \
  opcode(dnth,"dnth","(A B C) 2 -- (A B C) B"), \
  opcode(swapnth,"swapnth","(a b c) f 2 -- (a f c) b | (a b c) f 3 -- (a b f) c"), \
  opcode(setnth,"setnth","(A B C) E 2 -- (A E C)"), \
  opcode(collapse,"collapse","A B C -- (A B C)"), \
  opcode(restore,"restore","A B (C D) -- C D A B"), \
  opcode(expand,"expand","A B (C D) -- A B C D"), \
  opcode(sort,"sort","(D A C B) -- (A B C D)"), \
  opcode(rsort,"rsort","(D A C B) -- (D C B A)"), \
  opcode(clearlist,"clearlist","(A B C) -- ()"), \
  opcode(quote,"quote","A -- [A]"), \
  opcode(wrap,"wrap","A -- (A)"), \
  opcode(wrapn,"wrapn","A B 2 -- (A B)"), \
  opcode(protect,"protect","1 -- 1 | (A) -- (A) | [A] -- [[A]] | pop -- \\pop"), \
  opcode(add,"+","1 2 -- 3"), \
  opcode(sub,"-","2 1 -- 1"), \
  opcode(mul,"*","2 3 -- 6"), \
  opcode(div,"/","4 2 -- 2 | 2.0 4 -- 0.5"), \
  opcode(inc,"inc","1 -- 2"), \
  opcode(dec,"dec","2 -- 1"), \
  opcode(neg,"_","2 -- -2 | -2 -- 2"), \
  opcode(abs,"abs","2 -- 2 | -2 -- 2"), \
  opcode(sqrt,"sqrt","4 -- 2 | 4.0 -- 2.0"), \
  opcode(log,"log","A -- B"), \
  opcode(pow,"^","2 3 -- 8.0"), \
  opcode(mod,"%","4 3 -- 1"), \
  opcode(bit_and,"&","1 2 -- 0 | 2 3 -- 2"), \
  opcode(bit_or,"|","1 2 -- 3 | 2 3 -- 3"), \
  opcode(bit_xor,"xor","1 2 -- 3 | 3 3 -- 0"), \
  opcode(bit_not,"~",""), \
  opcode(bit_lshift,"lshift","1 2 -- 4 | 1 3 -- 8"), \
  opcode(bit_rshift,"rshift","4 1 -- 2 | 16 2 -- 4"), \
  opcode(lt,"<","1 2 -- 1 | 2 1 -- 0 | 2 2 -- 0"), \
  opcode(le,"le","2 1 -- 0 | 2 2 -- 1 | 1 2 -- 1"), \
  opcode(gt,">","2 1 -- 1 | 1 2 -- 0 | 2 2 -- 0"), \
  opcode(ge,"ge","1 2 -- 0 | 2 2 -- 1 | 3 2 -- 1"), \
  opcode(eq,"=","1 1 -- 1 | 1 2 -- 0"), \
  opcode(ne,"ne","1 1 -- 0 | 1 2 -- 1"), \
  opcode(compare,"compare","1 2 -- -1 | 2 1 -- 1 | 2 2 -- 0"), \
  opcode(bool,"bool","10 -- 1 | 1 -- 1 | 0 -- 0 | () -- 0 | (A) -- 1"), \
  opcode(not,"not","10 -- 0 | 1 -- 0 | 0 -- 1 | () -- 1 | (A) -- 0"), \
  opcode(and,"and","[A] [B] -- A [B] only bool"), \
  opcode(and_,"and_"," A B -- bool(A)&bool(B)"), \
  opcode(or,"or","[A] [B] -- A [B] unless bool"), \
  opcode(or_,"or_","A B -- bool(A)|bool(B)"), \
  opcode(find,"find","\"ABCD\" \"C\" -- 2 | \"ABCD\" \"E\" -- -1"), \
  opcode(parsenum,"parsenum","\"7\" 7 | \"7.0\" -- 7.0"), \
  opcode(toint,"toint","7 -- 7 | 7.0 -- 7"), \
  opcode(tofloat,"tofloat","7 -- 7.0 | 7.0 -- 7.0"), \
  opcode(tostring,"tostring","7 -- \"7\" | () -- \"()\""), \
  opcode(toident,"toident","\"eval\" -- eval"), \
  opcode(substr,"substr","\"ABCDE\" 1 3 -- \"BCD\""), \
  opcode(trim,"trim","\"  ABC \" -- \"ABC\""), \
  opcode(isnum,"isnum","A -- bool | 1 -- 1 | 1.0 -- 1 | () -- 0"), \
  opcode(isint,"isint","A -- bool | 1 -- 1 | 1.0 -- 0 | () -- 0"), \
  opcode(isfloat,"isfloat","A -- bool | 1 -- 0 | 1.0 -- 1 | () -- 0"), \
  opcode(isstring,"isstring","A -- bool | \"A\" -- 1 | 1.0 -- 0 | () -- 0"), \
  opcode(isident,"isident","A -- bool | \\inc -- 1 | inc -- 1 | 1 -- 0"), \
  opcode(isnative,"isnative","A -- bool | op(inc) -- 1 | inc -- 0 | 1 -- 0"), \
  opcode(islist,"islist","A -- bool | () -- 1 | [] -- 0 | 0 -- 0"), \
  opcode(iscode,"iscode","A -- bool | () -- 0 | [] -- 1 | 0 -- 0"), \
  opcode(islisttype,"islisttype","A -- bool | () -- 1 | [] -- 1 | 0 -- 0"), \
  opcode(isdict,"isdict","A -- bool | {DICT} -- 1 | () -- 0"), \
  opcode(isref,"isref","A -- bool | ref(A) -- 1 | 1 -- 0"), \
  opcode(isfile,"isfile","A -- bool | file -- 1 | 1 -- 0"), \
  opcode(isvm,"isvm","A -- bool | vm(...<|>...) -- 1 | 1 -- 0"), \
  opcode(ispush,"ispush","A -- bool | 1 -- 1 | () -- 1 | [] -- 0"), \
  opcode(dip,"dip","A [B] -- B A"), \
  opcode(dip2,"dip2","A B [C] -- C A B"), \
  opcode(dip3,"dip3","A B C [D] -- D A B C"), \
  opcode(dipn,"dipn","A B [C] 2 -- C A B"), \
  opcode(sip,"sip","A [B] -- A B A"), \
  opcode(sip2,"sip2","A B [C] -- A B C A B"), \
  opcode(sip3,"sip3","A B C [D] -- A B C D A B C"), \
  opcode(sipn,"sipn","A B [C] 2 -- A B C A B"), \
  opcode(0apply,"0apply","... [A] -- A (...) restore"), \
  opcode(1apply,"1apply","... A [B] -- A B (...) restore"), \
  opcode(2apply,"2apply","... A B [C] -- A B C (...) restore"), \
  opcode(3apply,"3apply","... A B C [D] -- A B C D (...) restore"), \
  opcode(napply,"napply","... A B C [D] 3 -- A B C D (...) restore"), \
  opcode(if,"if","[true] [A] -- A | [false] [A] -- "), \
  opcode(if_,"if_","1 [A] -- A | 0 [A] -- | [false] [A] -- A"), \
  opcode(ifelse,"ifelse","[true] [A] [B] -- A | [false] [A] [B] -- B"), \
  opcode(ifelse_,"ifelse_","1 [A] [B] -- A | 0 [A] [B] -- B | [false] [A] [B] -- A"), \
  opcode(only,"only","1 [A] -- A | 0 [A] -- 0"), \
  opcode(unless,"unless","1 [A] -- 1 | 0 [A] -- A"), \
  opcode(swaplt,"swaplt","1 2 -- 2 1 | 2 1 -- 2 1"), \
  opcode(swapgt,"swapgt","1 2 -- 1 2 | 2 1 -- 1 2"), \
  opcode(list,"list","--"), \
  opcode(print,"print","A --"), \
  opcode(print_,"print_","A --"), \
  opcode(printV,"printV","A --"), \
  opcode(printV_,"printV_","A --"), \
  opcode(printf,"printf","... \"fmt\" --"), \
  opcode(fprintf,"fprintf","... \"fmt\" file --"), \
  opcode(sprintf,"sprintf","... \"fmt\" -- \"fstring\""), \
  opcode(printlf,"printlf","... (...) \"fmt\" -- (...)"), \
  opcode(sprintlf,"sprintlf","... (...) \"fmt\" -- (...) \"fstring\""), \
  opcode(printlf2,"printlf2","... (...) (...) \"fmt\" -- (...) (...)"), \
  opcode(sprintlf2,"sprintlf2","... (...) (...) \"fmt\" -- (...) (...) \"fstring\""), \
  opcode(clear,"clear","... --"), \
  opcode(qstate,"qstate","--"), \
  opcode(vstate,"vstate","--"), \
  opcode(defined,"defined","ident -- bool"), \
  opcode(getdef,"getdef","ident -- val"), \
  opcode(def,"def","val ident --"), \
  opcode(mapdef,"mapdef","[A] ident -- ident getdef A ident def"), \
  opcode(resolve,"resolve","val -- val"), \
  opcode(rresolve,"rresolve","val -- val"), \
  opcode(scope,"scope","[A] -- _beginscope A _endscope"), \
  opcode(savescope,"savescope","[A] -- _beginscope A _popscope"), \
  opcode(_endscope,"_endscope","--"), \
  opcode(_popscope,"_popscope","-- {DICT}"), \
  opcode(usescope,"usescope","{DICT} [A] -- {DICT} _usescope A _popscope"), \
  opcode(usescope_,"usescope_","{DICT} [A] -- {DICT} _usescope A _endscope"), \
  opcode(dict_has,"dict.has","{DICT} ident -- bool"), \
  opcode(dict_get,"dict.get","{DICT} ident -- val"), \
  opcode(dict_put,"dict.put","{DICT} val ident --"), \
  opcode(open,"open","\"path\" \"mode\" -- file(\"path\")"), \
  opcode(close,"close","file() --"), \
  opcode(readline,"readline","file() -- file() \"line\""), \
  opcode(stdin_readline,"stdin.readline","-- \"line\""), \
  opcode(read,"read","file() len -- file() \"str\""), \
  opcode(write,"write","file() \"str\" -- file()"), \
  opcode(seek,"seek","file() pos -- file()"), \
  opcode(fpos,"fpos","file() -- file() pos"), \
  opcode(ref,"ref","A -- ref(A)"), \
  opcode(deref,"deref","ref(A) -- A"), \
  opcode(refswap,"refswap","ref(A) B -- ref(B) A"), \
  opcode(guard,"guard","ref(A) [B] -- lock(ref(A)) A B unlock(ref(A))"), \
  opcode(guard_sig,"guard.sig","ref(A) -- ref(A)"), \
  opcode(guard_bcast,"guard.bcast","ref(A) -- ref(A)"), \
  opcode(guard_waitwhile,"guard.waitwhile","ref(A) [testwait] [postwait] -- ref(A)"), \
  opcode(guard_sigwaitwhile,"guard.sigwaitwhile","ref(x) [testwait] [postwait] -- ref(x)"), \
  opcode(unguard,"unguard","A -- ref(A)"), \
  opcode(unguard_sig,"unguard.sig","A -- ref(A)"), \
  opcode(unguard_bcast,"unguard.bcast","A -- ref(A)"), \
  opcode(catch_unguard,"catch_unguard","-- unlock(ref(A))"), \
  opcode(waitwhile_,"waitwhile_",""), \
  opcode(sigwaitwhile_,"sigwaitwhile_",""), \
  opcode(wait,"wait",""), \
  opcode(signal,"signal",""), \
  opcode(broadcast,"broadcast",""), \
  opcode(vm,"vm","(A B C) (D E F) -- vm(A B C <|> F E D)"), \
  opcode(thread,"thread","(A B C) (D E F) -- vm_running(A B C <|> F E D)"), \
  opcode(debug,"debug","... -- vm(...<|>...)"), \
  opcode(vm_exec,"vm.exec","... vm(stack <|> work) -- stack work"), \
  opcode(vm_continue,"vm.continue","vm(stack <|> [work]) -- vm(stack work <|>)"), \
  opcode(vm_thread,"vm.thread","vm(...) -- vm_running(...)"), \
  opcode(vm_stack,"vm.stack","vm(A B C <|> ...) -- vm(A B C <|> ...) (A B C)"), \
  opcode(vm_wstack,"vm.wstack","vm(... <|> A B C) -- vm(... <|> A B C) (C B A)"), \
  opcode(vm_setstack,"vm.setstack","vm(... <|> ...) (A B C) -- vm(A B C <|> ...)"), \
  opcode(vm_wsetstack,"vm.wsetstack","vm(... <|> ...) (A B C) -- vm(... <|> C B A)"), \
  opcode(vm_qstate,"vm.qstate","vm(...) -- vm(...)"), \
  opcode(vm_vstate,"vm.vstate","vm(...) -- vm(...)"), \
  opcode(catch_debug,"catch_debug","-- vm(...)"), \
  opcode(trycatch,"trycatch","[A] [B] -- A | [A] [B] -- err B"), \
  opcode(trydebug,"trydebug","[A] -- A | [A] -- vm(... <|> ...) errno"), \
  opcode(_catch,"_catch","???"), \
  opcode(_endtry,"_endtry","???"), \
  opcode(_endtrydebug,"_endtrydebug","???"), \
  opcode(throw,"throw","E --"), \
  opcode(perror,"perror","errno --"), \
  opcode(open_code,"[","--"), \
  opcode(open_list,"(","--"), \
  opcode(close_list,")","--"), \
  opcode(catch_interactive,"catch_interactive","--"), \
  opcode(quit,"quit","--"), \
  opcode(debug_eval,"debug_eval","--"), \
  opcode(debug_noeval,"debug_noeval","--"), \
  opcode(debug_set,"debug_set","a b -- a{dbg(b)}"), \
  opcode(debug_get,"debug_get","a{dbg(b)} -- a b"), \
  opcode(ifdebug,"ifdebug","[A] -- | A"), \
  opcode(fork,"fork","-- A"), \
  opcode(socket,"socket","-- fd(A)"), \
  opcode(socket_dgram,"socket.dgram","-- fd(A)"), \
  opcode(socket_listen,"socket.listen","fd(A) B C -- fd(A)"), \
  opcode(socket_accept,"socket.accept","fd(A) -- fd(A) fd(B)"), \
  opcode(socket_connect,"socket.connect","fd(A) B C -- fd(A)"), \
  opcode(effects,"effects","\\dup -- \"A -- A A\"")

//TYPECODE - list of concat VM typecodes (opcodes used in bytecode for storing vals)
// - takes macro function with three arguments (C code opcode, concat opcode string, stack effects string)
#define TYPECODES(typecode) \
  typecode(int8,"int8",""), \
  typecode(int32,"int32",""), \
  typecode(float,"float",""), \
  typecode(string,"string",""), \
  typecode(qstring,"qstring",""), \
  typecode(ident,"ident",""), \
  typecode(qident,"qident",""), \
  typecode(bytecode,"bytecode",""), \
  typecode(list,"list",""), \
  typecode(code,"code",""), \
  typecode(dict,"dict",""), \
  typecode(ref,"ref",""), \
  typecode(file,"file",""), \
  typecode(vm,"vm","") \

//TODO: macros for builtin vm dictionary (to normalize vm code and feature set)
// - WIP code below (BUILTIN* macros)
//   - consider whether it goes here or in a new file
//   - include stack comments (after syntax set) for automatic verification of builtins
// - should make it easier to gen documentation and adjust vm size / features
//   - also allow for precompiled dict to be swapped in easily
// - might drop comma from end (since current code is list of statements)
// - 3 categories: 
//   - opcode builtins with defined dictionary words (might move into above macros)
//   - static builtins - take word and val_t
//   - compiled builtins - take word and string to compile
//
////BUILTIN_VALS - list of static concat builtins
//// - takes macro function with two arguments (concat word, static val to store)
//#define BUILTIN_VALS \
//  builtin("lt",__op_val(OP_lt)), \
//  builtin("gt",__op_val(OP_gt)), \
//  builtin("eq",__op_val(OP_eq)), \
//  builtin("stdin",TODO), \
//  builtin("stdout",TODO), \
//  builtin("stderr",TODO), \
//  builtin("true",val_true), \
//  builtin("false",val_false), \
//  builtin("pi",__dbl_val(3.14159265358979323846)), \
//  builtin("e",__dbl_val(2.71828182845904523536))
//
////BUILTINS - list of compiled concat builtins
//// - takes macro function with two arguments (concat word, builtin definition)
//#define BUILTINS \
//  builtin_compile("printlf_","sprintlf pop"), \
//  builtin_compile("printlf2_","sprintlf2 pop"), \
//  builtin_compile("second","2 nth"), \
//  builtin_compile("third","3 nth"), \
//  builtin_compile("dsecond","2 dnth"), \
//  builtin_compile("dthird","3 dnth"), \
//  builtin_compile("fourth","4 nth"), \
//  builtin_compile("fifth","5 nth"), \
//  builtin_compile("sixth","6 nth"), \
//  builtin_compile("seventh","7 nth"), \
//  builtin_compile("eigth","8 nth"), \
//  builtin_compile("ninth","9 nth"), \
//  builtin_compile("tenth","10 nth"), \
//  builtin_compile("dfourth","4 dnth"), \
//  builtin_compile("dfifth","5 dnth"), \
//  builtin_compile("dsixth","6 dnth"), \
//  builtin_compile("dseventh","7 dnth"), \
//  builtin_compile("deigth","8 dnth"), \
//  builtin_compile("dninth","9 dnth"), \
//  builtin_compile("dtenth","10 dnth"), \
//  builtin_compile("2dup","dup2 dup2"), \
//  builtin_compile("3dup","dup3 dup3 dup3"), \
//  builtin_compile("2pop","pop pop"), \
//  builtin_compile("3pop","pop pop pop"), \
//  builtin_compile("popd","swap pop"), \
//  builtin_compile("dupd","\\dup dip"), \
//  builtin_compile("swapd","\\swap dip"), \
//  builtin_compile("wrap2","wrap lpush"), \
//  builtin_compile("wrap3","wrap lpush lpush"), \
//  builtin_compile("mapnth","dup [ swapd 0 swap swapnth bury2 \\eval dip ] dip swapd setnth"), \
//  builtin_compile("dip2","2 dipn"), \
//  builtin_compile("dip3","3 dipn"), \
//  builtin_compile("dipe","dip eval"), \
//  builtin_compile("apply","[\\expand dip eval] 2apply"), \
//  builtin_compile("dig2","2 dign"), \
//  builtin_compile("dig3","3 dign"), \
//  builtin_compile("bury2","2 buryn"), \
//  builtin_compile("bury3","3 buryn"), \
//  builtin_compile("flip3","3 flipn"), \
//  builtin_compile("flip4","4 flipn"), \
//  builtin_compile("each","[ dup3 size [ [ \\lpop dip dup dip2 ] dip dup eval ] if ] dup eval pop pop pop"), \
//  builtin_compile("eachr","[ dup3 size [ [ \\rpop dip dup dip2 ] dip dup eval ] if ] dup eval pop pop pop"), \
//  builtin_compile("while","[ dup3 dip3 dig3 [ [ dup dip2 ] dip dup eval ] if ] dup eval pop pop pop"), \
//  builtin_compile("times","[ dup3 0 > [ [ \\dec dip dup dip2 ] dip dup eval ] if ] dup eval pop pop pop"), \
//  builtin_compile("loop_","[[dup dip] dip dup eval] dup eval"), \
//  builtin_compile("filter","dup2 clearlist flip3 [ flip3 dup3 flip3 dup dip3 [ dig2 \\rpush \\popd ifelse] dip ] each pop"), \
//  builtin_compile("filter2","[ dup clearlist dup flip3 ] dip swap [ bury3 \\dup dip3 dup 4 dipn 4 dign [ \\rpush dip2 ] [ [ swapd rpush ] dip ] ifelse ] each pop"), \
//  builtin_compile("map","dup2 clearlist flip3 [ bury2 dup dip2 \\rpush dip ] each pop"), \
//  builtin_compile("mapr","dup2 clearlist flip3 [ bury2 dup dip2 \\lpush dip ] eachr pop"), \
//  builtin_compile("mmap","dup2 clearlist flip3 [ bury2 dup dip2 \\rappend dip ] each pop"), \
//  builtin_compile("cleave","dup clearlist swap [ swap [sip swap] dip rpush ] each swap pop"), \
//  builtin_compile("spread","dup clearlist swap dup size swap [ dup2 inc dipn dup inc dign dig2 rpush swap dec ] each pop"), \
//  builtin_compile("bi","[ dup2 \\eval dip ] dip eval"), \
//  builtin_compile("tri","[ dup2 \\eval dip ] dip2 [ dup2 \\eval dip ] dip eval"), \
//  builtin_compile("findp","0 1 [ [ inc dup3 empty [ pop 0 0 ] 1 ifelse ] 0 ifelse ] [ \\lpop dip2 dup2 dip3 dig3 not ] while popd popd dec"), \
//  builtin_compile("evalr","[dup2 ispush [swap dip dup eval] unless] dup eval pop pop"), \
//  builtin_compile("linrec","dig2 [ [ 4 dupn 4 dipn 4 dign not [ inc dup3 4 dipn dup2 eval ] if ] 0 dup2 eval [ pop pop pop ] dip ] dip2 dip2 times"), \
//  builtin_compile("binrec","[ 5 dupn 5 dipn 5 dign [ 4 dupn 5 dipn ] [ dup3 5 dipn 5 dign dup2 dip 5 buryn dup eval dup2 5 dipn ] ifelse ] dup eval pop pop pop pop pop"), \
//  builtin_compile("openr","\"r\" open"), \
//  builtin_compile("include","fopenr eval"), \
//  builtin_compile("writeline","\"\n\" cat write"), \
//  builtin_compile("eachline","swap [ readline dup isint not ] [ bury2 dup2 dip2 ] while [pop pop] dip"), \
//  builtin_compile("isferr","0 <"), \
//  builtin_compile("isfeof","-18 =")
//
//  //builtin_compile("and","swap dip only bool"), \
//  //builtin_compile("or","swap dip unless bool"), \

//enum opcode_t   {   OP_end=0,  OP_break,   OP_native,   OP_keep_native,
//    OP_int8,   OP_int64,   OP_double,   OP_qstring,   OP_string,   OP_ident,   OP_bytecode,   OP_empty_list,   OP_empty_code, OP_list, OP_code,
//    OP_pop,   OP_swap,   OP_dup,
//    OP_empty,   OP_lpop,   OP_lpush,   OP_rpop,   OP_rpush,   OP_first,   OP_ith,
//    OP_add,   OP_sub,   OP_inc,   OP_dec,   OP_print,
//    OP_eq, OP_ne, OP_lt, OP_le, OP_gt, OP_ge,
//    OP_eval,  OP_if,   OP_if_,   OP_ifelse,   OP_ifelse_,   OP_only,   OP_unless,
//    NUM_OPCODES
//};

//TODO: evaluate, reconsider, and codify base opcode set and standard dictionary
// - this is not the smallest set of combinators possible (nor is it intended to be)
//   - beyond one of many minimal bases, many different combinator interconstructions are possible
//   - different applications are best / most efficiently implemented in terms of different base sets of combinators, so it is worth implementing more than just the minimum possible
// - once we start compiling to bytecode (for reuse) it will be much harder to change, so we need to codify before then
//   - having <= 127 core ops would be convenient (within one byte with space for flag bit or varbyte bit)
//     - right now we are already around 198+14 typecodes, so would require some refactoring (one of: make some quotations, make some native functions, make extended opcode range using e.g. varbyte)
//   - should be <= 256 (including type opcodes for all bytecode types)
// - add back support for native C functions -- use these for heavier or infrequently called ops rather than opcodes
//   - e.g. syscalls -- there are lots, they are generally heavy anyways, not all systems will support the same ones, and they generally just need wrapper func + marshalling
// - reconsider scoping namespaces as more features are added -- keep the root dictionary smaller with scoping, or flat namespace for simplicity
//   - e.g. all the constants in the file/stream interface -- alternatively could pass names (not constants) to these, and the API could have its own dict for resolving them
// - codify the concat minimal base -- those opcodes we can always assume, i.e. are safe for cross-platform bytecode
// - codify the minimal interpreter base -- superset of concat minimal opcode base, those words we can expect to be defined when running interpreted concat code
// - codify the standard interconstructions for everything between the opcode minimal base and the standard dictionary
// - add macro switches to enable/disable opcode ranges (and switches to provide default interconstrutions for the same ranges of opcodes)
//   - organize the ranges to best support portable opcodes, while also allowing us to reduce the ops array size for minimal vms
// - reconsider how strongly typed the stack should be -- we could push more work out onto the interpreter/compiler, or keep our strongly typed stack that we have now
// - should file operations have 'f' prefix (e.g. fopen/fclose/fread instead of open/close/read)
// - ops for consideration:
//   - ops that negligibly improve performance should likely be dropped (and if they don't improve performance or significantly reduce code size they definitely should be dropped)
//   - stack-effects interface -- need to design one (and then add it) - main needs are parsing, composing, and validating
//   - system ops -- there could be one "sys" opcode which takes an argument, or several base system ops
//   - loop ops -- we should probably add a least a few basic ones (e.g. each,eachr,while,times, maybe map,filter), but rest can all be interconstructed
//     - by having ops for the core loops we can cheat the stack to save cycles (e.g. store loop op under the op to reduce wstack/stack shuffling
//   - conditional ops -- if_ sufficient, only/unless/ifelse_ also very useful, swaplt/swapgt shortcut several steps in sorting or min/max, decide where to draw the line (if/ifelse/swaplt/swapgt/...)
//     - idea: implment if as [\eval dip if_] -- rely on compiler/optimizer to eliminate the first 2 steps when push-type (will still be a little slower when not push type or unknown type)
//     - and/or in particular seem like they may not be needed (especially if we have only/unless) -- and/or could be implemented as quotations and then optimized where possible at optimziation time
//   - #-prefix/suffix ops. some are so common they are probably worth having up to 2/3 as opcodes, rest (where not a performance bottleneck) should be quotations
//   - d-prefix/suffix ops. If the dup (might) be expensive, keep the op, otherwise drop. For dip, only keep if it is so common dropping the extra op is worth it (or would result in recursion problems)
//   - comparison ops -- keep them all, or have reduced opcode set and dictionary interconstructions?
//     - if we keep reduced set, we can rely on compiler/optimizer to optimize (as much as possible) cost for the available opcodes
//   - printing ops -- minimally only need sprintf/print_ ops (and maybe sprintflf/lf2), choose reasonable base set and interconstructions
//     - also consider non-printing variants that are just used for preformatting (i.e. printf_,printlf_,printlf2_)
//     - idea: have only print_/sprintf/fprintf ops, make print/printV/printV_/printf and non-printing variants quotations, and make lf/lf2 variants native functions (then only 3 ops instead of 13)
//       - could make sprintlf,sprintlf2 ops for 5 ops total (with rest being quotations)
//   - typechecking ops -- if we add numeric (or string-based) typecode, then only need single op to get type code (plus maybe a few for checking against type catgegories like number/list/string)
//     - also consider whether standard test ops should be test/replace-style (i.e. do you need to dup if you want to keep val -- it seems like maybe we usually want to keep val)
//   - try/catch ops -- since these are already heavy operations not to be overused, we could try having just the most primitive ops, and use quotations for the actual try/catch words
//   - debugging (vm-manipulation) ops -- could add a few basic self-vm-manip ops, then just a few ops for running code in a child vm and transfering stack vals between vms
//   - possible load/store interface -- this is very non-concat, but also is very much how nearly all real CPUs work, so adding this at the bottom may simplify some of the higher-level opcodes
//     - the current set is fairly "pure" (less some internal ops for err handling, debugging, and refs), which doesn't need to be the case for the vm-level opcodes

//opcodes (quit is the last core opcode, above that are system-specific natives)
#define OP_ENUM(op,opstr,effects) OP_##op
typedef enum { OPCODES(OP_ENUM), N_OPS, N_OPCODES = OP_quit } opcode_t;
#undef OP_ENUM

//bytecode opcodes (normal vm opcodes, typecodes, extra unsafe bytecode ops for use in compiled code)
#define OPCODE_ENUM(op,opstr,effects) OPCODE_##op
#define TYPECODE_ENUM(op,opstr,effects) TYPECODE_##op
typedef enum { OPCODES(OPCODE_ENUM), TYPECODES(TYPECODE_ENUM), N_BYTECODES } bytecode_t;
#undef TYPECODE_ENUM
#undef OPCODE_ENUM

extern const char *opstrings[];
extern const char *op_effects[];

#endif
