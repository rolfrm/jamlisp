//#include <iron/full.h>
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

typedef jamlisp_object object;

void free_object(cons_heap * heap, jamlisp_object_index obj){
  heap->cons_heap[obj].car = jamlisp_nil();
  heap->cons_heap[obj].cdr.cons = heap->free_object;
  heap->cons_heap[obj].cdr.type = JAMLISP_CONS;
  heap->free_object = obj;
}

jamlisp_object_index new_object(cons_heap * heap){
  if(heap->free_object == 0){
    size_t new_count = (heap->heap_size + 2) * 1.5;
    heap->cons_heap = realloc(heap->cons_heap, new_count * sizeof(heap->cons_heap[0]));
    for(size_t i = heap->heap_size; i < new_count; i++){
      free_object(heap, i);
    }
    heap->heap_size = new_count;
  }
  jamlisp_object_index out = heap->free_object;
  heap->free_object = heap->cons_heap[out].cdr.cons;
  return out;
}

cons cons_get(const cons_heap * heap, jamlisp_object_index idx){
  return heap->cons_heap[idx];
}

typedef jamlisp_stack_frame stack_frame;

struct _jamlisp_context {
  
  jamlisp_opcodedef * opcodedefs;
  
  size_t opcodedef_count;

  jamlisp_opcode current_opcode;

  hash_table * opcode_names;
  hash_table * symbol_names;
  jamlisp_object * symbol_values;
  size_t symbol_values_count;
  u32 symbol_counter;
  stack_frame * stack;
  size_t stack_capacity;
  
  cons_heap heap;

  stack value_stack;


  // control stack.
  stack_frame * frames;
  size_t frames_capacity;
  u32 frame_index;
  
};


void jamlisp_free(jamlisp_context * ctx, jamlisp_object_index obj){
  free_object(&ctx->heap, obj);
}

jamlisp_array * jamlisp_array_new(jamlisp_type t, void * data, size_t size){
  jamlisp_array * a = alloc0(sizeof(a[0]));
  a->data = iron_clone(data, size);
  a->type = t;
  a->size = size;
  return a;
}

void ensure_size(void ** ptr, size_t elem_size, size_t * count, size_t new_count){
  *ptr = realloc(*ptr, elem_size * new_count);
}

void symbol_set_value(jamlisp_context * ctx, jamlisp_object symbol, jamlisp_object value){
  ASSERT(jamlisp_symbolp(symbol));
  if(ctx->symbol_values_count < symbol.symbol){
    ctx->symbol_values = realloc(ctx->symbol_values, sizeof(ctx->symbol_values[0]) * (symbol.symbol + 10));
    for(u32 i = ctx->symbol_values_count; i < symbol.symbol+10; i++){
      ctx->symbol_values[i] = (jamlisp_object){0};
    }
    ctx->symbol_values_count = symbol.symbol + 10;
  }

  ctx->symbol_values[symbol.symbol] = value; 
}

jamlisp_object symbol_get_value(jamlisp_context * ctx, jamlisp_object symbol){
  ASSERT(jamlisp_symbolp(symbol));
  if(ctx->symbol_values_count >= symbol.symbol)
    return ctx->symbol_values[symbol.symbol];
  return jamlisp_nil();
}

void jamlisp_load_fcn_bytecode(jamlisp_context * ctx, jamlisp_object symbol, void * code, size_t code_size){
  jamlisp_object symbol_value = symbol_get_value(ctx, symbol);
  if(!jamlisp_nilp(symbol_value))
    ERROR("Function is already defined\n"); // remove this sanity check later.

  symbol_value.type = JAMLISP_ARRAY;
  symbol_value.ptr = jamlisp_array_new(JAMLISP_BYTE, code, code_size);
  symbol_set_value(ctx, symbol, symbol_value);
}

jamlisp_context * jamlisp_new(){
  jamlisp_context * ctx = alloc0(sizeof(*ctx));
  ctx->opcode_names = ht_create_strkey(sizeof(jamlisp_opcode));
  ctx->symbol_names = ht_create_strkey(sizeof(ctx->symbol_counter));
  {
    jamlisp_load_opcode(ctx, JAMLISP_OPCODE_ADD, "ADD", 2);
    jamlisp_load_opcode(ctx, JAMLISP_OPCODE_CONS, "CONS", 2);
    jamlisp_load_opcode(ctx, JAMLISP_OPCODE_PRINT, "PRINT", 1);
    jamlisp_load_opcode(ctx, JAMLISP_OPCODE_INT, "INT", 0);
  }
  {
    u8 code[] = {JAMLISP_OPCODE_ADD, JAMLISP_MAGIC, JAMLISP_OPCODE_LOCAL, 0, JAMLISP_MAGIC,JAMLISP_OPCODE_LOCAL, 1, JAMLISP_MAGIC};
    jamlisp_load_fcn_bytecode(ctx, jamlisp_symbol(ctx, "+"), code, array_count(code));
  }
  
  return ctx;
}

