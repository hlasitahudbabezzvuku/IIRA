/**
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "iic_codegen.h"
#include "ii_trace.h"
#include "iic_codegen_internal.h"

CodegenContext* iic_gen_context_new(Ast* ast, FILE* output, DiagnosticContext* diag, TraceContext* trace)
{
    CodegenContext* context = uf_mem_zalloc(sizeof(CodegenContext));

    context->ast = ast;
    context->diag = diag;
    context->trace = trace;

    TRACE_SCOPE(context->trace);

    /* TODO: add real code generation logic here :D. */

    return context;
}

void iic_gen_context_free(CodegenContext* context)
{
    if (!context) {
        return;
    }

    uf_mem_free(context);
}

void iic_gen_context_freep(CodegenContext** context_ptr)
{
    if (!context_ptr || !*context_ptr) {
        return;
    }

    iic_gen_context_free(*context_ptr);
    *context_ptr = nullptr;
}
