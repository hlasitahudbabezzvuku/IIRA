#pragma once

/**
 * @brief Private semantic analyzer types and helpers shared across all submodules.
 *
 * This is private header meant to be shared across parser submodules. This enables us to keep the
 * implementation "private" while not needing crazy getter/setters for every field in SemanticContext struct.
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast.h"
#include "ii_diagnostics.h"
#include "ii_trace.h"
#include "uf_containers.h"

/*
 * Type definitions.
 */

enum SymbolKind {
    SYMBOL_KIND_VAR,
    SYMBOL_KIND_PARAM,
    SYMBOL_KIND_FUNC,
    SYMBOL_KIND_BLUEPRINT,
    SYMBOL_KIND_FIELD,
    SYMBOL_KIND_SELF,
};

typedef struct SemanticContext SemanticContext;
typedef struct Symbol Symbol;
typedef struct Scope Scope;
typedef struct LoopContext LoopContext;
typedef struct PrimitiveTypeInfo PrimitiveTypeInfo;

struct SemanticContext {
    Source* src;
    DiagnosticContext* diag;
    TraceContext* trace;
    Ast* ast;

    bool had_error;

    Scope* global_scope;
    Scope* current_scope;

    AstBlueprintDecl* current_blueprint;
    AstFuncDecl* current_function;
    AstType* current_return_type;
    LoopContext* loop_stack;

    UfMemRegion* symbol_arena;
};

struct Symbol {
    const char* name;
    enum SymbolKind kind;
    void* decl;
    AstType* type;
    SourceSpan decl_span;
};

struct Scope {
    Scope* parent;
    const char* owner_name;
    UfConMap* symbols;
};

struct LoopContext {
    LoopContext* next;
    AstNode* node;
};

struct PrimitiveTypeInfo {
    uint32_t size;
    uint32_t alignment;
    const char* c_repr;
};

/*
 * Inline type classification helpers.
 */

static inline bool _is_primitive_type(AstType* type)
{
    return type != nullptr && type->tag == AST_TYPE_KIND_PRIMITIVE;
}

static inline bool _is_pointer_type(AstType* type)
{
    return type != nullptr && type->tag == AST_TYPE_KIND_POINTER;
}

static inline bool _is_array_type(AstType* type)
{
    return type != nullptr && type->tag == AST_TYPE_KIND_ARRAY;
}

static inline bool _is_blueprint_type(AstType* type)
{
    return type != nullptr && type->tag == AST_TYPE_KIND_BLUEPRINT;
}

static inline bool _is_arithmetic_prim(enum LexerPrimitiveType prim)
{
    return prim == LEXER_PRIM_INT || prim == LEXER_PRIM_LONG || prim == LEXER_PRIM_SHORT ||
           prim == LEXER_PRIM_FLOAT || prim == LEXER_PRIM_DOUBLE;
}

static inline bool _is_numeric_type(AstType* type)
{
    return _is_primitive_type(type) && _is_arithmetic_prim(ii_ast_type_get_primitive(type));
}

static inline bool _is_integer_prim(enum LexerPrimitiveType prim)
{
    return prim == LEXER_PRIM_INT || prim == LEXER_PRIM_LONG || prim == LEXER_PRIM_SHORT ||
           prim == LEXER_PRIM_CHAR;
}

static inline bool _is_integer_type(AstType* type)
{
    return _is_primitive_type(type) && _is_integer_prim(ii_ast_type_get_primitive(type));
}

static inline bool _is_int_type(AstType* type)
{
    if (!_is_primitive_type(type)) {
        return false;
    }
    enum LexerPrimitiveType prim = ii_ast_type_get_primitive(type);
    return prim == LEXER_PRIM_INT || prim == LEXER_PRIM_LONG || prim == LEXER_PRIM_SHORT;
}

static inline bool _is_float_type(AstType* type)
{
    if (!_is_primitive_type(type)) {
        return false;
    }
    enum LexerPrimitiveType prim = ii_ast_type_get_primitive(type);
    return prim == LEXER_PRIM_FLOAT || prim == LEXER_PRIM_DOUBLE;
}

static inline bool _is_bool_type(AstType* type)
{
    return _is_primitive_type(type) && ii_ast_type_get_primitive(type) == LEXER_PRIM_BOOL;
}

static inline bool _is_void_ptr(AstType* type)
{
    if (!_is_pointer_type(type)) {
        return false;
    }
    AstType* pointed = ii_ast_type_get_pointed(type);
    return pointed != nullptr && _is_primitive_type(pointed) &&
           ii_ast_type_get_primitive(pointed) == LEXER_PRIM_VOID;
}

static inline bool _is_zero_literal(AstExpr* expr)
{
    return expr != nullptr && ii_ast_expr_get_kind(expr) == AST_KIND_LITERAL &&
           ((AstLiteral*)expr)->variant == LITERAL_INT && ((AstLiteral*)expr)->literal.int_value == 0;
}

