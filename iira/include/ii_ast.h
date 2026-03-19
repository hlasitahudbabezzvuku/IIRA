#pragma once

/**
 * @brief Abstract Syntax Tree and Typed AST for iirac.
 *
 * The AST represents the parsed program structure. The TAST (Typed AST) extension is added by the semantic
 * analyzer with type information and resolved symbols.
 *
 * This means that we have to use two types of nodes:
 * - Ast*: Core AST nodes produced by the parser.
 * - Tast*: Typed AST extension added by semantic analyzer.
 *
 * All nodes are allocated from a single memory arena for cache locality. String names ware already interned
 * via Source manager and lexer module for pointer equality.
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_lexer.h"
#include "ii_source.h"
#include "uf_common.h"
#include "uf_containers.h" // IWYU pragma: keep
#include "uf_memory.h"     // IWYU pragma: keep

#include <stddef.h>
#include <stdint.h>

enum AstKind {
    /* Program */
    AST_KIND_PROGRAM,

    /* Types (as nodes for type annotations) */
    AST_KIND_TYPE_PRIMITIVE,
    AST_KIND_TYPE_POINTER,
    AST_KIND_TYPE_ARRAY,
    AST_KIND_TYPE_BLUEPRINT,
    AST_KIND_TYPE_ANON,

    /* Declarations */
    AST_KIND_FUNC_DECL,
    AST_KIND_BLUEPRINT_DECL,

    /* Members */
    AST_KIND_FIELD,
    AST_KIND_METHOD,
    AST_KIND_METHOD_OVERLOAD,
    AST_KIND_PARAM,
    AST_KIND_INHERIT,

    /* Statements */
    AST_KIND_BLOCK,
    AST_KIND_RETURN,
    AST_KIND_DECL,
    AST_KIND_IF,
    AST_KIND_FOR,
    AST_KIND_WHILE,
    AST_KIND_DO_WHILE,
    AST_KIND_BREAK,
    AST_KIND_CONTINUE,
    AST_KIND_EXPR_STMT,

    /* Expressions */
    AST_KIND_LITERAL,
    AST_KIND_IDENT,
    AST_KIND_BINARY,
    AST_KIND_UNARY,
    AST_KIND_CALL,
    AST_KIND_MEMBER,
    AST_KIND_INDEX,
    AST_KIND_INIT,
    AST_KIND_CAST,
    AST_KIND_FFI,

    /* Error placeholder */
    AST_KIND_ERROR,
};

typedef struct Ast Ast;
typedef struct AstNode AstNode;
typedef struct AstType AstType;
typedef struct TastType TastType;

typedef struct AstProgram AstProgram;
typedef struct AstFuncDecl AstFuncDecl;
typedef struct AstBlueprintDecl AstBlueprintDecl;
typedef struct AstField AstField;
typedef struct AstMethod AstMethod;
typedef struct AstMethodOverload AstMethodOverload;
typedef struct AstParam AstParam;
typedef struct AstInherit AstInherit;

typedef struct AstBlock AstBlock;
typedef struct AstReturn AstReturn;
typedef struct AstDecl AstDecl;
typedef struct AstIf AstIf;
typedef struct AstFor AstFor;
typedef struct AstWhile AstWhile;
typedef struct AstDoWhile AstDoWhile;
typedef struct AstBreak AstBreak;
typedef struct AstContinue AstContinue;
typedef struct AstExprStmt AstExprStmt;
typedef union AstStmt AstStmt;

typedef struct AstExpr AstExpr;
typedef struct AstLiteral AstLiteral;
typedef struct AstIdent AstIdent;
typedef struct AstBinary AstBinary;
typedef struct AstUnary AstUnary;
typedef struct AstCall AstCall;
typedef struct AstMember AstMember;
typedef struct AstIndex AstIndex;
typedef struct AstInit AstInit;
typedef struct AstCast AstCast;
typedef struct AstFfi AstFfi;
typedef struct AstError AstError;

