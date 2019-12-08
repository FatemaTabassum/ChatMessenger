CC=g++
CFLAGS=-g -Wall -std=c++11

all: messenger_server messenger_client
	

messenger_server: messenger_server.o
	$(CC) messenger_server.o -o messenger_server

messenger_client: messenger_client.o
	$(CC) messenger_client.o -o messenger_client -pthread

messenger_server.o: messenger_server.cpp
	$(CC) -c $(CFLAGS) messenger_server.cpp

messenger_client.o: messenger_client.cpp
	$(CC) -c $(CFLAGS) -pthread messenger_client.cpp

clean:
	rm -f messenger_server
	rm -f messenger_client
	rm -f *.o
	rm -f *~
	rm -f core
