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
#include <microio.h>

#include "binui.h"


typedef struct{
  io_reader * rd;
  size_t offset;
  int error;
}string_reader;

void fix_reader(string_reader rd){
  ASSERT(rd.error == 0);
  var rd2 = rd.rd;
  size_t loc = io_offset(rd2);
  if(loc > rd.offset){
    io_rewind(rd2, loc -rd.offset);
  }else if(loc < rd.offset){
    io_advance(rd2, rd.offset - loc);
  }
  ASSERT(rd.offset == io_offset(rd2));
}

string_reader read_until(string_reader rd, io_writer * writer, bool (* f)(char c)){
  var rd2 = rd.rd;
  fix_reader(rd);
  
  while(true){
    var ch = io_peek_u8(rd2);
    if(f(ch)) break;
    if(writer != NULL)
      io_write_u8(writer, ch);
    io_advance(rd.rd, 1);
    rd.offset += 1;
  }
  return rd;
}


static __thread bool (* current_f)(char c);
bool not_current_f(char c){
  return !current_f(c);
}

string_reader read_while(string_reader rd, io_writer * writer, bool (* f)(char c)){
  current_f = f;
  return read_until(rd, writer, not_current_f);
}

static __thread char selected_char;
bool check_selected_char(char c){
  return selected_char == c;
}

string_reader read_untilc(string_reader rd, io_writer * writer, char c){
  selected_char = c;
  return read_until(rd, writer, check_selected_char);
}

string_reader skip_until(string_reader rd, bool (*f)(char c)){
  return read_until(rd, NULL, f);
}

string_reader skip_untilc(string_reader rd, char c){
  return read_untilc(rd, NULL, c);
}

string_reader skip_while(string_reader rd, bool (*f)(char c)){
  return read_while(rd, NULL, f);
}

bool is_digit(char c){
  return c >= '0' && c <= '9';
}

