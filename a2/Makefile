all: stringServer stringClient

sockets.o: sockets.cpp sockets.hpp
	g++ -c sockets.cpp

stringServer: string_server.cpp sockets.o
	g++ string_server.cpp sockets.o -o stringServer

stringClient: string_client.cpp sockets.o
	g++ string_client.cpp sockets.o -o stringClient

.phony: style clean

style:
	astyle --indent=tab --add-one-line-brackets --indent-switches --break-blocks --unpad-paren --delete-empty-lines -A1 *.cpp *.hpp

clean:
	rm *.o stringServer stringClient
