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

#include "val_math.h"
#include "helpers.h"
#include <stdlib.h>
#include <ctype.h> //for char classes
#include <math.h>
//
//we ask all the math ops to be inlined because we always want those to be as few cycles as possible

inline void _val_dbl_toint32(val_t *a) {
  *a = __int_val( (uint32_t)__val_dbl(*a) );
}
inline void _val_int32_todbl(val_t *a) {
  *a = __dbl_val( (double)__val_int(*a) );
}

double pow10_small[] = { 1.0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15 };
double pow10_big[] = { 1e16, 1e32, 1e64, 1e128, 1e256 }; //308 is max

double pow10_neg_small[] = { 1.0, 1e-1, 1e-2, 1e-3, 1e-4, 1e-5, 1e-6, 1e-7, 1e-8, 1e-9, 1e-10, 1e-11, 1e-12, 1e-13, 1e-14, 1e-15 };
double pow10_neg_big[] = { 1e-16, 1e-32, 1e-64, 1e-128, 1e-256 }; //-324 is max

//multiply val by a power of 10 (between -324 and 308 inclusive)
double mul_pow10(double val, int e) {
  if (e > 308) return val*INFINITY;
  else if (e < -324) return val/INFINITY;
  else {
    if (e>0) {
      if (e & 0xf) val *= pow10_small[e&0xf];
      e >>= 4;
      int i;
      for(i=0;e;e>>=1,i++) {
        if (e&1) val *= pow10_big[i];
      }
    } else if (e<0) {
      e=-e;
      if (e & 0xf) val *= pow10_neg_small[e&0xf];
      e >>= 4;
      int i;
      for(i=0;e;e>>=1,i++) {
        if (e&1) val *= pow10_neg_big[i];
      }
    }
  }
  return val;
}


//Parse a number val (could be any type)
// - uses a switch-based state machine to parse
err_t val_num_parse(val_t *val, const char *s, unsigned int len) {
  //FIXME: parses digits as positive number, so can't get all the way to -2^63-1 (off by one)
  //FIXME: this currently only works for floats where the decimal digits all fit into a long int (and doesn't handle float rounding correctly)
  

  int state = 0;
  // 0    1          2          3       4      5    6   7    8      9      10
  //init +/-  leading_decimal digits decimal digits e  +/- digits space leading_0
  long int digits=0;
  int ndigits=0;
  int sign=1;
  int exp=0;
  int e=0;
  int esign=1;
  int isfloat=0;

  const char *p;
  unsigned int n;
  for(p=s,n=len; n; --n,++p) {
    if (isspace(*p)) {
      switch (state) {
        case 0: break;
        case 3: case 4: case 5: case 8: state=9; break;
        case 9: break;
        default: return ERR_BADPARSE;
      }
    } else if (*p == '+' || *p == '-') {
      switch (state) {
        case 0: state=1; break;
        case 6: state=7; break;
        default: return ERR_BADPARSE;
      }
    } else if (isdigit(*p)) {
      switch(state) {
        case 0: case 1:
          if (*p=='0' && state<2) state=10;
          else state=3;
          break;
        case 2: state=5; break;
        case 3: break;
        case 4: state=5; break;
        case 5: break;
        case 6: state=8; break;
        case 7: state=8; break;
        case 8: break;
        case 10: state=3; break;
        default: return ERR_BADPARSE;
      }
    } else if (*p == '.') {
      switch(state) {
        case 0: case 1: state=2; break;
        case 3: case 10: state=4; break;
        default: return ERR_BADPARSE;
      }
    } else if (*p == 'e' || *p == 'E') {
      switch(state) {
        case 3: case 10: case 4: case 5: state=6; break;
        default: return ERR_BADPARSE;
      }
    } else if (state==10 && (*p=='x'||*p=='X')) {
      return val_num_hex_parse(val,s,len);
    } else return ERR_BADPARSE;
    switch(state) {
      case 1:
        if (*p == '+') ; //sign=1;
        else if (*p == '-') sign=-1;
        else return ERR_BADPARSE;
        break;
      case 2: case 4:
        isfloat=1;
        break;
      case 3:
        if (ndigits<18 || (ndigits==18 && (digits*10+(*p-'0'))>digits)) {
          digits *= 10;
          digits += (*p-'0');
          ndigits++;
        } else {
          isfloat=1;
          ndigits++;
          exp++;
        }
        break;
      case 5:
        if (ndigits<18) {
          ndigits++;
          digits *= 10;
          digits += (*p-'0');
          exp--;
        } //else we've run out of buffer
        break;
      case 6:
        isfloat=1;
        break;
      case 7:
        if (*p == '+') ; //esign=1;
        else if (*p == '-') esign=-1;
        else return ERR_BADPARSE;
        break;
      case 8:
        e *= 10;
        e += (*p-'0');
        break;
    }
  }
  switch(state) {
    case 3: case 10: case 4: case 5: case 8: case 9: 
      if (isfloat) {
        //return val_float_init(val,sign * digits * pow(10.0,exp+(esign*e)));
        *val = __dbl_val(sign * mul_pow10((double)digits,exp+(esign*e)));
        return 0;
      } else {
        *val = __int_val(sign * digits);
        return 0;
      }
    default: return ERR_BADPARSE;
  }
}
err_t val_int_parse(val_t *val, const char *s, unsigned int len) {
  err_t e;
  if ((e = val_num_parse(val,s,len))) return e;
  if (val_is_double(*val)) *val = __int_val((int32_t)__val_dbl(*val));
  return 0;
}
err_t val_double_parse(val_t *val, const char *s, unsigned int len) {
  err_t e;
  if ((e = val_num_parse(val,s,len))) return e;
  if (val_is_int(*val)) *val = __dbl_val((double)__val_int(*val));
  return 0;
}


