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

#include "binui.h"


typedef binui_stack_frame stack_frame;

struct _binui_context {
  void * registers;
  u64 reg_ptr;
  u64 reg_cap;

  binui_opcodedef * opcodedefs;
  
  size_t opcodedef_count;

  binui_opcode current_opcode;

  hash_table * opcode_names;

  stack_frame * stack;
  size_t stack_capacity;
  
};

#ifdef NO_STDLIB
void  memcpy(void *, const void *, unsigned long);
void * realloc(void *, unsigned long);
void memset(void *, int chr, unsigned long );
#endif

typedef struct{
  void * elements;
  size_t capacity;
  size_t count;
}stack;

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

stack * binui_get_register(binui_context * ctx, binui_register * registerID){
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


void binui_stack_register_push(binui_context * ctx, binui_stack_register * reg, const void * value){
  reg->stack.size = sizeof(stack);
  stack * stk = binui_get_register(ctx, &reg->stack);
  stack_push(stk, value, reg->size);
}

void binui_stack_register_pop(binui_context * ctx, binui_stack_register * reg, void * value){
  reg->stack.size = sizeof(stack);
  stack * stk = binui_get_register(ctx, &reg->stack);
  stack_pop(stk, value, reg->size);
}

bool binui_stack_register_top(binui_context * ctx, binui_stack_register * reg, void * value){
  reg->stack.size = sizeof(stack);
  stack * stk = binui_get_register(ctx, &reg->stack);
  if(stk->count == 0) return false;
  if(value != NULL)
    stack_top(stk, value, reg->size);
  return true;
}

binui_stack_register color_register ={.size =sizeof(u32), .stack = {0}};
void push_color(binui_context * ctx, u32 color){
  binui_stack_register_push(ctx, &color_register, &color);
}

u32 pop_color(binui_context * ctx){
  u32 color;
  binui_stack_register_pop(ctx, &color_register, &color);
  return color;
}

u32 get_color(binui_context * ctx){
  u32 color;
  binui_stack_register_top(ctx, &color_register, &color);
  return color;
}

void binui_get_color(binui_context * ctx, u32 *color){
  *color = get_color(ctx);
}

void color_enter(binui_context * ctx){
  UNUSED(ctx);
}

void color_exit(binui_context * ctx){
  UNUSED(ctx);
}

static binui_stack_register position_reg ={.size =sizeof(vec2), .stack = {0}};

vec2 position_get(binui_context * ctx){
  vec2 get;
  if(!binui_stack_register_top(ctx, &position_reg, &get)){
    return vec2_zero;
  }
  return get;
}

void position_push(binui_context * ctx, vec2i pos){
  vec2i pos0 = {0};
  binui_stack_register_top(ctx, &position_reg, &pos0);
  pos = vec2i_add(pos, pos0);
  binui_stack_register_push(ctx, &position_reg, &pos);
}

void position_pop(binui_context * ctx){
  binui_stack_register_pop(ctx, &position_reg, NULL);
}

void position_enter(binui_context * ctx){
  UNUSED(ctx);
}

void position_exit(binui_context * ctx){
  UNUSED(ctx);
}

binui_stack_register size_reg = {.size = sizeof(vec2i), .stack = {0}};

void size_get(binui_context * ctx, vec2i * get){
  if(!binui_stack_register_top(ctx, &size_reg, get))
    *get = vec2i_zero;
}

void size_enter(binui_context * ctx){
  UNUSED(ctx);
}

void size_exit(binui_context * ctx){
  UNUSED(ctx);
}
void binui_get_size(binui_context * ctx, vec2i * get){
  size_get(ctx, get);
}

static binui_stack_register module_name_register ={.size = sizeof(char *), .stack = {0}};

char * get_module_name(binui_context * ctx){
  char * module;
  if(binui_stack_register_top(ctx, &module_name_register, &module))
    return module;
  return NULL;
}

void module_enter(binui_context * ctx){
  UNUSED(ctx);
  //char * mod = get_module_name(ctx);
  //logd("MODULE: %s\n", mod);
}

void rectangle_enter(binui_context * ctx){
  UNUSED(ctx); 
}

void canvas_enter(binui_context * ctx){
  UNUSED(ctx);
}

void _binui_set_opcodedef(binui_opcode opcode, binui_context * ctx, binui_opcodedef handler){
  handler.opcode = opcode;
  if(ctx->opcodedef_count <= opcode){
    ctx->opcodedefs = realloc(ctx->opcodedefs, sizeof(handler) * (opcode + 1));
    
    for(size_t i = ctx->opcodedef_count; i <= opcode; i++){
      ctx->opcodedefs[i] = (binui_opcodedef){0};
    }
    ctx->opcodedef_count = opcode + 1;
  }
  ctx->opcodedefs[opcode] = handler;
}

void binui_load_opcode(binui_context * ctx, const char * name, const binui_auto_type * type, size_t type_count, void (* enter) (binui_context * ctx), void (* exit) (binui_context * ctx), bool children){
  
    
    if(ht_get(ctx->opcode_names, &name, NULL)){
      ERROR("Opcode '%s' is already defined\n", name);
      return;
    }
    u32 newid = ctx->opcodedef_count;
    ht_set(ctx->opcode_names, &name, &newid);
    binui_opcodedef h;
    h.opcode_name = name;
    h.has_children = children;
    h.enter = enter;
    h.exit = exit;
    h.typesig = type;
    h.typesig_count = type_count;
    h.opcode = newid;
      
    _binui_set_opcodedef(newid, ctx, h);
}

binui_opcodedef binui_get_opcodedef(binui_context * ctx, binui_opcode opcode){
  ASSERT(opcode < ctx->opcodedef_count);
  return ctx->opcodedefs[opcode];   
}

binui_opcode binui_current_opcode(binui_context * ctx){
  return ctx->current_opcode;
}

binui_stack_register node_callback_reg = {.size = sizeof(node_callback), .stack = {0}};

void node_callback_push(binui_context * ctx, node_callback callback){
  binui_stack_register_push(ctx, &node_callback_reg, &callback);
}

void node_callback_pop(binui_context * ctx){
  binui_stack_register_pop(ctx, &node_callback_reg, NULL);
}

node_callback node_callback_get(binui_context * ctx){
  node_callback callback = {0};
  binui_stack_register_top(ctx, &node_callback_reg, &callback);
  return callback;
}

const u64 escape_hatch64 = 0x2835a751f009041cUL;
bool escape_hatch_activated(io_reader * reader){
  u64 peeked = io_peek_u64(reader);
  return peeked == escape_hatch64;
}

void enter_typesig(binui_context * reg, io_reader * reader, const binui_auto_type * type){
  union{
    void * ptr;
    vec4 v4;
    vec3 v3;
    vec2 v2;
    f32 f;
    f64 f2;
    u32 u;
    f32_array f32a;
  }data;
  if(escape_hatch_activated(reader)){
    io_advance(reader, sizeof(u64));
    if(io_peek_u64(reader) != escape_hatch64){
      u64 variable = io_read_u64_leb(reader);
      logd("Variable: %i\n", variable);
      ERROR("Variables are not supported");
    }else{
      // the escape-hatch was unescaped. This occurs in the case
      // that was actually the selected values.
      // almost impossible, but it can occur.
    }
  }
  data.ptr = NULL;
  switch(type->signature){
  case BINUI_STRING:
    {
      u32 len = 0;
      data.ptr = io_read_strn(reader, &len);
      data.ptr = realloc(data.ptr, len + 1);
      ((char*) data.ptr)[len] = 0;
      break;
    }
  case BINUI_VEC2:
    {
      data.v2 = io_read_vec2(reader);
      break;
    }
  case BINUI_VEC3:
    {
      data.v3 = io_read_vec3(reader);
      break;
    }
  case BINUI_VEC4:
    {
      data.v4 = io_read_vec4(reader);
      break;
    }
  case BINUI_F32:
    {
      data.f = io_read_f32(reader);
      break;
    }
  case BINUI_F64:
    {
      data.f = io_read_f64(reader);
      break;
    }
  case BINUI_F32A:
    {
      data.f32a.count = io_read_u64_leb(reader);
      data.f32a.array = alloc(data.f32a.count * sizeof(f32));
      io_read(reader, data.f32a.array, data.f32a.count * sizeof(f32));
      break;
    }
  case BINUI_TYPE_NONE:
    {
      return;
    }
  case BINUI_UINT32:
    {
      data.u = io_read_u32(reader);
      break;
    }
    break;
    
  default:
    ERROR("Cannot handle type\n");
  }
  binui_stack_register_push(reg, type->reg, &data);
}

void exit_typesig(binui_context * reg, const binui_auto_type * type){
  
  if(type->signature == BINUI_TYPE_NONE) return;
  if(type->signature == BINUI_STRING){
    void * data;
    binui_stack_register_pop(reg, type->reg, &data);
  
    free(data);
  }else{
    binui_stack_register_pop(reg, type->reg, NULL);  
  }
}

void binui_iterate_internal(binui_context * reg, io_reader * reader){
  
  stack_frame ** frames = &reg->stack;
  size_t * stack_capacity = &reg->stack_capacity;

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
    reg->current_opcode = frame->opcode = io_read_u64_leb(reader);
    
    if(frame->opcode == BINUI_OPCODE_NONE){
      break;
    }
    
    if(reg->opcodedef_count > frame->opcode){
      var handler = reg->opcodedefs[frame->opcode];
      for(u32 i = 0; i < handler.typesig_count; i++){
	enter_typesig(reg, reader, handler.typesig + i);
      }
      if(handler.enter != NULL)
	handler.enter(reg);
      if(handler.has_children)
	frame->child_count = io_read_u64_leb(reader);
    }else{
      ERROR("No handler for opcode!");
    }
    
    var magic = io_read_u64_leb(reader);
    if(magic != BINUI_MAGIC){
      logd("ERROR Expected magic! got: %i\n", magic);
      break;
    }
    var cb = node_callback_get(reg);
    
    if(cb.after_enter != NULL){
      cb.after_enter(frame, cb.userdata);
    }
    
    // if children >
    while(true){
      if(frame->child_count > 0){
	frame = frame + 1;
	break;
      }
      else
	{
	var handler = reg->opcodedefs[frame->opcode];
	var cb = node_callback_get(reg);
	if(cb.before_exit != NULL){
	  cb.before_exit(frame, cb.userdata);
	}

	for(u32 i = 0; i < handler.typesig_count; i++){
	  exit_typesig(reg, handler.typesig + i);
	}
      
	if(handler.exit != NULL){
	  handler.exit(reg);
	}
	if(frame == &(*frames)[0])
	  break;
	frame = frame - 1;
	frame->child_count -= 1;
      }
    }
  }
}

