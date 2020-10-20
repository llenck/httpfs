#include "read-queue.h"

#include "fuse-includes.h"

#include <stdlib.h>
#include <pthread.h>

#define par(i) (((i) - 1) / 2)
#define left_child(i) ((i) * 2 + 1)
#define right_child(i) (left_child(i) + 1)

// this might be faster than memcpy, depending on how well gcc inlines memcpy
#define rr_move(queue, dst, src) { \
	struct req_queue* _q = queue; \
	struct read_req* _dst = &_q->q[dst]; \
	struct read_req* _src = &_q->q[src]; \
	_dst->off = _src->off; \
	_dst->n = _src->n; \
	_dst->end = _src->end; \
	_dst->req = _src->req; \
}

// grow the queue if theres no space, and return -1 if not possible
static int can_insert_queue(struct req_queue* queue) {
	if (queue->used < queue->cap) {
		// queue big enough
		return 0;
	}

	int new_cap = queue->cap == 0? 4 : queue->cap * 2;
	struct read_req* new_q = realloc(queue->q, sizeof(*new_q) * new_cap);
	if (new_q == NULL) {
		return -1;
	}

	queue->q = new_q;
	queue->cap = new_cap;

	return 0;
}

static void check_shrink_queue(struct req_queue* queue) {
	if (queue->used <= queue->cap / 4 && queue->cap > 4) {
		int new_cap = queue->cap / 4;
		struct read_req* new_q = realloc(queue->q, sizeof(*new_q) * new_cap);
		if (new_q == NULL) {
			// doesn't really matter, just try again next time
			return;
		}

		queue->cap = new_cap;
		queue->q = new_q;
	}
}

int submit_req(struct req_queue* queue, struct read_req* in) {
	if (can_insert_queue(queue) < 0) {
		return -1;
	}

	// prevent dumb
	in->end = in->off + in->n;

	// "allocate" a new element
	int cur = queue->used;
	queue->used++; // can_insert_queue guarantees that this is still <= queue->cap

	// copy values of cur's parent down to cur and then set cur to its parent while
	// writing the current request to cur wouldn't satisfy the min heap definition
	while (cur != 0 && queue->q[par(cur)].end > in->end) {
		rr_move(queue, cur, par(cur));
		cur = par(cur);
	}

	queue->q[cur].off = in->off;
	queue->q[cur].n = in->n;
	queue->q[cur].end = in->end;
	queue->q[cur].req = in->req;

	return 0;
}

static void raw_peep(struct req_queue* queue, struct read_req* out) {
	if (out != NULL) {
		out->off = queue->q->off;
		out->n = queue->q->n;
		out->end = queue->q->end;
		out->req = queue->q->req;
	}
}

int peep_req(struct req_queue* queue, struct read_req* out) {
	if (queue->used == 0) {
		return -1;
	}

	raw_peep(queue, out);

	return 0;
}

int pop_req(struct req_queue* queue, struct read_req* out) {
	if (queue->used == 0) {
		return -1;
	}

	raw_peep(queue, out);

	int cur = 0;
	int last = queue->used - 1;

	// "free" the last element
	queue->used--;

	// copy values of cur's smaller child up to cur and then set cur to its smaller
	// child while inserting last at cur wouldn't satisfy the min heap definition
	while (1) {
		if (left_child(cur) >= queue->used) {
			// cur has no children to compare with, insert here
			break;
		}

		int smaller_child;
		if (right_child(cur) >= queue->used) {
			// use left as smaller child, as there is no right child
			smaller_child = left_child(cur);
		}
		else {
			// compare the two children
			smaller_child = queue->q[left_child(cur)].end <
				queue->q[right_child(cur)].end? left_child(cur) : right_child(cur);
		}

		if (queue->q[smaller_child].end >= queue->q[last].end) {
			// there are no smaller children then in this subtree; insert last here
			break;
		}

		// smaller_child is smaller than last; copy it to cur, and continue at cur
		rr_move(queue, cur, smaller_child);
		cur = smaller_child;
	}

	// copy last to cur
	rr_move(queue, cur, last);

	// possibly shrink the queue (can only check now because we still need
	// queue->q[last] to be valid until the last rr_move)
	check_shrink_queue(queue);

	return 0;
}

void init_queue(struct req_queue* queue) {
	// TODO: in prod, pre-allocate 32 or so elements in queue->q
	queue->q = NULL;
	queue->cap = 0;
	queue->used = 0;
	pthread_mutex_init(&queue->lock, NULL);
}

void destroy_queue(struct req_queue* queue) {
	free(queue->q);
	queue->cap = 0;
	queue->used = 0;
	pthread_mutex_destroy(&queue->lock);
}

#ifndef NDEBUG

#include <stdio.h>

static void print_subtree(struct req_queue* queue, int cur, int lvl) {
	struct read_req* rr = &queue->q[cur];
	printf("%*c%zu + %zu = %zu (for %p)\n", lvl, ' ', rr->off, rr->n, rr->end, rr->req);

	if (left_child(cur) >= queue->used) {
		return;
	}

	printf("%*cleft:\n", lvl, ' ');
	print_subtree(queue, left_child(cur), lvl + 2);

	if (right_child(cur) >= queue->used) {
		return;
	}

	printf("%*cright:\n", lvl, ' ');
	print_subtree(queue, right_child(cur), lvl + 2);
}

void print_queue(struct req_queue* queue) {
	pthread_mutex_lock(&queue->lock);

	print_subtree(queue, 0, 0);

	pthread_mutex_unlock(&queue->lock);
}

#endif
