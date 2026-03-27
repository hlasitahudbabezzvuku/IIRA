/**
 * @brief Statement analysis for semantic analysis.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast_iter.h"
#include "ii_semantic_internal.h"

/*
 * Statement dispatcher. Route to specific analyzer based on AST kind.
 */

void _analyze_stmt(SemanticContext* context, AstStmt* stmt)
{
    TRACE_SCOPE(context->trace);

    if (stmt == nullptr) {
        return;
    }

    switch (stmt->kind) {
    case AST_KIND_BLOCK:
        _analyze_block(context, &stmt->block);
        break;
    case AST_KIND_DECL:
        _analyze_decl_stmt(context, &stmt->decl);
        break;
    case AST_KIND_RETURN:
        _analyze_return(context, &stmt->ret, context->current_return_type);
        break;
    case AST_KIND_IF:
        _analyze_if(context, &stmt->if_stmt);
        break;
    case AST_KIND_FOR:
        _analyze_for(context, &stmt->for_stmt);
        break;
    case AST_KIND_WHILE:
        _analyze_while(context, &stmt->while_stmt);
        break;
    case AST_KIND_DO_WHILE:
        _analyze_do_while(context, &stmt->do_while);
        break;
    case AST_KIND_BREAK:
        _analyze_break(context, &stmt->break_stmt);
        break;
    case AST_KIND_CONTINUE:
        _analyze_continue(context, &stmt->continue_stmt);
        break;
    case AST_KIND_EXPR_STMT:
        _analyze_expr_stmt(context, &stmt->expr_stmt);
        break;
    default:
        break;
    }
}

/*
 * Block creates new scope for local variable declarations.
 *
 * Strategy: push new scope, analyze all statements, then pop scope.
 * All variables declared within block are released when scope exits.
 */

void _analyze_block(SemanticContext* context, AstBlock* block)
{
    TRACE_SCOPE(context->trace);

    if (block == nullptr || block->stmts == nullptr) {
        return;
    }

    Scope* block_scope = _scope_new(context, context->current_scope, "block");
    _scope_push(context, block_scope);

    ast_foreach_stmts(block, stmt)
    {
        _analyze_stmt(context, stmt);
    }
    ast_foreach_end;

    _scope_pop(context);
}

/*
 * Variable declaration with optional initializer.
 *
 * Coercion rules:
 *   - Integer 0 -> pointer (null)
 *   - Integer 0 -> float/double
 *   - Integer 0 -> single-field blueprint (unwrap to field)
 *
 * Strategy: analyze initializer first, then check type compatibility.
 * Allow coercion only for integer literal 0, not arbitrary expressions.
 */

void _analyze_decl_stmt(SemanticContext* context, AstDecl* decl)
{
    TRACE_SCOPE(context->trace);

    if (decl == nullptr) {
        return;
    }

    if (decl->type != nullptr) {
        _resolve_type(context, decl->type);

        if (decl->type->tag == AST_TYPE_KIND_ARRAY) {
            AstExpr* size_expr = ii_ast_type_get_array_size(decl->type);
            if (size_expr == nullptr && decl->init == nullptr) {
                _diag_error(context, decl->type->base.span, "Array declaration requires explicit size");
            }
            if (size_expr == nullptr && decl->init != nullptr && decl->init->base.kind == AST_KIND_INIT) {
                AstInit* init = (AstInit*)decl->init;
                if (init->values != nullptr && uf_con_vector_length(init->values) > 0) {
                    size_t elem_count = uf_con_vector_length(init->values);
                    TastType* elem_tast = nullptr;
                    AstType* elem_type = ii_ast_type_get_array_element(decl->type);
                    if (elem_type != nullptr && elem_type->tast != nullptr) {
                        elem_tast = elem_type->tast;
                    }
                    uint32_t elem_size = elem_tast != nullptr ? elem_tast->size : 4;
                    decl->type->tast->size = (uint32_t)(elem_count * elem_size);
                }
            }
        }
    }

    if (decl->init != nullptr) {
        if (decl->init->base.kind == AST_KIND_INIT) {
            ((AstInit*)decl->init)->target_type = decl->type;
        }
        TastExpr* init_tast = _analyze_expr(context, decl->init);
        if (init_tast != nullptr && decl->type != nullptr) {
            if (!_types_match(context, decl->type, init_tast->resolved_type)) {
                bool allow_coercion =
                    _can_coerce_expr(context, decl->type, init_tast->resolved_type, decl->init);
                if (!allow_coercion) {
                    _diag_error(context, decl->base.span, "Type mismatch: expected '%s' but got '%s'",
                                _type_name(context, decl->type),
                                _type_name(context, init_tast->resolved_type));
                }
            }
        }
    }

    /* Assign slot index for codegen */
    decl->slot_index = context->next_slot++;
    if (context->max_slot < context->next_slot) {
        context->max_slot = context->next_slot;
    }

    _scope_insert(context, context->current_scope, decl->name, SYMBOL_KIND_VAR, decl, decl->type,
                  decl->base.span);
}

/*
 * Return statement validation.
 *
 * Expected return type may be nullptr when analyzing return as standalone statement.
 * In that case, skip type checking (not in function context).
 */

void _analyze_return(SemanticContext* context, AstReturn* ret, AstType* expected)
{
    TRACE_SCOPE(context->trace);

    if (ret == nullptr) {
        return;
    }

    if (ret->value == nullptr) {
    } else {
        if (expected != nullptr && ret->value->base.kind == AST_KIND_INIT) {
            ((AstInit*)ret->value)->target_type = expected;
        }
        TastExpr* value_tast = _analyze_expr(context, ret->value);
        if (value_tast != nullptr && expected != nullptr) {
            if (!_types_match(context, expected, value_tast->resolved_type)) {
                if (!_can_coerce_expr(context, expected, value_tast->resolved_type, ret->value)) {
                    _diag_error(context, ret->base.span, "Return type mismatch");
                }
            }
        }
    }
}

