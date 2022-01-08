// Prosty test sprawdzający podstawowe działanie wszystkich funkcji.

#include "../Tree.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>

void sequential_small() {
	Tree *tree = tree_new();
	char *list_content = tree_list(tree, "/");
	assert(strcmp(list_content, "") == 0);
	free(list_content);
	assert(tree_list(tree, "/a/") == NULL);
    assert_zero(tree);
	assert(tree_create(tree, "/a/") == 0);
    assert_zero(tree);
	assert(tree_create(tree, "/a/b/") == 0);
    assert_zero(tree);
	assert(tree_create(tree, "/a/b/") == EEXIST);
    assert_zero(tree);
	assert(tree_create(tree, "/a/b/c/d/") == ENOENT);
    assert_zero(tree);
	assert(tree_remove(tree, "/a/") == ENOTEMPTY);
    assert_zero(tree);
	assert(tree_create(tree, "/b/") == 0);
    assert_zero(tree);
	assert(tree_create(tree, "/a/c/") == 0);
    assert_zero(tree);
	assert(tree_create(tree, "/a/c/d/") == 0);
    assert_zero(tree);
	assert(tree_move(tree, "/a/c/", "/b/c/") == 0);
    assert_zero(tree);
	assert(tree_remove(tree, "/b/c/d/") == 0);
    assert_zero(tree);
	list_content = tree_list(tree, "/b/");
    assert_zero(tree);
	assert(strcmp(list_content, "c") == 0);
	free(list_content);
	tree_free(tree);
}
