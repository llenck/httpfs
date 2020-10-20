#include "httpfs-ops.h"

#include "fuse-includes.h"
#include "const-inodes.h"
#include "tree.h"
#include "evloop.h"
#include "safe-macros.h"
#include "read-queue.h"

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
			fuse_reply_buf(req, rqb->resp + off, MIN(n, rqb->resp_len - off));
		}
		else if (!__atomic_load_n(&rqb->resp_finished, __ATOMIC_ACQUIRE)) {
			// if we might receive the bytes requested later, try to submit the request
			// to the read queue, for this file handle, otherwise answer with ENOMEM
			lock_queue(&rqb->read_queue);
			struct read_req rr = { off, n, 0, req };
			if (submit_req(&rqb->read_queue, &rr) < 0) {
				fuse_reply_err(req, ENOMEM);
			}
			unlock_queue(&rqb->read_queue);
		}
		else {
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
