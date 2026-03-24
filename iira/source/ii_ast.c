/**
 * @brief Abstract Syntax Tree and Typed AST implementation.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast.h"
#include "ii_ast_iter.h"
#include "uf_memory.h"

#include <stdio.h>
#include <string.h>

struct Ast {
    Source* source;
    UfMemRegion* arena;

    AstProgram* program;
    uint32_t anon_counter;

    /* Error tracking. */
    uint32_t error_count;
};

/*
 * Error tracking.
 */

void ii_ast_report_error(Ast* ast)
{
    ast->error_count++;
}

uint32_t ii_ast_get_error_count(const Ast* ast)
{
    return ast->error_count;
}

/*
 * Program builder API.
 */

AstFuncDecl* ii_ast_add_func(Ast* ast, const char* name)
{
    AstFuncDecl* func = uf_mem_region_zalloc(ast->arena, sizeof(AstFuncDecl));
    func->base.kind = AST_KIND_FUNC_DECL;
    func->name = name;

    if (ast->program->funcs == nullptr) {
        ast->program->funcs = uf_con_vector_new(sizeof(AstFuncDecl*));
    }
    uf_con_vector_push(ast->program->funcs, &func);

    return func;
}

AstBlueprintDecl* ii_ast_add_blueprint(Ast* ast, const char* name)
{
    AstBlueprintDecl* bp = uf_mem_region_zalloc(ast->arena, sizeof(AstBlueprintDecl));
    bp->base.kind = AST_KIND_BLUEPRINT_DECL;
    bp->name = name;

    if (ast->program->blueprints == nullptr) {
        ast->program->blueprints = uf_con_vector_new(sizeof(AstBlueprintDecl*));
    }
    uf_con_vector_push(ast->program->blueprints, &bp);

    return bp;
}

void ii_ast_program_finalize(Ast* ast)
{
    /* TODO: this is just placeholder for any post-processing. */
    (void)ast;
}

void ii_ast_program_add_func(Ast* ast, AstFuncDecl* func)
{
    AstProgram* prog = ii_ast_get_program(ast);
    if (prog->funcs == nullptr) {
        prog->funcs = uf_con_vector_new(sizeof(AstFuncDecl*));
    }
    uf_con_vector_push(prog->funcs, &func);
}

void ii_ast_program_add_blueprint(Ast* ast, AstBlueprintDecl* blueprint)
{
    AstProgram* prog = ii_ast_get_program(ast);
    if (prog->blueprints == nullptr) {
        prog->blueprints = uf_con_vector_new(sizeof(AstBlueprintDecl*));
    }
    uf_con_vector_push(prog->blueprints, &blueprint);
}

Source* ii_ast_get_source(const Ast* ast)
{
    return ast->source;
}

AstProgram* ii_ast_get_program(const Ast* ast)
{
    return ast->program;
}

/*
 * Type constructors.
 */

AstType* ii_ast_type_new(Ast* ast, enum AstKind kind)
{
    AstType* type = uf_mem_region_zalloc(ast->arena, sizeof(AstType));
    type->base.kind = kind;
    type->tast = nullptr;
    return type;
}

AstType* ii_ast_type_primitive(Ast* ast, enum LexerPrimitiveType prim)
{
    AstType* type = ii_ast_type_new(ast, AST_KIND_TYPE_PRIMITIVE);
    type->variant = AST_TYPE_KIND_PRIMITIVE;
    type->variant_u.primitive.prim_type = prim;
    return type;
}

AstType* ii_ast_type_pointer(Ast* ast, AstType* pointed)
{
    AstType* type = ii_ast_type_new(ast, AST_KIND_TYPE_POINTER);
    type->variant = AST_TYPE_KIND_POINTER;
    type->variant_u.pointer.pointed_type = pointed;
    return type;
}

AstType* ii_ast_type_array(Ast* ast, AstType* element, AstExpr* size_expr)
{
    AstType* type = ii_ast_type_new(ast, AST_KIND_TYPE_ARRAY);
    type->variant = AST_TYPE_KIND_ARRAY;
    type->variant_u.array.element_type = element;
    type->variant_u.array.size_expr = size_expr;
    type->variant_u.array.fixed_size = 0;
    return type;
}

AstType* ii_ast_type_blueprint(Ast* ast, const char* name)
{
    AstType* type = ii_ast_type_new(ast, AST_KIND_TYPE_BLUEPRINT);
    type->variant = AST_TYPE_KIND_BLUEPRINT;
    type->variant_u.blueprint.name = name;
    type->variant_u.blueprint.resolved = nullptr;
    return type;
}

AstType* ii_ast_type_anon(Ast* ast, const char* auto_name)
{
    AstType* type = ii_ast_type_new(ast, AST_KIND_TYPE_ANON);
    type->variant = AST_TYPE_KIND_ANON;
    type->variant_u.anon.fields = nullptr;
    type->variant_u.anon.field_count = 0;
    type->variant_u.anon.auto_name = auto_name;
    return type;
}

void ii_ast_type_anon_add_field(Ast* ast, AstType* anon, AstField* field)
{
    if (anon->variant_u.anon.fields == nullptr) {
        anon->variant_u.anon.fields = uf_mem_region_zalloc(ast->arena, sizeof(AstField) * 4);
        anon->variant_u.anon.field_count = 0;
    }
    anon->variant_u.anon.fields[anon->variant_u.anon.field_count++] = *field;
}

/*
 * Type constructors (span-aware).
 */

AstType* ii_ast_type_primitive_sp(Ast* ast, SourceSpan span, enum LexerPrimitiveType prim)
{
    AstType* type = ii_ast_type_primitive(ast, prim);
    type->base.span = span;
    return type;
}

AstType* ii_ast_type_pointer_sp(Ast* ast, SourceSpan span, AstType* pointed)
{
    AstType* type = ii_ast_type_pointer(ast, pointed);
    type->base.span = span;
    return type;
}

AstType* ii_ast_type_array_sp(Ast* ast, SourceSpan span, AstType* element, AstExpr* size_expr)
{
    AstType* type = ii_ast_type_array(ast, element, size_expr);
    type->base.span = span;
    return type;
}

AstType* ii_ast_type_blueprint_sp(Ast* ast, SourceSpan span, const char* name)
{
    AstType* type = ii_ast_type_blueprint(ast, name);
    type->base.span = span;
    return type;
}

AstType* ii_ast_type_anon_sp(Ast* ast, SourceSpan span, const char* auto_name)
{
    AstType* type = ii_ast_type_anon(ast, auto_name);
    type->base.span = span;
    return type;
}

/*
 * Declaration constructors.
 */

AstParam* ii_ast_param(Ast* ast, const char* name, AstType* type)
{
    AstParam* param = uf_mem_region_zalloc(ast->arena, sizeof(AstParam));
    param->base.kind = AST_KIND_PARAM;
    param->name = name;
    param->type = type;
    return param;
}

AstField* ii_ast_field(Ast* ast, const char* name, AstType* type, AstExpr* default_value)
{
    AstField* field = uf_mem_region_zalloc(ast->arena, sizeof(AstField));
    field->base.kind = AST_KIND_FIELD;
    field->name = name;
    field->type = type;
    field->default_value = default_value;
    return field;
}

AstMethodOverload* ii_ast_method_overload(Ast* ast, bool is_static, AstType* return_type)
{
    AstMethodOverload* overload = uf_mem_region_zalloc(ast->arena, sizeof(AstMethodOverload));
    overload->base.kind = AST_KIND_METHOD_OVERLOAD;
    overload->is_static = is_static;
    overload->return_type = return_type;
    overload->params = uf_con_vector_new(sizeof(AstParam*));
    overload->body = nullptr;
    return overload;
}

AstMethod* ii_ast_method(Ast* ast, const char* name)
{
    AstMethod* method = uf_mem_region_zalloc(ast->arena, sizeof(AstMethod));
    method->base.kind = AST_KIND_METHOD;
    method->name = name;
    method->overloads = uf_con_vector_new(sizeof(AstMethodOverload*));
    return method;
}

AstInherit* ii_ast_inherit(Ast* ast, const char* parent_name)
{
    AstInherit* inherit = uf_mem_region_zalloc(ast->arena, sizeof(AstInherit));
    inherit->base.kind = AST_KIND_INHERIT;
    inherit->parent_name = parent_name;
    inherit->resolved = nullptr;
    inherit->field_aliases = uf_con_vector_new(sizeof(struct {
        const char* original;
        const char* alias;
    }));
    return inherit;
}

AstBlueprintDecl* ii_ast_blueprint_decl(Ast* ast, const char* name)
{
    AstBlueprintDecl* blueprint = uf_mem_region_zalloc(ast->arena, sizeof(AstBlueprintDecl));
    blueprint->base.kind = AST_KIND_BLUEPRINT_DECL;
    blueprint->name = name;
    blueprint->parents = uf_con_vector_new(sizeof(AstInherit*));
    blueprint->fields = uf_con_vector_new(sizeof(AstField*));
    blueprint->methods = uf_con_vector_new(sizeof(AstMethod*));
    blueprint->flat_fields = uf_con_vector_new(sizeof(AstField*));
    blueprint->flat_methods = uf_con_vector_new(sizeof(AstMethod*));
    return blueprint;
}

