CC=g++
CFLAGS=-std=c++17

all : client server

client: chat_client.cpp
	$(CC) $(CFLAGS) -o client chat_client.cpp

server: chat_server.cpp
	$(CC) $(CFLAGS) -o server chat_server.cpp -lpthread


run:
	client
	server

clean:
	rm -f client server

