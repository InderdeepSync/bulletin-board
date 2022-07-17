
all: super-server.o
	g++ -lpthread -o bbserv tcp-utils.o utilities.o board-server.o sync-server.o super-server.o file-operations.o logger.o peer-communication.o

super-server.o: board-server.o sync-server.o
	g++ -lpthread -c -o super-server.o main.cpp

board-server.o: tcp-utils.o utilities.o file-operations.o logger.o peer-communication.o
	g++ -lpthread -c -o board-server.o board-server.cpp

sync-server.o: tcp-utils.o utilities.o file-operations.o logger.o
	g++ -lpthread -c -o sync-server.o sync-server.cpp

peer-communication.o: tcp-utils.o utilities.o file-operations.o logger.o
	g++ -lpthread -c -o peer-communication.o peer-communication.cpp

tcp-utils.o:
	g++ -lpthread -c -o tcp-utils.o tcp-utils.cpp

utilities.o:
	g++ -lpthread -c -o utilities.o utilities.cpp

file-operations.o:
	g++ -lpthread -c -o file-operations.o file-operations.cpp

logger.o:
	g++ -lpthread -c -o logger.o logger.cpp

clean:
	rm -f bbserv board-server.o super-server.o sync-server.o tcp-utils.o utilities.o file-operations.o logger.o peer-communication.o bbserv.log