void binui_3d_init(binui_context * ctx);
binui_context * binui_new(){
  binui_context * ctx = alloc0(sizeof(*ctx));
  ctx->opcode_names = ht_create_strkey(sizeof(u32));
  {
    static binui_auto_type type;
    type.signature = BINUI_STRING;
    type.reg = &module_name_register;
    binui_load_opcode(ctx, "__import__", &type, 1, module_enter, NULL, false);
  }
  {
    static binui_auto_type type;
    type.signature = BINUI_STRING;
    type.reg = &module_name_register;
    binui_load_opcode(ctx, "import", &type, 1, module_enter, NULL, false);
  }
  
  {
    static binui_auto_type type;
    type.signature = BINUI_UINT32;
    type.reg = &color_register;
    binui_load_opcode(ctx, "color", &type, 1, color_enter, color_exit, true);
  }
  {
    static binui_auto_type type;
    type.signature = BINUI_VEC2;
    type.reg = &size_reg;
    binui_load_opcode(ctx, "size", &type, 1, size_enter, size_exit, true);
  }

  {
    static binui_auto_type type;
    type.signature = BINUI_VEC2;
    type.reg = &position_reg;
    binui_load_opcode(ctx, "position", &type, 1, position_enter, position_exit, true);
  }
  {
    binui_load_opcode(ctx, "canvas", NULL, 0, canvas_enter, NULL, true);
    binui_load_opcode(ctx, "rectangle", NULL, 0, canvas_enter, NULL, false);
  }

  binui_3d_init(ctx);
  return ctx;
}

