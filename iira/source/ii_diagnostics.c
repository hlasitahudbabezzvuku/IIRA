/**
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_diagnostics.h"
#include "ii_source_manager.h"
#include "uf_containers.h"
#include "uf_logger.h"
#include "uf_memory.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define ARENA_BLOCK_SIZE (4 * 1024) /* 4 kilobytes blocks for strings */

struct Diagnostic {
    enum UfLogLevel level;
    SourceSpan span;
    const char* message;
};

struct DiagnosticsManager {
    SourceManager* source_manager;
    UfConVector* diagnostics;
    UfMemRegion* messages;
};

static const char* _map_level_x_string[] = {
    "debug", "info", "warning", "error", "panic",
};

static const int _map_level_x_color[] = {
    UF_COLOR_GREEN, UF_COLOR_BLUE, UF_COLOR_YELLOW, UF_COLOR_RED, UF_COLOR_RED,
};

#define _get_output_stream(level) ((level >= UF_LOG_ERROR) ? stdout : stderr)

DiagnosticsManager* ii_diag_engine_new(SourceManager* source_manager)
{
    DiagnosticsManager* diagnostic_engine = uf_mem_zalloc(sizeof(DiagnosticsManager));
    diagnostic_engine->source_manager = source_manager;
    diagnostic_engine->diagnostics = uf_con_vector_new(sizeof(struct Diagnostic));
    diagnostic_engine->messages = uf_mem_region_new(ARENA_BLOCK_SIZE);

    return diagnostic_engine;
}

void ii_diag_engine_free(DiagnosticsManager* engine)
{
    if (engine == nullptr) {
        return;
    }

    uf_con_vector_free(engine->diagnostics);
    uf_mem_region_free(engine->messages);
    uf_mem_free(engine);
}

void ii_diag_engine_freep(DiagnosticsManager** engine_ptr)
{
    if (engine_ptr && *engine_ptr) {
        ii_diag_engine_free(*engine_ptr);
        *engine_ptr = nullptr;
    }
}

void ii_diag_report(DiagnosticsManager* engine, enum UfLogLevel level, SourceSpan span, const char* format,
                    ...)
{
    va_list args, args_copy;
    va_start(args, format);
    va_copy(args_copy, args);
    int length = vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);

    char* formatted_message = nullptr;
    if _likely_ (length >= 0) {
        formatted_message = uf_mem_region_alloc(engine->messages, (size_t)length + 1);
        vsnprintf(formatted_message, (size_t)length + 1, format, args);
    }
    va_end(args);

    struct Diagnostic diagnostic = {
        .level = level,
        .span = span,
        .message = formatted_message,
    };

    uf_con_vector_push(engine->diagnostics, &diagnostic);
}

void ii_diag_output(DiagnosticsManager* engine)
{
    size_t count = uf_con_vector_length(engine->diagnostics);

    for (size_t i = 0; i < count; ++i) {
        const struct Diagnostic* diagnostic = uf_con_vector_get(engine->diagnostics, i);

        if (diagnostic->level < uf_log_get_level()) {
            return;
        }

        /* Level with the message. */
        fprintf(_get_output_stream(diagnostic->level),
                "[\033[%imiirac:\033[%im%s\033[%im] \033[%im%s\033[%im\n\n", UF_COLOR_BLACK_LIGHT,
                _map_level_x_color[diagnostic->level], _map_level_x_string[diagnostic->level], UF_COLOR_RESET,
                _map_level_x_color[diagnostic->level], diagnostic->message, UF_COLOR_RESET);

        SourceLocation location = ii_src_resolve_location(engine->source_manager, diagnostic->span.offset);

        /* File path and location. */
        fprintf(_get_output_stream(diagnostic->level), "    \033[%imat:\033[%im %s\033[%im:%u:%u\n",
                UF_COLOR_BLACK_LIGHT, UF_COLOR_WHITE_LIGHT, ii_src_get_file_path(engine->source_manager),
                UF_COLOR_RESET, location.line, location.column);

        size_t line_length = 0;
        const char* line_ptr = nullptr;

        /* Previous line */
        line_ptr = ii_src_resolve_line_bounds(engine->source_manager, location.line - 1, &line_length);
        if (line_ptr) {
            fprintf(_get_output_stream(diagnostic->level), "%4u ┆ %.*s\n", location.line - 1,
                    (int)line_length, line_ptr);
        }

        /* Line with error */
        line_ptr = ii_src_resolve_line_bounds(engine->source_manager, location.line, &line_length);
        if (line_ptr) {
            fprintf(_get_output_stream(diagnostic->level), "%4u ┆ %.*s\n     ┆ ", location.line,
                    (int)line_length, line_ptr);

            /* Replicate source whitespace to for proper alignment. */
            for (uint32_t i = 0; i < location.column - 1; i++) {
                if (line_ptr[i] == '\t') {
                    fputc('\t', _get_output_stream(diagnostic->level));
                    continue;
                }
                fputc(' ', _get_output_stream(diagnostic->level));
            }

            fprintf(_get_output_stream(diagnostic->level), "\033[%im^",
                    _map_level_x_color[diagnostic->level]);
            for (uint32_t c = 1; c < diagnostic->span.length; c++) {
                fputc('~', _get_output_stream(diagnostic->level));
            }
            fprintf(_get_output_stream(diagnostic->level), "\033[%im\n", UF_COLOR_RESET);
        }

        /* Next line */
        line_ptr = ii_src_resolve_line_bounds(engine->source_manager, location.line + 1, &line_length);
        if (line_ptr) {
            fprintf(_get_output_stream(diagnostic->level), "%4u ┆ %.*s\n", location.line + 1,
                    (int)line_length, line_ptr);
        }

        putc('\n', _get_output_stream(diagnostic->level));
    }
}
