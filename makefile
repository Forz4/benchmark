CC=gcc
CCFLAG=-Wall -std=gnu99

#CC=cc
#CCFLAG=

all: benchmark convert

benchmark: benchmark.c benchmark.h
	@$(CC) -o benchmark $(CCFLAG) benchmark.c

convert: convert.c
	@$(CC) -o convert $(CCFLAG) convert.c

clean:
	@rm -f *.o
	@rm -f benchmark convert

pack:
	@tar -cf benchmark.tar benchmark.c benchmark.h makefile convert.c README.md
