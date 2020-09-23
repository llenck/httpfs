FUSE_CFLAGS := $(shell pkg-config fuse3 --cflags)
FUSE_LIBS := $(shell pkg-config fuse3 --libs)

CFLAGS ?= -Wall -Werror -Og -g

tree-test: ../tree.c ../tree.h tree-test.c
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -I ../ ../tree.c tree-test.c -o $@

clean:
	$(RM) tree-test