AstFuncDecl* ii_ast_func_decl(Ast* ast, const char* name, AstType* return_type)
{
    AstFuncDecl* function = uf_mem_region_zalloc(ast->arena, sizeof(AstFuncDecl));
    function->base.kind = AST_KIND_FUNC_DECL;
    function->name = name;
    function->return_type = return_type;
    function->params = uf_con_vector_new(sizeof(AstParam*));
    function->body = nullptr;
    return function;
}

void ii_ast_func_add_param(AstFuncDecl* func, AstParam* param)
{
    uf_con_vector_push(func->params, &param);
}

void ii_ast_blueprint_add_field(AstBlueprintDecl* blueprint, AstField* field)
{
    if (blueprint->fields == nullptr) {
        blueprint->fields = uf_con_vector_new(sizeof(AstField*));
    }
    uf_con_vector_push(blueprint->fields, &field);
}

void ii_ast_blueprint_add_method(AstBlueprintDecl* blueprint, AstMethod* method)
{
    if (blueprint->methods == nullptr) {
        blueprint->methods = uf_con_vector_new(sizeof(AstMethod*));
    }
    uf_con_vector_push(blueprint->methods, &method);
}

void ii_ast_blueprint_add_inherit(AstBlueprintDecl* blueprint, AstInherit* inherit)
{
    if (blueprint->parents == nullptr) {
        blueprint->parents = uf_con_vector_new(sizeof(AstInherit*));
    }
    uf_con_vector_push(blueprint->parents, &inherit);
}

void ii_ast_inherit_add_alias(AstInherit* inherit, const char* original, const char* alias)
{
    struct {
        const char* original;
        const char* alias;
    } entry = {
        .original = original,
        .alias = alias,
    };
    uf_con_vector_push(inherit->field_aliases, &entry);
}

AstMethod* ii_ast_method_get_or_add(AstBlueprintDecl* blueprint, const char* name)
{
    if (blueprint->methods != nullptr) {
        size_t len = uf_con_vector_length(blueprint->methods);
        for (size_t i = 0; i < len; i++) {
            AstMethod* m = *(AstMethod**)uf_con_vector_get(blueprint->methods, i);
            if (m->name == name) {
                return m;
            }
        }
    }
    return nullptr;
}

void ii_ast_method_add_overload(AstMethod* method, AstMethodOverload* overload)
{
    if (method->overloads == nullptr) {
        method->overloads = uf_con_vector_new(sizeof(AstMethodOverload*));
    }
    uf_con_vector_push(method->overloads, &overload);
}

void ii_ast_method_overload_add_param(AstMethodOverload* overload, AstParam* param)
{
    if (overload->params == nullptr) {
        overload->params = uf_con_vector_new(sizeof(AstParam*));
    }
    uf_con_vector_push(overload->params, &param);
}

/*
 * Declaration constructors (span-aware).
 */

AstParam* ii_ast_param_sp(Ast* ast, SourceSpan span, const char* name, AstType* type)
{
    AstParam* param = ii_ast_param(ast, name, type);
    param->base.span = span;
    return param;
}

AstField* ii_ast_field_sp(Ast* ast, SourceSpan span, const char* name, AstType* type, AstExpr* default_value)
{
    AstField* field = ii_ast_field(ast, name, type, default_value);
    field->base.span = span;
    return field;
}

AstMethodOverload* ii_ast_method_overload_sp(Ast* ast, SourceSpan span, bool is_static, AstType* return_type)
{
    AstMethodOverload* overload = ii_ast_method_overload(ast, is_static, return_type);
    overload->base.span = span;
    return overload;
}

AstInherit* ii_ast_inherit_sp(Ast* ast, SourceSpan span, const char* parent_name)
{
    AstInherit* inherit = ii_ast_inherit(ast, parent_name);
    inherit->base.span = span;
    return inherit;
}

AstBlueprintDecl* ii_ast_blueprint_decl_sp(Ast* ast, SourceSpan span, const char* name)
{
    AstBlueprintDecl* blueprint = ii_ast_blueprint_decl(ast, name);
    blueprint->base.span = span;
    return blueprint;
}

AstFuncDecl* ii_ast_func_decl_sp(Ast* ast, SourceSpan span, const char* name, AstType* return_type)
{
    AstFuncDecl* func = ii_ast_func_decl(ast, name, return_type);
    func->base.span = span;
    return func;
}

/*
 * Statement constructors.
 */

AstBlock* ii_ast_block(Ast* ast)
{
    AstBlock* block = uf_mem_region_zalloc(ast->arena, sizeof(AstBlock));
    block->base.kind = AST_KIND_BLOCK;
    block->stmts = uf_con_vector_new(sizeof(AstStmt*));
    return block;
}

void ii_ast_block_add_stmt(AstBlock* block, AstStmt* stmt)
{
    uf_con_vector_push(block->stmts, &stmt);
}

AstReturn* ii_ast_return(Ast* ast, AstExpr* value)
{
    AstReturn* ret = uf_mem_region_zalloc(ast->arena, sizeof(AstReturn));
    ret->base.kind = AST_KIND_RETURN;
    ret->value = value;
    return (AstReturn*)ret;
}

AstDecl* ii_ast_decl(Ast* ast, const char* name, AstType* type, AstExpr* init, bool is_var)
{
    AstDecl* decl = uf_mem_region_zalloc(ast->arena, sizeof(AstDecl));
    decl->base.kind = AST_KIND_DECL;
    decl->name = name;
    decl->type = type;
    decl->init = init;
    decl->is_var = is_var;
    return decl;
}

AstIf* ii_ast_if(Ast* ast, AstExpr* condition, AstBlock* then_block, AstStmt* else_stmt)
{
    AstIf* if_stmt = uf_mem_region_zalloc(ast->arena, sizeof(AstIf));
    if_stmt->base.kind = AST_KIND_IF;
    if_stmt->condition = condition;
    if_stmt->then_block = then_block;
    if_stmt->else_stmt = else_stmt;
    return if_stmt;
}

AstFor* ii_ast_for(Ast* ast, AstStmt* init, AstExpr* condition, AstExpr* iter, AstBlock* body)
{
    AstFor* for_stmt = uf_mem_region_zalloc(ast->arena, sizeof(AstFor));
    for_stmt->base.kind = AST_KIND_FOR;
    for_stmt->init = init;
    for_stmt->condition = condition;
    for_stmt->iter = iter;
    for_stmt->body = body;
    return for_stmt;
}

AstWhile* ii_ast_while(Ast* ast, AstExpr* condition, AstBlock* body)
{
    AstWhile* while_stmt = uf_mem_region_zalloc(ast->arena, sizeof(AstWhile));
    while_stmt->base.kind = AST_KIND_WHILE;
    while_stmt->condition = condition;
    while_stmt->body = body;
    return while_stmt;
}

AstDoWhile* ii_ast_do_while(Ast* ast, AstBlock* body, AstExpr* condition)
{
    AstDoWhile* do_while = uf_mem_region_zalloc(ast->arena, sizeof(AstDoWhile));
    do_while->base.kind = AST_KIND_DO_WHILE;
    do_while->body = body;
    do_while->condition = condition;
    return do_while;
}

AstBreak* ii_ast_break(Ast* ast)
{
    AstBreak* brk = uf_mem_region_zalloc(ast->arena, sizeof(AstBreak));
    brk->base.kind = AST_KIND_BREAK;
    return brk;
}

AstContinue* ii_ast_continue(Ast* ast)
{
    AstContinue* cont = uf_mem_region_zalloc(ast->arena, sizeof(AstContinue));
    cont->base.kind = AST_KIND_CONTINUE;
    return cont;
}

AstExprStmt* ii_ast_expr_stmt(Ast* ast, AstExpr* expr)
{
    AstExprStmt* stmt = uf_mem_region_zalloc(ast->arena, sizeof(AstExprStmt));
    stmt->base.kind = AST_KIND_EXPR_STMT;
    stmt->expr = expr;
    return stmt;
}

/*
 * Statement constructors (span-aware).
 */

AstBlock* ii_ast_block_sp(Ast* ast, SourceSpan span)
{
    AstBlock* block = ii_ast_block(ast);
    block->base.span = span;
    return block;
}

AstReturn* ii_ast_return_sp(Ast* ast, SourceSpan span, AstExpr* value)
{
    AstReturn* ret = ii_ast_return(ast, value);
    ret->base.span = span;
    return ret;
}

AstDecl* ii_ast_decl_sp(Ast* ast, SourceSpan span, const char* name, AstType* type, AstExpr* init,
                        bool is_var)
{
    AstDecl* decl = ii_ast_decl(ast, name, type, init, is_var);
    decl->base.span = span;
    return decl;
}

