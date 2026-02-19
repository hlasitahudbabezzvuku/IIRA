/**
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "uf_containers.h"
#include "uf_logger.h"
#include "uf_memory.h"

#include <stdckdint.h>
#include <stdint.h>
#include <string.h>

#define VEC_INITIAL_CAPACITY 8
#define VEC_GROWTH_FACTOR 2

#define MAP_INITIAL_CAPACITY 16
#define MAP_LOAD_FACTOR_NUM 3
#define MAP_LOAD_FACTOR_DEN 4
#define MAP_GROWTH_FACTOR 2

/**
 * Vector
 **/

struct UfConVector {
    uint8_t* data;
    size_t length;
    size_t capacity;
    size_t element_size;
};

UfConVector* uf_con_vector_new(size_t element_size)
{
    uf_assert(element_size > 0);

    UfConVector* vector = uf_mem_zalloc(sizeof(UfConVector));
    vector->element_size = element_size;
    vector->length = 0;
    vector->capacity = VEC_INITIAL_CAPACITY;
    vector->data = uf_mem_calloc(vector->capacity, element_size);

    return vector;
}

void uf_con_vector_free(UfConVector* vector)
{
    if (!vector) {
        return;
    }

    if (vector->data) {
        uf_mem_free(vector->data);
    }

    uf_mem_free(vector);
}

void uf_con_vector_freep(UfConVector** ptr)
{
    if (ptr && *ptr) {
        uf_con_vector_free(*ptr);
        *ptr = nullptr;
    }
}

static void _vec_ensure_capacity(UfConVector* vector, size_t needed)
{
    if _likely_ (needed <= vector->capacity) {
        return;
    }

    size_t new_capacity = vector->capacity;
    while (new_capacity < needed) {
        if (ckd_mul(&new_capacity, new_capacity, VEC_GROWTH_FACTOR)) {
            uf_log_panic("Vector capacity overflow");
        }
    }

    size_t total_bytes;
    if (ckd_mul(&total_bytes, new_capacity, vector->element_size)) {
        uf_log_panic("Vector size in bytes overflow");
    }

    vector->data = uf_mem_realloc(vector->data, total_bytes);

    size_t old_bytes = vector->capacity * vector->element_size;
    memset(vector->data + old_bytes, 0, total_bytes - old_bytes);

    vector->capacity = new_capacity;
}

void* uf_con_vector_push(UfConVector* vector, const void* data)
{
    uf_assert(vector != nullptr);

    _vec_ensure_capacity(vector, vector->length + 1);

    uint8_t* target = vector->data + (vector->length * vector->element_size);
    if (data) {
        memcpy(target, data, vector->element_size);
    } else {
        memset(target, 0, vector->element_size);
    }

    vector->length++;
    return target;
}

void* uf_con_vector_get(UfConVector* vector, size_t index)
{
    uf_assert(vector != nullptr);

    if (_unlikely_(index >= vector->length)) {
        uf_log_panic("Vector index out of bounds: %zu >= %zu", index, vector->length);
    }

    return vector->data + (index * vector->element_size);
}

void uf_con_vector_remove(UfConVector* vector, size_t index)
{
    uf_assert(vector != nullptr);

    if (_unlikely_(index >= vector->length)) {
        uf_log_panic("Vector index out of bounds: %zu >= %zu", index, vector->length);
    }

    size_t tail_count = vector->length - 1 - index;
    if (tail_count > 0) {
        uint8_t* destination = vector->data + (index * vector->element_size);
        uint8_t* source = destination + vector->element_size;
        memmove(destination, source, tail_count * vector->element_size);
    }

    vector->length--;
}

size_t uf_con_vector_length(const UfConVector* vector)
{
    return vector ? vector->length : 0;
}

size_t uf_con_vector_capacity(const UfConVector* vector)
{
    return vector ? vector->capacity : 0;
}

void uf_con_vector_reserve(UfConVector* vector, size_t capacity)
{
    uf_assert(vector != nullptr);

    if (capacity > vector->capacity) {
        _vec_ensure_capacity(vector, capacity);
    }
}

/**
 * Map
 **/

static size_t _hash_with_fnv1a(const char* string)
{
    size_t hash = 0xcbf29ce484222325;
    unsigned char ch;

    while ((ch = (unsigned char)*string++)) {
        hash ^= ch;
        hash *= 0x00000100000001b3;
    }

    return hash;
}

enum MapEntryState { MAP_SLOT_EMPTY, MAP_SLOT_OCCUPIED, MAP_SLOT_TOMBSTONE };

struct MapEntry {
    char* key;
    void* value;
    size_t hash;
    enum MapEntryState state;
};

struct UfConMap {
    struct MapEntry* entries;
    size_t capacity;
    size_t count;
    size_t occupied;
};

UfConMap* uf_con_map_new(void)
{
    UfConMap* map = uf_mem_zalloc(sizeof(UfConMap));
    map->capacity = MAP_INITIAL_CAPACITY;
    map->entries = uf_mem_calloc(map->capacity, sizeof(struct MapEntry));

    return map;
}

void uf_con_map_free(UfConMap* map)
{
    if (!map) {
        return;
    }

    for (size_t i = 0; i < map->capacity; ++i) {
        if (map->entries[i].key) {
            uf_mem_free(map->entries[i].key);
        }
    }

    uf_mem_free(map->entries);
    uf_mem_free(map);
}

