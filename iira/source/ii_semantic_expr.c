/**
 * @brief Expression analysis for semantic analysis.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast_iter.h"
#include "ii_semantic.h"
#include "ii_semantic_internal.h"

#include <string.h>

/*
 * Loop context (break/continue validation).
 */

void _enter_loop(SemanticContext* context, AstNode* node)
{
    LoopContext* loop = uf_mem_region_zalloc(context->symbol_arena, sizeof(LoopContext));
    loop->node = node;
    loop->next = context->loop_stack;
    context->loop_stack = loop;
}

void _exit_loop(SemanticContext* context)
{
    if (context->loop_stack == nullptr) {
        return;
    }
    context->loop_stack = context->loop_stack->next;
}

bool _in_loop(const SemanticContext* context)
{
    return context->loop_stack != nullptr;
}

