

myshell: myshell.o LineParser.o
	gcc -m32 -Wall -g myshell.o LineParser.o -o myshell

mypipe: mypipe.c
	gcc -m32 -Wall -g mypipe.c -o mypipe

myshell.o: myshell.c LineParser.h
	gcc -m32 -Wall -g -c myshell.c -o myshell.o

LineParser.o: LineParser.c LineParser.h
	gcc -m32 -Wall -g -c LineParser.c -o LineParser.o

.PHONY:clean

clean: LineParser.o myshell.o myshell
	rm -f LineParser.o myshell.o myshell mypipe