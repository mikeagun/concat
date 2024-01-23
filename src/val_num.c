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

#include "val_num.h"
#include "val_num_internal.h"
#include "val_string.h"
#include "val_printf.h"
#include <math.h>
#include <string.h> //memcpy
#include <stdlib.h> //labs



//printf functions

int val_int32_fprintf(int32_t v, FILE *file, const struct printf_fmt *fmt) {
  char conv = fmt->conversion;
  if (conv == 'c') {
    return fprintf(file,"%c",(int)v);
  } else if (conv=='f' || conv=='e'||conv=='E' || conv=='g'||conv=='G' || conv=='q'||conv=='Q' || conv=='a'||conv=='A') {
    return _val_double_fprintf((double)v,file,fmt);
  } else if (conv=='m') {
    return val_fprint_cstr(file,err_string(v));
  } else {
    int r;
    val_t tbuf;
    if ((r = val_string_init_empty(&tbuf))) return r;
    valstruct_t *tv = __str_ptr(tbuf);
    if (0>(r = val_int32_sprintf(v,tv,fmt))) goto out_buf;
    r = val_fprint_(file,_val_str_begin(tv),_val_str_len(tv));
    val_destroy(tbuf);
    return r;
out_buf:
    val_destroy(tbuf);
    return r;
  }
}

int val_int32_sprintf(int32_t v, valstruct_t *buf, const struct printf_fmt *fmt) {
  char conv = fmt->conversion;
  if (conv=='f' || conv=='e'||conv=='E' || conv=='g'||conv=='G' || conv=='q'||conv=='Q' || conv=='a'||conv=='A') {
    return _val_double_sprintf((double)v,buf,fmt);
  } else if (conv=='m') {
    return val_sprint_cstr(buf,err_string(v));
  } else {
    return _val_long_sprintf(v,buf,fmt);
  }
}

int val_double_fprintf(double v, FILE *file, const struct printf_fmt *fmt) {
  return _val_double_fprintf(v,file,fmt);
}

int val_double_sprintf(double v, valstruct_t *buf, const struct printf_fmt *fmt) {
  return _val_double_sprintf(v,buf,fmt);
}







char _val_num_signchar(int isneg, int flags) {
  if (isneg) return '-';
  else if (flags & PRINTF_F_PLUS) return '+';
  else if (flags & PRINTF_F_SPACE) return ' ';
  else return '\0';
}

int _val_num_string_zero(const char *n,int len) {
  int i;
  for(i=0;i<len;++i) {
    if (n[i] != '0') return 0;
  }
  return 1;
}

int _val_num_dstring_round_nhe(char *n, int len, int trunc) {
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

int _val_num_dstring_round_nho(char *n, int len, int trunc) {
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
    if (*off<=*sepi) return _throw(ERR_BADESCAPE);
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
    int r;
    val_t tbuf;
    if ((r = val_string_init_empty(&tbuf))) return r;
    valstruct_t *tv = __str_ptr(tbuf);
    r = _val_num_dstring_sprint(tv,sign,buf,buflen,off,digits,roundto,extendto,sepi,exp,conv,flags);
    if (r > 0) fprintf(file,"%*.s",r,_val_str_begin(tv));
    val_destroy(tbuf);
    return r;
  } else {
    _val_num_dstring_format(buf,buflen, &off, &digits, roundto, extendto, &sepi, &exp, &extraoff, &extralen, &expoff, &explen, conv, flags);
    if (sepi<0) return fprintf(file,"%.1s%.*s%.*s%.*s",&sign,digits,buf+off,extralen,buf+extraoff,explen,buf+expoff); //no dot
    else if (sepi<=digits) return fprintf(file,"%.1s%.*s.%.*s%.*s%.*s",&sign,sepi,buf+off,digits-sepi,buf+off+sepi,extralen,buf+extraoff,explen,buf+expoff); //dot within (or just after) digits
    else return fprintf(file,"%.1s%.*s.%.*s%.*s%.*s",&sign,digits,buf+off,sepi-digits,buf+extraoff,extralen-(sepi-digits),buf+extraoff+(sepi-digits),explen,buf+expoff); //dot in zero padding
  }
}

