#pragma once

/**
 * @brief Abstract Syntax Tree and Typed AST for iirac.
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
} AstKind;

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

/*
 * This is type information added by semantic analyzer.
 */
struct TastType {
    bool is_resolved;
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