typedef struct TastExpr TastExpr;

/*
 * These are the TAST extensions.
 */

enum TastResolutionState {
    TAST_RESOLUTION_UNRESOLVED,
    TAST_RESOLUTION_RESOLVING,
    TAST_RESOLUTION_RESOLVED,
};

/*
 * This is type information added by semantic analyzer.
 */
struct TastType {
    enum TastResolutionState state;
    uint32_t size;
    uint32_t alignment;
    const char* c_repr; /* C representation for QBE (e.g., "l", "d") */
};

/*
 * This is expression type information added by semantic analyzer.
 */
struct TastExpr {
    AstType* resolved_type;
    bool is_lvalue;
    bool is_constant;
    int32_t const_int_value;
};

/*
 * And those are the base AST nodes.
 */

/*
 * Base AST node header. Note that all AST nodes start with this header for uniform handling.
 */
struct AstNode {
    enum AstKind kind;
    SourceSpan span;
};

/*
 * Type node for type annotations. It's used in variable declarations, function parameters, field definitions.
 * TAST extension is populated during semantic analysis.
 */
struct AstType {
    AstNode base;

    /* TAST extension populated by semantic analyzer. */
    TastType* tast;

    /* Type variant */
    enum {
        AST_TYPE_KIND_PRIMITIVE,
        AST_TYPE_KIND_POINTER,
        AST_TYPE_KIND_ARRAY,
        AST_TYPE_KIND_BLUEPRINT,
        AST_TYPE_KIND_ANON,
    } variant;

    union {
        /* Primitive types: int, float, bool, char, etc. */
        struct {
            enum LexerPrimitiveType prim_type;
        } primitive;

        /* Pointer types: type* */
        struct {
            AstType* pointed_type;
        } pointer;

        /* Array types: type[] or type[size] */
        struct {
            AstType* element_type;
            AstExpr* size_expr;  // nullptr for dynamic [], fixed for [n]
            uint32_t fixed_size; // Valid if size_expr is nullptr and is_resolved
        } array;

        /* Named blueprint types: Point, Vehicle, etc. */
        struct {
            const char* name;           // Interned
            AstBlueprintDecl* resolved; // Resolved in semantic
        } blueprint;

        /* Anonymous blueprint: { x: int; y: float; } */
        struct {
            AstField* fields;
            size_t field_count;
            const char* auto_name; // e.g., "__anon_0"
        } anon;
    } variant_u;
};

/*
 * Those are declaration nodes. I think that the name is self explanatory.
 */

/*
 * Top-level program container.
 */
struct AstProgram {
    AstNode base;

    /* Declarations */
    UfConVector* funcs;
    UfConVector* blueprints;

    /* Error tracking */
    uint32_t error_count;
};

/*
 * Function declaration. Note that body is nullptr for declaration-only (like C forward declarations).
 */
struct AstFuncDecl {
    AstNode base;

    const char* name; // Interned
    UfConVector* params;
    AstType* return_type;
    AstBlock* body; // nullptr = declaration only
};

/*
 * Inheritance specification for blueprint. It represents one parent in "Child: Parent1, Parent2 { ... }"
 */
struct AstInherit {
    AstNode base;

    const char* parent_name;    // Interned
    AstBlueprintDecl* resolved; // Resolved in semantic analyzer

    /* Field aliases: "speed as boatSpeed" */
    UfConVector* field_aliases;
};

/*
 * Blueprint (class-like) declaration. It represents "Name: { fields; methods; }" or inheritance.
 */
struct AstBlueprintDecl {
    AstNode base;

    const char* name; // Interned

    /* Inheritance */
    UfConVector* parents;

    /* Members (raw - semantic flattens for code generation) */
    UfConVector* fields;
    UfConVector* methods;

