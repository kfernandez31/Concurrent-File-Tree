#include "Tree.h"

#define PRINT_AND_FREE(s, n)  \
do {                    \
    if (str == NULL) { \
        printf( "[%d]\n", n);\
    } \
    else {\
        printf("%s [%d]\n", str, n);\
        free(str);        \
        str = NULL;\
    }\
} while (0)

bool TEST_edge_cases() {
    bool test_passed = true;
    Tree *t = tree_new();

    test_passed |= !tree_create(t, "zlanazwa");
    test_passed |= !tree_move(t, "/a/", "/a/b/d/x/");

    tree_free(t);
    return test_passed;
}

bool TEST_tree_move_example() {
    bool test_passed = true;
    Tree *t = tree_new();
    char *str = NULL;

    test_passed |= tree_create(t, "/a/");
    test_passed |= tree_create(t, "/b/");
    test_passed |= tree_create(t, "/a/b/");
    test_passed |= tree_create(t, "/b/a/");
    test_passed |= tree_create(t, "/b/a/d/");
    test_passed |= tree_create(t, "/a/b/c/");
    test_passed |= tree_create(t, "/a/b/d/");

    str = tree_list(t, "/a/");
    test_passed |= (strcmp(str, "b") == 0);
    PRINT_AND_FREE(str, 1);

    str = tree_list(t, "/a/b/");
    test_passed |= (strcmp(str, "c,d") == 0);
    PRINT_AND_FREE(str, 2);

    str = tree_list(t, "/a/b/c/");
    test_passed |= (str == NULL);
    PRINT_AND_FREE(str, 3);

    str = tree_list(t, "/a/b/d/");
    test_passed |= (str == NULL);
    PRINT_AND_FREE(str, 4);

    str = tree_list(t, "/b/");
    test_passed |= (strcmp(str, "a") == 0);
    PRINT_AND_FREE(str, 5);

    str = tree_list(t, "/b/a/");
    test_passed |= (strcmp(str, "d") == 0);
    PRINT_AND_FREE(str, 6);

    str = tree_list(t, "/b/a/d/");
    test_passed |= (str == NULL);
    PRINT_AND_FREE(str, 7);

    test_passed |= tree_move(t, "/a/", "/a/b/d/x/");

    str = tree_list(t, "/a/");
    test_passed |= (str == NULL);
    PRINT_AND_FREE(str, 8);

    str = tree_list(t, "/b/");
    test_passed |= (strcmp(str, "a,x") == 0);
    PRINT_AND_FREE(str, 9);

    str = tree_list(t, "/b/a/");
    test_passed |= (strcmp(str, "d") == 0);
    PRINT_AND_FREE(str, 10);

    str = tree_list(t, "/b/a/d/");
    test_passed |= (str == NULL);
    PRINT_AND_FREE(str, 11);

    str = tree_list(t, "/b/x/");
    test_passed |= (strcmp(str, "c,d") == 0);
    PRINT_AND_FREE(str, 12);

    str = tree_list(t, "/b/x/c/");
    test_passed |= (str == NULL);
    PRINT_AND_FREE(str, 13);

    str = tree_list(t, "/b/x/d/");
    test_passed |= (str == NULL);
    PRINT_AND_FREE(str, 14);

    tree_free(t);
    return test_passed;
}

int main(void) {
    bool tests_passed = true;

    tests_passed |= TEST_edge_cases();
    tests_passed |= TEST_tree_move_example();

    assert(tests_passed);
}