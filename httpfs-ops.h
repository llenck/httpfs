#ifndef _HTTPFS_OPS_INCLUDED
#define _HTTPFS_OPS_INCLUDED

#include "fuse-includes.h"

void httpfs_lookup(fuse_req_t req, fuse_ino_t parent, const char* name);
void httpfs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi);
void httpfs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
	struct fuse_file_info* fi);

#endif
