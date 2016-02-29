CXFLAGS = $(DFLAG) #-ggdb -pedantic -Wextra -Wall
HEADERS = $(shell find -name "*.hpp")

all: librpc.a binder

BINDER_OBJS = binder.o common.o debug.o name_service.o postman.o sockets.o
binder: $(BINDER_OBJS)
	g++ $(CXFLAGS) $(BINDER_OBJS) -o binder

binder.o: binder.cpp $(HEADERS)
	g++ $(CXFLAGS) binder.cpp -c

LIBRPC_OBJS = common.o debug.o name_service.o postman.o rpc.o sockets.o
librpc.a: $(LIBRPC_OBJS)
	ar rcs librpc.a $(LIBRPC_OBJS)

sockets.o: sockets.cpp $(HEADERS)
	g++ $(CXFLAGS) sockets.cpp -c

rpc.o: rpc.cpp $(HEADERS)
	g++ $(CXFLAGS) rpc.cpp -c

common.o: common.cpp $(HEADERS)
	g++ $(CXFLAGS) common.cpp -c

name_service.o: name_service.cpp $(HEADERS)
	g++ $(CXFLAGS) name_service.cpp -c

postman.o: postman.cpp $(HEADERS)
	g++ $(CXFLAGS) postman.cpp -c

debug.o: debug.cpp $(HEADERS)
	g++ $(CXFLAGS) debug.cpp -c

.phony: style clean

clean:
	rm -f *.a *.o binder

style:
	astyle --indent=tab --add-one-line-brackets --indent-switches --break-blocks --unpad-paren --delete-empty-lines -A1 *.cpp *.hpp
