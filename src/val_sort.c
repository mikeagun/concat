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

#include "val_sort.h"
#include "val_compare.h"

void val_qsortp(val_t *p, unsigned int size, int (compare_lt)(val_t *lhs, val_t *rhs)) {
  if (size < 2) return;

  //we are using the median of first/last/middle for the pivot
  val_t *pval = &p[0];
  val_t *mid = &p[size/2];
  val_t *last = &p[size-1];
  unsigned int i=0, j=size;

  if ((compare_lt(pval,mid) && compare_lt(mid,last)) || (compare_lt(last,mid) && compare_lt(mid,pval)) ) pval=mid;
  else if ((compare_lt(pval,last) && compare_lt(last,mid)) || (compare_lt(mid,last) && compare_lt(last,pval)) ) pval=last;

  while (1) {
    j--;
    while (compare_lt(pval,&p[j])) j--; //find last record that should go before pivot
    while (compare_lt(&p[i],pval)) i++; //find first record that should go after pivot
    if  (i < j) { //swap (if the list isn't fully partitioned)
      val_swap(&p[i],&p[j]);
    } else {
      break; //j is now the pivot, and the list is partitioned
    }
    i++;
  }
  j++; //we put the pivot in the second half (TODO: not sure if this is needed - maybe edge cases)
  val_qsortp(&p[0], j, compare_lt);
  val_qsortp(&p[j],size-j, compare_lt);
}
void val_qsort(val_t *p, unsigned int size) {
    if (size < 2) return;

    //we are using the median of first/last/middle for the pivot
    val_t *pval = &p[0];
    val_t *mid = &p[size/2];
    val_t *last = &p[size-1];
    unsigned int i=0, j=size;

    if ((val_lt(pval,mid) && val_lt(mid,last)) || (val_lt(last,mid) && val_lt(mid,pval)) ) pval=mid;
    else if ((val_lt(pval,last) && val_lt(last,mid)) || (val_lt(mid,last) && val_lt(last,pval)) ) pval=last;

    while (1) {
      j--;
      while (val_lt(pval,&p[j])) j--; //find last record that should go before pivot
      while (val_lt(&p[i],pval)) i++; //find first record that should go after pivot
      if  (i < j) { //swap (if the list isn't fully partitioned)
        val_swap(&p[i],&p[j]);
      } else {
        break; //j is now the pivot, and the list is partitioned
      }
      i++;
    }
    j++; //we put the pivot in the second half (TODO: not sure if this is needed - maybe edge cases)
    val_qsort(&p[0], j);
    val_qsort(&p[j],size-j);
}
void val_rqsort(val_t *p, unsigned int size) {
    if (size < 2) return;

    //we are using the median of first/last/middle for the pivot
    val_t *pval = &p[0];
    val_t *mid = &p[size/2];
    val_t *last = &p[size-1];
    unsigned int i=0, j=size;

    if ((val_gt(pval,mid) && val_gt(mid,last)) || (val_gt(last,mid) && val_gt(mid,pval)) ) pval=mid;
    else if ((val_gt(pval,last) && val_gt(last,mid)) || (val_gt(mid,last) && val_gt(last,pval)) ) pval=last;

    while (1) {
      j--;
      while (val_gt(pval,&p[j])) j--; //find last record that should go before pivot
      while (val_gt(&p[i],pval)) i++; //find first record that should go after pivot
      if  (i < j) { //swap (if the list isn't fully partitioned)
        val_swap(&p[i],&p[j]);
      } else {
        break; //j is now the pivot, and the list is partitioned
      }
      i++;
    }
    j++; //we put the pivot in the second half (TODO: not sure if this is needed - maybe edge cases)
    val_rqsort(&p[0], j);
    val_rqsort(&p[j],size-j);
}

