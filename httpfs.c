#include "fuse-includes.h"
#include "httpfs-ops.h"
#include "tree.h"
#include "evloop.h"

#include <stdio.h>
#include <stdlib.h>

const struct fuse_lowlevel_ops httpfs_ops = {
	.lookup     = httpfs_lookup,
	.getattr    = httpfs_getattr,
	.readdir    = httpfs_readdir,
	.open       = httpfs_open,
	.read       = httpfs_read,
	.release    = httpfs_release,
};

int main(int argc, char *argv[]) {
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct fuse_session *se;
	struct fuse_cmdline_opts opts;
	struct fuse_loop_config config;
	int ret = -1;

	if (init_tree() < 0) {
		printf("Failed to init locks for the inode-path-map tree\n");
		return 1;
	}

	if (start_evloop() < 0) {
		printf("Failed to start the event loop\n");
		return 1;
	}

	if (fuse_parse_cmdline(&args, &opts) != 0) {
		printf("Failed to parse command line\n");
		goto err_out0;
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

err_out0:
	// stop event loop
	stop_evloop();
	// clean inode to path mapping
	clean_tree();

	return ret ? 1 : 0;
}
