#include "sequential_small.h"
#include "sequential_big_random.h"
#include "valid_path.h"
#include "deadlock.h"
#include "concurrent_same_as_some_sequential.h"
#include "liveness.h"

#include <stdio.h>

#define RUN_TEST(f) \
	fprintf(stderr, "Running test " #f "...\n"); \
	f()

#include "../path_utils.h"

int main() {
	fprintf(stderr, "Each test/subtest should run in less than 1 second.\n");
//	sequential_small();
//    valid_path();
//    sequential_big_random();
//    concurrent_same_as_some_sequential();
	deadlock();
//	liveness();
}