AstIf* ii_ast_if_sp(Ast* ast, SourceSpan span, AstExpr* condition, AstBlock* then_block, AstStmt* else_stmt)
{
    AstIf* if_stmt = ii_ast_if(ast, condition, then_block, else_stmt);
    if_stmt->base.span = span;
    return if_stmt;
}

AstFor* ii_ast_for_sp(Ast* ast, SourceSpan span, AstStmt* init, AstExpr* condition, AstExpr* iter,
                      AstBlock* body)
{
    AstFor* for_stmt = ii_ast_for(ast, init, condition, iter, body);
    for_stmt->base.span = span;
    return for_stmt;
}

AstWhile* ii_ast_while_sp(Ast* ast, SourceSpan span, AstExpr* condition, AstBlock* body)
{
    AstWhile* while_stmt = ii_ast_while(ast, condition, body);
    while_stmt->base.span = span;
    return while_stmt;
}

AstDoWhile* ii_ast_do_while_sp(Ast* ast, SourceSpan span, AstBlock* body, AstExpr* condition)
{
    AstDoWhile* do_while = ii_ast_do_while(ast, body, condition);
    do_while->base.span = span;
    return do_while;
}

AstBreak* ii_ast_break_sp(Ast* ast, SourceSpan span)
{
    AstBreak* brk = ii_ast_break(ast);
    brk->base.span = span;
    return brk;
}

AstContinue* ii_ast_continue_sp(Ast* ast, SourceSpan span)
{
    AstContinue* cont = ii_ast_continue(ast);
    cont->base.span = span;
    return cont;
}

AstExprStmt* ii_ast_expr_stmt_sp(Ast* ast, SourceSpan span, AstExpr* expr)
{
    AstExprStmt* stmt = ii_ast_expr_stmt(ast, expr);
    stmt->base.span = span;
    return stmt;
}

/*
 * Expression constructors.
 */

AstLiteral* ii_ast_literal_int(Ast* ast, int64_t value)
{
    AstLiteral* lit = uf_mem_region_zalloc(ast->arena, sizeof(AstLiteral));
    lit->expr.base.kind = AST_KIND_LITERAL;
    lit->expr.tast = nullptr;
    lit->variant = LITERAL_INT;
    lit->literal.int_value = value;
    return lit;
}

AstLiteral* ii_ast_literal_float(Ast* ast, double value)
{
    AstLiteral* lit = uf_mem_region_zalloc(ast->arena, sizeof(AstLiteral));
    lit->expr.base.kind = AST_KIND_LITERAL;
    lit->expr.tast = nullptr;
    lit->variant = LITERAL_FLOAT;
    lit->literal.float_value = value;
    return lit;
}

AstLiteral* ii_ast_literal_string(Ast* ast, const char* value)
{
    AstLiteral* lit = uf_mem_region_zalloc(ast->arena, sizeof(AstLiteral));
    lit->expr.base.kind = AST_KIND_LITERAL;
    lit->expr.tast = nullptr;
    lit->variant = LITERAL_STRING;
    lit->literal.string_value = value;
    return lit;
}

AstLiteral* ii_ast_literal_bool(Ast* ast, bool value)
{
    AstLiteral* lit = uf_mem_region_zalloc(ast->arena, sizeof(AstLiteral));
    lit->expr.base.kind = AST_KIND_LITERAL;
    lit->expr.tast = nullptr;
    lit->variant = LITERAL_BOOL;
    lit->literal.bool_value = value;
    return lit;
}

AstLiteral* ii_ast_literal_char(Ast* ast, char value)
{
    AstLiteral* lit = uf_mem_region_zalloc(ast->arena, sizeof(AstLiteral));
    lit->expr.base.kind = AST_KIND_LITERAL;
    lit->expr.tast = nullptr;
    lit->variant = LITERAL_CHAR;
    lit->literal.char_value = value;
    return lit;
}

AstLiteral* ii_ast_literal_null(Ast* ast)
{
    AstLiteral* lit = uf_mem_region_zalloc(ast->arena, sizeof(AstLiteral));
    lit->expr.base.kind = AST_KIND_LITERAL;
    lit->expr.tast = nullptr;
    lit->variant = LITERAL_NULL;
    return lit;
}

AstLiteral* ii_ast_literal_int_sp(Ast* ast, SourceSpan span, int64_t value)
{
    AstLiteral* lit = ii_ast_literal_int(ast, value);
    lit->expr.base.span = span;
    return lit;
}

AstLiteral* ii_ast_literal_float_sp(Ast* ast, SourceSpan span, double value)
{
    AstLiteral* lit = ii_ast_literal_float(ast, value);
    lit->expr.base.span = span;
    return lit;
}

AstLiteral* ii_ast_literal_string_sp(Ast* ast, SourceSpan span, const char* value)
{
    AstLiteral* lit = ii_ast_literal_string(ast, value);
    lit->expr.base.span = span;
    return lit;
}

AstLiteral* ii_ast_literal_bool_sp(Ast* ast, SourceSpan span, bool value)
{
    AstLiteral* lit = ii_ast_literal_bool(ast, value);
    lit->expr.base.span = span;
    return lit;
}

AstLiteral* ii_ast_literal_char_sp(Ast* ast, SourceSpan span, char value)
{
    AstLiteral* lit = ii_ast_literal_char(ast, value);
    lit->expr.base.span = span;
    return lit;
}

AstLiteral* ii_ast_literal_null_sp(Ast* ast, SourceSpan span)
{
    AstLiteral* lit = ii_ast_literal_null(ast);
    lit->expr.base.span = span;
    return lit;
}

AstIdent* ii_ast_ident(Ast* ast, const char* name)
{
    AstIdent* ident = uf_mem_region_zalloc(ast->arena, sizeof(AstIdent));
    ident->expr.base.kind = AST_KIND_IDENT;
    ident->expr.tast = nullptr;
    ident->name = name;
    ident->resolved_kind = AST_IDENT_NONE;
    return ident;
}

AstBinary* ii_ast_binary(Ast* ast, AstExpr* left, enum LexerTokenType op, AstExpr* right)
{
    AstBinary* bin = uf_mem_region_zalloc(ast->arena, sizeof(AstBinary));
    bin->expr.base.kind = AST_KIND_BINARY;
    bin->expr.tast = nullptr;
    bin->left = left;
    bin->right = right;
    bin->op = op;
    return bin;
}

AstUnary* ii_ast_unary(Ast* ast, enum LexerTokenType op, AstExpr* operand)
{
    AstUnary* un = uf_mem_region_zalloc(ast->arena, sizeof(AstUnary));
    un->expr.base.kind = AST_KIND_UNARY;
    un->expr.tast = nullptr;
    un->operand = operand;
    un->op = op;
    return un;
}

AstCall* ii_ast_call(Ast* ast, AstExpr* callee)
{
    AstCall* call = uf_mem_region_zalloc(ast->arena, sizeof(AstCall));
    call->expr.base.kind = AST_KIND_CALL;
    call->expr.tast = nullptr;
    call->callee = callee;
    call->args = uf_con_vector_new(sizeof(AstExpr*));
    call->resolved_overload = nullptr;
    return call;
}

void ii_ast_call_add_arg(AstCall* call, AstExpr* arg)
{
    uf_con_vector_push(call->args, &arg);
}

AstMember* ii_ast_member(Ast* ast, AstExpr* object, const char* member_name)
{
    AstMember* member = uf_mem_region_zalloc(ast->arena, sizeof(AstMember));
    member->expr.base.kind = AST_KIND_MEMBER;
    member->expr.tast = nullptr;
    member->object = object;
    member->member_name = member_name;
    member->is_method_call = false;
    member->resolved_field = nullptr;
    member->resolved_method = nullptr;
    return member;
}

AstIndex* ii_ast_index(Ast* ast, AstExpr* array, AstExpr* index)
{
    AstIndex* idx = uf_mem_region_zalloc(ast->arena, sizeof(AstIndex));
    idx->expr.base.kind = AST_KIND_INDEX;
    idx->expr.tast = nullptr;
    idx->array = array;
    idx->index = index;
    return idx;
}

AstInit* ii_ast_init(Ast* ast)
{
    AstInit* init = uf_mem_region_zalloc(ast->arena, sizeof(AstInit));
    init->expr.base.kind = AST_KIND_INIT;
    init->expr.tast = nullptr;
    init->values = uf_con_vector_new(sizeof(AstExpr*));
    init->named = uf_con_vector_new(sizeof(struct {
        const char* name;
        AstExpr* value;
    }));
    init->indexed = uf_con_vector_new(sizeof(struct {
        AstExpr* index;
        AstExpr* value;
    }));
    init->target_type = nullptr;
    return init;
}

void ii_ast_init_add_value(AstInit* init, AstExpr* value)
{
    uf_con_vector_push(init->values, &value);
}

