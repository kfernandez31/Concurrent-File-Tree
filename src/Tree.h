#pragma once

/* Let "Tree" mean the same as "struct Tree". */
typedef struct Tree Tree;

/**
 * Tree constructor.
 * @return : pointer to the newly created tree
 */
Tree* tree_new();

/**
 * Tree destructor. Deallocates all memory belonging to the tree.
 */
void tree_free(Tree* tree);

/**
 * Lists all directories contained by the tree, starting from the path.
 * @param tree : file tree
 * @param path : file path
 * @return : list of all of the path's contents
 */
char *tree_list(Tree *tree, const char *path);

/**
 * Creates a new directory in the specified path.
 * @param tree : file tree
 * @param path : file path
 * @return : error code / success
 */
int tree_create(Tree* tree, const char* path);

/**
 * Removes a new directory in the specified path.
 * @param tree : file tree
 * @param path : file path
 * @return : error code / success
 */
int tree_remove(Tree* tree, const char* path);

 /**
  * Moves the folder specified in `source` to the specified `target`.
  * @param tree : file tree
  * @param s_path : source directory
  * @param t_path : target directory
  * @return : error code / success
  */
int tree_move(Tree *tree, const char *s_path, const char *t_path);

Tree* chamski_get(Tree* tree, const char* path);
void assert_zero(Tree* tree);