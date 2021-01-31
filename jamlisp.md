# Prototype
## A Common Lisp Interpreter supporting webassembly compile target.

Reason: Since I am frustrated there is no common lisp for the web I'll try to make one for webassembly. Probably I'll never get done, this is an exercise.

Here I'll define the basis for the interpreter, I will lay the foundation for a basic lisp and then build most of the common lisp parts in lisp itself. 

# Byte Code Interpreter.

This will be based around building a byte code compiler and byte code interpreter. The following opcodes exists:

CONS [arg1] [arg2] |  (cons arg1 arg2)
arg1: Lisp object
arg2: Lisp object

CAR [arg1] | (car arg1)
Pops an item from the stack.
if not item is a cons emit error
else push the car of the cons

CDR [arg1] | (cdr arg1)
Pops an item from the stack.
if not item is a cons emit an error
else push the cdr of the cons

EVAL [expr1] | (eval expr)
eval is critical in order to support branching. 

PROG2 [expr1] [expr2] | (progn expr1 expr2)
Executes expr2 after expr1.

SELECT [arg1] [arg2] [arg3]
if arg1 push arg2
if not arg1 push arg 3

FAIL arg1
emits an error to be caught by the error handler. 

TRAP code error-handler
sets up an error handler for the code. If the code generates an error, error-handler will be evaluated. When the error-handler runs the first item on the stack will be the error. 

### Data

These are the 

INT value

value is an LEB encoded signed integer.

FLOAT value

value is an arbritrarily precise float

STRING count: LEB, bytes

EXPR pointer
pointer is an LEB encoded signed integer with an offset. This is used for quote.

### Binary Trivial byte codes
ADD [x] [y] | (+ x y)
SUB [x] [y] | (- x y)
DIV [x] [y] | (/ x y)
MUL [x] [y] | (* x y)

### Unary Trivial byte codes
NOT [x] | (not x)

### Branching byte codes
There is no branching byte codes, these are implemented by calls to eval.

## Branching
branching is done by eval'ing a byte code pointer.

(eval (quote (+ 1 2)))

quote is a special function that is noticed by the lisp compiler. The lisp compiler will insert a reference to the quoted byte code. That means there exists a kind of encoded cons list

## Data types
nil
cons: cons cell
Fixnum: 32 bit integer
single-float: 32 bit float
double-float: 64 bit float
integer: arbritrary precision integer
bigfloat: arbritrary precision floating point
string: 0-terminated utf8 string
const cons: cons cell that cannot be modified
function:

## error handling
If an error occurs the stack will be unrolled until there is an error


# Binary code format example

The byte is a cons-like structure.

`(CONS (+ 1 2) 3)`

Does not equate this:

```
INT 1
INT 2
ADD
INT 3
CONS
```

but instead:
```
CONS
ADD
INT 1
INT 2
INT 3
```

in code this is literally:
CONS ADD INT 1 INT 2 INT 3

This is also known as polish notation

Another example:
`(IF 1 2 3)`

```
INT 2   (addr 1)
INT 3   (addr 2)

INT 1
EXPR (addr 1)
EXPR (addr 2)
SELECT
EVAL
```
1 is pushed to the stack
the two expr locations are pushed to the stack
select sees 1 and pushes address 1
eval executes address 1.

But how does the interpreter know not to jump to INT 3? The code could also have been
```
INT 2
INT 3
ADD 
```

### Functions
Functions are declared to take a number of arguments.

`(x 1 2)
```
CALL X N (n args).
INT 1
INT 2
```
### Variables
```
(let ((x 1))
   (print x))
```
->
```
LET X
INT 1
PROGN 1
CALL PRINT
VAR X
```

### Closures

Now it becomes a bit spicy
```
(let ((y 5))
  (let ((f (lambda (x) (print (+ y x)))))
    (f y)))
```
f closed over the y variable
```
LET Y
INT 5
LET F
LAMBDA
CONS  <- first list, closed symbols
CAPTURE Y  <- Upgrade Y to being a closure
NIL
CONS <- Seconds list all the arguments as a list of symbols
ARGUMENT X
NIL
QUOTE <- In-line quote is probably slow. This could be moved out.
PRINT
ADD
GET Y
GET X
CALL F
GET Y
```

# The Stack 
Generally objects on the stack are lisp objects. 

Since WebAssembly is normally 32 bit, I think it would be unwise to use some header bitset to mark if something is an integer. Any lisp stack object is a pointer to some space


# Garbage Collection

I will probably implement some simple mark-and-sweep garbage collector.

The root nodes are bound to symbols in the global scope. Then there is the call stack, which consists primarily of lisp objects. Some of these lisp objects are static and should not be reclaimed by gc.

# Variables

Symbol values is a huge table. This includes function arguments. For a single threaded versoin

