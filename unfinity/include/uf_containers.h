#pragma once

/**
 * @brief Simple containers.
 *
 * Yeah, contaaaiiineeers. Meet the Vector and Map; we'll be using them a lot. We don't really have any other
 * generic data structures implemented, because we don't really need any other.
 *
 * Currently, we are using a `void*` backend and macro frontend. There are definitely other ways to do generic
 * containers. I'm not fan of using `void*`, but nothing really beats this approach in simplicity and ease of
 * debugging.
 *
 * Proper List implementation should be added once needed.
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "uf_common.h"

#include <stddef.h>

/*
 * Those functions and structures are for Vectors. Vectors are simply put a resizable/dynamic arrays. Do not
 * confuse them with Lists (linked-lists). You can read about Vectors here:
 * https://en.wikipedia.org/wiki/Dynamic_array
 */

typedef struct UfConVector UfConVector;

UfConVector* uf_con_vector_new(size_t element_size) _nodiscard_;
void uf_con_vector_free(UfConVector*);
void uf_con_vector_freep(UfConVector** ptr);
#define _autovector_ _cleanup_(uf_con_vector_freep)

size_t uf_con_vector_length(const UfConVector*);
size_t uf_con_vector_capacity(const UfConVector*);

void* uf_con_vector_push(UfConVector*, const void* data);
void* uf_con_vector_get(UfConVector*, size_t index) _nodiscard_;
void uf_con_vector_reserve(UfConVector*, size_t capacity);
void uf_con_vector_remove(UfConVector*, size_t index);

/*
 * Those functions and structures are for Maps. Map is just a list of key-value pairs (a dictionary if you
 * will). Maps are implemented as hash tables. You can read about them here:
 * https://en.wikipedia.org/wiki/Hash_table
 */

typedef struct UfConMap UfConMap;

UfConMap* uf_con_map_new(void) _nodiscard_;
void uf_con_map_free(UfConMap*);
void uf_con_map_freep(UfConMap** ptr);
#define _automap_ _cleanup_(uf_con_map_freep)

size_t uf_con_map_length(const UfConMap*);

bool uf_con_map_put(UfConMap*, const char* key, void* value);
void* uf_con_map_get(const UfConMap*, const char* key) _nodiscard_;
bool uf_con_map_next(const UfConMap*, size_t* iterator, const char** out_key, void** out_value);
bool uf_con_map_remove(UfConMap*, const char* key);
