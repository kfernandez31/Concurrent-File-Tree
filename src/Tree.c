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
/** Checks if the directory represents the root **/
#define IS_ROOT(path) (strcmp(path, "/") == 0)
/** Performs a block of code under the node's mutex **/
#define UNDER_MUTEX(node, code_block)                                 \
do {                                                                  \
    assert(pthread_mutex_lock(&(node)->var_protection) == SUCCESS);   \
    code_block                                                        \
    assert(pthread_mutex_unlock(&(node)->var_protection) == SUCCESS); \
} while(0);

struct Tree {
    Tree *parent;                            /** Parent directory. NULL for the root **/
    HashMap* subdirectories;                 /** HashMap of (name, node) pairs, where node is of type Tree **/
    pthread_mutex_t var_protection;          /** Mutual exclusion for variable access **/
    pthread_cond_t reader_cond;              /** Condition to hang readers **/
    pthread_cond_t writer_cond;              /** Condition to hang writers **/
    pthread_cond_t refcount_cond;            /** Condition to wait on until all subtree operations finish **/
    size_t r_count, w_count, r_wait, w_wait; /** Counters of active and waiting readers/writers **/
    size_t refcount;                         /** Reference count of operations currently performed in the subtree **/
};

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
 * Waits if there are other active or waiting writers.
 * @param tree : file tree
 */
static void reader_lock(Tree* tree) {
    UNDER_MUTEX(tree,
        while (tree->w_wait || tree->w_count) { // Wait if necessary
            tree->r_wait++;
            assert(pthread_cond_wait(&tree->reader_cond, &tree->var_protection) == SUCCESS);
            tree->r_wait--;
        }
        assert(tree->w_count == 0);
        tree->r_count++;
    );
}

/**
 * Called by a read-type operation to unlock the tree from reading.
 * @param tree : file tree
 */
static void reader_unlock(Tree* tree) {
    UNDER_MUTEX(tree,
        assert(tree->r_count > 0);
        assert(tree->w_count == 0);
        tree->r_count--;

        if (tree->r_count == 0)
            assert(pthread_cond_signal(&tree->writer_cond) == SUCCESS);
    );
}

/**
 * Called by a write-type operation to lock the tree for writing.
 * Waits if there are other active readers or writers.
 * @param tree : file tree
 */
static void writer_lock(Tree* tree) {
    UNDER_MUTEX(tree,
        while (tree->r_count > 0 || tree->w_count > 0) {
            tree->w_wait++;
            assert(pthread_cond_wait(&tree->writer_cond, &tree->var_protection) == SUCCESS);
            tree->w_wait--;
        }
        assert(tree->r_count == 0);
        assert(tree->w_count == 0);
        tree->w_count++;
    );
}

/**
 * Called by a write-type operation to unlock the tree from writing.
 * @param tree : file tree
 */
static void writer_unlock(Tree* tree) {
    UNDER_MUTEX(tree,
        assert(tree->w_count == 1);
        assert(tree->r_count == 0);
        tree->w_count--;

        if (tree->w_count == 0) {
            if (tree->r_wait > 0)
                assert(pthread_cond_broadcast(&tree->writer_cond) == SUCCESS);
            else
                assert(pthread_cond_signal(&tree->writer_cond) == SUCCESS);
        }
    );
}

/**
 *
 * @param tree : file tree
 */
static void subtree_lock(Tree* tree) {
    //wait_on_refcount_cond(s_dir); // Wait until refcount reaches zero //TODO: wstawić w subtree_lock
}

/**
 *
 * @param tree : file tree
 */
static void subtree_unlock(Tree* tree) {

}

/**
 * Waits for all operations to finish in nodes below `tree`.
 * @param tree : file tree
 */
static void wait_on_refcount_cond(Tree* tree) {
    UNDER_MUTEX(tree, // This is only to satisfy `pthread_cond_wait`
        while (tree->refcount > 0) //Wait if necessary
            assert(pthread_cond_wait(&tree->refcount_cond, &tree->var_protection) == SUCCESS);
    );
}

/**
 * Performs a cleanup along the path - decrements reference counters along its path.
 * @param node : node in a file tree
 */
static void unwind_path(Tree *node) {
    Tree* next = NULL;
    while (node != NULL){
        UNDER_MUTEX(node,
                next = node->parent;
                node->refcount--;
        );
        node = next;
    }
}

/**
 * Gets a pointer to the directory in the `tree` specified by the `path`.
 * @param tree : file tree
 * @param path : file path
 * @return : pointer to the requested directory
 */
