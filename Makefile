FUSE_CFLAGS := $(shell pkg-config fuse3 --cflags)
FUSE_LIBS := $(shell pkg-config fuse3 --libs)

CFLAGS ?= -Wall -Wextra -O3 -DNDEBUG

httpfs: httpfs.o tree.o const-inode-parsing.o stat.o read.o
	$(CC) $(CFLAGS) $(FUSE_LIBS) httpfs.o tree.o const-inode-parsing.o stat.o read.o -o httpfs

%.o: %.c
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -c $< -o $@

clean:
	$(RM) httpfs httpfs.o tree.o const-inode-parsing.o stat.o read.o
