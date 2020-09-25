#include "fuse-includes.h"
#include "const-inodes.h"
#include "tree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

void httpfs_fill_stats(struct stat* st, fuse_ino_t ino, fuse_ino_t par_ino) {
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
			st->st_mode = S_IFREG | 0555;
			break;

		case GET_INODE:
		case HEAD_INODE:
		case DELETE_INODE:
			st->st_mode = S_IFREG | 0755;
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

const struct fuse_lowlevel_ops httpfs_ops = {
	.lookup		= httpfs_lookup,
	.getattr	= httpfs_getattr,
//	.readdir	= httpfs_readdir,
//	.open		= httpfs_open,
//	.read		= httpfs_read,
};

int main(int argc, char *argv[]) {
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_session *se;
	struct fuse_cmdline_opts opts;
	struct fuse_loop_config config;
	int ret = -1;

	if (fuse_parse_cmdline(&args, &opts) != 0) {
		printf("Failed to parse command line\n");
		return 1;
	}

	if (opts.show_help) {
		printf("usage: %s [options] <mountpoint>\n\n", argv[0]);
		fuse_cmdline_help();
		fuse_lowlevel_help();
		ret = 0;
		goto err_out1;
	}
	else if (opts.show_version) {
		printf("FUSE library version %s\n", fuse_pkgversion());
		fuse_lowlevel_version();
		ret = 0;
		goto err_out1;
	}

	if (opts.mountpoint == NULL) {
		printf("usage: %s [options] <mountpoint>\n", argv[0]);
		printf("       %s --help\n", argv[0]);
		ret = 1;
		goto err_out1;
	}

	se = fuse_session_new(&args, &httpfs_ops, sizeof(httpfs_ops), NULL);
	if (se == NULL)
	    goto err_out1;

	if (fuse_set_signal_handlers(se) != 0)
	    goto err_out2;

	if (fuse_session_mount(se, opts.mountpoint) != 0)
	    goto err_out3;

	fuse_daemonize(opts.foreground);

	/* Block until ctrl+c or fusermount -u */
	if (opts.singlethread)
		ret = fuse_session_loop(se);
	else {
		config.clone_fd = opts.clone_fd;
		config.max_idle_threads = opts.max_idle_threads;
		ret = fuse_session_loop_mt(se, &config);
	}

	fuse_session_unmount(se);
err_out3:
	fuse_remove_signal_handlers(se);
err_out2:
	fuse_session_destroy(se);
err_out1:
	free(opts.mountpoint);
	fuse_opt_free_args(&args);

	return ret ? 1 : 0;
}
