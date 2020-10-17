#include "../read-queue.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

struct tuple {
	int a, b;
};

struct tuple nums[] = { {0, 10}, {10, 0}, {5, 5}, {3, 5}, {10, 10}, {69, 17}, {5, 6}, {123, 321} };
int num_pairs = sizeof(nums) / sizeof(*nums);

int main() {
	struct req_queue queue;
	init_queue(&queue);

	struct read_req rr = { 0 };

	struct read_req min = { ~0, ~0, ~0, NULL };

	// insert some elements before printing
	for (int i = 0; i < num_pairs; i++) {
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

	printf("Before popping the top two request:\n");
	print_queue(&queue);

	// when popping, it we should get min the first time, and another value after
	assert(pop_req(&queue, &rr) == 0);
	assert(memcmp(&rr, &min, sizeof(rr)) == 0);

	printf("\nAfter popping the first time:\n");
	print_queue(&queue);

	assert(pop_req(&queue, &rr) == 0);
	assert(memcmp(&rr, &min, sizeof(rr)) != 0);

	printf("\nAfter popping twice:\n");
	print_queue(&queue);

	// also pop more elements until the queue should shrink
	int elements_left = num_pairs - 2;
	for (int i = 0; i < elements_left - 2; i++) {
		assert(pop_req(&queue, &rr) == 0);
	}

	destroy_queue(&queue);
}
