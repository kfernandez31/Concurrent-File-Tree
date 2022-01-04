#include "Tree.h"
#include "HashMap.h"
#include "path_utils.h"
#include "safe_allocations.h"
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
    HashMap* subdirectories;                 /** HashMap of (name, node) pairs, where node is of type Tree **/
    pthread_mutex_t var_protection;          /** Mutual exclusion for variable access **/
    pthread_cond_t reader_cond;              /** Condition to hang readers **/
    pthread_cond_t writer_cond;              /** Condition to hang writers **/
    pthread_cond_t refcount_cond;            /** Condition to wait on until all subtree operations finish **/
    size_t r_count, w_count, r_wait, w_wait; /** Counters of active and waiting readers/writers **/
    size_t refcount;                         /** Reference count of operations currently performed in the subtree **/
};

typedef enum Group {
    CREATE_OR_LIST,
    REMOVE_OR_MOVE
} Group;

static inline Group opposite_group(Group group) {
    return (group == CREATE_OR_LIST)? REMOVE_OR_MOVE : CREATE_OR_LIST;
}

static Tree* pop_subdir(Tree* tree, const char* key) {
    Tree* subdir = hmap_get(tree->subdirectories, key);
    if (subdir) {
        hmap_remove(tree->subdirectories, key);
    }
    return subdir;
}

/**
 * Gets the number of immediate subdirectories / tree children.
 * @param tree : file tree
 * @return : number of subdirectories
 */
static inline size_t subdir_count(Tree* tree) {
    return hmap_size(tree->subdirectories);
}

/**
 * Called by a read-type operation to lock the tree for reading.
 * Waits if there are other threads writing to the tree.
 * @param tree : file tree
 */
static void reader_lock(Tree* tree) { //TODO: kiedy robić reader_lock(), a kiedy zwykłe P(var_protection) ?
    assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);

    while (tree->w_wait || tree->w_count) { // Wait if necessary
        tree->r_wait++;
        assert(pthread_cond_wait(&tree->reader_cond, &tree->var_protection) == SUCCESS);
        tree->r_wait--;
    }
    assert(tree->w_count == 0);
    tree->r_count++;

    assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
}

/**
 * Called by a read-type operation to unlock the tree from reading.
 * @param tree : file tree
 */
static void reader_unlock(Tree* tree) {
    assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);

    assert(tree->r_count > 0);
    assert(tree->w_count == 0);
    tree->r_count--;

    if (tree->r_count == 0) {
        if (tree->w_wait > 0) {
            assert(pthread_cond_signal(&tree->writer_cond) == SUCCESS);
        }
        else if (tree->r_wait > 0) {
            assert(pthread_cond_broadcast(&tree->writer_cond) == SUCCESS);
        }
    }

    assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
}

/**
 * Called by a write-type operation to lock the tree for writing.
 * Waits if there are other threads writing or reading from the tree.
 * @param tree : file tree
 */
static void writer_lock(Tree* tree) {
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
static void writer_unlock(Tree* tree) {
    assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);

    assert(tree->w_count == 1);
    assert(tree->r_count == 0);
    tree->w_count--;

    if (tree->w_count == 0) {
        if (tree->r_wait > 0) {
            assert(pthread_cond_broadcast(&tree->writer_cond) == SUCCESS);
        }
        else if (tree->w_wait > 0) {
            assert(pthread_cond_signal(&tree->writer_cond) == SUCCESS);
        }
    }

    assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
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
static void iter_path(Tree* tree, const char* path, const bool tail_rec, void (*func)(Tree*)) {
    char component[MAX_FOLDER_NAME_LENGTH + 1];
    const char* subpath = split_path(path, component);
    if (!subpath) return;

    reader_lock(tree);
    Tree* subtree = hmap_get(tree->subdirectories, component);
    if (!subtree) {
        reader_unlock(tree);
        return; // Directory not found
    }
    reader_unlock(tree);

    if (tail_rec) func(tree);
    iter_path(subtree, subpath, tail_rec, func);
    if (!tail_rec) func(tree);
}

/**
 * Gets a pointer to the directory in the `tree` specified by the `path`.
 * @param tree : file tree
 * @param path : file path
 * @return : pointer to the requested directory
 */
static Tree* get_node(Tree* tree, const char* path, Group group) {
    char component[MAX_FOLDER_NAME_LENGTH + 1] = {0};
    const char* subpath = split_path(path, component);

    if (!subpath) return tree;

    assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);
    tree->refcount++;
    assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);

    reader_lock(tree);
    Tree* subtree = hmap_get(tree->subdirectories, component);
    if (!subtree) {
        reader_unlock(tree);
        assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);
        tree->refcount--;
        assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
        return NULL; // Directory not found
    }
    reader_unlock(tree);

    Tree* res = get_node(subtree, subpath, group);
    if (!res) {
        assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS);
        tree->refcount--;
        assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
    }
    return res;
}