//parse hex string which could be int or float (with optional 0x after optional sign)
// - uses a switch-based state machine to parse
err_t val_num_hex_parse(val_t *val, const char *s, unsigned int len) {
  int state = 0;
  // 0    1          2          3       4      5    6   7    8      9      10      11
  //init +/-  leading_decimal digits decimal digits e  +/- digits space leading_0   x
  long int mant=0;
  int sticky=0;
  int nbits=0;
  int neg=0;
  int exp=0;
  int e=0;
  int esign=1;
  int isfloat=0;
  int n=0;
  for(n=len; n; --n,++s) {
    if (isspace(*s)) {
      switch (state) {
        case 0: break;
        case 3: case 4: case 5: case 8: state=9; break;
        case 9: break;
        default: return ERR_BADPARSE;
      }
    } else if (*s == '+' || *s == '-') {
      switch (state) {
        case 0: state=1; break;
        case 6: state=7; break;
        default: return ERR_BADPARSE;
      }
    } else if (ishex(*s)) {
      switch(state) {
        case 0: case 1:
          if (*s=='0' && state<2) state=10;
          else state=3;
          break;
        case 2: state=5; break;
        case 3: break;
        case 4: state=5; break;
        case 5: break;
        case 6: state=8; break;
        case 7: state=8; break;
        case 8: break;
        case 10: case 11: state=3; break;
        default: return ERR_BADPARSE;
      }
    } else if (*s == '.') {
      switch(state) {
        case 0: case 1: case 11: state=2; break;
        case 3: case 10: state=4; break;
        default: return ERR_BADPARSE;
      }
    } else if (*s == 'p' || *s == 'P') {
      switch(state) {
        case 3: case 4: case 5: case 10: state=6; break;
        default: return ERR_BADPARSE;
      }
    } else if (*s == 'x' || *s == 'X') {
      if (state==10) state=11;
      else return ERR_BADPARSE;
    } else return ERR_BADPARSE;
    switch(state) {
      case 1:
        if (*s == '+') ;
        else if (*s == '-') neg=1;
        else return ERR_BADPARSE;
        break;
      case 2: case 4:
        isfloat=1;
        break;
      case 3:
      case 5:
        if (nbits>=VALDBL_MANT_BITS) { //we already have all our bits, just check for rounding/exponent changes
          if (state==3) exp+=4;
          if (sticky) {
            if (*s!='0') { //trigger rounding
              sticky=0;
              mant++;
              if (mant>=(1L<<nbits)) { //rounding carried all the way over
                mant>>=1; //drop final bit
              }
            }
          } //else we don't have to do anything
          break;
        } else if (nbits>0 && nbits+4<VALDBL_MANT_BITS) { //simple case, just append the bits and continue
          mant <<= 4;
          mant += dehex(*s);
          nbits+=4;
          if (state==5) exp-=4;
        } else if (nbits+4>=VALDBL_MANT_BITS) { //we've already got our bits, update rounding and escape
          char h = dehex(*s);
          //first take the bits we need from h
          int need = VALDBL_MANT_BITS-nbits;
          mant <<= need;
          mant += (h>>(4-need));
          h &= (1<<(4-need))-1;

          int bits=4-need; //how many bits we have left in h
          nbits+=need;
          if (state==5) exp-=need;
          else exp+=bits;
          if (bits==0) { //ran out of bits exactly, so need to look ahead
            if (len>0) {
              if (ishex(s[1]) && s[1]>='8') {
                --n;++s;
                h=dehex(*s);
                if (state==3) exp+=4;
              } else if (s[1] == '.' && len>1 && ishex(s[2]) && s[2]>='8') {
                n-=2;s+=2;
                h=dehex(*s);
              }
            } //else no carry needed so we are done (at least with mantissa)
            bits=4;
          }

          if (h&(1<<(bits-1))) { //if first truncated bit is a 1 we may have to round up (otherwise we are done)
            h &= (1<<(bits-1))-1;
            if (mant&1 || h) {
              mant++;
              if (mant>=(1L<<nbits)) { //rounding carried all the way over
                mant>>=1; //drop final bit
              }
              if (mant>=(1L<<nbits)) return ERR_BADPARSE; //should never happen (for debugging)
            } else {
              sticky=1; //keep waiting for a non-zero digit to determine rounding
            }
          }
        } else { //nbits == 0, initialize with bits from leading hex char
          mant = (*s>'9') ? 9 + (*s&0xf) : (*s-'0');
          if (state==5) {
            nbits=4;
            exp-=4;
          }
          else if (mant>7) nbits=4; //determine sig-bits of leading hex digit
          else if (mant>3) nbits=3;
          else if (mant>1) nbits=2;
          else if (mant>0) nbits=1;
          //else nbits=0; //default to zero so we try again with next digit
        }
        break;
      case 6:
        isfloat=1;
        break;
      case 7:
        if (*s == '+') ;
        else if (*s == '-') esign=-1;
        else return ERR_BADPARSE;
        break;
      case 8:
        if (*s>'9') return ERR_BADPARSE; //exponent is in decimal
        e *= 10;
        e += (*s-'0');
        break;
    }
  }
  switch(state) {
    case 3: case 4: case 5: case 8: case 9: 
      if (isfloat) {
        e *= esign;
        e += exp;
        //double v = ldexp(copysign(mant,neg ? -1.0 : 1.0),e);
        double v = (double)mant;
        if (neg) v=-v;
        v *= pow(2,e);
        *val = __dbl_val(v);
      } else {
        *val = __int_val(mant);
      }
      return 0;
    default: return ERR_BADPARSE;
  }
}

