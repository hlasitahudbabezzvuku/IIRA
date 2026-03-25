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

/*
 * Function body analysis.
 */

static bool _block_has_return(AstBlock* block)
{
    if (block == nullptr || block->stmts == nullptr) {
        return false;
    }
    ast_foreach_stmts(block, stmt)
    {
        if (stmt->kind == AST_KIND_RETURN && stmt->ret.value != nullptr) {
            return true;
        }
        if (stmt->kind == AST_KIND_IF) {
            if (stmt->if_stmt.then_block != nullptr && _block_has_return(stmt->if_stmt.then_block)) {
                return true;
            }
            if (stmt->if_stmt.else_stmt != nullptr && stmt->if_stmt.else_stmt->kind == AST_KIND_BLOCK) {
                if (_block_has_return(&stmt->if_stmt.else_stmt->block)) {
                    return true;
                }
            }
        }
        if (stmt->kind == AST_KIND_FOR) {
            if (stmt->for_stmt.body != nullptr && _block_has_return(stmt->for_stmt.body)) {
                return true;
            }
        }
        if (stmt->kind == AST_KIND_WHILE) {
            if (stmt->while_stmt.body != nullptr && _block_has_return(stmt->while_stmt.body)) {
                return true;
            }
        }
        if (stmt->kind == AST_KIND_DO_WHILE) {
            if (stmt->do_while.body != nullptr && _block_has_return(stmt->do_while.body)) {
                return true;
            }
        }
    }
    ast_foreach_end;
    return false;
}

void _analyze_function_bodies(SemanticContext* context)
{
    TRACE_SCOPE(context->trace);

    ast_foreach_funcs(context->ast, func)
    {
        Scope* func_scope = _scope_new(context, context->current_scope, func->name);

        context->current_function = func;
        context->current_return_type = func->return_type;

        ast_foreach_params(func, param)
        {
            _scope_insert(context, func_scope, param->name, SYMBOL_KIND_PARAM, param, param->type,
                          param->base.span);
        }
        ast_foreach_end;

        _scope_push(context, func_scope);

        if (func->body != nullptr) {
            _analyze_block(context, func->body);

            if (func->return_type != nullptr && _is_primitive_type(func->return_type) &&
                ii_ast_type_get_primitive(func->return_type) != LEXER_PRIM_VOID) {
                if (!_block_has_return(func->body)) {
                    _diag_error(context, func->base.span,
                                "Missing return statement in non-void function '%s'", func->name);
                }
            }
        }

        _scope_pop(context);

        context->current_function = nullptr;
        context->current_return_type = nullptr;
    }
    ast_foreach_end;

    ast_foreach_blueprints(context->ast, bp)
    {
        if (bp->methods == nullptr) {
            continue;
        }

        context->current_blueprint = bp;

        ast_foreach_methods(bp, method)
        {
            if (method->overloads == nullptr) {
                continue;
            }

            ast_foreach_overloads(method, overload)
            {
                Scope* method_scope = _scope_new(context, context->current_scope, method->name);

                context->current_function = nullptr;
                context->current_return_type = overload->return_type;

                bool has_self_param = false;
                ast_foreach_params(overload, param)
                {
                    if (param->name == ii_src_intern_cstr(context->src, "self")) {
                        has_self_param = true;
                        if (param->type == nullptr) {
                            param->type = _make_self_type(context, bp);
                        }
                    }
                    _scope_insert(context, method_scope, param->name, SYMBOL_KIND_PARAM, param, param->type,
                                  param->base.span);
                }
                ast_foreach_end;

                if (!overload->is_static && !has_self_param) {
                    AstType* self_type = _make_self_type(context, bp);
                    _scope_insert(context, method_scope, "self", SYMBOL_KIND_SELF, nullptr, self_type,
                                  bp->base.span);
                }

                _scope_push(context, method_scope);

                if (overload->body != nullptr) {
                    _analyze_block(context, overload->body);
                }

                _scope_pop(context);

                context->current_return_type = nullptr;
            }
            ast_foreach_end;
        }
        ast_foreach_end;
    }
    ast_foreach_end;
}

/*
 * Expression dispatcher.
 */

TastExpr* _analyze_expr(SemanticContext* context, AstExpr* expr)
{
    TRACE_SCOPE(context->trace);

    if (expr == nullptr) {
        return nullptr;
    }

    switch (ii_ast_expr_get_kind(expr)) {
    case AST_KIND_LITERAL:
        return _analyze_literal(context, (AstLiteral*)expr);
    case AST_KIND_IDENT:
        return _analyze_ident(context, (AstIdent*)expr);
    case AST_KIND_BINARY:
        return _analyze_binary(context, (AstBinary*)expr);
    case AST_KIND_UNARY:
        return _analyze_unary(context, (AstUnary*)expr);
    case AST_KIND_CALL:
        return _analyze_call(context, (AstCall*)expr);
    case AST_KIND_MEMBER:
        return _analyze_member(context, (AstMember*)expr);
    case AST_KIND_INDEX:
        return _analyze_index(context, (AstIndex*)expr);
    case AST_KIND_INIT:
        return _analyze_init(context, (AstInit*)expr);
    case AST_KIND_CAST:
        return _analyze_cast(context, (AstCast*)expr);
    case AST_KIND_FFI:
        return _analyze_ffi(context, (AstFfi*)expr);
    default:
        return nullptr;
    }
}

