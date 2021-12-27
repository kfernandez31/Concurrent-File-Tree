#include "Tree.h"
#include <stdarg.h>

typedef void (TEST) (size_t);
typedef void (runnable) (void*);

/* ------------------------------ Parametrized macros ------------------------------ */

#define PRINT_AND_FREE(s, n)  \
do {                    \
    if (str == NULL) { \
        printf("NULL [%d]\n", n);\
    } \
    else {\
        printf("%s [%d]\n", str, n);\
        free(str);        \
        str = NULL;\
    }\
} while (0)

#define COUNT_OF(arr) ((sizeof(arr)/sizeof(arr[0])) / ((size_t)(!(sizeof(arr) % sizeof(arr[0])))))

/* ------------------------------ Constants ------------------------------ */
#define NUM_TREE_LIST   1
#define NUM_TREE_CREATE 1
#define NUM_TREE_REMOVE 1
#define NUM_TREE_MOVE   1
#define NUM_ALL         NUM_TREE_LIST + NUM_TREE_CREATE + NUM_TREE_REMOVE + NUM_TREE_MOVE

typedef enum operation {
    LIST,
    CREATE,
    REMOVE,
    MOVE,

    NUM_OPERATIONS
} operation;

/* ------------------------------ Global variables ------------------------------ */
const char *dir_names[] = {"/a/", "/b/", "/c/", "/d/", "/e/", "/f/",
                           "/g/", "/h/", "/i/", "/j/", "/k/", "/l/",
                           "/m/", "/n/", "/o/", "/p/", "/q/", "/r/",
                           "/s/", "/t/", "/u/", "/v/", "/w/", "/x/", "/y/", "/z/"};
const char *example_paths[] = {"/a/", "/b/", "/a/b/", "/b/a/", "/b/a/d/", "/a/b/c/", "/a/b/d/"};
size_t i_create, i_remove;
size_t i_create, i_src, i_dest;
Tree *tree = NULL;
pthread_mutex_t mutexes[NUM_OPERATIONS];

/* ------------------------------ Helper functions ------------------------------ */
static void init_mutexes() {
    for (size_t i = 0; i < NUM_OPERATIONS; i++) {
        pthread_mutexattr_t mutex_attr;
        err_check(pthread_mutexattr_init(&mutex_attr), "pthread_mutexattr_init");
        err_check(pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK), "pthread_mutexattr_settype");
        err_check(pthread_mutex_init(&mutexes[i], &mutex_attr), "pthread_mutex_init");
    }
}

static void destroy_mutexes() {
    for (size_t i = 0; i < NUM_OPERATIONS; i++) {
        err_check(pthread_mutex_destroy(&mutexes[i]), "pthread_mutex_destroy");
    }
}

static void log_with_thread_name(const char *fmt, ...) {
    pthread_t tid = pthread_self();

    err_check(pthread_mutex_lock(&mutexes[LIST]), "pthread_mutex_lock");

    printf("[THREAD %ld] ", tid);
    va_list fmt_args;
    va_start(fmt_args, fmt);
    vprintf(fmt, fmt_args);
    va_end(fmt_args);
    printf("\n");

    err_check(pthread_mutex_unlock(&mutexes[LIST]), "pthread_mutex_unlock");
}

static inline void init_example_tree() {
    tree = tree_new();
    for (size_t i = 0; i < COUNT_OF(example_paths); i++) {
        assert(!tree_create(tree, example_paths[i]));
    }
}

/* ------------------------------ Runnables ------------------------------ */
static void* runnable_list(void* data) {
    static char PATH[] = "/";
    char *str = tree_list(tree, PATH);
    if (str == NULL) {
        log_with_thread_name("Listing tree: NULL");
    }
    else {
        log_with_thread_name("Listing tree: %s", str);
        free(str);
    }
    return 0;
}

static void* runnable_create(void* data) {
    size_t i;
    err_check(pthread_mutex_lock(&mutexes[CREATE]), "pthread_mutex_lock");
    i = i_create;
    if (++i_create >= COUNT_OF(dir_names)) i = 0;
    err_check(pthread_mutex_unlock(&mutexes[CREATE]), "pthread_mutex_unlock");

    if (tree_create(tree, dir_names[i]) == 0) {
        log_with_thread_name("Successfully created directory: %s", dir_names[i]);
    }
    else {
        log_with_thread_name("Unable to create directory: %s", dir_names[i]);
    }

    return 0;
}

static void* runnable_remove(void* data) {
    size_t i;
    err_check(pthread_mutex_lock(&mutexes[REMOVE]), "pthread_mutex_lock");
    i = i_remove;
    if (++i_remove >= COUNT_OF(dir_names)) i = 0;
    err_check(pthread_mutex_unlock(&mutexes[REMOVE]), "pthread_mutex_unlock");

    if (tree_remove(tree, dir_names[i]) == 0) {
        log_with_thread_name("Successfully removed directory: %s", dir_names[i]);
    }
    else {
        log_with_thread_name("Unable to remove directory: %s", dir_names[i]);
    }

    return 0;
}

static void* runnable_move(void* data) {
    //TODO: może kolejka nazw?
    return 0;
}


