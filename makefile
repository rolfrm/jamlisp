all:
	make -C iron
	make -f jamlisp.makefile
clean:
	make -C iron clean
	make -f jamlisp.makefile clean

