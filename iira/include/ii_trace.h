#pragma once

/**
 * @brief A lightweight tracing module for debugging parser and semantic analyzer.
 *
 * This module provides scope-based function entry/exit tracing with indentation, colors, and source location
 * information. It's designed for detecting infinite loops and understanding call flow when developing iira
 * modules (mainly parser and semantic analyzer).
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "uf_common.h"

#include <stdint.h>
#include <stdio.h>

typedef struct TraceContext TraceContext;

typedef struct TraceScope TraceScope;
struct TraceScope {
    TraceContext* context;
    const char* func;
    const char* file;
    int line;
    uint32_t depth;
};

TraceContext* ii_trace_context_new(FILE* output) _nodiscard_;
void ii_trace_context_free(TraceContext*);
void ii_trace_context_freep(TraceContext**);
#define _autotrace_ _cleanup_(ii_trace_context_freep)

void ii_trace_set_enabled(TraceContext*, bool enabled);
uint32_t ii_trace_get_depth(const TraceContext*);

#define TRACE_SCOPE(context)                                                                                 \
    TraceScope _trace_scope_ __attribute__((unused, cleanup(_ii_trace_scope_exit))) =                        \
        _ii_trace_scope_enter(context, __func__, __FILE__, __LINE__)

#define TRACE_SCOPE_MESSAGE(context, format, ...)                                                            \
    TraceScope _trace_scope_ __attribute__((unused, cleanup(_ii_trace_scope_exit))) =                        \
        _ii_trace_scope_enter_message(context, __func__, __FILE__, __LINE__, format, ##__VA_ARGS__)

void _ii_trace_log(TraceContext*, const char* file, int line, const char* format, ...)
    __attribute__((format(printf, 4, 5)));

#define TRACE_LOG(context, format, ...) _ii_trace_log(context, __FILE__, __LINE__, format, ##__VA_ARGS__)

TraceScope _ii_trace_scope_enter(TraceContext*, const char* func, const char* file, int line);
TraceScope _ii_trace_scope_enter_message(TraceContext*, const char* func, const char* file, int line,
                                         const char* format, ...) __attribute__((format(printf, 5, 6)));
void _ii_trace_scope_exit(TraceScope*);
