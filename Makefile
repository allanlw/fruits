CFLAGS=-fstack-protector
CFLAGS+=-Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter
CFLAGS+=-fpie -pie -O2
CFLAGS+=-fno-rtti -fno-exceptions
CFLAGS+=-fno-inline-functions -fno-default-inline -fno-inline-small-functions

main: main.o
	gcc -o $@ $< $(CFLAGS)
#	strip -s main

main.o: main.cc
	g++ -c -o $@ $< $(CFLAGS)

clean:
	rm -rf main main.o
