#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <assert.h>

#include<microio.h>
#include <iron/full.h>
#include <iron/gl.h>
typedef int8_t i8;

typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

#include<binui.h>
#define UNUSED(x) (void)(x)

typedef struct {

}binui_context;


static io_reader * current_reader;
io_writer * binui_stdout;
static void describe(){
  // todo:
  // 3 things can happen in an element
  // 1 - it describes something - pushes onto the stack
  // 2 - it finishes something - pops from the stack
  // 3 - it moves to the next element - stack stays the same place.
  
  static int space = 0;
  space += 1;
  binui_opcodes opcode = (binui_opcodes)io_read_u8(current_reader);
  io_write_f(binui_stdout, "%*s", space, "");
  switch(opcode){
  case BINUI_GROUP:{
    u32 count = io_read_u32_leb(current_reader);
    printf("Group (%i)\n", count);
    for(i32 i = 0; i < count; i++){
      describe();
    }
    break;
  }
  case BINUI_SIZE:
    {
      u16 w = io_read_u16(current_reader);
      u16 h = io_read_u16(current_reader);
      io_write_f(binui_stdout, "Size: %i %i\n", w, h);
      describe();
    }
    break;
  case BINUI_VIEW:
    break;
  case BINUI_RECT:
    {
      io_write_f(binui_stdout, "Model: Rectangle\n");
    }
    break;
  case BINUI_ID:
    {
      u32 len = 0;
      char * name_buf = io_read_strn(current_reader, &len);
      io_write_f(binui_stdout, "ID: %.*s\n", len, name_buf);
      free(name_buf);
      describe();
      break;
    }
  case BINUI_CIRCLE:
    io_write_f(binui_stdout, "Model: Circle\n");
    break;
  case BINUI_COLOR:
    {
      u8 color[4];
      io_write_f(binui_stdout, "Color: ");
      for(int i = 0; i < 4; i++){
	color[i] = io_read_u8(current_reader);
	io_write_f(binui_stdout, "%i ", color[i]);
      }
      io_write_f(binui_stdout, "\n");
      describe();
      break;
    }
  default:
    break;

  }
  space -= 1;  
}

static io_writer _binui_stdout;


static void write_stdout(void * data, size_t count, void * userdata){
  UNUSED(userdata);
  fwrite(data, count, 1, stdout);
}

void binui_describe(io_reader * reader){
  if(binui_stdout == NULL){
    binui_stdout = &_binui_stdout;
    binui_stdout->f =  write_stdout;
  }
  
  io_reader * prev_reader = current_reader;
  current_reader = reader;
  describe();
  current_reader = prev_reader;
}

static void binui_iterate_internal(binui reg, io_reader * reader, void (* callback)(binui * registers, void * userdata), void * userdata){

  while(true){
    binui_opcodes opcode = (binui_opcodes)io_read_u8(reader);
    printf("opcode: %i\n", opcode);
 
    switch(opcode){
    case BINUI_GROUP:
      {
	// some things needs to recurse,
	u32 count = io_read_u32_leb(reader);
	for(u32 i = 0; i < count; i++)
	  binui_iterate_internal(reg, reader, callback, userdata);
	return;
      }
    case BINUI_SIZE:
      {
	u16 w = io_read_u16(reader);
	u16 h = io_read_u16(reader);
	reg.w = w;
	reg.h = h;
      }
    break;
    case BINUI_VIEW:
      break;
    case BINUI_RECT:
      reg.model = BINUI_MODEL_RECT;
      callback(&reg, userdata);
      
      return;
    case BINUI_ID:
      {
	u32 len = 0;
	char * name_buf = io_read_strn(reader, &len);
	reg.id = name_buf;
	break;
      }
    case BINUI_CIRCLE:
      reg.model = BINUI_MODEL_CIRCLE;
      callback(&reg, userdata);
      return;
    case BINUI_COLOR:
      {
	for(int i = 0; i < 4; i++){
	  reg.color[i] = io_read_u8(reader);
	}
	break;
      }
      
    default:
      printf("???\n");
      break;
    }
  }
}

void binui_iterate(io_reader * reader, void (* callback)(binui * registers, void * userdata), void * userdata){
  binui reg = {0};
  binui_iterate_internal(reg, reader, callback, userdata);
}

void test_iterate(binui * reg, void * userdata){
  printf("%i %i ", reg->w, reg->h);
  if(reg->model == BINUI_MODEL_CIRCLE)
    printf("Circle");
  else if(reg->model == BINUI_MODEL_RECT){
    printf("Rect");
  }else{
    printf("??");
  }
  printf("\n");
}

int main(int argc, char ** argv){
  io_writer _wd = {0};
  io_writer * wd = &_wd;
  io_write_u8(wd, BINUI_GROUP);
  io_write_u32_leb(wd, 2);
  
  io_write_u8(wd, BINUI_SIZE);
  // 500x500
  io_write_u16(wd, 500);
  io_write_u16(wd, 500);
  io_write_u8(wd, BINUI_COLOR);
  // rgba
  io_write_u8(wd, 255);
  io_write_u8(wd, 0);
  io_write_u8(wd, 0);
  io_write_u8(wd, 255);
  io_write_u8(wd, BINUI_ID);
  const char * grpname = "hello world";
  io_write_strn(wd, grpname);
  
  io_write_u8(wd, BINUI_RECT);

  io_write_u8(wd, BINUI_SIZE);
  // 500x500
  io_write_u16(wd, 500);
  io_write_u16(wd, 500);
  io_write_u8(wd, BINUI_COLOR);
  // rgba
  io_write_u8(wd, 255);
  io_write_u8(wd, 255);
  io_write_u8(wd, 0);
  io_write_u8(wd, 255);
  io_write_u8(wd, BINUI_ID);
  const char * grpname2 = "hello world2";
  io_write_strn(wd, grpname2);
  io_write_u8(wd, BINUI_CIRCLE);
  
  io_reset(wd);
  binui_describe(wd);
  io_reset(wd);
  binui_iterate(wd, test_iterate, NULL);
  gl_window * w = gl_window_open(512, 512);
  gl_window_make_current(w);
  while(true){
    gl_window_poll_events();
    gl_window_swap(w);
  }
  
  return 0;
}
