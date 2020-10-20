#ifndef _REQ_H_INCLUDED
#define _REQ_H_INCLUDED

#include "fuse-includes.h"
#include "const-inodes.h"
#include "read-queue.h"

#include <curl/curl.h>

struct req_buf {
	enum const_inodes par_ino;
	int sent;

	char* url;

	CURL* easy_handle;

	// request body, if post or put
	char* body;
	size_t body_len;

	// response body
	char* resp;
	size_t resp_len;

	// when someone calls read and we don't have enough bytes yet, let the event loop
	// answer the request later
	struct req_queue read_queue;

	unsigned char resp_finished; // can't be a bit field because we use gcc atomics on it
	unsigned char handle_on_multi: 1;
};

enum evmsg_type { EVMSG_ADD_REQ, EVMSG_DEL_REQ, EVMSG_EXIT };

struct evmsg {
	unsigned char type;
	struct req_buf* req;
} __attribute__((packed));

int start_evloop();
void stop_evloop();

struct req_buf* create_req(const char* url, fuse_ino_t par_ino);

int send_req(struct req_buf* req);

// del_req frees just the req_buf members in the calling thread, while del_sent_req
// sends a request to the event loop to stop the request and then use del_req
void del_req(struct req_buf* req);
int del_sent_req(struct req_buf* req);

#endif
