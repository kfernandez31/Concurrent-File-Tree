#include "HashMap.h"
#include "Tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRINT_FREE(s, n)  \
do {                    \
    if (str == NULL) { \
        printf( "[%d]\n", n);\
    } \
    else {\
        printf("%s[%d]\n", str, n);\
        free(str);        \
        str = NULL;\
    }\
} while (0)

void print_n_times(char c, int n) {
    for (int i = 0; i < n; i++) {
        printf("%c", c);
    }
}

int main(void) {
    Tree *t = tree_new();
    char *str = NULL;
    int x = 0;

    //str = tree_list(t, "/");
    //PRINT_FREE(str, -1);

    //tree_create(t, "zlanazwa");

/*    tree_create(t, "/foo/");
    str = tree_list(t, "/foo/");
    PRINT_FREE(str, 0);

    x = tree_create(t, "/foo/barr/");
    str = tree_list(t, "/foo/");
    PRINT_FREE(str, 1);
    str = tree_list(t, "/foo/barr/");
    PRINT_FREE(str, 2);

    x = tree_create(t, "/foo/barr/lol/");
    x = tree_create(t, "/foo/barr/xd/");
    str = tree_list(t, "/foo/barr/");
    PRINT_FREE(str, 3);*/

    tree_free(t);

    t = tree_new();
    Tree *root = tree_get(t, false, "/", 0);
    x = tree_create(t, "/a/");
    Tree *a = tree_get(t, false, "/a/", 1);
    x = tree_create(t, "/b/");
    Tree *b = tree_get(t, false, "/b/", 1);
    x = tree_create(t, "/a/b/");
    Tree *ab = tree_get(t, false, "/a/b/", 2);
    x = tree_create(t, "/b/a/");
    Tree *ba = tree_get(t, false, "/b/a/", 2);
    x = tree_create(t, "/b/a/d/");
    Tree *bad = tree_get(t, false, "/b/a/d/", 3);
    x = tree_create(t, "/a/b/c/");
    Tree *abc = tree_get(t, false, "/a/b/c/", 3);
    x = tree_create(t, "/a/b/d/");
    Tree *abd = tree_get(t, false, "/a/b/d/", 3);
    //x = tree_move(t, "/a/b/", "/b/x/");
    x = tree_move(t, "/a/", "/a/b/d/x/");
    Tree *bx = tree_get(t, false, "/b/x/", 2);
    str = tree_list(t, "/a/");
    PRINT_FREE(str, 4);
    str = tree_list(t, "/b/");
    PRINT_FREE(str, 5);
    str = tree_list(t, "/b/a/");
    PRINT_FREE(str, 6);
    str = tree_list(t, "/b/x/");
    PRINT_FREE(str, 7);

    tree_free(t);

}