jamlisp_object jamlisp_symbol(jamlisp_context * ctx, const char * name){
  jamlisp_object out = {0};
  out.type = JAMLISP_SYMBOL;
  if(ht_get(ctx->symbol_names, &name, &out.symbol))
    return out;
  out.symbol = ++ctx->symbol_counter;
  ht_set(ctx->symbol_names, &name, &out.symbol);
  return out;
}


#ifdef NO_STDLIB
void  memcpy(void *, const void *, unsigned long);
void * realloc(void *, unsigned long);
void memset(void *, int chr, unsigned long );
#endif

void jamlisp_push(jamlisp_context * ctx, jamlisp_object obj){
  stack_push(&ctx->value_stack, &obj, sizeof(obj));
}

void jamlisp_push_i64(jamlisp_context * ctx, i64 value){
  jamlisp_object v =
    {
     .type = JAMLISP_INT64,
     .int64 = value
    };
  
  jamlisp_push(ctx, v);
}

void jamlisp_push_symbol(jamlisp_context * ctx, u32 value){
  jamlisp_object v =
    {
     .type = JAMLISP_SYMBOL,
     .int64 = value
    };
  
  jamlisp_push(ctx, v);
}


jamlisp_object jamlisp_pop(jamlisp_context * ctx){
  jamlisp_object idx;
  stack_pop(&ctx->value_stack, &idx, sizeof(idx));
  return idx;
}

i64 jamlisp_pop_i64(jamlisp_context * ctx){
  var od = jamlisp_pop(ctx);
  ASSERT(od.type == JAMLISP_INT64);
  return od.int64;
}

bool jamlisp_nilp(jamlisp_object obj){
  return obj.type == JAMLISP_NIL;
}

bool jamlisp_symbolp(jamlisp_object obj){
  return obj.type == JAMLISP_SYMBOL;
}

bool jamlisp_consp(jamlisp_object obj){
  return obj.type == JAMLISP_CONS;
}

jamlisp_object jamlisp_i64(i64 v){
  return (jamlisp_object) {.type = JAMLISP_INT64, .int64 = v}; 
}
jamlisp_object jamlisp_i32(i32 v){
  return (jamlisp_object) {.type = JAMLISP_INT32, .fixnum = v}; 
}
jamlisp_object jamlisp_f32(f32 v){
  return (jamlisp_object) {.type = JAMLISP_F32, .float32 = v}; 
}
jamlisp_object jamlisp_f64(f64 v){
  return (jamlisp_object) {.type = JAMLISP_F64, .float64 = v}; 
}
jamlisp_object jamlisp_nil(){
  return (jamlisp_object) {0}; 
}
jamlisp_object jamlisp_new_object(){
  return jamlisp_nil();
}

jamlisp_object jamlisp_new_cons(jamlisp_context * ctx){
  jamlisp_object_index idx = new_object(&ctx->heap);
  return (jamlisp_object){.type = JAMLISP_CONS, .cons = idx};
}
void jamlisp_free_cons(jamlisp_context * ctx, jamlisp_object * cons){
  ASSERT(jamlisp_consp(*cons));
  free_object(&ctx->heap, cons->cons);
  *cons = jamlisp_nil();
}



jamlisp_object jamlisp_add(jamlisp_object a, jamlisp_object b){
  jamlisp_object v = {0};
  if(a.type == JAMLISP_INT64 && b.type == JAMLISP_INT64){
    v.type = a.type;
    v.int64 = a.int64 + b.int64;
    return v;
  }
  ERROR("Unsupported ADD!\n");
  return v;
}

void jamlisp_print(jamlisp_object obj){
  switch(obj.type){
  case JAMLISP_INT64:
    logd("%lld", obj.int64);
    break;
  case JAMLISP_INT32:
    logd("%i", obj.fixnum);
    break;
  default:
    logd("OBJECT(%i)", obj.type);
  }
}

void jamlisp_load_opcode(jamlisp_context * ctx, jamlisp_opcode op, const char * name, size_t arg_count){
    
  if(ht_get(ctx->opcode_names, &name, NULL)){
    ERROR("Opcode '%s' is already defined\n", name);
    return;
  }
  ht_set(ctx->opcode_names, &name, &op);
  jamlisp_opcodedef h;
  h.opcode_name = name;
  h.arg_count = arg_count;
  h.opcode = op;
  if(ctx->opcodedef_count <= op){
    ctx->opcodedefs = realloc(ctx->opcodedefs, sizeof(ctx->opcodedefs[0]) * op * 2);
    ctx->opcodedef_count = op * 2;
  }
  ctx->opcodedefs[op] = h;
}


jamlisp_opcodedef jamlisp_get_opcodedef(jamlisp_context * ctx, jamlisp_opcode opcode){
  ASSERT(opcode < ctx->opcodedef_count);
  return ctx->opcodedefs[opcode];   
}

jamlisp_opcode jamlisp_current_opcode(jamlisp_context * ctx){
  return ctx->current_opcode;
}

typedef struct{
  jamlisp_opcode opcode;
  const char * name;
}jamlisp_opcode_name2;

