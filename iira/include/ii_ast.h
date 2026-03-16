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
 * Base AST node header. Note that all AST nodes start with this header for uniform handling.
 */
struct AstNode {
    enum AstKind kind;
    SourceSpan span;
};

