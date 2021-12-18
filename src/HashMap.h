#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "safe_allocations.h"

// A structure representing a mapping from keys to values.
// Keys are C-strings (null-terminated char*), all distinct.
// Values are non-null pointers (void*, which you can cast to any other pointer type).
typedef struct HashMap HashMap;

typedef struct HashMapIterator {
    int bucket;
    void* pair;
} HashMapIterator;

// Create a new, empty map.
HashMap* hmap_new();

// Clear the map and free its memory. This frees the map and the keys
// copied by hmap_insert, but does not free any values.
void hmap_free(HashMap* map);

// Get the value stored under `key`, or NULL if not present.
void *hmap_get(HashMap *map, const bool pop, const char *key, const size_t size);

// Insert a `value` under `key` and return true,
// or do nothing and return false if `key` already exists in the map.
// `value` must not be NULL.
// (The caller can free `key` at any time - the map internally uses a copy of it).
bool hmap_insert(HashMap *map, const char *key, const size_t size, void *value);

// Remove the value under `key` and return true (the value is not free'd),
// or do nothing and return false if `key` was not present.
bool hmap_remove(HashMap *map, const char *key, const size_t size);

// Return the number of elements in the map.
size_t hmap_size(HashMap* map);

// Return an iterator to the map. See `hmap_next`.
HashMapIterator hmap_new_iterator(HashMap* map);

// Set `*key` and `*value` to the current element pointed by iterator and
// move the iterator to the next element.
// If there are no more elements, leaves `*key` and `*value` unchanged and
// returns false.
//
// The map cannot be modified between calls to `hmap_new_iterator` and `hmap_next`.
//
// Usage: ```
//     const char* key;
//     void* value;
//     HashMapIterator it = hmap_new_iterator(map);
//     while (hmap_next(map, &it, &key, &value))
//         tree_pop_recursive(key, value);
// ```
bool hmap_next(HashMap* map, HashMapIterator* it, const char** key, void** value);

