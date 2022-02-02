CC = gcc
FLAGS = -pedantic -g -Wall -Wextra -pthread
BIN = program
SOURCE = program.c 
HEADER = base.h

target : all

all: $(HEADER) $(SOURCE)
	$(CC) $(SOURCE) $(FLAGS) -o $(BIN)

clean :
	rm $(BIN)