/**
 * Waits for all operations to finish in nodes below `tree`.
 * @param tree : file tree
 */
static void wait_on_refcount_cond(Tree* tree) {
    assert(pthread_mutex_lock(&tree->var_protection) == SUCCESS); // This is only to satisfy `pthread_cond_wait`
    while (tree->refcount > 0) { //Wait if necessary
        assert(pthread_cond_wait(&tree->refcount_cond, &tree->var_protection) == SUCCESS);
    }
    assert(pthread_mutex_unlock(&tree->var_protection) == SUCCESS);
}

/**
 * Helper function for `tree_move`.
 * Decrements the number of active descendants under the node and wakes any threads waiting for them to become 0.
 * @param node : node in a file tree
 */
static void decrement_ref_count(Tree* node) {
    assert(pthread_mutex_lock(&node->var_protection) == SUCCESS);
    assert(node->refcount > 0);
    node->refcount--;
    assert(pthread_cond_signal(&node->refcount_cond) == SUCCESS);
    assert(pthread_mutex_unlock(&node->var_protection) == SUCCESS);
}

//TODO: doc
static void unwind_path(Tree *tree, Tree *node, const char* path, const bool reader) {
    if (reader) reader_unlock(node);
    else writer_unlock(node);

    iter_path(tree, path, false, decrement_ref_count);
}

Tree* tree_new() {
    Tree* tree = safe_calloc(1, sizeof(Tree));
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
    const char* key = NULL;
    void* value = NULL;
    HashMapIterator it = hmap_iterator(tree->subdirectories);

    while (hmap_next(tree->subdirectories, &it, &key, &value)) {
        Tree* subdir = pop_subdir(tree, key);
        assert(subdir);
        tree_free(subdir);
    }

    hmap_free(tree->subdirectories);
    assert(pthread_cond_destroy(&tree->writer_cond) == SUCCESS);
    assert(pthread_cond_destroy(&tree->reader_cond) == SUCCESS);
    assert(pthread_cond_destroy(&tree->refcount_cond) == SUCCESS);
    assert(pthread_mutex_destroy(&tree->var_protection) == SUCCESS);
    free(tree);
    tree = NULL;
}

char *tree_list(Tree* tree, const char* path) {
    if (!is_valid_path(path))
        return NULL;

    char* result = NULL;
    Tree* dir = get_node(tree, path, CREATE_OR_LIST);
    if (!dir) {
        return NULL; // The directory doesn't exist
    }

    reader_lock(dir);
    result = make_map_contents_string(dir->subdirectories); // The read

    unwind_path(tree, dir, path, true);
    return result;
}

int tree_create(Tree* tree, const char* path) {
    if (!is_valid_path(path))
        return EINVAL; // Invalid path
    if (IS_ROOT(path))
        return EEXIST; // The root always exists

    char child_name[MAX_FOLDER_NAME_LENGTH + 1] = {0}, parent_path[MAX_PATH_LENGTH + 1] = {0};
    make_path_to_parent(path, child_name, parent_path);

    Tree* parent = get_node(tree, parent_path, CREATE_OR_LIST);
    if (!parent) {
        return ENOENT; // The directory's parent doesn't exist
    }

    writer_lock(parent);
    if (hmap_get(parent->subdirectories, child_name)) {
        unwind_path(tree, parent, parent_path, false);
        return EEXIST; // The directory already exists
    }

    hmap_insert(parent->subdirectories, child_name, tree_new()); // The insertion

    unwind_path(tree, parent, parent_path, false);
    return SUCCESS;
}

