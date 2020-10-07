FUSE_CFLAGS := $(shell pkg-config fuse3 --cflags)
FUSE_LIBS := $(shell pkg-config fuse3 --libs)
CURL_LIBS := -lcurl
OBJECT_FILES := httpfs.o tree.o const-inode-parsing.o stat.o read.o evloop.o
HEADER_FILES := httpfs-ops.h tree.h const-inodes.h evloop.h fuse-includes.h

CFLAGS ?= -Wall -Wextra -O3 -DNDEBUG

httpfs: $(OBJECT_FILES) $(HEADER_FILES)
	$(CC) $(CFLAGS) $(FUSE_LIBS) $(CURL_LIBS) $(OBJECT_FILES) -o httpfs

%.o: %.c $(HEADER_FILES)
	$(CC) $(CFLAGS) $(FUSE_CFLAGS) -c $< -o $@

clean:
	$(RM) httpfs $(OBJECT_FILES)
