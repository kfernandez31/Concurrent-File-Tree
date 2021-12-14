#include "Tree.h"

#define SEPARATOR '/'
#define SUCCESS 0

struct Tree {
    HashMap *subdirectories;
};

/**
 * Gets the number of immediate subdirectories / tree children.
 * @param tree : file tree
 * @return number of subdirectories
 */
static size_t tree_size(Tree *tree) {
    return hmap_size(tree->subdirectories);
}

/**
 * Checks whether `dir_name` represents a directory in the accepted convention (all lowercase chars).
 * @param dir_name : string to check
 * @return true if `dir_name` is a directory, false otherwise
 */
static bool tree_is_valid_dir_name(const char *dir_name) {
    size_t length = strlen(dir_name);

    if (length == 0 || length > MAX_DIR_NAME_LENGTH) {
        return false;
    }

    for (size_t i = 0; i < length; i++) {
        if (!islower(dir_name[i])) {
            return false;
        }
    }

    return true;
}

/**
 * Checks whether `dir_name` represents a directory in the accepted convention (/dir_1/dir_2/.../dir_n/ -
 * a string of valid directory names separated by slashes).
 * @param dir_name : string to check
 * @return : true if `dir_name` is a directory, false otherwise
 */
static bool tree_is_valid_path_name(const char *path_name) {
    size_t length = strlen(path_name);
    bool sep = false;

    if (length == 0 || length > MAX_PATH_NAME_LENGTH ||
        path_name[0] != SEPARATOR || path_name[length - 1] != SEPARATOR) {
        return false;
    }

    for (size_t i = 0; i < length; i++) {
        if (path_name[i] == SEPARATOR) {
            sep = true;
            continue;
        }

        if (islower(path_name[i])) {
            sep = false;
        }
        else if (sep == false){
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
static size_t tree_extract_root_name(const char *path, char *root_name) {
    size_t length = strlen(path);
    size_t result_length = 0;

    for (size_t i = 1; i < length && path[i] != SEPARATOR; i++) {
        root_name[i - 1] = path[i];
        result_length++;
    }

    return result_length;
}

/**
 * Returns the inputted path without the last (deepest) directory name.
 * @param path_without_new_dir : buffer to which the result is written to
 * @param new_dir : new directory name
 * @param path : file path
 * @param path_length : length of the path name
 */
static void tree_separate_path_and_new_dir(char *path_without_new_dir, char *new_dir, const char *path, const size_t path_length) {
    if (strcmp(path, "/") == 0) {
        path_without_new_dir[0] = SEPARATOR;
        return;
    }

    size_t last_sep_idx = -1;
    for (int i = path_length - 2; i >= 0; i--) {
        if (path[i] == SEPARATOR) {
            last_sep_idx = i;
            break;
        }
    }
    if (last_sep_idx == -1) {
        fprintf(stderr, "get_path_without_last_dir(): bad input string\n");
        exit(EINVAL);
    }

    memcpy(new_dir, path + last_sep_idx + 1, path_length - last_sep_idx - 2);
    memcpy(path_without_new_dir, path, (last_sep_idx + 1) * sizeof(char));
}

/**
 * Recursive function for `tree_get`.
 * @param tree : file tree
 * @param path : file path
 * @return : pointer to the requested directory
 */
static Tree* tree_get_recursive(Tree* tree, const char* path) {
    size_t path_length = strlen(path);
    char root_name[path_length];
    memset(root_name, '\0', path_length * sizeof(char));

    size_t root_name_length = tree_extract_root_name(path, root_name);

    Tree *next = hmap_get(tree->subdirectories, root_name);
    if (!next) {
        return NULL; // not found
    }
    else if (tree_size(tree) == 0 && strcmp(path + root_name_length + 1, "/") != 0) {
        return NULL; // path doesn't exist (is too deep)
    }
    else if (strcmp(path + root_name_length + 1, "/") == 0) {
        return next;
    }
    return tree_get_recursive(next, path + root_name_length + 1);
}

//TODO: this is a lot of code repetition
/**
 * Recursive function for `tree_pop`
 * @param tree : file tree
 * @param path : file path
 * @return : pointer to the requested directory
 */
static Tree* tree_pop_recursive(Tree* tree, const char* path) {
    size_t path_length = strlen(path);
    char root_name[path_length];
    memset(root_name, '\0', path_length * sizeof(char));

    size_t root_name_length = tree_extract_root_name(path, root_name);

    Tree *next = hmap_get(tree->subdirectories, root_name);
    if (!next) {
        return NULL; // not found
    }
    else if (tree_size(tree) == 0 && strcmp(path + root_name_length + 1, "/") != 0) {
        return NULL; // path doesn't exist (is too deep)
    }
    else if (strcmp(path + root_name_length + 1, "/") == 0) {
        Tree *value = hmap_pop_value(tree->subdirectories, root_name);
        return value;
    }
    return tree_pop_recursive(next, path + root_name_length + 1);
}

/**
 * Fetches a pointer to the requested directory after removing it from its parent.
 * @param tree : file tree
 * @param path : file path
 * @return : pointer to the requested directory
 */
static Tree *tree_pop(Tree *tree, const char* path) {
    if (!tree_is_valid_path_name(path)) {
        return NULL;
    }
    if (strcmp(path, "/") == 0) {
        return tree;
    }
    return tree_pop_recursive(tree, path);
}

/**
 * Gets a pointer to the directory specified by the path.
 * @param tree : file tree
 * @param path : file path
 * @return : pointer to the requested directory
 */
static Tree* tree_get(Tree* tree, const char* path) {
    if (!tree_is_valid_path_name(path)) {
        return NULL;
    }
    if (strcmp(path, "/") == 0) {
        return tree;
    }

    return tree_get_recursive(tree, path);
}




Tree* tree_new() {
    Tree *tree = safe_malloc(sizeof(struct Tree));
    tree->subdirectories = hmap_new();
    hmap_insert(tree->subdirectories, "/", NULL); //TODO: ?

    return tree;
}

void tree_free(Tree* tree) {
    if (tree) {
        if (tree_size(tree) > 0) {
            const char *key;
            void *value;
            HashMapIterator it = hmap_iterator(tree->subdirectories);

            while (hmap_next(tree->subdirectories, &it, &key, &value)) {
                tree_free(hmap_get(tree->subdirectories, key));
            }
        }
        hmap_free(tree->subdirectories);
        tree->subdirectories = NULL;
        free(tree);
        tree = NULL;
    }
}

char *tree_list(Tree *tree, const char *path) {
    char *result = NULL;

    if (!tree_is_valid_path_name(path)) {
        return NULL;
    }

    Tree *dir = tree_get(tree, path);
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
    HashMapIterator it = hmap_iterator(dir->subdirectories);

    while (hmap_next(dir->subdirectories, &it, &key, &value)) {
        length += strlen(key); // collect all subdirectories' lengths
    }

    result = safe_calloc(length + subdirs, sizeof(char));

    // Getting the first subdirectory name
    it = hmap_iterator(dir->subdirectories);
    hmap_next(dir->subdirectories, &it, &key, &value);
    size_t first_length = strlen(key);
    memcpy(result, key, first_length * sizeof(char));

    if (subdirs == 1) {
        return result; // one subdirectory, no commas
    }

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
    if (!tree_is_valid_path_name(path)) {
        return EINVAL;
    }

    if (tree_get(tree, "path") != NULL) {
        return ENOENT;
    }

    size_t path_length = strlen(path);
    char path_without_new_dir[path_length], new_dir[path_length];
    memset(path_without_new_dir, '\0', path_length * sizeof(char));
    memset(new_dir, '\0', path_length * sizeof(char));
    tree_separate_path_and_new_dir(path_without_new_dir, new_dir, path, path_length);

    if (!tree_is_valid_dir_name(new_dir)) {
        return EINVAL;
    }

    Tree *dir = tree_get(tree, path_without_new_dir);
    if (!dir) {
        return ENOENT;
    }
    Tree *inserted_dir = tree_new();
    hmap_insert(dir->subdirectories, new_dir, inserted_dir);


    return SUCCESS;
}

int tree_remove(Tree* tree, const char* path) {
    if (strcmp(path, "/") == 0) {
        return EBUSY;
    }

    if (!tree_is_valid_path_name(path)) {
        return ENOENT;
    }

    Tree *dir = tree_pop(tree, path);
    if (!dir) {
        return ENOENT;
    }
    if (tree_size(dir) > 0) {
        return ENOTEMPTY;
    }
    tree_free(dir);

    return SUCCESS;
}

int tree_move(Tree *tree, const char *source, const char *target) {
    if (!tree_is_valid_path_name(source) || !tree_is_valid_path_name(target)) {
        return EINVAL;
    }

    Tree *source_dir = tree_pop(tree, source);
    if (!source_dir) {
        return EINVAL;
    }

    Tree *target_dir = tree_get(tree, target);
    if (target_dir) {
      return EEXIST;
    }

    if (tree_create(tree, target) == EINVAL) {
        return EINVAL;
    }
    target_dir = tree_get(tree, target);

    const char *key;
    void *value;
    HashMapIterator it = hmap_iterator(tree->subdirectories);
    while (hmap_next(source_dir->subdirectories, &it, &key, &value)) {
        hmap_remove(source_dir->subdirectories, key);
    }

    hmap_free(source_dir->subdirectories);
    free(source_dir);

    return SUCCESS;
}
