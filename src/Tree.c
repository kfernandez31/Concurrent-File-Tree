#include "Tree.h"

#define SEPARATOR '/'
#define SUCCESS 0

struct Tree {
    HashMap *subdirectories;
    pthread_mutex_t reader_lock;
    pthread_mutex_t writer_lock;
    //TODO: readers&writers sync algorithm, lock
};

/**
 * Gets the number of immediate subdirectories / tree children.
 * @param tree : file tree
 * @return :  number of subdirectories
 */
static size_t tree_size(Tree *tree) {
    return hmap_size(tree->subdirectories);
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
 * Extracts the first directory name from the given path.
 * @param path : file path
 * @param root_name : buffer to which the first directory name is written to
 * @return : length of the root directory's name
 */
static size_t get_first_dir_name_len(const char *path) {
    size_t length = strlen(path);
    size_t res = 0;

    for (size_t i = 1; i < length && path[i] != SEPARATOR; i++) {
        res++;
    }
    return res;
}

/**
 * Returns the inputted path without the last (deepest) directory name.
 * @param path_without_new_dir : buffer to which the result is written to
 * @param new_dir : new directory name
 * @param path : file path
 * @param path_length : length of the path name
 */
static void get_deepest_dir_name_index_and_length(const char *path, size_t *index, size_t *length) {
    if (strcmp(path, "/") == 0) {
        // SEPARATOR; TODO: coś tu powinno być
        return;
    }

    size_t path_len = strlen(path);
    for (int i = path_len - 2; i >= 0; i--) {
        if (path[i] == SEPARATOR) {
            *index = i;
            break;
        }
    }
    if (*index == -1) {
        fprintf(stderr, "get_path_without_last_dir(): bad input string\n");
        exit(EINVAL);
    }

    *length = 0;
    for (size_t i = *index + 1; i < path_len && path[i] != SEPARATOR; i++) {
        *length++;
    }
}

/**
 * Recursive function for `tree_get`.
 * @param tree : file tree
 * @param path : file path
 * @return : pointer to the requested directory
 */
static Tree* tree_get_recursive(Tree* tree, const bool pop, const char* path, const size_t size, const size_t used_bytes) {
    size_t r_len = get_first_dir_name_len(path);

    Tree *next = hmap_get(tree->subdirectories, false, path + 1, r_len);
    if (!next) {
        return NULL; // not found
    }
    if ((tree_size(tree) == 0 && strcmp(path + r_len + 1, "/") != 0) || used_bytes - 1 > size) {
        return NULL; // path doesn't exist (is too deep)
    }
    if (strcmp(path + r_len + 1, "/") == 0) {
        return hmap_get(tree->subdirectories, pop, path + 1, r_len);
    }
    return tree_get_recursive(next, pop, path + r_len + 1, size, used_bytes + r_len + 1);
}

/**
 * Gets a pointer to the directory specified by the path.
 * @param tree : file tree
 * @param path : file path
 * @return : pointer to the requested directory
 */
static Tree *tree_get(Tree *tree, const bool pop, const char *path, const size_t size) {
    if (!is_valid_path_name(path)) {
        return NULL;
    }
    if (strcmp(path, "/") == 0) {
        return tree;
    }
    return tree_get_recursive(tree, pop, path, size, 0);
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

    Tree *dir = tree_get(tree, false, path, strlen(path));
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

int tree_create(Tree* tree, const char* path) { //TODO: przejrzeć
    if (!is_valid_path_name(path)) {
        return EINVAL; //Invalid path
    }

    size_t length = strlen(path);

    if (tree_get(tree, false, path, length)) {
        return EEXIST; //Directory already exists within the tree
    }

    size_t d_index, d_length;
    get_deepest_dir_name_index_and_length(path, &d_index, &d_length);

    Tree *parent = tree_get(tree, false, path, length - d_length - 1);
    if (!parent) {
        return ENOENT; //The directory's parent does not exist within the tree
    }
    Tree *inserted_dir = tree_new();
    hmap_insert(parent->subdirectories, path + d_index, length - d_length - 2, inserted_dir);

    return SUCCESS;
}

int tree_remove(Tree* tree, const char* path) {
    if (strcmp(path, "/") == 0) {
        return EBUSY; //Cannot remove from a "/" directory
    }

    Tree *dir = tree_get(tree, true, path, strlen(path));
    if (!dir) {
        return ENOENT; //Path does not exist within the tree
    }
    if (tree_size(dir) > 0) {
        return ENOTEMPTY; //Directory specified in the path is not empty
    }
    tree_free(dir);

    return SUCCESS;
}

//TODO: rethink whether I can just change the label of the moved directory
int tree_move(Tree *tree, const char *source, const char *target) {
    if (!is_valid_path_name(source) || !is_valid_path_name(target)) {
        return EINVAL; //Invalid path names
    }

    Tree *source_dir = tree_get(tree, false, source, strlen(source)); //TODO: should probably be true
    if (!source_dir) {
        return EINVAL;
    }

    Tree *target_dir = tree_get(tree, false, target, strlen(target));
    if (target_dir) {
      return EEXIST;
    }

    if (tree_create(tree, target) == EINVAL) {
        return EINVAL;
    }

    const char *key;
    void *value;
    HashMapIterator it = hmap_new_iterator(tree->subdirectories);
    while (hmap_next(source_dir->subdirectories, &it, &key, &value)) {
        hmap_remove(source_dir->subdirectories, key);
    }

    hmap_free(source_dir->subdirectories);
    free(source_dir);

    return SUCCESS;
}
