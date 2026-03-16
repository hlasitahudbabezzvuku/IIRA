/**
 * @brief Abstract Syntax Tree and Typed AST implementation.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast.h"
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

static void* _alloc_ast(Ast* ast, size_t size)
{
    return uf_mem_region_malloc(ast->arena, size);
}

/*
 * Error tracking.
 */

uint32_t ii_ast_get_error_count(const Ast* ast)
{
    return ast->error_count;
}

/*
 * Program builder API.
 */

AstFuncDecl* ii_ast_add_func(Ast* ast, const char* name)
{
    AstFuncDecl* func = _alloc_ast(ast, sizeof(AstFuncDecl));
    memset(func, 0, sizeof(AstFuncDecl));
    func->base.kind = AST_KIND_FUNC_DECL;
    func->name = name;

    if (ast->program->funcs == NULL) {
        ast->program->funcs = uf_con_vector_new(sizeof(AstFuncDecl*));
    }
    uf_con_vector_push(ast->program->funcs, &func);

    return func;
}

AstBlueprintDecl* ii_ast_add_blueprint(Ast* ast, const char* name)
{
    AstBlueprintDecl* bp = _alloc_ast(ast, sizeof(AstBlueprintDecl));
    memset(bp, 0, sizeof(AstBlueprintDecl));
    bp->base.kind = AST_KIND_BLUEPRINT_DECL;
    bp->name = name;

    if (ast->program->blueprints == NULL) {
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

AstProgram* ii_ast_get_program(const Ast* ast)
{
    return ast->program;
}

/*
 * Type constructors.
 */

AstType* ii_ast_type_new(Ast* ast, enum AstKind kind)
{
    AstType* type = _alloc_ast(ast, sizeof(AstType));
    memset(type, 0, sizeof(AstType));
    type->base.kind = kind;
    type->tast = NULL;
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
    type->variant_u.blueprint.resolved = NULL;
    return type;
}

AstType* ii_ast_type_anon(Ast* ast, const char* auto_name)
{
    AstType* type = ii_ast_type_new(ast, AST_KIND_TYPE_ANON);
    type->variant = AST_TYPE_KIND_ANON;
    type->variant_u.anon.fields = NULL;
    type->variant_u.anon.field_count = 0;
    type->variant_u.anon.auto_name = auto_name;
    return type;
}

/*
 * Declaration constructors.
 */

AstParam* ii_ast_param(Ast* ast, const char* name, AstType* type)
{
    AstParam* param = _alloc_ast(ast, sizeof(AstParam));
    memset(param, 0, sizeof(AstParam));
    param->base.kind = AST_KIND_PARAM;
    param->name = name;
    param->type = type;
    return param;
}

AstField* ii_ast_field(Ast* ast, const char* name, AstType* type, AstExpr* default_value)
{
    AstField* field = _alloc_ast(ast, sizeof(AstField));
    memset(field, 0, sizeof(AstField));
    field->base.kind = AST_KIND_FIELD;
    field->name = name;
    field->type = type;
    field->default_value = default_value;
    return field;
}

AstMethodOverload* ii_ast_method_overload(Ast* ast, bool is_static, AstType* return_type)
{
    AstMethodOverload* overload = _alloc_ast(ast, sizeof(AstMethodOverload));
    memset(overload, 0, sizeof(AstMethodOverload));
    overload->base.kind = AST_KIND_METHOD_OVERLOAD;
    overload->is_static = is_static;
    overload->return_type = return_type;
    overload->params = uf_con_vector_new(sizeof(AstParam*));
    overload->body = NULL;
    return overload;
}

AstMethod* ii_ast_method(Ast* ast, const char* name)
{
    AstMethod* method = _alloc_ast(ast, sizeof(AstMethod));
    memset(method, 0, sizeof(AstMethod));
    method->base.kind = AST_KIND_METHOD;
    method->name = name;
    method->overloads = uf_con_vector_new(sizeof(AstMethodOverload*));
    return method;
}

AstInherit* ii_ast_inherit(Ast* ast, const char* parent_name)
{
    AstInherit* inherit = _alloc_ast(ast, sizeof(AstInherit));
    memset(inherit, 0, sizeof(AstInherit));
    inherit->base.kind = AST_KIND_INHERIT;
    inherit->parent_name = parent_name;
    inherit->resolved = NULL;
    inherit->field_aliases = uf_con_vector_new(sizeof(struct {
        const char* original;
        const char* alias;
    }));
    return inherit;
}

AstBlueprintDecl* ii_ast_blueprint_decl(Ast* ast, const char* name)
{
    AstBlueprintDecl* blueprint = _alloc_ast(ast, sizeof(AstBlueprintDecl));
    memset(blueprint, 0, sizeof(AstBlueprintDecl));
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
    AstFuncDecl* function = _alloc_ast(ast, sizeof(AstFuncDecl));
    memset(function, 0, sizeof(AstFuncDecl));
    function->base.kind = AST_KIND_FUNC_DECL;
    function->name = name;
    function->return_type = return_type;
    function->params = uf_con_vector_new(sizeof(AstParam*));
    function->body = NULL;
    return function;
}

/*
 * Statement constructors.
 */

AstBlock* ii_ast_block(Ast* ast)
{
    AstBlock* block = _alloc_ast(ast, sizeof(AstBlock));
    memset(block, 0, sizeof(AstBlock));
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
    AstReturn* ret = _alloc_ast(ast, sizeof(AstReturn));
    memset(ret, 0, sizeof(AstReturn));
    ret->base.kind = AST_KIND_RETURN;
    ret->value = value;
    return (AstReturn*)ret;
}

AstDecl* ii_ast_decl(Ast* ast, const char* name, AstType* type, AstExpr* init, bool is_var)
{
    AstDecl* decl = _alloc_ast(ast, sizeof(AstDecl));
    memset(decl, 0, sizeof(AstDecl));
    decl->base.kind = AST_KIND_DECL;
    decl->name = name;
    decl->type = type;
    decl->init = init;
    decl->is_var = is_var;
    return decl;
}

AstIf* ii_ast_if(Ast* ast, AstExpr* condition, AstBlock* then_block, AstStmt* else_stmt)
{
    AstIf* if_stmt = _alloc_ast(ast, sizeof(AstIf));
    memset(if_stmt, 0, sizeof(AstIf));
    if_stmt->base.kind = AST_KIND_IF;
    if_stmt->condition = condition;
    if_stmt->then_block = then_block;
    if_stmt->else_stmt = else_stmt;
    return if_stmt;
}

AstFor* ii_ast_for(Ast* ast, AstStmt* init, AstExpr* condition, AstExpr* iter, AstBlock* body)
{
    AstFor* for_stmt = _alloc_ast(ast, sizeof(AstFor));
    memset(for_stmt, 0, sizeof(AstFor));
    for_stmt->base.kind = AST_KIND_FOR;
    for_stmt->init = init;
    for_stmt->condition = condition;
    for_stmt->iter = iter;
    for_stmt->body = body;
    return for_stmt;
}

AstWhile* ii_ast_while(Ast* ast, AstExpr* condition, AstBlock* body)
{
    AstWhile* while_stmt = _alloc_ast(ast, sizeof(AstWhile));
    memset(while_stmt, 0, sizeof(AstWhile));
    while_stmt->base.kind = AST_KIND_WHILE;
    while_stmt->condition = condition;
    while_stmt->body = body;
    return while_stmt;
}

AstDoWhile* ii_ast_do_while(Ast* ast, AstBlock* body, AstExpr* condition)
{
    AstDoWhile* do_while = _alloc_ast(ast, sizeof(AstDoWhile));
    memset(do_while, 0, sizeof(AstDoWhile));
    do_while->base.kind = AST_KIND_DO_WHILE;
    do_while->body = body;
    do_while->condition = condition;
    return do_while;
}

AstBreak* ii_ast_break(Ast* ast)
{
    AstBreak* brk = _alloc_ast(ast, sizeof(AstBreak));
    memset(brk, 0, sizeof(AstBreak));
    brk->base.kind = AST_KIND_BREAK;
    return brk;
}

AstContinue* ii_ast_continue(Ast* ast)
{
    AstContinue* cont = _alloc_ast(ast, sizeof(AstContinue));
    memset(cont, 0, sizeof(AstContinue));
    cont->base.kind = AST_KIND_CONTINUE;
    return cont;
}

AstExprStmt* ii_ast_expr_stmt(Ast* ast, AstExpr* expr)
{
    AstExprStmt* stmt = _alloc_ast(ast, sizeof(AstExprStmt));
    memset(stmt, 0, sizeof(AstExprStmt));
    stmt->base.kind = AST_KIND_EXPR_STMT;
    stmt->expr = expr;
    return stmt;
}

/*
 * Expression constructors.
 */

AstLiteral* ii_ast_literal_int(Ast* ast, int64_t value)
{
    AstLiteral* lit = _alloc_ast(ast, sizeof(AstLiteral));
    memset(lit, 0, sizeof(AstLiteral));
    lit->expr.base.kind = AST_KIND_LITERAL;
    lit->expr.tast = NULL;
    lit->variant = LITERAL_INT;
    lit->literal.int_value = value;
    return lit;
}

AstLiteral* ii_ast_literal_float(Ast* ast, double value)
{
    AstLiteral* lit = _alloc_ast(ast, sizeof(AstLiteral));
    memset(lit, 0, sizeof(AstLiteral));
    lit->expr.base.kind = AST_KIND_LITERAL;
    lit->expr.tast = NULL;
    lit->variant = LITERAL_FLOAT;
    lit->literal.float_value = value;
    return lit;
}

AstLiteral* ii_ast_literal_string(Ast* ast, const char* value)
{
    AstLiteral* lit = _alloc_ast(ast, sizeof(AstLiteral));
    memset(lit, 0, sizeof(AstLiteral));
    lit->expr.base.kind = AST_KIND_LITERAL;
    lit->expr.tast = NULL;
    lit->variant = LITERAL_STRING;
    lit->literal.string_value = value;
    return lit;
}

AstLiteral* ii_ast_literal_bool(Ast* ast, bool value)
{
    AstLiteral* lit = _alloc_ast(ast, sizeof(AstLiteral));
    memset(lit, 0, sizeof(AstLiteral));
    lit->expr.base.kind = AST_KIND_LITERAL;
    lit->expr.tast = NULL;
    lit->variant = LITERAL_BOOL;
    lit->literal.bool_value = value;
    return lit;
}

AstLiteral* ii_ast_literal_char(Ast* ast, char value)
{
    AstLiteral* lit = _alloc_ast(ast, sizeof(AstLiteral));
    memset(lit, 0, sizeof(AstLiteral));
    lit->expr.base.kind = AST_KIND_LITERAL;
    lit->expr.tast = NULL;
    lit->variant = LITERAL_CHAR;
    lit->literal.char_value = value;
    return lit;
}

AstLiteral* ii_ast_literal_null(Ast* ast)
{
    AstLiteral* lit = _alloc_ast(ast, sizeof(AstLiteral));
    memset(lit, 0, sizeof(AstLiteral));
    lit->expr.base.kind = AST_KIND_LITERAL;
    lit->expr.tast = NULL;
    lit->variant = LITERAL_NULL;
    return lit;
}

AstIdent* ii_ast_ident(Ast* ast, const char* name)
{
    AstIdent* ident = _alloc_ast(ast, sizeof(AstIdent));
    memset(ident, 0, sizeof(AstIdent));
    ident->expr.base.kind = AST_KIND_IDENT;
    ident->expr.tast = NULL;
    ident->name = name;
    ident->resolved_kind = AST_IDENT_NONE;
    return ident;
}

AstBinary* ii_ast_binary(Ast* ast, AstExpr* left, enum LexerTokenType op, AstExpr* right)
{
    AstBinary* bin = _alloc_ast(ast, sizeof(AstBinary));
    memset(bin, 0, sizeof(AstBinary));
    bin->expr.base.kind = AST_KIND_BINARY;
    bin->expr.tast = NULL;
    bin->left = left;
    bin->right = right;
    bin->op = op;
    return bin;
}

AstUnary* ii_ast_unary(Ast* ast, enum LexerTokenType op, AstExpr* operand)
{
    AstUnary* un = _alloc_ast(ast, sizeof(AstUnary));
    memset(un, 0, sizeof(AstUnary));
    un->expr.base.kind = AST_KIND_UNARY;
    un->expr.tast = NULL;
    un->operand = operand;
    un->op = op;
    return un;
}

AstCall* ii_ast_call(Ast* ast, AstExpr* callee)
{
    AstCall* call = _alloc_ast(ast, sizeof(AstCall));
    memset(call, 0, sizeof(AstCall));
    call->expr.base.kind = AST_KIND_CALL;
    call->expr.tast = NULL;
    call->callee = callee;
    call->args = uf_con_vector_new(sizeof(AstExpr*));
    call->resolved_overload = NULL;
    return call;
}

void ii_ast_call_add_arg(AstCall* call, AstExpr* arg)
{
    uf_con_vector_push(call->args, &arg);
}

AstMember* ii_ast_member(Ast* ast, AstExpr* object, const char* member_name)
{
    AstMember* member = _alloc_ast(ast, sizeof(AstMember));
    memset(member, 0, sizeof(AstMember));
    member->expr.base.kind = AST_KIND_MEMBER;
    member->expr.tast = NULL;
    member->object = object;
    member->member_name = member_name;
    member->is_method_call = false;
    member->resolved_field = NULL;
    member->resolved_method = NULL;
    return member;
}

AstIndex* ii_ast_index(Ast* ast, AstExpr* array, AstExpr* index)
{
    AstIndex* idx = _alloc_ast(ast, sizeof(AstIndex));
    memset(idx, 0, sizeof(AstIndex));
    idx->expr.base.kind = AST_KIND_INDEX;
    idx->expr.tast = NULL;
    idx->array = array;
    idx->index = index;
    return idx;
}

AstInit* ii_ast_init(Ast* ast)
{
    AstInit* init = _alloc_ast(ast, sizeof(AstInit));
    memset(init, 0, sizeof(AstInit));
    init->expr.base.kind = AST_KIND_INIT;
    init->expr.tast = NULL;
    init->values = uf_con_vector_new(sizeof(AstExpr*));
    init->named = uf_con_vector_new(sizeof(struct {
        const char* name;
        AstExpr* value;
    }));
    init->indexed = uf_con_vector_new(sizeof(struct {
        AstExpr* index;
        AstExpr* value;
    }));
    init->target_type = NULL;
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
    AstCast* cast = _alloc_ast(ast, sizeof(AstCast));
    memset(cast, 0, sizeof(AstCast));
    cast->expr.base.kind = AST_KIND_CAST;
    cast->expr.tast = NULL;
    cast->target_type = target_type;
    cast->expr_ = expr;
    return cast;
}

AstFfi* ii_ast_ffi(Ast* ast, const char* function_name)
{
    AstFfi* ffi = _alloc_ast(ast, sizeof(AstFfi));
    memset(ffi, 0, sizeof(AstFfi));
    ffi->expr.base.kind = AST_KIND_FFI;
    ffi->expr.tast = NULL;
    ffi->function_name = function_name;
    ffi->args = uf_con_vector_new(sizeof(AstExpr*));
    return ffi;
}

void ii_ast_ffi_add_arg(AstFfi* ffi, AstExpr* arg)
{
    uf_con_vector_push(ffi->args, &arg);
}

/*
 * Error node constructor.
 */

AstError* ii_ast_error(Ast* ast, SourceSpan span)
{
    AstError* err = _alloc_ast(ast, sizeof(AstError));
    memset(err, 0, sizeof(AstError));
    err->base.kind = AST_KIND_ERROR;
    err->base.span = span;
    ast->error_count++;
    return err;
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

    for (size_t i = 0; i < uf_con_vector_length(overload->params); i++) {
        AstParam** p = uf_con_vector_get(overload->params, i);
        _visit_param(*p, visitor, context, depth + 1, pre_order);
    }

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

    for (size_t i = 0; i < uf_con_vector_length(method->overloads); i++) {
        AstMethodOverload** ov = uf_con_vector_get(method->overloads, i);
        _visit_method_overload(*ov, visitor, context, depth + 1, pre_order);
    }

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

    for (size_t i = 0; i < uf_con_vector_length(bp->parents); i++) {
        AstInherit** inh = uf_con_vector_get(bp->parents, i);
        _visit_inherit(*inh, visitor, context, depth + 1, pre_order);
    }

    for (size_t i = 0; i < uf_con_vector_length(bp->fields); i++) {
        AstField** field = uf_con_vector_get(bp->fields, i);
        _visit_field(*field, visitor, context, depth + 1, pre_order);
    }

    for (size_t i = 0; i < uf_con_vector_length(bp->methods); i++) {
        AstMethod** method = uf_con_vector_get(bp->methods, i);
        _visit_method(*method, visitor, context, depth + 1, pre_order);
    }

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

    for (size_t i = 0; i < uf_con_vector_length(func->params); i++) {
        AstParam** param = uf_con_vector_get(func->params, i);
        _visit_param(*param, visitor, context, depth + 1, pre_order);
    }

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
        for (size_t i = 0; i < uf_con_vector_length(call->args); i++) {
            AstExpr** arg = uf_con_vector_get(call->args, i);
            _visit_expr(*arg, visitor, context, depth + 1, pre_order);
        }
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
        for (size_t i = 0; i < uf_con_vector_length(init->values); i++) {
            AstExpr** val = uf_con_vector_get(init->values, i);
            _visit_expr(*val, visitor, context, depth + 1, pre_order);
        }
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
        for (size_t i = 0; i < uf_con_vector_length(ffi->args); i++) {
            AstExpr** arg = uf_con_vector_get(ffi->args, i);
            _visit_expr(*arg, visitor, context, depth + 1, pre_order);
        }
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

    for (size_t i = 0; i < uf_con_vector_length(block->stmts); i++) {
        AstStmt** stmt = uf_con_vector_get(block->stmts, i);
        _visit_stmt(*stmt, visitor, context, depth + 1, pre_order);
    }

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
    for (size_t i = 0; i < uf_con_vector_length(ast->program->funcs); i++) {
        AstFuncDecl** func = uf_con_vector_get(ast->program->funcs, i);
        _visit_func(*func, visitor, context, 1, true);
    }

    /* Visit blueprints. */
    for (size_t i = 0; i < uf_con_vector_length(ast->program->blueprints); i++) {
        AstBlueprintDecl** bp = uf_con_vector_get(ast->program->blueprints, i);
        _visit_blueprint(*bp, visitor, context, 1, true);
    }
}

void ii_ast_visit_reverse(Ast* ast, AstVisitorFn visitor, void* context)
{
    if (!ast || !ast->program || !visitor) {
        return;
    }

    /* Visit functions first (in reverse order). */
    for (size_t i = uf_con_vector_length(ast->program->funcs); i > 0; i--) {
        AstFuncDecl** func = uf_con_vector_get(ast->program->funcs, i - 1);
        _visit_func(*func, visitor, context, 1, false);
    }

    /* Visit blueprints (in reverse order). */
    for (size_t i = uf_con_vector_length(ast->program->blueprints); i > 0; i--) {
        AstBlueprintDecl** bp = uf_con_vector_get(ast->program->blueprints, i - 1);
        _visit_blueprint(*bp, visitor, context, 1, false);
    }

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

/*
 * AST creation, destruction, and utility functions.
 */

Ast* ii_ast_new(Source* source)
{
    Ast* ast = uf_mem_zalloc(sizeof(Ast));
    ast->source = source;
    ast->arena = uf_mem_region_new(4096);
    ast->program = _alloc_ast(ast, sizeof(AstProgram));
    memset(ast->program, 0, sizeof(AstProgram));
    ast->error_count = 0;
    ast->anon_counter = 0;
    return ast;
}

void ii_ast_free(Ast* ast)
{
    if (!ast || !ast->program) {
        return;
    }

    /* Free program-level vectors. */
    uf_con_vector_free(ast->program->funcs);
    uf_con_vector_free(ast->program->blueprints);

    /* Free function vectors. */
    for (size_t i = 0; i < uf_con_vector_length(ast->program->funcs); i++) {
        AstFuncDecl* func = *(AstFuncDecl**)uf_con_vector_get(ast->program->funcs, i);

        if (!func) {
            continue;
        }

        uf_con_vector_free(func->params);
        if (func->body) {
            uf_con_vector_free(func->body->stmts);
        }
    }

    /* Free blueprint vectors. */
    for (size_t i = 0; i < uf_con_vector_length(ast->program->blueprints); i++) {
        AstBlueprintDecl* bp = *(AstBlueprintDecl**)uf_con_vector_get(ast->program->blueprints, i);
        if (!bp) {
            continue;
        }

        uf_con_vector_free(bp->parents);
        uf_con_vector_free(bp->fields);
        uf_con_vector_free(bp->methods);
        uf_con_vector_free(bp->flat_fields);
        uf_con_vector_free(bp->flat_methods);

        /* Free parent inheritance vectors. */
        for (size_t j = 0; j < uf_con_vector_length(bp->parents); j++) {
            AstInherit* inh = *(AstInherit**)uf_con_vector_get(bp->parents, j);
            if (inh) {
                uf_con_vector_free(inh->field_aliases);
            }
        }

        /* Free field vectors. */
        for (size_t j = 0; j < uf_con_vector_length(bp->fields); j++) {
            AstField* field = *(AstField**)uf_con_vector_get(bp->fields, j);
            /* TODO: free field vectors. */
            (void)field;
        }

        /* Free method vectors and their contents. */
        for (size_t j = 0; j < uf_con_vector_length(bp->methods); j++) {
            AstMethod* method = *(AstMethod**)uf_con_vector_get(bp->methods, j);
            if (!method) {
                continue;
            }

            uf_con_vector_free(method->overloads);

            /* Free method overload vectors. */
            for (size_t k = 0; k < uf_con_vector_length(method->overloads); k++) {
                AstMethodOverload* ov = *(AstMethodOverload**)uf_con_vector_get(method->overloads, k);
                if (!ov) {
                    continue;
                }

                uf_con_vector_free(ov->params);
                if (ov->body) {
                    uf_con_vector_free(ov->body->stmts);
                }
            }
        }
    }

    uf_mem_region_free(ast->arena);
    uf_mem_free(ast);
}

void ii_ast_freep(Ast** ast)
{
    if (!ast || !*ast) {
        return;
    }

    ii_ast_free(*ast);
    *ast = NULL;
}

void ii_ast_print_debug(const Ast* ast)
{
    /* TODO: implement. */
    (void)ast;
    printf("AST print not yet implemented\n");
}
