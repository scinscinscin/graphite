all:
	gcc -o graphite_assembler *.c -l:scinstdlib.a -O0 -g
