#ifndef _REQ_H_INCLUDED
#define _REQ_H_INCLUDED

#include "fuse-includes.h"
#include "const-inodes.h"

struct req_buf {
	enum const_inodes par_ino;

	char* url;

	char* resp;
	size_t resp_len;
};

#endif
