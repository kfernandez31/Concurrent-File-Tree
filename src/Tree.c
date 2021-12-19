#include "Tree.h"

#define SEPARATOR '/'
#define SUCCESS 0

struct Tree {
    HashMap *subdirectories;
    pthread_mutex_t var_protection;
    pthread_cond_t reader_cond;
    pthread_cond_t writer_cond;
    int r_count, w_count, r_wait, w_wait;
    bool change;
};

/**
 * Checks whether the caller's result was success.
 * Calls `syserr` with the caller's error code if one occurred.
 * @param res : function call result
 * @param caller : calling function name
 */
static void err_check(int res, const char *caller) {
    if (res != SUCCESS) {
        syserr("ERROR %d in call of %s\n", caller);
    }
}

/**
 * Gets the number of immediate subdirectories / tree children.
 * @param tree : file tree
 * @return : number of subdirectories
 */
static inline size_t tree_size(Tree *tree) {
    return hmap_size(tree->subdirectories);
}

/**
 * Calculates the depth of the `path` based on its number of separators.
 * @param path : file path
 * @return : depth of the path
 */
static size_t get_path_depth(const char *path) {
    size_t res = 0;
    while (*path) {
        if (*path == SEPARATOR) {
            res++;
        }
        path++;
    }
    return res - 1;
}

/**
 * Checks whether `dir_name` represents a directory in the accepted convention
 * (/dir_1/dir_2/.../dir_n/ - a string of valid directory names separated by slashes).
 * @param dir_name : string to check
 * @return : true if `dir_name` is a valid path, false otherwise
 */
static bool is_valid_path_name(const char *path_name) {
    size_t length = strlen(path_name);
    bool sep = false;

    if (length == 0 || length > MAX_PATH_NAME_LENGTH ||
        path_name[0] != SEPARATOR || path_name[length - 1] != SEPARATOR) {
        return false;
    }

    size_t dir_name_length = 0;
    for (size_t i = 0; i < length; i++) {
        if (path_name[i] == SEPARATOR) {
            if (sep) {
                return false;
            }
            dir_name_length = 0;
            sep = true;
        }
        else if (islower(path_name[i])) {
            dir_name_length++;
            if (dir_name_length > MAX_DIR_NAME_LENGTH) {
                return false;
            }
            sep = false;
        }
        else {
            return false;
        }
    }

    return true;
}

/**
 * Gets the name and length of `n`-th directory along the path starting from the root.
 * Passing `n`==0 will return the root.
 * @param path : file path
 * @param n : depth of the directory
 * @param index : index of the directory in the path
 * @param length : length of the directory
 */
static void get_nth_dir_name_and_length(const char *path, const size_t n, size_t *index, size_t *length) {
    size_t len = strlen(path);
    size_t seps = 0;

    for (size_t i = 0; i < len; i++) {
        if (path[i] == SEPARATOR) {
            seps++;
        }
        if (seps == n) {
            *index = i;
            *length = 0;
            for (i = *index + 1; path[i] != SEPARATOR; i++) {
                (*length)++;
            }
            return;
        }
    }
    fprintf(stderr, "get_nth_dir_name_and_length(): invalid n\n");
    exit(EXIT_FAILURE);
}

/**
 * Recursive function for `tree_get`.
 * @param tree : file tree
 * @param path : file path
 * @return : pointer to the requested directory
 */