//parse hex floating point literal (with optional 0x after optional sign)
// - uses a switch-based state machine to parse
err_t _hfloat_parse(double *val, const char *s, unsigned int len) {
  int state = 0;
  // 0    1          2          3       4      5    6   7    8      9      10      11
  //init +/-  leading_decimal digits decimal digits e  +/- digits space leading_0   x
  long int mant=0;
  int sticky=0;
  int nbits=0;
  int neg=0;
  int exp=0;
  int e=0;
  int esign=1;
  int isfloat=0;
  int n=0;
  for(n=len; n; --n,++s) {
    if (isspace(*s)) {
      switch (state) {
        case 0: break;
        case 3: case 4: case 5: case 8: state=9; break;
        case 9: break;
        default: return ERR_BADPARSE;
      }
    } else if (*s == '+' || *s == '-') {
      switch (state) {
        case 0: state=1; break;
        case 6: state=7; break;
        default: return ERR_BADPARSE;
      }
    } else if (ishex(*s)) {
      switch(state) {
        case 0: case 1:
          if (*s=='0' && state<2) state=10;
          else state=3;
          break;
        case 2: state=5; break;
        case 3: break;
        case 4: state=5; break;
        case 5: break;
        case 6: state=8; break;
        case 7: state=8; break;
        case 8: break;
        case 10: case 11: state=3; break;
        default: return ERR_BADPARSE;
      }
    } else if (*s == '.') {
      switch(state) {
        case 0: case 1: case 11: state=2; break;
        case 3: case 10: state=4; break;
        default: return ERR_BADPARSE;
      }
    } else if (*s == 'p' || *s == 'P') {
      switch(state) {
        case 3: case 4: case 5: case 10: state=6; break;
        default: return ERR_BADPARSE;
      }
    } else if (*s == 'x' || *s == 'X') {
      if (state==10) state=11;
      else return ERR_BADPARSE;
    } else return ERR_BADPARSE;
    switch(state) {
      case 1:
        if (*s == '+') ;
        else if (*s == '-') neg=1;
        else return ERR_BADPARSE;
        break;
      case 2: case 4:
        isfloat=1;
        break;
      case 3:
      case 5:
        if (nbits>=VALDBL_MANT_BITS) { //we already have all our bits, just check for rounding/exponent changes
          if (state==3) exp+=4;
          if (sticky) {
            if (*s!='0') { //trigger rounding
              sticky=0;
              mant++;
              if (mant>=(1L<<nbits)) { //rounding carried all the way over
                mant>>=1; //drop final bit
              }
            }
          } //else we don't have to do anything
          break;
        } else if (nbits>0 && nbits+4<VALDBL_MANT_BITS) { //simple case, just append the bits and continue
          mant <<= 4;
          mant += dehex(*s);
          nbits+=4;
          if (state==5) exp-=4;
        } else if (nbits+4>=VALDBL_MANT_BITS) { //we've already got our bits, update rounding and escape
          char h = dehex(*s);
          //first take the bits we need from h
          int need = VALDBL_MANT_BITS-nbits;
          mant <<= need;
          mant += (h>>(4-need));
          h &= (1<<(4-need))-1;

          int bits=4-need; //how many bits we have left in h
          nbits+=need;
          if (state==5) exp-=need;
          else exp+=bits;
          if (bits==0) { //ran out of bits exactly, so need to look ahead
            if (len>0) {
              if (ishex(s[1]) && s[1]>='8') {
                --n;++s;
                h=dehex(*s);
                if (state==3) exp+=4;
              } else if (s[1] == '.' && len>1 && ishex(s[2]) && s[2]>='8') {
                n-=2;s+=2;
                h=dehex(*s);
              }
            } //else no carry needed so we are done (at least with mantissa)
            bits=4;
          }

          if (h&(1<<(bits-1))) { //if first truncated bit is a 1 we may have to round up (otherwise we are done)
            h &= (1<<(bits-1))-1;
            if (mant&1 || h) {
              mant++;
              if (mant>=(1L<<nbits)) { //rounding carried all the way over
                mant>>=1; //drop final bit
              }
              if (mant>=(1L<<nbits)) return ERR_BADPARSE; //should never happen (for debugging)
            } else {
              sticky=1; //keep waiting for a non-zero digit to determine rounding
            }
          }
        } else { //nbits == 0, initialize with bits from leading hex char
          mant = (*s>'9') ? 9 + (*s&0xf) : (*s-'0');
          if (state==5) {
            nbits=4;
            exp-=4;
          }
          else if (mant>7) nbits=4; //determine sig-bits of leading hex digit
          else if (mant>3) nbits=3;
          else if (mant>1) nbits=2;
          else if (mant>0) nbits=1;
          //else nbits=0; //default to zero so we try again with next digit
        }
        break;
      case 6:
        isfloat=1;
        break;
      case 7:
        if (*s == '+') ;
        else if (*s == '-') esign=-1;
        else return ERR_BADPARSE;
        break;
      case 8:
        if (*s>'9') return ERR_BADPARSE; //exponent is in decimal
        e *= 10;
        e += (*s-'0');
        break;
    }
  }
  switch(state) {
    case 3: case 4: case 5: case 8: case 9: 
      e *= esign;
      e += exp;
      //*val = ldexp(copysign(mant,neg ? -1.0 : 1.0),e);
      *val = (double)mant;
      if (neg) *val=-*val;
      *val *= pow(2,e);
      return len;
    default: return ERR_BADPARSE;
  }
}


