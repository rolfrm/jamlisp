
typedef enum{
	     JAMLISP_OPCODE_NONE = 0,
	     JAMLISP_OPCODE_CONS,
	     JAMLISP_OPCODE_ADD,
	     JAMLISP_OPCODE_SUB,
	     JAMLISP_OPCODE_MUL,
	     JAMLISP_OPCODE_DIV,
	     JAMLISP_OPCODE_INT,
	     JAMLISP_OPCODE_PRINT,
	     JAMLISP_OPCODE_CALL,
	     JAMLISP_OPCODE_LOCAL,
	     JAMLISP_OPCODE_LET,
	     JAMLISP_MAGIC = 0x5a,
}jamlisp_opcode;

#define JAMLISP_OPCODE_NONE 0
#define JAMLISP_MAGIC 0x5a
typedef enum{
	     JAMLISP_NIL = 0,
	     JAMLISP_CONS = 1,
	     JAMLISP_SYMBOL,
	     JAMLISP_CONS_CONST,
	     JAMLISP_FIXNUM,
	     JAMLISP_F32,
	     JAMLISP_F64,
	     JAMLISP_INT32,
	     JAMLISP_INT64,
	     JAMLISP_BYTE,
	     JAMLISP_BIGFLOAT,
	     JAMLISP_STRING,
	     JAMLISP_FUNCTION,
	     JAMLISP_ARRAY,
	     JAMLISP_TYPE,
	     JAMLISP_TYPE_NONE
}jamlisp_type;

typedef struct _jamlisp_context jamlisp_context;

typedef struct _jamlisp_stack_frame jamlisp_stack_frame;

typedef u32 jamlisp_object_index;


typedef struct _jamlisp_array jamlisp_array;


typedef struct _jamlisp_object{
  union{
    i32 fixnum;
    i64 int64;
    u32 symbol;
    f32 float32;
    f64 float64;
    jamlisp_object_index cons;
    jamlisp_array * ptr;
  };
  jamlisp_type type;
}jamlisp_object;

struct _cons{
  jamlisp_object car;
  jamlisp_object cdr;
};
typedef struct _cons cons;


struct _jamlisp_array{
  jamlisp_object_index type;
  u32 size;
  void * data;
};


typedef struct _cons_heap{
  cons * cons_heap;
  size_t heap_size;
  jamlisp_object_index free_object;
}cons_heap;


typedef struct{
  u32 arg_count;
  jamlisp_opcode opcode;
  const char * opcode_name;
}jamlisp_opcodedef;



const char * jamlisp_opcode_name(jamlisp_context * ctx, jamlisp_opcode);
jamlisp_opcode jamlisp_opcode_parse(jamlisp_context * ctx, const char * name);
jamlisp_opcode jamlisp_current_opcode(jamlisp_context * ctx);
jamlisp_context * jamlisp_new();
void jamlisp_load_opcode(jamlisp_context * ctx, jamlisp_opcode opcode, const char * name, size_t arg_count);

jamlisp_opcodedef jamlisp_get_opcodedef(jamlisp_context * ctx, jamlisp_opcode opcode);

void jamlisp_iterate(jamlisp_context * reg, io_reader * reader);

void jamlisp_free(jamlisp_context * ctx, jamlisp_object_index obj);
jamlisp_object jamlisp_new_object();
jamlisp_object jamlisp_pop(jamlisp_context * ctx);
void jamlisp_push(jamlisp_context * ctx, jamlisp_object obj);
jamlisp_object jamlisp_top(jamlisp_context * ctx);

void jamlisp_push_i64(jamlisp_context * ctx, i64 value);
i64 jamlisp_pop_i64(jamlisp_context * ctx);
void jamlisp_print(jamlisp_object obj);

jamlisp_object symbol_get_value(jamlisp_context * ctx, jamlisp_object symbol);
void symbol_set_value(jamlisp_context * ctx, jamlisp_object symbol, jamlisp_object object);


void jamlisp_load_lisp2(jamlisp_context * ctx, io_writer * wd, const char * code);
void jamlisp_load_lisp(jamlisp_context * ctx, io_writer * wd, io_reader * code);

jamlisp_object jamlisp_i64(i64 v);
jamlisp_object jamlisp_i32(i32 v);
jamlisp_object jamlisp_f32(f32 v);
jamlisp_object jamlisp_f64(f64 v);
jamlisp_object jamlisp_nil();
jamlisp_object jamlisp_new_cons(jamlisp_context * ctx);
void jamlisp_free_cons(jamlisp_context * ctx, jamlisp_object * cons);

bool jamlisp_nilp(jamlisp_object obj);
bool jamlisp_symbolp(jamlisp_object obj);
bool jamlisp_integerp(jamlisp_object obj);
bool jamlisp_consp(jamlisp_object obj);

jamlisp_object jamlisp_symbol(jamlisp_context * ctx, const char * symbol_name);


struct _jamlisp_stack_frame{
  u32 opcode;
  u32 child_count;
  u64 node_id;
  u32 call;
};

void jamlisp_3d_init(jamlisp_context * ctx);

void jamlisp_load_lisp_string(jamlisp_context * ctx, io_writer * wd, const char * target);

void jamlisp_test_load(jamlisp_context * ctx, io_writer * wd);

// stack

typedef struct{
  void * elements;
  size_t capacity;
  size_t count;
}stack;


void stack_push(stack * stk, const void * data, size_t count);
void stack_pop(stack * stk, void * data, size_t count);
void stack_top(stack * stk, void * data, size_t count);



// utils

void * alloc_elems(void ** ptr, size_t elem_size, size_t * count, size_t * capacity, size_t elem_count);

void ensure_size(void ** ptr, size_t elem_size, size_t * count, size_t new_count);
size_t grow_elems(void ** ptr, size_t elem_size, size_t * count);
