/**
 * @brief Expression parser using Pratt top-down operator precedence.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_parser_expr.h"

#include "ii_parser_internal.h"
#include "ii_parser_type.h"

#include <stdlib.h>
#include <string.h>

/*
 * Operator precedence table.
 *
 * Lower values mean lower precedence. Right-associative operators bind tighter on the right.
 * Postfix operators (call, index, member) have precedence higher than all binary ops.
 */
enum {
    PREC_LOWEST = 0,
    PREC_ASSIGN = 1,
    PREC_OR = 2,
    PREC_AND = 3,
    PREC_BITOR = 4,
    PREC_XOR = 5,
    PREC_BITAND = 6,
    PREC_EQ = 7,
    PREC_CMP = 8,
    PREC_SHIFT = 9,
    PREC_ADD = 10,
    PREC_MUL = 11,
    PREC_UNARY = 12,
    PREC_CALL = 13,
    PREC_HIGHEST = 14,
};

/*
 * Precedence info indexed by LexerTokenType.
 * prec=-1 marks tokens that are not operators.
 * right_assoc=true for assignment operators (a = b = c parses as a = (b = c)).
 */
#define _PREC_NOT_OP {-1, false}
#define _PREC(prec) {prec, false}
#define _PREC_R(prec) {prec, true}

_Static_assert(LEXER_TOK_BIT_NOT < 256, "Token enum exceeds array bounds");

static const struct {
    int8_t prec;
    bool right_assoc;
} _prec_info[256] = {
    [LEXER_TOK_EOF] = _PREC_NOT_OP,
    [LEXER_TOK_ERROR] = _PREC_NOT_OP,
    [LEXER_TOK_SYMBOL] = _PREC_NOT_OP,
    [LEXER_TOK_LBRACE] = _PREC_NOT_OP,
    [LEXER_TOK_RBRACE] = _PREC_NOT_OP,
    [LEXER_TOK_LPAREN] = _PREC_NOT_OP,
    [LEXER_TOK_RPAREN] = _PREC_NOT_OP,
    [LEXER_TOK_LBRACKET] = _PREC_NOT_OP,
    [LEXER_TOK_RBRACKET] = _PREC_NOT_OP,
    [LEXER_TOK_QUESTION] = _PREC_NOT_OP,
    [LEXER_TOK_COLON] = _PREC_NOT_OP,
    [LEXER_TOK_SEMICOLON] = _PREC_NOT_OP,
    [LEXER_TOK_COMMA] = _PREC_NOT_OP,
    [LEXER_TOK_DOT] = _PREC_NOT_OP,
    [LEXER_TOK_DOLLAR] = _PREC_NOT_OP,

    [LEXER_TOK_NOT] = _PREC_NOT_OP,
    [LEXER_TOK_BIT_NOT] = _PREC_NOT_OP,

    [LEXER_TOK_ASSIGN] = _PREC_R(PREC_ASSIGN),
    [LEXER_TOK_PLUS_ASSIGN] = _PREC_R(PREC_ASSIGN),
    [LEXER_TOK_MINUS_ASSIGN] = _PREC_R(PREC_ASSIGN),
    [LEXER_TOK_STAR_ASSIGN] = _PREC_R(PREC_ASSIGN),
    [LEXER_TOK_SLASH_ASSIGN] = _PREC_R(PREC_ASSIGN),

    [LEXER_TOK_OR] = _PREC(PREC_OR),
    [LEXER_TOK_AND] = _PREC(PREC_AND),
    [LEXER_TOK_BIT_OR] = _PREC(PREC_BITOR),
    [LEXER_TOK_BIT_XOR] = _PREC(PREC_XOR),
    [LEXER_TOK_BIT_AND] = _PREC(PREC_BITAND),
    [LEXER_TOK_EQ] = _PREC(PREC_EQ),
    [LEXER_TOK_NEQ] = _PREC(PREC_EQ),
    [LEXER_TOK_LT] = _PREC(PREC_CMP),
    [LEXER_TOK_GT] = _PREC(PREC_CMP),
    [LEXER_TOK_LTE] = _PREC(PREC_CMP),
    [LEXER_TOK_GTE] = _PREC(PREC_CMP),
    [LEXER_TOK_PLUS] = _PREC(PREC_ADD),
    [LEXER_TOK_MINUS] = _PREC(PREC_ADD),
    [LEXER_TOK_STAR] = _PREC(PREC_MUL),
    [LEXER_TOK_SLASH] = _PREC(PREC_MUL),
    [LEXER_TOK_PERCENT] = _PREC(PREC_MUL),
};

