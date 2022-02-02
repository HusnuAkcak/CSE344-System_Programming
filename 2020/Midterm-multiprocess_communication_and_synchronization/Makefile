CC = gcc
FLAGS = -pedantic -g -Wall -lm -lrt -pthread
BIN = program
SOURCE = program.c io_helper.c cook.c student.c supplier.c
HEADER = io_helper.h base.h cook.h student.h supplier.h

target : all

all: $(HEADER) $(SOURCE)
	$(CC) $(SOURCE) $(FLAGS) -o $(BIN)

clean :
	rm $(BIN)
