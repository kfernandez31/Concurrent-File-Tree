#include "HashMap.h"
#include "Tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_TREE_LIST   1

#define NUM_TREE_CREATE 1
#define NUM_TREE_REMOVE 1
#define NUM_TREE_MOVE   1
#define NUM_TREE_FREE   1
#define NUM_ALL         NUM_TREE_LIST + NUM_TREE_CREATE + NUM_TREE_REMOVE + NUM_TREE_MOVE + NUM_TREE_FREE

int main() {
    pthread_t th[NUM_ALL];
    pthread_attr_t attr;
    void *retval;

    err_check(pthread_attr_init(&attr), "pthread_attr_init");
    err_check(pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE), "pthread_attr_setdetachstate");

    for (size_t i = 0; i < NUM_TREE_LIST; i++) {
        err_check(pthread_create(&th[i], &attr, reader, 0), "pthread_create");
    }
    for (size_t i = 0; i < NUM_TREE_CREATE; i++) {
        err_check(pthread_create(&th[i], &attr, reader, 0), "pthread_create");
    }
    for (size_t i = 0; i < NUM_TREE_REMOVE; i++) {
        err_check(pthread_create(&th[i], &attr, reader, 0), "pthread_create");
    }
    for (size_t i = 0; i < NUM_TREE_MOVE; i++) {
        err_check(pthread_create(&th[i], &attr, reader, 0), "pthread_create");
    }
    for (size_t i = 0; i < NUM_TREE_FREE; i++) {
        err_check(pthread_create(&th[i], &attr, reader, 0), "pthread_create");
    }

    for (size_t i = 0; i < NUM_ALL; i++) {
        err_check(pthread_join(th[i], &retval), "pthread_join");
    }

    err_check(pthread_attr_destroy(&attr), "pthread_attr_destroy");

    return EXIT_SUCCESS;
}
