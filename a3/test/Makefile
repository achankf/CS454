DFLAG = -ggdb -pedantic
WFLAG = -Wextra -Wall
LIBS = -L. -lrpc -lpthread

all:
	gcc *.c -c
	g++ *.cpp -I../src -c
	g++ $(DFLAG) $(WFLAG) client1.o -o client1 $(LIBS)
	g++ $(DFLAG) $(WFLAG) client2.o -o client2 $(LIBS)
	g++ $(DFLAG) $(WFLAG) client3.o -o client3 $(LIBS)
	g++ $(DFLAG) $(WFLAG) bad_client1.o -o bad_client1 $(LIBS)
	g++ $(DFLAG) $(WFLAG) server.o server_*.o -o server $(LIBS)
	g++ $(DFLAG) $(WFLAG) server2.o server_*.o -o server2 $(LIBS)
	g++ $(DFLAG) $(WFLAG) bad_server1.o -o bad_server1 $(LIBS)

.phony: clean

clean:
	rm -f client1 client2 client3  server server2 bad_server1 bad_client1 *.o *.a
