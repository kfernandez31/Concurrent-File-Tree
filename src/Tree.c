#include "Tree.h"
#include "path.h"

#define SUCCESS 0

struct Tree {
    HashMap *subdirectories;
    pthread_mutex_t var_protection;
    pthread_cond_t reader_cond, writer_cond, descendants_cond;
    int r_count, w_count, r_wait, w_wait, active_descendants; //TODO: naming
    bool change;
};

/**
 * Gets the number of immediate subdirectories / tree children.
 * @param tree : file tree
 * @return : number of subdirectories
 */
static inline size_t tree_size(Tree *tree) {
    return hmap_size(tree->subdirectories);
}

//TODO: odpowiednie lock/unlock w iter_tree i iter_path

/**
 * Performs `func` recursively on the `tree` node and its entire subtree.
 * The `func(tree)` call is performed first if `tail_recursive` is false and last otherwise.
 * @param tree : file tree
 * @param tail_recursive : tail recursion flag
 * @param func : operation to perform
 */
static void iter_tree(Tree *tree, const bool tail_recursive, void (func) (Tree*)) {
    if (!tail_recursive) {
        func(tree);
    }

    const char *key;
    void *value;
    HashMapIterator it = hmap_new_iterator(tree->subdirectories);
    while (hmap_next(tree->subdirectories, &it, &key, &value)) {
        iter_tree(value, tail_recursive, func);
    }

    if (tail_recursive) {
        func(tree);
    }
}

/**
 * Recursive function for `iter_path`.
 * @param tree : file tree
 * @param path : file path
 * @param depth : depth of the directory along the path
 * @param cur_depth : current depth
 * @param tail_recursive : tail recursion flag
 * @param func : operation to perform
 */
static void iter_path_recursive(Tree *tree, const char *path, const size_t depth, const size_t cur_depth, const bool tail_recursive, void (func) (Tree*)) {
    size_t index, length;
    get_nth_dir_name_and_length(path, 1, &index, &length);

    Tree *next = hmap_get(tree->subdirectories, false, path + 1, length);

    if (!next || cur_depth > depth) {
        return; //Directory not found
    }
    if (cur_depth == depth) {
        Tree *last = hmap_get(tree->subdirectories, false, path + 1, length);
        func(last);
        return;
    }

    iter_path_recursive(next, path + length + 1, depth, cur_depth + 1, tail_recursive, func);
}

/**
 * Performs `func` recursively in the `tree` along the `path` up to the specified `depth`.
 * The `func(tree)` call is performed first if `tail_recursive` is false and last otherwise.
 * @param tree : file tree
 * @param path : file path
 * @param depth : depth of the directory along the path
 * @param tail_recursive : tail recursion flag
 * @param func : operation to perform

 */
static void iter_path(Tree *tree, const char *path, const size_t depth, const bool tail_recursive, void (func) (Tree*)) {
    if (!tree || !is_valid_path_name(path)) {
        return;
    }
    if (depth == 0) { //TODO: czy potrzebne?
        func(tree);
        return;
    }

    if (!tail_recursive) {
        func(tree);
    }
    iter_path_recursive(tree, path, depth, 1, tail_recursive, func);
    if (tail_recursive) {
        func(tree);
    }
}

/**
 * Called by a read-type operation to lock the tree for reading.
 * Waits if there are other threads writing to the tree.
 * @param tree : file tree
 */
static void reader_lock(Tree *tree) {
    err_check(pthread_mutex_lock(&tree->var_protection), "pthread_mutex_lock");

    while (tree->w_wait + tree->w_count > 0 && !tree->change) { //Wait if necessary
        tree->r_wait++;
        err_check(pthread_cond_wait(&tree->reader_cond, &tree->var_protection), "pthread_cond_wait");
        tree->r_wait--;
    }
    assert(tree->w_count == 0);
    tree->r_count++;

    if (tree->r_wait > 0) { //Wake other readers if there are any
        err_check(pthread_mutex_unlock(&tree->var_protection), "pthread_mutex_unlock");
        err_check(pthread_cond_signal(&tree->reader_cond), "pthread_cond_signal");
    }
    else {
        tree->change = false;
    }

    err_check(pthread_mutex_unlock(&tree->var_protection), "pthread_mutex_unlock");
}

/**
 * Called by a read-type operation to unlock the tree from reading.
 * @param tree : file tree
 */
