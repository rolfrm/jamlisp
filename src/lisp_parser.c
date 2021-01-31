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

#include "jamlisp.h"


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
  if(buffer->offset == 0){
    rd.error = 1;
    return rd;
  }
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


string_reader parse_sub(jamlisp_context * ctx, string_reader rd, io_writer * write){
  rd = skip_while(rd, is_whitespace);
  io_writer name_buffer = {0};
  {
    i64 integer;
    string_reader rd_int = read_integer(rd, &name_buffer, &integer);
    if(rd_int.error == 0){
      logd("LOAD int: %i\n", integer);
      io_write_u8(write, JAMLISP_OPCODE_INT);
      io_write_i64_leb(write, integer);

      io_writer_clear(&name_buffer);
      io_write_u32_leb(write, JAMLISP_MAGIC);
      return rd_int;
    }
  }
  rd = skip_untilc(rd, '(');
  rd.offset += 1;
  

  var rd4 = read_until(rd, &name_buffer, is_endexpr);
  ASSERT(!rd4.error);
  io_write_u8(&name_buffer, 0);

  jamlisp_object sym = jamlisp_symbol(ctx, name_buffer.data);
  logd("SYMBOL: %s\n", name_buffer.data);
  io_reset(&name_buffer);
  string_reader rd_after;
  rd4 = skip_while(rd4, is_whitespace);
  
  rd_after = rd4;
  io_reset(&name_buffer);
  u32 child_count = 0;
  while(true){
    var rd2 = skip_while(rd_after, is_whitespace);
    
    char next = next_byte(rd2);
    if(next == ')'){
      rd2.offset += 1;
      rd_after = rd2;
      break;
    }
    rd2 = parse_sub(ctx, rd2, &name_buffer);
    if(rd2.error != 0){
      ERROR("could not parse sub expression\n");
      break;
    }
    rd_after = rd2;
    child_count += 1;
  }
  io_write_u32_leb(write, JAMLISP_OPCODE_CALL);
  io_write_u32_leb(write, sym.symbol);
  io_write_u32_leb(write, child_count);
  io_write_u32_leb(write, JAMLISP_MAGIC);
  
  io_write(write, name_buffer.data, name_buffer.offset);
  io_writer_clear(&name_buffer);
  return rd_after;
}

void jamlisp_load_lisp(jamlisp_context * ctx, io_reader * rd, io_writer * write){
  string_reader r = {.rd = rd, .offset = io_offset(rd)};
  r = parse_sub(ctx, r, write);
  if(r.error){
    ERROR("ERROR!\n");
  }
  io_write_i8(write, JAMLISP_OPCODE_NONE);
}


void test_jamlisp_string_reader(){
  logd("TEST Jamlisp String Reader\n");
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


void jamlisp_load_lisp2(jamlisp_context * reg, io_writer * wd, const char * target){
  io_reader rd = io_from_bytes(target, strlen(target) + 1);
  jamlisp_load_lisp(reg, &rd, wd);
}


void test_jamlisp_lisp_loader(){
  logd("TEST Jamlisp Lisp Loader\n");
  {
    jamlisp_context * reg = jamlisp_new();
    const char * target = "   \n (color 11223344)";
    io_writer writer = {0};
    jamlisp_load_lisp2(reg, &writer, target); 
    char * buffer = writer.data;
    for(size_t i = 0; i < writer.offset; i++){
      logd("%x ", buffer[i]);
    }
    logd("\nDone loading lisp (%i bytes)\n", writer.offset);
  }
  {
    const char * target = "   \n (color 0x44332211 (import \"3d\") (color 0x55443322 (position 1 2 (size 10 10 (rectangle)) (size 20 20 (position 10 5 (rectangle) (size 1 1 (scale 0.5 1.0 0.5 (translate 10 0 10 (rotate 0 0 1 0.5 (rectangle) (polygon 1.0 0.0 0.0  0.0 1.0 0.0 0.0 0.0 0.0)))))))))) (color 0x1)";
    jamlisp_context * reg = jamlisp_new();
  
    io_writer writer = {0};
    jamlisp_load_lisp2(reg, &writer, target); 
    
    io_reader rd2 = io_from_bytes(writer.data, writer.offset);

    jamlisp_iterate(reg, &rd2);
    
    char * buffer = writer.data;
    for(size_t i = 0; i < writer.offset; i++){
      logd("%i ", buffer[i]);
    }
    logd("\nDone loading lisp (%i bytes)\n", writer.offset);
    //test_write_lisp(reg, writer.data, writer.offset);
    logd("Rewriting lisp\n");
  }
 
}

void test_jamlisp_load_lisp(){
  test_jamlisp_string_reader();
  test_jamlisp_lisp_loader();
}