    /* TAST extension: populated by semantic analyzer */
    UfConVector* flat_fields;
    UfConVector* flat_methods;
};

/*
 * Blueprint field definition. It represents "name: type;" or "name: type = default;"
 */
struct AstField {
    AstNode base;

    const char* name; // Interned
    AstType* type;
    AstExpr* default_value; // nullptr if no default
};

/*
 * Method overload variant. It represents only one overload with specific parameter types.
 */
struct AstMethodOverload {
    AstNode base;

    bool is_static;
    UfConVector* params;
    AstType* return_type;
    AstBlock* body; // nullptr means declaration only
};

/*
 * Method declaration (container for overloads). It represents "name(params): return_type = { ... }". Multiple
 * overloads are stored as separate overloads.
 */
struct AstMethod {
    AstNode base;

    const char* name; // Interned

    /* Overloads (for method overloading) */
    UfConVector* overloads;
};

/*
 * Function/method parameter.
 */
struct AstParam {
    AstNode base;

    const char* name; // Interned
    AstType* type;
};

/*
 * Those are statement nodes... also quite self explanatory :D.
 */

/*
 * Block statement: { stmt1; stmt2; ... }
 */
struct AstBlock {
    AstNode base;

    UfConVector* stmts;
};

/*
 * Return statement: return expr;
 */
struct AstReturn {
    AstNode base;

    AstExpr* value; // nullptr for void returns
};

/*
 * Variable declaration: var name: type = expr; or name: type = expr;
 */
struct AstDecl {
    AstNode base;

    const char* name; // Interned
    AstType* type;
    AstExpr* init;
    bool is_var; // true for "var x:", false for "x:" shorthand
};

/*
 * If statement: if (cond) then else else
 */
struct AstIf {
    AstNode base;

    AstExpr* condition;
    AstBlock* then_block;
    AstStmt* else_stmt; // AstBlock* or AstIf*
};

/*
 * For loop: for (init; cond; iter) { body }
 */
struct AstFor {
    AstNode base;

    AstStmt* init;
    AstExpr* condition;
    AstExpr* iter;
    AstBlock* body;
};

/*
 * While loop: while (cond) { body }
 */
struct AstWhile {
    AstNode base;

    AstExpr* condition;
    AstBlock* body;
};

/*
 * Do-while loop: do { body } while (cond);
 */
struct AstDoWhile {
    AstNode base;

    AstBlock* body;
    AstExpr* condition;
};

/*
 * Break statement.
 */
struct AstBreak {
    AstNode base;
};

/*
 * Continue statement.
 */
struct AstContinue {
    AstNode base;
};

/*
 * Expression statement: expr;
 */
struct AstExprStmt {
    AstNode base;

    AstExpr* expr;
};

/* Union for statement types */
typedef union AstStmt AstStmt;
union AstStmt {
    enum AstKind kind;
    AstBlock block;
    AstReturn ret;
    AstDecl decl;
    AstIf if_stmt;
    AstFor for_stmt;
    AstWhile while_stmt;
    AstDoWhile do_while;
    AstBreak break_stmt;
    AstContinue continue_stmt;
    AstExprStmt expr_stmt;
};

/*
 * Those are expression nodes... do I really need to make those comments?
 */

/*
 * Base expression node. Note that the TAST extension populated by semantic analyzer.
 */
struct AstExpr {
    AstNode base;

    /* TAST extension: populated by semantic analyzer */
    TastExpr* tast;
};

/*
 * Literal expression: 42, 3.14, "hello", true, 'c'
 */
struct AstLiteral {
    AstExpr expr;

    enum {
        LITERAL_INT,
        LITERAL_FLOAT,
        LITERAL_STRING,
        LITERAL_BOOL,
        LITERAL_CHAR,
        LITERAL_NULL,
    } variant;

    union {
        int64_t int_value;
        double float_value;
        const char* string_value; // Interned
        bool bool_value;
        char char_value;
    } literal;
};

