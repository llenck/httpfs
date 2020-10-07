#ifndef _REQ_H_INCLUDED
#define _REQ_H_INCLUDED

#include "fuse-includes.h"
#include "const-inodes.h"

#include <curl/curl.h>

struct req_buf {
	enum const_inodes par_ino;
	int sent;

	char* url;

	CURL* easy_handle;

	char* body;
	size_t body_len;

	char* resp;
	size_t resp_len;
};

enum evmsg_type { EVMSG_ADD_REQ, EVMSG_DEL_REQ, EVMSG_EXIT };

struct evmsg {
	unsigned char type;
	struct req_buf* req;
} __attribute__((packed));

int start_evloop();
void stop_evloop();

struct req_buf* create_req(const char* url, fuse_ino_t par_ino);

#endif
