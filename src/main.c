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


void print_map(HashMap* map) {
    const char* key = NULL;
    void* value = NULL;

    printf("Size=%zd\n", hmap_size(map));
    HashMapIterator it = hmap_iterator(map);
    while (hmap_next(map, &it, &key, &value)) {
        printf("Key=%s Value=%p\n", key, value);
    }
    printf("\n");
}


int main(void)
{
/*    HashMap* map = hmap_new();
    hmap_insert(map, "a", hmap_new());
    print_map(map);

    HashMap* child = (HashMap*)hmap_get(map, "a");
    hmap_free(child);
    hmap_remove(map, "a");
    print_map(map);

    hmap_free(map);*/

    Tree *t;
    char *str;
/*    t = tree_new();

    str = tree_list(t, "/");
    PRINT_FREE(str, 0);

    tree_create(t, "zlanazwa");

    tree_create(t, "/foo/");
    str = tree_list(t, "/foo/");
    PRINT_FREE(str, 1);

    tree_create(t, "/foo/barr/");
    str = tree_list(t, "/foo/");
    PRINT_FREE(str, 2);
    str = tree_list(t, "/foo/barr/");
    PRINT_FREE(str, 3);

    tree_create(t, "/foo/barr/lol/");
    tree_create(t, "/foo/barr/xd/");
    str = tree_list(t, "/tree_pop_recursive/barr/");
    PRINT_FREE(str, 4);

    tree_free(t);*/

/*
    t = tree_new();
    tree_create(t, "/a/");
    tree_create(t, "/b/");
    tree_create(t, "/a/b/");
    tree_create(t, "/b/a/");
    tree_create(t, "/a/b/c/");
    tree_create(t, "/a/b/d/");
    tree_create(t, "/b/a/d/");
    //Tree *y = tree_get_directory(t, "/a/");
    tree_move(t, "/a/b/", "/b/x/");
    //y = tree_get(t, "/a/");
    //y = NULL;
    str = tree_list(t, "/a/");
    PRINT_FREE(str, 4);
    str = tree_list(t, "/b/");
    PRINT_FREE(str, 5);
    str = tree_list(t, "/b/a/");
    PRINT_FREE(str, 6);
    str = tree_list(t, "/b/x/");
    PRINT_FREE(str, 7);
    tree_free(t);
*/

    t = tree_new();
    tree_create(t, "/a/");
    tree_create(t, "/b/");
    tree_create(t, "/a/b/");
    tree_create(t, "/b/a/");
    tree_create(t, "/a/b/c/");
    tree_create(t, "/a/b/d/");
    tree_create(t, "/b/a/d/");
    int x = tree_move(t, "/a/b/", "/b/a/");
    str = tree_list(t, "/a/");
    PRINT_FREE(str, 8);
    str = tree_list(t, "/b/");
    PRINT_FREE(str, 9);
    tree_free(t);

    return 0;
}