void ii_ast_init_add_named(AstInit* init, const char* name, AstExpr* value)
{
    struct {
        const char* name;
        AstExpr* value;
    } entry = {name, value};
    uf_con_vector_push(init->named, &entry);
}

void ii_ast_init_add_indexed(AstInit* init, AstExpr* index, AstExpr* value)
{
    struct {
        AstExpr* index;
        AstExpr* value;
    } entry = {index, value};
    uf_con_vector_push(init->indexed, &entry);
}

AstCast* ii_ast_cast(Ast* ast, AstType* target_type, AstExpr* expr)
{
    AstCast* cast = uf_mem_region_zalloc(ast->arena, sizeof(AstCast));
    cast->expr.base.kind = AST_KIND_CAST;
    cast->expr.tast = nullptr;
    cast->target_type = target_type;
    cast->expr_ = expr;
    return cast;
}

AstFfi* ii_ast_ffi(Ast* ast, const char* function_name)
{
    AstFfi* ffi = uf_mem_region_zalloc(ast->arena, sizeof(AstFfi));
    ffi->expr.base.kind = AST_KIND_FFI;
    ffi->expr.tast = nullptr;
    ffi->function_name = function_name;
    ffi->args = uf_con_vector_new(sizeof(AstExpr*));
    return ffi;
}

void ii_ast_ffi_add_arg(AstFfi* ffi, AstExpr* arg)
{
    uf_con_vector_push(ffi->args, &arg);
}

/*
 * Expression constructors (span-aware).
 */

AstIdent* ii_ast_ident_sp(Ast* ast, SourceSpan span, const char* name)
{
    AstIdent* ident = ii_ast_ident(ast, name);
    ident->expr.base.span = span;
    return ident;
}

AstBinary* ii_ast_binary_sp(Ast* ast, SourceSpan span, AstExpr* left, enum LexerTokenType op, AstExpr* right)
{
    AstBinary* bin = ii_ast_binary(ast, left, op, right);
    bin->expr.base.span = span;
    return bin;
}

AstUnary* ii_ast_unary_sp(Ast* ast, SourceSpan span, enum LexerTokenType op, AstExpr* operand)
{
    AstUnary* unary = ii_ast_unary(ast, op, operand);
    unary->expr.base.span = span;
    return unary;
}

AstCall* ii_ast_call_sp(Ast* ast, SourceSpan span, AstExpr* callee)
{
    AstCall* call = ii_ast_call(ast, callee);
    call->expr.base.span = span;
    return call;
}

AstMember* ii_ast_member_sp(Ast* ast, SourceSpan span, AstExpr* object, const char* member_name)
{
    AstMember* member = ii_ast_member(ast, object, member_name);
    member->expr.base.span = span;
    return member;
}

AstIndex* ii_ast_index_sp(Ast* ast, SourceSpan span, AstExpr* array, AstExpr* index)
{
    AstIndex* idx = ii_ast_index(ast, array, index);
    idx->expr.base.span = span;
    return idx;
}

AstInit* ii_ast_init_sp(Ast* ast, SourceSpan span)
{
    AstInit* init = ii_ast_init(ast);
    init->expr.base.span = span;
    return init;
}

AstCast* ii_ast_cast_sp(Ast* ast, SourceSpan span, AstType* target_type, AstExpr* expr)
{
    AstCast* cast = ii_ast_cast(ast, target_type, expr);
    cast->expr.base.span = span;
    return cast;
}

AstFfi* ii_ast_ffi_sp(Ast* ast, SourceSpan span, const char* function_name)
{
    AstFfi* ffi = ii_ast_ffi(ast, function_name);
    ffi->expr.base.span = span;
    return ffi;
}

/*
 * Error node constructor.
 */

AstError* ii_ast_error(Ast* ast, SourceSpan span)
{
    AstError* err = uf_mem_region_zalloc(ast->arena, sizeof(AstError));
    err->base.kind = AST_KIND_ERROR;
    err->base.span = span;
    ast->error_count++;
    return err;
}

/*
 * TAST (Typed AST) helpers.
 */

TastExpr* ii_tast_expr_new(Ast* ast)
{
    TastExpr* tast = uf_mem_region_zalloc(ast->arena, sizeof(TastExpr));
    tast->is_lvalue = false;
    tast->is_constant = false;
    tast->const_int_value = 0;
    return tast;
}

TastType* ii_tast_type_new(Ast* ast)
{
    TastType* tast = uf_mem_region_zalloc(ast->arena, sizeof(TastType));
    tast->state = TAST_RESOLUTION_UNRESOLVED;
    tast->size = 0;
    tast->alignment = 0;
    tast->c_repr = nullptr;
    return tast;
}

/*
 * Traversal helpers.
 */

static void _visit_type(AstType* type, AstVisitorFn visitor, void* context, uint32_t depth, bool pre_order);
static void _visit_expr(AstExpr* expr, AstVisitorFn visitor, void* context, uint32_t depth, bool pre_order);
static void _visit_stmt(AstStmt* stmt, AstVisitorFn visitor, void* context, uint32_t depth, bool pre_order);
static void _visit_block(AstBlock* block, AstVisitorFn visitor, void* context, uint32_t depth,
                         bool pre_order);

static void _visit_param(AstParam* param, AstVisitorFn visitor, void* context, uint32_t depth, bool pre_order)
{
    if (!param) {
        return;
    }

    if (pre_order && !visitor(&param->base, context, depth)) {
        return;
    }

    _visit_type(param->type, visitor, context, depth + 1, pre_order);
    if (!pre_order) {
        visitor(&param->base, context, depth);
    }
}

static void _visit_field(AstField* field, AstVisitorFn visitor, void* context, uint32_t depth, bool pre_order)
{
    if (!field) {
        return;
    }

    if (pre_order && !visitor(&field->base, context, depth)) {
        return;
    }

    _visit_type(field->type, visitor, context, depth + 1, pre_order);
    if (field->default_value) {
        _visit_expr(field->default_value, visitor, context, depth + 1, pre_order);
    }

    if (!pre_order) {
        visitor(&field->base, context, depth);
    }
}

static void _visit_method_overload(AstMethodOverload* overload, AstVisitorFn visitor, void* context,
                                   uint32_t depth, bool pre_order)
{
    if (!overload) {
        return;
    }

    if (pre_order && !visitor(&overload->base, context, depth)) {
        return;
    }

    ast_foreach_params(overload, param)
    {
        _visit_param(param, visitor, context, depth + 1, pre_order);
    }
    ast_foreach_end;

    if (overload->return_type) {
        _visit_type(overload->return_type, visitor, context, depth + 1, pre_order);
    }

    if (overload->body) {
        _visit_block(overload->body, visitor, context, depth + 1, pre_order);
    }

    if (!pre_order) {
        visitor(&overload->base, context, depth);
    }
}

static void _visit_method(AstMethod* method, AstVisitorFn visitor, void* context, uint32_t depth,
                          bool pre_order)
{
    if (!method) {
        return;
    }

    if (pre_order && !visitor(&method->base, context, depth)) {
        return;
    }

    ast_foreach_overloads(method, ov)
    {
        _visit_method_overload(ov, visitor, context, depth + 1, pre_order);
    }
    ast_foreach_end;

    if (!pre_order) {
        visitor(&method->base, context, depth);
    }
}

static void _visit_inherit(AstInherit* inherit, AstVisitorFn visitor, void* context, uint32_t depth,
                           bool pre_order)
{
    if (!inherit) {
        return;
    }

    if (pre_order && !visitor(&inherit->base, context, depth)) {
        return;
    }

    /* Don't visit resolved - it's set during semantic. */
    if (!pre_order) {
        visitor(&inherit->base, context, depth);
    }
}

static void _visit_blueprint(AstBlueprintDecl* bp, AstVisitorFn visitor, void* context, uint32_t depth,
                             bool pre_order)
{
    if (!bp) {
        return;
    }

    if (pre_order && !visitor(&bp->base, context, depth)) {
        return;
    }

    ast_foreach_parents(bp, inh)
    {
        _visit_inherit(inh, visitor, context, depth + 1, pre_order);
    }
    ast_foreach_end;

    ast_foreach_fields(bp, field)
    {
        _visit_field(field, visitor, context, depth + 1, pre_order);
    }
    ast_foreach_end;

    ast_foreach_methods(bp, method)
    {
        _visit_method(method, visitor, context, depth + 1, pre_order);
    }
    ast_foreach_end;

    if (!pre_order) {
        visitor(&bp->base, context, depth);
    }
}

static void _visit_func(AstFuncDecl* func, AstVisitorFn visitor, void* context, uint32_t depth,
                        bool pre_order)
{
    if (!func) {
        return;
    }

    if (pre_order && !visitor(&func->base, context, depth)) {
        return;
    }

    ast_foreach_params(func, param)
    {
        _visit_param(param, visitor, context, depth + 1, pre_order);
    }
    ast_foreach_end;

    if (func->return_type) {
        _visit_type(func->return_type, visitor, context, depth + 1, pre_order);
    }

    if (func->body) {
        _visit_block(func->body, visitor, context, depth + 1, pre_order);
    }

    if (!pre_order) {
        visitor(&func->base, context, depth);
    }
}

