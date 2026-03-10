#pragma once

/**
 * @brief Abstract Syntax Tree node definitions for IIRA.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_lexer.h"
#include "uf_common.h"
#include "uf_containers.h"
#include "uf_memory.h"

#include <stddef.h>
#include <stdint.h>

/*
 * This enum represents all possible syntax elements in IIRA. Organized by category: declarations, statements,
 * expressions, and types.
 */
enum AstNodeType {
    /* Program-level */
    AST_ROOT, /* Top-level container for all declarations */

    /* Declarations */
    AST_FUNC_DECL,      // name(parameters): return_type = { ... }
    AST_BLUEPRINT_DECL, // name: { ... }
    AST_FIELD,          // name: type [= default];
    AST_METHOD,         // name([self,] parameters): return_type = { ... }
    AST_PARAM,          // name: type

    /* Statements */
    AST_BLOCK,         // { statement1; statement2; ... }
    AST_RETURN_STMT,   // return expression;
    AST_DECL_STMT,     // var name: type [= expression];
    AST_EXPR_STMT,     // statement: expression;
    AST_IF_STMT,       // if (condition) { ... } else { ... }
    AST_FOR_STMT,      // for (statement; condition; statement) { ... }
    AST_WHILE_STMT,    // while (condition) { ... }
    AST_DO_WHILE_STMT, // do { ... } while (condition);
    AST_BREAK_STMT,    // break;
    AST_CONTINUE_STMT, // continue;

    /* Expressions */
    /* TODO: simplified right now, who knows how the expression parser will work. */
    AST_LITERAL,     // Numeric, string, boolean literals, etc.
    AST_IDENT,       // Identifier reference (aka name)
    AST_BINARY_EXPR, // Binary operation: A operator B
    AST_UNARY_EXPR,  // Unary operation: operator A
    AST_CALL_EXPR,   // Function/method call: function(arguments)
    AST_MEMBER_EXPR, // Member access: object.member
    AST_INDEX_EXPR,  // Array subscript: array[expression]
    AST_INIT_EXPR,   // Initialization: { expression, ... } or { field = expression, ... }
    AST_CAST_EXPR,   // Type cast: (blueprint or primitive)expression
    AST_FFI_EXPR,    // FFI call: $function(arguments)

    /* Types (for type annotations) */
    /* TODO: I'm not sure how we'll handle those in semantic analyzer -> they will change. */
    AST_TYPE_PRIMITIVE, // Primitive type: int, float, bool, etc.
    AST_TYPE_ARRAY,     // Array type: type[]
    AST_TYPE_POINTER,   // Pointer type: type*
    AST_TYPE_BLUEPRINT, // Named blueprint reference
    AST_TYPE_ANON,      // Anonymous blueprint: { field: type; ... }
};

/*
 * This is the base AST node. Every node starts with this header for common fields. This allows uniform
 * handling and iterating over all nodes. Another approach (and the original plan) is to use tagged union. The
 * problem with tagged unions is that they get big and messy really quick. This way we keep the structure
 * clearly separated at the cost of few ugly casts.
 */
typedef struct AstNode AstNode;
struct AstNode {
    enum AstNodeType type; /* The specific type of this node */
    SourceSpan span;       /* Source code location for error reporting */
};

/*
 * Declaration union for top-level program declarations.
 */
typedef union AstDeclaration AstDeclaration;
union AstDeclaration {
    enum AstNodeType kind; // Tag to distinguish which member is valid
    struct {
        enum AstNodeType kind;
        struct AstFuncDecl* func;
    } func_decl;
    struct {
        enum AstNodeType kind;
        struct AstBlueprintDecl* blueprint;
    } blueprint_decl;
};

/*
 * This is the top-level container for the full AST. It's parser's job to create it. It should be then
 * passed to semantic analyzer and QBE Graph Builder.
 */
typedef struct Ast Ast;
struct Ast {
    AstNode base;

    Source* source;
    UfMemRegion* node_arena;
    UfConVector* node_vector;

    /* For now, IIRA can have only functions and blueprints as the file root scope.
     * TODO: add support for handling top-level members. */
    AstDeclaration* declarations;
    size_t decl_count;
};

/* IIRA types are represented separately from expressions since they appear in multiple contexts. */
struct AstType {
    AstNode base;

