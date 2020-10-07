#include "../evloop.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

int main() {
	assert(start_evloop() == 0);

	stop_evloop();
}
