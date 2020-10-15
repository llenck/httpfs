#include "../read-queue.h"

#include <assert.h>
#include <stdio.h>

int main() {
	struct req_queue queue;
	init_queue(&queue);

	struct read_req rr = { 0, 10, 0, NULL };
	assert(submit_req(&queue, &rr) == 0);

	rr.off = 0;
	rr.n = 10;
	assert(submit_req(&queue, &rr) == 0);

	rr.off = 3;
	rr.n = 5;
	assert(submit_req(&queue, &rr) == 0);

	rr.off = 10;
	rr.n = 0;
	assert(submit_req(&queue, &rr) == 0);

	rr.off = 10;
	rr.n = 10;
	assert(submit_req(&queue, &rr) == 0);

	rr.off = 69;
	rr.n = 17;
	assert(submit_req(&queue, &rr) == 0);

	rr.off = 1;
	rr.n = 100;
	assert(submit_req(&queue, &rr) == 0);

	rr.off = 5;
	rr.n = 6;
	assert(submit_req(&queue, &rr) == 0);

	print_queue(&queue);

	destroy_queue(&queue);
}
