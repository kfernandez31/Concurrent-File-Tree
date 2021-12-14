#include "HashMap.h"

// We fix the number of hash buckets for simplicity.
#define N_BUCKETS 8

typedef struct Pair Pair;

struct Pair {
    char* key;
    void* value;
    Pair* next; // Next item in a single-linked list.
};

struct HashMap {
    Pair* buckets[N_BUCKETS]; // Linked lists of key-value pairs.
    size_t size; // total number of entries in map.
};

static Pair* hmap_find(HashMap* map, unsigned h, const char* key)
{
    for (Pair* p = map->buckets[h]; p; p = p->next) {
        if (strcmp(key, p->key) == 0) {
            return p;
        }
    }
    return NULL;
}

static unsigned int get_hash(const char* key)
{
    unsigned int hash = 17;
    while (*key) {
        hash = (hash << 3) + hash + *key;
        ++key;
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
    for (int h = 0; h < N_BUCKETS; ++h) {
        Pair* p = map->buckets[h];
        while (p) {
            Pair* q = p;
            p = p->next;
            free(q->key);
            free(q);
        }
    }
    free(map);
}

void* hmap_get(HashMap* map, const char* key)
{
    unsigned h = get_hash(key);
    Pair* p = hmap_find(map, h, key);
    if (p)
        return p->value;
    else
        return NULL;
}

bool hmap_insert(HashMap* map, const char* key, void* value)
{
    if (!value)
        return false;
    int h = get_hash(key);
    Pair* p = hmap_find(map, h, key);
    if (p)
        return false; // Already exists.
    Pair* new_p = malloc(sizeof(Pair));
    new_p->key = strdup(key);
    new_p->value = value;
    new_p->next = map->buckets[h];
    map->buckets[h] = new_p;
    map->size++;
    return true;
}

bool hmap_remove(HashMap* map, const char* key)
{
    int h = get_hash(key);
    Pair** pp = &(map->buckets[h]);
    while (*pp) {
        Pair* p = *pp;
        if (strcmp(key, p->key) == 0) {
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

size_t hmap_size(HashMap* map)
{
    return map->size;
}

HashMapIterator hmap_iterator(HashMap* map) {
    HashMapIterator it = { 0, map->buckets[0] };
    return it;
}

bool hmap_next(HashMap* map, HashMapIterator* it, const char** key, void** value)
{
    Pair* p = it->pair;
    while (!p && it->bucket < N_BUCKETS - 1) {
        p = map->buckets[++it->bucket];
    }
    if (!p)
        return false;
    *key = p->key;
    *value = p->value;
    it->pair = p->next;
    return true;
}

void *hmap_pop_value(HashMap* map, const char *key) {
    unsigned h = get_hash(key);
    Pair* p = hmap_find(map, h, key);
    if (p) {
        void *value = p->value;
        hmap_remove(map, key);
        return value;
    }
    else {
        return NULL;
    }
}