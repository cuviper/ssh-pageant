SRCS = main.c winpgntc.c
HDRS = winpgntc.h

OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

ssh-pageant.exe: $(OBJS)
	$(CC) $(LDFLAGS) $(LOADLIBES) $^ $(LDLIBS) -o $@

.PHONY: clean all cscope

clean:
	rm -f ssh-pageant.exe cscope.out $(OBJS) $(DEPS)

all: ssh-pageant.exe

CFLAGS = -O2 -Werror -Wall -Wextra -MMD
LDFLAGS = -Wl,--strip-all

CSCOPE = $(firstword $(shell which cscope mlcscope 2>/dev/null) false)
cscope: cscope.out
cscope.out: $(SRCS) $(HDRS)
	$(CSCOPE) -b $^

-include $(DEPS)
