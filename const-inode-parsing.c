#include "fuse-includes.h"
#include "const-inodes.h"

#include <string.h>
#include <ctype.h>

static void copy_as_lower(char* restrict dst, const char* restrict src) {
	do {
		*dst++ = tolower(*src++);
	} while (*src != '\0');
}

fuse_ino_t tld_to_inode(const char* dir) {
	int dir_len = strlen(dir);

	char dir_lower[dir_len + 1];

	copy_as_lower(dir_lower, dir);

	// this will probably compile to a jump table, and then there are at max
	// 2 candidates for a given length, so this should be quite fast
	switch (dir_len) {
	case 3:
		if (!memcmp(dir_lower, "get", dir_len))
			return GET_INODE;
		if (!memcmp(dir_lower, "put", dir_len))
			return PUT_INODE;
		return 0;
		break;

	case 4:
		if (!memcmp(dir_lower, "post", dir_len))
			return POST_INODE;
		if (!memcmp(dir_lower, "head", dir_len))
			return HEAD_INODE;
		return 0;
		break;

	case 6:
		if (!memcmp(dir_lower, "delete", dir_len))
			return DELETE_INODE;
		return 0;
		break;

	default:
		return 0;
		break;
	}
}

const char* inode_to_tld(fuse_ino_t ino) {
	switch (ino) {
	case GET_INODE:
		return "get";
		break;

	case POST_INODE:
		return "post";
		break;

	case HEAD_INODE:
		return "head";
		break;

	case PUT_INODE:
		return "put";
		break;

	case DELETE_INODE:
		return "get";
		break;

	default:
		return NULL;
		break;
	}
}