static Tree* get_node(Tree* tree, const char* path, const bool reader) {
    char child_name[MAX_FOLDER_NAME_LENGTH + 1];

    if (IS_ROOT(path) && !reader)
        writer_lock(tree);
    else
        reader_lock(tree);

    UNDER_MUTEX(tree, tree->refcount++;);
    while ((path = split_path(path, child_name))) {
        Tree* subtree = hmap_get(tree->subdirectories, child_name);
        if (!subtree) {
            unwind_path(tree);
            reader_unlock(tree);
            return NULL;
        }
        if (IS_ROOT(path) && !reader)
            writer_lock(subtree);
        else
            reader_lock(subtree);

        UNDER_MUTEX(subtree, subtree->refcount++;);

        reader_unlock(tree);
        tree = subtree;
    }

    return tree;
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

char* tree_list(Tree* tree, const char* path) {
    if (!is_valid_path(path))
        return NULL;

    char* result = NULL;
    Tree* dir = get_node(tree, path, true);
    if (!dir) {
        return NULL; // The directory doesn't exist
    }

    result = make_map_contents_string(dir->subdirectories); // The read

    unwind_path(dir);
    reader_unlock(dir);
    return result;
}

int tree_create(Tree* tree, const char* path) {
    if (!is_valid_path(path))
        return EINVAL; // Invalid path
    if (IS_ROOT(path))
        return EEXIST; // The root always exists

    char child_name[MAX_FOLDER_NAME_LENGTH + 1] = {0}, parent_path[MAX_PATH_LENGTH + 1] = {0};
    make_path_to_parent(path, child_name, parent_path);

    Tree* parent = get_node(tree, parent_path, false);
    if (!parent) {
        return ENOENT; // The directory's parent doesn't exist
    }

    if (hmap_get(parent->subdirectories, child_name)) {
        unwind_path(parent);
        writer_unlock(parent);
        return EEXIST; // The directory already exists
    }

    Tree* child = tree_new();
    child->parent = parent;
    hmap_insert(parent->subdirectories, child_name, child); // The insertion

    unwind_path(parent);
    writer_unlock(parent);
    return SUCCESS;
}

int tree_remove(Tree* tree, const char* path) {
    if (IS_ROOT(path))
        return EBUSY; // Cannot remove the root

    char child_name[MAX_FOLDER_NAME_LENGTH + 1] = {0}, parent_path[MAX_PATH_LENGTH + 1] = {0};
    make_path_to_parent(path, child_name, parent_path);

    Tree* parent = get_node(tree, parent_path, false);
    if (!parent) {
        return ENOENT; // The directory's parent doesn't exist
    }

    Tree* child = hmap_get(parent->subdirectories, child_name);
    if (!child) {
        unwind_path(parent);
        writer_unlock(parent);
        return ENOENT; // The directory doesn't exist
    }
    writer_lock(child);

    if (subdir_count(child) > 0) {
        writer_unlock(child);
        unwind_path(parent);
        writer_unlock(parent);
        return ENOTEMPTY; // The directory is not empty
    }

    // The removal
    pop_subdir(parent, child_name);

    writer_unlock(child);
    unwind_path(parent);
    writer_unlock(parent);
    tree_free(child);
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

    if (cmp < 0) {
        //TODO:
    }

    if (!(s_parent = get_node(tree, s_parent_path, false))) {
        return ENOENT; // The source's parent doesn't exist
    }

    if (!(s_dir = hmap_get(s_parent->subdirectories, s_name))) {
        unwind_path(s_parent);
        reader_unlock(s_parent);
        return ENOENT; //The source doesn't exist
    }
    subtree_lock(s_dir);
    reader_unlock(s_parent);

    if (strcmp(s_parent_path, t_parent_path) != 0) {
        if (!(t_parent = get_node(tree, t_parent_path, false))) {
            unwind_path(s_parent);
            subtree_unlock(s_dir);
            return ENOENT; // The source's parent doesn't exist
        }

        // Check if target already exists
        if (hmap_get(t_parent->subdirectories, t_name)) {
            if (strcmp(s_path, t_path) == 0) {
                unwind_path(s_parent);
                subtree_unlock(s_dir);
                unwind_path(t_parent);
                writer_unlock(t_parent);
                return SUCCESS; // The source and target are the same - nothing to move
            }
            if (is_ancestor(s_path, t_path)) {
                unwind_path(s_parent);
                subtree_unlock(s_dir);
                unwind_path(t_parent);
                writer_unlock(t_parent);
                return EANCMOVE; // No directory can be moved to its descendant
            }
            unwind_path(s_parent);
            subtree_unlock(s_dir);
            unwind_path(t_parent);
            writer_unlock(t_parent);
            return EEXIST; // There already exists a directory with the same name as the target
        }

        // Pop and insert the source
        writer_lock(s_parent);
        s_dir = pop_subdir(s_parent, s_name);
        writer_unlock(s_parent);
        hmap_insert(t_parent->subdirectories, t_name, s_dir);

        unwind_path(s_parent);
        unwind_path(t_parent);
        subtree_unlock(s_dir);
        writer_unlock(t_parent);
        return SUCCESS;
    }
    else {
        t_parent = s_parent;

        // Check if target already exists
        if (hmap_get(t_parent->subdirectories, t_name)) {
            if (strcmp(s_path, t_path) == 0) {
                unwind_path(s_parent);
                subtree_unlock(s_dir);
                return SUCCESS; // The source and target are the same - nothing to move
            }
            if (is_ancestor(s_path, t_path)) {
                unwind_path(s_parent);
                subtree_unlock(s_dir);
                return EANCMOVE; // No directory can be moved to its descendant
            }
            unwind_path(s_parent);
            subtree_unlock(s_dir);
            return EEXIST; // There already exists a directory with the same name as the target
        }

        // Pop and insert the source
        writer_lock(s_parent);
        s_dir = pop_subdir(s_parent, s_name);
        hmap_insert(t_parent->subdirectories, t_name, s_dir);

        unwind_path(s_parent);
        subtree_unlock(s_dir);
        return SUCCESS;
    }
}

Tree* chamski_get(Tree* tree, const char* path) {
    char child_name[256];
    while ((path = split_path(path, child_name))) {
        Tree* subtree = hmap_get(tree->subdirectories, child_name);
        if (!subtree) {
            return NULL;
        }
        tree = subtree;
    }
    return tree;
}