static void _visit_type(AstType* type, AstVisitorFn visitor, void* context, uint32_t depth, bool pre_order)
{
    if (!type) {
        return;
    }

    if (pre_order && !visitor(&type->base, context, depth)) {
        return;
    }

    switch (type->variant) {
    case AST_TYPE_KIND_POINTER:
        _visit_type(type->variant_u.pointer.pointed_type, visitor, context, depth + 1, pre_order);
        break;
    case AST_TYPE_KIND_ARRAY:
        _visit_type(type->variant_u.array.element_type, visitor, context, depth + 1, pre_order);
        if (type->variant_u.array.size_expr) {
            _visit_expr(type->variant_u.array.size_expr, visitor, context, depth + 1, pre_order);
        }
        break;
    case AST_TYPE_KIND_BLUEPRINT:
        /* Don't visit resolved - set during semantic */
        break;
    case AST_TYPE_KIND_ANON:
        for (size_t i = 0; i < type->variant_u.anon.field_count; i++) {
            _visit_field(&type->variant_u.anon.fields[i], visitor, context, depth + 1, pre_order);
        }
        break;
    case AST_TYPE_KIND_PRIMITIVE:
        break;
    }

    if (!pre_order) {
        visitor(&type->base, context, depth);
    }
}

static void _visit_expr(AstExpr* expr, AstVisitorFn visitor, void* context, uint32_t depth, bool pre_order)
{
    if (!expr) {
        return;
    }

    if (pre_order && !visitor(&expr->base, context, depth)) {
        return;
    }

    switch (expr->base.kind) {
    case AST_KIND_LITERAL:
    case AST_KIND_IDENT:
        /* No children. */
        break;
    case AST_KIND_BINARY: {
        AstBinary* bin = (AstBinary*)expr;
        _visit_expr(bin->left, visitor, context, depth + 1, pre_order);
        _visit_expr(bin->right, visitor, context, depth + 1, pre_order);
        break;
    }
    case AST_KIND_UNARY: {
        AstUnary* un = (AstUnary*)expr;
        _visit_expr(un->operand, visitor, context, depth + 1, pre_order);
        break;
    }
    case AST_KIND_CALL: {
        AstCall* call = (AstCall*)expr;
        _visit_expr(call->callee, visitor, context, depth + 1, pre_order);
        ast_foreach_call_args(call, arg)
        {
            _visit_expr(arg, visitor, context, depth + 1, pre_order);
        }
        ast_foreach_end;
        break;
    }
    case AST_KIND_MEMBER: {
        AstMember* member = (AstMember*)expr;
        _visit_expr(member->object, visitor, context, depth + 1, pre_order);
        break;
    }
    case AST_KIND_INDEX: {
        AstIndex* idx = (AstIndex*)expr;
        _visit_expr(idx->array, visitor, context, depth + 1, pre_order);
        _visit_expr(idx->index, visitor, context, depth + 1, pre_order);
        break;
    }
    case AST_KIND_INIT: {
        AstInit* init = (AstInit*)expr;
        ast_foreach_init_values(init, val)
        {
            _visit_expr(val, visitor, context, depth + 1, pre_order);
        }
        ast_foreach_end;
        /* 'named' and 'indexed' contain pairs. We'll skip them for simplicity. */
        if (init->target_type) {
            _visit_type(init->target_type, visitor, context, depth + 1, pre_order);
        }
        break;
    }
    case AST_KIND_CAST: {
        AstCast* cast = (AstCast*)expr;
        _visit_type(cast->target_type, visitor, context, depth + 1, pre_order);
        _visit_expr(cast->expr_, visitor, context, depth + 1, pre_order);
        break;
    }
    case AST_KIND_FFI: {
        AstFfi* ffi = (AstFfi*)expr;
        ast_foreach_ffi_args(ffi, arg)
        {
            _visit_expr(arg, visitor, context, depth + 1, pre_order);
        }
        ast_foreach_end;
        break;
    }
    default:
        break;
    }

    if (!pre_order) {
        visitor(&expr->base, context, depth);
    }
}

static void _visit_block(AstBlock* block, AstVisitorFn visitor, void* context, uint32_t depth, bool pre_order)
{
    if (!block) {
        return;
    }

    if (pre_order && !visitor(&block->base, context, depth)) {
        return;
    }

    ast_foreach_stmts(block, stmt)
    {
        _visit_stmt(stmt, visitor, context, depth + 1, pre_order);
    }
    ast_foreach_end;

    if (!pre_order) {
        visitor(&block->base, context, depth);
    }
}

static void _visit_stmt(AstStmt* stmt, AstVisitorFn visitor, void* context, uint32_t depth, bool pre_order)
{
    if (!stmt) {
        return;
    }
    if (pre_order && !visitor((AstNode*)stmt, context, depth)) {
        return;
    }

    switch (stmt->kind) {
    case AST_KIND_BLOCK:
        _visit_block((AstBlock*)stmt, visitor, context, depth + 1, pre_order);
        break;
    case AST_KIND_RETURN: {
        AstReturn* ret = (AstReturn*)stmt;
        if (ret->value)
            _visit_expr(ret->value, visitor, context, depth + 1, pre_order);
        break;
    }
    case AST_KIND_DECL: {
        AstDecl* decl = (AstDecl*)stmt;
        _visit_type(decl->type, visitor, context, depth + 1, pre_order);
        if (decl->init)
            _visit_expr(decl->init, visitor, context, depth + 1, pre_order);
        break;
    }
    case AST_KIND_IF: {
        AstIf* if_stmt = (AstIf*)stmt;
        _visit_expr(if_stmt->condition, visitor, context, depth + 1, pre_order);
        _visit_block(if_stmt->then_block, visitor, context, depth + 1, pre_order);
        if (if_stmt->else_stmt)
            _visit_stmt(if_stmt->else_stmt, visitor, context, depth + 1, pre_order);
        break;
    }
    case AST_KIND_FOR: {
        AstFor* for_stmt = (AstFor*)stmt;
        if (for_stmt->init)
            _visit_stmt(for_stmt->init, visitor, context, depth + 1, pre_order);
        if (for_stmt->condition)
            _visit_expr(for_stmt->condition, visitor, context, depth + 1, pre_order);
        if (for_stmt->iter)
            _visit_expr(for_stmt->iter, visitor, context, depth + 1, pre_order);
        if (for_stmt->body)
            _visit_block(for_stmt->body, visitor, context, depth + 1, pre_order);
        break;
    }
    case AST_KIND_WHILE: {
        AstWhile* while_stmt = (AstWhile*)stmt;
        _visit_expr(while_stmt->condition, visitor, context, depth + 1, pre_order);
        _visit_block(while_stmt->body, visitor, context, depth + 1, pre_order);
        break;
    }
    case AST_KIND_DO_WHILE: {
        AstDoWhile* do_while = (AstDoWhile*)stmt;
        _visit_block(do_while->body, visitor, context, depth + 1, pre_order);
        _visit_expr(do_while->condition, visitor, context, depth + 1, pre_order);
        break;
    }
    case AST_KIND_BREAK:
    case AST_KIND_CONTINUE:
        /* No children. */
        break;
    case AST_KIND_EXPR_STMT: {
        AstExprStmt* expr_stmt = (AstExprStmt*)stmt;
        if (expr_stmt->expr)
            _visit_expr(expr_stmt->expr, visitor, context, depth + 1, pre_order);
        break;
    }
    default:
        break;
    }

    if (!pre_order) {
        visitor((AstNode*)stmt, context, depth);
    }
}

void ii_ast_visit(Ast* ast, AstVisitorFn visitor, void* context)
{
    if (!ast || !ast->program || !visitor) {
        return;
    }

    /* Visit program node first. */
    if (!visitor(&ast->program->base, context, 0)) {
        return;
    }

    /* Visit functions. */
    ast_foreach_funcs(ast, func)
    {
        _visit_func(func, visitor, context, 1, true);
    }
    ast_foreach_end;

    /* Visit blueprints. */
    ast_foreach_blueprints(ast, bp)
    {
        _visit_blueprint(bp, visitor, context, 1, true);
    }
    ast_foreach_end;
}

void ii_ast_visit_reverse(Ast* ast, AstVisitorFn visitor, void* context)
{
    if (!ast || !ast->program || !visitor) {
        return;
    }

    /* Visit functions first (in reverse order). */
    ast_foreach_funcs_rev(ast, func)
    {
        _visit_func(func, visitor, context, 1, false);
    }
    ast_foreach_end_rev;

    /* Visit blueprints (in reverse order). */
    ast_foreach_blueprints_rev(ast, bp)
    {
        _visit_blueprint(bp, visitor, context, 1, false);
    }
    ast_foreach_end_rev;

    /* Visit program node last. */
    visitor(&ast->program->base, context, 0);
}

