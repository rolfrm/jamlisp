all:
	make -C iron
	make -C libmicroio
	make -f jamlisp.makefile
clean:
	make -C iron clean
	make -C libmicroio clean
	make -f jamlisp.makefile clean

