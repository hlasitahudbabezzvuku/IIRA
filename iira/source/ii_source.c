/**
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_source.h"
#include "uf_containers.h"
#include "uf_memory.h"

#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define ARENA_BLOCK_SIZE (4 * 1024)
#define MAX_STACK_BUFFER 256

struct Source {
    const char* file_path;
    int file_descriptor;
    const char* file_buffer;
    size_t file_size;
    UfConVector* newline_offsets;

    UfMemRegion* interner_arena;
    UfConMap* interner_map;
};

Source* ii_src_new(const char* file_path)
{
    int file_descriptor = open(file_path, O_RDONLY);
    if (file_descriptor < 0) {
        return nullptr;
    }

    struct stat file_stat;
    if (fstat(file_descriptor, &file_stat) < 0 || !S_ISREG(file_stat.st_mode) || file_stat.st_size == 0) {
        close(file_descriptor);
        return nullptr;
    }

    size_t file_length = (size_t)file_stat.st_size;
    const char* file_map = mmap(nullptr, file_length, PROT_READ, MAP_PRIVATE, file_descriptor, 0);
    if (file_map == MAP_FAILED) {
        close(file_descriptor);
        return nullptr;
    }

    Source* source = uf_mem_zalloc(sizeof(Source));
    source->file_path = file_path;
    source->file_descriptor = file_descriptor;
    source->file_buffer = file_map;
    source->file_size = file_length;
    source->newline_offsets = uf_con_vector_new(sizeof(uint32_t));

    source->interner_arena = uf_mem_region_new(ARENA_BLOCK_SIZE);
    source->interner_map = uf_con_map_new();

    uf_con_vector_push(source->newline_offsets, &(uint32_t){0});

    return source;
}

void ii_src_free(Source* source)
{
    if (source == nullptr) {
        return;
    }

    if (source->file_buffer != nullptr && source->file_size > 0) {
        munmap((void*)source->file_buffer, source->file_size);
    }

    if (source->file_descriptor >= 0) {
        close(source->file_descriptor);
    }

    uf_con_vector_free(source->newline_offsets);
    uf_con_map_free(source->interner_map);
    uf_mem_region_free(source->interner_arena);
    uf_mem_free(source);
}

void ii_src_freep(Source** source)
{
    if (source != nullptr && *source != nullptr) {
        ii_src_free(*source);
        *source = nullptr;
    }
}

const char* ii_src_get_file_path(const Source* source)
{
    return source->file_path;
}

const char* ii_src_get_buffer(const Source* source)
{
    return source->file_buffer;
}

size_t ii_src_get_size(const Source* source)
{
    return source->file_size;
}

void ii_src_add_newline(Source* source, uint32_t offset)
{
    uf_con_vector_push(source->newline_offsets, &offset);
}

SourceLocation ii_src_resolve_location(const Source* source, uint32_t offset)
{
    SourceLocation location = {.line = 1, .column = 1};

    size_t line_count = uf_con_vector_length(source->newline_offsets);
    if _unlikely_ (line_count == 0) {
        return location;
    }

    /*
     * We are using binary search to find the correct line we are on. It's really the fastest way to search
     * through a sorted set. You can read more about it here: https://en.wikipedia.org/wiki/Binary_search.
     */
    size_t low = 0;
    size_t high = line_count - 1;
    size_t best_match = 0;

    while (low <= high) {
        size_t mid = low + (high - low) / 2;
        const uint32_t* mid_offset = uf_con_vector_get(source->newline_offsets, mid);

        if (*mid_offset <= offset) {
            best_match = mid;
            low = mid + 1;
        } else {
            if (mid == 0)
                break;
            high = mid - 1;
        }
    }

    const uint32_t* line_start = uf_con_vector_get(source->newline_offsets, best_match);

    location.line = (uint32_t)(best_match + 1);
    location.column = (offset - *line_start) + 1;

    return location;
}

const char* ii_src_resolve_line_bounds(const Source* source, uint32_t line, size_t* out_length)
{
    size_t line_count = uf_con_vector_length(source->newline_offsets);
    if (line == 0 || line > line_count) {
        *out_length = 0;
        return nullptr;
    }

    const uint32_t* start_offset = uf_con_vector_get(source->newline_offsets, line - 1);
    const char* line_ptr = source->file_buffer + *start_offset;

    uint32_t end_offset = (uint32_t)source->file_size;
    if (line < line_count) {
        const uint32_t* next_offset = uf_con_vector_get(source->newline_offsets, line);
        end_offset = *next_offset - 1;
    }

    *out_length = (end_offset > *start_offset) ? (end_offset - *start_offset) : 0;

    /* Yeah... I love Windows... */
    if (*out_length > 0 && line_ptr[*out_length - 1] == '\r') {
        *out_length -= 1;
    }

    return line_ptr;
}

/*
 * String interning lives in Source rather than Lexer because all compiler stages need access to
 * pointer-comparable strings. This way the Parser and later stages can use simple pointer equality to check
 * if two identifiers are the same, without needing strcmp.
 */
static const char* _intern_impl(Source* source, const char* raw, size_t length)
{
    /*
     * For short strings, use a stack buffer to avoid heap allocation in the common case. This significantly
     * speeds up interning of identifiers and keywords (typically < 32 chars).
     */
    char stack_buffer[MAX_STACK_BUFFER];
    bool fits_in_stack = length < MAX_STACK_BUFFER;

    char* search_str = nullptr;
    if _likely_ (fits_in_stack) {
        memcpy(stack_buffer, raw, length);
        stack_buffer[length] = '\0';
        search_str = stack_buffer;
    } else {
        search_str = uf_mem_region_malloc(source->interner_arena, length + 1);
        memcpy(search_str, raw, length);
        search_str[length] = '\0';
    }

    /* Check if we've already interned this string - enables pointer equality comparison. */
    const char* existing = uf_con_map_get(source->interner_map, search_str);
    if (existing != nullptr) {
        return existing;
    }

    /*
     * Need to copy to arena: stack buffer won't persist after return, and for heap search_str we still need
     * to own the memory. When fits_in_stack is true, search_str points to stack and we must allocate;
     * otherwise it's already in arena so reuse it.
     */
    char* final_str = fits_in_stack ? uf_mem_region_malloc(source->interner_arena, length + 1) : search_str;
    if (fits_in_stack) {
        memcpy(final_str, search_str, length + 1);
    }

    uf_con_map_put(source->interner_map, final_str, (void*)final_str);
    return final_str;
}

const char* ii_src_intern(Source* source, const char* str, size_t len)
{
    if (str == nullptr || len == 0) {
        return nullptr;
    }
    return _intern_impl(source, str, len);
}

const char* ii_src_intern_cstr(Source* source, const char* cstr)
{
    if (cstr == nullptr) {
        return nullptr;
    }
    return _intern_impl(source, cstr, strlen(cstr));
}
