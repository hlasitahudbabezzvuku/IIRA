/**
 * @brief Semantic analyzer core module.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_semantic.h"
#include "ii_semantic_internal.h"
#include "ii_trace.h"
#include "uf_memory.h"

SemanticContext* ii_sem_context_new(Ast* ast, DiagnosticContext* diag, TraceContext* trace_context)
{
    SemanticContext* context = uf_mem_zalloc(sizeof(SemanticContext));
    context->ast = ast;
    context->src = ii_ast_get_source(ast);
    context->diag = diag;
    context->had_error = false;
    context->trace = trace_context;
    context->symbol_arena = uf_mem_region_new(4096);
    context->global_scope = _scope_new(context, nullptr, "global");
    context->current_scope = context->global_scope;

    TRACE_SCOPE(context->trace);

    if (_likely_(!context->had_error)) {
        TRACE_LOG(context->trace, "Phase 1: Declaration analysis");
        _analyze_declarations(context);
    }

    if (_likely_(!context->had_error)) {
        TRACE_LOG(context->trace, "Phase 2: Inheritance analysis");
        _resolve_inheritance(context);
    }

    if (_likely_(!context->had_error)) {
        TRACE_LOG(context->trace, "Phase 3: Type resolution");
        _resolve_all_types(context);
    }

    if (_likely_(!context->had_error)) {
        TRACE_LOG(context->trace, "Phase 4: Expression analysis");
        _analyze_function_bodies(context);
    }

    ii_ast_program_finalize(context->ast);
    ii_diag_output(context->diag);

    return context;
}

void ii_sem_context_free(SemanticContext* context)
{
    if (context == nullptr) {
        return;
    }

    uf_con_map_free(context->global_scope->symbols);
    uf_mem_region_free(context->symbol_arena);
    uf_mem_free(context);
}

void ii_sem_context_freep(SemanticContext** context_ptr)
{
    if (context_ptr == nullptr || *context_ptr == nullptr) {
        return;
    }

    ii_sem_context_free(*context_ptr);
    *context_ptr = nullptr;
}