/*
 * If statement: condition must be bool.
 *
 * Handle else-if chains by recursive call on else if it's another if.
 */

void _analyze_if(SemanticContext* context, AstIf* if_stmt)
{
    TRACE_SCOPE(context->trace);

    if (if_stmt == nullptr) {
        return;
    }

    if (if_stmt->condition != nullptr) {
        TastExpr* cond_tast = _analyze_expr(context, if_stmt->condition);
        if (cond_tast != nullptr && cond_tast->resolved_type != nullptr) {
            if (!_is_valid_condition_type(cond_tast->resolved_type, if_stmt->condition)) {
                _diag_error(context, if_stmt->condition->base.span, "If condition must be bool type");
            }
        }
    }

    if (if_stmt->then_block != nullptr) {
        _analyze_block(context, if_stmt->then_block);
    }

    if (if_stmt->else_stmt != nullptr) {
        if (if_stmt->else_stmt->kind == AST_KIND_BLOCK) {
            _analyze_block(context, &if_stmt->else_stmt->block);
        } else if (if_stmt->else_stmt->kind == AST_KIND_IF) {
            _analyze_if(context, &if_stmt->else_stmt->if_stmt);
        }
    }
}

/*
 * For loop: init -> condition -> iter -> body.
 *
 * Enter loop context before body analysis for break/continue validation.
 */

void _analyze_for(SemanticContext* context, AstFor* for_stmt)
{
    TRACE_SCOPE(context->trace);

    if (for_stmt == nullptr) {
        return;
    }

    if (for_stmt->init != nullptr) {
        _analyze_stmt(context, for_stmt->init);
    }

    if (for_stmt->condition != nullptr) {
        TastExpr* cond_tast = _analyze_expr(context, for_stmt->condition);
        if (cond_tast != nullptr && cond_tast->resolved_type != nullptr) {
            if (!_is_valid_condition_type(cond_tast->resolved_type, for_stmt->condition)) {
                _diag_error(context, for_stmt->condition->base.span, "For condition must be bool type");
            }
        }
    }

    if (for_stmt->iter != nullptr) {
        _analyze_expr(context, for_stmt->iter);
    }

    if (for_stmt->body != nullptr) {
        _enter_loop(context, (AstNode*)for_stmt);
        _analyze_block(context, for_stmt->body);
        _exit_loop(context);
    }
}

/*
 * While loop: condition -> body.
 *
 * Enter loop context before body analysis for break/continue validation.
 */

void _analyze_while(SemanticContext* context, AstWhile* while_stmt)
{
    TRACE_SCOPE(context->trace);

    if (while_stmt == nullptr) {
        return;
    }

    if (while_stmt->condition != nullptr) {
        TastExpr* cond_tast = _analyze_expr(context, while_stmt->condition);
        if (cond_tast != nullptr && cond_tast->resolved_type != nullptr) {
            if (!_is_valid_condition_type(cond_tast->resolved_type, while_stmt->condition)) {
                _diag_error(context, while_stmt->condition->base.span, "While condition must be bool type");
            }
        }
    }

    if (while_stmt->body != nullptr) {
        _enter_loop(context, (AstNode*)while_stmt);
        _analyze_block(context, while_stmt->body);
        _exit_loop(context);
    }
}

/*
 * Do-while loop: body -> condition.
 *
 * Loop context entered before body so break/continue work.
 */

void _analyze_do_while(SemanticContext* context, AstDoWhile* do_while)
{
    TRACE_SCOPE(context->trace);

    if (do_while == nullptr) {
        return;
    }

    _enter_loop(context, (AstNode*)do_while);

    if (do_while->body != nullptr) {
        _analyze_block(context, do_while->body);
    }

    _exit_loop(context);

    if (do_while->condition != nullptr) {
        TastExpr* cond_tast = _analyze_expr(context, do_while->condition);
        if (cond_tast != nullptr && cond_tast->resolved_type != nullptr) {
            if (!_is_valid_condition_type(cond_tast->resolved_type, do_while->condition)) {
                _diag_error(context, do_while->condition->base.span, "Do-while condition must be bool type");
            }
        }
    }
}

/*
 * Break statement: only valid inside loop context.
 */

void _analyze_break(SemanticContext* context, AstBreak* brk)
{
    TRACE_SCOPE(context->trace);

    (void)brk;

    if (!_in_loop(context)) {
        _diag_error(context, (SourceSpan){0}, "'break' statement outside of loop");
    }
}

/*
 * Continue statement: only valid inside loop context.
 */

void _analyze_continue(SemanticContext* context, AstContinue* cont)
{
    TRACE_SCOPE(context->trace);

    (void)cont;

    if (!_in_loop(context)) {
        _diag_error(context, (SourceSpan){0}, "'continue' statement outside of loop");
    }
}

/*
 * Expression statement: analyze expression for side effects.
 *
 * Result type is discarded - statements produce no value.
 */

void _analyze_expr_stmt(SemanticContext* context, AstExprStmt* expr_stmt)
{
    TRACE_SCOPE(context->trace);

    if (expr_stmt == nullptr || expr_stmt->expr == nullptr) {
        return;
    }

    _analyze_expr(context, expr_stmt->expr);
}