static Tree* tree_get_recursive(Tree* tree, const bool pop, const char* path, const size_t max_depth, const size_t cur_depth) {
    size_t index, length;
    get_nth_dir_name_and_length(path, 1, &index, &length);

    Tree *next = hmap_get(tree->subdirectories, false, path + 1, length);
    if (!next || cur_depth > max_depth) {
        return NULL; //Directory not found
    }
    if (strcmp(path + length + 1, "/") == 0 || cur_depth == max_depth) {
        return hmap_get(tree->subdirectories, pop, path + 1, length);
    }
    return tree_get_recursive(next, pop, path + length + 1, max_depth, cur_depth + 1);
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
Tree *tree_get(Tree *tree, const bool pop, const char *path, const size_t depth) {//TODO: STATIC
    if (!tree || !is_valid_path_name(path)) {
        return NULL;
    }
    if (depth == 0) {
        return tree;
    }
    return tree_get_recursive(tree, pop, path, depth, 1);
}

/**
 * Checks whether both directories lie on the same path in a tree,
 * furthermore - if path1 branches out to path2.
 * @param path1 : path to the first directory
 * @param path2 : path to the second directory
 * @return : whether the first directory is an ancestor of the second
 */
bool is_ancestor(const char *path1, const char *path2) {
    size_t length1 = strlen(path1);
    return strncmp(path1, path2, length1) == 0;
}


Tree* tree_new() {
    Tree *tree = safe_calloc(sizeof(Tree), 1);
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

    return tree;
}

void tree_free(Tree* tree) {
    if (tree) {
        if (tree_size(tree) > 0) {
            const char *key;
            void *value;
            HashMapIterator it = hmap_new_iterator(tree->subdirectories);
            //Free subdirectories' memory
            while (hmap_next(tree->subdirectories, &it, &key, &value)) {
                tree_free(hmap_get(tree->subdirectories, false, key, strlen(key)));
            }
        }
        hmap_free(tree->subdirectories);
        err_check(pthread_cond_destroy(&tree->writer_cond), "pthread_cond_destroy");
        err_check(pthread_cond_destroy(&tree->reader_cond), "pthread_cond_destroy");
        err_check(pthread_mutex_destroy(&tree->var_protection), "pthread_mutex_destroy");
        free(tree);
        tree = NULL;
    }
}

char *tree_list(Tree *tree, const char *path) {
    char *result = NULL;

    if (!is_valid_path_name(path)) {
        return NULL;
    }

    Tree *dir = tree_get(tree, false, path, get_path_depth(path));
    if (!dir) {
        return NULL;
    }

    size_t subdirs = tree_size(dir);
    if (subdirs == 0) {
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

    //Getting the first subdirectory name
    it = hmap_new_iterator(dir->subdirectories);
    hmap_next(dir->subdirectories, &it, &key, &value);
    size_t first_length = strlen(key);
    memcpy(result, key, first_length * sizeof(char));

    //Getting remaining subdirectory names
    length_used = first_length;
    while (hmap_next(dir->subdirectories, &it, &key, &value)) {
        length_used++;
        result[length_used - 1] = ',';

        size_t next_length = strlen(key);
        memcpy(result + length_used, key, next_length * sizeof(char));
        length_used += next_length;
    }

    return result;
}

int tree_create(Tree* tree, const char* path) {
    if (!is_valid_path_name(path)) {
        return EINVAL; //Invalid path
    }

    size_t depth = get_path_depth(path);
    size_t index, length;
    get_nth_dir_name_and_length(path, depth, &index, &length);

    Tree *parent = tree_get(tree, false, path, get_path_depth(path) - 1);
    if (!parent) {
        return ENOENT; //The directory's parent does not exist in the tree
    }

    if (tree_get(parent, false, path + index, 1)) {
        return EEXIST; //Directory already exists in the tree
    }

    hmap_insert(parent->subdirectories, path + index + 1, length, tree_new());

    return SUCCESS;
}

int tree_remove(Tree* tree, const char* path) {
    if (strcmp(path, "/") == 0) {
        return EBUSY; //Cannot remove from a "/" directory
    }

    Tree *dir = tree_get(tree, true, path, get_path_depth(path));
    if (!dir) {
        return ENOENT; //Path does not exist in the tree
    }
    if (tree_size(dir) > 0) {
        return ENOTEMPTY; //Directory specified in the path is not empty
    }
    tree_free(dir);

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

    Tree *source_dir = tree_get(source_parent, false, source + s_index, 1);
    if (!source_dir) {
        return ENOENT; //The source directory does not exist in the tree
    }

    size_t t_depth = get_path_depth(target);
    size_t t_index, t_length;
    get_nth_dir_name_and_length(target, t_depth, &t_index, &t_length);

    Tree *parent = tree_get(tree, false, target, t_depth - 1);
    if (!parent) {
        return ENOENT; //The target's parent does not exist in the tree
    }

    if (tree_get(parent, false, source + t_index, 1)) {
        return EEXIST; //The target directory already exists in the tree
    }

    source_dir = tree_get(source_parent, true, source + s_index, 1);
    hmap_insert(parent->subdirectories, target + t_index + 1, t_length, source_dir);

    return SUCCESS;
}
