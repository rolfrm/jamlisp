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

void free_object(object_heap * heap, jamlisp_object_index obj){
  heap->object_heap[obj].type = JAMLISP_CONS;
  heap->object_heap[obj].c.cdr = heap->free_object;
  heap->free_object = obj;
}

jamlisp_object_index new_object(object_heap * heap){
  if(heap->free_object == 0){
    size_t new_count = (heap->heap_size + 2) * 1.5;
    heap->object_heap = realloc(heap->object_heap, new_count * sizeof(heap->object_heap[0]));
    for(size_t i = heap->heap_size; i < new_count; i++){
      free_object(heap, i);
    }
    heap->heap_size = new_count;
  }
  jamlisp_object_index out = heap->free_object;
  heap->free_object = heap->object_heap[out].c.cdr;
  return out;
}

jamlisp_object object_get(const object_heap * heap, jamlisp_object_index idx){
  return heap->object_heap[idx];
}

jamlisp_object_index new_clone(object_heap * heap, jamlisp_object prototype){
  var idx = new_object(heap);
  heap->object_heap[idx] = prototype;
  return idx;
}

typedef jamlisp_stack_frame stack_frame;

struct _jamlisp_context {
  void * registers;
  u64 reg_ptr;
  u64 reg_cap;

  jamlisp_opcodedef * opcodedefs;
  
  size_t opcodedef_count;

  jamlisp_opcode current_opcode;

  hash_table * opcode_names;
  
  stack_frame * stack;
  size_t stack_capacity;
  
  object_heap heap;

  stack value_stack;
};


void jamlisp_free(jamlisp_context * ctx, jamlisp_object_index obj){
  free_object(&ctx->heap, obj);
}

jamlisp_object_index jamlisp_new_object(jamlisp_context * ctx){
  return new_object(&ctx->heap);
}
jamlisp_object_index jamlisp_new_object2(jamlisp_context * ctx, jamlisp_object prototype){
  return new_clone(&ctx->heap, prototype);
}


jamlisp_context * jamlisp_new(){
  jamlisp_context * ctx = alloc0(sizeof(*ctx));
  ctx->opcode_names = ht_create_strkey(sizeof(jamlisp_opcode));
  {
    jamlisp_load_opcode(ctx, JAMLISP_OPCODE_ADD, "ADD", 2);
    jamlisp_load_opcode(ctx, JAMLISP_OPCODE_CONS, "CONS", 2);
    jamlisp_load_opcode(ctx, JAMLISP_OPCODE_PRINT, "PRINT", 1);
    jamlisp_load_opcode(ctx, JAMLISP_OPCODE_INT, "INT", 0);
  }

  return ctx;
}

#ifdef NO_STDLIB
void  memcpy(void *, const void *, unsigned long);
void * realloc(void *, unsigned long);
void memset(void *, int chr, unsigned long );
#endif

void jamlisp_push_object(jamlisp_context * ctx, jamlisp_object obj){
  var idx = jamlisp_new_object2(ctx, obj);
  stack_push(&ctx->value_stack, &idx, sizeof(idx));
}

void jamlisp_push_i64(jamlisp_context * ctx, i64 value){
  jamlisp_object v =
    {
     .type = JAMLISP_INT64,
     .large_int = value
    };
  
  jamlisp_push_object(ctx, v);
}

jamlisp_object_index jamlisp_pop(jamlisp_context * ctx){
  jamlisp_object_index idx;
  stack_pop(&ctx->value_stack, &idx, sizeof(idx));
  return idx;
}

jamlisp_object jamlisp_pop_object(jamlisp_context * ctx){
  var o = jamlisp_pop(ctx);
  var od = object_get(&ctx->heap, o);
  free_object(&ctx->heap, o);
  return od;
}

i64 jamlisp_pop_i64(jamlisp_context * ctx){
  var od = jamlisp_pop_object(ctx);
  ASSERT(od.type == JAMLISP_INT64);
  return od.large_int;
}

jamlisp_object jamlisp_add(jamlisp_object a, jamlisp_object b){
  jamlisp_object v = {0};
  if(a.type == JAMLISP_INT64 && b.type == JAMLISP_INT64){
    v.type = a.type;
    v.large_int = a.large_int + b.large_int;
    return v;
  }
  ERROR("Unsupported ADD!\n");
  return v;
}

void jamlisp_print(jamlisp_object obj){
  switch(obj.type){
  case JAMLISP_INT64:
    logd("%lld", obj.large_int);
    break;
  case JAMLISP_INT32:
    logd("%i", obj.fixnum);
    break;
  default:
    logd("OBJECT(%i)", obj.type);
  }
}