static void reader_unlock(Tree *tree) {
    err_check(pthread_mutex_lock(&tree->var_protection), "pthread_mutex_lock");

    assert(tree->r_count > 0);
    assert(tree->w_count == 0);
    tree->r_count--;

    if (tree->r_count == 0 && tree->w_wait > 0) { //Wake waiting writers if there are any
        err_check(pthread_cond_signal(&tree->writer_cond), "pthread_cond_signal");
    }

    err_check(pthread_mutex_unlock(&tree->var_protection), "pthread_mutex_unlock");
}

/**
 * Called by a write-type operation to lock the tree for writing.
 * Waits if there are other threads writing or reading from the tree.
 * @param tree : file tree
 */
static void writer_lock(Tree *tree) {
    err_check(pthread_mutex_lock(&tree->var_protection), "pthread_mutex_lock");

    while (tree->r_count > 0 || tree->w_count > 0) {
        tree->w_wait++;
        err_check(pthread_cond_wait(&tree->writer_cond, &tree->var_protection), "pthread_cond_wait");
        tree->w_wait--;
    }
    assert(tree->r_count == 0);
    assert(tree->w_count == 0);
    tree->w_count++;

    err_check(pthread_mutex_unlock(&tree->var_protection), "pthread_mutex_unlock");
}

/**
 * Called by a write-type operation to unlock the tree from reading.
 * @param tree : file tree
 */
static void writer_unlock(Tree *tree) {
    err_check(pthread_mutex_lock(&tree->var_protection), "thread_mutex_lock");

    assert(tree->w_count == 1);
    assert(tree->r_count == 0);
    tree->w_count--;

    if (tree->w_count == 0) {
        if (tree->r_wait > 0) {
            tree->change = true;
            err_check(pthread_cond_signal(&tree->reader_cond), "pthread_cond_signal");
        }
        else if (tree->w_wait > 0) {
            err_check(pthread_cond_signal(&tree->writer_cond), "pthread_cond_signal");
        }
    }

    err_check(pthread_mutex_unlock(&tree->var_protection), "pthread_mutex_unlock");
}

/**
 * Recursive function for `tree_get`.
 * @param tree : file tree
 * @param pop : directory removal flag
 * @param path : file path
 * @param depth : depth of the directory along the path
 * @param depth : current depth
 * @return : pointer to the requested directory
 */