typedef struct{
  binui_opcode opcode;
  const char * name;
}binui_opcode_name2;


const char * binui_opcode_name(binui_context * ctx, binui_opcode opcode){
  if(opcode >= ctx->opcodedef_count){
    ERROR("Unrecognized opcode %i\n", opcode);
    return NULL;
  }
  return ctx->opcodedefs[opcode].opcode_name;
}

binui_opcode binui_opcode_parse(binui_context * ctx, const char * name){
  binui_opcode opcode;
  if(!ht_get(ctx->opcode_names, &name, &opcode)){
    return BINUI_OPCODE_NONE;
  }
  
  return opcode;
}

void handle_opcode(binui_context * registers, void * userdata){
  UNUSED(registers, userdata);
}

void binui_iterate(binui_context * reg, io_reader * reader){
  binui_iterate_internal(reg, reader);
}

void binui_test_load(binui_context * ctx, io_writer * wd){
  binui_opcode BINUI_IMPORT_MODULE = binui_opcode_parse(ctx, "import");
  
  binui_opcode BINUI_CANVAS = binui_opcode_parse(ctx, "canvas");
  
  binui_opcode BINUI_POSITION = binui_opcode_parse(ctx, "position");
  binui_opcode BINUI_COLOR = binui_opcode_parse(ctx, "color");
  binui_opcode BINUI_SIZE = binui_opcode_parse(ctx, "size");
  binui_opcode BINUI_RECTANGLE = binui_opcode_parse(ctx, "rectangle");
  binui_opcode BINUI_3D_POLYGON = binui_opcode_parse(ctx, "polygon");
  binui_opcode BINUI_3D = binui_opcode_parse(ctx, "3d");
 

  io_write_u32_leb(wd, BINUI_IMPORT_MODULE);
 

 const char * modname = "graphics";
  io_write_strn(wd, modname);
  io_write_u32_leb(wd, BINUI_MAGIC);

  io_write_u32_leb(wd, BINUI_CANVAS);
  io_write_u32_leb(wd, 1);
  io_write_u32_leb(wd, BINUI_MAGIC);
  io_write_u32_leb(wd, BINUI_COLOR);
  io_write_u32(wd, 0xFF0000FF);
  io_write_u32_leb(wd, 1);
  io_write_u32_leb(wd, BINUI_MAGIC);
  io_write_u32_leb(wd, BINUI_POSITION);
  io_write_i32_leb(wd, 200);
  io_write_i32_leb(wd, 200);
  io_write_u32_leb(wd, 1);
  io_write_u32_leb(wd, BINUI_MAGIC);

  io_write_u32_leb(wd, BINUI_SIZE);
  io_write_i32_leb(wd, 15);
  io_write_i32_leb(wd, 15);
  io_write_u32_leb(wd, 1);
  io_write_u32_leb(wd, BINUI_MAGIC);
  
  
  io_write_u32_leb(wd, BINUI_SIZE);
  io_write_i32_leb(wd, 10);
  io_write_i32_leb(wd, 10);
  io_write_u32_leb(wd, 2);
  io_write_u32_leb(wd, BINUI_MAGIC);

  io_write_u32_leb(wd, BINUI_POSITION);
  io_write_i32_leb(wd, 5);
  io_write_i32_leb(wd, 5);
  io_write_u32_leb(wd, 1);
  io_write_u32_leb(wd, BINUI_MAGIC);
  
  
  io_write_u32_leb(wd, BINUI_SIZE);
  io_write_i32_leb(wd, 50);
  io_write_i32_leb(wd, 50);
  io_write_u32_leb(wd, 1);
  io_write_u32_leb(wd, BINUI_MAGIC);
  
  io_write_u32_leb(wd, BINUI_RECTANGLE);
  io_write_u32_leb(wd, BINUI_MAGIC);
  
  io_write_u32_leb(wd, BINUI_COLOR);
  io_write_u32(wd, 0xFFFFFFFF);
  io_write_u32_leb(wd, 2);
  io_write_u32_leb(wd, BINUI_MAGIC);
  
  io_write_u32_leb(wd, BINUI_RECTANGLE);
  io_write_u32_leb(wd, BINUI_MAGIC);

  
  io_write_u32_leb(wd, BINUI_POSITION);
  io_write_i32_leb(wd, 15);
  io_write_i32_leb(wd, 15);
  io_write_u32_leb(wd, 2);
  
  io_write_u32_leb(wd, BINUI_MAGIC);
  
  io_write_u32_leb(wd, BINUI_RECTANGLE);
  io_write_u32_leb(wd, BINUI_MAGIC);

  io_write_u32_leb(wd, BINUI_POSITION);
  io_write_i32_leb(wd, 15);
  io_write_i32_leb(wd, 15);
  io_write_u32_leb(wd, 2);
  io_write_u32_leb(wd, BINUI_MAGIC);
  
  io_write_u32_leb(wd, BINUI_RECTANGLE);
  io_write_u32_leb(wd, BINUI_MAGIC);

  io_write_u32_leb(wd, BINUI_3D);
  
  io_write_u32_leb(wd, 1);
  io_write_u32_leb(wd, BINUI_MAGIC);
  
  io_write_u32_leb(wd, BINUI_3D_POLYGON);
  io_write_u32_leb(wd, 4); // vertexes
  io_write_u32_leb(wd, 2); // vertex dimensions
  float pts[8] = {0,0,1,0,1,1,0,1};
  io_write(wd, pts, 8 * sizeof(pts[0]));
  
  io_write_u32_leb(wd, BINUI_MAGIC);
  
  io_write_u32_leb(wd, BINUI_RECTANGLE);
  io_write_u32_leb(wd, BINUI_MAGIC);
  
  io_write_u32_leb(wd, BINUI_OPCODE_NONE);
  io_reset(wd);
}