/*
 * Identifier reference.
 */
struct AstIdent {
    AstExpr expr;

    const char* name; // Interned

    /* TAST extension: resolved reference */
    enum {
        AST_IDENT_NONE,
        AST_IDENT_VAR,
        AST_IDENT_PARAM,
        AST_IDENT_FUNC,
        AST_IDENT_BLUEPRINT,
        AST_IDENT_FIELD,
    } resolved_kind;

    union {
        AstDecl* var_decl;
        AstParam* param_decl;
        AstFuncDecl* func_decl;
        AstBlueprintDecl* blueprint_decl;
        AstField* field_decl;
    } resolved;
};

/*
 * Binary expression: a + b, a && b, etc.
 */
struct AstBinary {
    AstExpr expr;

    AstExpr* left;
    AstExpr* right;
    enum LexerTokenType op;
};

/*
 * Unary expression: -a, !a, ~a, +a
 */
struct AstUnary {
    AstExpr expr;

    AstExpr* operand;
    enum LexerTokenType op;
};

/*
 * Function/method call expression.
 */
struct AstCall {
    AstExpr expr;

    AstExpr* callee; // Function expression
    UfConVector* args;

    /* TAST extension: resolved overload */
    AstMethodOverload* resolved_overload;
};

/*
 * Member access: object.member. Note that this also handles method calls (callee is set).
 */
struct AstMember {
    AstExpr expr;

    AstExpr* object;         // The object being accessed
    const char* member_name; // Interned

    /* TAST extension: resolved member */
    bool is_method_call;
    AstField* resolved_field;
    AstMethodOverload* resolved_method;
};

/*
 * Array subscript: array[index]
 */
struct AstIndex {
    AstExpr expr;

    AstExpr* array;
    AstExpr* index;
};

/*
 * Initialization expression: { 1, 2 } or { x = 1, y = 2 }. It's used for blueprint and array initialization.
 */
struct AstInit {
    AstExpr expr;

    /* Positional: { 1, 2, 3 } */
    UfConVector* values;

    /* Named: { x = 1, y = 2 } */
    UfConVector* named;

    /* Indexed: { [0] = 1, [2] = 3 } */
    UfConVector* indexed;

    /* TAST extension: target type */
    AstType* target_type;
};

/*
 * Type cast expression: (type)expr
 */
struct AstCast {
    AstExpr expr;

    AstType* target_type;
    AstExpr* expr_;
};

/*
 * FFI call: $function(args)
 */
struct AstFfi {
    AstExpr expr;

    const char* function_name; // Interned
    UfConVector* args;
};

/*
 * Error placeholder node. Inserted by parser/semantic when errors are found.
 */
struct AstError {
    AstNode base;
};

/*
 * API for interacting with AST.
 */

Ast* ii_ast_new(Source* source) _nodiscard_;
void ii_ast_free(Ast* ast);
void ii_ast_freep(Ast** ast);
#define _autoast_ _cleanup_(ii_ast_freep)

/*
 * Error handling.
 */

void ii_ast_report_error(Ast* ast);
uint32_t ii_ast_get_error_count(const Ast* ast);

/*
 * Program builder API.
 */

AstFuncDecl* ii_ast_add_func(Ast* ast, const char* name);
AstBlueprintDecl* ii_ast_add_blueprint(Ast* ast, const char* name);
void ii_ast_program_finalize(Ast* ast);

void ii_ast_program_add_func(Ast* ast, AstFuncDecl* func);
void ii_ast_program_add_blueprint(Ast* ast, AstBlueprintDecl* blueprint);

/*
 * Type constructors.
 */

AstType* ii_ast_type_new(Ast* ast, enum AstKind kind);
AstType* ii_ast_type_primitive(Ast* ast, enum LexerPrimitiveType prim);
AstType* ii_ast_type_pointer(Ast* ast, AstType* pointed);
AstType* ii_ast_type_array(Ast* ast, AstType* element, AstExpr* size_expr);
AstType* ii_ast_type_blueprint(Ast* ast, const char* name);
AstType* ii_ast_type_anon(Ast* ast, const char* auto_name);

