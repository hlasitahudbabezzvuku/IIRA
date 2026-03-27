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

        /* Initialize slot allocation for parameters (start at 0) */
        context->next_slot = 0;
        context->max_slot = 0;

        ast_foreach_params(func, param)
        {
            /* Assign slot index to parameter */
            param->slot_index = context->next_slot++;
            if (context->max_slot < context->next_slot) {
                context->max_slot = context->next_slot;
            }

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

        /* Compute frame size: round up max_slot*8 to 8-byte alignment */
        func->frame_size = (context->max_slot * 8 + 7) & ~7;

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

                /* Initialize slot allocation for parameters */
                context->next_slot = 0;
                context->max_slot = 0;

                bool has_self_param = false;
                ast_foreach_params(overload, param)
                {
                    if (param->name == ii_src_intern_cstr(context->src, "self")) {
                        has_self_param = true;
                        if (param->type == nullptr) {
                            param->type = _make_self_type(context, bp);
                        }
                    }
                    /* Assign slot index to parameter */
                    param->slot_index = context->next_slot++;
                    if (context->max_slot < context->next_slot) {
                        context->max_slot = context->next_slot;
                    }

                    _scope_insert(context, method_scope, param->name, SYMBOL_KIND_PARAM, param, param->type,
                                  param->base.span);
                }
                ast_foreach_end;

                if (!overload->is_static && !has_self_param) {
                    AstType* self_type = _make_self_type(context, bp);
                    /* Assign implicit self parameter a slot */
                    overload->self_slot = context->next_slot;
                    context->next_slot++;
                    if (context->max_slot < context->next_slot) {
                        context->max_slot = context->next_slot;
                    }
                    _scope_insert(context, method_scope, "self", SYMBOL_KIND_SELF, overload, self_type,
                                  bp->base.span);
                }

                _scope_push(context, method_scope);

                if (overload->body != nullptr) {
                    _analyze_block(context, overload->body);
                }

                /* Compute frame size: round up max_slot*8 to 8-byte alignment */
                overload->frame_size = (context->max_slot * 8 + 7) & ~7;

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
    case SYMBOL_KIND_SELF:
        ident->resolved_kind = AST_IDENT_SELF;
        ident->resolved.method_overload = symbol->decl;
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
    tast->is_lvalue = (symbol->kind == SYMBOL_KIND_VAR || symbol->kind == SYMBOL_KIND_FIELD ||
                       symbol->kind == SYMBOL_KIND_SELF);
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
    case LEXER_TOK_SLASH_ASSIGN: {
        if (left_tast == nullptr || !left_tast->is_lvalue) {
            _diag_error(context, bin->expr.base.span, "Assignment target must be an lvalue");
            return nullptr;
        }

        if (!_types_match(context, left_tast->resolved_type, right_tast->resolved_type)) {
            if (!_can_coerce_numeric(left_tast->resolved_type, right_tast->resolved_type)) {
                _diag_error(context, bin->expr.base.span, "Type mismatch in assignment");
                return nullptr;
            }
        }

        TastExpr* tast = ii_tast_expr_new(context->ast);
        tast->resolved_type = left_tast->resolved_type;
        tast->is_lvalue = false;
        tast->is_constant = false;
        bin->expr.tast = tast;

        return tast;
    }

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

/*
 * Unary expression analysis.
 */

TastExpr* _analyze_unary(SemanticContext* context, AstUnary* un)
{
    TRACE_SCOPE(context->trace);

    TastExpr* operand_tast = _analyze_expr(context, un->operand);
    if (operand_tast == nullptr) {
        return nullptr;
    }

    AstType* result_type = nullptr;
    bool is_lvalue = false;

    switch (un->op) {
    case LEXER_TOK_MINUS:
        if (!_is_numeric_type(operand_tast->resolved_type)) {
            _diag_error(context, un->expr.base.span, "Unary minus requires numeric operand");
            return nullptr;
        }
        result_type = operand_tast->resolved_type;
        break;

    case LEXER_TOK_PLUS:
        if (!_is_numeric_type(operand_tast->resolved_type)) {
            _diag_error(context, un->expr.base.span, "Unary plus requires numeric operand");
            return nullptr;
        }
        result_type = operand_tast->resolved_type;
        break;

    case LEXER_TOK_NOT:
        if (!_is_bool_type(operand_tast->resolved_type)) {
            _diag_error(context, un->expr.base.span, "Logical not requires boolean operand");
            return nullptr;
        }
        result_type = ii_ast_type_primitive(context->ast, LEXER_PRIM_BOOL);
        break;

    case LEXER_TOK_BIT_NOT:
        if (!_is_integer_type(operand_tast->resolved_type)) {
            _diag_error(context, un->expr.base.span, "Bitwise not requires integer operand");
            return nullptr;
        }
        result_type = operand_tast->resolved_type;
        break;

    case LEXER_TOK_STAR:
        if (operand_tast->resolved_type == nullptr || !_is_pointer_type(operand_tast->resolved_type)) {
            _diag_error(context, un->expr.base.span, "Dereference requires pointer operand");
            return nullptr;
        }
        result_type = ii_ast_type_get_pointed(operand_tast->resolved_type);
        is_lvalue = true;
        break;

    case LEXER_TOK_BIT_AND:
        if (!operand_tast->is_lvalue) {
            _diag_error(context, un->expr.base.span, "Address-of requires lvalue operand");
            return nullptr;
        }
        result_type = ii_ast_type_pointer(context->ast, operand_tast->resolved_type);
        break;

    default:
        _diag_error(context, un->expr.base.span, "Unknown unary operator");
        return nullptr;
    }

    TastExpr* tast = ii_tast_expr_new(context->ast);
    tast->resolved_type = result_type;
    tast->is_lvalue = is_lvalue;
    tast->is_constant = operand_tast->is_constant;
    un->expr.tast = tast;

    return tast;
}

/*
 * Call expression analysis.
 */

TastExpr* _analyze_call(SemanticContext* context, AstCall* call)
{
    TRACE_SCOPE(context->trace);

    TastExpr* callee_tast = _analyze_expr(context, call->callee);
    if (callee_tast == nullptr) {
        return nullptr;
    }

    size_t arg_count = call->args ? uf_con_vector_length(call->args) : 0;
    TastExpr** args_tast = nullptr;
    if (arg_count > 0) {
        args_tast = uf_mem_region_zalloc(context->symbol_arena, sizeof(TastExpr*) * arg_count);
        size_t i = 0;
        ast_foreach_call_args(call, arg)
        {
            args_tast[i] = _analyze_expr(context, arg);
            if (args_tast[i] == nullptr) {
                return nullptr;
            }
            i++;
        }
        ast_foreach_end;
    }

    AstMethodOverload* overload = nullptr;
    const char* func_name = nullptr;

    if (ii_ast_expr_get_kind(call->callee) == AST_KIND_MEMBER) {
        AstMember* member = (AstMember*)call->callee;
        if (member->resolved_method == nullptr) {
            _diag_error(context, call->expr.base.span, "Unknown method '%s'", member->member_name);
            return nullptr;
        }
        func_name = member->member_name;

        AstMethod* method = (AstMethod*)member->resolved_method;
        if (method->overloads != nullptr) {
            ast_foreach_overloads(method, ov)
            {
                size_t param_count = ov->params ? uf_con_vector_length(ov->params) : 0;
                bool is_static = ov->is_static;
                if (!is_static && param_count > 0) {
                    param_count--;
                }
                if (param_count != arg_count) {
                    continue;
                }
                bool match = true;
                size_t j = 0;
                ast_foreach_params(ov, param)
                {
                    if (!is_static && j == 0 && param->name == ii_src_intern_cstr(context->src, "self")) {
                        continue;
                    }
                    if (!_types_match(context, args_tast[j]->resolved_type, param->type)) {
                        match = false;
                        break;
                    }
                    j++;
                }
                ast_foreach_end;
                if (match) {
                    overload = ov;
                    break;
                }
            }
            ast_foreach_end;
        }

        if (overload == nullptr) {
            _diag_error(context, call->expr.base.span, "No matching overload for '%s'", func_name);
            return nullptr;
        }

        if (overload->body == nullptr) {
            _diag_error(context, call->expr.base.span, "Cannot call unimplemented method '%s'", func_name);
            return nullptr;
        }

        call->resolved_overload = overload;
    } else if (ii_ast_expr_get_kind(call->callee) == AST_KIND_IDENT) {
        AstIdent* ident = (AstIdent*)call->callee;
        if (ident->resolved_kind != AST_IDENT_FUNC || ident->resolved.func_decl == nullptr) {
            _diag_error(context, call->expr.base.span, "'%s' is not a function", ident->name);
            return nullptr;
        }
        func_name = ident->name;
        AstFuncDecl* func = ident->resolved.func_decl;

        size_t param_count = func->params ? uf_con_vector_length(func->params) : 0;
        if (param_count != arg_count) {
            _diag_error(context, call->expr.base.span, "Argument count mismatch for '%s'", func_name);
            return nullptr;
        }

        size_t i = 0;
        ast_foreach_params(func, param)
        {
            if (!_types_match(context, args_tast[i]->resolved_type, param->type)) {
                _diag_error(context, call->expr.base.span, "Argument type mismatch for '%s'", func_name);
                return nullptr;
            }
            i++;
        }
        ast_foreach_end;

        if (func->body == nullptr) {
            _diag_error(context, call->expr.base.span, "Cannot call unimplemented function '%s'", func_name);
            return nullptr;
        }
    } else {
        _diag_error(context, call->expr.base.span, "Cannot call non-function");
        return nullptr;
    }

    AstType* result_type = nullptr;
    if (ii_ast_expr_get_kind(call->callee) == AST_KIND_MEMBER && overload) {
        result_type = overload->return_type;
    } else if (ii_ast_expr_get_kind(call->callee) == AST_KIND_IDENT) {
        AstIdent* ident = (AstIdent*)call->callee;
        result_type = ident->resolved.func_decl->return_type;
    }

    TastExpr* tast = ii_tast_expr_new(context->ast);
    tast->resolved_type = result_type;
    call->expr.tast = tast;

    return tast;
}

/*
 * Member access analysis.
 */

TastExpr* _analyze_member(SemanticContext* context, AstMember* member)
{
    TRACE_SCOPE(context->trace);

    TastExpr* object_tast = _analyze_expr(context, member->object);
    if (object_tast == nullptr) {
        return nullptr;
    }

    AstType* object_type = object_tast->resolved_type;
    if (object_type == nullptr) {
        return nullptr;
    }

    if (_is_pointer_type(object_type)) {
        object_type = ii_ast_type_get_pointed(object_type);
    }

    if (object_type == nullptr || !_is_blueprint_type(object_type)) {
        _diag_error(context, member->expr.base.span, "Cannot access member of non-blueprint type");
        return nullptr;
    }

    AstBlueprintDecl* bp = ii_ast_type_get_blueprint_resolved(object_type);
    if (bp == nullptr) {
        _diag_error(context, member->expr.base.span, "Unknown blueprint type");
        return nullptr;
    }

    AstType* result_type = nullptr;
    bool is_lvalue = false;

    ast_foreach_flat_fields(bp, field)
    {
        if (field->name == member->member_name) {
            member->resolved_field = field;
            member->is_method_call = false;
            result_type = field->type;
            is_lvalue = true;
            goto found_member;
        }
    }
    ast_foreach_end;

    ast_foreach_flat_methods(bp, method)
    {
        if (method->name == member->member_name) {
            AstMethodOverload* first_overload = *(AstMethodOverload**)uf_con_vector_get(method->overloads, 0);
            member->resolved_method = method;
            member->is_method_call = true;
            result_type = first_overload->return_type;
            is_lvalue = false;
            goto found_member;
        }
    }
    ast_foreach_end;

    _diag_error(context, member->expr.base.span, "Unknown member '%s'", member->member_name);
    return nullptr;

found_member:
    TastExpr* tast = ii_tast_expr_new(context->ast);
    tast->resolved_type = result_type;
    tast->is_lvalue = is_lvalue;
    member->expr.tast = tast;

    return tast;
}

/*
 * Index expression analysis.
 */

TastExpr* _analyze_index(SemanticContext* context, AstIndex* idx)
{
    TRACE_SCOPE(context->trace);

    TastExpr* array_tast = _analyze_expr(context, idx->array);
    if (array_tast == nullptr) {
        return nullptr;
    }

    TastExpr* index_tast = _analyze_expr(context, idx->index);
    if (index_tast == nullptr) {
        return nullptr;
    }

    if (array_tast->resolved_type == nullptr || !_is_array_type(array_tast->resolved_type)) {
        _diag_error(context, idx->expr.base.span, "Cannot index non-array type");
        return nullptr;
    }

    if (index_tast->resolved_type == nullptr || !_is_primitive_type(index_tast->resolved_type) ||
        !_is_integer_type(index_tast->resolved_type)) {
        _diag_error(context, idx->expr.base.span, "Array index must be integer type");
        return nullptr;
    }

    AstType* result_type = ii_ast_type_get_array_element(array_tast->resolved_type);

    TastExpr* tast = ii_tast_expr_new(context->ast);
    tast->resolved_type = result_type;
    tast->is_lvalue = true;
    idx->expr.tast = tast;

    return tast;
}

/*
 * Initialization expression analysis.
 */

TastExpr* _analyze_init(SemanticContext* context, AstInit* init)
{
    TRACE_SCOPE(context->trace);

    if (init->target_type == nullptr) {
        _diag_error(context, init->expr.base.span, "Initialization target has no type");
        return nullptr;
    }

    _resolve_type(context, init->target_type);

    switch (init->target_type->tag) {
    case AST_TYPE_KIND_ARRAY: {
        AstType* element_type = ii_ast_type_get_array_element(init->target_type);

        AstExpr* size_expr = ii_ast_type_get_array_size(init->target_type);
        if (size_expr != nullptr && ii_ast_expr_get_kind(size_expr) == AST_KIND_LITERAL) {
            AstLiteral* size_lit = (AstLiteral*)size_expr;
            if (size_lit->variant == LITERAL_INT) {
                int64_t declared_size = size_lit->literal.int_value;
                size_t init_count = init->values ? uf_con_vector_length(init->values) : 0;
                if (declared_size >= 0 && (int64_t)init_count > declared_size) {
                    _diag_error(context, init->expr.base.span, "Array initializer has too many elements");
                }
            }
        }

        if (init->named != nullptr && uf_con_vector_length(init->named) > 0) {
            _diag_error(context, init->expr.base.span, "Array initialization does not support named values");
        }

        if (init->values != nullptr) {
            size_t count = uf_con_vector_length(init->values);
            for (size_t i = 0; i < count; i++) {
                AstExpr** value_ptr = uf_con_vector_get(init->values, i);
                if (value_ptr != nullptr && *value_ptr != nullptr &&
                    ii_ast_expr_get_kind(*value_ptr) == AST_KIND_INIT) {
                    ((AstInit*)*value_ptr)->target_type = element_type;
                }
            }
        }

        if (element_type != nullptr) {
            ast_foreach_init_values(init, value)
            {
                TastExpr* value_tast = _analyze_expr(context, value);
                if (value_tast != nullptr && value_tast->resolved_type != nullptr) {
                    if (!_types_match(context, value_tast->resolved_type, element_type) &&
                        !_can_coerce_numeric(element_type, value_tast->resolved_type)) {
                        _diag_error(context, init->expr.base.span, "Type mismatch in initialization");
                    }
                }
            }
            ast_foreach_end;

            ast_foreach_init_indexed(init, entry)
            {
                TastExpr* value_tast = _analyze_expr(context, entry.value);
                if (value_tast != nullptr && value_tast->resolved_type != nullptr) {
                    if (!_types_match(context, value_tast->resolved_type, element_type) &&
                        !_can_coerce_numeric(element_type, value_tast->resolved_type)) {
                        _diag_error(context, init->expr.base.span,
                                    "Type mismatch in indexed array initialization");
                    }
                }
            }
            ast_foreach_init_indexed_end;
        }

        TastExpr* tast = ii_tast_expr_new(context->ast);
        tast->resolved_type = init->target_type;
        init->expr.tast = tast;
        return tast;
    }

    case AST_TYPE_KIND_ANON: {
        TastExpr* tast = ii_tast_expr_new(context->ast);
        tast->resolved_type = init->target_type;
        init->expr.tast = tast;
        return tast;
    }

    case AST_TYPE_KIND_BLUEPRINT: {
        AstBlueprintDecl* bp = ii_ast_type_get_blueprint_resolved(init->target_type);
        if (bp == nullptr) {
            _diag_error(context, init->expr.base.span, "Unknown blueprint type in initialization");
            return nullptr;
        }

        if (bp->flat_fields == nullptr) {
            bp->flat_fields = uf_con_vector_new(sizeof(AstField*));
        }
        size_t field_count = uf_con_vector_length(bp->flat_fields);

        if (init->values != nullptr) {
            size_t value_count = uf_con_vector_length(init->values);
            if (value_count > field_count) {
                _diag_error(context, init->expr.base.span, "Too many values in initialization");
            }
            size_t i = 0;
            ast_foreach_init_values(init, value)
            {
                TastExpr* value_tast = _analyze_expr(context, value);
                if (value_tast == nullptr) {
                    i++;
                    continue;
                }
                AstField* field = *(AstField**)uf_con_vector_get(bp->flat_fields, i);
                if (field != nullptr && (!_types_match(context, value_tast->resolved_type, field->type) &&
                                         !_can_coerce_numeric(field->type, value_tast->resolved_type))) {
                    _diag_error(context, init->expr.base.span,
                                "Type mismatch in initialization for field '%s'", field->name);
                }
                i++;
            }
            ast_foreach_end;
        }

        ast_foreach_init_named(init, entry)
        {
            bool found = false;
            ast_foreach_flat_fields(bp, field)
            {
                if (field->name == entry.name) {
                    TastExpr* value_tast = _analyze_expr(context, entry.value);
                    if (value_tast != nullptr && value_tast->resolved_type != nullptr &&
                        !_types_match(context, value_tast->resolved_type, field->type) &&
                        !_can_coerce_numeric(field->type, value_tast->resolved_type)) {
                        _diag_error(context, init->expr.base.span,
                                    "Type mismatch in initialization for field '%s'", field->name);
                    }
                    found = true;
                    break;
                }
            }
            ast_foreach_end;
            if (!found) {
                _diag_error(context, init->expr.base.span, "Unknown field '%s' in initialization",
                            entry.name);
            }
        }
        ast_foreach_init_named_end;

        ast_foreach_init_indexed(init, entry)
        {
            TastExpr* index_tast = _analyze_expr(context, entry.index);
            if (index_tast == nullptr || index_tast->resolved_type == nullptr ||
                !_is_primitive_type(index_tast->resolved_type) ||
                ii_ast_type_get_primitive(index_tast->resolved_type) != LEXER_PRIM_INT) {
                _diag_error(context, init->expr.base.span, "Array index must be integer constant");
                continue;
            }
            int32_t idx = index_tast->const_int_value;
            if (idx < 0 || (size_t)idx >= field_count) {
                _diag_error(context, init->expr.base.span, "Index out of bounds");
                continue;
            }
            AstField* field = *(AstField**)uf_con_vector_get(bp->flat_fields, idx);
            TastExpr* value_tast = _analyze_expr(context, entry.value);
            if (value_tast != nullptr && !_types_match(context, value_tast->resolved_type, field->type) &&
                !_can_coerce_numeric(field->type, value_tast->resolved_type)) {
                _diag_error(context, init->expr.base.span, "Type mismatch in initialization for field '%s'",
                            field->name);
            }
        }
        ast_foreach_init_indexed_end;

        TastExpr* tast = ii_tast_expr_new(context->ast);
        tast->resolved_type = init->target_type;
        init->expr.tast = tast;
        return tast;
    }

    default:
        _diag_error(context, init->expr.base.span, "Can only initialize blueprint or array types");
        return nullptr;
    }
}

/*
 * Cast analysis.
 */

TastExpr* _analyze_cast(SemanticContext* context, AstCast* cast)
{
    TRACE_SCOPE(context->trace);

    if (cast->target_type == nullptr) {
        _diag_error(context, cast->expr.base.span, "Cast target type is null");
        return nullptr;
    }

    TastExpr* expr_tast = _analyze_expr(context, cast->expr_);
    if (expr_tast == nullptr) {
        return nullptr;
    }

    AstType* src_type = expr_tast->resolved_type;
    AstType* dst_type = cast->target_type;

    if (src_type == nullptr || dst_type == nullptr) {
        return nullptr;
    }

    bool valid_cast = false;

    if (_is_primitive_type(src_type) && _is_primitive_type(dst_type)) {
        valid_cast = true;
    } else if (_is_pointer_type(src_type) && _is_pointer_type(dst_type)) {
        valid_cast = true;
    } else if (_is_pointer_type(src_type) && _is_primitive_type(dst_type)) {
        valid_cast = true;
    } else if (_is_primitive_type(src_type) && _is_pointer_type(dst_type)) {
        valid_cast = true;
    } else if (_is_blueprint_type(src_type) && _is_blueprint_type(dst_type)) {
        valid_cast = true;
    }

    if (!valid_cast) {
        _diag_error(context, cast->expr.base.span, "Invalid cast");
        return nullptr;
    }

    TastExpr* tast = ii_tast_expr_new(context->ast);
    tast->resolved_type = cast->target_type;
    cast->expr.tast = tast;

    return tast;
}

/*
 * FFI analysis.
 */

TastExpr* _analyze_ffi(SemanticContext* context, AstFfi* ffi)
{
    TRACE_SCOPE(context->trace);

    ast_foreach_ffi_args(ffi, arg)
    {
        _analyze_expr(context, arg);
    }
    ast_foreach_end;

    TastExpr* tast = ii_tast_expr_new(context->ast);
    tast->resolved_type = nullptr;
    ffi->expr.tast = tast;

    return tast;
}