typedef struct{
  binui_context * ctx;
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
  io_write_fmt(wd, "(%s", binui_opcode_name(ctx->ctx, ctx->ctx->current_opcode));
  var reg = ctx->ctx;
  ASSERT(frame->opcode < reg->opcodedef_count);
  var handler = reg->opcodedefs[frame->opcode];
  for(u32 i = 0; i < handler.typesig_count; i++){
    let typesig = handler.typesig[i];
    switch(typesig.signature)
      {
      case BINUI_F32A:
	{
	  f32_array array;
	  ASSERT(binui_stack_register_top(reg, typesig.reg, &array));
	  for(size_t i = 0; i < array.count; i++){
	    io_write_fmt(wd, " %g", array.array[i]);
	  }
	}
	break;
      case BINUI_STRING:
	{
	  char * t;
	  ASSERT(binui_stack_register_top(reg, typesig.reg, &t));
	  io_write_fmt(wd, " \"%s\"", t);
	  break;
	}
      case BINUI_F32:
	{
	  f32 t;
	  ASSERT(binui_stack_register_top(reg, typesig.reg, &t));
	  io_write_fmt(wd, " %g", t);
	  break;
	}
      case BINUI_F64:
	{
	  f64 t;
	  ASSERT(binui_stack_register_top(reg, typesig.reg, &t));
	  io_write_fmt(wd, " %g", t);
	  break;
	}
      case BINUI_VEC2:
	{
	  vec2 t;
	  ASSERT(binui_stack_register_top(reg, typesig.reg, &t));
	  io_write_fmt(wd, " %g %g", t.x, t.y);
	  break;
	}
      case BINUI_VEC3:
	{
	  vec3 t;
	  ASSERT(binui_stack_register_top(reg, typesig.reg, &t));
	  io_write_fmt(wd, " %g %g %g", t.x, t.y, t.z);
	  break;
	}
      case BINUI_VEC4:
	{
	  vec4 t;
	  ASSERT(binui_stack_register_top(reg, typesig.reg, &t));
	  io_write_fmt(wd, " %g %g %g %g", t.x, t.y, t.z, t.w);
	  break;
	}
      case BINUI_UINT32:
	{
	  u32 t;
	  ASSERT(binui_stack_register_top(reg, typesig.reg, &t));
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
void test_binui_load_lisp();
io_reader io_from_bytes(const void * bytes, size_t size);
void test_write_lisp(binui_context * reg, void * buffer, size_t size){
  
  io_writer wd = {0};
  test_render_context rctx = {.ctx = reg, .stack_level = 0 , .wd = &wd};
  
  node_callback cb = {.after_enter = test_after_enter,
		      .before_exit = test_before_exit,
		      .userdata = &rctx};
  node_callback_push(reg, cb);
  
  io_reader rd = io_from_bytes(buffer, size);
  binui_iterate(reg, &rd);
  io_write_u8(&wd, 0);
  logd("%s", wd.data);
  logd("\n");
  node_callback_pop(reg);
  
}


void binui_test(){
  binary_io buffer = {0};
  lisp_to_bytes_str("(cons 1 2)", &buffer);
  io_writer buffer2 = {0};
  buffer.length = buffer.offset;
  buffer.offset = 0;
  bytes_to_lisp(&buffer, &buffer2);
  //test_binui_load_lisp();  
 return;
 /*
  binui_context reg;
  binui_init(&reg);
  test_render_context rctx = {.ctx = &reg, .stack_level = 0};
  
  node_callback cb = {.after_enter = test_after_enter,
			.before_exit = test_before_exit,
			.userdata = &rctx};
  
  node_callback_push(&reg, cb);
  push_color(&reg, 1);
  push_color(&reg, 2);
  push_color(&reg, 3);
  u32 color = get_color(&reg);
  u32 color2 = pop_color(&reg);
  ASSERT(color2 == 3);
  ASSERT(color == 3);
  logd("Color: %u\n", color);
  
  logd("Color: %u\n", color2);
  logd("Color: %u\n", pop_color(&reg));
  logd("Color: %u\n", pop_color(&reg));
  
  vec2i s;
  position_push(&reg, vec2i_new(1, 1));
  position_get(&reg, &s);
  logd(" %i %i\n", s.x, s.y);
  position_push(&reg, vec2i_new(2, 2));
  position_push(&reg, vec2i_new(3, 3));

  position_get(&reg, &s);
  logd(" %i %i\n", s.x, s.y);
  ASSERT(s.x == 6 && s.y == 6);
  position_pop(&reg);
  position_get(&reg, &s);
  ASSERT(s.x == 3 && s.y == 3);
  position_pop(&reg);
  position_pop(&reg);

  size_push(&reg, vec2i_new(1, 1));
  size_push(&reg, vec2i_new(2, 2));
  s = vec2i_zero;
  size_get(&reg, &s);
  ASSERT(s.x == 2 && s.y == 2);
  size_pop(&reg);
  
  size_get(&reg, &s);
  ASSERT(s.x == 1 && s.y == 1);
  
  
  io_writer _wd = {0};
  io_writer * wd = &_wd;
  binui_test_load(wd);
  binui_iterate(&reg, wd);
  logd("\n");
  test_binui_load_lisp();  
 */
}
