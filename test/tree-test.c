#include "../tree.h"
#include "../fuse-includes.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

int main() {
	assert(init_tree() == 0);

	// insert some elements to see whether they are more-or-less evenly distributed
	assert(save_url("test", NULL, 1) == 0);
	assert(save_url("test2", NULL, 1) == 0);
	assert(save_url("bruh", NULL, 1) == 0);
	assert(save_url("1", NULL, 1) == 0);
	assert(save_url("2", NULL, 1) == 0);
	assert(save_url("3", NULL, 1) == 0);
	assert(save_url("aaaaaaaaaaaaaaa", NULL, 1) == 0);

	print_tree();

	clean_tree();

	// test whether duplicate entries are handled correctly
	fuse_ino_t first = 0, second = 0;
	assert(save_url("duplicate", &first, 1) == 0);
	assert(save_url("duplicate", &second, 1) == 0);
	assert(first == second && first != 0 && second != 0);

	print_tree();

	clean_tree();

	// test node refreshing
	fuse_ino_t tmp;
	assert(save_url("test", &tmp, 1) == 0);
	assert(save_url("test2", NULL, 1) == 0);
	sleep(2);
	assert(save_url("bruh", NULL, 1) == 0);

	print_tree();

	sleep(2);
	refresh_inode(tmp);

	print_tree();

	clean_tree();

	return 0;
}
