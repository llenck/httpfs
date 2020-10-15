#include "../read-queue.h"

#include <assert.h>
#include <stdio.h>

struct tuple {
	int a, b;
};

struct tuple nums[] = { {0, 10}, {10, 0}, {5, 5}, {3, 5}, {10, 10}, {69, 17}, {5, 6} };

int main() {
	struct req_queue queue;
	init_queue(&queue);

	struct read_req rr = { 0 };

	// insert some elements before printing
	for (int i = 0; i < sizeof(nums) / sizeof(struct tuple); i++) {
		rr.off =  nums[i].a;
		rr.n =  nums[i].b;
		assert(submit_req(&queue, &rr) == 0);
	}

	print_queue(&queue);

	destroy_queue(&queue);
}
