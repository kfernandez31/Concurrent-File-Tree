#include "Tree.h"
#include "HashMap.h"
#include "path.h"
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

/** Generic success code **/
#define SUCCESS 0
/** Error code for when an ancestor is being moved to its descendant **/
#define EANCMOVE (-1)

#define IS_ROOT(path) (strcmp(path, "/") == 0)

struct Tree {
    HashMap *subdirectories;                 /** HashMap of (name, node) pairs, where node is of type Tree **/
    pthread_mutex_t var_protection;          /** Mutual exclusion for variable access **/
    pthread_cond_t reader_cond;              /** Condition to hang readers **/
    pthread_cond_t writer_cond;              /** Condition to hang writers **/
    pthread_cond_t refcount_cond;            /** Condition to wait on until all subtree operations finish **/
    size_t r_count, w_count, r_wait, w_wait; /** Counters of active and waiting readers/writers **/
    size_t refcount;                         /** Reference count of operations currently performed in the subtree **/
    size_t change;                           /** Flag set to true if a reader is waking a group of writers **/
};

/**
 * Gets the number of immediate subdirectories / tree children.
 * @param tree : file tree
 * @return : number of subdirectories
 */
static inline size_t subdir_count(Tree *tree) {
    return hmap_size(tree->subdirectories);
}

/**
 * Performs `func` recursively on the `tree` node and its entire subtree.
 * The `func(tree)` call is performed first if `tail_rec` is false and last otherwise.
 * @param tree : file tree
 * @param tail_rec : tail recursion flag
 * @param func : operation to perform
 */
static void iter_tree(Tree *tree, const bool tail_rec, void (func) (Tree*)) {
    if (tail_rec) func(tree);

    const char *key;
    void *value;
    HashMapIterator it = hmap_new_iterator(tree->subdirectories);

    assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);
    while (hmap_next(tree->subdirectories, &it, &key, &value)) {
        assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
        iter_tree(value, tail_rec, func);
        assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);
    }
    assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);

    if (!tail_rec) func(tree);
}

/**
 * Recursive function for `iter_path`.
 * @param tree : file tree
 * @param path : file path
 * @param depth : depth of the directory along the path
 * @param cur_depth : current depth
 * @param tail_rec : tail recursion flag
 * @param func : operation to perform
 */
static void iter_path_recursive(Tree *tree, const char *path, const size_t depth, const size_t cur_depth, const bool tail_rec, void (func) (Tree*)) {
    size_t index, length;
    get_nth_dir_name_and_length(path, 1, &index, &length);

    if (cur_depth > depth) {
        return; //Directory not found
    }

    if (cur_depth == depth) {
        assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);
        Tree *dir = hmap_get(tree->subdirectories, false, path + 1, length);
        assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
        func(dir);
        return;
    }

    assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);
    Tree *next = hmap_get(tree->subdirectories, false, path + 1, length);
    if (!next) {
        assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
        return; //Directory not found
    }
    assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);

    if (tail_rec) func(next);
    iter_path_recursive(next,path + length + 1, depth, cur_depth + 1, tail_rec, func);
    if (!tail_rec) func(next);
}

/**
 * Performs `func` recursively in the `tree` along the `path` up to the specified `depth`.
 * The `func(tree)` call is performed first if `tail_rec` is false and last otherwise.
 * @param tree : file tree
 * @param path : file path
 * @param depth : depth of the directory along the path
 * @param tail_rec : tail recursion flag
 * @param func : operation to perform

 */
static void iter_path(Tree *tree, const char *path, const size_t depth, const bool tail_rec, void (func) (Tree*)) {
    if (!tree || !is_valid_path_name(path)) {
        return;
    }
    if (depth == 0) { //TODO: czy potrzebne?
        func(tree);
        return;
    }

    if (tail_rec) func(tree);
    iter_path_recursive(tree, path, depth, 1, tail_rec, func);
    if (!tail_rec) func(tree);
}

/**
 * Called by a read-type operation to lock the tree for reading.
 * Waits if there are other threads writing to the tree.
 * @param tree : file tree
 */
static void reader_lock(Tree *tree) { //TODO: kiedy robić reader_lock(), a kiedy zwykłe P(var_protection) ?
    assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);

    while ((tree->w_wait || tree->w_count) && !tree->change) { //Wait if necessary
        tree->r_wait++;
        assert(pthread_cond_wait(&tree->reader_cond, &tree->var_protection) == SUCCESS);
        tree->r_wait--;
    }
    assert(tree->w_count == 0);
    tree->r_count++;

    if (tree->r_wait > 0) { //Wake other readers if there are any
        assert(pthread_cond_signal(&tree->reader_cond) == SUCCESS);
    }
    else {
        tree->change = false;
    }

    assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
}

