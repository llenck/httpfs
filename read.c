#include "httpfs-ops.h"

#include "fuse-includes.h"
#include "const-inodes.h"
#include "tree.h"
#include "evloop.h"
#include "safe-macros.h"

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

	switch (par) {
	case HEAD_INODE:
	case GET_INODE:
	case DELETE_INODE:
		send_req(newb);
		break;

	default:
		fprintf(stderr, "post/put are not implemented yet\n");
		fuse_reply_err(req, ENOSYS);
		del_req(newb);
		return;

		break;
	}

	fi->direct_io = 1;
	fi->nonseekable = 1;
	fi->fh = (uint64_t)newb;

	fuse_reply_open(req, fi);
}

char test_buf[256] = { 0 };

void httpfs_read(fuse_req_t req, fuse_ino_t ino, size_t n, off_t off,
		struct fuse_file_info* fi)
{
	(void)ino;

	fuse_ino_t par;
	const char* path = get_inode_info(ino, &par);
	if (path == NULL) {
		fuse_reply_err(req, ENOENT);
		return;
	}

	struct req_buf* rqb = (struct req_buf*)fi->fh;
	switch (par) {
	case HEAD_INODE:
	case GET_INODE:
	case DELETE_INODE:
		// TODO add and lock pthread_rwlock in req_buf
		if (rqb->resp_len >= (size_t)off + 1) { // off should be positive
			printf("Answering with %zu bytes\n", MIN(n, rqb->resp_len - off));
			fuse_reply_buf(req, rqb->resp + off, MIN(n, rqb->resp_len - off));
		}
		else {
			// TODO queue the request for the event loop to answer it
			fuse_reply_buf(req, NULL, 0);
		}
		break;

	default:
		fprintf(stderr, "post/put are not implemented yet\n");
		fuse_reply_err(req, ENOSYS);
		return;
	}
}

void httpfs_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
	(void)ino;

	fuse_reply_err(req, 0);

	struct req_buf* rqb = (struct req_buf*)fi->fh;
	if (rqb->sent) {
		del_sent_req(rqb);
	}
	else {
		del_req(rqb);
	}
}