int _val_num_dstring_sprint(valstruct_t *strbuf, char sign, char *buf, int buflen, int off, int digits, int roundto, int extendto, int sepi, int exp, char conv, int flags) {
  int expoff;
  int explen;
  int extraoff;
  int extralen;
  int chars = _val_num_dstring_format(buf,buflen, &off, &digits, roundto, extendto, &sepi, &exp, &extraoff, &extralen, &expoff, &explen, conv, flags);
  if (chars<0) return -1;
  if (sign) chars++;
  int thousands = (flags & PRINTF_F_SQUOTE) && (sepi>3 || (sepi<0 && digits>3));
  if (thousands) {
    if (sepi>0) chars += (sepi-1)/3;
    else if (sepi<0) chars += (digits-1)/3;
  }
  if (strbuf) {
    int r;
    char *nbuf;
    if ((r = _val_str_rextend(strbuf,chars,&nbuf))) return r;
    if (sign) *(nbuf++) = sign;

    if (thousands) {
      int i,j=0;
      if (sepi<0) {
        int t=digits+extralen;
        for(i=0;i<digits;i++) {
          nbuf[j++]=buf[off+i];
          if (--t && !(t%3)) nbuf[j++]=',';
        }
        for(i=0;i<extralen;i++) {
          nbuf[j++]=buf[extraoff+i];
          if (--t && !(t%3)) nbuf[j++]=',';
        }

        memcpy(nbuf+j,buf+expoff,explen);
      } else if (sepi<=digits) {
        int t=sepi;
        for(i=0;i<sepi;i++) {
          nbuf[j++]=buf[off+i];
          if (--t && !(t%3)) nbuf[j++]=',';
        }

        nbuf[j++]='.';
        memcpy(nbuf+j,buf+off+sepi,digits-sepi);
        memcpy(nbuf+j+digits-sepi,buf+extraoff,extralen);
        memcpy(nbuf+j+digits-sepi+extralen,buf+expoff,explen);
      } else {
        int t=sepi;
        for(i=0;i<digits;i++) {
          nbuf[j++]=buf[off+i];
          if (--t && !(t%3)) nbuf[j++]=',';
        }
        for(i=0;i<sepi-digits;i++) {
          nbuf[j++]=buf[extraoff+i];
          if (--t && !(t%3)) nbuf[j++]=',';
        }

        nbuf[j++]='.';
        memcpy(nbuf+j,buf+extraoff+(sepi-digits),extralen-(sepi-digits));
        memcpy(nbuf+j+extralen-(sepi-digits),buf+expoff,explen);
      }
    } else {
      if (sepi<0) {
        memcpy(nbuf,buf+off,digits);
        memcpy(nbuf+digits,buf+extraoff,extralen);
        memcpy(nbuf+digits+extralen,buf+expoff,explen);
      } else if (sepi<=digits) {
        memcpy(nbuf,buf+off,sepi);
        nbuf[sepi]='.';
        memcpy(nbuf+sepi+1,buf+off+sepi,digits-sepi);
        memcpy(nbuf+digits+1,buf+extraoff,extralen);
        memcpy(nbuf+digits+1+extralen,buf+expoff,explen);
      } else {
        memcpy(nbuf,buf+off,digits);
        memcpy(nbuf+digits,nbuf+extraoff,sepi-digits);
        nbuf[sepi]='.';
        memcpy(nbuf+sepi+1,buf+extraoff+(sepi-digits),extralen-(sepi-digits));
        memcpy(nbuf+digits+1+extralen,buf+expoff,explen);
      }
    }
  }
  return chars;
}


//int _val_long_fprintf(long val, FILE *file, const struct printf_fmt *fmt) {
//}

