#include "../tree.h"
#include "../fuse-includes.h"

#include <assert.h>
#include <stdio.h>

int main() {
	assert(save_url("test", NULL) == 0);
	assert(save_url("test2", NULL) == 0);
	assert(save_url("bruh", NULL) == 0);
	assert(save_url("1", NULL) == 0);
	assert(save_url("2", NULL) == 0);
	assert(save_url("3", NULL) == 0);
	assert(save_url("aaaaaaaaaaaaaaa", NULL) == 0);

	print_tree();

	clean_tree();

	fuse_ino_t first = 0, second = 0;
	assert(save_url("duplicate", &first) == 0);
	assert(save_url("duplicate", &second) == 0);
	assert(first == second && first != 0 && second != 0);

	print_tree();

	clean_tree();

	return 0;
}
