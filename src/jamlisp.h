
typedef enum{
	     JAMLISP_OPCODE_NONE = 0,
	     JAMLISP_OPCODE_CONS,
	     JAMLISP_OPCODE_ADD,
	     JAMLISP_OPCODE_SUB,
	     JAMLISP_OPCODE_MUL,
	     JAMLISP_OPCODE_DIV,
	     JAMLISP_OPCODE_INT,
	     JAMLISP_OPCODE_PRINT,
	     JAMLISP_MAGIC = 0x5a,
}jamlisp_opcode;

#define JAMLISP_OPCODE_NONE 0
#define JAMLISP_MAGIC 0x5a
typedef enum{
	     JAMLISP_NIL = 0,
	     JAMLISP_CONS = 1,
	     JAMLISP_SYMBOL = 2,
	     JAMLISP_CONS_CONST,
	     JAMLISP_FIXNUM,
	     JAMLISP_F32,
	     JAMLISP_F64,
	     JAMLISP_INT32,
	     JAMLISP_INT64,
	     JAMLISP_BIGFLOAT,
	     JAMLISP_STRING,
	     JAMLISP_FUNCTION,
	     JAMLISP_TYPE_NONE
}jamlisp_type;

typedef struct _jamlisp_stack_register jamlisp_stack_register;

typedef struct _jamlisp_context jamlisp_context;

typedef struct _jamlisp_stack_frame jamlisp_stack_frame;

typedef u32 jamlisp_object_index;

struct _cons{
  jamlisp_object_index car;
  jamlisp_object_index cdr;
};
typedef struct _cons cons;

typedef struct _jamlisp_object{
  union{
    i32 fixnum;
    i64 large_int;
    u32 symbol;
    cons c;
  };
  jamlisp_type type;
}jamlisp_object;


typedef struct _object_heap{
  jamlisp_object * object_heap;
  size_t heap_size;
  jamlisp_object_index free_object;
}object_heap;


typedef struct{
  u32 arg_count;
  jamlisp_opcode opcode;
  const char * opcode_name;
}jamlisp_opcodedef;


typedef struct {
  u32 size;
  u32 id;
}jamlisp_register;

struct _jamlisp_stack_register{
  u32 size;
  jamlisp_register stack;
};

const char * jamlisp_opcode_name(jamlisp_context * ctx, jamlisp_opcode);
jamlisp_opcode jamlisp_opcode_parse(jamlisp_context * ctx, const char * name);
jamlisp_opcode jamlisp_current_opcode(jamlisp_context * ctx);
jamlisp_context * jamlisp_new();
void jamlisp_load_opcode(jamlisp_context * ctx, jamlisp_opcode opcode, const char * name, size_t arg_count);

jamlisp_opcodedef jamlisp_get_opcodedef(jamlisp_context * ctx, jamlisp_opcode opcode);

void jamlisp_iterate_internal( jamlisp_context * reg, io_reader * reader);

void jamlisp_stack_register_push(jamlisp_context * ctx, jamlisp_stack_register * reg, const void * value);
void jamlisp_stack_register_pop(jamlisp_context * ctx, jamlisp_stack_register * reg, void * out_value);
bool jamlisp_stack_register_top(jamlisp_context * ctx, jamlisp_stack_register * reg, void * out_value);

void jamlisp_iterate_internal(jamlisp_context * reg, io_reader * reader);

void jamlisp_iterate(jamlisp_context * reg, io_reader * reader);

void jamlisp_free(jamlisp_context * ctx, jamlisp_object_index obj);
jamlisp_object_index jamlisp_new_object(jamlisp_context * ctx);
jamlisp_object_index jamlisp_pop(jamlisp_context * ctx);
void jamlisp_push_i64(jamlisp_context * ctx, i64 value);
i64 jamlisp_pop_i64(jamlisp_context * ctx);

void jamlisp_print(jamlisp_object obj);

struct _jamlisp_stack_frame{
  u32 opcode;
  u32 child_count;
  u64 node_id;
};

typedef struct{
  
  void (* after_enter)(jamlisp_stack_frame * frame, void * userdata);
  void (* before_exit)(jamlisp_stack_frame * frame, void * userdata);
  
  void * userdata;
}node_callback;

void node_callback_push(jamlisp_context * ctx, node_callback callback);
node_callback node_callback_get(jamlisp_context * ctx);
void node_callback_pop(jamlisp_context * ctx);
void jamlisp_3d_init(jamlisp_context * ctx);

void jamlisp_load_lisp_string(jamlisp_context * ctx, io_writer * wd, const char * target);

void jamlisp_test_load(jamlisp_context * ctx, io_writer * wd);

typedef struct{
  void * elements;
  size_t capacity;
  size_t count;
}stack;


void stack_push(stack * stk, const void * data, size_t count);
void stack_pop(stack * stk, void * data, size_t count);
void stack_top(stack * stk, void * data, size_t count);