void ii_ast_type_anon_add_field(Ast* ast, AstType* anon, AstField* field);

/*
 * Type constructors (span-aware).
 */

AstType* ii_ast_type_primitive_sp(Ast* ast, SourceSpan span, enum LexerPrimitiveType prim);
AstType* ii_ast_type_pointer_sp(Ast* ast, SourceSpan span, AstType* pointed);
AstType* ii_ast_type_array_sp(Ast* ast, SourceSpan span, AstType* element, AstExpr* size_expr);
AstType* ii_ast_type_blueprint_sp(Ast* ast, SourceSpan span, const char* name);
AstType* ii_ast_type_anon_sp(Ast* ast, SourceSpan span, const char* auto_name);

/*
 * Declaration constructors.
 */

AstParam* ii_ast_param(Ast* ast, const char* name, AstType* type);
AstField* ii_ast_field(Ast* ast, const char* name, AstType* type, AstExpr* default_value);
AstMethodOverload* ii_ast_method_overload(Ast* ast, bool is_static, AstType* return_type);
AstMethod* ii_ast_method(Ast* ast, const char* name);
AstInherit* ii_ast_inherit(Ast* ast, const char* parent_name);
AstBlueprintDecl* ii_ast_blueprint_decl(Ast* ast, const char* name);
AstFuncDecl* ii_ast_func_decl(Ast* ast, const char* name, AstType* return_type);
void ii_ast_func_add_param(AstFuncDecl* func, AstParam* param);

void ii_ast_blueprint_add_field(AstBlueprintDecl* blueprint, AstField* field);
void ii_ast_blueprint_add_method(AstBlueprintDecl* blueprint, AstMethod* method);
void ii_ast_blueprint_add_inherit(AstBlueprintDecl* blueprint, AstInherit* inherit);
void ii_ast_inherit_add_alias(AstInherit* inherit, const char* original, const char* alias);

AstMethod* ii_ast_method_get_or_add(AstBlueprintDecl* blueprint, const char* name);
void ii_ast_method_add_overload(AstMethod* method, AstMethodOverload* overload);
void ii_ast_method_overload_add_param(AstMethodOverload* overload, AstParam* param);

/*
 * Declaration constructors (span-aware).
 */

AstParam* ii_ast_param_sp(Ast* ast, SourceSpan span, const char* name, AstType* type);
AstField* ii_ast_field_sp(Ast* ast, SourceSpan span, const char* name, AstType* type, AstExpr* default_value);
AstMethodOverload* ii_ast_method_overload_sp(Ast* ast, SourceSpan span, bool is_static, AstType* return_type);
AstInherit* ii_ast_inherit_sp(Ast* ast, SourceSpan span, const char* parent_name);
AstBlueprintDecl* ii_ast_blueprint_decl_sp(Ast* ast, SourceSpan span, const char* name);
AstFuncDecl* ii_ast_func_decl_sp(Ast* ast, SourceSpan span, const char* name, AstType* return_type);

/*
 * Statement constructors.
 */

AstBlock* ii_ast_block(Ast* ast);
void ii_ast_block_add_stmt(AstBlock* block, AstStmt* stmt);

AstReturn* ii_ast_return(Ast* ast, AstExpr* value);
AstDecl* ii_ast_decl(Ast* ast, const char* name, AstType* type, AstExpr* init, bool is_var);
AstIf* ii_ast_if(Ast* ast, AstExpr* condition, AstBlock* then_block, AstStmt* else_stmt);
AstFor* ii_ast_for(Ast* ast, AstStmt* init, AstExpr* condition, AstExpr* iter, AstBlock* body);
AstWhile* ii_ast_while(Ast* ast, AstExpr* condition, AstBlock* body);
AstDoWhile* ii_ast_do_while(Ast* ast, AstBlock* body, AstExpr* condition);
AstBreak* ii_ast_break(Ast* ast);
AstContinue* ii_ast_continue(Ast* ast);
AstExprStmt* ii_ast_expr_stmt(Ast* ast, AstExpr* expr);

