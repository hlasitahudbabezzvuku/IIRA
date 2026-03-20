/**
 * @brief Type annotation parser.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_parser_type.h"

#include "ii_parser_expr.h"
#include "ii_parser_internal.h"

static AstType* _parse_anon_type(ParserContext* context);

/*
 * Anonymous type parsing.
 *
 * Syntax: { field: Type; ... }
 *
 * Creates unnamed struct-like type with fields.
 * Each field: name: Type (= default)?;
 *
 * Error recovery: EOF triggers sync to '}' or statement boundary.
 */
static AstType* _parse_anon_type(ParserContext* context)
{
    TRACE_SCOPE(context->trace);
    /* Generate unique anonymous name for the type */
    const char* anon_name = _make_anon_name(context);
    AstType* anon = ii_ast_type_anon(context->ast, anon_name);

    while (!_check(context, LEXER_TOK_RBRACE)) {
        /* EOF inside anonymous type */
        if (_check(context, LEXER_TOK_EOF)) {
            _diag_error(context, "Unexpected end of file in anonymous type");
            break;
        }

        /* Expect field name */
        if (!_check_ident(context)) {
            _diag_error(context, "Expected field name in anonymous type");
            _sync_to_stmt(context);
            if (_check(context, LEXER_TOK_RBRACE)) {
                break;
            }
            continue;
        }

        SourceSpan field_span = _span(context);
        const char* field_name = _sym_text(context);
        _advance(context);

        /* Expect colon after field name */
        if (!_expect(context, LEXER_TOK_COLON)) {
            _sync_to_stmt(context);
            if (_check(context, LEXER_TOK_RBRACE)) {
                break;
            }
            continue;
        }

        /* Field type */
        AstType* field_type = ii_parser_parse_type(context);

        /* Optional default value */
        AstExpr* default_val = NULL;
        if (_match(context, LEXER_TOK_ASSIGN)) {
            default_val = ii_parser_parse_expression(context);
        }

        /* Field terminates with semicolon */
        if (!_expect(context, LEXER_TOK_SEMICOLON)) {
            _sync_to_stmt(context);
            if (_check(context, LEXER_TOK_RBRACE)) {
                break;
            }
            continue;
        }

        AstField* field = ii_ast_field_sp(context->ast, field_span, field_name, field_type, default_val);
        ii_ast_type_anon_add_field(context->ast, anon, field);
    }

    _expect(context, LEXER_TOK_RBRACE);
    return anon;
}

/*
 * Type suffix parsing (pointers and arrays).
 *
 * After base type, handles suffixes:
 * - '*' -> pointer type
 * - '[]' -> dynamic array
 * - '[expr]' -> fixed-size array
 *
 * Suffixes chain: int*[]* means pointer to array of pointer to int.
 * Loop handles arbitrary chaining.
 */
static AstType* _parse_type_suffixes(ParserContext* context, AstType* base)
{
    TRACE_SCOPE(context->trace);
    AstType* current = base;

    while (true) {
        /* Pointer suffix: Type* */
        if (_check(context, LEXER_TOK_STAR)) {
            SourceSpan span = _span(context);
            _advance(context);
            current = ii_ast_type_pointer_sp(context->ast, span, current);
        }
        /* Array suffix: Type[] or Type[expr] */
        else if (_check(context, LEXER_TOK_LBRACKET)) {
            SourceSpan bracket_span = _span(context);
            _advance(context);

            /* Empty brackets: dynamic array */
            AstExpr* size_expr = NULL;
            if (!_check(context, LEXER_TOK_RBRACKET)) {
                size_expr = ii_parser_parse_expression(context);
            }

            _expect(context, LEXER_TOK_RBRACKET);
            current = ii_ast_type_array_sp(context->ast, bracket_span, current, size_expr);
        } else {
            /* No more suffixes */
            break;
        }
    }

    return current;
}

/*
 * Type parsing.
 *
 * Parses IIRA type expressions:
 * - Primitive: int, float, bool, char, void, etc.
 * - Blueprint: UserDefined (named type)
 * - Anonymous: { field: Type; ... }
 *
 * After base type, handles pointer/array suffixes.
 *
 * Error recovery: Unknown token reports error, defaults to void.
 */
AstType* ii_parser_parse_type(ParserContext* context)
{
    TRACE_SCOPE(context->trace);

    AstType* base;

    /* Primitive type: int, float, bool, etc. */
    if (_check_prim(context)) {
        SourceSpan span = _span(context);
        enum LexerPrimitiveType prim = _peek_current(context)->variant.symbol->prim_type;
        _advance(context);
        base = ii_ast_type_primitive_sp(context->ast, span, prim);
    }
    /* Named type: identifier */
    else if (_check_ident(context)) {
        SourceSpan span = _span(context);
        const char* name = _sym_text(context);
        _advance(context);
        base = ii_ast_type_blueprint_sp(context->ast, span, name);
    }
    /* Anonymous type: { ... } */
    else if (_match(context, LEXER_TOK_LBRACE)) {
        base = _parse_anon_type(context);
    } else {
        /* Unknown token - report error, default to void */
        _diag_error(context, "Expected type but got '%s'", _token_name(_peek_current(context)->type));
        base = ii_ast_type_primitive_sp(context->ast, _span(context), LEXER_PRIM_VOID);
    }

    /* Handle pointer/array suffixes */
    return _parse_type_suffixes(context, base);
}

/*
 * Type annotation parsing.
 *
 * Wrapper for ii_parser_parse_type for use where IIRA syntax requires
 * a type annotation (e.g., function return types, parameter types).
 */
AstType* ii_parser_parse_type_annotation(ParserContext* context)
{
    TRACE_SCOPE(context->trace);
    return ii_parser_parse_type(context);
}
