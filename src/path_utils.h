#pragma once

#include "HashMap.h"
#include <stdbool.h>
#include <string.h>

// Max length of path (excluding terminating null character).
#define MAX_PATH_LENGTH 4095
// Max length of folder name (excluding terminating null character).
#define MAX_FOLDER_NAME_LENGTH 255

/**
 * Checks whether `path` represents a valid path.
 * Valid paths are '/'-separated sequences of folder names, always starting and ending with '/'.
 * Valid paths have length at most MAX_PATH_LENGTH (and at least 1).
 * Valid folder names are are sequences of 'a'-'z' ASCII characters,
 * of length from 1 to MAX_FOLDER_NAME_LENGTH.
 * @param path : string to check
 * @return : true if `path` is a valid path, false otherwise
 */
bool is_valid_path(const char *path_name);

// Return the subpath obtained by removing the first component.
// Args:
// - `path`: should be a valid path (see `is_path_valid`).
// - `component`: if not NULL, should be a buffer of size at least MAX_FOLDER_NAME_LENGTH + 1.
//    Then the first component will be copied there (without any '/' characters).
// If path is "/", returns NULL and leaves `component` unchanged.
// Otherwise the returns a pointer into `path`, representing a valid subpath.
//
// This can be used to iterate over all components of a path:
//     char component[MAX_FOLDER_NAME_LENGTH + 1];
//     const char* subpath = path;
//     while (subpath = split_path(subpath, component))
//         printf("%s", component);
const char* split_path(const char* path, char* component);

// Stores a copy of the subpath obtained by removing the last component in `parent_path`.
// Args:
// - `path`: should be a valid path (see `is_path_valid`).
// - `component`: if not NULL, should be a buffer of size at least MAX_FOLDER_NAME_LENGTH + 1.
//    Then the last component will be copied there (without any '/' characters).
// If path is "/", returns NULL and leaves `component` unchanged.
// Otherwise the result is a valid path.
void make_path_to_parent(const char* path, char* component, char parent_path[MAX_FOLDER_NAME_LENGTH + 1]);

/**
 * Gets the name and length of `n`-th directory along the path starting from the root.
 * @param path : file path
 * @param n : depth of the directory (must be > 0)
 * @param index : index of the directory in the path
 * @param length : length of the directory
 */
//void get_nth_dir_name_and_length(const char *path, const size_t n, size_t *index, size_t *length);

/**
 * Checks whether both directories lie on the same path in a tree,
 * and if path2 branches out from path1.
 * @param path1 : path to the first directory
 * @param path2 : path to the second directory
 * @return : whether the first directory is an ancestor of the second
 */
bool is_ancestor(const char *path1, const char *path2);

// Return an array containing all keys, lexicographically sorted.
// The result is null-terminated.
// Keys are not copied, they are only valid as long as the map.
// The caller should free the result.
const char** make_map_contents_array(HashMap* map);

// Return a string containing all keys in map, sorted, comma-separated.
// The result has no trailing comma. An empty map yields an empty string.
// The caller should free the result.
char* make_map_contents_string(HashMap* map);
