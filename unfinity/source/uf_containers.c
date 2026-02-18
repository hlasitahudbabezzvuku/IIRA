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

    UfConVector* vec = uf_mem_zalloc(sizeof(UfConVector));
    vec->element_size = element_size;
    vec->length = 0;
    vec->capacity = VEC_INITIAL_CAPACITY;
    vec->data = uf_mem_calloc(vec->capacity, element_size);

    return vec;
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

    size_t new_cap = vector->capacity;
    while (new_cap < needed) {
        if (ckd_mul(&new_cap, new_cap, VEC_GROWTH_FACTOR)) {
            uf_log_panic("Vector capacity overflow");
        }
    }

    size_t total_bytes;
    if (ckd_mul(&total_bytes, new_cap, vector->element_size)) {
        uf_log_panic("Vector size in bytes overflow");
    }

    vector->data = uf_mem_realloc(vector->data, total_bytes);

    size_t old_bytes = vector->capacity * vector->element_size;
    memset(vector->data + old_bytes, 0, total_bytes - old_bytes);

    vector->capacity = new_cap;
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
        uint8_t* dest = vector->data + (index * vector->element_size);
        uint8_t* src = dest + vector->element_size;
        memmove(dest, src, tail_count * vector->element_size);
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