/**
 * Called by a read-type operation to unlock the tree from reading.
 * @param tree : file tree
 */
static void reader_unlock(Tree *tree) {
    assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);

    assert(tree->r_count > 0);
    assert(tree->w_count == 0);
    tree->r_count--;

    if (tree->r_count == 0 && tree->w_wait > 0) { //Wake waiting writers if there are any
        assert(pthread_cond_signal(&tree->writer_cond) == SUCCESS);
    }

    assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
}

/**
 * Called by a write-type operation to lock the tree for writing.
 * Waits if there are other threads writing or reading from the tree.
 * @param tree : file tree
 */
static void writer_lock(Tree *tree) {
    assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);

    while (tree->r_count > 0 || tree->w_count > 0) {
        tree->w_wait++;
        assert(pthread_cond_wait(&tree->writer_cond, &tree->var_protection) == SUCCESS);
        tree->w_wait--;
    }
    assert(tree->r_count == 0);
    assert(tree->w_count == 0);
    tree->w_count++;

    assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
}

/**
 * Called by a write-type operation to unlock the tree from reading.
 * @param tree : file tree
 */
static void writer_unlock(Tree *tree) {
    assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);

    assert(tree->w_count == 1);
    assert(tree->r_count == 0);
    tree->w_count--;

    if (tree->w_count == 0) {
        if (tree->r_wait > 0) {
            tree->change = true;
            assert(pthread_cond_signal(&tree->reader_cond) == SUCCESS);
        }
        else if (tree->w_wait > 0) {
            assert(pthread_cond_signal(&tree->writer_cond) == SUCCESS);
        }
    }

    assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
}

/**
 * Recursive function for `get_node`.
 * @param tree : file tree
 * @param pop : directory removal flag
 * @param path : file path
 * @param depth : depth of the directory along the path
 * @param depth : current depth
 * @return : pointer to the requested directory
 */
static Tree* get_node_recursive(Tree* tree, const bool pop, const char* path, const size_t depth, const size_t cur_depth) {
    size_t index, length;
    get_nth_dir_name_and_length(path, 1, &index, &length);

    if (cur_depth == depth) {
        assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);
        Tree *res = hmap_get(tree->subdirectories, pop, path + 1, length);
        assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
        return res;
    }

    if (cur_depth > depth) {
        return NULL; //Directory not found
    }

    assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);
    Tree *next = hmap_get(tree->subdirectories, false, path + 1, length);
    if (!next) {
        assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
        return NULL; //Directory not found
    }
    assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);

    return get_node_recursive(next, pop, path + length + 1, depth, cur_depth + 1);
}

/**
 * Gets a pointer to the directory specified by the path at `depth`.
 * Removes the directory from the tree if `pop` is true.
 * @param tree : file tree
 * @param pop : directory removal flag
 * @param path : file path
 * @param depth : depth of the directory along the path
 * @return : pointer to the requested directory
 */
static Tree *get_node(Tree *tree, const bool pop, const char *path, const size_t depth) {
    //get node tylko inkrementuje, chyba że NULL, to wtedy też robi cleanup
    if (!tree || !is_valid_path_name(path)) {
        return NULL;
    }
    if (depth == 0) {
        return tree;
    }
    return get_node_recursive(tree, pop, path, depth, 1);
}

/**
 * Waits for all operations to finish in nodes below `tree`.
 * @param tree : file tree
 */
static void wait_on_refcount_cond(Tree *tree) {
    assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);
    while (tree->refcount > 0) { //Wait if necessary
        assert(pthread_cond_wait(&tree->refcount_cond, &tree->var_protection) == SUCCESS);
    }
    assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
}

/**
 * Helper function for `tree_move`.
 * Increments the number of active descendants under the node.
 * @param node : node in a file tree
 */
static void increment_ref_count(Tree *node) {
    assert(pthread_mutex_lock(&node->var_protection) == SUCCESS);
    node->refcount++;
    assert(pthread_mutex_unlock(&node->var_protection) == SUCCESS);
}

/**
 * Helper function for `tree_move`.
 * Decrements the number of active descendants under the node and wakes any threads waiting for them to become 0.
 * @param node : node in a file tree
 */
static void decrement_ref_count(Tree *node) {
    assert(pthread_mutex_lock(&node->var_protection) == SUCCESS);
    assert(node->refcount > 0);
    node->refcount--;
    assert(pthread_mutex_unlock(&node->var_protection) == SUCCESS);
    assert(pthread_cond_signal(&node->refcount_cond) == SUCCESS);
}

/**
 * Helper function for `tree_free`.
 * Frees the entire memory of a node.
 * @param node : node in a file tree
 */
