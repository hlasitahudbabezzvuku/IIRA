/**
 * @brief A lightweight tracing module for debugging parser and semantic analyzer.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_trace.h"

#include "uf_logger.h"
#include "uf_memory.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_DEPTH 100

struct TraceContext {
    FILE* output;
    bool enabled;
    uint32_t depth;
};

TraceContext* ii_trace_context_new(FILE* output)
{
    TraceContext* context = uf_mem_zalloc(sizeof(TraceContext));
    context->output = output;
    context->enabled = false;
    context->depth = 0;

    return context;
}

void ii_trace_context_free(TraceContext* context)
{
    uf_mem_free(context);
}

void ii_trace_context_freep(TraceContext** context_ptr)
{
    if _likely_ (context_ptr && *context_ptr) {
        ii_trace_context_free(*context_ptr);
        *context_ptr = nullptr;
    }
}

void ii_trace_set_enabled(TraceContext* context, bool enabled)
{
    context->enabled = enabled;
}

uint32_t ii_trace_get_depth(const TraceContext* context)
{
    return context ? context->depth : 0;
}

static void _print_indent(TraceContext* context, FILE* output)
{
    for (uint32_t i = 0; i < context->depth && i < MAX_DEPTH; i++) {
        fputs("  ", output);
    }
}

TraceScope _ii_trace_scope_enter(TraceContext* context, const char* func, const char* file, int line)
{
    TraceScope scope = {
        .context = context,
        .func = func,
        .file = file,
        .line = line,
        .depth = context ? context->depth : 0,
    };

    if (context == nullptr) {
        return scope;
    }

    if _likely_ (context->enabled && context->depth < MAX_DEPTH) {
        _print_indent(context, context->output);
        fprintf(context->output, "\033[%im>>\033[%im \033[%im%s\033[%im \033[%im(%s:%d)\033[%im\n",
                UF_COLOR_GREEN, UF_COLOR_RESET, UF_COLOR_CYAN, func, UF_COLOR_RESET, UF_COLOR_WHITE_LIGHT,
                file, line, UF_COLOR_RESET);
        fflush(context->output);
        context->depth++;
    }

    return scope;
}

TraceScope _ii_trace_scope_enter_message(TraceContext* context, const char* func, const char* file, int line,
                                         const char* format, ...)
{
    TraceScope scope = {
        .context = context,
        .func = func,
        .file = file,
        .line = line,
        .depth = context ? context->depth : 0,
    };

    if (context == nullptr) {
        return scope;
    }

    if _likely_ (context->enabled && context->depth < MAX_DEPTH) {
        _print_indent(context, context->output);
        fprintf(context->output, "\033[%im>>\033[%im \033[%im%s\033[%im(\033[%im", UF_COLOR_GREEN,
                UF_COLOR_RESET, UF_COLOR_CYAN, func, UF_COLOR_RESET, UF_COLOR_WHITE_LIGHT);

        va_list args;
        va_start(args, format);
        vfprintf(context->output, format, args);
        va_end(args);

        fprintf(context->output, "\033[%im) \033[%im(%s:%d)\033[%im\n", UF_COLOR_RESET, UF_COLOR_WHITE_LIGHT,
                file, line, UF_COLOR_RESET);
        context->depth++;
    }

    return scope;
}

void _ii_trace_scope_exit(TraceScope* scope)
{
    if (scope == nullptr || scope->context == nullptr) {
        return;
    }

    TraceContext* context = scope->context;

    if _likely_ (context->enabled && scope->depth < MAX_DEPTH) {
        context->depth--;
        _print_indent(context, context->output);
        fprintf(context->output, "\033[%im<<\033[%im \033[%im%s\033[%im\n", UF_COLOR_YELLOW, UF_COLOR_RESET,
                UF_COLOR_CYAN, scope->func, UF_COLOR_RESET);
        fflush(context->output);
    }
}

void _ii_trace_log(TraceContext* context, const char* file, int line, const char* format, ...)
{
    if (context == nullptr || !context->enabled) {
        return;
    }

    _print_indent(context, context->output);
    fprintf(context->output, "\033[%im::\033[%im ", UF_COLOR_WHITE_LIGHT, UF_COLOR_RESET);

    va_list args;
    va_start(args, format);
    vfprintf(context->output, format, args);
    va_end(args);

    fprintf(context->output, " \033[%im(%s:%d)\033[%im\n", UF_COLOR_WHITE_LIGHT, file, line, UF_COLOR_RESET);
}