inline void _val_dbl_add(val_t *a, val_t b) {
  double x = __val_dbl(*a) + __val_dbl(b);
  __val_set(a,__dbl_val(x));
}
inline void _val_dbl_sub(val_t *a, val_t b) {
  double x = __val_dbl(*a) - __val_dbl(b);
  __val_set(a,__dbl_val(x));
}
inline void _val_dbl_mul(val_t *a, val_t b) {
  double x = __val_dbl(*a) * __val_dbl(b);
  __val_set(a,__dbl_val(x));
}
inline void _val_dbl_div(val_t *a, val_t b) {
  double x = __val_dbl(*a) / __val_dbl(b);
  __val_set(a,__dbl_val(x));
}
inline void _val_dbl_neg(val_t *a) {
  //double x = -__val_dbl(*a);
  //__val_set(a,__dbl_val(x));
  *a ^= (val_t)1UL << 63; //directly flip sign bit
}
inline void _val_dbl_inc(val_t *a) {
  double x = __val_dbl(*a);
  ++x;
  __val_set(a,__dbl_val(x));
}
inline void _val_dbl_dec(val_t *a) {
  double x = __val_dbl(*a);
  --x;
  __val_set(a,__dbl_val(x));
}

inline void _val_int32_add(val_t *a, val_t b) { *(int32_t*)a += __val_int(b); }
inline void _val_int32_sub(val_t *a, val_t b) { *(int32_t*)a -= __val_int(b); }
inline void _val_int32_mul(val_t *a, val_t b) { *(int32_t*)a *= __val_int(b); }
inline void _val_int32_div(val_t *a, val_t b) { *(int32_t*)a /= __val_int(b); }
inline void _val_int32_neg(val_t *a) { *(int32_t*)a = -(*(int32_t*)a); }
inline void _val_int32_inc(val_t *a) { ++(*(int32_t*)a); }
inline void _val_int32_dec(val_t *a) { --(*(int32_t*)a); }

