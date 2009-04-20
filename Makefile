ssh-pageant.exe: main.o winpgntc.o
	$(CC) $(LDFLAGS) $(LOADLIBES) $^ $(LDLIBS) -o $@

main.o: main.c winpgntc.h
winpgntc.o: winpgntc.c winpgntc.h

.PHONY: clean all

clean:
	rm -f ssh-pageant.exe main.o winpgntc.o

all: ssh-pageant.exe

CFLAGS = -O2 -Werror -Wall -Wextra
LDFLAGS = -Wl,--strip-all