/*
 * Utility functions.
 */

const char* ii_ast_kind_name(enum AstKind kind)
{
    switch (kind) {
    case AST_KIND_PROGRAM:
        return "PROGRAM";
    case AST_KIND_TYPE_PRIMITIVE:
        return "TYPE_PRIMITIVE";
    case AST_KIND_TYPE_POINTER:
        return "TYPE_POINTER";
    case AST_KIND_TYPE_ARRAY:
        return "TYPE_ARRAY";
    case AST_KIND_TYPE_BLUEPRINT:
        return "TYPE_BLUEPRINT";
    case AST_KIND_TYPE_ANON:
        return "TYPE_ANON";
    case AST_KIND_FUNC_DECL:
        return "FUNC_DECL";
    case AST_KIND_BLUEPRINT_DECL:
        return "BLUEPRINT_DECL";
    case AST_KIND_FIELD:
        return "FIELD";
    case AST_KIND_METHOD:
        return "METHOD";
    case AST_KIND_METHOD_OVERLOAD:
        return "METHOD_OVERLOAD";
    case AST_KIND_PARAM:
        return "PARAM";
    case AST_KIND_INHERIT:
        return "INHERIT";
    case AST_KIND_BLOCK:
        return "BLOCK";
    case AST_KIND_RETURN:
        return "RETURN";
    case AST_KIND_DECL:
        return "DECL";
    case AST_KIND_IF:
        return "IF";
    case AST_KIND_FOR:
        return "FOR";
    case AST_KIND_WHILE:
        return "WHILE";
    case AST_KIND_DO_WHILE:
        return "DO_WHILE";
    case AST_KIND_BREAK:
        return "BREAK";
    case AST_KIND_CONTINUE:
        return "CONTINUE";
    case AST_KIND_EXPR_STMT:
        return "EXPR_STMT";
    case AST_KIND_LITERAL:
        return "LITERAL";
    case AST_KIND_IDENT:
        return "IDENT";
    case AST_KIND_BINARY:
        return "BINARY";
    case AST_KIND_UNARY:
        return "UNARY";
    case AST_KIND_CALL:
        return "CALL";
    case AST_KIND_MEMBER:
        return "MEMBER";
    case AST_KIND_INDEX:
        return "INDEX";
    case AST_KIND_INIT:
        return "INIT";
    case AST_KIND_CAST:
        return "CAST";
    case AST_KIND_FFI:
        return "FFI";
    case AST_KIND_ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

static const char* _token_type_to_string(enum LexerTokenType tok)
{
    switch (tok) {
    case LEXER_TOK_ASSIGN:
        return "=";
    case LEXER_TOK_PLUS:
        return "+";
    case LEXER_TOK_MINUS:
        return "-";
    case LEXER_TOK_STAR:
        return "*";
    case LEXER_TOK_SLASH:
        return "/";
    case LEXER_TOK_PERCENT:
        return "%";
    case LEXER_TOK_PLUS_ASSIGN:
        return "+=";
    case LEXER_TOK_MINUS_ASSIGN:
        return "-=";
    case LEXER_TOK_STAR_ASSIGN:
        return "*=";
    case LEXER_TOK_SLASH_ASSIGN:
        return "/=";
    case LEXER_TOK_EQ:
        return "==";
    case LEXER_TOK_NEQ:
        return "!=";
    case LEXER_TOK_LT:
        return "<";
    case LEXER_TOK_GT:
        return ">";
    case LEXER_TOK_LTE:
        return "<=";
    case LEXER_TOK_GTE:
        return ">=";
    case LEXER_TOK_AND:
        return "&&";
    case LEXER_TOK_OR:
        return "||";
    case LEXER_TOK_NOT:
        return "!";
    case LEXER_TOK_BIT_AND:
        return "&";
    case LEXER_TOK_BIT_OR:
        return "|";
    case LEXER_TOK_BIT_XOR:
        return "^";
    case LEXER_TOK_BIT_NOT:
        return "~";
    default:
        return "?";
    }
}

static const char* _primitive_type_to_string(enum LexerPrimitiveType prim)
{
    switch (prim) {
    case LEXER_PRIM_VOID:
        return "void";
    case LEXER_PRIM_BOOL:
        return "bool";
    case LEXER_PRIM_CHAR:
        return "char";
    case LEXER_PRIM_INT:
        return "int";
    case LEXER_PRIM_LONG:
        return "long";
    case LEXER_PRIM_SHORT:
        return "short";
    case LEXER_PRIM_FLOAT:
        return "float";
    case LEXER_PRIM_DOUBLE:
        return "double";
    default:
        return "?";
    }
}

struct AstPrintContext {
    FILE* output;
    int indent_level;
};

static void _print_indent(struct AstPrintContext* context)
{
    for (int i = 0; i < context->indent_level; i++) {
        fprintf(context->output, "  ");
    }
}

static void _print_type(struct AstPrintContext* context, AstType* type);

static void _print_type_variant(struct AstPrintContext* context, AstType* type)
{
    switch (type->variant) {
    case AST_TYPE_KIND_PRIMITIVE:
        fprintf(context->output, "%s", _primitive_type_to_string(type->variant_u.primitive.prim_type));
        break;
    case AST_TYPE_KIND_POINTER:
        _print_type(context, type->variant_u.pointer.pointed_type);
        fprintf(context->output, "*");
        break;
    case AST_TYPE_KIND_ARRAY:
        _print_type(context, type->variant_u.array.element_type);
        fprintf(context->output, "[]");
        break;
    case AST_TYPE_KIND_BLUEPRINT:
        fprintf(context->output, "%s", type->variant_u.blueprint.name);
        break;
    case AST_TYPE_KIND_ANON:
        fprintf(context->output, "{ ... }");
        break;
    }
}

static void _print_type(struct AstPrintContext* context, AstType* type)
{
    if (!type) {
        fprintf(context->output, "<null>");
        return;
    }
    _print_type_variant(context, type);
}

static void _print_literal(struct AstPrintContext* context, AstLiteral* lit)
{
    switch (lit->variant) {
    case LITERAL_INT:
        fprintf(context->output, "%lld", (long long)lit->literal.int_value);
        break;
    case LITERAL_FLOAT:
        fprintf(context->output, "%f", lit->literal.float_value);
        break;
    case LITERAL_STRING:
        fprintf(context->output, "\"%s\"", lit->literal.string_value);
        break;
    case LITERAL_BOOL:
        fprintf(context->output, "%s", lit->literal.bool_value ? "true" : "false");
        break;
    case LITERAL_CHAR:
        fprintf(context->output, "'%c'", lit->literal.char_value);
        break;
    case LITERAL_NULL:
        fprintf(context->output, "null");
        break;
    }
}

static void _print_expr(struct AstPrintContext* context, AstExpr* expr)
{
    if (!expr) {
        fprintf(context->output, "<null>");
        return;
    }

    switch (expr->base.kind) {
    case AST_KIND_LITERAL:
        _print_literal(context, (AstLiteral*)expr);
        break;
    case AST_KIND_IDENT:
        fprintf(context->output, "%s", ((AstIdent*)expr)->name);
        break;
    case AST_KIND_BINARY: {
        AstBinary* bin = (AstBinary*)expr;
        _print_expr(context, bin->left);
        fprintf(context->output, " %s ", _token_type_to_string(bin->op));
        _print_expr(context, bin->right);
        break;
    }
    case AST_KIND_UNARY: {
        AstUnary* un = (AstUnary*)expr;
        fprintf(context->output, "%s", _token_type_to_string(un->op));
        _print_expr(context, un->operand);
        break;
    }
    case AST_KIND_CALL: {
        AstCall* call = (AstCall*)expr;
        _print_expr(context, call->callee);
        fprintf(context->output, "(");
        bool first = true;
        ast_foreach_call_args(call, arg)
        {
            if (!first) {
                fprintf(context->output, ", ");
            }
            _print_expr(context, arg);
            first = false;
        }
        ast_foreach_end;
        fprintf(context->output, ")");
        break;
    }
    case AST_KIND_MEMBER: {
        AstMember* member = (AstMember*)expr;
        _print_expr(context, member->object);
        fprintf(context->output, ".%s", member->member_name);
        break;
    }
    case AST_KIND_INDEX: {
        AstIndex* idx = (AstIndex*)expr;
        _print_expr(context, idx->array);
        fprintf(context->output, "[");
        _print_expr(context, idx->index);
        fprintf(context->output, "]");
        break;
    }
    case AST_KIND_INIT:
        fprintf(context->output, "{ ... }");
        break;
    case AST_KIND_CAST: {
        AstCast* cast = (AstCast*)expr;
        fprintf(context->output, "(");
        _print_type(context, cast->target_type);
        fprintf(context->output, ")");
        _print_expr(context, cast->expr_);
        break;
    }
    case AST_KIND_FFI: {
        AstFfi* ffi = (AstFfi*)expr;
        fprintf(context->output, "$%s(", ffi->function_name);
        if (ffi->args) {
            bool first = true;
            ast_foreach_ffi_args(ffi, arg)
            {
                if (!first) {
                    fprintf(context->output, ", ");
                }
                _print_expr(context, arg);
                first = false;
            }
            ast_foreach_end;
        }
        fprintf(context->output, ")");
        break;
    }
    default:
        fprintf(context->output, "<expr>");
        break;
    }
}

static bool _print_visitor(AstNode* node, void* context, uint32_t depth)
{
    struct AstPrintContext* print_context = (struct AstPrintContext*)context;
    print_context->indent_level = (int)depth;
    _print_indent(print_context);

    fprintf(print_context->output, "%s", ii_ast_kind_name(node->kind));

    switch (node->kind) {
    case AST_KIND_PROGRAM: {
        AstProgram* prog = (AstProgram*)node;
        fprintf(print_context->output, " (errors: %u)", prog->error_count);
        size_t func_count = prog->funcs ? uf_con_vector_length(prog->funcs) : 0;
        size_t bp_count = prog->blueprints ? uf_con_vector_length(prog->blueprints) : 0;
        fprintf(print_context->output, " [funcs: %zu, blueprints: %zu]", func_count, bp_count);
        fprintf(print_context->output, "\n");
        break;
    }
    case AST_KIND_FUNC_DECL: {
        AstFuncDecl* func = (AstFuncDecl*)node;
        fprintf(print_context->output, " %s(", func->name);
        ast_foreach_params(func, param)
        {
            if (param != *(AstParam**)uf_con_vector_get(func->params, 0)) {
                fprintf(print_context->output, ", ");
            }
            fprintf(print_context->output, "%s: ", param->name);
            _print_type(print_context, param->type);
        }
        ast_foreach_end;
        fprintf(print_context->output, ")");
        if (func->return_type) {
            fprintf(print_context->output, " -> ");
            _print_type(print_context, func->return_type);
        }
        break;
    }
    case AST_KIND_BLUEPRINT_DECL: {
        AstBlueprintDecl* bp = (AstBlueprintDecl*)node;
        fprintf(print_context->output, " %s", bp->name);
        if (bp->parents && uf_con_vector_length(bp->parents) > 0) {
            fprintf(print_context->output, " : ");
            bool first = true;
            ast_foreach_parents(bp, inh)
            {
                if (!first)
                    fprintf(print_context->output, ", ");
                fprintf(print_context->output, "%s", inh->parent_name);
                first = false;
            }
            ast_foreach_end;
        }
        size_t field_count = bp->fields ? uf_con_vector_length(bp->fields) : 0;
        size_t method_count = bp->methods ? uf_con_vector_length(bp->methods) : 0;
        fprintf(print_context->output, " [fields: %zu, methods: %zu]", field_count, method_count);
        fprintf(print_context->output, "\n");
        break;
    }
    case AST_KIND_FIELD: {
        AstField* field = (AstField*)node;
        fprintf(print_context->output, " %s: ", field->name);
        _print_type(print_context, field->type);
        if (field->default_value) {
            fprintf(print_context->output, " = ");
            _print_expr(print_context, field->default_value);
        }
        fprintf(print_context->output, "\n");
        break;
    }
    case AST_KIND_METHOD: {
        AstMethod* method = (AstMethod*)node;
        fprintf(print_context->output, " %s", method->name);
        size_t ov_count = method->overloads ? uf_con_vector_length(method->overloads) : 0;
        fprintf(print_context->output, " [overloads: %zu]\n", ov_count);
        break;
    }
    case AST_KIND_METHOD_OVERLOAD: {
        AstMethodOverload* ov = (AstMethodOverload*)node;
        fprintf(print_context->output, " %sstatic: %s", ov->is_static ? "" : "non-",
                ov->is_static ? "true" : "false");
        size_t param_count = ov->params ? uf_con_vector_length(ov->params) : 0;
        fprintf(print_context->output, " [params: %zu]", param_count);
        if (ov->return_type) {
            fprintf(print_context->output, " -> ");
            _print_type(print_context, ov->return_type);
        }
        fprintf(print_context->output, "\n");
        break;
    }
    case AST_KIND_PARAM: {
        AstParam* param = (AstParam*)node;
        fprintf(print_context->output, " %s: ", param->name);
        _print_type(print_context, param->type);
        fprintf(print_context->output, "\n");
        break;
    }
    case AST_KIND_INHERIT: {
        AstInherit* inh = (AstInherit*)node;
        fprintf(print_context->output, " %s\n", inh->parent_name);
        break;
    }
    case AST_KIND_BLOCK: {
        AstBlock* block = (AstBlock*)node;
        size_t stmt_count = block->stmts ? uf_con_vector_length(block->stmts) : 0;
        fprintf(print_context->output, " [statements: %zu]\n", stmt_count);
        break;
    }
    case AST_KIND_RETURN: {
        AstReturn* ret = (AstReturn*)node;
        fprintf(print_context->output, " ");
        _print_expr(print_context, ret->value);
        fprintf(print_context->output, "\n");
        break;
    }
    case AST_KIND_DECL: {
        AstDecl* decl = (AstDecl*)node;
        fprintf(print_context->output, " %s%s: ", decl->is_var ? "var " : "", decl->name);
        _print_type(print_context, decl->type);
        if (decl->init) {
            fprintf(print_context->output, " = ");
            _print_expr(print_context, decl->init);
        }
        fprintf(print_context->output, "\n");
        break;
    }
    case AST_KIND_IF: {
        AstIf* if_stmt = (AstIf*)node;
        fprintf(print_context->output, " ");
        _print_expr(print_context, if_stmt->condition);
        fprintf(print_context->output, "\n");
        break;
    }
    case AST_KIND_FOR: {
        AstFor* for_stmt = (AstFor*)node;
        if (for_stmt->init) {
            fprintf(print_context->output, " INIT:");
        }
        if (for_stmt->condition) {
            fprintf(print_context->output, " COND:");
        }
        if (for_stmt->iter) {
            fprintf(print_context->output, " ITER:");
        }
        fprintf(print_context->output, "\n");
        break;
    }
    case AST_KIND_WHILE: {
        AstWhile* while_stmt = (AstWhile*)node;
        fprintf(print_context->output, " ");
        _print_expr(print_context, while_stmt->condition);
        fprintf(print_context->output, "\n");
        break;
    }
    case AST_KIND_DO_WHILE: {
        AstDoWhile* do_while = (AstDoWhile*)node;
        fprintf(print_context->output, " ");
        _print_expr(print_context, do_while->condition);
        fprintf(print_context->output, "\n");
        break;
    }
    case AST_KIND_BREAK:
    case AST_KIND_CONTINUE:
        fprintf(print_context->output, "\n");
        break;
    case AST_KIND_EXPR_STMT: {
        AstExprStmt* expr_stmt = (AstExprStmt*)node;
        fprintf(print_context->output, " ");
        _print_expr(print_context, expr_stmt->expr);
        fprintf(print_context->output, "\n");
        break;
    }
    case AST_KIND_LITERAL: {
        AstLiteral* lit = (AstLiteral*)node;
        fprintf(print_context->output, " ");
        _print_literal(print_context, lit);
        fprintf(print_context->output, "\n");
        break;
    }
    case AST_KIND_IDENT: {
        AstIdent* ident = (AstIdent*)node;
        fprintf(print_context->output, " %s\n", ident->name);
        break;
    }
    case AST_KIND_BINARY:
    case AST_KIND_UNARY:
    case AST_KIND_CALL:
    case AST_KIND_MEMBER:
    case AST_KIND_INDEX: {
        fprintf(print_context->output, " ");
        _print_expr(print_context, (AstExpr*)node);
        fprintf(print_context->output, "\n");
        break;
    }
    case AST_KIND_INIT: {
        AstInit* init = (AstInit*)node;
        size_t val_count = init->values ? uf_con_vector_length(init->values) : 0;
        size_t named_count = init->named ? uf_con_vector_length(init->named) : 0;
        size_t indexed_count = init->indexed ? uf_con_vector_length(init->indexed) : 0;
        fprintf(print_context->output, " {values: %zu, named: %zu, indexed: %zu}\n", val_count, named_count,
                indexed_count);
        break;
    }
    case AST_KIND_CAST:
        fprintf(print_context->output, "\n");
        break;
    case AST_KIND_FFI: {
        AstFfi* ffi = (AstFfi*)node;
        fprintf(print_context->output, " $%s", ffi->function_name);
        fprintf(print_context->output, "\n");
        break;
    }
    case AST_KIND_TYPE_PRIMITIVE: {
        AstType* type = (AstType*)node;
        fprintf(print_context->output, " %s\n",
                _primitive_type_to_string(type->variant_u.primitive.prim_type));
        break;
    }
    case AST_KIND_TYPE_POINTER: {
        fprintf(print_context->output, " *\n");
        break;
    }
    case AST_KIND_TYPE_ARRAY: {
        AstType* type = (AstType*)node;
        fprintf(print_context->output, " []");
        if (type->variant_u.array.size_expr) {
            fprintf(print_context->output, " [fixed]");
        }
        fprintf(print_context->output, "\n");
        break;
    }
    case AST_KIND_TYPE_BLUEPRINT: {
        AstType* type = (AstType*)node;
        fprintf(print_context->output, " %s\n", type->variant_u.blueprint.name);
        break;
    }
    case AST_KIND_TYPE_ANON: {
        AstType* type = (AstType*)node;
        fprintf(print_context->output, " [fields: %zu]\n", type->variant_u.anon.field_count);
        break;
    }
    default:
        fprintf(print_context->output, "\n");
        break;
    }

    return true;
}

/*
 * AST creation, destruction, and utility functions.
 */

Ast* ii_ast_new(Source* source)
{
    Ast* ast = uf_mem_zalloc(sizeof(Ast));
    ast->source = source;
    ast->arena = uf_mem_region_new(4096);
    ast->program = uf_mem_region_zalloc(ast->arena, sizeof(AstProgram));
    ast->error_count = 0;
    ast->anon_counter = 0;
    return ast;
}

static void _free_expr_vectors(AstExpr* expr);

static void _free_expr_vectors(AstExpr* expr)
{
    if (!expr)
        return;

    switch (expr->base.kind) {
    case AST_KIND_BINARY: {
        AstBinary* bin = (AstBinary*)expr;
        _free_expr_vectors(bin->left);
        _free_expr_vectors(bin->right);
        break;
    }

    case AST_KIND_UNARY: {
        AstUnary* un = (AstUnary*)expr;
        _free_expr_vectors(un->operand);
        break;
    }

    case AST_KIND_CALL: {
        AstCall* call = (AstCall*)expr;
        _free_expr_vectors(call->callee);
        if (call->args) {
            ast_foreach_call_args(call, arg)
            {
                _free_expr_vectors(arg);
            }
            ast_foreach_end;
            uf_con_vector_free(call->args);
        }
        break;
    }

    case AST_KIND_MEMBER: {
        AstMember* member = (AstMember*)expr;
        _free_expr_vectors(member->object);
        break;
    }

    case AST_KIND_INDEX: {
        AstIndex* idx = (AstIndex*)expr;
        _free_expr_vectors(idx->array);
        _free_expr_vectors(idx->index);
        break;
    }

    case AST_KIND_INIT: {
        AstInit* init = (AstInit*)expr;
        if (init->values) {
            ast_foreach_init_values(init, val)
            {
                _free_expr_vectors(val);
            }
            ast_foreach_end;
            uf_con_vector_free(init->values);
        }
        if (init->named) {
            size_t len = uf_con_vector_length(init->named);
            for (size_t i = 0; i < len; i++) {
                void* entry_ptr = uf_con_vector_get(init->named, i);
                AstExpr** value_ptr = (AstExpr**)((char*)entry_ptr + sizeof(const char*));
                _free_expr_vectors(*value_ptr);
            }
            uf_con_vector_free(init->named);
        }
        if (init->indexed) {
            size_t len = uf_con_vector_length(init->indexed);
            for (size_t i = 0; i < len; i++) {
                void* entry_ptr = uf_con_vector_get(init->indexed, i);
                AstExpr** index_ptr = (AstExpr**)entry_ptr;
                AstExpr** value_ptr = (AstExpr**)((char*)entry_ptr + sizeof(AstExpr*));
                _free_expr_vectors(*index_ptr);
                _free_expr_vectors(*value_ptr);
            }
            uf_con_vector_free(init->indexed);
        }
        break;
    }

    case AST_KIND_CAST: {
        AstCast* cast = (AstCast*)expr;
        _free_expr_vectors(cast->expr_);
        break;
    }

    case AST_KIND_FFI: {
        AstFfi* ffi = (AstFfi*)expr;
        if (ffi->args) {
            ast_foreach_ffi_args(ffi, arg)
            {
                _free_expr_vectors(arg);
            }
            ast_foreach_end;
            uf_con_vector_free(ffi->args);
        }
        break;
    }

    default:
        break;
    }
}

static void _free_stmt_vectors(AstStmt* stmt)
{
    if (!stmt)
        return;

    switch (stmt->kind) {
    case AST_KIND_BLOCK: {
        AstBlock* block = (AstBlock*)stmt;
        if (block->stmts) {
            ast_foreach_stmts(block, s)
            {
                _free_stmt_vectors(s);
            }
            ast_foreach_end;
            uf_con_vector_free(block->stmts);
        }
        break;
    }

    case AST_KIND_RETURN: {
        AstReturn* ret = (AstReturn*)stmt;
        _free_expr_vectors(ret->value);
        break;
    }

    case AST_KIND_DECL: {
        AstDecl* decl = (AstDecl*)stmt;
        _free_expr_vectors(decl->init);
        break;
    }

    case AST_KIND_IF: {
        AstIf* if_stmt = (AstIf*)stmt;
        _free_expr_vectors(if_stmt->condition);
        _free_stmt_vectors((AstStmt*)if_stmt->then_block);
        _free_stmt_vectors(if_stmt->else_stmt);
        break;
    }

    case AST_KIND_FOR: {
        AstFor* for_stmt = (AstFor*)stmt;
        _free_stmt_vectors(for_stmt->init);
        _free_expr_vectors(for_stmt->condition);
        _free_expr_vectors(for_stmt->iter);
        _free_stmt_vectors((AstStmt*)for_stmt->body);
        break;
    }

    case AST_KIND_WHILE: {
        AstWhile* while_stmt = (AstWhile*)stmt;
        _free_expr_vectors(while_stmt->condition);
        _free_stmt_vectors((AstStmt*)while_stmt->body);
        break;
    }

    case AST_KIND_DO_WHILE: {
        AstDoWhile* do_while = (AstDoWhile*)stmt;
        _free_stmt_vectors((AstStmt*)do_while->body);
        _free_expr_vectors(do_while->condition);
        break;
    }

    case AST_KIND_EXPR_STMT: {
        AstExprStmt* expr_stmt = (AstExprStmt*)stmt;
        _free_expr_vectors(expr_stmt->expr);
        break;
    }

    default:
        break;
    }
}

static void _free_vectors_in_program(AstProgram* program)
{
    if (!program)
        return;

    for (size_t i = 0; i < uf_con_vector_length(program->funcs); i++) {
        AstFuncDecl* func = *(AstFuncDecl**)uf_con_vector_get(program->funcs, i);
        uf_con_vector_free(func->params);
        _free_stmt_vectors((AstStmt*)func->body);
    }

    for (size_t i = 0; i < uf_con_vector_length(program->blueprints); i++) {
        AstBlueprintDecl* bp = *(AstBlueprintDecl**)uf_con_vector_get(program->blueprints, i);

        for (size_t j = 0; j < uf_con_vector_length(bp->parents); j++) {
            AstInherit* inh = *(AstInherit**)uf_con_vector_get(bp->parents, j);
            uf_con_vector_free(inh->field_aliases);
        }
        uf_con_vector_free(bp->parents);

        for (size_t j = 0; j < uf_con_vector_length(bp->fields); j++) {
            AstField* field = *(AstField**)uf_con_vector_get(bp->fields, j);
            _free_expr_vectors(field->default_value);
        }
        uf_con_vector_free(bp->fields);

        for (size_t j = 0; j < uf_con_vector_length(bp->methods); j++) {
            AstMethod* method = *(AstMethod**)uf_con_vector_get(bp->methods, j);
            for (size_t k = 0; k < uf_con_vector_length(method->overloads); k++) {
                AstMethodOverload* ov = *(AstMethodOverload**)uf_con_vector_get(method->overloads, k);
                uf_con_vector_free(ov->params);
                _free_stmt_vectors((AstStmt*)ov->body);
            }
            uf_con_vector_free(method->overloads);
        }
        uf_con_vector_free(bp->methods);

        uf_con_vector_free(bp->flat_fields);
        uf_con_vector_free(bp->flat_methods);
    }

    uf_con_vector_free(program->funcs);
    uf_con_vector_free(program->blueprints);
}

void ii_ast_free(Ast* ast)
{
    if (!ast || !ast->program) {
        return;
    }

    _free_vectors_in_program(ast->program);

    uf_mem_region_free(ast->arena);
    uf_mem_free(ast);
}

void ii_ast_freep(Ast** ast)
{
    if (!ast || !*ast) {
        return;
    }

    ii_ast_free(*ast);
    *ast = nullptr;
}

void ii_ast_print_debug(const Ast* ast)
{
    if (!ast || !ast->program) {
        return;
    }

    struct AstPrintContext print_context = {.output = stdout, .indent_level = 0};
    ii_ast_visit((Ast*)ast, _print_visitor, &print_context);
}
