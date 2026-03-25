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

/*
 * Literal analysis.
 */

TastExpr* _analyze_literal(SemanticContext* context, AstLiteral* lit)
{
    TRACE_SCOPE(context->trace);

    TastExpr* tast = ii_tast_expr_new(context->ast);

    enum LexerPrimitiveType prim;
    AstType* result_type = nullptr;
    switch (lit->variant) {
    case LITERAL_INT:
        prim = LEXER_PRIM_INT;
        tast->const_int_value = (int32_t)lit->literal.int_value;
        break;
    case LITERAL_FLOAT:
        prim = LEXER_PRIM_FLOAT;
        break;
    case LITERAL_STRING: {
        prim = LEXER_PRIM_CHAR;
        AstType* char_type = ii_ast_type_primitive(context->ast, LEXER_PRIM_CHAR);
        result_type = ii_ast_type_array(context->ast, char_type, nullptr);
        break;
    }
    case LITERAL_NULL: {
        prim = LEXER_PRIM_VOID;
        result_type = ii_ast_type_pointer(context->ast, ii_ast_type_primitive(context->ast, LEXER_PRIM_VOID));
        break;
    }
    case LITERAL_BOOL:
        prim = LEXER_PRIM_BOOL;
        break;
    case LITERAL_CHAR:
        prim = LEXER_PRIM_CHAR;
        break;
    default:
        prim = LEXER_PRIM_VOID;
        break;
    }

    if (result_type == nullptr) {
        result_type = ii_ast_type_primitive(context->ast, prim);
    }
    tast->resolved_type = result_type;
    lit->expr.tast = tast;

    return tast;
}

/*
 * Identifier analysis.
 */

TastExpr* _analyze_ident(SemanticContext* context, AstIdent* ident)
{
    TRACE_SCOPE(context->trace);

    Symbol* symbol = _scope_lookup_in_chain(context->current_scope, ident->name);
    if (symbol == nullptr) {
        _diag_error(context, ident->expr.base.span, "Undefined variable '%s'", ident->name);
        return nullptr;
    }

    switch (symbol->kind) {
    case SYMBOL_KIND_VAR:
        ident->resolved_kind = AST_IDENT_VAR;
        ident->resolved.var_decl = symbol->decl;
        break;
    case SYMBOL_KIND_PARAM:
        ident->resolved_kind = AST_IDENT_PARAM;
        ident->resolved.param_decl = symbol->decl;
        break;
    case SYMBOL_KIND_FUNC:
        ident->resolved_kind = AST_IDENT_FUNC;
        ident->resolved.func_decl = symbol->decl;
        break;
    case SYMBOL_KIND_BLUEPRINT:
        ident->resolved_kind = AST_IDENT_BLUEPRINT;
        ident->resolved.blueprint_decl = symbol->decl;
        break;
    case SYMBOL_KIND_FIELD:
        ident->resolved_kind = AST_IDENT_FIELD;
        ident->resolved.field_decl = symbol->decl;
        break;
    default:
        ident->resolved_kind = AST_IDENT_NONE;
        break;
    }

    TastExpr* tast = ii_tast_expr_new(context->ast);
    tast->resolved_type = symbol->type;
    if (symbol->type != nullptr) {
        _resolve_type(context, symbol->type);
    }
    tast->is_lvalue = (symbol->kind == SYMBOL_KIND_VAR || symbol->kind == SYMBOL_KIND_FIELD);
    ident->expr.tast = tast;

    return tast;
}

/*
 * Binary expression analysis.
 */