    /* Type kind determines which field is valid */
    enum {
        AST_TYPE_KIND_PRIMITIVE,
        AST_TYPE_KIND_ARRAY,
        AST_TYPE_KIND_POINTER,
        AST_TYPE_KIND_BLUEPRINT,
        AST_TYPE_KIND_ANON,
    } kind;

    /* Semantic phase: type resolution and code generation info */
    bool is_resolved;
    uint32_t size_in_bytes;
    uint32_t alignment;

    union {
        /* For primitive types: int, float, bool, char, etc. */
        struct {
            const char* name;
            enum LexerPrimitiveType prim_type;
        } primitive;

        /* For array types: int[], float[], etc. */
        struct {
            struct AstType* element_type; // The type of array elements
            struct AstExpr* size;         // Optional fixed size: int[10]
        } array;

        /* For pointer types: type* */
        struct {
            struct AstType* pointed_type;
        } pointer;

        /* For named blueprint types: Point, Vehicle, etc. */
        struct {
            const char* name;                  // Blueprint name
            struct AstBlueprintDecl* resolved; // Resolved declaration (semantic phase)
        } blueprint;

        /* For anonymous blueprints: { x: int; y: float; } */
        struct {
            struct AstField** fields; // List of fields
            size_t field_count;
        } anon;
    } variant;
};

/*
 * Expression Nodes
 *
 * All expressions derive from this base. Expressions produce values and can appear on the right-hand side of
 * assignments, as function arguments, etc.
 */

struct AstExpr {
    AstNode base;
    struct AstType* inferred_type; // Type inferred during semantic analysis
};

/* Literal */
struct AstLiteral {
    AstNode base;
    struct AstType* inferred_type;

    enum {
        LITERAL_INT,
        LITERAL_FLOAT,
        LITERAL_STRING,
        LITERAL_BOOL,
        LITERAL_CHAR,
    } kind;

    union {
        int64_t int_value;
        double float_value;
        const char* string_value;
        bool bool_value;
        char char_value;
    } variant;
};

/* Identifier */
struct AstIdent {
    AstNode base;
    struct AstType* inferred_type;

    const char* name;

    /* Semantic analysis: what does this identifier refer to? */
    union {
        struct AstDeclStmt* var_decl;     // Local variable or parameter
        struct AstFuncDecl* func_decl;    // Function reference
        struct AstBlueprintDecl* bp_decl; // Blueprint reference
        struct AstField* field_decl;      // Blueprint field
    } resolved;
};

/* Binary expression */
struct AstBinaryExpr {
    AstNode base;
    struct AstType* inferred_type;

    struct AstExpr* left;
    struct AstExpr* right;
    enum LexerTokenType op; // The operator token type
};

/* Unary expression */
struct AstUnaryExpr {
    AstNode base;
    struct AstType* inferred_type;

    struct AstExpr* operand;
    enum LexerTokenType op;
};

/* Function/method call */
struct AstCallExpr {
    AstNode base;
    struct AstType* inferred_type;

    struct AstExpr* callee; // The function being called
    struct AstExpr** args;  // Arguments passed
    size_t arg_count;
};

/* Member access (e.g., 'object.field') */
struct AstMemberExpr {
    AstNode base;
    struct AstType* inferred_type;

    struct AstExpr* object; // The object being accessed
    const char* member;     // The member name
    bool is_method;         // True if this is a method call (for semantic)
};

/* Array subscript */
struct AstIndexExpr {
    AstNode base;
    struct AstType* inferred_type;

    struct AstExpr* array;
    struct AstExpr* index;
};

/* Initialization */
struct AstInitExpr {
    AstNode base;
    struct AstType* inferred_type;

    struct AstType* target_type; // The type being initialized

    /* For positional initialization (e.g., '{ 1, 2, 3 }') */
    struct AstExpr** values;
    size_t value_count;

    /* For named initialization (e.g., '{ x = 1, y = 2 }') */
    struct {
        const char* name;
        struct AstExpr* value;
    }* named_values;
    size_t named_value_count;

    /* For indexed initialization (e.g., '{ [0] = 1, [2] = 2 }') */
    struct {
        struct AstExpr* index;
        struct AstExpr* value;
    }* indexed_values;
    size_t indexed_value_count;
};

/* Type cast */
struct AstCastExpr {
    AstNode base;
    struct AstType* inferred_type;

    struct AstType* target_type;
    struct AstExpr* expr;
};

/* FFI call */
struct AstFfiExpr {
    AstNode base;
    struct AstType* inferred_type;

    const char* function_name;
    struct AstExpr** args;
    size_t arg_count;
};

