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

#include "val_num.h"
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <string.h> //for memcpy

#include "val_string.h" //for casting
#include "val_coll.h"
#include "helpers.h"
#include "vm_err.h"
#include "val_printf.h"

#define FBUFLEN 1000
char _fbuf[FBUFLEN]; //should be sufficient for smallest negative double

//TODO: refactor
//  - right now this is a mix of almost complete implementation of printf, followed by calling standard system sprintf/fprintf
//  - instead use val_sprint/fprint, or at least some other intermediate function so we don't rely on system printf for the final step
//  - if we want to just use existing printf, maybe have macro switch for internal implementation

//we use _num_case to handle the 4 standard type cases for binary ops
enum _num_case {
  INT_INT=0,
  INT_FLOAT=1,
  FLOAT_INT=2,
  FLOAT_FLOAT=3,
};

enum _num_case _val_num_case(val_t *lhs, val_t *rhs) {
  return (lhs->type == TYPE_INT ? 0 : 2) + (rhs->type == TYPE_INT ? 0 : 1);
}


val_t* val_int_init(val_t *val, valint_t i) {
  val->type = TYPE_INT;
  val->val.i = i;
  return val;
}
void val_int_replace(val_t *val, valint_t i) {
  val_destroy(val);
  val_int_init(val,i);
}
val_t* val_float_init(val_t *val, valfloat_t f) {
  val->type = TYPE_FLOAT;
  val->val.f = f;
  return val;
}

valint_t val_int(val_t *val) {
  return val->val.i;
}
valfloat_t val_float(val_t *val) {
  return val->val.f;
}

void val_int_set(val_t *val, valint_t i) {
  val->val.i = i;
}
void val_float_set(val_t *val, valfloat_t f) {
  val->val.f = f;
}

int val_num_compare(val_t *lhs, val_t *rhs) {
  if (lhs->type == TYPE_INT && rhs->type == TYPE_INT) {
    return val_int(lhs)-val_int(rhs);
  } else {
    valfloat_t c = val_num_float(lhs)-val_num_float(rhs);
    if (c==0) return 0;
    else if (c>0) return 1;
    else return -1;
  }
}
int val_num_lt(val_t *lhs, val_t *rhs) {
  switch(_val_num_case(lhs,rhs)) {
    case INT_INT:     return lhs->val.i < rhs->val.i;
    case INT_FLOAT:   return lhs->val.i < rhs->val.f;
    case FLOAT_INT:   return lhs->val.f < rhs->val.i;
    case FLOAT_FLOAT: return lhs->val.f < rhs->val.f;
    default:          return _throw(ERR_BADARGS);
  }
}
int val_num_eq(val_t *lhs, val_t *rhs) {
  switch(_val_num_case(lhs,rhs)) {
    case INT_INT:     return lhs->val.i == rhs->val.i;
    case INT_FLOAT:   return lhs->val.i == rhs->val.f;
    case FLOAT_INT:   return lhs->val.f == rhs->val.i;
    case FLOAT_FLOAT: return lhs->val.f == rhs->val.f;
    default:          return _throw(ERR_BADARGS);
  }
}

err_t val_num_add(val_t *val, val_t *rhs) {
  switch(_val_num_case(val,rhs)) {
    case INT_INT:     val->val.i += rhs->val.i; return 0;
    case INT_FLOAT:   val_float_init(val,(valfloat_t)val->val.i + rhs->val.f); return 0;
    case FLOAT_INT:   val->val.f += rhs->val.i; return 0;
    case FLOAT_FLOAT: val->val.f += rhs->val.f; return 0;
    default:          return _throw(ERR_BADARGS);
  }
}

err_t val_num_sub(val_t *val, val_t *rhs) {
  switch(_val_num_case(val,rhs)) {
    case INT_INT:     val->val.i -= rhs->val.i; return 0;
    case INT_FLOAT:   val_float_init(val,(valfloat_t)val->val.i - rhs->val.f); return 0;
    case FLOAT_INT:   val->val.f -= rhs->val.i; return 0;
    case FLOAT_FLOAT: val->val.f -= rhs->val.f; return 0;
    default:          return _throw(ERR_BADARGS);
  }
}

err_t val_num_mul(val_t *val, val_t *rhs) {
  switch(_val_num_case(val,rhs)) {
    case INT_INT:     val->val.i *= rhs->val.i; return 0;
    case INT_FLOAT:   val_float_init(val,(valfloat_t)val->val.i * rhs->val.f); return 0;
    case FLOAT_INT:   val->val.f *= rhs->val.i; return 0;
    case FLOAT_FLOAT: val->val.f *= rhs->val.f; return 0;
    default:          return _throw(ERR_BADARGS);
  }
}

err_t val_num_div(val_t *val, val_t *rhs) {
  switch(_val_num_case(val,rhs)) {
    case INT_INT:     val->val.i /= rhs->val.i; return 0;
    case INT_FLOAT:   val_float_init(val,(valfloat_t)val->val.i / rhs->val.f); return 0;
    case FLOAT_INT:   val->val.f /= rhs->val.i; return 0;
    case FLOAT_FLOAT: val->val.f /= rhs->val.f; return 0;
    default:          return _throw(ERR_BADARGS);
  }
}

err_t val_num_mod(val_t *val, val_t *rhs) {
  switch(_val_num_case(val,rhs)) {
    case INT_INT:     val->val.i %= rhs->val.i; return 0;
    default:          return _throw(ERR_BADARGS);
  }
}

