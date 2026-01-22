#include "uf_memory.h"
#include "uf_logger.h"

#include <stdlib.h>
#include <string.h>

void* uf_mem_malloc(size_t size)
{
    if _unlikely_ (size == 0) {
        return nullptr;
    }

    void* ptr = malloc(size);
    if _unlikely_ (!ptr) {
        uf_log_panic("OOM: Failed to allocate %zu bytes", size);
    }

    return ptr;
}

void* uf_mem_calloc(size_t count, size_t size)
{
    if _unlikely_ (count == 0 || size == 0) {
        return nullptr;
    }

    if _unlikely_ (__builtin_mul_overflow(count, size, (size_t[]){0})) {
        uf_log_panic("OOM: Overflow in calloc (%zu * %zu)", count, size);
    }

    void* ptr = calloc(count, size);
    if _unlikely_ (!ptr) {
        uf_log_panic("OOM: Failed to allocate %zu bytes (calloc)", count * size);
    }

    return ptr;
}

void* uf_mem_zalloc(size_t size)
{
    void* ptr = uf_mem_malloc(size);
    memset(ptr, 0, size);
    return ptr;
}

void* uf_mem_realloc(void* ptr, size_t new_size)
{
    if (new_size == 0) {
        uf_mem_free(ptr);
        return nullptr;
    }

    void* new_ptr = realloc(ptr, new_size);
    if _unlikely_ (!new_ptr) {
        uf_log_panic("OOM: Failed to realloc to %zu bytes", new_size);
    }

    return new_ptr;
}

void uf_mem_free(void* ptr)
{
    if (ptr) {
        free(ptr);
    }
}

void uf_mem_freep(void** ptr)
{
    if (ptr && *ptr) {
        uf_mem_free(*ptr);
    }
}
