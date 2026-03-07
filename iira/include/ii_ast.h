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
    union {
        struct AstFuncDecl* func;
        struct AstBlueprintDecl* blueprint;
    }* declarations;
    size_t decl_count;
};