err_t val_num_neg(val_t *val) {
  if (val->type == TYPE_INT) val->val.i *= -1;
  else val->val.f *= -1;
  return 0;
}

err_t val_num_abs(val_t *val) {
  if (val->type == TYPE_INT) {
    if (val->val.i < 0) val->val.i *= -1;
  } else {
    if (val->val.f < 0) val->val.f *= -1;
  }
  return 0;
}

err_t val_num_inc(val_t *val) {
  if (val->type == TYPE_INT) val->val.i++;
  else val->val.f++;
  return 0;
}

err_t val_num_dec(val_t *val) {
  if (val->type == TYPE_INT) val->val.i--;
  else val->val.f--;
  return 0;
}

void val_int_dec_(val_t *val) {
  val->val.i--;
}

err_t val_num_sqrt(val_t *val) {
  if (val->type == TYPE_INT) val->val.i = sqrt(val->val.i);
  else val->val.f = sqrt(val->val.f);
  return 0;
}
err_t val_num_log(val_t *val) {
  if (val->type == TYPE_INT) val->val.i = log(val->val.i);
  else val->val.f = log(val->val.f);
  return 0;
}

valint_t val_num_int(val_t *val) {
  if (val->type == TYPE_FLOAT) return (valint_t)val->val.f;
  else return val->val.i;
}

valfloat_t val_num_float(val_t *val) {
  if (val->type == TYPE_FLOAT) return val->val.f;
  else return (valfloat_t)val->val.i;
}

err_t val_num_toint(val_t *val) {
  argcheck_r(val_isnumber(val));
  if (val->type == TYPE_INT) return 0;
  
  val_int_init(val,(valint_t)val->val.f);
  return 0;
}

err_t val_num_tofloat(val_t *val) {
  argcheck_r(val_isnumber(val));
  if (val->type == TYPE_FLOAT) return 0;
  
  val_float_init(val,(valfloat_t)val->val.i);
  return 0;
}

char _val_num_signchar(int isneg, int flags) {
  if (isneg) return '-';
  else if (flags & PRINTF_F_PLUS) return '+';
  else if (flags & PRINTF_F_SPACE) return ' ';
  else return '\0';
}

int val_int_fprintf(val_t *val, FILE *file, const fmt_t *fmt) {
  char conv = fmt->conversion;
  if (conv == 'c') {
    return fprintf(file,"%c",(int)val_int(val));
  } else if (conv=='f' || conv=='e'||conv=='E' || conv=='g'||conv=='G' || conv=='q'||conv=='Q' || conv=='a'||conv=='A') {
    return _val_float_fprintf(val_num_float(val),file,fmt);
  } else {
    val_t buf;
    err_t r;
    val_string_init(&buf);
    if (0>(r = val_sprintf_(val,&buf,fmt))) goto out;
    if (0>(r = val_fprintf_(&buf,stdout,fmt_v))) goto out;
out:
    val_destroy(&buf);
    return r;
  }
}
int _val_int_sprintf(valint_t val, val_t *buf, const fmt_t *fmt) {
  int prec=fmt->precision;
  if (prec == 0 && val == 0) return 0;
  else if (prec < 0) prec = 1;
  
  if (fmt->conversion=='c') {
    err_t r;
    char *p;
    if ((r = val_string_rextend(buf,1,&p))) return r;
    *p = (char)val;
    return 1;
  } else if (fmt->conversion=='d'||fmt->conversion=='i'||fmt->conversion=='v'||fmt->conversion=='V') {
    int digits = floor( log10( labs( val ) ) ) + 1;
    if (digits < prec) digits = prec;
    int thousands = (fmt->flags & PRINTF_F_SQUOTE) && digits>3;
    int chars = digits;
    if (val < 0 || (fmt->flags & (PRINTF_F_PLUS|PRINTF_F_SPACE))) chars++;
    if (thousands) {
      chars += (digits-1)/3;
    }

    if (buf) { //if we are actually printing
      char *p;
      err_t r;
      if ((r = val_string_rextend(buf,chars,&p))) return r;
      long t = val;
      char signchar = _val_num_signchar(t<0,fmt->flags);
      if (signchar) {
        *(p++) = signchar;
      }
      if (t<0) t=-t;
      if (thousands&&digits>3) {
        int i=chars; if (signchar) i--;
        int c=0;
        while(digits--) {
          if (c && !(c%3)) p[--i] = ',';
          c++;
          p[--i] = (t % 10) + '0';
          t /= 10;
        }
      } else {
        while(digits--) {
          p[digits] = (t % 10) + '0';
          t /= 10;
        }
      }
    }
    return chars;
  } else if (fmt->conversion=='o') { //octal
    int digits = floor( log( labs( val ) )/log(8) ) + 1;
    if (digits < prec) digits = prec;
    int chars = digits;
    if (val < 0 || (fmt->flags & (PRINTF_F_PLUS|PRINTF_F_SPACE))) chars++;
    if(val != 0 && (fmt->flags & PRINTF_F_ALT)) chars++; // (#) prefix with 0 if first char not already 0

    if (buf) { //if we are actually printing
      char *p;
      err_t r;
      if ((r = val_string_rextend(buf,chars,&p))) return r;
      long t = val;
      char signchar = _val_num_signchar(val<0,fmt->flags);
      if (signchar) *(p++) = signchar;
      if (val<0) val=-val;

      if(val != 0 && (fmt->flags & PRINTF_F_ALT)) *(p++) = '0'; // (#) prefix with 0 if first char not already 0
      while(digits--) {
        p[digits] = (t % 8) + '0';
        t /= 8;
      }
    }
    return chars;
  //} else if (fmt->conversion=='u') { //unsigned decimal
  //  return _throw(BADESCAPE);
  } else if (fmt->conversion=='x'||fmt->conversion=='X') { //hex
    int digits = floor( log( labs( val ) )/log(16) ) + 1;
    if (digits < prec) digits = prec;
    int chars = digits;
    if (val < 0 || (fmt->flags & (PRINTF_F_PLUS|PRINTF_F_SPACE))) chars++;
    if (fmt->flags & PRINTF_F_ALT) chars+=2; // (#) prefix with 0x/0X

    if (buf) { //if we are actually printing
      err_t r;
      char *p;
      if ((r = val_string_rextend(buf,chars,&p))) return r;
      long t = val;
      char signchar = _val_num_signchar(val<0,fmt->flags);
      if (signchar) *(p++) = signchar;
      if (val<0) val=-val;

      if (fmt->flags & PRINTF_F_ALT) {
        *(p++) = '0';
        *(p++) = fmt->conversion;
      }
      while(digits--) {
        if (t%16<10) p[digits] = (t%16)+'0';
        else if (fmt->conversion=='x') p[digits] = (t%16-10)+'a';
        else p[digits] = (t%16-10)+'A';
        t /= 16;
      }
    }
    return chars;
  } else {
    return _throw(ERR_BADESCAPE);
  }
}

