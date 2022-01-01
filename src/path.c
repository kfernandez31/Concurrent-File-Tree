#include "path.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#define SEPARATOR '/'

size_t get_path_depth(const char *path) {
    size_t res = 0;
    while (*path) {
        if (*path == SEPARATOR) {
            res++;
        }
        path++;
    }
    return res - 1;
}

bool is_valid_path_name(const char *path_name) {
    size_t length = strlen(path_name);
    bool sep = false;

    if (length == 0 || length > MAX_PATH_LENGTH ||
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

void get_nth_dir_name_and_length(const char *path, const size_t n, size_t *index, size_t *length) {
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

bool is_ancestor(const char *path1, const char *path2) {
    return strncmp(path1, path2, strlen(path1)) == 0;
}
