#include "httpfs-ops.h"

#include "fuse-includes.h"
#include "const-inodes.h"
#include "tree.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void httpfs_fill_stats(struct stat* st, fuse_ino_t ino, fuse_ino_t par_ino) {
	memset(st, 0, sizeof(*st));

	// other properties (e.g. access/modification/creation times) are already 0
	st->st_ino = ino;
	st->st_nlink = 1;
	st->st_blksize = 4096;

	if (ino == FUSE_ROOT_ID) {
		st->st_mode = S_IFDIR | 0555;
	}
	else if (par_ino == FUSE_ROOT_ID && inode_to_tld(ino) != NULL) {
		st->st_mode = S_IFDIR | 0755;
	}
	else {
		switch (par_ino) {
		case POST_INODE:
		case PUT_INODE:
			st->st_mode = S_IFREG | 0755;
			break;

		case GET_INODE:
		case HEAD_INODE:
		case DELETE_INODE:
			st->st_mode = S_IFREG | 0555;
			break;

		default:
			st->st_mode = 0;
			break;
		}
	}
}

void httpfs_lookup(fuse_req_t req, fuse_ino_t parent, const char* name) {
	fuse_ino_t inode;

	if (parent == FUSE_ROOT_ID) {
		inode = tld_to_inode(name);

		if (inode == 0) {
			fuse_reply_err(req, ENOENT);
			return;
		}
	}
	else if (inode_to_tld(parent) != NULL) {
		if (save_url(name, &inode, parent) < 0) {
			// could also be ENOMEM, but EINVAL is more likely, and it doesn't really
			// matter all that much anyway
			fuse_reply_err(req, EINVAL);
			return;
		}
	}
	else {
		fuse_reply_err(req, ENOSYS);
		return;
	}

	struct fuse_entry_param e;

	memset(&e, 0, sizeof(e));
	e.ino = inode;
	// use timeouts smaller than the refresh timeout in tree.c
	e.attr_timeout = 600.0; // see above
	e.entry_timeout = 600.0;

	httpfs_fill_stats(&e.attr, inode, parent);

	fuse_reply_entry(req, &e);
}

void httpfs_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
	(void)fi;

	fuse_ino_t par_ino;

	if (get_inode_info(ino, &par_ino) != NULL) {
		fuse_reply_err(req, ENOENT);
		return;
	}

	struct stat st;
	httpfs_fill_stats(&st, ino, par_ino);

	// this timeout is half of the refresh timeout in tree.c
	fuse_reply_attr(req, &st, 600.0);
}

struct dirbuf {
	char* p;
	size_t size;
};

// kinda stolen from fuses hello_ll.c, but there aren't many easy ways to do
// this so I would've ended up with very similar code anyways
static int dirbuf_add(fuse_req_t req, struct dirbuf* b, const char* name,
		fuse_ino_t ino)
{
	size_t offs = b->size;
	b->size += fuse_add_direntry(req, NULL, 0, name, NULL, 0);
	b->p = (char*)realloc(b->p, b->size);
	if (b->p == NULL) {
		free(b->p);
		return -1;
	}

	struct stat st;
	memset(&st, 0, sizeof(st));
	httpfs_fill_stats(&st, ino, 0); // parent ino isn't used anyways, so use 0

	fuse_add_direntry(req, b->p + offs, b->size - offs, name, &st,
			  b->size);

	return 0;
}

#define MIN(a, b) ({ \
	__auto_type _a = a; __auto_type _b = b; \
	_a < _b? _a : _b; \
})

void httpfs_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off,
	struct fuse_file_info* fi)
{
	(void) fi;

	if (ino == FUSE_ROOT_ID) {
		struct dirbuf b = { NULL, 0 };

		if (
			dirbuf_add(req, &b, "get", GET_INODE) < 0 ||
			dirbuf_add(req, &b, "post", POST_INODE) < 0 ||
			dirbuf_add(req, &b, "head", HEAD_INODE) < 0 ||
			dirbuf_add(req, &b, "put", PUT_INODE) < 0 ||
			dirbuf_add(req, &b, "delete", DELETE_INODE) < 0 ||
			dirbuf_add(req, &b, ".", ino) < 0 ||
			dirbuf_add(req, &b, "..", ino) < 0)
		{
			fuse_reply_err(req, ENOMEM);
			return;
		}

		// off should always be positive, so cast to size_t
		if ((size_t)off >= b.size) {
			fuse_reply_buf(req, NULL, 0);
		}
		else {
			fuse_reply_buf(req, b.p + off, MIN(b.size - off, size));
		}

		free(b.p);
	}
	else if (inode_to_tld(ino) != NULL) {
		struct dirbuf b = { NULL, 0 };

		if (
			dirbuf_add(req, &b, ".", ino) < 0 ||
			dirbuf_add(req, &b, "..", FUSE_ROOT_ID) < 0)
		{
			fuse_reply_err(req, ENOMEM);
			return;
		}

		// off should always be positive, so cast to size_t
		if ((size_t)off >= b.size) {
			fuse_reply_buf(req, NULL, 0);
		}
		else {
			fuse_reply_buf(req, b.p + off, MIN(b.size - off, size));
		}

		free(b.p);
	}
	else {
		if (get_inode_info(ino, NULL) != NULL) {
			fuse_reply_err(req, ENOTDIR);
		}
		else {
			fuse_reply_err(req, ENOENT);
		}

		return;
	}
}