int val_int_sprintf(val_t *val, val_t *buf, const fmt_t *fmt) {
  char conv = fmt->conversion;
  if (conv=='f' || conv=='e'||conv=='E' || conv=='g'||conv=='G' || conv=='q'||conv=='Q' || conv=='a'||conv=='A') {
    return _val_float_sprintf(val_num_float(val),buf,fmt);
  } else {
    return _val_int_sprintf(val_int(val),buf,fmt);
  }
}

int _val_num_string_zero(const char *n,int len) { //whether an ascii digit string is all zeroes
  int i;
  for(i=0;i<len;++i) {
    if (n[i] != '0') return 0;
  }
  return 1;
}

int _val_num_dstring_round_nhe(char *n, int len, int trunc) { //nearest-half-even rounding (does nothing if len<=trunc). return value: carry from the left-most digit
  //should be the normal rounding mode
  int carry=0;
  if (trunc >= 0 && len > trunc) { //need rounding (use nearest-even)
    if (
        n[trunc] > '5' ||
        (n[trunc] == '5' && ((trunc>0&&n[trunc-1]%2) || !_val_num_string_zero(n+trunc,len-trunc)))
       ) {
      if (trunc==0) {
        return 1;
      } else if (n[--trunc]<'9') { //simple case, no carry
        n[trunc]++;
      } else {
        int i=trunc-1;
        while(i>=0 && n[i]=='9') i--; //find first digit that doesn't need carry
        if (i<0) { //case for 0.9999...
          carry=1;
        } else {
          n[i]++;
        }
        while(++i<=trunc) n[i]='0'; //<= instead of < since decremented above
      }
    }
  }
  return carry;
}

int _val_num_dstring_round_nho(char *n, int len, int trunc) { //nearest-half-odd rounding. Useful to shorten intermediates since nhe(nho(x,n),m<(n-1)) rounds like correctly like nhe(x,m)
  //essentially truncation + sticky bit
  int carry=0;
  if (trunc >= 0 && len > trunc) { //need rounding (use nearest-even)
    if (
        n[trunc] > '5' ||
        (n[trunc] == '5' && (trunc<1 || n[trunc-1]%2==0 || !_val_num_string_zero(n+trunc,len-trunc)))
       ) {
      if (trunc==0) {
        return 1;
      } else if (n[--trunc]<'9') { //simple case, no carry
        n[trunc]++;
      } else {
        int i=trunc-1;
        while(i>=0 && n[i]=='9') i--; //find first digit that doesn't need carry
        if (i<0) { //case for 0.9999...
          carry=1;
        } else {
          n[i]++;
        }
        while(++i<=trunc) n[i]='0'; //<= instead of < since decremented above
      }
    }
  }
  return carry;
}

int _val_num_dstring_format(char *buf, int buflen, int *off, int *digits, int roundto, int extendto, int *sepi, int *exp, int *extraoff, int *extralen, int *expoff, int *explen, char conv, int flags) {
  const int expchars=7; //number of chars allocated for exponent string. should be more than enough (though TODO if that ever changes)
  buflen-=expchars;
  *extraoff=0;
  *extralen=0;
  *expoff=0;
  *explen=0;

  char expch='\0';
  if (conv=='e'||conv=='E') expch=conv;

  if (roundto<*digits) {
    int carry = _val_num_dstring_round_nhe(buf+*off,*digits,roundto);
    *digits=roundto;
    if (carry) {
      buf[--(*off)] = '1';
      if (!expch) (*digits)++;
      if (expch) (*exp)++;
      else (*sepi)++;
    }
  }

  if (*sepi<=0) { //need at least one digit before sep, zero-pad until that is so
    throw_if(ERR_BADESCAPE,*off<=*sepi);
    while(*sepi<=0) {
      buf[--(*off)] = '0';
      (*sepi)++;
      if (expch) (*exp)++;
      else (*digits)++;
    }
  }

  if (extendto>0 && *digits<extendto) {
    if (*off+extendto<buflen) { //have space, can zero-extend in-place
      while(*digits<extendto) buf[*off+(*digits)++]='0';
    } else if (*off > (extendto-*digits)) { //we can build the zeroes to the left of offset
      int i=extendto-*digits; //how many extra zeros we need
      while(i--) buf[i]='0';
      *extralen=extendto-*digits;
    } else { //ouch, you requested a lot of precision. lets just throw an error
      return _throw(ERR_BADESCAPE);
    }
  }

  //We don't need a decimal point if:
  //  - sep index invalid
  //  - sep index at end of number and we didn't pass the # flag to printf
  if (*sepi>(*digits+*extralen) || (*sepi==(*digits+*extralen) && !(flags & PRINTF_F_ALT))) {
    *sepi=-1;
  }

  if (expch!='\0') {
    int e=*exp;
    int nexp=0;
    char esign='+';
    if (e<0) {
      esign='-';
      e=-e;
    }
    int i=expchars;
    while(e) {
      buf[buflen + --i]='0'+e%10;
      e/= 10;
      nexp++;
    }
    while(nexp<2) { buf[buflen + --i]='0'; nexp++; } //min exp digits
    buf[buflen + --i]=esign;
    buf[buflen + --i]=expch;
    nexp+=2;
    *expoff=buflen+i;
    *explen=nexp;
  }
  int ret = *digits+*extralen+*explen;
  if (*sepi >= 0) ret++;
  return ret;
}