static Tree* tree_get_recursive(Tree* tree, const bool pop, const char* path, const size_t depth, const size_t cur_depth) {
    size_t index, length;
    get_nth_dir_name_and_length(path, 1, &index, &length);

    reader_lock(tree);
    Tree *next = hmap_get(tree->subdirectories, false, path + 1, length);

    if (!next || cur_depth > depth) {
        reader_unlock(tree);
        return NULL; //Directory not found
    }
    if (cur_depth == depth) {
        Tree *res = hmap_get(tree->subdirectories, pop, path + 1, length);
        reader_unlock(tree);
        return res;
    }

    reader_unlock(tree);
    Tree *res = tree_get_recursive(next, pop, path + length + 1, depth, cur_depth + 1);
    return res;
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
static Tree *tree_get(Tree *tree, const bool pop, const char *path, const size_t depth) {
    if (!tree || !is_valid_path_name(path)) {
        return NULL;
    }
    if (depth == 0) {
        return tree;
    }
    return tree_get_recursive(tree, pop, path, depth, 1);
}

/**
 * Waits for all operations to finish in nodes below `tree`.
 * @param tree : file tree
 */
static void wait_for_descendants_to_finish(Tree *tree) {
    err_check(pthread_mutex_lock(&tree->var_protection), "pthread_mutex_lock");
    while (tree->active_descendants > 0) { //Wait if necessary
        err_check(pthread_cond_wait(&tree->descendants_cond, &tree->var_protection), "pthread_cond_wait");
    }
    err_check(pthread_mutex_unlock(&tree->var_protection), "pthread_mutex_unlock");
}

/**
 * Helper function for `tree_move`.
 * Increments the number of active descendants under the node.
 * @param node : node in a file tree
 */
void increment_active_descendants(Tree *node) {
    err_check(pthread_mutex_lock(&node->var_protection), "pthread_mutex_lock");
    node->active_descendants--;
    err_check(pthread_mutex_unlock(&node->var_protection), "pthread_mutex_unlock");
}

/**
 * Helper function for `tree_move`.
 * Decrements the number of active descendants under the node and wakes any threads waiting for them to become 0.
 * @param node : node in a file tree
 */
void decrement_active_descendants(Tree *node) {
    err_check(pthread_mutex_lock(&node->var_protection), "pthread_mutex_lock");
    node->active_descendants--;
    err_check(pthread_mutex_unlock(&node->var_protection), "pthread_mutex_unlock");
    err_check(pthread_cond_signal(&node->descendants_cond), "pthread_cond_signal");
}

/**
 * Helper function for `tree_free`.
 * Frees the entire memory of a node.
 * @param node : node in a file tree
 */
static void node_free(Tree *node) {
    hmap_free(node->subdirectories);
    err_check(pthread_cond_destroy(&node->writer_cond), "pthread_cond_destroy");
    err_check(pthread_cond_destroy(&node->reader_cond), "pthread_cond_destroy");
    err_check(pthread_cond_destroy(&node->descendants_cond), "pthread_cond_destroy");
    err_check(pthread_mutex_destroy(&node->var_protection), "pthread_mutex_destroy");
    free(node);
    node = NULL;
}

Tree* tree_new() {
    Tree *tree = safe_calloc(1, sizeof(Tree));
    tree->subdirectories = hmap_new();

    //`var_protection` initialization
    pthread_mutexattr_t protection_attr;
    err_check(pthread_mutexattr_init(&protection_attr), "pthread_mutexattr_init");
    err_check(pthread_mutexattr_settype(&protection_attr, PTHREAD_MUTEX_ERRORCHECK), "pthread_mutexattr_settype");
    err_check(pthread_mutex_init(&tree->var_protection, &protection_attr), "pthread_mutex_init");

    //`reader_cond` initialization
    pthread_condattr_t reader_attr;
    err_check(pthread_condattr_init(&reader_attr), "pthread_condattr_init");
    err_check(pthread_cond_init(&tree->reader_cond, &reader_attr), "pthread_cond_init");

    //`writer_cond` initialization
    pthread_condattr_t writer_attr;
    err_check(pthread_condattr_init(&writer_attr), "pthread_condattr_init");
    err_check(pthread_cond_init(&tree->writer_cond, &writer_attr), "pthread_cond_init");

    //`descendants_cond` initialization
    pthread_condattr_t descendants_attr;
    err_check(pthread_condattr_init(&descendants_attr), "pthread_condattr_init");
    err_check(pthread_cond_init(&tree->descendants_cond, &descendants_attr), "pthread_cond_init");

    return tree;
}

//TODO: czy to wymaga write-synchronizacji?
void tree_free(Tree* tree) {
    iter_tree(tree, true, node_free);
}

char *tree_list(Tree *tree, const char *path) {
    char *result = NULL;

    if (!is_valid_path_name(path)) {
        return NULL;
    }

    size_t depth = get_path_depth(path);
    Tree *dir = tree_get(tree, false, path, depth);
    if (!dir) {
        return NULL;
    }
    reader_lock(dir);
    iter_path(tree, path, depth, false, increment_active_descendants);

    size_t subdirs = tree_size(dir);
    if (subdirs == 0) {
        iter_path(tree, path, depth, true, decrement_active_descendants);
        reader_unlock(dir);
        return NULL;
    }

    size_t length = tree_size(dir) - 1; //Initially == number of commas
    size_t length_used;
    const char* key;
    void* value;

    HashMapIterator it = hmap_new_iterator(dir->subdirectories);
    while (hmap_next(dir->subdirectories, &it, &key, &value)) {
        length += strlen(key); //Collect all subdirectories' lengths
    }

    result = safe_calloc(length + subdirs, sizeof(char));

    //Get the first subdirectory name
    it = hmap_new_iterator(dir->subdirectories);
    hmap_next(dir->subdirectories, &it, &key, &value);
    size_t first_length = strlen(key);
    memcpy(result, key, first_length * sizeof(char));

    //Get remaining subdirectory names
    length_used = first_length;
    while (hmap_next(dir->subdirectories, &it, &key, &value)) {
        length_used++;
        result[length_used - 1] = ',';

        size_t next_length = strlen(key);
        memcpy(result + length_used, key, next_length * sizeof(char));
        length_used += next_length;
    }

    iter_path(tree, path, depth, true, decrement_active_descendants);
    reader_unlock(dir);
    return result;
}

int tree_create(Tree* tree, const char* path) {
    if (!is_valid_path_name(path)) {
        return EINVAL; //Invalid path
    }
    if (strcmp(path, "/") == 0) {
        return EEXIST; //The root always exists
    }

    size_t c_depth = get_path_depth(path);
    size_t c_index, c_length;
    get_nth_dir_name_and_length(path, c_depth, &c_index, &c_length);

    Tree *parent = tree_get(tree, false, path, c_depth - 1);
    if (!parent) {
        return ENOENT; //The directory's parent does not exist in the tree
    }
    writer_lock(parent);
    iter_path(tree, path, c_depth - 1, false, increment_active_descendants);

    if (hmap_get(parent->subdirectories, false, path + c_index, c_length)) {
        iter_path(tree, path, c_depth - 1, true, decrement_active_descendants);
        writer_unlock(parent);
        return EEXIST; //The directory already exists in the tree
    }

    hmap_insert(parent->subdirectories, path + c_index + 1, c_length, tree_new());

    iter_path(tree, path, c_depth - 1, true, decrement_active_descendants);
    writer_unlock(parent);
    return SUCCESS;
}

int tree_remove(Tree* tree, const char* path) {
    if (strcmp(path, "/") == 0) {
        writer_unlock(tree);
        return EBUSY; //Cannot remove the root
    }

    size_t c_depth = get_path_depth(path);
    size_t c_index, c_length;
    get_nth_dir_name_and_length(path, c_depth, &c_index, &c_length);

    Tree *parent = tree_get(tree, false, path, c_depth - 1);
    if (!parent) {
        return ENOENT; //The directory's parent does not exist in the tree
    }
    writer_lock(parent);
    iter_path(tree, path, c_depth - 1, false, increment_active_descendants);

    Tree *child = hmap_get(parent->subdirectories, false, path + c_index, c_length);
    if (!child) {
        iter_path(tree, path, c_depth - 1, true, decrement_active_descendants);
        writer_unlock(parent);
        return ENOENT; //The directory doesn't exist in the tree
    }

    if (tree_size(child) > 0) {
        iter_path(tree, path, c_depth - 1, true, decrement_active_descendants);
        writer_unlock(parent);
        return ENOTEMPTY; //The directory specified in the path is not empty
    }

    child = hmap_get(parent->subdirectories, true, path + c_index, c_length);
    tree_free(child);

    iter_path(tree, path, c_depth - 1, true, decrement_active_descendants);
    writer_unlock(parent);
    return SUCCESS;
}

int tree_move(Tree *tree, const char *source, const char *target) {
    if (!is_valid_path_name(source) || !is_valid_path_name(target)) {
        return EINVAL; //Invalid path names
    }
    if (is_ancestor(source, target)) {
        return EINVAL; //No directory can be moved to its descendant
    }

    size_t s_depth = get_path_depth(source);
    size_t s_index, s_length;
    get_nth_dir_name_and_length(source, s_depth, &s_index, &s_length);

    Tree *source_parent = tree_get(tree, false, source, s_depth - 1);
    if (!source_parent) {
        return ENOENT; //The source's parent does not exist in the tree
    }
    writer_lock(source_parent);

    Tree *source_dir = hmap_get(source_parent->subdirectories, false, source + s_index, s_length);
    if (!source_dir) {
        writer_unlock(source_parent);
        return ENOENT; //The source does not exist in the tree
    }
    wait_for_descendants_to_finish(source_dir);
    iter_path(tree, source, s_depth, false, increment_active_descendants);

    size_t t_depth = get_path_depth(target);
    size_t t_index, t_length;
    get_nth_dir_name_and_length(target, t_depth, &t_index, &t_length);

    Tree *target_parent = tree_get(tree, false, target, t_depth - 1);
    if (!target_parent) {
        iter_path(tree, source, s_depth, true, decrement_active_descendants);
        writer_unlock(source_parent);
        return ENOENT; //The target's parent does not exist in the tree
    }
    writer_lock(target_parent);

    if (hmap_get(target_parent->subdirectories, false, source + t_index, t_length)) {
        writer_unlock(target_parent);
        iter_path(tree, source, s_depth, true, decrement_active_descendants);
        writer_unlock(source_parent);
        return EEXIST; //The target already exists in the tree
    }
    iter_path(tree, target, t_depth - 1, false, increment_active_descendants);

    source_dir = hmap_get(source_parent->subdirectories, true, source + s_index, s_length); //Pop the source directory
    hmap_insert(target_parent->subdirectories, target + t_index + 1, t_length, source_dir);

    iter_path(tree, target, t_depth - 1, true, decrement_active_descendants);
    writer_unlock(target_parent);
    iter_path(tree, source, s_depth, true, decrement_active_descendants);
    writer_unlock(source_parent);

    return SUCCESS;
}
