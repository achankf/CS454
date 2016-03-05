all:
	gcc *.c -c
	g++ $(DFLAG) $(WFLAG) client1.o -L. -lrpc -o client1 -lpthread
	g++ $(DFLAG) $(WFLAG) server*.o -L. -lrpc -o server -lpthread

.phony: clean

clean:
	rm -f client1 server *.o *.a