//TODO: consider generating case integer, then we can use switch statement (or computed goto) to handle all cases


#define _val_num_binop(op,case_dd,case_di,case_id,case_ii) \
inline int val_##op(val_t *a, val_t b) { \
  if (val_is_double(*a)) { \
    if (val_is_double(b)) { case_dd; } else if (val_is_int(b)) { case_di; } else return -1; \
  } else if (val_is_int(*a)) { \
    if (val_is_double(b)) { case_id; } else if (val_is_int(b)) { case_ii; } else return -1; \
  } else return -1; \
  return 0; \
} \
inline void _val_##op(val_t *a, val_t b) { \
  if (val_is_double(*a)) { if (!val_is_double(b)) { _val_int32_todbl(&b); } case_dd; } \
  else if (val_is_double(b)) { _val_int32_todbl(a); case_dd; } \
  else { case_ii; } \
}

#define _val_num_unaryop(op,case_d,case_i) \
inline int val_##op(val_t *a) { \
  if (val_is_double(*a)) { case_d; } \
  else if (val_is_int(*a)) { case_i; } \
  else return -1; \
  return 0; \
} \
inline void _val_##op(val_t *a) { \
  if (val_is_double(*a)) { case_d; } else { case_i; } \
}


#define basic_num_binop(op) _val_num_binop(op,_val_dbl_##op(a,b), _val_int32_todbl(&b);_val_dbl_##op(a,b), _val_int32_todbl(a);_val_dbl_##op(a,b), _val_int32_##op(a,b))
#define basic_num_unaryop(op) _val_num_unaryop(op,_val_dbl_##op(a), _val_int32_##op(a))
basic_num_binop(add)
basic_num_binop(sub)
basic_num_binop(mul)
basic_num_binop(div)
basic_num_unaryop(neg)
basic_num_unaryop(inc)
basic_num_unaryop(dec)