/*
 * Precedence lookup. Returns -1 for non-operators or invalid token types.
 */
static int _get_prec(enum LexerTokenType op)
{
    size_t count = sizeof(_prec_info) / sizeof(_prec_info[0]);
    if (op < 0 || (size_t)op >= count) {
        return -1;
    }
    if (_prec_info[op].prec == -1) {
        return -1;
    }
    return _prec_info[op].prec;
}

/*
 * Check if operator is right-associative. Only assignment operators are right-associative.
 */
static bool _is_right_assoc(enum LexerTokenType op)
{
    if (op < 0 || (size_t)op >= sizeof(_prec_info) / sizeof(_prec_info[0])) {
        return false;
    }
    return _prec_info[op].right_assoc;
}

/*
 * Postfix operators consume the left-hand side and produce a new expression.
 * These are handled specially in the Pratt loop to avoid recursive descent.
 */
static bool _is_postfix(enum LexerTokenType op)
{
    return op == LEXER_TOK_DOT || op == LEXER_TOK_LPAREN || op == LEXER_TOK_LBRACKET;
}

/*
 * Literal parsing.
 *
 * Detects type suffix (f, F, d, D) for float literals.
 * Falls back to integer if no decimal point or suffix.
 * Advances past the literal token.
 */
static AstExpr* _parse_literal(ParserContext* context, SourceSpan span, const char* text)
{
    TRACE_SCOPE(context->trace);
    size_t len = strlen(text);
    /* Type suffix detection: 1.5f, .5d, 10F */
    if (len > 0 &&
        (text[len - 1] == 'f' || text[len - 1] == 'F' || text[len - 1] == 'd' || text[len - 1] == 'D')) {
        double val = strtod(text, nullptr);
        _advance(context);
        return (AstExpr*)ii_ast_literal_float_sp(context->ast, span, val);
    }
    /* Decimal point detection: 3.14, .5 */
    if (strchr(text, '.') != nullptr) {
        double val = strtod(text, nullptr);
        _advance(context);
        return (AstExpr*)ii_ast_literal_float_sp(context->ast, span, val);
    }
    /* Integer literal */
    int64_t val = strtoll(text, nullptr, 0);
    _advance(context);
    return (AstExpr*)ii_ast_literal_int_sp(context->ast, span, val);
}

/*
 * Identifier parsing. Advances past the identifier token.
 */
static AstExpr* _parse_identifier(ParserContext* context)
{
    TRACE_SCOPE(context->trace);
    SourceSpan span = _span(context);
    const char* name = _sym_text(context);
    _advance(context);
    return (AstExpr*)ii_ast_ident_sp(context->ast, span, name);
}

/*
 * Call arguments parsing.
 *
 * Parses comma-separated expressions between parentheses.
 * Returns call expression with all arguments accumulated.
 *
 * Error recovery: EOF inside argument list triggers error, breaks loop.
 * Missing closing paren is handled by _expect at the end.
 */
static AstExpr* _parse_call_args(ParserContext* context, AstExpr* callee)
{
    TRACE_SCOPE(context->trace);
    AstCall* call = ii_ast_call_sp(context->ast, _span(context), callee);

    while (!_check(context, LEXER_TOK_RPAREN)) {
        /* EOF in middle of argument list - report error, break to expect closing paren */
        if (_check(context, LEXER_TOK_EOF)) {
            _diag_error(context, "Unexpected end of file in argument list");
            break;
        }
        AstExpr* arg = ii_parser_parse_expression(context);
        ii_ast_call_add_arg(call, arg);
        /* Comma separates arguments, no comma means end of argument list */
        if (!_match(context, LEXER_TOK_COMMA)) {
            break;
        }
    }

    _expect(context, LEXER_TOK_RPAREN);
    return (AstExpr*)call;
}

/*
 * Aggregate/struct initialization parsing.
 *
 * Handles three forms:
 * 1. Empty: { }
 * 2. Indexed: { [0] = x, [2] = y }
 * 3. Named: { field = x, other = y }
 * 4. Positional: { x, y, z }
 *
 * Detects form by first token after opening brace.
 */