int tree_remove(Tree* tree, const char* path) {
    if (IS_ROOT(path))
        return EBUSY; // Cannot remove the root

    char child_name[MAX_FOLDER_NAME_LENGTH + 1] = {0}, parent_path[MAX_PATH_LENGTH + 1] = {0};
    make_path_to_parent(path, child_name, parent_path);

    Tree* parent = get_node(tree, parent_path, REMOVE_OR_MOVE);
    if (!parent) {
        return ENOENT; //The directory's parent doesn't exist
    }

    writer_lock(parent);
    Tree* child = hmap_get(parent->subdirectories, child_name);
    if (!child) {
        unwind_path(tree, parent, parent_path, false);
        return ENOENT; // The directory doesn't exist
    }

    if (subdir_count(child) > 0) {
        unwind_path(tree, parent, parent_path, false);
        return ENOTEMPTY; // The directory is not empty
    }

    /*TODO: dodać te typy
    //remove(/a/) [1] -> zdobywa wskaźnik na /a/, czeka na wait_on_refcount_cond, mrozi się,
    //remove(/a/) [2] -> wykonuje się w pełni
    //remove(/a/) [1] -> budzi się i gówno

    //remove(/a/)   -> dostaje wskaźnik na /a/, mrozi się
    //create(/a/b/) -> nie powinnno nawet dostać wskaźnika na /a/

    //byśmy przekazywali do get_node typ operacji (remove/move lub create/list)
    //i do locków w sumie też, żeby było uczciwie/żywotnie
    */

    // The removal
    pop_subdir(parent, child_name);
    tree_free(child);

    unwind_path(tree, parent, parent_path, false);
    return SUCCESS;
}

int tree_move(Tree* tree, const char* s_path, const char* t_path) {
    if (!is_valid_path(s_path) || !is_valid_path(t_path))
        return EINVAL; // Invalid path names
    if (IS_ROOT(s_path))
        return EBUSY; // Can't move the root
    if (IS_ROOT(t_path))
        return EEXIST; // Can't assign a new root
    if (is_ancestor(s_path, t_path))
        return EANCMOVE; // No directory can be moved to its descendant

    char s_name[MAX_FOLDER_NAME_LENGTH + 1] = {0}, t_name[MAX_FOLDER_NAME_LENGTH + 1] = {0};
    char s_parent_path[MAX_PATH_LENGTH + 1] = {0}, t_parent_path[MAX_PATH_LENGTH + 1] = {0};
    Tree *s_dir = NULL, *s_parent = NULL, *t_parent = NULL;
    make_path_to_parent(s_path, s_name, s_parent_path);
    make_path_to_parent(t_path, t_name, t_parent_path);
    int cmp = strcmp(s_parent_path, t_parent_path);

    if (!(s_parent = get_node(tree, s_parent_path, REMOVE_OR_MOVE))) {
        return ENOENT; // The source's parent doesn't exist
    }

    reader_lock(s_parent);
    if (!(s_dir = hmap_get(s_parent->subdirectories, s_name))) {
        reader_unlock(s_parent);
        unwind_path(tree, s_parent, s_parent_path, true);
        return ENOENT; //The source doesn't exist
    }
    subtree_lock(s_dir); //TODO: i tutaj inkrementacja nowej zmiennej
    //wait_on_refcount_cond(s_dir); // Wait until refcount reaches zero //TODO: wstawić w subtree_lock
    reader_unlock(s_parent);

    if (strcmp(s_parent_path, t_parent_path) == 0) {
        t_parent = s_parent;
    }
    else if (!(t_parent = get_node(tree, t_parent_path, REMOVE_OR_MOVE))) {
        unwind_path(tree, s_parent, s_parent_path, true);
        return ENOENT; // The source's parent doesn't exist
    }
    if (s_parent != t_parent) writer_lock(t_parent);

    if (hmap_get(t_parent->subdirectories, t_name)) {
        if (strcmp(s_path, t_path) == 0) {
            unwind_path(tree, s_parent, s_parent_path, true);
            if (s_parent != t_parent) unwind_path(tree, t_parent, t_parent_path, false);
            return SUCCESS; // The source and target are the same - nothing to move
        }
        if (is_ancestor(s_path, t_path)) {
            unwind_path(tree, s_parent, s_parent_path, true);
            if (s_parent != t_parent) unwind_path(tree, t_parent, t_parent_path, false);
            return EANCMOVE; // No directory can be moved to its descendant
        }
        unwind_path(tree, s_parent, s_parent_path, true);
        if (s_parent != t_parent) unwind_path(tree, t_parent, t_parent_path, false);
        return EEXIST; // There already exists a directory with the same name as the target
    }

    // Pop and insert the source
    writer_lock(s_parent);
    s_dir = pop_subdir(s_parent, s_name);
    writer_unlock(s_parent);
    hmap_insert(t_parent->subdirectories, t_name, s_dir);

    subtree_unlock(s_dir);
    iter_path(tree, s_parent_path, false, decrement_ref_count);
    if (s_parent != t_parent) unwind_path(tree, t_parent, t_parent_path, false);
    return SUCCESS;
}