int _val_num_dstring_fprint(FILE *file, char sign, char *buf, int buflen, int off, int digits, int roundto, int extendto, int sepi, int exp, char conv, int flags) {
  int expoff;
  int explen;
  int extraoff;
  int extralen;
  int thousands = (flags & PRINTF_F_SQUOTE);
  if (thousands) {
    //just use sprint so we don't have to dup the code
    val_t tbuf;
    err_t r;
    val_string_init(&tbuf);
    r = _val_num_dstring_sprint(&tbuf,sign,buf,buflen,off,digits,roundto,extendto,sepi,exp,conv,flags);
    if (r > 0) fprintf(file,"%*.s",r,val_string_str(&tbuf));
    val_destroy(&tbuf);
    return r;
  } else {
    _val_num_dstring_format(buf,buflen, &off, &digits, roundto, extendto, &sepi, &exp, &extraoff, &extralen, &expoff, &explen, conv, flags);
    if (sepi<0) return fprintf(file,"%.1s%.*s%.*s%.*s",&sign,digits,buf+off,extralen,buf+extraoff,explen,buf+expoff); //no dot
    else if (sepi<=digits) return fprintf(file,"%.1s%.*s.%.*s%.*s%.*s",&sign,sepi,buf+off,digits-sepi,buf+off+sepi,extralen,buf+extraoff,explen,buf+expoff); //dot within (or just after) digits
    else return fprintf(file,"%.1s%.*s.%.*s%.*s%.*s",&sign,digits,buf+off,sepi-digits,buf+extraoff,extralen-(sepi-digits),buf+extraoff+(sepi-digits),explen,buf+expoff); //dot in zero padding
  }
}

int _val_num_dstring_sprint(val_t *strbuf, char sign, char *buf, int buflen, int off, int digits, int roundto, int extendto, int sepi, int exp, char conv, int flags) {
  int expoff;
  int explen;
  int extraoff;
  int extralen;
  int chars;
  if (0>(chars = _val_num_dstring_format(buf,buflen, &off, &digits, roundto, extendto, &sepi, &exp, &extraoff, &extralen, &expoff, &explen, conv, flags))) return chars;

  if (sign) chars++;
  int thousands = (flags & PRINTF_F_SQUOTE) && (sepi>3 || (sepi<0 && digits>3));
  if (thousands) {
    if (sepi>0) chars += (sepi-1)/3;
    else if (sepi<0) chars += (digits-1)/3;
  }
  if (strbuf) {
    err_t r;
    char *p;
    if ((r = val_string_rextend(strbuf,chars,&p))) return r;
    if (r) return r;
    if (sign) *(p++) = sign;

    if (thousands) {
      int i,j=0;
      if (sepi<0) {
        int t=digits+extralen;
        for(i=0;i<digits;i++) {
          p[j++]=buf[off+i];
          if (--t && !(t%3)) p[j++]=',';
        }
        for(i=0;i<extralen;i++) {
          p[j++]=buf[extraoff+i];
          if (--t && !(t%3)) p[j++]=',';
        }

        memcpy(p+j,buf+expoff,explen);
      } else if (sepi<=digits) {
        int t=sepi;
        for(i=0;i<sepi;i++) {
          p[j++]=buf[off+i];
          if (--t && !(t%3)) p[j++]=',';
        }

        p[j++]='.';
        memcpy(p+j,buf+off+sepi,digits-sepi);
        memcpy(p+j+digits-sepi,buf+extraoff,extralen);
        memcpy(p+j+digits-sepi+extralen,buf+expoff,explen);
      } else {
        int t=sepi;
        for(i=0;i<digits;i++) {
          p[j++]=buf[off+i];
          if (--t && !(t%3)) p[j++]=',';
        }
        for(i=0;i<sepi-digits;i++) {
          p[j++]=buf[extraoff+i];
          if (--t && !(t%3)) p[j++]=',';
        }

        p[j++]='.';
        memcpy(p+j,buf+extraoff+(sepi-digits),extralen-(sepi-digits));
        memcpy(p+j+extralen-(sepi-digits),buf+expoff,explen);
      }
    } else {
      if (sepi<0) {
        memcpy(p,buf+off,digits);
        memcpy(p+digits,buf+extraoff,extralen);
        memcpy(p+digits+extralen,buf+expoff,explen);
      } else if (sepi<=digits) {
        memcpy(p,buf+off,sepi);
        p[sepi]='.';
        memcpy(p+sepi+1,buf+off+sepi,digits-sepi);
        memcpy(p+digits+1,buf+extraoff,extralen);
        memcpy(p+digits+1+extralen,buf+expoff,explen);
      } else {
        memcpy(p,buf+off,digits);
        memcpy(p+digits,p+extraoff,sepi-digits);
        p[sepi]='.';
        memcpy(p+sepi+1,buf+extraoff+(sepi-digits),extralen-(sepi-digits));
        memcpy(p+digits+1+extralen,buf+expoff,explen);
      }
    }
  }
  return chars;
}