const char * jamlisp_opcode_name(jamlisp_context * ctx, jamlisp_opcode opcode){
  if(opcode >= ctx->opcodedef_count){
    ERROR("Unrecognized opcode %i\n", opcode);
    return NULL;
  }
  return ctx->opcodedefs[opcode].opcode_name;
}

jamlisp_opcode jamlisp_opcode_parse(jamlisp_context * ctx, const char * name){
  jamlisp_opcode opcode;
  if(!ht_get(ctx->opcode_names, &name, &opcode)){
    return JAMLISP_OPCODE_NONE;
  }  
  return opcode;
}

void jamlisp_iterate_internal(jamlisp_context * ctx, io_reader * reader){
  if(NULL == ctx->frames){
    ctx->frames = alloc0(sizeof(ctx->frames[0]) * 64);
    ctx->frames_capacity = 64;
  }
  
  while(true){

    if(ctx->frame_index == ctx->frames_capacity){
      ctx->frames_capacity *= 2;
      ctx->frames = realloc(ctx->frames, sizeof(ctx->frames[0]) * ctx->frames_capacity);
    }
    var frame = ctx->frames + ctx->frame_index;
    frame[0] = (stack_frame){0};
    frame->node_id = reader->offset;
    if(reader->offset == reader->size)
      break;
    ctx->current_opcode = frame->opcode = io_read_u64_leb(reader);
    logd("OPCODE %u\n", frame->opcode);
    if(frame->opcode == JAMLISP_OPCODE_NONE){
      break;
    }

    frame->child_count = ctx->opcodedefs[frame->opcode].arg_count;
    
    switch(frame->opcode){
    case JAMLISP_OPCODE_NONE:
      ERROR("INVALID OPCODE");
      return;
    case JAMLISP_OPCODE_ADD:
    case JAMLISP_OPCODE_SUB:
    case JAMLISP_OPCODE_MUL:
    case JAMLISP_OPCODE_DIV:
    case JAMLISP_OPCODE_CONS:
      break;
    case JAMLISP_OPCODE_INT:
      logd("ENTER INT\n");
      jamlisp_push_i64(ctx, io_read_i64_leb(reader));
      frame->child_count = 0;
      break;
    case JAMLISP_OPCODE_CALL:
      frame->call = io_read_u32_leb(reader);
      frame->child_count = io_read_u32_leb(reader);
      logd("ENTER CALL %i %i\n", frame->call, frame->child_count);
      break;
    default:
      ERROR("No Handler for opcode!");  
    }
    
    var magic = io_read_u64_leb(reader);
    if(magic != JAMLISP_MAGIC){
      logd("ERROR Expected magic! got: %i\n", magic);
      break;
    }
    
    // if children >
    while(true){
      if(frame->child_count > 0){
	frame = frame + 1;
	break;
      }
      else
	{
	  
	  switch(frame->opcode){
	  case JAMLISP_OPCODE_ADD:
	    {
	      var a = jamlisp_pop(ctx);
	      var b = jamlisp_pop(ctx);
	      var c = jamlisp_add(a, b);
	      jamlisp_push(ctx, c);
	    }
	    break;
	  case JAMLISP_OPCODE_CALL:
	    {
	      logd("EXIT CALL %i \n", frame->call);
	      jamlisp_object s = {.type = JAMLISP_SYMBOL, .symbol = frame->call};
	      s = symbol_get_value(ctx, s);
	      logd("EXIT CALL %i %i\n", s.type, s.ptr->type);
	      io_reader rd2 = {.offset = 0, .data = s.ptr->data, .size = s.ptr->size};
	      jamlisp_iterate_internal(ctx, &rd2);
	      
	    }
	    break;

	  case JAMLISP_OPCODE_PRINT:
	    {
	      var a = jamlisp_pop(ctx);
	      jamlisp_print(a);
	    }
	    break;
	  default:
	    break;
	  }
	  
	if(frame == ctx->frames)
	  break;
	frame = frame - 1;
	frame->child_count -= 1;
      }
    }
  }
}

void jamlisp_iterate(jamlisp_context * reg, io_reader * reader){
  jamlisp_iterate_internal(reg, reader);
}
		   
void jamlisp_test_load(jamlisp_context * ctx, io_writer * wd){
  jamlisp_opcode JAMLISP_ADD = jamlisp_opcode_parse(ctx, "ADD");
  jamlisp_opcode JAMLISP_INT = jamlisp_opcode_parse(ctx, "INT");
  ASSERT(JAMLISP_ADD == JAMLISP_OPCODE_ADD);
  io_write_u8(wd, JAMLISP_OPCODE_PRINT);
  io_write_u8(wd, JAMLISP_MAGIC);
  io_write_u8(wd, JAMLISP_ADD);
  io_write_u8(wd, JAMLISP_MAGIC);
  io_write_u8(wd, JAMLISP_INT);
  io_write_i32_leb(wd, 10);
  io_write_u8(wd, JAMLISP_MAGIC);
  io_write_u8(wd, JAMLISP_INT);
  io_write_i32_leb(wd, 10);
  io_write_u8(wd, JAMLISP_MAGIC);
  wd->size = wd->offset;
  wd->offset = 0;
}
