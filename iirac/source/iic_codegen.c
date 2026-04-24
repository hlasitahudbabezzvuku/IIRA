/**
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "iic_codegen.h"
#include "ii_ast_iter.h"
#include "ii_trace.h"
#include "iic_codegen_internal.h"

#include <stdio.h>

CodegenContext* iic_gen_context_new(Ast* ast, FILE* output, DiagnosticContext* diag, TraceContext* trace)
{
    CodegenContext* context = uf_mem_zalloc(sizeof(CodegenContext));

    context->ast = ast;
    context->output = output;
    context->diag = diag;
    context->trace = trace;
    context->source = ii_ast_get_source(ast);

    /* Initialize arena for temporary allocations */
    context->arena = uf_mem_region_new(4096);

    /* Initialize counters */
    context->label_counter = 0;
    context->temp_counter = 0;
    context->string_counter = 0;

    /* Initialize current function context */
    context->current_func = nullptr;
    context->current_frame_size = 0;
    context->next_slot = 0;

    /* Initialize string literal collection */
    context->string_literals = nullptr;
    context->string_count = 0;

    TRACE_SCOPE(context->trace);

    /* Emit type definitions for all blueprints */
    ast_foreach_blueprints(context->ast, bp)
    {
        _emit_type_def(context, bp);
        putc('\n', context->output);
    }
    ast_foreach_end;

    /* Emit all top-level functions */
    ast_foreach_funcs(context->ast, func)
    {
        if (func->body == nullptr) {
            continue;
        }
        _emit_function(context, func);
    }
    ast_foreach_end;

    /* Emit all methods from blueprints */
    ast_foreach_blueprints(context->ast, bp)
    {
        if (bp->flat_methods != nullptr) {
            ast_foreach_flat_methods(bp, method)
            {
                _emit_method(context, bp, method);
            }
            ast_foreach_end;
        }
    }
    ast_foreach_end;

    /* Emit data section (string literals) - must be after functions to collect all strings */
    _emit_data_section(context);

    return context;
}

void iic_gen_context_free(CodegenContext* context)
{
    if (!context) {
        return;
    }

    /* Free arena */
    if (context->arena != nullptr) {
        uf_mem_region_free(context->arena);
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