void _val_float_to_decimal(double val, char *buf, int buflen, int *offset, int *len, int *dp) {
  int digits=0;
  int intdigits;
  int off;

  if (0.0 == val) {
    buf[0] = '0';
    intdigits=1;
    digits=1;
    off=0;
  } else {
    int e;
    long mantissa = (long)(frexp(val,&e)*(1L<<VALFLOAT_MANT_BITS)); //exponent where all mantissas unique as integer and fit in int64
    e-=VALFLOAT_MANT_BITS;
    int i;

    //first handle sign
    if (mantissa<0) mantissa=-mantissa;

    if (e>0) off=buflen-20; //we'll be doubling, so place all the way right (we leave a little extra space for zero-exension)
    else off=20; //we'll be halving, so place most of the way left (since we build int fromt right-to-left)


    while(mantissa) {
      buf[off-(++digits)] = (mantissa % 10) + '0';
      mantissa /= 10;
    }
    off -= digits; //the start of the digits

    //return fprintf(file,"%.1s%.*s*2^%d",&sign,digits,fbuf+off,e); //print sign + raw decimal integer mantissa and exponent

    int x;
    //now we multiply/divide by 2 e times
    for(; e>0; --e) { //double iteratively if e>0
      x=0;
      for(i=off+digits-1; i>=off; --i) {
        x += 2 * (buf[i] - '0');
        buf[i] = '0' + (x%10); x /= 10;
      }
      if (x) { //carry from leftmost digit
        off--;
        buf[off] = '1';
        digits++;
      }
    }
    intdigits = digits;

    for(; e<0; ++e) { //else halve iteratively if e<0
      x=0;
      if (buf[off+digits-1]%2) buf[off+(digits++)] = '0'; //last digit odd, add a digit
      if ((buf[off])<'2') { //first digit is going to zero
        x=buf[off++]%2;
        digits--;
        intdigits--;
      }
      for(i=off; i<off+digits; ++i) {
        x = x*10 + (buf[i] - '0');
        buf[i] = '0' + (x/2); x %= 2;
      }
    }

    //return fprintf(file,"%.1s%.*s.%.*s",&sign,intdigits,fbuf+off,digits-intdigits,fbuf+off+intdigits); //print (optional) sign + raw base-10 converted float
  }
  *offset=off;
  *len = digits;
  *dp = intdigits;
}

int _val_float_to_hex(double val, char *buf, int buflen, char conv, int flags, int precision) {
    int i=0;
    if (buf) {
      fatal_if(ERR_FATAL,buflen<5); //not enough space for smallest string
      buf[i++] = '0';
      buf[i++] = conv + ('X'-'A');
    } else {
      i += 2;
    }

    if (0.0 == val) {
      if (buf) buf[i]='0'; i++;
      if (precision>0 || flags&PRINTF_F_ALT) {
        if (buf) buf[i]='.'; i++;
        while(precision>0) {
          if (buf) buf[i]='0'; i++;
          precision--;
        }
      }
      if (buf) {
        buf[i++] = conv + ('P'-'A');
        buf[i++] = '+';
        buf[i++] = '0';
      } else {
        i += 3;
      }
      return i;
    } else {
      int e;
      val = frexp(val,&e);
      val=fabs(val); //sign handled elsewhere
      //val = ldexp(val,1); e--; //apparently ldexp is strangely slow on many systems
      val *= 2; e--;
      fatal_if(ERR_FATAL,val < 1.0 || val >= 2.0); //should never happen
      //number now in format 1.xxx...
      
      //rounding for explicit precision
      if (precision>=0) {
        //val = ldexp(val,precision*4); //see ldexp above
        val *= (1<<(precision*4));
        val=round(val);
        val *= 1.0 / (1<<(precision*4));
        //val = ldexp(val,-precision*4); //see ldexp above
      }

      double h;
      val=modf(val,&h);
      if (buf) buf[i]='0'+(int)h; i++; //h can only be 1 or 2 (2 if we carried all the way over during rounding)

      if (precision>0 || flags&PRINTF_F_ALT || (precision<0&&val!=0.0)) {
        if (buf) buf[i] = '.'; i++;
        while(precision>0 || (precision<0 && val != 0.0)) {
          //val = ldexp(val,4); //push next 4 bits left of the decimal (**apparently ldexp strangely slow on many systems)
          val *= (1<<4); //push next 4 bits left of the decimal
          val = modf(val,&h); //keep fractional in val, save next hex digit to h
          int t = (int)h;
          if (buf) buf[i] = ((t>9) ? conv+(t - 10) : '0'+t); i++;
          precision--;
        }
      }
      if (buf) buf[i] = conv + ('P'-'A'); i++;
      if (buf) return i+sprintf(buf+i,"%+d",e);
      else return i+snprintf(NULL,0,"%+d",e);
    }
}


