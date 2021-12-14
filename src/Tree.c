#include "Tree.h"

#define SEPARATOR '/'
#define SUCCESS 0

struct Tree {
    HashMap *subdirectories;
    size_t height; //TODO: set this somewhere
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
static bool is_valid_dir_name(const char *dir_name) {
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
static bool is_valid_path_name(const char *path_name) {
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
static size_t extract_root_name(const char *path, char *root_name) {
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
static void separate_path_and_new_dir(char *path_without_new_dir, char *new_dir, const char *path, const size_t path_length) {
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
 * Returns the depth of the specified path.
 * @param path : file path
 * @return : path depth
 */
static size_t path_depth(const char *path) {
    if (strcmp(path, "/") == 0) {
        return 0;
    }
    size_t length = strlen(path);
    size_t num_separators = 0;
    for (size_t i = 0; i < length; i++) {
        if (path[i] == SEPARATOR) {
            num_separators++;
        }
    }
    return num_separators - 1;
}

/**
 * Gets a pointer to the directory specified by the path.
 * @param tree : file tree
 * @param path : file path
 * @return : pointer to the requested directory
 */
static Tree* tree_get_directory_recursive(Tree* tree, const char* path) {
    size_t path_length = strlen(path);
    char root_name[path_length];
    memset(root_name, '\0', path_length * sizeof(char));

    size_t root_name_length = extract_root_name(path, root_name);

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
    return tree_get_directory_recursive(next, path + root_name_length + 1);
}


//TODO: this is a lot of code repetition
static Tree* tree_pop_recursive(Tree* tree, const char* path) {
    size_t path_length = strlen(path);
    char root_name[path_length];
    memset(root_name, '\0', path_length * sizeof(char));

    size_t root_name_length = extract_root_name(path, root_name);

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

static Tree *tree_pop(Tree *tree, const char* path) {
    if (!is_valid_path_name(path)) {
        return NULL;
    }
    if (path_depth(path) > tree->height) {
        return NULL;
    }
    if (strcmp(path, "/") == 0) {
        return tree;
    }
    return tree_pop_recursive(tree, path);
}

/**
 * Wrapper for `tree_get_directory_recursive`.
 * @param tree : file tree
 * @param path : file path
 * @return : pointer to the requested directory
 */
Tree* tree_get_directory(Tree* tree, const char* path) {
    if (!is_valid_path_name(path)) {
        return NULL;
    }
    if (path_depth(path) > tree->height) {
        return NULL;
    }
    if (strcmp(path, "/") == 0) {
        return tree;
    }

    return tree_get_directory_recursive(tree, path);
}

/**
 * Makes the `source` directory a subdirectory of `target`
 * @param source : source directory
 * @param source_name : name of the source directory
 * @param target : target directory
 */
static void tree_transfer_dir(Tree *source, const char *source_name, Tree *target) {
    hmap_insert(target->subdirectories, source_name, source);
}

/**
 * Adds subdirectories from `source` to `target` normally if their names do not exist within `target`.
 * If they do, then the contents of `source`'s subdirectories are added to `target`'s subdirectories.
 * @param source
 * @param target
 */
static void tree_merge_subdirs(Tree *source, Tree *target) {

}

Tree* tree_new() {
    Tree *tree = safe_malloc(sizeof(struct Tree));
    tree->height = 0;
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

    if (!is_valid_path_name(path)) {
        return NULL;
    }

    Tree *dir = tree_get_directory(tree, path);
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
    if (!is_valid_path_name(path)) {
        return EINVAL;
    }

    int depth = path_depth(path);
    if (depth - 1 > tree->height) {
        return EINVAL;
    }
    if (tree_get_directory(tree, "path") != NULL) {
        return ENOENT;
    }

    size_t path_length = strlen(path);
    char path_without_new_dir[path_length], new_dir[path_length];
    memset(path_without_new_dir, '\0', path_length * sizeof(char));
    memset(new_dir, '\0', path_length * sizeof(char));
    separate_path_and_new_dir(path_without_new_dir, new_dir, path, path_length);

    if (!is_valid_dir_name(new_dir)) {
        return EINVAL;
    }

    Tree *dir = tree_get_directory(tree, path_without_new_dir);
    if (!dir) {
        return EINVAL;
    }
    Tree *inserted_dir = tree_new();
    hmap_insert(dir->subdirectories, new_dir, inserted_dir);

    if (tree->height < depth) {
        tree->height = depth;
    }

    return SUCCESS;
}

int tree_remove(Tree* tree, const char* path) {
    if (strcmp(path, "/") == 0) {
        return EBUSY;
    }

    if (!is_valid_path_name(path)) {
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
    if (!is_valid_path_name(source) || !is_valid_path_name(target)) {
        return EINVAL;
    }

    Tree *source_dir = tree_pop(tree, source);
    if (!source_dir) {
        return EINVAL;
    }

    Tree *target_dir = tree_get_directory(tree, target);

    if (!target_dir) {
        if (tree_create(tree, target) == EINVAL) {
            return EINVAL;
        }
        target_dir = tree_get_directory(tree, target);
    }

    const char *key;
    void *value;
    HashMapIterator it = hmap_iterator(tree->subdirectories);
    while (hmap_next(source_dir->subdirectories, &it, &key, &value)) {
        tree_merge_subdirs(source_dir, target_dir);
        hmap_remove(source_dir->subdirectories, key);
    }

    //TODO: ogarnąć merge

    hmap_free(source_dir->subdirectories);
    free(source_dir);

    return SUCCESS;
}
