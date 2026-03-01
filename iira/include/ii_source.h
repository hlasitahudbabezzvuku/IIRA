#pragma once

/**
 * @brief A simple Source Manager for Diagnostic Engine.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "uf_common.h"

#include <stddef.h>
#include <stdint.h>

typedef struct SourceSpan SourceSpan;
struct SourceSpan {
    uint32_t offset;
    uint32_t length;
};

typedef struct SourceLocation SourceLocation;
struct SourceLocation {
    uint32_t line;
    uint32_t column;
};

typedef struct Source Source;

Source* ii_src_new(const char* file_path) _nodiscard_;
void ii_src_free(Source*);
void ii_src_freep(Source**);
#define _autosrc_ _cleanup_(ii_src_freep)

const char* ii_src_get_file_path(const Source*) _nodiscard_;
const char* ii_src_get_buffer(const Source*) _nodiscard_;
size_t ii_src_get_size(const Source*) _nodiscard_;

void ii_src_add_newline(Source*, uint32_t offset);
SourceLocation ii_src_resolve_location(const Source*, uint32_t offset) _nodiscard_;
const char* ii_src_resolve_line_bounds(const Source*, uint32_t line, size_t* out_length) _nodiscard_;