int _val_float_fprintf(double val, FILE *file, const fmt_t *fmt) {
  //TODO: implement more optimized binary-to-ascii (and have printf fallback and minimal macro switches)
  //  - this code handles rounding correctly, but is slow (no optimizations -- generates full base 10 result using doubling/halving on ascii chars (from debugging code) and then rounds)
  char conv = fmt->conversion;

  char sign= _val_num_signchar(signbit(val),fmt->flags);
  if (isnan(val)) {
    return fprintf(file,"%.1snan",&sign);
  } else if (!isfinite(val)) {
    return fprintf(file,"%.1sinf",&sign);
  } else if (conv=='f' || conv=='e'||conv=='E'  ||  conv=='g'||conv=='G'  ||  conv=='q'||conv=='Q'  ||  conv=='v'||conv=='V') {
    //if it is a supported decimal format, first convert to decimal digits
    int off,digits,intdigits;
    _val_float_to_decimal(val,_fbuf,FBUFLEN,&off,&digits,&intdigits);

    //we have a decimal digit string, now let's do some formatting

    if (conv == 'v' || conv == 'V') conv = 'f'; //TODO: later use minimum exact rep for V
    int prec = fmt->precision;
    if (prec<0) prec=6; //default=6

    int flags = fmt->flags;
    
    if (conv == 'f') {
      return _val_num_dstring_fprint(file,sign,_fbuf,FBUFLEN,off,digits,intdigits+prec,intdigits+prec,intdigits,0,conv,flags);
    } else {
      int exp;
      int firstdigit=0;
      while(firstdigit<digits&&_fbuf[off+firstdigit]=='0') firstdigit++;
      if (firstdigit==digits) { firstdigit=0; exp=0; }
      else exp = intdigits-firstdigit-1;

      if (conv == 'e' || conv == 'E') {
        int lhs=1;
        if (flags & PRINTF_F_SQUOTE) { //engineering notation with fixed sig figs (as long as prec>1)
          if (exp >= 0 ) lhs=(exp%3)+1;
          else lhs=((exp+1)%3)+3;
          if (digits>lhs+prec) { //if rounding causes carry we need to update number of digits
            int carry = _val_num_dstring_round_nhe(_fbuf+off,digits,lhs+prec);
            if (carry) {
              _fbuf[--off] = '1';
              intdigits++;
              exp++;
              if (exp >= 0 ) lhs=(exp%3)+1;
              else lhs=((exp+1)%3)+3;
            }
            digits=lhs+prec;
          }
          if (1+prec < lhs) prec=lhs-1; //need trailing zeros
        }
        return _val_num_dstring_fprint(file,sign,_fbuf,FBUFLEN,off+firstdigit,digits-firstdigit,1+prec,1+prec,lhs,exp-(lhs-1),conv,flags);
      } else if (conv == 'q' || conv == 'Q') {
        conv -= ('q'-'e');
        int lhs;
        if (exp >= 0 ) lhs=(exp%3)+1;
        else lhs=((exp+1)%3)+3;
        if (digits>lhs+prec) { //if rounding causes carry we need to update number of digits
          int carry = _val_num_dstring_round_nhe(_fbuf+off,digits,lhs+prec);
          if (carry) {
            _fbuf[--off] = '1';
            intdigits++;
            exp++;
            if (exp >= 0 ) lhs=(exp%3)+1;
            else lhs=((exp+1)%3)+3;
          }
          digits=lhs+prec;
        }

        return _val_num_dstring_fprint(file,sign,_fbuf,FBUFLEN,off+firstdigit,digits-firstdigit,lhs+prec,lhs+prec,lhs,exp-(lhs-1),conv,flags);
      } else if (conv == 'g' || conv == 'G') {
        if (prec == 0) prec = 1;

        if (digits>prec) { //need rounding, so do that first so we can determine how many digits we actually have
          int carry = _val_num_dstring_round_nhe(_fbuf+off,digits,prec);
          digits=prec;
          if (carry) {
            _fbuf[--off] = '1';
            intdigits++;
            exp++;
          }
        }

        int lastdigit=digits; //last non-zero digit + 1
        if (!(flags & PRINTF_F_ALT)) {
          while(lastdigit>0 && _fbuf[off+lastdigit-1]=='0') lastdigit--;
        }

        if (exp<-4 || exp >= prec) { //e/E style when <-4 or >=prec (with no trailing zeros)
          int extendto=1;
          if ((flags & PRINTF_F_ALT) && extendto<prec) extendto=prec;
          return _val_num_dstring_fprint(file,sign,_fbuf,FBUFLEN,off+firstdigit,lastdigit-firstdigit,digits,extendto,1,exp,conv-2,flags);
        } else { //else f style (with no trailing zeros)
          if (lastdigit<intdigits) lastdigit=intdigits;
          int extendto=lastdigit-firstdigit;
          if (extendto<intdigits) extendto=intdigits;
          if ((flags & PRINTF_F_ALT) && extendto<prec) extendto=prec;
          return _val_num_dstring_fprint(file,sign,_fbuf,FBUFLEN,off+firstdigit,lastdigit-firstdigit,digits,extendto,intdigits,exp,'f',flags);
        }
      } else {
        return _throw(ERR_BADESCAPE);
      }
    }
  } else if (conv=='a'||conv=='A') {
    int blen = _val_float_to_hex(val,_fbuf,FBUFLEN,conv,fmt->flags,fmt->precision);
    return fprintf(file,"%.1s%.*s",&sign,blen,_fbuf);
  } else {
    return _throw(ERR_BADESCAPE);
  }
}
int val_float_fprintf(val_t *val, FILE *file, const fmt_t *fmt) {
  return _val_float_fprintf(val_float(val),file,fmt);
}
int _val_float_sprintf(double val, val_t *buf, const fmt_t *fmt) {
  //TODO: consolidate this with fprint code. mostly duplicate
  char conv = fmt->conversion;
  char sign= _val_num_signchar(signbit(val),fmt->flags);

  err_t r;

  if (isnan(val) || !isfinite(val)) {
    int ret=3;
    if (sign) ret++;
    if (buf) {
      char *p;
      if (sign) {
        if ((r = val_string_rextend(buf,4,&p))) return r;
        *(p++) = sign;
      } else {
        if ((r = val_string_rextend(buf,3,&p))) return r;
      }
      if (isnan(val)) {
        memcpy(p,"nan",3);
      } else {
        memcpy(p,"inf",3);
      }
    }
    return ret;
  } else if (conv=='f' || conv=='e'||conv=='E'  ||  conv=='g'||conv=='G'  ||  conv=='q'||conv=='Q'  ||  conv=='v'||conv=='V') {
    //if it is a supported decimal format, first convert to decimal digits
    int off,digits,intdigits;
    _val_float_to_decimal(val,_fbuf,FBUFLEN,&off,&digits,&intdigits);

    //we have a decimal digit string, now let's do some formatting

    if (conv == 'v' || conv == 'V') conv = 'f'; //TODO: later use minimum exact rep for V
    int prec = fmt->precision;
    if (prec<0) prec=6; //default=6

    int flags = fmt->flags;
    
    if (conv == 'f') {
      return _val_num_dstring_sprint(buf,sign,_fbuf,FBUFLEN,off,digits,intdigits+prec,intdigits+prec,intdigits,0,conv,flags);
    } else {
      int exp;
      int firstdigit=0;
      while(firstdigit<digits&&_fbuf[off+firstdigit]=='0') firstdigit++;
      if (firstdigit==digits) { firstdigit=0; exp=0; }
      else exp = intdigits-firstdigit-1;


      if (conv == 'e' || conv == 'E') {
        int lhs=1;
        if (flags & PRINTF_F_SQUOTE) { //engineering notation with fixed sig figs (as long as prec>1)
          if (exp >= 0 ) lhs=(exp%3)+1;
          else lhs=((exp+1)%3)+3;
          if (digits>lhs+prec) { //if rounding causes carry we need to update number of digits
            int carry = _val_num_dstring_round_nhe(_fbuf+off,digits,lhs+prec);
            if (carry) {
              _fbuf[--off] = '1';
              intdigits++;
              exp++;
              if (exp >= 0 ) lhs=(exp%3)+1;
              else lhs=((exp+1)%3)+3;
            }
            digits=lhs+prec;
          }
          if (1+prec < lhs) prec=lhs-1; //need trailing zeros
        }
        return _val_num_dstring_sprint(buf,sign,_fbuf,FBUFLEN,off+firstdigit,digits-firstdigit,1+prec,1+prec,lhs,exp-(lhs-1),conv,flags);
      } else if (conv == 'q' || conv == 'Q') {
        conv -= ('q'-'e');
        int lhs;
        if (exp >= 0 ) lhs=(exp%3)+1;
        else lhs=((exp+1)%3)+3;
        if (digits>lhs+prec) { //if rounding causes carry we need to update number of digits
          int carry = _val_num_dstring_round_nhe(_fbuf+off,digits,lhs+prec);
          if (carry) {
            _fbuf[--off] = '1';
            intdigits++;
            exp++;
            if (exp >= 0 ) lhs=(exp%3)+1;
            else lhs=((exp+1)%3)+3;
          }
          digits=lhs+prec;
        }

        return _val_num_dstring_sprint(buf,sign,_fbuf,FBUFLEN,off+firstdigit,digits-firstdigit,lhs+prec,lhs+prec,lhs,exp-(lhs-1),conv,flags);
      } else if (conv == 'g' || conv == 'G') {
        if (prec == 0) prec = 1;

        if (digits>prec) { //need rounding, so do that first so we can determine how many digits we actually have
          int carry = _val_num_dstring_round_nhe(_fbuf+off,digits,prec);
          digits=prec;
          if (carry) {
            _fbuf[--off] = '1';
            intdigits++;
            exp++;
          }
        }

        int lastdigit=digits; //last non-zero digit + 1
        if (!(flags & PRINTF_F_ALT)) {
          while(lastdigit>0 && _fbuf[off+lastdigit-1]=='0') lastdigit--;
        }

        if (exp<-4 || exp >= prec) { //e/E style when <-4 or >=prec (with no trailing zeros)
          int extendto=1;
          if ((flags & PRINTF_F_ALT) && extendto<prec) extendto=prec;
          return _val_num_dstring_sprint(buf,sign,_fbuf,FBUFLEN,off+firstdigit,lastdigit-firstdigit,digits,extendto,1,exp,conv-2,flags);
        } else { //else f style (with no trailing zeros)
          if (lastdigit<intdigits) lastdigit=intdigits;
          int extendto=lastdigit-firstdigit;
          if (extendto<intdigits) extendto=intdigits;
          if ((flags & PRINTF_F_ALT) && extendto<prec) extendto=prec;
          return _val_num_dstring_sprint(buf,sign,_fbuf,FBUFLEN,off+firstdigit,lastdigit-firstdigit,digits,extendto,intdigits,exp,'f',flags);
        }
      } else {
        return _throw(ERR_BADESCAPE);
      }
    }
  } else if (conv=='a'||conv=='A') {
    if (buf) {
      int len = _val_float_to_hex(val,_fbuf,FBUFLEN,conv,fmt->flags,fmt->precision);
      int rlen=len;
      char *p;
      if (sign) {
        rlen++;
        err_t r = val_string_rextend(buf,rlen,&p); if (r) return r;
        *(p++) = sign;
      } else {
        err_t r = val_string_rextend(buf,len,&p); if (r) return r;
      }
      memcpy(p,_fbuf,len);
      return rlen;
    } else {
      int len = _val_float_to_hex(val,NULL,0,conv,fmt->flags,fmt->precision);
      if (sign) len++;
      return len;
    }
  } else {
    return _throw(ERR_BADESCAPE);
  }
}
int val_float_sprintf(val_t *val, val_t *buf, const fmt_t *fmt) {
  return _val_float_sprintf(val_float(val),buf,fmt);
}