stack * jamlisp_get_register(jamlisp_context * ctx, jamlisp_register * registerID){
  static u32 register_counter = 1;
  if(registerID->id == 0){
    registerID->id = register_counter;
    register_counter += registerID->size;
  }
  
  while(registerID->id + registerID->size >= ctx->reg_cap){
    u32 new_size = (registerID->id + registerID->size) * 2;
    ctx->registers = realloc(ctx->registers, new_size);
    // set all new registers to 0.
    memset(ctx->registers + ctx->reg_cap, 0, new_size - ctx->reg_cap);
    ctx->reg_cap = new_size;
  }
  
  // this assert is for catching possibily errors early. New registers should not be created very often.
  // in the future remove this.
  ASSERT(ctx->reg_cap < 10000); 
  return ctx->registers + registerID->id;
}

void jamlisp_stack_register_push(jamlisp_context * ctx, jamlisp_stack_register * reg, const void * value){
  reg->stack.size = sizeof(stack);
  stack * stk = jamlisp_get_register(ctx, &reg->stack);
  stack_push(stk, value, reg->size);
}

void jamlisp_stack_register_pop(jamlisp_context * ctx, jamlisp_stack_register * reg, void * value){
  reg->stack.size = sizeof(stack);
  stack * stk = jamlisp_get_register(ctx, &reg->stack);
  stack_pop(stk, value, reg->size);
}

