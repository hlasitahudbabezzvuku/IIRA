/**
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "uf_memory.h"
#include "uf_common.h"
#include "uf_logger.h"

#include <stdint.h>
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

/*
 * Regions (arena/linear allocators)
 */

struct UfMemRegionChunk {
    struct UfMemRegionChunk* next;
    size_t capacity;
    size_t used;
    uint8_t data[] _aligned_(16); /* We are aligning everything to addresses divisible by 16. Keep that in
                                     mind because few things are done in particular way thanks to that. */
};

struct UfMemRegion {
    struct UfMemRegionChunk* head;
    struct UfMemRegionChunk* tail;
    size_t block_size;
};

static struct UfMemRegionChunk* _region_new_chunk(size_t size)
{
    struct UfMemRegionChunk* chunk = uf_mem_malloc(sizeof(struct UfMemRegionChunk) + size);
    chunk->next = nullptr;
    chunk->capacity = size;
    chunk->used = 0;
    return chunk;
}

UfMemRegion* uf_mem_region_new(size_t block_size)
{
    UfMemRegion* region = uf_mem_malloc(sizeof(UfMemRegion));
    region->block_size = (block_size > 0) ? block_size : 8192;
    region->head = _region_new_chunk(region->block_size);
    region->tail = nullptr;
    return region;
}

void* uf_mem_region_malloc(UfMemRegion* region, size_t size)
{
    size_t aligned = (size + 15) & ~15;

    /* The allocation fits into the current chunk (likeliest) */
    if _likely_ (region->head->used + aligned <= region->head->capacity) {
        void* ptr = region->head->data + region->head->used;
        region->head->used += aligned;
        return ptr;
    }

    /* The allocation is bigger than block_size. We'll put it in tail so we don't clog the head */
    if (aligned > region->block_size) {
        struct UfMemRegionChunk* new_chunk = _region_new_chunk(aligned);
        new_chunk->next = region->tail;
        region->tail = new_chunk;
        new_chunk->used = aligned;
        return new_chunk->data;
    }

    /* Otherwise, create new standard block */
    struct UfMemRegionChunk* new_chunk = _region_new_chunk(region->block_size);
    region->head->next = region->tail;
    region->tail = region->head;
    region->head = new_chunk;

    new_chunk->used += aligned;
    return new_chunk->data;
}

void* uf_mem_region_zalloc(UfMemRegion* region, size_t size)
{
    void* ptr = uf_mem_region_malloc(region, size);
    memset(ptr, 0, size);
    return ptr;
}

void uf_mem_region_reset(UfMemRegion* region)
{
    struct UfMemRegionChunk* current = region->tail;

    while (current != nullptr) {
        struct UfMemRegionChunk* next = current->next;
        uf_mem_free(current);
        current = next;
    }

    region->tail = nullptr;
    region->head->used = 0;
}

void uf_mem_region_free(UfMemRegion* region)
{
    if _unlikely_ (!region) {
        return;
    }

    uf_mem_region_reset(region);
    uf_mem_free(region->head);
    uf_mem_free(region);
}

void uf_mem_region_freep(UfMemRegion** region)
{
    if _likely_ (region && *region)
        uf_mem_region_free(*region);
}
