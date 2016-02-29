all:
	gcc *.c -c
	g++ client1.o -L. -lrpc -o client1
	g++ server*.o -L. -lrpc -o server

.phony: clean

clean:
	rm -f client1 server *.o *.a
