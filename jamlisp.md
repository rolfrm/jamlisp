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

SELECT [arg1] [arg2] [arg3]
if arg1 push arg2
if not arg1 push arg 3

FAIL arg1
emits an error to be caught by the error handler. 

TRAP code error-handler
sets up an error handler for the code. If the code generates an error, error-handler will be evaluated. When the error-handler runs the first item on the stack will be the error. 

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
string: 0-terminated utf8 string
const cons: cons cell that cannot be modified

## error handling
If an error occurs the stack will be unrolled until there is an error
