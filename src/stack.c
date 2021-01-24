#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <iron/types.h>
#include <iron/utils.h>
#include <iron/log.h>
#include <iron/mem.h>
#include <iron/linmath.h>
#include <iron/hashtable.h>
#include <microio.h>

#include "jamlisp.h"

void stack_push(stack * stk, const void * data, size_t count){
  if(stk->count + count >= stk->capacity){
    size_t newcap = stk->count * 2 + count;
    stk->capacity = newcap;
    stk->elements= realloc(stk->elements, newcap);
  }
  memcpy(stk->elements + stk->count, data, count);
  stk->count += count;
}

void stack_pop(stack * stk, void * data, size_t count){
  ASSERT(stk->count >= count);
  stk->count -= count;
  if(data != NULL)
    memcpy(data, stk->elements + stk->count, count);
}

void stack_top(stack * stk, void * data, size_t count){  
  memcpy(data, stk->elements + stk->count - count, count);
}
