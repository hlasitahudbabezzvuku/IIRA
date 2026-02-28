/**
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_source_manager.h"
#include "uf_containers.h"
#include "uf_logger.h"
#include "uf_memory.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdckdint.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

struct SourceManager {
    const char* file_path;
    int file_descriptor;
    const char* file_buffer;
    size_t file_size;
    UfConVector* newline_offsets;
};

SourceManager* ii_src_manager_new(const char* file_path)
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

    SourceManager* manager = uf_mem_zalloc(sizeof(SourceManager));
    manager->file_path = file_path;
    manager->file_descriptor = file_descriptor;
    manager->file_buffer = file_map;
    manager->file_size = file_length;
    manager->newline_offsets = uf_con_vector_new(sizeof(uint32_t));

    uf_con_vector_push(manager->newline_offsets, &(uint32_t){0});

    return manager;
}

void ii_src_manager_free(SourceManager* manager)
{
    if (manager == nullptr) {
        return;
    }

    if (manager->file_buffer != nullptr && manager->file_size > 0) {
        munmap((void*)manager->file_buffer, manager->file_size);
    }

    if (manager->file_descriptor >= 0) {
        close(manager->file_descriptor);
    }

    uf_con_vector_free(manager->newline_offsets);
    uf_mem_free(manager);
}

void ii_src_manager_freep(SourceManager** manager_ptr)
{
    if (manager_ptr != nullptr && *manager_ptr != nullptr) {
        ii_src_manager_free(*manager_ptr);
        *manager_ptr = nullptr;
    }
}

const char* ii_src_get_buffer(const struct SourceManager* manager)
{
    return manager->file_buffer;
}

size_t ii_src_get_size(const struct SourceManager* manager)
{
    return manager->file_size;
}

void ii_src_add_newline(SourceManager* manager, uint32_t offset)
{
    uint32_t next_line_start;
    if (ckd_add(&next_line_start, offset, 1)) {
        uf_log_panic("Source buffer offset arithmetic overflowed");
    }

    uf_con_vector_push(manager->newline_offsets, &next_line_start);
}

SourceLocation ii_src_resolve_location(const SourceManager* manager, uint32_t offset)
{
    SourceLocation location = {.line = 1, .column = 1};

    size_t line_count = uf_con_vector_length(manager->newline_offsets);
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
        const uint32_t* mid_offset = uf_con_vector_get(manager->newline_offsets, mid);

        if (*mid_offset <= offset) {
            best_match = mid;
            low = mid + 1;
        } else {
            if (mid == 0)
                break;
            high = mid - 1;
        }
    }

    const uint32_t* line_start = uf_con_vector_get(manager->newline_offsets, best_match);

    location.line = (uint32_t)(best_match + 1);
    location.column = (offset - *line_start) + 1;

    return location;
}

const char* ii_src_resolve_line_bounds(const SourceManager* manager, uint32_t line, size_t* out_length)
{
    size_t line_count = uf_con_vector_length(manager->newline_offsets);
    if (line == 0 || line > line_count) {
        *out_length = 0;
        return nullptr;
    }

    const uint32_t* start_offset = uf_con_vector_get(manager->newline_offsets, line - 1);
    const char* line_ptr = manager->file_buffer + *start_offset;

    uint32_t end_offset = (uint32_t)manager->file_size;
    if (line < line_count) {
        const uint32_t* next_offset = uf_con_vector_get(manager->newline_offsets, line);
        end_offset = *next_offset - 1;
    }

    *out_length = (end_offset > *start_offset) ? (end_offset - *start_offset) : 0;

    /* Yeah... I love Windows... */
    if (*out_length > 0 && line_ptr[*out_length - 1] == '\r') {
        *out_length -= 1;
    }

    return line_ptr;
}
