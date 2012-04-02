C=gcc
FLAGS= -fPIC -Wall -Wextra -DDEBUG

all: libcall.o libtest.o libtest.so libcall.so test

libcall.o: call_cnt.h call_cnt.c
	$(C) $(FLAGS) -c call_cnt.c -o libcall.o

libcall.so: libcall.o
	ld -shared -soname libcall.so.1 -o libcall.so.1 -lc libcall.o -ldl
	ln -sf libcall.so.1 libcall.so

libtest.o: libtest.h libtest.c
	$(C) $(FLAGS) -c libtest.c -o libtest.o

libtest.so: libtest.o
	ld -shared -soname libtest.so.1 -o libtest.so.1 -lc libtest.o
	ln -sf libtest.so.1 libtest.so

test: libcall.o test.c
	$(C) test.c -o test -L. -lcall -ltest -pthread

clean:
	rm -rf *~ libcall.o test libcall.so.1 libcall.so libtest.o libtest.so libtest.so.1