TastExpr* _analyze_binary(SemanticContext* context, AstBinary* bin)
{
    TRACE_SCOPE(context->trace);

    TastExpr* left_tast = _analyze_expr(context, bin->left);
    TastExpr* right_tast = _analyze_expr(context, bin->right);

    if (left_tast == nullptr || right_tast == nullptr) {
        return nullptr;
    }

    AstType* result_type = nullptr;

    switch (bin->op) {
    case LEXER_TOK_PLUS:
    case LEXER_TOK_MINUS:
    case LEXER_TOK_STAR:
    case LEXER_TOK_SLASH:
    case LEXER_TOK_PERCENT: {
        if (!_is_numeric_type(left_tast->resolved_type) || !_is_numeric_type(right_tast->resolved_type)) {
            _diag_error(context, bin->expr.base.span, "Arithmetic operators require numeric operands");
            return nullptr;
        }
        if (!_types_match(context, left_tast->resolved_type, right_tast->resolved_type)) {
            _diag_error(context, bin->expr.base.span, "Type mismatch: operands must have the same type");
            return nullptr;
        }
        result_type = left_tast->resolved_type;
        break;
    }

    case LEXER_TOK_LT:
    case LEXER_TOK_GT:
    case LEXER_TOK_LTE:
    case LEXER_TOK_GTE: {
        if (!_is_numeric_type(left_tast->resolved_type) || !_is_numeric_type(right_tast->resolved_type)) {
            _diag_error(context, bin->expr.base.span, "Comparison operators require numeric operands");
            return nullptr;
        }
        if (!_types_match(context, left_tast->resolved_type, right_tast->resolved_type)) {
            bool left_is_int = _is_int_type(left_tast->resolved_type);
            bool right_is_int = _is_int_type(right_tast->resolved_type);
            bool left_is_float = _is_float_type(left_tast->resolved_type);
            bool right_is_float = _is_float_type(right_tast->resolved_type);
            bool allowed = (left_is_int && right_is_float) || (left_is_float && right_is_int);
            if (!allowed) {
                _diag_error(context, bin->expr.base.span, "Type mismatch: operands must have the same type");
                return nullptr;
            }
        }
        result_type = ii_ast_type_primitive(context->ast, LEXER_PRIM_BOOL);
        break;
    }

    case LEXER_TOK_EQ:
    case LEXER_TOK_NEQ: {
        bool allowed = _can_coerce_for_equality(context, left_tast->resolved_type, right_tast->resolved_type,
                                                bin->left, bin->right);
        if (!allowed) {
            _diag_error(context, bin->expr.base.span,
                        "Type mismatch: operands must have the same type for equality");
            return nullptr;
        }
        result_type = ii_ast_type_primitive(context->ast, LEXER_PRIM_BOOL);
        break;
    }

    case LEXER_TOK_AND:
    case LEXER_TOK_OR: {
        if (!_is_bool_type(left_tast->resolved_type) || !_is_bool_type(right_tast->resolved_type)) {
            _diag_error(context, bin->expr.base.span, "Logical operators require boolean operands");
            return nullptr;
        }
        result_type = ii_ast_type_primitive(context->ast, LEXER_PRIM_BOOL);
        break;
    }

    case LEXER_TOK_BIT_AND:
    case LEXER_TOK_BIT_OR:
    case LEXER_TOK_BIT_XOR: {
        if (!_is_integer_type(left_tast->resolved_type) || !_is_integer_type(right_tast->resolved_type)) {
            _diag_error(context, bin->expr.base.span, "Bitwise operators require integer operands");
            return nullptr;
        }
        if (!_types_match(context, left_tast->resolved_type, right_tast->resolved_type)) {
            _diag_error(context, bin->expr.base.span, "Type mismatch: operands must have the same type");
            return nullptr;
        }
        result_type = left_tast->resolved_type;
        break;
    }

    case LEXER_TOK_ASSIGN:
    case LEXER_TOK_PLUS_ASSIGN:
    case LEXER_TOK_MINUS_ASSIGN:
    case LEXER_TOK_STAR_ASSIGN:
    case LEXER_TOK_SLASH_ASSIGN:
        return nullptr;

    default:
        _diag_error(context, bin->expr.base.span, "Unknown binary operator");
        return nullptr;
    }

    TastExpr* tast = ii_tast_expr_new(context->ast);
    tast->resolved_type = result_type;
    tast->is_constant = left_tast->is_constant && right_tast->is_constant;
    bin->expr.tast = tast;

    return tast;
}

