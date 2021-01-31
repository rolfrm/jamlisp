#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>

#include<microio.h>
#include <iron/full.h>
#include "jamlisp.h"


void run_tests();

int main(int argc, char ** argv){
  run_tests();

  jamlisp_context * ctx = jamlisp_new();
  for(int i = 0; i < 40; i++){
    
    jamlisp_object indexes[32];
    for(int i =0 ; i < 32; i++){
      indexes[i] = jamlisp_new_cons(ctx);
      ASSERT(indexes[i].cons < 50);
    }
    for(int i =0 ; i < 32; i++){
      jamlisp_free_cons(ctx, &indexes[i]);
    }
  }


  io_writer wd = {0};
  jamlisp_test_load(ctx, &wd);
  jamlisp_iterate(ctx, &wd);
  logd("\n");

  return 0;
}

void test_load2(){
  io_writer _wd = {0};
  io_writer * wd = &_wd;
  io_write_u8(wd, JAMLISP_OPCODE_LET);
}

void test_heap_objects(){
  jamlisp_context * ctx = jamlisp_new();
  for(int i = 0; i < 40; i++){
    
    jamlisp_object indexes[32];
    for(int i =0 ; i < 32; i++){
      indexes[i] = jamlisp_new_cons(ctx);
      ASSERT(indexes[i].cons < 50);
    }
    for(int i =0 ; i < 32; i++){
      jamlisp_free_cons(ctx, &indexes[i]);
    }
  }
  jamlisp_push_i64(ctx, 1000);
  jamlisp_push_i64(ctx, 10000000000001L);
  jamlisp_push_i64(ctx, 10);
  i64 value = jamlisp_pop_i64(ctx);
  i64 value2 = jamlisp_pop_i64(ctx);
  i64 value3 = jamlisp_pop_i64(ctx);
  ASSERT(value == 10);
  logd("Popped: %i %lld %lld\n", value, value2 ,value3);
}

void test_lisp_symbols(){
  jamlisp_context * ctx = jamlisp_new();
  var sym1 = jamlisp_symbol(ctx, "test1");
  var sym2 = jamlisp_symbol(ctx, "test1");
  var sym3 = jamlisp_symbol(ctx, "test");
  ASSERT(sym3.type == JAMLISP_SYMBOL);
  ASSERT(sym2.type == JAMLISP_SYMBOL);
  ASSERT(sym1.type == JAMLISP_SYMBOL);
  ASSERT(sym1.symbol == sym2.symbol);
  ASSERT(sym1.symbol != sym3.symbol);
}

void test_lisp_symbol_values(){
  logd("test_lisp_symbol_values\n");
  jamlisp_context * ctx = jamlisp_new();
  var sym1 = jamlisp_symbol(ctx, "test1");
  var sym2 = jamlisp_symbol(ctx, "test");
  ASSERT(jamlisp_nilp(symbol_get_value(ctx, sym1)));
  ASSERT(jamlisp_nilp(symbol_get_value(ctx, sym2)));
  symbol_set_value(ctx, sym1, jamlisp_i64(5));
  symbol_set_value(ctx, sym2, jamlisp_i64(50));
  ASSERT(symbol_get_value(ctx, sym1).int64 == 5);
  ASSERT(symbol_get_value(ctx, sym2).int64 == 50);
  
  symbol_set_value(ctx, sym2, jamlisp_nil());
  ASSERT(jamlisp_nilp(symbol_get_value(ctx, sym2)));

}

void test_run_lisp(){
  jamlisp_context * ctx = jamlisp_new();
  io_writer wd ={0};
  jamlisp_load_lisp2(ctx, &wd, "(+ 1 2)");
  wd.size = wd.offset;
  wd.offset = 0;
  for(int i = 0; i < wd.size; i++){
    u8 c = io_read_u8(&wd);
    logd("%x ", c);
  }
  logd("\n");
  wd.offset = 0;
  jamlisp_iterate(ctx, &wd);
  i64 result = jamlisp_pop_i64(ctx);
  ASSERT(result == 3);
}


void test_alloc_alg(){
  logd("test_alloc_alg\n");
  int * ptr = NULL;
  size_t ptr_cap = 0;
  size_t ptr_count = 0;
  int * a = alloc_elems((void **) &ptr, sizeof(*ptr), &ptr_count, &ptr_cap, 10);
  int * b = alloc_elems((void **) &ptr, sizeof(*ptr), &ptr_count, &ptr_cap, 10);
  ASSERT(a == ptr);
  ASSERT(b == ptr + 10); 
  int * c = alloc_elems((void **) &ptr, sizeof(*ptr), &ptr_count, &ptr_cap, 10);
  ASSERT(c == ptr + 20);  
  for(int i = 0; i < 30; i++){
    a[i] = i;
  }

  ASSERT(ptr_count == 30);
  int * d = alloc_elems((void **) &ptr, sizeof(*ptr), &ptr_count, &ptr_cap, 10);
  UNUSED(d);
  logd("test_alloc_alg: %i %i\n", ptr_cap, ptr_count);
}


void run_tests(){
  test_alloc_alg();

  test_heap_objects();
  test_lisp_symbols();
  test_lisp_symbol_values();
  test_run_lisp();
  
}
