#include "httpfs-ops.h"

#include "fuse-includes.h"
#include "const-inodes.h"
#include "tree.h"
#include "evloop.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void httpfs_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
	fuse_ino_t par;
	const char* path = get_inode_info(ino, &par);
	if (path == NULL) {
		fuse_reply_err(req, ENOENT);
		return;
	}

	struct req_buf* newb = create_req(path, par);
	if (newb == NULL) {
		fuse_reply_err(req, ENOMEM);
		return;
	}

	fi->direct_io = 1;
	fi->nonseekable = 1;
	fi->fh = (uint64_t)newb;

	fuse_reply_open(req, fi);
}

void httpfs_read(fuse_req_t req, fuse_ino_t ino, size_t n, off_t off,
		struct fuse_file_info* fi)
{
	(void)ino;
	(void)n;
	(void)off;

	fuse_reply_buf(req, NULL, 0);

	struct req_buf* rq = (struct req_buf*)fi->fh;
	// TODO
}

void httpfs_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
	(void)ino;

	fuse_reply_err(req, 0);

	struct req_buf* rq = (struct req_buf*)fi->fh;
	if (rq != NULL) {
		free(rq->url);
		free(rq->resp);
		free(rq);
	}
}
