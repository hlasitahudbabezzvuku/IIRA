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

