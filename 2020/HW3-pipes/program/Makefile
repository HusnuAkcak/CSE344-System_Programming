CC = gcc
FLAGS = -pedantic -g -Wall -lm
BIN = program
SOURCE = program.c base.c io_helper.c calc.c piping.c
HEADER = calc.h base.h io_helper.h piping.h

target : all

all: $(HEADER) $(SOURCE)
	$(CC) $(SOURCE) $(FLAGS) -o $(BIN)

clean :
	rm $(BIN)
