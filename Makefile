PREFIX = /usr
BINDIR = $(PREFIX)/bin
DOCDIR = $(PREFIX)/share/doc/ssh-pageant
MANDIR = $(PREFIX)/share/man/man1

PROGRAM = ssh-pageant.exe
SRCS = main.c winpgntc.c
HDRS = winpgntc.h
MANPAGE = ssh-pageant.1
DOCS = README COPYING COPYING.PuTTY

OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

.PHONY: clean all install uninstall cscope

all: $(PROGRAM)

clean:
	rm -f cscope.out $(PROGRAM) $(OBJS) $(DEPS)

install: all
	install -d $(BINDIR) $(DOCDIR) $(MANDIR)
	install -p -t $(BINDIR) $(PROGRAM)
	install -p -m 0644 -t $(DOCDIR) $(DOCS)
	install -p -m 0644 -t $(MANDIR) $(MANPAGE)

uninstall:
	rm -f $(BINDIR)/$(PROGRAM)
	rm -f $(MANDIR)/$(MANPAGE)
	rm -rf $(DOCDIR)

$(PROGRAM): $(OBJS)
	$(CC) $(LDFLAGS) $(LOADLIBES) $^ $(LDLIBS) -o $@

CFLAGS = -O2 -Werror -Wall -Wextra -MMD

CSCOPE = $(firstword $(shell which cscope mlcscope 2>/dev/null) false)
cscope: cscope.out
cscope.out: $(SRCS) $(HDRS)
	$(CSCOPE) -b $^

-include $(DEPS)
