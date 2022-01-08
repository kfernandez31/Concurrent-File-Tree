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
/*    char lca_path[256];
    char* path1 = "/b/c/a/c/";
    char* path2 = "/b/b/c/a/";
    //TEST 1
    make_path_to_LCA(path1, path2, lca_path);
    printf("lca_path = %s\n", lca_path);
    path1 = "/a/b/c/d/";
    path2 = "/a/b/c/d/";
    //TEST 2
    make_path_to_LCA(path1, path2, lca_path);
    printf("lca_path = %s\n", lca_path);
    path1 = "/a/b/";
    path2 = "/a/b/c/d/";
    //TEST 3
    make_path_to_LCA(path1, path2, lca_path);
    printf("lca_path = %s\n", lca_path);*/

	fprintf(stderr, "Each test/subtest should run in less than 1 second.\n");
	sequential_small();
    valid_path();
    sequential_big_random();
    concurrent_same_as_some_sequential();
//	deadlock(); //stopuje na masce 8
//	liveness();
}
