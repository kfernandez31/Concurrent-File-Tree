#include "Tree.h"
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define TEST_DIR_COUNT 3
#define COUNT_OF(arr) ((sizeof(arr)/sizeof(arr[0])) / ((size_t)(!(sizeof(arr) % sizeof(arr[0])))))

typedef void (TEST) (size_t);
typedef void* (runnable) (void*);

typedef enum operation {
    LIST = 0,
    CREATE,
    REMOVE,
    MOVE,

    NUM_OPERATIONS
} operation;

static void* runnable_list(void* ignored);
static void* runnable_create(void* ignored);
static void* runnable_remove(void* ignored);
static void* runnable_move(void* ignored);

runnable* operations[NUM_OPERATIONS] = {runnable_list, runnable_create, runnable_remove, runnable_move};
const char *dir_names[] = {"/a/", "/b/", "/c/", "/d/", "/e/", "/f/",
                           "/g/", "/h/", "/i/", "/j/", "/k/", "/l/",
                           "/m/", "/n/", "/o/", "/p/", "/q/", "/r/",
                           "/s/", "/t/", "/u/", "/v/", "/w/", "/x/", "/y/", "/z/"};
const char *example_paths[] = {"/a/", "/b/", "/a/b/", "/b/a/", "/b/a/d/", "/a/b/c/", "/a/b/d/"};
Tree *tree = NULL;
pthread_mutex_t mutex;

/* ------------------------------ Helper functions ------------------------------ */
static void init_mutex(pthread_mutex_t *mtx) {
    pthread_mutexattr_t mutex_attr;
    assert(pthread_mutexattr_init(&mutex_attr) == 0);
    assert(pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK) == 0);
    assert(pthread_mutex_init(mtx, &mutex_attr) == 0);
}

static void destroy_mutex(pthread_mutex_t *mtx) {
    assert(pthread_mutex_destroy(mtx) == 0);
}

static void log_with_thread_name(const char *fmt, ...) {
    pthread_t tid = pthread_self();

    assert(pthread_mutex_lock(&mutex) == 0);

    printf("[THREAD %ld] ", tid);
    va_list fmt_args;
    va_start(fmt_args, fmt);
    vprintf(fmt, fmt_args);
    va_end(fmt_args);
    printf("\n");

    assert(pthread_mutex_unlock(&mutex) == 0);
}

static inline void init_example_tree() {
    tree = tree_new();
    for (size_t i = 0; i < COUNT_OF(example_paths); i++) {
        assert(!tree_create(tree, example_paths[i]));
    }
}

/* ------------------------------ Runnables ------------------------------ */
static void* runnable_list(void* ignored) {
    size_t i = rand() % 2;

    char *str = tree_list(tree, dir_names[i]);

    if (str == NULL) {
        log_with_thread_name("Unable to list node: %s", dir_names[i]);
    }
    else {
        log_with_thread_name("Listing node: %s", dir_names[i], str);
        free(str);
    }

    return 0;
}

static void* runnable_create(void* ignored) {
    size_t i = rand() % TEST_DIR_COUNT;

    if (tree_create(tree, dir_names[i]) == 0) {
        log_with_thread_name("Successfully created directory: %s", dir_names[i]);
    }
    else {
        log_with_thread_name("Unable to create directory: %s", dir_names[i]);
    }

    return 0;
}

static void* runnable_remove(void* ignored) {
    size_t i = rand() % TEST_DIR_COUNT;

    if (tree_remove(tree, dir_names[i]) == 0) {
        log_with_thread_name("Successfully removed directory: %s", dir_names[i]);
    }
    else {
        log_with_thread_name("Unable to remove directory: %s", dir_names[i]);
    }

    return 0;
}

static void* runnable_move(void* ignored) {
    //TODO
    return 0;
}

void TEST_all_operations(size_t num_threads[4]) {
    size_t num_all = num_threads[LIST] + num_threads[CREATE] + num_threads[REMOVE] + num_threads[MOVE];
    pthread_t th[num_all];
    pthread_attr_t attr;
    tree = tree_new();
    assert(pthread_attr_init(&attr) == 0);
    assert(pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE) == 0);

    /* Create runnables and start threads */
    size_t size_used = 0;
    for (size_t i = 0; size_used < num_all; i++) {
        size_t op = i % NUM_OPERATIONS;
        if (num_threads[op] > 0) {
            assert(pthread_create(&th[size_used], &attr, operations[op], 0) == 0);
            num_threads[op]--;
            size_used++;
        }
    }

    /* Wait until threads finish */
    void* retval;
    for (size_t i = 0; i < num_all; i++) {
        assert(pthread_join(th[i], &retval) == 0);
        //assert(retval == NULL);
    }

    assert(pthread_attr_destroy(&attr) == 0);
    tree_free(tree);
}

/* ------------------------------ Sequential tests------------------------------ */
//TODO: templatka z varargs co tworzy drzewko z listy dirsów
void TEST_tree_move_example() {
    Tree *t = tree_new();
    char *str = NULL;

    assert(!tree_create(t, "/a/"));
    assert(!tree_create(t, "/b/"));
    assert(!tree_create(t, "/a/b/"));
    assert(!tree_create(t, "/b/a/"));
    assert(!tree_create(t, "/b/a/d/"));
    assert(!tree_create(t, "/a/b/c/"));
    assert(!tree_create(t, "/a/b/d/"));

    str = tree_list(t, "/a/");
    assert(strcmp(str, "b") == 0);
    free(str);

    str = tree_list(t, "/a/b/");
    assert(strcmp(str, "c,d") == 0);
    free(str);

    str = tree_list(t, "/a/b/c/");
    assert(strcmp(str, "") == 0);
    free(str);

    str = tree_list(t, "/a/b/d/");
    assert(strcmp(str, "") == 0);
    free(str);

    str = tree_list(t, "/b/");
    assert((strcmp(str, "a") == 0));
    free(str);

    str = tree_list(t, "/b/a/");
    assert(strcmp(str, "d") == 0);
    free(str);

    str = tree_list(t, "/b/a/d/");
    assert(strcmp(str, "") == 0);
    free(str);

    assert(!tree_move(t, "/a/b/", "/b/x/"));

    str = tree_list(t, "/a/");
    assert(strcmp(str, "") == 0);
    free(str);

    str = tree_list(t, "/b/");
    assert(strcmp(str, "x,a") == 0);
    free(str);

    str = tree_list(t, "/b/a/");
    assert(strcmp(str, "d") == 0);
    free(str);

    str = tree_list(t, "/b/a/d/");
    assert(strcmp(str, "") == 0);
    free(str);

    str = tree_list(t, "/b/x/");
    assert(strcmp(str, "c,d") == 0);
    free(str);

    str = tree_list(t, "/b/x/c/");
    assert(strcmp(str, "") == 0);
    free(str);

    str = tree_list(t, "/b/x/d/");
    assert(strcmp(str, "") == 0);
    free(str);

    tree_free(t);
}

int main(void) {
    init_mutex(&mutex);

    srand(time(NULL));

    /* Sequential tests */
    TEST_tree_move_example();

    /* Concurrent tests */
    size_t num_threads[NUM_OPERATIONS];

    num_threads[LIST] = 21;
    num_threads[CREATE] = 21;
    num_threads[REMOVE] = 0;
    num_threads[MOVE] = 0;
    TEST_all_operations(num_threads);

    destroy_mutex(&mutex);
    return 0;
}
