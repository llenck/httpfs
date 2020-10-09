#include "../evloop.h"
#include "../const-inodes.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

int main() {
	assert(start_evloop() == 0);

	// test deleting a request before sending it
	struct req_buf* rqb = create_req("kek", GET_INODE);
	del_req(rqb);

	// test receiving a request fully and then deleting it
	rqb = create_req("http://api.ipify.org", GET_INODE);
	assert(rqb != NULL);

	assert(send_req(rqb) == 0);

	sleep(2);

	assert(del_sent_req(rqb) == 0);

	// test deleting a partly received request
	rqb = create_req("http://speed.hetzner.de/10GB.bin", GET_INODE);
	assert(rqb != NULL);

	assert(send_req(rqb) == 0);

	// don't test this on a server with 10GB/s download :)
	sleep(1);

	assert(del_sent_req(rqb) == 0);

	// test head requests
	rqb = create_req("http://api.ipify.org", HEAD_INODE);
	assert(rqb != NULL);

	assert(send_req(rqb) == 0);

	sleep(2);

	printf("Response to head request: %.*s\n", (int)rqb->resp_len, rqb->resp);

	assert(del_sent_req(rqb) == 0);

	stop_evloop();
}