static inline bool _is_int_literal(AstExpr* expr)
{
    return expr != nullptr && ii_ast_expr_get_kind(expr) == AST_KIND_LITERAL &&
           ((AstLiteral*)expr)->variant == LITERAL_INT;
}

static inline bool _is_null_literal(AstExpr* expr)
{
    return expr != nullptr && ii_ast_expr_get_kind(expr) == AST_KIND_LITERAL &&
           ((AstLiteral*)expr)->variant == LITERAL_NULL;
}

static inline bool _is_valid_condition_type(AstType* type, AstExpr* expr)
{
    return _is_bool_type(type) || _is_int_literal(expr);
}

/*
 * Scope management.
 */

Scope* _scope_new(SemanticContext* context, Scope* parent, const char* owner);
void _scope_push(SemanticContext* context, Scope* scope);
void _scope_pop(SemanticContext* context);

Symbol* _scope_lookup(const Scope* scope, const char* name);
Symbol* _scope_insert(SemanticContext* context, Scope* scope, const char* name, enum SymbolKind kind,
                      void* decl, AstType* type, SourceSpan span);
bool _scope_exists_in_parent(const Scope* scope, const char* name);
Symbol* _scope_lookup_in_chain(const Scope* scope, const char* name);

/*
 * Diagnostic helpers.
 */

#define _diag_error(context, span, ...)                                                                      \
    ({                                                                                                       \
        ii_diag_report((context)->diag, UF_LOG_ERROR, span, __VA_ARGS__);                                    \
        (context)->had_error = true;                                                                         \
    })

/*
 * Type utilities.
 */

bool _types_match(SemanticContext* context, AstType* a, AstType* b);
const char* _type_name(SemanticContext* context, AstType* type);
TastType* _resolve_type(SemanticContext* context, AstType* type);
AstType* _make_self_type(SemanticContext* context, AstBlueprintDecl* bp);

const PrimitiveTypeInfo* _get_prim_info(enum LexerPrimitiveType prim);
const char* _type_to_string(enum LexerPrimitiveType prim);

/*
 * Coercion checking functions.
 */

bool _can_coerce_numeric(AstType* target, AstType* source) _nodiscard_;
bool _can_coerce_expr(SemanticContext* context, AstType* target, AstType* source, AstExpr* expr) _nodiscard_;
bool _can_coerce_for_equality(SemanticContext* context, AstType* left, AstType* right, AstExpr* left_expr,
                              AstExpr* right_expr) _nodiscard_;

/*
 * Analysis phase entry points.
 */

void _analyze_declarations(SemanticContext* context);
void _resolve_inheritance(SemanticContext* context);
void _resolve_blueprint_inheritance(SemanticContext* context, AstBlueprintDecl* bp);
void _resolve_all_types(SemanticContext* context);
void _analyze_function_bodies(SemanticContext* context);

/*
 * Loop context (break/continue validation).
 */

void _enter_loop(SemanticContext* context, AstNode* node);
void _exit_loop(SemanticContext* context);
bool _in_loop(const SemanticContext* context);

/*
 * Expression analysis.
 */

TastExpr* _analyze_expr(SemanticContext* context, AstExpr* expr);
TastExpr* _analyze_literal(SemanticContext* context, AstLiteral* lit);
TastExpr* _analyze_ident(SemanticContext* context, AstIdent* ident);
TastExpr* _analyze_binary(SemanticContext* context, AstBinary* bin);
TastExpr* _analyze_unary(SemanticContext* context, AstUnary* un);
TastExpr* _analyze_call(SemanticContext* context, AstCall* call);
TastExpr* _analyze_member(SemanticContext* context, AstMember* member);
TastExpr* _analyze_index(SemanticContext* context, AstIndex* idx);
TastExpr* _analyze_init(SemanticContext* context, AstInit* init);
TastExpr* _analyze_cast(SemanticContext* context, AstCast* cast);
TastExpr* _analyze_ffi(SemanticContext* context, AstFfi* ffi);

/*
 * Statement analysis.
 */

void _analyze_stmt(SemanticContext* context, AstStmt* stmt);
void _analyze_block(SemanticContext* context, AstBlock* block);
void _analyze_decl_stmt(SemanticContext* context, AstDecl* decl);
void _analyze_return(SemanticContext* context, AstReturn* ret, AstType* expected);
void _analyze_if(SemanticContext* context, AstIf* if_stmt);
void _analyze_for(SemanticContext* context, AstFor* for_stmt);
void _analyze_while(SemanticContext* context, AstWhile* while_stmt);
void _analyze_do_while(SemanticContext* context, AstDoWhile* do_while);
void _analyze_break(SemanticContext* context, AstBreak* brk);
void _analyze_continue(SemanticContext* context, AstContinue* cont);
void _analyze_expr_stmt(SemanticContext* context, AstExprStmt* expr_stmt);
