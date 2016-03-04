CXX = g++
CXFLAGS = $(DFLAG) $(OFLAG) $(WFLAG)
HEADERS = $(shell find -name "*.hpp")

all: librpc.a binder

BINDER_OBJS = binder.o common.o debug.o name_service.o postman.o sockets.o
binder: $(BINDER_OBJS)
	$(CXX) $(CXFLAGS) $(BINDER_OBJS) -o binder

binder.o: binder.cpp $(HEADERS)
	$(CXX) $(CXFLAGS) binder.cpp -c

LIBRPC_OBJS = common.o debug.o name_service.o postman.o rpc.o sockets.o tasks.o
librpc.a: $(LIBRPC_OBJS)
	ar rcs librpc.a $(LIBRPC_OBJS)

sockets.o: sockets.cpp $(HEADERS)
	$(CXX) $(CXFLAGS) sockets.cpp -c

rpc.o: rpc.cpp $(HEADERS)
	$(CXX) $(CXFLAGS) rpc.cpp -c

common.o: common.cpp $(HEADERS)
	$(CXX) $(CXFLAGS) common.cpp -c

name_service.o: name_service.cpp $(HEADERS)
	$(CXX) $(CXFLAGS) name_service.cpp -c

postman.o: postman.cpp $(HEADERS)
	$(CXX) $(CXFLAGS) postman.cpp -c

debug.o: debug.cpp $(HEADERS)
	$(CXX) $(CXFLAGS) debug.cpp -c

tasks.o: tasks.cpp $(HEADERS)
	$(CXX) $(CXFLAGS) tasks.cpp -c

.phony: style clean

clean:
	rm -f *.a *.o binder

style:
	astyle --indent=tab --add-one-line-brackets --indent-switches --break-blocks --unpad-paren --delete-empty-lines -A1 *.cpp *.hpp