static void free_node(Tree *node) {
    hmap_free(node->subdirectories);
    assert(pthread_cond_destroy(&node->writer_cond) == SUCCESS);
    assert(pthread_cond_destroy(&node->reader_cond) == SUCCESS);
    assert(pthread_cond_destroy(&node->refcount_cond) == SUCCESS);
    assert(pthread_mutex_destroy(&node->var_protection) == SUCCESS);
    free(node);
    node = NULL;
}

/**
 * Lists all subdirectories of a tree, separating them with a comma.
 * @param tree : file tree
 * @param num_subdirs : number of subdirectories in `tree`
 * @return : list of subdirectories
 */
static char *collect_subdir_names(Tree *tree) {
    char *result = NULL;
    size_t num_subdirs, length, length_used;
    const char* key;
    void* value;
    HashMapIterator it;

    assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);
    num_subdirs = subdir_count(tree);

    if (num_subdirs == 0) {
        result = safe_malloc(1 * sizeof(char));
        result[0] = '\0';
        assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
        return result;
    }

    length = num_subdirs - 1; //Initially == number of commas

    it = hmap_new_iterator(tree->subdirectories);
    while (hmap_next(tree->subdirectories, &it, &key, &value)) {
        length += strlen(key); //Get lengths of subdirectory names
    }
    result = safe_calloc(length + num_subdirs, sizeof(char));

    it = hmap_new_iterator(tree->subdirectories);
    hmap_next(tree->subdirectories, &it, &key, &value); //Get the first subdirectory name
    size_t first_length = strlen(key);
    memcpy(result, key, first_length * sizeof(char));

    length_used = first_length; //Get remaining subdirectory names
    while (hmap_next(tree->subdirectories, &it, &key, &value)) {
        length_used++;
        result[length_used - 1] = ',';
        size_t next_length = strlen(key);
        memcpy(result + length_used, key, next_length * sizeof(char));
        length_used += next_length;
    }

    assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
    return result;
}

Tree* tree_new() {
    Tree *tree = safe_calloc(1, sizeof(Tree));
    tree->subdirectories = hmap_new();

    //`var_protection` initialization
    pthread_mutexattr_t protection_attr;
    assert(pthread_mutexattr_init(&protection_attr) == SUCCESS);
    assert(pthread_mutexattr_settype(&protection_attr, PTHREAD_MUTEX_ERRORCHECK) == SUCCESS);
    assert(pthread_mutex_init(&tree->var_protection, &protection_attr) == SUCCESS);
    //`reader_cond` initialization
    pthread_condattr_t reader_attr;
    assert(pthread_condattr_init(&reader_attr) == SUCCESS);
    assert(pthread_cond_init(&tree->reader_cond, &reader_attr) == SUCCESS);
    //`writer_cond` initialization
    pthread_condattr_t writer_attr;
    assert(pthread_condattr_init(&writer_attr) == SUCCESS);
    assert(pthread_cond_init(&tree->writer_cond, &writer_attr) == SUCCESS);
    //`refcount_cond` initialization
    pthread_condattr_t refcount_attr;
    assert(pthread_condattr_init(&refcount_attr) == SUCCESS);
    assert(pthread_cond_init(&tree->refcount_cond, &refcount_attr) == SUCCESS);

    return tree;
}

void tree_free(Tree* tree) {
    iter_tree(tree, false, free_node);
}

char *tree_list(Tree *tree, const char *path) {
    char *result = NULL;

    if (!is_valid_path_name(path)) {
        return NULL;
    }
    if (IS_ROOT(path)) {
        return collect_subdir_names(tree);
    }

    size_t p_depth = get_path_depth(path);
    size_t p_index, p_length;
    get_nth_dir_name_and_length(path, p_depth, &p_index, &p_length);
    Tree *parent = get_node(tree, false, path + p_index + 1, p_length);
    if (!parent) {
        return NULL; //The directory does not exist in the tree
    }
    reader_lock(parent);

    result = collect_subdir_names(parent);

    reader_unlock(parent);
    iter_path(tree, path, p_depth - 1, false, decrement_ref_count);
    return result;
}

int tree_create(Tree* tree, const char* path) {
    if (!is_valid_path_name(path)) {
        return EINVAL; //Invalid path
    }
    if (IS_ROOT(path)) {
        return EEXIST; //The root always exists
    }
    size_t c_depth = get_path_depth(path);
    size_t c_index, c_length;
    get_nth_dir_name_and_length(path, c_depth, &c_index, &c_length);

    Tree *parent = get_node(tree, false, path, c_depth - 1);
    if (!parent) {
        return ENOENT; //The directory's parent does not exist in the tree
    }
    writer_lock(parent);

    if (hmap_get(parent->subdirectories, false, path + c_index + 1, c_length)) {
        writer_unlock(parent);
        iter_path(tree, path, c_depth - 1, false, decrement_ref_count);
        return EEXIST; //The directory already exists in the tree
    }

    hmap_insert(parent->subdirectories, path + c_index + 1, c_length, tree_new()); //The insertion

    writer_unlock(parent);
    iter_path(tree, path, c_depth - 1, false, decrement_ref_count);
    return SUCCESS;
}