/*
 * Statement constructors (span-aware).
 */

AstBlock* ii_ast_block_sp(Ast* ast, SourceSpan span);
AstReturn* ii_ast_return_sp(Ast* ast, SourceSpan span, AstExpr* value);
AstDecl* ii_ast_decl_sp(Ast* ast, SourceSpan span, const char* name, AstType* type, AstExpr* init,
                        bool is_var);
AstIf* ii_ast_if_sp(Ast* ast, SourceSpan span, AstExpr* condition, AstBlock* then_block, AstStmt* else_stmt);
AstFor* ii_ast_for_sp(Ast* ast, SourceSpan span, AstStmt* init, AstExpr* condition, AstExpr* iter,
                      AstBlock* body);
AstWhile* ii_ast_while_sp(Ast* ast, SourceSpan span, AstExpr* condition, AstBlock* body);
AstDoWhile* ii_ast_do_while_sp(Ast* ast, SourceSpan span, AstBlock* body, AstExpr* condition);
AstBreak* ii_ast_break_sp(Ast* ast, SourceSpan span);
AstContinue* ii_ast_continue_sp(Ast* ast, SourceSpan span);
AstExprStmt* ii_ast_expr_stmt_sp(Ast* ast, SourceSpan span, AstExpr* expr);

/*
 * Expression constructors.
 */

AstLiteral* ii_ast_literal_int(Ast* ast, int64_t value);
AstLiteral* ii_ast_literal_float(Ast* ast, double value);
AstLiteral* ii_ast_literal_string(Ast* ast, const char* value);
AstLiteral* ii_ast_literal_bool(Ast* ast, bool value);
AstLiteral* ii_ast_literal_char(Ast* ast, char value);
AstLiteral* ii_ast_literal_null(Ast* ast);

AstLiteral* ii_ast_literal_int_sp(Ast* ast, SourceSpan span, int64_t value);
AstLiteral* ii_ast_literal_float_sp(Ast* ast, SourceSpan span, double value);
AstLiteral* ii_ast_literal_string_sp(Ast* ast, SourceSpan span, const char* value);
AstLiteral* ii_ast_literal_bool_sp(Ast* ast, SourceSpan span, bool value);
AstLiteral* ii_ast_literal_char_sp(Ast* ast, SourceSpan span, char value);
AstLiteral* ii_ast_literal_null_sp(Ast* ast, SourceSpan span);

AstIdent* ii_ast_ident(Ast* ast, const char* name);

AstBinary* ii_ast_binary(Ast* ast, AstExpr* left, enum LexerTokenType op, AstExpr* right);
AstUnary* ii_ast_unary(Ast* ast, enum LexerTokenType op, AstExpr* operand);

AstCall* ii_ast_call(Ast* ast, AstExpr* callee);
void ii_ast_call_add_arg(AstCall* call, AstExpr* arg);

AstMember* ii_ast_member(Ast* ast, AstExpr* object, const char* member_name);

AstIndex* ii_ast_index(Ast* ast, AstExpr* array, AstExpr* index);

AstInit* ii_ast_init(Ast* ast);
void ii_ast_init_add_value(AstInit* init, AstExpr* value);
void ii_ast_init_add_named(AstInit* init, const char* name, AstExpr* value);
void ii_ast_init_add_indexed(AstInit* init, AstExpr* index, AstExpr* value);

AstCast* ii_ast_cast(Ast* ast, AstType* target_type, AstExpr* expr);