/*
 * Now the statement nodes. Statements are executable units that don't produce values. They control flow and
 * side effects.
 */

struct AstStmt {
    AstNode base;
};

/* Block */
struct AstBlock {
    AstNode base;

    struct AstStmt** statements; // Statements in the block
    size_t stmt_count;
    struct AstScope* local_scope; // Symbol table for this block, populated by semantic phase
};

/* Return statement */
struct AstReturnStmt {
    AstNode base;

    struct AstExpr* value; // Value to return, or nullptr for void
    void* exit_block;      // QBE basic block for return jump (filled by codegen)
};

/* Variable declaration */
struct AstDeclStmt {
    AstNode base;

    const char* name;     // Variable name
    struct AstType* type; // Declared type
    struct AstExpr* init; // Optional initializer
    bool is_var;          // true for "var x:", false for "x:" shorthand

    struct AstType* resolved_type;   // Resolved to type (after type checking)
    struct AstVarSlot* codegen_slot; // Code generation: where this variable lives
};

/* Expression statement (for function calls, assignments, etc.) */
struct AstExprStmt {
    AstNode base;

    struct AstExpr* expr;
};

/* If statement */
struct AstIfStmt {
    AstNode base;

    struct AstExpr* condition;
    struct AstBlock* then_block;
    struct AstBlock* else_block; // nullptr if no else clause
};

/* For loop */
struct AstForStmt {
    AstNode base;

    struct AstStmt* init;      // Initialization (usually decl or expr)
    struct AstExpr* condition; // Loop condition
    struct AstExpr* iter;      // Iteration expression
    struct AstBlock* body;

    /* Code generation: control flow targets */
    void* break_target;    // QBE basic block for break
    void* continue_target; // QBE basic block for continue
};

/* While loop */
struct AstWhileStmt {
    AstNode base;

    struct AstExpr* condition;
    struct AstBlock* body;
    bool is_do_while; // true for do-while, false for while

    /* Code generation: control flow targets */
    void* break_target;    // QBE basic block for break
    void* continue_target; // QBE basic block for continue
};

/* Break statement: break; */
struct AstBreakStmt {
    AstNode base;
};

/* Continue statement: continue; */
struct AstContinueStmt {
    AstNode base;

    /* Code generation: target loop */
    void* target_loop; // Pointer to AstForStmt or AstWhileStmt
};

/*
 * Scope / Symbol Table
 *
 * It's used during semantic analysis to track local variables and types, attached to AstBlock nodes.
 */
struct AstScope {
    struct AstScope* parent; // Parent scope (nullptr for global)

    /* Symbol maps - name -> declaration */
    UfConMap* variables; // const char* -> AstDeclStmt*
    UfConMap* types;     // const char* -> AstBlueprintDecl*
    UfConMap* functions; // const char* -> AstFuncDecl*
};

/*
 * Local Variable Slot (for code generation)
 *
 * Tracks where a local variable lives during code generation.
 */
struct AstVarSlot {
    int slot;         // Stack offset or register index
    bool in_register; // True if stored in register
};

/*
 * And those are declaration nodes, aka top-level constructs that define types, functions, and data.
 */

/* Function parameter */
struct AstParam {
    AstNode base;

    const char* name;
    struct AstType* type;
};

/* Function declaration */
struct AstFuncDecl {
    AstNode base;

    const char* name;
    struct AstParam* params; // Function parameters
    size_t param_count;
    struct AstType* return_type; // Return type
    struct AstBlock* body;       // Function body, nullptr for declaration only
};

/* Blueprint field */
struct AstField {
    AstNode base;

    const char* name;
    struct AstType* type;
    struct AstExpr* default_value; // Optional default value
};

/* Blueprint method */
struct AstMethod {
    AstNode base;

    const char* name;
    bool is_static;          // Static method (no self parameter)
    struct AstParam* params; // Parameters including self if not static
    size_t param_count;
    struct AstType* return_type;
    struct AstBlock* body; // nullptr for declaration only
};

/* Blueprint declaration: name: { fields; methods; } */
struct AstBlueprintDecl {
    AstNode base;

    const char* name;

    /* Inheritance */
    struct {
        const char* name;  // Parent blueprint name
        const char* alias; // Optional alias: speed as boatSpeed
    }* parents;
    size_t parent_count;

    /* Members */
    struct AstField* fields;
    size_t field_count;

    struct AstMethod* methods;
    size_t method_count;
};
