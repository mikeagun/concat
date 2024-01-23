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

#ifndef _VAL_PRINTF_FMT_H_
#define _VAL_PRINTF_FMT_H_ 1

typedef struct printf_fmt {
  const char *spec; //conversion spec string (mainly for debugging)
  int speclen; //length of conversion specifier string (spec)
  int field_width; //minimum field width (pad out to this width if needed based on padding rules)
  int precision; //meaning depends on the conversion (e.g. number of sig figs, number of bytes from string)
  enum {
    PRINTF_F_NONE    = 0,

    //PRINTF_FLAGS
    PRINTF_F_ALT     = (1<<0), //(#) printf alternate form (o - first char is zero, prefix zero if needed; x X - nonzero has 0x/0X prepended; aAeEfFgG - always dot (even if no digits after); gG - trailing zeros not removed as normally would be. undefined for other conversions
    PRINTF_F_ZERO    = (1<<1), //(0) zero padded (overridden by _LEFT)
    PRINTF_F_MINUS    = (1<<2), //(-) left-justification (default is right) (overrides _LEFT)
    PRINTF_F_SPACE   = (1<<3), //( ) insert ' ' before positive number / empty string from signed conversion (aligns pos/neg number starts)
    PRINTF_F_PLUS    = (1<<4), //(+) sign (+ or -) should always be inserted before result of signed number conversion (overrides ' ')
    PRINTF_F_SQUOTE  = (1<<5), //(') for decimal conversions, use thousands separator, for float conversions with exponent use a multiple of 3 for exponent -- PHP uses ' to specify padding char (so 'A means pad with 'A' chars)
    //PRINTF_F_I       = (1<<6), //(I) for decimal integer conversions, use alternative output digits from locale
    PRINTF_F_POP     = (1<<7), // - whether val should be popped (determined by conv type, not a user-set flag)
    PRINTF_F_BINARY  = (1<<8), //(b) print int/float data in binary (so just print raw bytes)

    //SCANF_FLAGS
    SCANF_DISCARD    = (1<<9), //(*) assignment supression flag, not included in success count, reads input as directed but discards
    //SCANF_ALLOCATE   = (1<<10), //(m) used with %s %c %[, allocates buffer for caller of sufficient size (not needed for concat)
  } flags;
  enum { //length_mod - concat doesn't use this yet, but we may for binary printf
    PRINTF_none,   //no length modifier
    PRINTF_hh,     //following int is signed/unsigned char, or following n is pointer to signed char
    PRINTF_h,      //following int is signed/unsigned short int, or following n is pointer to short int
    PRINTF_l,      //following int is signed/unsigned long int, or following n is pointer to long int, or following c is wint_t, or following s is pointer to wchar_t
    PRINTF_ll,     //following int is signed/unsigned long long int, or following n is pointer to long long int
    PRINTF_L,      //following a,A,e,E,f,F,g,G is long double. synonym for ll
    PRINTF_j,      //following int is intmax_t/uintmax_t, or following n is pointer to intmax_t
    PRINTF_z,      //following int is size_t/ssize_t, or following n is pointer to size_t
    PRINTF_t,      //following int is ptrdiff_t, or following n is pointer to ptrdiff_t
  } length_mod;
  char conversion; //conversion specifier character
  //Standard c printf conversion specifiers:
  //d i          - signed int. precision specifies min digits (zero padded,default=1). when zero is printed with precision=0, output is empty
  //o u x X      - unsigned int. octal (o), decimal (u), or hex (x X). x=abcdef, X=ABCDEF. precision is min digits (see (d i) above)
  //e E          - double. rounded and converted to style [-]d.ddde+-dd, with 1 digit before dot and num digits after dot equals precision (default=0). precision=0 means no dot. E uses E for exponent instead of e. always 2+ digits in exponent
  //f F          - double. rounded and converted to style [-]ddd.ddd, where num digits after dot is equal to precision (default=6). precision=0 means no decimal point. if dot, at least one digit before it
  //g G          - double. converted in style f or e (or F or E for G). precision specifies num sig digits (default=6,precision=0 treated as 1). e used if exp < -4 or >= precision, f used otherwise. trailing zeros removed from fraction l part, only dot if there is following digit
  //a A          - double. converted to hex in style [-]0xh.hhhhp+-. for A prefix is 0X and uses ABCDEF and exp sep P. 1 hex before dot, precision hex digits after. default precision = exact representation if on exists in base 2, otherwise large enough to distinguish values of type double. digit before dot unspecified for nonnormalized numbers, and nonzero by otherwise unspecified for normalized numbers
  //c            - if no l flag, int converted to unsigned char and written. with l, wint_t (wide char) converted to multibyte sequence by wcrtomb(3), with conversion starting in init state, and resulting multibyte string written
  //s            - if no l flag, const char* arg written up to '\0'. with precision, no more than precision bytes are written (still honors '\0').
  //p            - void* arg printed in hex (as if by %#x or %#lx
  //n            - number of chars written so far stored into integer pointed to by corresponding arg
  //m            - glibc extension to print strerror(errno)
  //%            - escaped '%'
  //////C            - synonym for lc (DONT USE)
  //////S            - synonym for ls (DONT USE)
  
  //Concat printf extensions
  //CONVERSION SPECS
  //_            - ignore the argument (if popping arguments then pop it, but don't print anything)
  //v            - print value in default print format
  //V            - print value in more verbose/unambiguous/parsable format (e.g. string printed in double quotes with escapes)
  //q Q          - engineering format -- like e/E but with precision specifying digits after decimal and exponent a multiple of 3 (so 1-3 digits before decimal point)
  //               - e/E with ' flag in concat work as engineering format with precision specifying sig figs (so 1-3 digits before decimal, prec = total digits)
  

  //Standard c scanf conversion specifiers
  //%          - matches literal %
  //d          - matches optionally signed decimal integer
  //D          - equivalent to ld and for backwards compatibility only (DONT USE)
  //i          - matches optionally signed int (read in base 8 with 0 prefix, 16 with 0x/0X prefix, and 10 otherwise)
  //o          - matches unsigned octal int
  //u          - matches unsigned decimal int
  //x X        - matches unsigned hex int
  //f e g E a  - matches optionally signed float
  //s          - matches sequence of non-white-space chars. input stops at whitespace or max field width
  //c          - sequence of chars of length max field width (default: 1). supressess witespace skip
  //[          - matches set of chars between [ and ] (or inverted set if first char is ^). supressess witespace skip
  //p          - matches pointer value (as from %p in printf)
  //n          - saves number of chars consumed so far (not counted as a conversion for return count)
  //
  //libc scanf returns number of items, but we can just return list of items
} fmt_t;

extern const fmt_t *fmt_v; //standard print format
extern const fmt_t *fmt_V; //verbose/code print format

#endif
