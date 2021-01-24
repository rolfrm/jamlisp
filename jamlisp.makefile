OPT = -g3 -O0
LIB_SOURCES1 = bytecode.c lisp_parser.c
LIB_SOURCES = $(addprefix src/, $(LIB_SOURCES1)) libmicroio/src/microio.c
CC = gcc
TARGET = run
LIB_OBJECTS =$(LIB_SOURCES:.c=.o)
LDFLAGS= $(OPT) 
LIBS= -lglfw -lGL -lX11 -lm
ALL= $(TARGET)
CFLAGS = -I. -Isrc/ -Ilibmicroio/include -Iinclude/ -std=gnu11 -c $(OPT) -Werror -Werror=implicit-function-declaration -Wformat=0 -D_GNU_SOURCE -fdiagnostics-color  -Wwrite-strings -msse4.2 -Werror=uninitialized -DUSE_VALGRIND -DDEBUG -Wall

$(TARGET): $(LIB_OBJECTS)
	$(CC) $(LDFLAGS) $(LIB_OBJECTS)  iron/libiron.a $(LIBS) -o $@

all: $(ALL)

.c.o: $(HEADERS) $(LEVEL_CS)
	$(CC) $(CFLAGS) $< -o $@ -MMD -MF $@.depends

src/ttf_font.c: 
	xxd -i /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf > src/ttf_font.c
main.o: ttf_font.c

depend: h-depend
clean:
	rm -f $(LIB_OBJECTS) $(ALL) src/*.o.depends src/*.o src/level*.c src/*.shader.c 
.PHONY: test
test: $(TARGET)
	make -f makefile.compiler
	make -f makefile.test test

-include $(LIB_OBJECTS:.o=.o.depends)


