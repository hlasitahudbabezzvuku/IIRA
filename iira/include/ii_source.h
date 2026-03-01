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

typedef struct SourceManager SourceManager;

enum DiagnosticLevel { DIAG_LEVEL_ERROR, DIAG_LEVEL_WARNING, DIAG_LEVEL_NOTE };

SourceManager* ii_src_manager_new(const char* file_path) _nodiscard_;
void ii_src_manager_free(SourceManager*);
void ii_src_manager_freep(SourceManager**);
#define _autosrc_ _cleanup_(ii_src_manager_freep)

const char* ii_src_get_buffer(const SourceManager*) _nodiscard_;
size_t ii_src_get_size(const SourceManager*) _nodiscard_;

void ii_src_add_newline(SourceManager*, uint32_t offset);
SourceLocation ii_src_resolve_location(const SourceManager*, uint32_t offset) _nodiscard_;
const char* ii_src_resolve_line_bounds(const SourceManager*, uint32_t line, size_t* out_length) _nodiscard_;
