all: stringServer stringClient

FLAGS = -DNDEBUG -ggdb -pedantic -Wextra -Wall

sockets.o: sockets.cpp sockets.hpp
	g++ $(FLAGS) -c sockets.cpp

channel.o: channel.cpp channel.hpp
	g++ $(FLAGS) -c channel.cpp

stringServer: string_server.cpp sockets.o channel.o
	g++ $(FLAGS) string_server.cpp sockets.o channel.o -o stringServer

stringClient: string_client.cpp sockets.o channel.o
	g++ $(FLAGS) -pthread string_client.cpp sockets.o channel.o -o stringClient

.phony: style clean

style:
	astyle --indent=tab --add-one-line-brackets --indent-switches --break-blocks --unpad-paren --delete-empty-lines -A1 *.cpp *.hpp

clean:
	rm -f *.o stringServer stringClient