static AstExpr* _parse_aggregate_init(ParserContext* context, SourceSpan span)
{
    TRACE_SCOPE(context->trace);
    AstInit* init = ii_ast_init_sp(context->ast, span);

    /* Empty aggregate */
    if (_check(context, LEXER_TOK_RBRACE)) {
        _advance(context);
        return (AstExpr*)init;
    }

    /* Indexed initialization: { [expr] = expr, ... } */
    if (_check(context, LEXER_TOK_LBRACKET)) {
        while (!_check(context, LEXER_TOK_RBRACE) && !_check(context, LEXER_TOK_EOF)) {
            _advance(context); /* past [ */
            AstExpr* index = ii_parser_parse_expression(context);
            _expect(context, LEXER_TOK_RBRACKET);
            _expect(context, LEXER_TOK_ASSIGN);
            AstExpr* value = ii_parser_parse_expression(context);
            ii_ast_init_add_indexed(init, index, value);
            if (!_match(context, LEXER_TOK_COMMA)) {
                break;
            }
        }
        _expect(context, LEXER_TOK_RBRACE);
        return (AstExpr*)init;
    }

    /* Named initialization: { name = expr, ... } */
    /* Detection: identifier followed by = (not : which would be type annotation) */
    if (_check_ident(context) && _peek_check(context, 1, LEXER_TOK_ASSIGN)) {
        while (!_check(context, LEXER_TOK_RBRACE) && !_check(context, LEXER_TOK_EOF)) {
            const char* name = _sym_text(context);
            _advance(context);                 /* past identifier */
            _match(context, LEXER_TOK_ASSIGN); /* past = */
            AstExpr* value = ii_parser_parse_expression(context);
            ii_ast_init_add_named(init, name, value);
            if (!_match(context, LEXER_TOK_COMMA)) {
                break;
            }
        }
        _expect(context, LEXER_TOK_RBRACE);
        return (AstExpr*)init;
    }

    /* Positional initialization: { expr, expr, ... } */
    while (!_check(context, LEXER_TOK_RBRACE) && !_check(context, LEXER_TOK_EOF)) {
        AstExpr* value = ii_parser_parse_expression(context);
        ii_ast_init_add_value(init, value);
        if (!_match(context, LEXER_TOK_COMMA)) {
            break;
        }
    }
    _expect(context, LEXER_TOK_RBRACE);
    return (AstExpr*)init;
}

/*
 * Disambiguates cast from grouping.
 *
 * Strategy: Save position after '('. If we can parse a type, and the next token after ')'
 * can start an expression, then it's a cast (type)expr. Otherwise, backtrack and parse
 * as grouping (expr).
 *
 * Example:
 * - (int)x   -> cast (type)x
 * - (a + b)  -> grouping of sum
 * - (int)*x  -> cast to pointer
 */
static AstExpr* _parse_group_or_cast(ParserContext* context)
{
    TRACE_SCOPE(context->trace);
    /* Note: caller already consumed the '(' via _advance in _nud */
    size_t saved = context->current; /* Position at first token inside parens */

    /* Try parsing as type */
    if (_can_parse_as_type(context)) {
        AstType* type = ii_parser_parse_type(context);
        /* Cast confirmed if: closing paren followed by expression-start */
        if (_check(context, LEXER_TOK_RPAREN) && _is_expression_start_ahead(context, 1)) {
            _advance(context); /* past ')' */
            AstExpr* expr = ii_parser_parse_expression(context);
            return (AstExpr*)ii_ast_cast_sp(context->ast, _prev_span(context), type, expr);
        }
    }

    /* Backtrack: not a cast, parse as grouping */
    context->current = saved;
    AstExpr* expr = ii_parser_parse_expression(context);
    _expect(context, LEXER_TOK_RPAREN);
    return expr;
}

/*
 * FFI call parsing ($identifier(args...)).
 *
 * Consumes '$', then identifier, then '('.
 * Parses arguments like regular call arguments.
 *
 * Error recovery: EOF inside FFI arguments triggers error, breaks to closing paren.
 */
