#pragma once

/*
 * @brief Wrappers for standard memory allocation/deallocation functions.
 *
 * Use this to wrap all memory operations => It does null checking for you and panics when it fails, so you
 * don't have to care about OOM handling. This module (for now) doesn't support advanced memory management
 * techniques like regions (group/tagged allocations) or areas (allocate once and then reuse).
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 * */

#include "uf_common.h"

#include <stddef.h>

#if defined(__SANITIZE_ADDRESS__) || defined(__HAS_FEATURE_ADDRESS_SANITIZER)

#include <sanitizer/asan_interface.h>
#define _uf_mem_poison(addr, size) ASAN_POISON_MEMORY_REGION(addr, size)
#define _uf_mem_unpoison(addr, size) ASAN_UNPOISON_MEMORY_REGION(addr, size)

#else

#define _uf_mem_poison(addr, size) ((void)0)
#define _uf_mem_unpoison(addr, size) ((void)0)

#endif

void* uf_mem_malloc(size_t size) _nodiscard_ _malloc_ _alloc_size_(1);
void* uf_mem_calloc(size_t count, size_t size) _nodiscard_ _malloc_ _alloc_size_(1, 2);
void* uf_mem_zalloc(size_t size) _nodiscard_ _malloc_ _alloc_size_(1);
void* uf_mem_realloc(void* ptr, size_t new_size) _nodiscard_ _alloc_size_(2);

void uf_mem_free(void* ptr);
void uf_mem_freep(void** ptr);

#define _autofree_ _cleanup_(uf_mem_freep)

#define uf_mem_free_null(ptr)                                                                                \
    ({                                                                                                       \
        typeof(ptr)* _p_ = &(ptr);                                                                           \
        uf_mem_free(*_p_);                                                                                   \
        *_p_ = nullptr;                                                                                      \
    })