int _val_long_sprintf(long val, valstruct_t *buf, const struct printf_fmt *fmt) {
  int prec=fmt->precision;
  if (prec == 0 && val == 0) return 0;
  else if (prec < 0) prec = 1;
  
  if (fmt->conversion=='c') {
    if (buf) {
      int r;
      char *p;
      if ((r = _val_str_rextend(buf,1,&p))) return r;
      *p = (char)val;
    }
    return 1;
  } else if (fmt->conversion=='d'||fmt->conversion=='i'||fmt->conversion=='v'||fmt->conversion=='V') {
    int digits = floor( log10( labs( val ) ) ) + 1; //precalculate how many digits
    if (digits < prec) digits = prec;
    int thousands = (fmt->flags & PRINTF_F_SQUOTE) && digits>3;
    int chars = digits;
    if (val < 0 || (fmt->flags & (PRINTF_F_PLUS|PRINTF_F_SPACE))) chars++;
    if (thousands) {
      chars += (digits-1)/3;
    }

    if (buf) { //if we are actually printing (not just counting)
      int r;
      char *b; //start of our reserved section
      if ((r = _val_str_rextend(buf,chars,&b))) return r;
      long t = val;
      char signchar = _val_num_signchar(t<0,fmt->flags);
      if (signchar) {
        *(b++) = signchar;
      }
      if (t<0) t=-t;
      if (thousands&&digits>3) {
        int i=chars; if (signchar) i--;
        int c=0;
        while(digits--) {
          if (c && !(c%3)) b[--i] = ',';
          c++;
          b[--i] = (t % 10) + '0';
          t /= 10;
        }
      } else {
        while(digits--) {
          b[digits] = (t % 10) + '0';
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
      int r;
      char *b; //start of our reserved section
      if ((r = _val_str_rextend(buf,chars,&b))) return r;
      long t = val;
      char signchar = _val_num_signchar(val<0,fmt->flags);
      if (signchar) *(b++) = signchar;
      if (val<0) val=-val;

      if(val != 0 && (fmt->flags & PRINTF_F_ALT)) *(b++) = '0'; // (#) prefix with 0 if first char not already 0
      while(digits--) {
        b[digits] = (t % 8) + '0';
        t /= 8;
      }
    }
    return chars;
  //} else if (fmt->conversion=='u') { //unsigned decimal
  //  return _throw(ERR_BADESCAPE);
  } else if (fmt->conversion=='x'||fmt->conversion=='X') { //hex
    int digits = floor( log( labs( val ) )/log(16) ) + 1;
    if (digits < prec) digits = prec;
    int chars = digits;
    if (val < 0 || (fmt->flags & (PRINTF_F_PLUS|PRINTF_F_SPACE))) chars++;
    if (fmt->flags & PRINTF_F_ALT) chars+=2; // (#) prefix with 0x/0X

    if (buf) { //if we are actually printing
      int r;
      char *b; //start of our reserved section
      if ((r = _val_str_rextend(buf,chars,&b))) return r;
      long t = val;
      char signchar = _val_num_signchar(val<0,fmt->flags);
      if (signchar) *(b++) = signchar;
      if (val<0) val=-val;

      if (fmt->flags & PRINTF_F_ALT) {
        *(b++) = '0';
        *(b++) = fmt->conversion;
      }
      while(digits--) {
        if (t%16<10) b[digits] = (t%16)+'0';
        else if (fmt->conversion=='x') b[digits] = (t%16-10)+'a';
        else b[digits] = (t%16-10)+'A';
        t /= 16;
      }
    }
    return chars;
  } else {
    return _throw(ERR_BADESCAPE);
  }
}

int _val_double_fprintf(double val, FILE *file, const struct printf_fmt *fmt) {
  //TODO: implement more optimized binary-to-ascii (or do easy/probably correct thing and fallback on printf when possible)
  //  - this code handles rounding correctly, but is slow (no optimizations -- generates full base 10 result using doubling/halving on ascii chars (easy debugging) and then rounds)

  int FBUFLEN = 4096; //FIXME: use vm-specific fbuf -- or local sbuf
  char _fbuf[FBUFLEN];

  char conv = fmt->conversion;

  char sign= _val_num_signchar(signbit(val),fmt->flags);
  if (isnan(val)) {
    return fprintf(file,"%.1snan",&sign);
  } else if (!isfinite(val)) {
    return fprintf(file,"%.1sinf",&sign);
  } else if (conv=='f' || conv=='e'||conv=='E'  ||  conv=='g'||conv=='G'  ||  conv=='q'||conv=='Q'  ||  conv=='v'||conv=='V') {
    //if it is a supported decimal format, first convert to decimal digits
    int off,digits,intdigits;
    _val_double_to_decimal(val,_fbuf,FBUFLEN,&off,&digits,&intdigits);

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
    int blen = _val_double_to_hex(val,_fbuf,FBUFLEN,conv,fmt->flags,fmt->precision);
    return fprintf(file,"%.1s%.*s",&sign,blen,_fbuf);
  } else {
    return _throw(ERR_BADESCAPE);
  }
}

int _val_double_sprintf(double val, valstruct_t *buf, const struct printf_fmt *fmt) {
  //TODO: consolidate this with fprint code. mostly duplicate
  int FBUFLEN = 4096; //FIXME: use vm-specific fbuf -- or local sbuf
  char _fbuf[FBUFLEN];

  char conv = fmt->conversion;
  char sign= _val_num_signchar(signbit(val),fmt->flags);

  if (isnan(val) || !isfinite(val)) {
    int ret=3;
    if (sign) ret++;
    if (buf) {
      char *p;
      if (sign) {
        _val_str_rextend(buf,4,&p);
        *(p++) = sign;
      } else {
        _val_str_rextend(buf,3,&p);
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
    _val_double_to_decimal(val,_fbuf,FBUFLEN,&off,&digits,&intdigits);

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
      int len = _val_double_to_hex(val,_fbuf,FBUFLEN,conv,fmt->flags,fmt->precision);
      int rlen=len;
      char *p;
      if (sign) {
        rlen++;
        int r;
        if ((r = _val_str_rextend(buf,rlen,&p))) return r;
        *(p++) = sign;
      } else {
        int r;
        if ((r = _val_str_rextend(buf,len,&p))) return r;
      }
      memcpy(p,_fbuf,len);
      return rlen;
    } else {
      int len = _val_double_to_hex(val,NULL,0,conv,fmt->flags,fmt->precision);
      if (sign) len++;
      return len;
    }
  } else {
    return _throw(ERR_BADESCAPE);
  }
}

void _val_double_to_decimal(double val, char *buf, int buflen, int *offset, int *len, int *dp) {
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
    long mantissa = (long)(frexp(val,&e)*(1L<<VALDBL_MANT_BITS)); //exponent where all mantissas unique as integer and fit in int64
    e-=VALDBL_MANT_BITS;
    int i;

    //first handle sign
    if (mantissa<0) mantissa=-mantissa;

    if (e>0) off=buflen-20; //we'll be doubling, so place all the way right (we leave a little extra space for zero-exension)
    else off=20; //we'll be halving, so place most of the way left (since we build int from right-to-left)


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

int _val_double_to_hex(double val, char *buf, int buflen, char conv, int flags, int precision) {
  int i=0;
  if (buf) {
    if (buflen<5) return _fatal(ERR_FATAL); //not enough space for smallest string
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
    if (val < 1.0 || val >= 2.0) return _fatal(ERR_FATAL); //should never happen
    //number now in format 1.xxx...
    
    //rounding for explicit precision
    if (precision>=0) {
      //val = ldexp(val,precision*4);
      val *= (1<<(precision*4));
      val=round(val);
      val *= 1.0 / (1<<(precision*4));
      //val = ldexp(val,-precision*4);
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
    //else return i+sprintf(NULL,"%+d",e);
    else return i+snprintf(NULL,0,"%+d",e);
  }
}
