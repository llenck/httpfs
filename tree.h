#ifndef _TREE_INCLUDED
#define _TREE_INCLUDED

#include <time.h>

#include "fuse-includes.h"

// tree for searching, but also linked list for triggering timeouts
struct node {
	fuse_ino_t inode;
	char* url;

	time_t del_at;
	struct node* prev, * next;

	struct node* parent, * left, * right;
};

extern int node_count; // only do timeouts if > 0

// the path gets copied, so don't worry about memory management
// return value: 0 on success, -1 on not enough memory
int save_url(const char* url, fuse_ino_t* out_ino);

// returns the path associated with an inode number, as saved in the tree,
// or NULL if search failed
const char* path_of_inode(fuse_ino_t inode);

void clean_old_nodes();

void clean_tree();

#ifndef NDEBUG

void print_tree();

#endif

#endif