void uf_con_map_freep(UfConMap** ptr)
{
    if (ptr && *ptr) {
        uf_con_map_free(*ptr);
        *ptr = nullptr;
    }
}

static void _map_resize(UfConMap* map, size_t new_capacity)
{
    struct MapEntry* old_entries = map->entries;
    size_t old_capacity = map->capacity;

    map->entries = uf_mem_calloc(new_capacity, sizeof(struct MapEntry));
    map->capacity = new_capacity;
    map->count = 0;
    map->occupied = 0;

    for (size_t i = 0; i < old_capacity; ++i) {
        if (old_entries[i].state == MAP_SLOT_OCCUPIED) {
            size_t hash = old_entries[i].hash;
            size_t index = hash & (new_capacity - 1);

            while (true) {
                if (map->entries[index].state == MAP_SLOT_EMPTY) {
                    map->entries[index].key = old_entries[i].key;
                    map->entries[index].value = old_entries[i].value;
                    map->entries[index].hash = hash;
                    map->entries[index].state = MAP_SLOT_OCCUPIED;
                    map->count++;
                    map->occupied++;
                    break;
                }
                index = (index + 1) & (new_capacity - 1);
            }
        } else if (old_entries[i].key) {
            uf_mem_free(old_entries[i].key);
        }
    }

    uf_mem_free(old_entries);
}

bool uf_con_map_put(UfConMap* map, const char* key, void* value)
{
    uf_assert(map != nullptr);
    uf_assert(key != nullptr);

    if ((map->occupied + 1) * MAP_LOAD_FACTOR_DEN >= map->capacity * MAP_LOAD_FACTOR_NUM) {
        size_t new_capacity;

        if (ckd_mul(&new_capacity, map->capacity, MAP_GROWTH_FACTOR)) {
            uf_log_panic("Map capacity overflow");
        }

        _map_resize(map, new_capacity);
    }

    size_t hash = _hash_with_fnv1a(key);
    size_t index = hash & (map->capacity - 1);

    /**
     * Here we use linear probing to solve the collisions. It's form of open addressing. Linear probing is
     * probably simplest yet really fast way of resolving collisions within the map's key storage.
     *
     * Here you can read about liner probing: https://en.wikipedia.org/wiki/Linear_probing
     * And here you can read about open addressing: https://en.wikipedia.org/wiki/Open_addressing
     **/
    while (true) {
        struct MapEntry* entry = &map->entries[index];

        if (entry->state == MAP_SLOT_EMPTY) {
            size_t key_len = strlen(key) + 1;
            entry->key = uf_mem_malloc(key_len);
            memcpy(entry->key, key, key_len);

            entry->value = value;
            entry->hash = hash;
            entry->state = MAP_SLOT_OCCUPIED;

            map->count++;
            map->occupied++;
            return true;
        } else if ((entry->state == MAP_SLOT_OCCUPIED) &&
                   (entry->hash == hash && strcmp(entry->key, key) == 0)) {
            entry->value = value;
            return false;
        }

        index = (index + 1) & (map->capacity - 1);
    }
}

void* uf_con_map_get(const UfConMap* map, const char* key)
{
    if (!map || !key) {
        return nullptr;
    }

    size_t hash = _hash_with_fnv1a(key);
    size_t index = hash & (map->capacity - 1);
    size_t start_index = index;

    do {
        const struct MapEntry* entry = &map->entries[index];

        if (entry->state == MAP_SLOT_EMPTY) {
            return nullptr;
        }

        if (entry->state == MAP_SLOT_OCCUPIED) {
            if (entry->hash == hash && strcmp(entry->key, key) == 0) {
                return entry->value;
            }
        }

        index = (index + 1) & (map->capacity - 1);
    } while (index != start_index);

    return nullptr;
}

bool uf_con_map_remove(UfConMap* map, const char* key)
{
    uf_assert(map != nullptr);

    if (!key) {
        return false;
    }

    size_t hash = _hash_with_fnv1a(key);
    size_t index = hash & (map->capacity - 1);
    size_t start_index = index;

    do {
        struct MapEntry* entry = &map->entries[index];

        if (entry->state == MAP_SLOT_EMPTY) {
            return false;
        }

        if (entry->state == MAP_SLOT_OCCUPIED) {
            if (entry->hash == hash && strcmp(entry->key, key) == 0) {
                uf_mem_free(entry->key);
                entry->key = nullptr;
                entry->value = nullptr;
                entry->state = MAP_SLOT_TOMBSTONE;
                map->count--;
                return true;
            }
        }

        index = (index + 1) & (map->capacity - 1);
    } while (index != start_index);

    return false;
}

size_t uf_con_map_length(const UfConMap* map)
{
    return map ? map->count : 0;
}

bool uf_con_map_next(const UfConMap* map, size_t* iterator, const char** out_key, void** out_value)
{
    if (!map || !iterator) {
        return false;
    }

    while (*iterator < map->capacity) {
        struct MapEntry* entry = &map->entries[*iterator];
        (*iterator)++;

        if (entry->state == MAP_SLOT_OCCUPIED) {
            if (out_key) {
                *out_key = entry->key;
            }

            if (out_value) {
                *out_value = entry->value;
            }

            return true;
        }
    }

    return false;
}
