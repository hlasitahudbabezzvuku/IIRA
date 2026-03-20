/**
 * @brief Semantic analyzer core module.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_semantic_internal.h"

#include "ii_semantic.h"
#include "ii_trace.h"

SemanticContext* ii_sem_context_new(Ast* ast, DiagnosticContext* diag, TraceContext* trace_context)
{
    SemanticContext* context = uf_mem_zalloc(sizeof(SemanticContext));

    context->ast = ast;
    context->src = ii_ast_get_source(ast);
    context->diag = diag;
    context->had_error = false;
    context->trace = trace_context;

    TRACE_SCOPE(context->trace);

    /* TODO: run semantic analysis */

    ii_ast_program_finalize(context->ast);
    ii_diag_output(context->diag);

    return context;
}

void ii_sem_context_free(SemanticContext* context)
{
    if (!context) {
        return;
    }

    uf_mem_free(context);
}

void ii_sem_context_freep(SemanticContext** context_ptr)
{
    if (!context_ptr || !*context_ptr) {
        return;
    }

    ii_sem_context_free(*context_ptr);
    *context_ptr = NULL;
}