bool is_alpha(char c){
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_alphanum(char c){
  return is_digit(c) || is_alpha(c);
}

bool is_whitespace(char c){
  return c == ' ' || c == '\t' || c == '\n';
}

bool is_hex(char c){
  return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool is_endexpr(char c){
  return c == ')' || c == '(' || is_whitespace(c) || c == 0 || c ==';';
}

u8 next_byte(string_reader rd){
  fix_reader(rd);
  
  var rd2 = rd.rd;
  return io_peek_u8(rd2);  
}

io_reader io_from_bytes(const void * bytes, size_t size){
  io_reader rd = {.data = (void *) bytes, .offset = 0, .size = size}; 
  return rd;
}

string_reader read_str(string_reader rd, io_writer * writeto){
  rd = skip_while(rd, is_whitespace);
  var rd2 = rd.rd;
  var ch1 = io_peek_u8(rd2);
  if(ch1 != '"'){
    rd.error = 1;
    return rd;
  }
  io_advance(rd2, 1);
  while(true){
    var ch = io_read_u8(rd2);
    if(ch == '"'){
      if(io_peek_u8(rd2) == '"'){
	io_advance(rd2, 1);
      }else{
	break;
      }
    }
    io_write_u8(writeto, ch);
  }
  rd.offset = io_offset(rd2);
  
  return rd;
}


string_reader read_hex(string_reader rd, io_writer * buffer, u64 * out){
  u64 r = 0;
  io_reset(buffer);
  rd = read_until(rd, buffer, is_endexpr);
  char * str = buffer->data;
  
  for(size_t i = 0; i < buffer->offset; i++){
    var c = str[i];
    if(i == 0 && c == '0' && str[1] == 'x'){
      i++;
      continue;
    }
    if(!is_hex(str[i])){
      rd.error = 1;
      break;
    }
    u64 v = 0;
    if(c >= '0' && c <= '9'){
      v = c - '0';
    }else if(c >= 'a' && c <= 'f'){
      v = c - 'a' + 10;
    }else if(c >= 'A' && c <= 'F'){
      v = c - 'A' + 10;    
    }else{
      ERROR("This is impossible!");
    }
    r = (r << 4) | v;
  }
  *out = r;
  return rd;
}

string_reader read_f64(string_reader rd, io_writer * buffer, f64 * out){
  io_reset(buffer);
  rd = skip_while(rd, is_whitespace);
  rd = read_until(rd, buffer, is_endexpr);
  io_write_u8(buffer, 0);
  char * str = buffer->data;
  char * tail = NULL;
  
  *out = strtod(str, &tail);
  //logd("reading %s\n", str);
  if(*str == 0 || tail != (str + buffer->offset - 1)){
    logd("Done!\n");
    rd.error = 1;
  }
  return rd;
}


string_reader read_integer(string_reader rd, io_writer * buffer, i64 * out){
  i64 r = 0;
  bool negative = false;
  io_reset(buffer);
  rd = skip_while(rd, is_whitespace);
  rd = read_until(rd, buffer, is_endexpr);
  char * str = buffer->data;
  for(size_t i = 0; i < buffer->offset; i++){
    var c = str[i];
    if(c == '-'){
      negative = true;
      continue;
    }
    if(!is_digit(str[i])){
      //ERROR("CANNOT read integer");
      rd.error = 1;
      break;
    }
    u64 v = 0;
    if(c >= '0' && c <= '9'){
      v = c - '0';
    }else{
      ERROR("This is impossible!");
    }
    r = (r * 10) + v;
  }
  *out = negative ? -r : r;
  return rd;
}


string_reader parse_sub(binui_context * ctx, string_reader rd, io_writer * write){
  rd = skip_untilc(rd, '(');
  rd.offset += 1;
  rd = skip_while(rd, is_whitespace);
  
  io_writer name_buffer = {0};
  var rd4 = read_until(rd, &name_buffer, is_endexpr);
  ASSERT(!rd4.error);
  io_write_u8(&name_buffer, 0);
 
  binui_opcode opcode = binui_opcode_parse(ctx, name_buffer.data);
  io_write_u8(write, opcode);
  io_reset(&name_buffer);
  string_reader rd_after;
  rd4 = skip_while(rd4, is_whitespace);
  binui_opcodedef type = binui_get_opcodedef(ctx, opcode);

  for(size_t i = 0; i < type.typesig_count; i++){
    let type = type.typesig[i];
    size_t count = 0;
    switch(type.signature){
    case BINUI_VEC2:
      count = 2;
      goto handle_float;
      break;  
    case BINUI_VEC3:
      count = 3;
      goto handle_float;
      break;
    case BINUI_VEC4:
      count = 4;
      goto handle_float;
      
    case BINUI_F32:
      count = 1;
    handle_float:;
      for(size_t i = 0; i < count; i++){
	rd4 = skip_while(rd4, is_whitespace);
	f64 x = 0;
	rd4 = read_f64(rd4, &name_buffer, &x);
	io_write_f32(write, x);
      }
      break;
    case BINUI_F32A:
      {
	io_writer numbers_buffer = {0};
	u64 count = 0;
	while(true){
	  rd4 = skip_while(rd4, is_whitespace);
	  f64 x = 0;
	  var rd4_2 = read_f64(rd4, &name_buffer, &x);
	  if(rd4_2.error != 0){
	    break;
	  }
	  rd4 = rd4_2;
	  count += 1;
	  io_write_f32(&numbers_buffer, x);
	}
	io_write_u64_leb(write, count);
	io_write(write, numbers_buffer.data, numbers_buffer.offset);
	io_writer_clear(&numbers_buffer);
      }
      break;
    case BINUI_INT8:
    case BINUI_INT16:
    case BINUI_INT32:
    case BINUI_INT64:
      //case BINUI_UINT8:
      //case BINUI_UINT16:
    case BINUI_UINT32:
    case BINUI_UINT64:
    case BINUI_INT64_LEB:
      {
	i64 x = 0;
	
	var rd5 = read_integer(rd4, &name_buffer, &x);
	if(rd5.error != 0){
	  rd5 = read_hex(rd4, &name_buffer, (u64 *) &x);
	}
	if(rd5.error != 0)
	  ERROR("Unable to read integer");
	rd4 = rd5;

	switch(type.signature){
	case BINUI_INT8:
	  io_write_i8(write, x);
	  break;
	case BINUI_INT16:
	  io_write_i16(write, x);
	  break;
	case BINUI_INT32:
	  io_write_i32(write, x);
	  break;
	case BINUI_INT64:
	  io_write_i64(write, x);
	  break;
	  /*case BINUI_UINT8:
	  io_write_u8(write, x);
	  break;
	case BINUI_UINT16:
	  io_write_u16(write, x);
	  break;*/
	case BINUI_UINT32:
	  io_write_u32(write, x);
	  break;
	case BINUI_UINT64:
	  io_write_u64(write, x);
	  break;
	
	case BINUI_INT64_LEB:
	  io_write_i64_leb(write, x);
	  break;
	default:
	  ERROR("UNSUPPORTED TYPE");
	  break;
	}
      }
      break;
    case BINUI_STRING:
      io_reset(&name_buffer);
      rd4 = read_str(rd4, &name_buffer);
      if(rd4.error == 0){
	io_write_u8(&name_buffer, 0);
	io_write_strn(write, name_buffer.data);
      }else{
	ERROR("Unable to read string");
      }
      break;
    default:
      ERROR("UNSUPPORTED TYPE\n");
      break;
    }
  
  }
  rd_after = rd4;
  io_reset(&name_buffer);
  u32 child_count = 0;
  while(true){
    var rd2 = skip_while(rd_after, is_whitespace);
    
    char next = next_byte(rd2);
    if(next == '('){
      rd_after = parse_sub(ctx, rd2, &name_buffer);
      child_count += 1;
      continue;
    }
    else if(next == ')'){
      rd2.offset += 1;
      rd_after = rd2;
      break;
    }
    else{
      logd("Unexpected token '%c'", next);
      rd2.error = 1;
      rd_after = rd2;
      break;
    }
  }
  if(type.has_children){
    
    io_write_u32_leb(write, child_count);
  }
  io_write_u32_leb(write, BINUI_MAGIC);
  
  io_write(write, name_buffer.data, name_buffer.offset);
  io_writer_clear(&name_buffer);
  return rd_after;
}

void binui_load_lisp(binui_context * ctx, io_reader * rd, io_writer * write){
  string_reader r = {.rd = rd, .offset = io_offset(rd)};
  r = parse_sub(ctx, r, write);
  if(r.error){
    ERROR("ERROR!\n");
  }
  io_write_i8(write, BINUI_OPCODE_NONE);
}


void test_binui_string_reader(){
  logd("TEST Binui String Reader\n");
  const char * target = "   \n (color #112233fFff -123)";

  io_reader rd = io_from_bytes(target, strlen(target) + 1);
  string_reader rd2 = {.rd = &rd, .offset = 0};
  var rd3 = skip_untilc(rd2, '(');
  rd3.offset += 1;
  
  var next = next_byte(rd3);
  ASSERT(next == 'c');

  next = next_byte(rd2);
  ASSERT(next == ' ');

  next = next_byte(rd3);
  ASSERT(next == 'c');
  
  io_writer symbol_writer = {0};
  var rd4 = read_until(rd3, &symbol_writer, is_endexpr);
  ASSERT(symbol_writer.offset == strlen("color"));
  ASSERT(strncmp(symbol_writer.data, "color", symbol_writer.offset) == 0);
   
  var rd5 = skip_while(rd4, is_whitespace); 
  var next2 = next_byte(rd5);
  
  ASSERT(next2 == '#');
  rd5.offset += 1;
  io_reset(&symbol_writer);
  u64 hexv = 0;
  var rd6 = read_hex(rd5, &symbol_writer, &hexv);
  ASSERT(rd6.error == 0);
  ASSERT(hexv == 0x112233ffff);

  logd("Hex: %p\n", hexv);
  
  var next3 = next_byte(rd6);
  ASSERT(next3 == ' ');
  var rd7 = skip_while(rd6, is_whitespace);
  i64 i = 0;
  var rd8 = read_integer(rd7, &symbol_writer, &i);
  ASSERT(i == -123);
  rd8 = skip_while(rd8, is_whitespace);
  var next5 = next_byte(rd8);
  logd("next5: '%c'\n", next5);
  ASSERT(next5 == ')');
  var next4 = next_byte(rd3);
  ASSERT(next4 == 'c');

  io_writer_clear(&symbol_writer);
  logd("OK\n");
}


void binui_load_lisp_string(binui_context * reg, io_writer * wd, const char * target){
  io_reader rd = io_from_bytes(target, strlen(target) + 1);
  binui_load_lisp(reg, &rd, wd);
}

void test_binui_lisp_loader(){
  logd("TEST Binui Lisp Loader\n");
  {
    binui_context * reg = binui_new();
    const char * target = "   \n (color 11223344)";
    io_writer writer = {0};
    binui_load_lisp_string(reg, &writer, target); 
    char * buffer = writer.data;
    for(size_t i = 0; i < writer.offset; i++){
      logd("%x ", buffer[i]);
    }
    logd("\nDone loading lisp (%i bytes)\n", writer.offset);
  }
  {
    const char * target = "   \n (color 0x44332211 (import \"3d\") (color 0x55443322 (position 1 2 (size 10 10 (rectangle)) (size 20 20 (position 10 5 (rectangle) (size 1 1 (scale 0.5 1.0 0.5 (translate 10 0 10 (rotate 0 0 1 0.5 (rectangle) (polygon 1.0 0.0 0.0  0.0 1.0 0.0 0.0 0.0 0.0)))))))))) (color 0x1)";
    binui_context * reg = binui_new();
  
    io_writer writer = {0};
    binui_load_lisp_string(reg, &writer, target); 
    
    io_reader rd2 = io_from_bytes(writer.data, writer.offset);

    binui_iterate(reg, &rd2);
    
    char * buffer = writer.data;
    for(size_t i = 0; i < writer.offset; i++){
      logd("%i ", buffer[i]);
    }
    logd("\nDone loading lisp (%i bytes)\n", writer.offset);
    test_write_lisp(reg, writer.data, writer.offset);
    logd("Rewriting lisp\n");
  }
  

}

void test_binui_load_lisp(){
  test_binui_string_reader();
  test_binui_lisp_loader();
}
