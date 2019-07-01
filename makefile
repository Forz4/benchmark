all: benchmark

benchmark:  benchmark.o
	@gcc -o benchmark benchmark.o

benchmark.o:	benchmark.c	benchmark.h
	@gcc -c benchmark.c -Wall -std=gnu99

clean:
	@rm -f *.o

pack:
	@tar -cf benchmark.tar benchmark.c benchmark.h makefile
