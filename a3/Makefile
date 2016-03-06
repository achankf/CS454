all:
	make -C src DFLAG="-DNDEBUG" WFLAG="-pedantic -Wextra -Wall"
	cp src/librpc.a .
	cp src/binder .

.phony: clean

clean:
	rm -f binder librpc.a
	make -C src clean
