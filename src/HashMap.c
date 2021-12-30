#include "HashMap.h"

// We fix the number of hash buckets for simplicity.
#define N_BUCKETS 8

typedef struct Pair {
    char* key;
    void* value;
    struct Pair* next; // Next item in a single-linked list.
} Pair;

struct HashMap {
    Pair* buckets[N_BUCKETS]; // Linked lists of key-value pairs.
    size_t size; // total number of entries in map.
};

void print_map(HashMap* map) {
    const char* key = NULL;
    void* value = NULL;

    printf("Size=%zd\n", hmap_size(map));
    HashMapIterator it = hmap_new_iterator(map);
    while (hmap_next(map, &it, &key, &value)) {
        printf("Key=%s Value=%p\n", key, value);
    }
    printf("\n");
}

static Pair *hmap_find(HashMap *map, const unsigned h, const char *key, const size_t size) {
    for (Pair* p = map->buckets[h]; p; p = p->next) {
        if (strncmp(key, p->key, size) == 0) {
            return p;
        }
    }
    return NULL;
}

static unsigned get_hash(const char* key, const size_t size) {
    unsigned hash = 17;

    for (size_t i = 0; i < size && *key; i++, key++) {
        hash = (hash << 3) + hash + *key;
    }
    return hash % N_BUCKETS;
}

HashMap* hmap_new() {
    HashMap* map = safe_calloc(1, sizeof(HashMap));
    if (!map) {
        return NULL;
    }
    memset(map, 0, sizeof(HashMap));
    return map;
}

void hmap_free(HashMap* map) {
    if (map) {
        for (size_t h = 0; h < N_BUCKETS; ++h) {
            Pair *p = map->buckets[h];
            while (p) {
                Pair *q = p;
                p = p->next;
                free(q->key);
                free(q);
            }
        }
        free(map);
    }
}
//TODO: size/length naming
void *hmap_get(HashMap *map, const bool pop, const char *key, const size_t size) {
    unsigned h = get_hash(key, size);
    Pair* p = hmap_find(map, h, key, size);
    if (p) {
        void *value = p->value;
        if (pop) {
            hmap_remove(map, key, size);
        }
        return value;
    }
    else {
        return NULL;
    }
}

bool hmap_insert(HashMap *map, const char *key, const size_t size, void *value) {
    if (!value) {
        return false;
    }

    unsigned h = get_hash(key, size);
    Pair* p = hmap_find(map, h, key, size);
    if (p) {
        return false; // Already exists.
    }

    Pair* new_p = safe_malloc(sizeof(Pair));
    new_p->key = strndup(key, size);
    new_p->value = value;
    new_p->next = map->buckets[h];
    map->buckets[h] = new_p;
    map->size++;

    return true;
}

bool hmap_remove(HashMap *map, const char *key, const size_t size) {
    unsigned h = get_hash(key, size);
    Pair** pp = &(map->buckets[h]);

    while (*pp) {
        Pair* p = *pp;
        if (strncmp(key, p->key, size) == 0) {
            *pp = p->next;
            free(p->key);
            free(p);
            map->size--;
            return true;
        }
        pp = &(p->next);
    }

    return false;
}

size_t hmap_size(HashMap* map) {
    return map->size;
}

HashMapIterator hmap_new_iterator(HashMap* map) {
    return (HashMapIterator) {
        .bucket = 0,
        .pair = map->buckets[0]
    };
}

bool hmap_next(HashMap* map, HashMapIterator* it, const char** key, void** value) {
    Pair* p = it->pair;

    while (!p && it->bucket < N_BUCKETS - 1) {
        p = map->buckets[++it->bucket];
    }
    if (!p) {
        return false;
    }
    *key = p->key;
    *value = p->value;
    it->pair = p->next;
    return true;
}

// A wrapper for using strcmp in qsort.
// The arguments here are actually pointers to (const char*).
static int compare_string_pointers(const void* p1, const void* p2) {
    return strcmp(*(const char**)p1, *(const char**)p2);
}

const char** make_map_contents_array(HashMap* map) {
    size_t n_keys = hmap_size(map);
    const char** result = calloc(n_keys + 1, sizeof(char*));
    HashMapIterator it = hmap_new_iterator(map);
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
        return result;
    }

    char* result = malloc(result_size);
    char* position = result;
    for (const char** key = keys; *key; ++key) {
        size_t keylen = strlen(*key);
        //assert(position + keylen <= result + result_size);
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
