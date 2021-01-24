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
    
    jamlisp_object_index indexes[32];
    for(int i =0 ; i < 32; i++){
      indexes[i] = jamlisp_new_object(ctx);
      ASSERT(indexes[i] < 50);
    }
    for(int i =0 ; i < 32; i++){
      jamlisp_free(ctx, indexes[i]);
    }
  }


  io_writer wd = {0};
  jamlisp_test_load(ctx, &wd);
  jamlisp_iterate(ctx, &wd);
  logd("\n");

  return 0;
}


void test_heap_objects(){
  jamlisp_context * ctx = jamlisp_new();
  for(int i = 0; i < 40; i++){
    
    jamlisp_object_index indexes[32];
    for(int i =0 ; i < 32; i++){
      indexes[i] = jamlisp_new_object(ctx);
      ASSERT(indexes[i] < 50);
    }
    for(int i =0 ; i < 32; i++){
      jamlisp_free(ctx, indexes[i]);
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



void run_tests(){
  test_heap_objects();
}
