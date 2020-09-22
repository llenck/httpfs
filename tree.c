#include "tree.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

struct node* tree = NULL;
struct node* ll_start = NULL, * ll_end = NULL;
int node_count = 0;

pthread_mutex_t tree_mutex = PTHREAD_MUTEX_INITIALIZER;

static void replace_parent_ptr(struct node* nod, struct node* new) {
	struct node* par = nod->parent;
	struct node** rep;
	if (par == NULL) {
		rep = &tree;
	}
	else {
		rep = par->left == nod? &par->left : &par->right;
	}
	*rep = new;
}

static struct node* smallest_right(struct node* nod) {
	nod = nod->right;
	while (nod->left != NULL) {
		nod = nod->left;
	}
	return nod;
}

static struct node* biggest_left(struct node* nod) {
	nod = nod->left;
	while (nod->right != NULL) {
		nod = nod->right;
	}
	return nod;
}

static void delete_from_bintree(struct node* nod) {
	if (nod->left == NULL && nod->right == NULL) {
		// just delete this node
		replace_parent_ptr(nod, NULL);
	}
	else if (nod->left == NULL) {
		// replace our node with nod->right
		replace_parent_ptr(nod, nod->right);
		nod->right->parent = nod->parent;
	}
	else if (nod->right == NULL) {
		// replace our node with nod->left
		replace_parent_ptr(nod, nod->left);
		nod->left->parent = nod->parent;
	}
	else {
		// replace our node with the one with the smallest on the right or the
		// biggest value on the left, alternating between the two strategies to
		// give our tree a statistically good change of being balanced
		struct node* repl;

		// no synchronization necessary as we're already in a tree-global lock, and
		// there's only 1 thread doing deletions anyway
		static unsigned int last_strat = 0;

		if (last_strat) {
			repl = smallest_right(nod);
			repl->parent->left = NULL;
		}
		else {
			repl = biggest_left(nod);
			repl->parent->right = NULL;
		}

		last_strat = !last_strat;

		repl->left = nod->left;
		repl->right = nod->right;
		replace_parent_ptr(nod, repl);
	}
}

static int insert_to_bintree(struct node* nod) {
	nod->left = NULL;
	nod->right = NULL;

	if (tree == NULL) {
		tree = nod;
		nod->parent = NULL;
		return 0;
	}

	struct node* cur = tree;
	struct node** next;

	while (1) {
		if (cur->inode == nod->inode) {
			// prevent hash collisions
			return -1;
		}
		else if (cur->inode > nod->inode) {
			next = &cur->left;
		}
		else {
			next = &cur->right;
		}

		if (*next == NULL) {
			// next place we'd search is empty, so insert nod there
			*next = nod;
			nod->parent = cur;
			return 0;
		}
		else {
			// search for place to insert in subtrees of *next
			cur = *next;
		}
	}
}

static void delete_from_ll(struct node* nod) {
	if (nod->prev) {
		nod->prev->next = nod->next;
	}
	else {
		// nod is the first node
		ll_start = nod->next;
	}
	if (nod->next) {
		nod->next->prev = nod->prev;
	}
	else {
		// nod is the last node
		ll_end = nod->prev;
	}
}

static void append_to_ll(struct node* nod) {
	if (ll_start == NULL) {
		// we're the first and only element
		ll_start = nod;
		nod->prev = NULL;
	}
	else {
		// other elements exist before us
		ll_end->next = nod;
		nod->prev = ll_end;
	}

	// in any case, we're the last element
	ll_end = nod;
	nod->next = NULL;
}

static void delete_node(struct node* nod) {
	delete_from_bintree(nod);

	delete_from_ll(nod);

	free(nod->url);
	free(nod);

	node_count--;
}

static fuse_ino_t xorshift_step(fuse_ino_t in) {
	in ^= in << 13;
	in ^= in >> 17;
	in ^= in << 5;
	return in;
}

// since we check for hash collisions, we don't need a cryptographically secure
// hash function; just do some xorshifts so we get a pretty uniform hash
// distribution, which we need because we don't do any manual tree balancing
static fuse_ino_t hash_url(const unsigned char* url) {
	fuse_ino_t ret = 0;

	while (*url != 0) {
		// combine ret with the entropy of the next character
		ret ^= *url;

		// and do an xorshift
		ret = xorshift_step(ret);

		url++;
	}

	// at the end, do a few more xorshifts so that we get well distributed
	// values even for short urls
	for (int i = 0; i < 10; i++) {
		ret = xorshift_step(ret);
	}

	return ret;
}

int create_node(const char* url) {
	struct node* new_nod = malloc(sizeof(*new_nod));
	if (new_nod == NULL) {
		return -1;
	}

	new_nod->url = strdup(url);
	if (new_nod->url == NULL) {
		goto err_nod_cleanup;
	}

	new_nod->inode = hash_url((const unsigned char*)url);
	new_nod->del_at = time(NULL) + 1200; // delete mapping after 20 minutes

	pthread_mutex_lock(&tree_mutex);

	// insert to binary tree at first, because that can fail, while the linked list
	// append always succeeds
	if (insert_to_bintree(new_nod) < 0) {
		pthread_mutex_unlock(&tree_mutex);

		goto err_full_cleanup;
	}

	append_to_ll(new_nod);

	node_count++;

	pthread_mutex_unlock(&tree_mutex);

	return 0;

err_full_cleanup:
	free(new_nod->url);
err_nod_cleanup:
	free(new_nod);

	return -1;
}

const char* path_of_inode(fuse_ino_t inode) {
	pthread_mutex_lock(&tree_mutex);

	struct node* t = tree;

	while (t) {
		if (t->inode == inode) {
			pthread_mutex_unlock(&tree_mutex);
			return t->url;
		}
		else if (t->inode > inode) {
			t = t->left;
		}
		else {
			t = t->right;
		}
	}

	pthread_mutex_unlock(&tree_mutex);

	return NULL;
}

void clean_old_nodes() {
	pthread_mutex_lock(&tree_mutex);

	time_t now = time(NULL);
	while (ll_start != NULL && ll_start->del_at < now) {
		// delete_node updates ll_start
		delete_node(ll_start);
	}

	pthread_mutex_unlock(&tree_mutex);
}

#ifndef NDEBUG

#include <stdio.h>

static void print_subtree(struct node* cur, int lvl) {
	printf("%*c%016lx -> %s\n", lvl, ' ', cur->inode, cur->url);

	if (cur->left != NULL) {
		printf("%*cleft:\n", lvl, ' ');
		print_subtree(cur->left, lvl + 2);
	}

	if (cur->right != NULL) {
		printf("%*cright:\n", lvl, ' ');
		print_subtree(cur->right, lvl + 2);
	}
}

void print_tree() {
	if (tree == NULL) {
		printf("No elements in tree\n");
		return;
	}

	printf("linked list:\n");

	for (struct node* cur = ll_start; cur != NULL; cur = cur->next) {
		printf("  %016lx -> %s\n", cur->inode, cur->url);
	}

	printf("\ntree:\n");

	print_subtree(tree, 2);

	printf("\n");
}

#endif
