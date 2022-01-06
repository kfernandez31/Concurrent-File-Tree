#include "path_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#define SEPARATOR '/'

bool is_valid_path(const char* path) {
    size_t len = strlen(path);

    if (len == 0 || len > MAX_PATH_LENGTH) {
        return false;
    }
    if (path[0] != SEPARATOR || path[len - 1] != SEPARATOR) {
        return false;
    }

    const char* name_start = path + 1; // Start of current path component, just after '/'.
    while (name_start < path + len) {
        char* name_end = strchr(name_start, SEPARATOR); // End of current path component, at '/'.
        if (!name_end || name_end == name_start || name_end > name_start + MAX_FOLDER_NAME_LENGTH) {
            return false;
        }
        for (const char* p = name_start; p != name_end; p++) {
            if (!islower(*p)) {
                return false;
            }
        }
        name_start = name_end + 1;
    }
    return true;
}

const char* split_path(const char* path, char* component) {
    const char* subpath = strchr(path + 1, SEPARATOR); // Pointer to second '/' character.
    if (!subpath) {
        return NULL; // Path is "/".
    }

    if (component) {
        size_t len = subpath - (path + 1);
        assert(len >= 1 && len <= MAX_FOLDER_NAME_LENGTH);
        strncpy(component, path + 1, len);
        component[len] = '\0';
    }
    return subpath;
}

void make_path_to_parent(const char* path, char* component, char parent_path[MAX_PATH_LENGTH + 1]) {
    if (strcmp(path, "/") == 0) {
        return; // Path is "/".
    }

    size_t len = strlen(path);
    const char* p = path + len - 2; // Point before final '/' character.
    // Move p to last-but-one '/' character.
    while (*p != SEPARATOR) {
        p--;
    }

    size_t subpath_len = p - path + 1; // Include '/' at p.
    strncpy(parent_path, path, subpath_len);
    parent_path[subpath_len] = '\0';

    if (component) {
        size_t component_len = len - subpath_len - 1; // Skip final '/' as well.
        assert(component_len >= 1 && component_len <= MAX_FOLDER_NAME_LENGTH);
        strncpy(component, p + 1, component_len);
        component[component_len] = '\0';
    }
}

// A wrapper for using strcmp in qsort.
// The arguments here are actually pointers to (const char*).
static int compare_string_pointers(const void* p1, const void* p2) {
    return strcmp(*(const char**)p1, *(const char**)p2);
}

const char** make_map_contents_array(HashMap* map) {
    size_t n_keys = hmap_size(map);
    const char** result = calloc(n_keys + 1, sizeof(char*));
    HashMapIterator it = hmap_iterator(map);
    const char** key = result;
    void* value = NULL;
    while (hmap_next(map, &it, key, &value)) {
        key++;
    }
    *key = NULL; // Set last array element to NULL.
    qsort(result, n_keys, sizeof(char*), compare_string_pointers);
    return result;
}

char* make_map_contents_string(HashMap* map) {
    const char** keys = make_map_contents_array(map);

    unsigned int result_size = 0; // Including ending null character.
    for (const char** key = keys; *key; ++key)
        result_size += strlen(*key) + 1;

    // Return empty string if map is empty.
    if (!result_size) {
        // Note we can't just return "", as it can't be free'd.
        char* result = malloc(1);
        *result = '\0';
        free(keys);
        return result;
    }

    char* result = malloc(result_size);
    char* position = result;
    for (const char** key = keys; *key; ++key) {
        size_t keylen = strlen(*key);
        assert(position + keylen <= result + result_size);
        strcpy(position, *key); // NOLINT: array size already checked.
        position += keylen;
        *position = ',';
        position++;
    }
    position--;
    *position = '\0';
    free(keys);
    return result;
}

bool is_ancestor(const char *path1, const char *path2) {
    return (strncmp(path1, path2, strlen(path1)) == 0) && (strcmp(path1, path2) != 0);
}

void make_path_to_LCA(const char* path1, const char* path2, char lca_path[MAX_PATH_LENGTH + 1]) {
    char comp1[MAX_FOLDER_NAME_LENGTH + 1], comp2[MAX_FOLDER_NAME_LENGTH + 1];
    lca_path[0] = SEPARATOR;
    size_t i = 0;
/*    while ((path1 = split_path(path1, comp1)) && (path2 = split_path(path2, comp2))) {
        if (strcmp(comp1, comp2) == 0) {
            strcpy(lca_path + used_len, comp1);
            lca_path[++used_len] = SEPARATOR;
            used_len++;
        }
    }*/

    while ((path1[i] != '\0' && path2[i] != '\0') && path1[i] == path2[i]) {
        lca_path[i] = path1[i];
        i++;
    }
    lca_path[i] = '\0';
    if (i > 1) {
        i -= 2;
        while (lca_path[i] != SEPARATOR) {
            lca_path[i--] = '\0';
        }
    }
}