void val_int_init_handlers(struct type_handlers *h) {
  h->fprintf = val_int_fprintf;
  h->sprintf = val_int_sprintf;
}
void val_float_init_handlers(struct type_handlers *h) {
  h->fprintf = val_float_fprintf;
  h->sprintf = val_float_sprintf;
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


int _val_num_parse(val_t *val, const char *s, unsigned int len) {
  //FIXME: parses digits as positive number, so can't get all the way to -2^63-1 (off by one)
  //FIXME: this currently only works for floats where the decimal digits all fit into a long int (and doesn't handle float rounding correctly)
  
  int state = 0; //TODO: clean this up with enum
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
        default: goto bad;
      }
    } else if (*p == '+' || *p == '-') {
      switch (state) {
        case 0: state=1; break;
        case 6: state=7; break;
        default: goto bad;
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
        default: goto bad;
      }
    } else if (*p == '.') {
      switch(state) {
        case 0: case 1: state=2; break;
        case 3: case 10: state=4; break;
        default: goto bad;
      }
    } else if (*p == 'e' || *p == 'E') {
      switch(state) {
        case 3: case 10: case 4: case 5: state=6; break;
        default: goto bad;
      }
    } else if (state==10 && (*p=='x'||*p=='X')) {
      double v;
      err_t r;
      if (0 > (r = _hfloat_parse(&v,s,len))) return r;

      val_float_init(val,v);
      return 0;
    } else goto bad;
    switch(state) {
      case 1:
        if (*p == '+') ; //sign=1;
        else if (*p == '-') sign=-1;
        else goto bad;
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
        else goto bad;
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
        val_float_init(val,sign * mul_pow10((double)digits,exp+(esign*e)));
        return 0;
      } else {
        val_int_init(val,sign * digits);
        return 0;
      }
    default: goto bad;
  }