static AstExpr* _parse_ffi_call(ParserContext* context)
{
    TRACE_SCOPE(context->trace);
    SourceSpan span = _span(context);
    const char* name = _sym_text(context);
    if (!_expect_ident(context)) {
        return (AstExpr*)ii_ast_error(context->ast, span);
    }

    _expect(context, LEXER_TOK_LPAREN);
    AstFfi* ffi = ii_ast_ffi_sp(context->ast, span, name);

    while (!_check(context, LEXER_TOK_RPAREN)) {
        /* EOF in FFI argument list */
        if (_check(context, LEXER_TOK_EOF)) {
            _diag_error(context, "Unexpected end of file in FFI call");
            break;
        }
        AstExpr* arg = ii_parser_parse_expression(context);
        ii_ast_ffi_add_arg(ffi, arg);
        if (!_match(context, LEXER_TOK_COMMA)) {
            break;
        }
    }

    _expect(context, LEXER_TOK_RPAREN);
    return (AstExpr*)ffi;
}

/*
 * Null denotation (prefix/binding-power-0).
 *
 * Handles tokens that can start an expression.
 * Returns the parsed expression or an error node.
 */
static AstExpr* _nud(ParserContext* context)
{
    TRACE_SCOPE(context->trace);
    /* EOF or error token - return error node */
    if (_check(context, LEXER_TOK_EOF) || _check(context, LEXER_TOK_ERROR)) {
        return (AstExpr*)ii_ast_error(context->ast, _span(context));
    }

    LexerToken* tok = _peek_current(context);

    /* Symbol tokens: identifiers, keywords, literals */
    if (tok->type == LEXER_TOK_SYMBOL) {
        switch (tok->variant.symbol->type) {
        case LEXER_SYM_IDENTIFIER:
            return _parse_identifier(context);
        case LEXER_SYM_KEY_TRUE:
            _advance(context);
            return (AstExpr*)ii_ast_literal_bool_sp(context->ast, _prev_span(context), true);
        case LEXER_SYM_KEY_FALSE:
            _advance(context);
            return (AstExpr*)ii_ast_literal_bool_sp(context->ast, _prev_span(context), false);
        case LEXER_SYM_KEY_NULL:
            _advance(context);
            return (AstExpr*)ii_ast_literal_null_sp(context->ast, _prev_span(context));
        /* self is parsed like identifier - semantic analysis handles verification */
        case LEXER_SYM_KEY_SELF:
            return _parse_identifier(context);
        case LEXER_SYM_NUMBER:
            return _parse_literal(context, _span(context), tok->variant.symbol->text);
        case LEXER_SYM_STRING:
            _advance(context);
            return (AstExpr*)ii_ast_literal_string_sp(context->ast, _prev_span(context),
                                                      tok->variant.symbol->text);
        case LEXER_SYM_CHAR: {
            _advance(context);
            char val = tok->variant.symbol->text[0];
            return (AstExpr*)ii_ast_literal_char_sp(context->ast, _prev_span(context), val);
        }
        default:
            break;
        }
    }

    /* Operators and punctuation */
    switch (tok->type) {
    /* Unary operators: - + ! ~ * & */
    case LEXER_TOK_MINUS:
    case LEXER_TOK_PLUS:
    case LEXER_TOK_NOT:
    case LEXER_TOK_BIT_NOT:
    case LEXER_TOK_STAR:
    case LEXER_TOK_BIT_AND: {
        SourceSpan span = _span(context);
        _advance(context);
        AstExpr* operand = ii_parser_parse_expression(context);
        return (AstExpr*)ii_ast_unary_sp(context->ast, span, tok->type, operand);
    }
    /* FFI call: $identifier(args...) */
    case LEXER_TOK_DOLLAR:
        _advance(context);
        return _parse_ffi_call(context);
    /* Aggregate init: { ... } */
    case LEXER_TOK_LBRACE: {
        SourceSpan span = _span(context);
        _advance(context);
        return _parse_aggregate_init(context, span);
    }
    /* Array literal: [ expr, expr, ... ] */
    case LEXER_TOK_LBRACKET: {
        SourceSpan span = _span(context);
        _advance(context);
        AstInit* init = ii_ast_init_sp(context->ast, span);
        while (!_check(context, LEXER_TOK_RBRACKET) && !_check(context, LEXER_TOK_EOF)) {
            AstExpr* value = ii_parser_parse_expression(context);
            ii_ast_init_add_value(init, value);
            if (!_match(context, LEXER_TOK_COMMA)) {
                break;
            }
        }
        _expect(context, LEXER_TOK_RBRACKET);
        return (AstExpr*)init;
    }
    /* Grouping or cast: ( expr ) or ( type ) expr */
    case LEXER_TOK_LPAREN:
        _advance(context);
        return _parse_group_or_cast(context);
    /* Leading dot for float: .5 becomes 0.5 */
    case LEXER_TOK_DOT:
        _advance(context);
        return _parse_literal(context, _prev_span(context), ".");
    default:
        break;
    }

    /* Unknown token - report error, advance to avoid infinite loop */
    _diag_error(context, "Expected expression but got '%s'", _token_name(tok->type));
    _advance(context);
    return (AstExpr*)ii_ast_error(context->ast, _span(context));
}

