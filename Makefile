C=gcc
FLAGS= -fPIC -Wall -Wextra -DDEBUG

all: libcall_cnt.o libtest.o libtest.so libcall_cnt.so test

libcall_cnt.o: call_cnt.h call_cnt.c
	$(C) $(FLAGS) -c call_cnt.c -o libcall_cnt.o

libcall_cnt.so: libcall_cnt.o
	ld -shared -soname libcall_cnt.so -o libcall_cnt.so -lc libcall_cnt.o -ldl

libtest.o: libtest.h libtest.c
	$(C) $(FLAGS) -c libtest.c -o libtest.o

libtest.so: libtest.o
	ld -shared -soname libtest.so.1 -o libtest.so.1 -lc libtest.o
	ln -sf libtest.so.1 libtest.so

test: libcall_cnt.o test.c
	$(C) test.c -o test -L. -lcall_cnt -ltest -pthread

clean:
	rm -rf *~ libcall_cnt.o test libcall_cnt.so libtest.o libtest.so libtest.so.1
