CC = gcc
FLAGS = -pedantic -g -Wall -lpthread -pthread -lrt
SERVER_SOURCE = server.c
SERVER_HEADER = 
CLIENT_SOURCE = client.c
CLIENT_HEADER = 

target : server client

server: $(SERVER_HEADER) $(SERVER_SOURCE)
	$(CC) $(SERVER_SOURCE) $(FLAGS) -o server

client: $(CLIENT_HEADER) $(CLIENT_SOURCE)
	$(CC) $(CLIENT_SOURCE) $(FLAGS) -o client

clean :
	rm client server
