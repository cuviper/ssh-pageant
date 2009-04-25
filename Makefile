ssh-pageant.exe: main.o winpgntc.o
	$(CC) $(LDFLAGS) $(LOADLIBES) $^ $(LDLIBS) -o $@

main.o: main.c winpgntc.h
winpgntc.o: winpgntc.c winpgntc.h

.PHONY: clean all cscope

clean:
	rm -f ssh-pageant.exe main.o winpgntc.o cscope.out

all: ssh-pageant.exe

CFLAGS = -O2 -Werror -Wall -Wextra
LDFLAGS = -Wl,--strip-all

CSCOPE = $(firstword $(shell which cscope mlcscope 2>/dev/null) false)
cscope: cscope.out
cscope.out: main.c winpgntc.c winpgntc.h
	$(CSCOPE) -b $^
