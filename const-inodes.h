#ifndef _CONST_INODE_DEFS_INCLUDED
#define _CONST_INODE_DEFS_INCLUDED

// todo/wontdo: implement put, delete, options, connect, patch and trace methods

// use a random-ish number as a start, because FUSE might have some special numbers
// itself (e.g. 1, which is FUSE_ROOT in my installation)
enum const_inodes { GET_INODE = 0x314159, POST_INODE, HEAD_INODE, PUT_INODE,
	DELETE_INODE };

// 0 on error, a const_inode enum member otherwise
fuse_ino_t tld_to_inode(const char* dir);

const char* inode_to_tld(fuse_ino_t ino);

#endif