bool jamlisp_stack_register_top(jamlisp_context * ctx, jamlisp_stack_register * reg, void * value){
  reg->stack.size = sizeof(stack);
  stack * stk = jamlisp_get_register(ctx, &reg->stack);
  if(stk->count == 0) return false;
  if(value != NULL)
    stack_top(stk, value, reg->size);
  return true;
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

jamlisp_stack_register node_callback_reg = {.size = sizeof(node_callback), .stack = {0}};

void node_callback_push(jamlisp_context * ctx, node_callback callback){
  jamlisp_stack_register_push(ctx, &node_callback_reg, &callback);
}

void node_callback_pop(jamlisp_context * ctx){
  jamlisp_stack_register_pop(ctx, &node_callback_reg, NULL);
}

node_callback node_callback_get(jamlisp_context * ctx){
  node_callback callback = {0};
  jamlisp_stack_register_top(ctx, &node_callback_reg, &callback);
  return callback;
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
  
  stack_frame ** frames = &ctx->stack;
  size_t * stack_capacity = &ctx->stack_capacity;

  stack_frame * frame = *frames; // select the first frame.
  stack_frame * end_frame = *frames + *stack_capacity;
  while(true){

    if(frame == end_frame){
      *stack_capacity = *stack_capacity == 0 ? 64 : *stack_capacity * 2;
      *frames = realloc(*frames, sizeof(*frame) * *stack_capacity);
      frame = *frames + (end_frame - frame);
      end_frame = *frames + *stack_capacity;
    }
    *frame = (stack_frame){0};
    
    frame->node_id = reader->offset;
    if(reader->offset == reader->size)
      break;
    ctx->current_opcode = frame->opcode = io_read_u64_leb(reader);
    
    if(frame->opcode == JAMLISP_OPCODE_NONE){
      break;
    }

    frame->child_count = ctx->opcodedefs[frame->opcode].arg_count;
    i64 value;

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
      value = io_read_i64_leb(reader);
      jamlisp_push_i64(ctx, value);
      frame->child_count = 0;
      break;
      //default:
      //ERROR("No Handler for opcode!");  
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
	      var a = jamlisp_pop_object(ctx);
	      var b = jamlisp_pop_object(ctx);
	      var c = jamlisp_add(a, b);
	      jamlisp_push_object(ctx, c);
	    }
	    break;
	  case JAMLISP_OPCODE_PRINT:
	    {
	      var a = jamlisp_pop_object(ctx);
	      jamlisp_print(a);
	    }
	    break;
	  default:
	    break;
	  }
	  
	if(frame == &(*frames)[0])
	  break;
	frame = frame - 1;
	frame->child_count -= 1;
      }
    }
  }
}
/*
void jamlisp_3d_init(jamlisp_context * ctx);

void handle_opcode(jamlisp_context * registers, void * userdata){
  UNUSED(registers, userdata);
}

*/
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
		     /*
typedef struct{
  jamlisp_context * ctx;
  int stack_level;
  u64 last_id;
  bool prev_enter;
  io_writer * wd;

}test_render_context;

void test_after_enter(stack_frame * frame, void * userdata){
  test_render_context * ctx = userdata;
  ctx->last_id = frame->node_id;
  ctx->prev_enter = true;
  var wd = ctx->wd;
  io_write_str(wd, "\n");
  for(int i = 0; i < ctx->stack_level; i++)
    io_write_str(wd, " ");
  io_write_fmt(wd, "(%s", jamlisp_opcode_name(ctx->ctx, ctx->ctx->current_opcode));
  var reg = ctx->ctx;
  ASSERT(frame->opcode < ctx->opcodedef_count);
  var handler = ctx->opcodedefs[frame->opcode];
  for(u32 i = 0; i < handler.typesig_count; i++){
    let typesig = handler.typesig[i];
    switch(typesig.signature)
      {
      case JAMLISP_F32A:
	{
	  f32_array array;
	  ASSERT(jamlisp_stack_register_top(reg, typesig.reg, &array));
	  for(size_t i = 0; i < array.count; i++){
	    io_write_fmt(wd, " %g", array.array[i]);
	  }
	}
	break;
      case JAMLISP_STRING:
	{
	  char * t;
	  ASSERT(jamlisp_stack_register_top(reg, typesig.reg, &t));
	  io_write_fmt(wd, " \"%s\"", t);
	  break;
	}
      case JAMLISP_F32:
	{
	  f32 t;
	  ASSERT(jamlisp_stack_register_top(reg, typesig.reg, &t));
	  io_write_fmt(wd, " %g", t);
	  break;
	}
      case JAMLISP_F64:
	{
	  f64 t;
	  ASSERT(jamlisp_stack_register_top(reg, typesig.reg, &t));
	  io_write_fmt(wd, " %g", t);
	  break;
	}
      case JAMLISP_VEC2:
	{
	  vec2 t;
	  ASSERT(jamlisp_stack_register_top(reg, typesig.reg, &t));
	  io_write_fmt(wd, " %g %g", t.x, t.y);
	  break;
	}
      case JAMLISP_VEC3:
	{
	  vec3 t;
	  ASSERT(jamlisp_stack_register_top(reg, typesig.reg, &t));
	  io_write_fmt(wd, " %g %g %g", t.x, t.y, t.z);
	  break;
	}
      case JAMLISP_VEC4:
	{
	  vec4 t;
	  ASSERT(jamlisp_stack_register_top(reg, typesig.reg, &t));
	  io_write_fmt(wd, " %g %g %g %g", t.x, t.y, t.z, t.w);
	  break;
	}
      case JAMLISP_UINT32:
	{
	  u32 t;
	  ASSERT(jamlisp_stack_register_top(reg, typesig.reg, &t));
	  io_write_fmt(wd, " 0x%x", t);
	  break;
	}

      default:
	ERROR("UNSUPPORTED\n");
	break;
      }
  }
  
  ctx->stack_level += 1;
}

void test_before_exit(stack_frame * frame, void * userdata){
  test_render_context * ctx = userdata;
  var wd = ctx->wd;
  
  if(frame->node_id == ctx->last_id){
    io_write_str(wd, ")");
    ctx->stack_level -= 1;
    return;
  }
  if(ctx->prev_enter){
    ctx->prev_enter = false;
  }io_write_str(wd, ")");
  
  ctx->stack_level -= 1;
}
void test_jamlisp_load_lisp();
io_reader io_from_bytes(const void * bytes, size_t size);
void test_write_lisp(jamlisp_context * reg, void * buffer, size_t size){
  
  io_writer wd = {0};
  test_render_context rctx = {.ctx = reg, .stack_level = 0 , .wd = &wd};
  
  node_callback cb = {.after_enter = test_after_enter,
		      .before_exit = test_before_exit,
		      .userdata = &rctx};
  node_callback_push(reg, cb);
  
  io_reader rd = io_from_bytes(buffer, size);
  jamlisp_iterate(reg, &rd);
  io_write_u8(&wd, 0);
  logd("%s", wd.data);
  logd("\n");
  node_callback_pop(reg);
  
}

void jamlisp_test(){
  binary_io buffer = {0};
  lisp_to_bytes_str("(cons 1 2)", &buffer);
  io_writer buffer2 = {0};
  buffer.length = buffer.offset;
  buffer.offset = 0;
  bytes_to_lisp(&buffer, &buffer2);

  }*/