bad:
  return -1;
}
err_t val_num_parse(val_t *val, const char *s, unsigned int len) {
  err_t r;
  if ((r = _val_num_parse(val,s,len))) return _throw(ERR_BADPARSE);
  else return 0;
}
err_t val_int_parse(val_t *val, const char *s, unsigned int len) {
  err_t r;
  if ((r = _val_num_parse(val,s,len))) return _throw(ERR_BADPARSE);
  else return val_num_toint(val);
}
err_t val_float_parse(val_t *val, const char *s, unsigned int len) {
  err_t r;
  if ((r = _val_num_parse(val,s,len))) return _throw(ERR_BADPARSE);
  else return val_num_tofloat(val);
}


//parse hex floating point literal (with optional 0x after optional sign)
int _hfloat_parse(double *val, const char *s, unsigned int len) {
  int state = 0; //TODO: clean this up with enum
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
        default: goto bad;
      }
    } else if (*s == '+' || *s == '-') {
      switch (state) {
        case 0: state=1; break;
        case 6: state=7; break;
        default: goto bad;
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
        default: goto bad;
      }
    } else if (*s == '.') {
      switch(state) {
        case 0: case 1: case 11: state=2; break;
        case 3: case 10: state=4; break;
        default: goto bad;
      }
    } else if (*s == 'p' || *s == 'P') {
      switch(state) {
        case 3: case 4: case 5: case 10: state=6; break;
        default: goto bad;
      }
    } else if (*s == 'x' || *s == 'X') {
      if (state==10) state=11;
      else goto bad;
    } else goto bad;
    switch(state) {
      case 1:
        if (*s == '+') ;
        else if (*s == '-') neg=1;
        else goto bad;
        break;
      case 2: case 4:
        isfloat=1;
        break;
      case 3:
      case 5:
        if (nbits>=VALFLOAT_MANT_BITS) { //we already have all our bits, just check for rounding/exponent changes
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
        } else if (nbits>0 && nbits+4<VALFLOAT_MANT_BITS) { //simple case, just append the bits and continue
          mant <<= 4;
          mant += dehex(*s);
          nbits+=4;
          if (state==5) exp-=4;
        } else if (nbits+4>=VALFLOAT_MANT_BITS) { //we've already got our bits, update rounding and escape
          char h = dehex(*s);
          //first take the bits we need from h
          int need = VALFLOAT_MANT_BITS-nbits;
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
              if (mant>=(1L<<nbits)) goto bad; //should never happen (for debugging)
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
        else goto bad;
        break;
      case 8:
        if (*s>'9') goto bad; //exponent is in decimal
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
    default: goto bad;
  }
bad:
  return _throw(ERR_BADPARSE);
}
