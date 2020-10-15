#ifndef _READ_QUEUE_INCLUDED
#define _READ_QUEUE_INCLUDED

#include "fuse-includes.h"

#include <pthread.h>

struct read_req {
	size_t off;
	size_t n;
	size_t end;
	fuse_req_t req;
};

struct req_queue {
	// use an implicit data structure on an array, as described by eg wikipedia.
	// this will give insert/delete operations a worst case of O(n), but we can still
	// get O(log n) amortized by doubling/dividing the length by 4 when we need to resize
	// it. using pointers would still not give O(1) insert, but O(log n) insert (when
	// counting the number of child nodes in nodes to find the best place for
	// insertion), so this is probably faster
	struct read_req* q;
	int cap;
	int used;

	// a readers/writer lock doesn't make sense here as there is only ever 1 reader
	pthread_mutex_t lock;
};

int submit_req(struct req_queue* queue, struct read_req* in);
void peep_req(struct req_queue* queue, struct read_req* out);
void pop_req(struct req_queue* queue, struct read_req* out);
void init_queue(struct req_queue* queue);
void destroy_queue(struct req_queue* queue);

#ifndef NDEBUG

void print_queue(struct req_queue* queue);

#endif

#endif
