#pragma once



#include <stdbool.h>
#include <string.h>

#define MAX_DIR_NAME_LENGTH 255
#define MAX_PATH_LENGTH 4095

/**
 * Calculates the depth of the `path` based on its number of separators.
 * @param path : file path
 * @return : depth of the path
 */
size_t get_path_depth(const char *path);

/**
 * Checks whether `dir_name` represents a directory in the accepted convention
 * (/dir_1/dir_2/.../dir_n/ - a string of valid directory names separated by slashes).
 * @param dir_name : string to check
 * @return : true if `dir_name` is a valid path, false otherwise
 */
bool is_valid_path_name(const char *path_name);

/**
 * Gets the name and length of `n`-th directory along the path starting from the root.
 * @param path : file path
 * @param n : depth of the directory (must be > 0)
 * @param index : index of the directory in the path
 * @param length : length of the directory
 */
void get_nth_dir_name_and_length(const char *path, const size_t n, size_t *index, size_t *length);

/**
 * Checks whether both directories lie on the same path in a tree,
 * and if path2 branches out from path1.
 * @param path1 : path to the first directory
 * @param path2 : path to the second directory
 * @return : whether the first directory is an ancestor of the second
 */
bool is_ancestor(const char *path1, const char *path2);