AstFfi* ii_ast_ffi(Ast* ast, const char* function_name);
void ii_ast_ffi_add_arg(AstFfi* ffi, AstExpr* arg);

/*
 * Expression constructors (span-aware).
 */

AstIdent* ii_ast_ident_sp(Ast* ast, SourceSpan span, const char* name);
AstBinary* ii_ast_binary_sp(Ast* ast, SourceSpan span, AstExpr* left, enum LexerTokenType op, AstExpr* right);
AstUnary* ii_ast_unary_sp(Ast* ast, SourceSpan span, enum LexerTokenType op, AstExpr* operand);
AstCall* ii_ast_call_sp(Ast* ast, SourceSpan span, AstExpr* callee);
AstMember* ii_ast_member_sp(Ast* ast, SourceSpan span, AstExpr* object, const char* member_name);
AstIndex* ii_ast_index_sp(Ast* ast, SourceSpan span, AstExpr* array, AstExpr* index);
AstInit* ii_ast_init_sp(Ast* ast, SourceSpan span);
AstCast* ii_ast_cast_sp(Ast* ast, SourceSpan span, AstType* target_type, AstExpr* expr);
AstFfi* ii_ast_ffi_sp(Ast* ast, SourceSpan span, const char* function_name);

/*
 * Error node constructor.
 */

AstError* ii_ast_error(Ast* ast, SourceSpan span);

/*
 * Traversal helpers.
 */

/**
 * @brief Visitor callback for AST traversal.
 * @param node Current node
 * @param context User context
 * @param depth Current depth in tree (0 = root)
 * @return True to continue traversal, false to stop
 **/
typedef bool (*AstVisitorFn)(AstNode* node, void* context, uint32_t depth);

/**
 * @brief Visits all nodes in pre-order (parent before children).
 * @param ast Program to traverse
 * @param visitor Visitor callback
 * @param context User context
 **/
void ii_ast_visit(Ast* ast, AstVisitorFn visitor, void* context);

/**
 * @brief Visits all nodes in post-order (children before parent).
 * @param ast Program to traverse
 * @param visitor Visitor callback
 * @param context User context
 **/
void ii_ast_visit_reverse(Ast* ast, AstVisitorFn visitor, void* context);

/*
 * Iterator macros. They are ugly as hell (and clang-format isn't helping it), but they massively simplify the
 * task of traversing through the generated AST.
 */

#define AST_FOREACH_DECL(ast, var)                                                                           \
    for (AstProgram* _prog = ii_ast_get_program(ast), *var = NULL;                                           \
         var = NULL, _prog && (_prog->funcs || _prog->blueprints);)                                          \
        for (size_t _i = 0;                                                                                  \
             _i < uf_con_vector_length(_prog->funcs) + uf_con_vector_length(_prog->blueprints) &&            \
             ((_i < uf_con_vector_length(_prog->funcs) &&                                                    \
               (var = (Ast*)(*(AstFuncDecl**)uf_con_vector_get(_prog->funcs, _i)), true)) ||                 \
              (_i >= uf_con_vector_length(_prog->funcs) &&                                                   \
               (var = (Ast*)(*(AstBlueprintDecl**)uf_con_vector_get(                                         \
                    _prog->blueprints, _i - uf_con_vector_length(_prog->funcs))),                            \
               true)));                                                                                      \
             _i++)

#define AST_FOREACH_FUNC(prog, var)                                                                          \
    for (size_t _i = 0; prog && prog->funcs && _i < uf_con_vector_length(prog->funcs) &&                     \
                        (var = *(AstFuncDecl**)uf_con_vector_get(prog->funcs, _i));                          \
         _i++)

#define AST_FOREACH_BLUEPRINT(prog, var)                                                                     \
    for (size_t _i = 0; prog && prog->blueprints && _i < uf_con_vector_length(prog->blueprints) &&           \
                        (var = *(AstBlueprintDecl**)uf_con_vector_get(prog->blueprints, _i));                \
         _i++)

