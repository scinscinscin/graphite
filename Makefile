assembler: assembler.c
	gcc -o assembler assembler.c -l:scinstdlib.a -O0 -g