/*
 * Left denotation (infix/postfix operators).
 *
 * Handles operators that appear after an expression (left operand).
 * Postfix: . member, ( call, [ index
 * Infix: + - * / etc.
 */
static AstExpr* _led_postfix(ParserContext* context, AstExpr* left, enum LexerTokenType op)
{
    TRACE_SCOPE(context->trace);

    switch (op) {
    /* Member access: expr.member */
    case LEXER_TOK_DOT: {
        if (!_check_ident(context)) {
            _diag_error(context, "Expected member name after '.'");
            _sync_to_stmt(context);
            return (AstExpr*)ii_ast_error(context->ast, _span(context));
        }
        SourceSpan span = _span(context);
        const char* name = _sym_text(context);
        _advance(context);
        return (AstExpr*)ii_ast_member_sp(context->ast, span, left, name);
    }
    /* Function call: expr(args...) */
    case LEXER_TOK_LPAREN:
        return _parse_call_args(context, left);
    /* Index: expr[index] */
    case LEXER_TOK_LBRACKET: {
        SourceSpan span = _span(context);
        /* Empty index is an error */
        if (_check(context, LEXER_TOK_RBRACKET)) {
            _diag_error(context, "Empty index expression");
            _advance(context);
            return (AstExpr*)ii_ast_error(context->ast, span);
        }
        AstExpr* index = ii_parser_parse_expression(context);
        _expect(context, LEXER_TOK_RBRACKET);
        return (AstExpr*)ii_ast_index_sp(context->ast, span, left, index);
    }
    default:
        return left;
    }
}

/*
 * Pratt parser (top-down operator precedence).
 *
 * Core algorithm:
 * 1. Parse prefix expression (atom) via _nud()
 * 2. While next operator has precedence >= min_prec:
 *    a. If postfix, consume and produce new expression
 *    b. If infix, consume operator, parse RHS with adjusted precedence
 * 3. Return accumulated expression
 *
 * Right-associativity: increase min_prec for RHS to allow same-precedence operators to nest on right.
 * Example: a = b = c becomes a = (b = c) because ASSIGN is right-assoc.
 */
static AstExpr* _pratt(ParserContext* context, uint8_t min_prec)
{
    TRACE_SCOPE(context->trace);

    /* EOF or error - bail out early */
    if (_check(context, LEXER_TOK_EOF) || _check(context, LEXER_TOK_ERROR)) {
        return (AstExpr*)ii_ast_error(context->ast, _span(context));
    }

    /* Parse atom/preamble */
    AstExpr* left = _nud(context);

    while (true) {
        enum LexerTokenType op = _peek_current(context)->type;
        int prec = _get_prec(op);

        /* Stop on low precedence, but continue for postfix operators (which have prec=-1) */
        if (prec < 0) {
            if (!_is_postfix(op)) {
                break;
            }
        } else if (prec < min_prec) {
            break;
        }

        /* Right-associative: use prec+1 for RHS to allow nesting */
        uint8_t next_min = _is_right_assoc(op) ? (uint8_t)(prec + 1) : prec;
        _advance(context); /* consume operator */

        /* Postfix operators don't parse RHS - they use the left operand directly */
        if (_is_postfix(op)) {
            left = _led_postfix(context, left, op);
        } else {
            /* Infix: parse RHS with adjusted precedence */
            AstExpr* right = _pratt(context, next_min);
            left = (AstExpr*)ii_ast_binary_sp(context->ast, _prev_span(context), left, op, right);
        }
    }

    return left;
}

AstExpr* ii_parser_parse_expression(ParserContext* context)
{
    TRACE_SCOPE(context->trace);

    return _pratt(context, PREC_LOWEST);
}