/* ------------------------------ Concurrent tests------------------------------ */
void TEMPLATE_many_1_type(void* (rtype) (void*), const size_t num_threads) {
    pthread_t th[num_threads];
    pthread_attr_t attr;

    err_check(pthread_attr_init(&attr), "pthread_attr_init");
    err_check(pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE), "pthread_attr_setdetachstate");

    /* Create runnables and start threads */
    for (size_t i = 0; i < num_threads; i++) {
        err_check(pthread_create(&th[i], &attr, rtype, 0), "pthread_create");
    }

    /* Wait until threads finish */
    for (size_t i = 0; i < num_threads; i++) {
        void* retval = NULL;
        err_check(pthread_join(th[i], &retval), "pthread_join");
    }

    err_check(pthread_attr_destroy(&attr), "pthread_attr_destroy");

    /* Tree destruction */
    tree_free(tree);
}

void TEMPLATE_many_2_types(void* (rtype1) (void*), void* (rtype2) (void*), const size_t num_threads) {
    pthread_t th[num_threads];
    pthread_attr_t attr;

    err_check(pthread_attr_init(&attr), "pthread_attr_init");
    err_check(pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_JOINABLE), "pthread_attr_setdetachstate");

    /* Create runnables and start threads */
    for (size_t i = 0; i < num_threads; i++) {
        if (i % 2 == 0) {
            /* Create writer */
            err_check(pthread_create(&th[i], &attr, rtype1, 0), "pthread_create");
        }
        else {
            /* Create reader */
            err_check(pthread_create(&th[i], &attr, rtype2, 0), "pthread_create");
        }
    }

    /* Wait until threads finish */
    for (size_t i = 0; i < num_threads; i++) {
        void* retval = NULL;
        err_check(pthread_join(th[i], &retval), "pthread_join");
    }

    err_check(pthread_attr_destroy(&attr), "pthread_attr_destroy");

    /* Tree destruction */
    tree_free(tree);
}

void TEST_many_list(const size_t num_threads) {
    init_example_tree();
    return TEMPLATE_many_1_type(runnable_list, num_threads);
}

void TEST_many_create(const size_t num_threads) {
    tree = tree_new();
    return TEMPLATE_many_1_type(runnable_create, num_threads);
}

void TEST_many_create_many_list(const size_t num_threads) {
    //TODO:
    //Assertion `tree->w_count == 0' failed.
    //ok: ccll, clcl, llcc, lclc, lccl
    //nie-ok: ccl(sigsegv), cl(sigsegv)
    tree = tree_new();
    return TEMPLATE_many_2_types(runnable_create, runnable_list, num_threads);
}

void TEST_many_create_many_remove(const size_t num_threads) {
    tree = tree_new();
    assert(num_threads % 2 == 0);
    return TEMPLATE_many_2_types(runnable_create, runnable_remove, num_threads);
}

/* ------------------------------ Sequential tests------------------------------ */
void TEST_edge_cases() {
    Tree *t = tree_new();

    assert(!tree_create(t, "zlanazwa"));
    assert(!tree_move(t, "/a/", "/a/b/d/x/"));

    tree_free(t);
}

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
    PRINT_AND_FREE(str, 1);

    str = tree_list(t, "/a/b/");
    assert(strcmp(str, "c,d") == 0);
    PRINT_AND_FREE(str, 2);

    str = tree_list(t, "/a/b/c/");
    assert(str == NULL);
    PRINT_AND_FREE(str, 3);

    str = tree_list(t, "/a/b/d/");
    assert(str == NULL);
    PRINT_AND_FREE(str, 4);

    str = tree_list(t, "/b/");
    assert((strcmp(str, "a") == 0));
    PRINT_AND_FREE(str, 5);

    str = tree_list(t, "/b/a/");
    assert(strcmp(str, "d") == 0);
    PRINT_AND_FREE(str, 6);

    str = tree_list(t, "/b/a/d/");
    assert(str == NULL);
    PRINT_AND_FREE(str, 7);

    assert(!tree_move(t, "/a/", "/a/b/d/x/"));

    str = tree_list(t, "/a/");
    assert(str == NULL);
    PRINT_AND_FREE(str, 8);

    str = tree_list(t, "/b/");
    assert(strcmp(str, "a,x") == 0);
    PRINT_AND_FREE(str, 9);

    str = tree_list(t, "/b/a/");
    assert(strcmp(str, "d") == 0);
    PRINT_AND_FREE(str, 10);

    str = tree_list(t, "/b/a/d/");
    assert(str == NULL);
    PRINT_AND_FREE(str, 11);

    str = tree_list(t, "/b/x/");
    assert(strcmp(str, "c,d") == 0);
    PRINT_AND_FREE(str, 12);

    str = tree_list(t, "/b/x/c/");
    assert(str == NULL);
    PRINT_AND_FREE(str, 13);

    str = tree_list(t, "/b/x/d/");
    assert(str == NULL);
    PRINT_AND_FREE(str, 14);

    tree_free(t);
}

int main(void) {
    init_mutexes();

    /* Sequential tests */
    // TEST_edge_cases();
    // TEST_tree_move_example();

    /* Concurrent tests */
     TEST_many_list(100);
//     TEST_many_create(7);
//     TEST_many_create_many_list(4);
//     TEST_many_create_many_remove(88);

    destroy_mutexes();
    return 0;
}
