all: stringServer stringClient

common.o: common.cpp common.hpp
	g++ -c common.cpp

stringServer: string_server.cpp common.o
	g++ string_server.cpp common.o -o stringServer

stringClient: string_client.cpp common.o
	g++ string_client.cpp common.o -o stringClient

.phony: style

style:
	astyle --indent=tab --add-one-line-brackets --indent-switches --break-blocks --unpad-paren --delete-empty-lines -A1 *.cpp *.hpp