int tree_remove(Tree* tree, const char* path) {
    if (IS_ROOT(path)) {
        return EBUSY; //Cannot remove the root
    }
    size_t c_depth = get_path_depth(path);
    size_t c_index, c_length;
    get_nth_dir_name_and_length(path, c_depth, &c_index, &c_length);

    Tree *parent = get_node(tree, false, path, c_depth - 1);
    if (!parent) {
        return ENOENT; //The directory's parent does not exist in the tree
    }
    writer_lock(parent);

    Tree *child = hmap_get(parent->subdirectories, false, path + c_index + 1, c_length);
    if (!child) {
        writer_unlock(parent);
        iter_path(tree, path, c_depth - 1, false, decrement_ref_count);
        return ENOENT; //The directory doesn't exist in the tree
    }
    wait_on_refcount_cond(child); //Wait until refcount reaches zero

    if (subdir_count(child) > 0) {
        writer_unlock(parent);
        iter_path(tree, path, c_depth - 1, false, decrement_ref_count);
        return ENOTEMPTY; //The directory specified in the path is not empty
    }

    child = hmap_get(parent->subdirectories, true, path + c_index + 1, c_length);
    tree_free(child); //The removal

    writer_unlock(parent);
    iter_path(tree, path, c_depth - 1, false, decrement_ref_count);
    return SUCCESS;
}

int tree_move(Tree *tree, const char *source, const char *target) {
    if (!is_valid_path_name(source) || !is_valid_path_name(target)) {
        return EINVAL; //Invalid path names
    }
    if (IS_ROOT(source)) {
        return EBUSY; //Cannot move the root
    }
    if (IS_ROOT(target)) {
        return EEXIST; //Cannot assign a new root
    }
    //tree_move(t, "/a/b/c/", "/x/y/")

    size_t s_depth = get_path_depth(source);
    size_t s_index, s_length;
    get_nth_dir_name_and_length(source, s_depth, &s_index, &s_length);
    Tree *source_parent = get_node(tree, false, source, s_depth - 1);
    if (!source_parent) {
        return ENOENT; //The source's parent does not exist in the tree
    }
    writer_lock(source_parent);

    Tree *source_dir = hmap_get(source_parent->subdirectories, false, source + s_index + 1, s_length);
    if (!source_dir) {
        writer_unlock(source_parent);
        return ENOENT; //The source directory does not exist in the tree
    }

    size_t t_depth = get_path_depth(target);
    size_t t_index, t_length;
    get_nth_dir_name_and_length(target, t_depth, &t_index, &t_length);
    Tree *target_parent = get_node(tree, false, target, t_depth - 1);
    if (!target_parent) {
        writer_unlock(source_parent);
        return ENOENT; //The target directory's parent does not exist in the tree
    }
    if (source_parent != target_parent) {
        writer_lock(target_parent);
    }

    if (hmap_get(target_parent->subdirectories, false, target + t_index + 1, t_length)) {
        if (strcmp(source, target) == 0) {
            if (source_parent != target_parent) {
                writer_unlock(target_parent);
            }
            writer_unlock(source_parent);
            return SUCCESS; //The source and target are the same - nothing to move
        }
        if (is_ancestor(source, target)) {
            if (source_parent != target_parent) {
                writer_unlock(target_parent);
            }
            writer_unlock(source_parent);
            return EANCMOVE; //No directory can be moved to its descendant
        }
        if (source_parent != target_parent) {
            writer_unlock(target_parent);
        }
        writer_unlock(source_parent);
        return EEXIST; //There already exists a directory with the same name as the target
    }

    if (is_ancestor(source, target)) { //TODO: umieścić na początku
        writer_unlock(target_parent);
        writer_unlock(source_parent);
        return EANCMOVE; //No directory can be moved to its descendant
    }

    wait_on_refcount_cond(source_dir); //Wait until refcount reaches zero
    source_dir = hmap_get(source_parent->subdirectories, true, source + s_index + 1, s_length); //Pop the source directory
    hmap_insert(target_parent->subdirectories, target + t_index + 1, t_length, source_dir); //The insertion

    writer_unlock(source_parent);
    iter_path(tree, source, s_depth - 1, false, decrement_ref_count);
    if (source_parent != target_parent) {
        writer_unlock(target_parent);
        iter_path(tree, target, t_depth - 1, false, decrement_ref_count); //TODO: czy dobra kolejność?
    }
    return SUCCESS;
}

//TODO: kiedy hmap_get, a kiedy reader_lock?