FUSE_CFLAGS := $(shell pkg-config fuse3 --cflags)
FUSE_LIBS := $(shell pkg-config fuse3 --libs)
LIBS := -lcurl -lpthread
TARGETS := tree-test evloop-test

CFLAGS ?= -Wall -Werror -Og -g

all: $(TARGETS)

tree-test: ../tree.c ../tree.h tree-test.c
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -I ../ ../tree.c tree-test.c -o $@

evloop-test: ../evloop.c ../evloop.h evloop-test.c
	$(CC) $(CFLAGS) $(LIBS) $(FUSE_CFLAGS) -I ../ ../evloop.c evloop-test.c -o $@

clean:
	$(RM) $(TARGETS)
