#include "../read-queue.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

struct tuple {
	int a, b;
};

struct tuple nums[] = { {0, 10}, {10, 0}, {5, 5}, {3, 5}, {10, 10}, {69, 17}, {5, 6} };

int main() {
	struct req_queue queue;
	init_queue(&queue);

	struct read_req rr = { 0 };

	struct read_req min = { ~0, ~0, ~0, NULL };

	// insert some elements before printing
	for (int i = 0; i < sizeof(nums) / sizeof(struct tuple); i++) {
		struct tuple* cur = &nums[i];
		rr.off = cur->a;
		rr.n =  cur->b;
		assert(submit_req(&queue, &rr) == 0);

		size_t end = cur->a + cur->b;
		if (end < min.end) {
			min.off = cur->a;
			min.n = cur->b;
			min.end = end;
		}
	}

	assert(peep_req(&queue, &rr) == 0);

	// using memcmp this way is probably struct packing dependent, but since
	// sizeof(struct read_req) == 24 on pretty much all platforms, this should work
	assert(memcmp(&rr, &min, sizeof(rr)) == 0);

	// check twice
	assert(peep_req(&queue, &rr) == 0);
	assert(memcmp(&rr, &min, sizeof(rr)) == 0);

	print_queue(&queue);

	destroy_queue(&queue);
}
