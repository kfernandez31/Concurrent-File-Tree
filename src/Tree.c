#include "Tree.h"

#define SEPARATOR '/'
#define SUCCESS 0

struct Tree {
    HashMap *subdirectories;
    pthread_mutex_t reader_lock;
    pthread_mutex_t writer_lock;
    pthread_mutex_t var_protection;
    //TODO: readers&writers sync algorithm, lock
};

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
Tree *tree_get(Tree *tree, const bool pop, const char *path, const size_t depth) {
    if (!is_valid_path_name(path)) {
        return NULL;
    }
    if (depth == 0) {
        return tree;
    }
    return tree_get_recursive(tree, pop, path, depth, 1);
}




Tree* tree_new() {
    Tree *tree = safe_malloc(sizeof(struct Tree));
    tree->subdirectories = hmap_new();
    hmap_insert(tree->subdirectories, "/", 1, NULL);

    int err;
    pthread_mutexattr_t reader_attr, writer_attr;
    if ((err = pthread_mutexattr_init(&reader_attr)) != SUCCESS) {
        syserr("ERROR %d: mutexattr_init failed\n", err);
    }
    if ((err = pthread_mutexattr_settype(&reader_attr, PTHREAD_MUTEX_ERRORCHECK)) != SUCCESS) {
        syserr("ERROR %d: mutexattr_settype failed\n", err);
    }
    if ((err = pthread_mutex_init(&tree->reader_lock, &reader_attr)) != SUCCESS) {
        syserr("ERROR %d: mutex init failed\n", err);
    }

    if ((err = pthread_mutexattr_init(&writer_attr)) != SUCCESS) {
        syserr("ERROR %d: mutexattr_init failed\n", err);
    }
    if ((err = pthread_mutexattr_settype(&writer_attr, PTHREAD_MUTEX_ERRORCHECK)) != SUCCESS) {
        syserr("ERROR %d: mutexattr_settype failed\n", err);
    }
    if ((err = pthread_mutex_init(&tree->writer_lock, &writer_attr)) != SUCCESS) {
        syserr("ERROR %d: mutex init failed\n", err);
    }

    return tree;
}

void tree_free(Tree* tree) {
    if (tree) {
        if (tree_size(tree) > 0) {
            const char *key;
            void *value;
            HashMapIterator it = hmap_new_iterator(tree->subdirectories);

            while (hmap_next(tree->subdirectories, &it, &key, &value)) {
                tree_free(hmap_get(tree->subdirectories, false, key, strlen(key)));
            }
        }
        hmap_free(tree->subdirectories);
        int err;
        if ((err = pthread_mutex_destroy(&tree->reader_lock)) != 0) {
            syserr("ERROR %d: mutex destroy failed\n", err);
        }
        if ((err = pthread_mutex_destroy(&tree->writer_lock)) != 0) {
            syserr("ERROR %d: mutex destroy failed\n", err);
        }
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

    size_t length = tree_size(dir) - 1; // initially - number of commas
    size_t length_used;
    const char* key;
    void* value;

    HashMapIterator it = hmap_new_iterator(dir->subdirectories);
    while (hmap_next(dir->subdirectories, &it, &key, &value)) {
        length += strlen(key); // collect all subdirectories' lengths
    }

    result = safe_calloc(length + subdirs, sizeof(char));

    // Getting the first subdirectory name
    it = hmap_new_iterator(dir->subdirectories);
    hmap_next(dir->subdirectories, &it, &key, &value);
    size_t first_length = strlen(key);
    memcpy(result, key, first_length * sizeof(char));

    // Getting remaining subdirectory names
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

    Tree *source_dir = tree_get(tree, true, source, get_path_depth(source));
    if (!source_dir) {
        return ENOENT; //The source directory does not exist in the tree
    }

    size_t target_depth = get_path_depth(target);
    size_t index, length;
    get_nth_dir_name_and_length(target, target_depth, &index, &length);

    Tree *parent = tree_get(tree, false, target, target_depth - 1);
    if (!parent) {
        return ENOENT; //The target's parent does not exist in the tree
    }

    if (tree_get(parent, false, source + index, 1)) {
        return EEXIST; //The target directory already exists in the tree
    }

    hmap_insert(parent->subdirectories, target + index + 1, length, source_dir);

    return SUCCESS;
}