#define AST_FOREACH_PARENT(bp, var)                                                                          \
    for (size_t _i = 0; bp && bp->parents && _i < uf_con_vector_length(bp->parents) &&                       \
                        (var = *(AstInherit**)uf_con_vector_get(bp->parents, _i));                           \
         _i++)

#define AST_FOREACH_FIELD(bp, var)                                                                           \
    for (size_t _i = 0; bp && bp->fields && _i < uf_con_vector_length(bp->fields) &&                         \
                        (var = *(AstField**)uf_con_vector_get(bp->fields, _i));                              \
         _i++)

#define AST_FOREACH_METHOD(bp, var)                                                                          \
    for (size_t _i = 0; bp && bp->methods && _i < uf_con_vector_length(bp->methods) &&                       \
                        (var = *(AstMethod**)uf_con_vector_get(bp->methods, _i));                            \
         _i++)

#define AST_FOREACH_OVERLOAD(method, var)                                                                    \
    for (size_t _i = 0; method && method->overloads && _i < uf_con_vector_length(method->overloads) &&       \
                        (var = *(AstMethodOverload**)uf_con_vector_get(method->overloads, _i));              \
         _i++)

#define AST_FOREACH_PARAM(func, var)                                                                         \
    for (size_t _i = 0; func && func->params && _i < uf_con_vector_length(func->params) &&                   \
                        (var = *(AstParam**)uf_con_vector_get(func->params, _i));                            \
         _i++)

#define AST_FOREACH_STMT(block, var)                                                                         \
    for (size_t _i = 0; block && block->stmts && _i < uf_con_vector_length(block->stmts) &&                  \
                        (var = *(AstStmt**)uf_con_vector_get(block->stmts, _i));                             \
         _i++)

#define AST_FOREACH_CALL_ARG(call, var)                                                                      \
    for (size_t _i = 0; call && call->args && _i < uf_con_vector_length(call->args) &&                       \
                        (var = *(AstExpr**)uf_con_vector_get(call->args, _i));                               \
         _i++)

#define AST_FOREACH_FFI_ARG(ffi, var)                                                                        \
    for (size_t _i = 0; ffi && ffi->args && _i < uf_con_vector_length(ffi->args) &&                          \
                        (var = *(AstExpr**)uf_con_vector_get(ffi->args, _i));                                \
         _i++)

#define AST_FOREACH_INIT_VALUE(init, var)                                                                    \
    for (size_t _i = 0; init && init->values && _i < uf_con_vector_length(init->values) &&                   \
                        (var = *(AstExpr**)uf_con_vector_get(init->values, _i));                             \
         _i++)

#define AST_FOREACH_INIT_NAMED(init, var)                                                                    \
    for (size_t _i = 0; init && init->named && _i < uf_con_vector_length(init->named) &&                     \
                        (var = *(                                                                            \
                             struct {                                                                        \
                                 const char* name;                                                           \
                                 AstExpr* value;                                                             \
                             }**)uf_con_vector_get(init->named, _i));                                        \
         _i++)

#define AST_FOREACH_INIT_INDEXED(init, var)                                                                  \
    for (size_t _i = 0; init && init->indexed && _i < uf_con_vector_length(init->indexed) &&                 \
                        (var = *(                                                                            \
                             struct {                                                                        \
                                 AstExpr* index;                                                             \
                                 AstExpr* value;                                                             \
                             }**)uf_con_vector_get(init->indexed, _i));                                      \
         _i++)

#define AST_FOREACH_CHILD(node, var) /* Implementation-specific iteration based on node type */

/*
 * Utility functions.
 */

Source* ii_ast_get_source(const Ast* ast);
AstProgram* ii_ast_get_program(const Ast* ast);
const char* ii_ast_kind_name(enum AstKind kind);
void ii_ast_print_debug(const Ast* ast);
