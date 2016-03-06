all:
	gcc *.c -c
	g++ $(DFLAG) $(WFLAG) client1.o -L. -lrpc -o client1 -lpthread
	g++ $(DFLAG) $(WFLAG) client2.o -L. -lrpc -o client2 -lpthread
	g++ $(DFLAG) $(WFLAG) client3.o -L. -lrpc -o client3 -lpthread
	g++ $(DFLAG) $(WFLAG) server.o server_*.o -L. -lrpc -o server -lpthread
	g++ $(DFLAG) $(WFLAG) server2.o server_*.o -L. -lrpc -o server2 -lpthread

.phony: clean

clean:
	rm -f client1 server *.o *.a
