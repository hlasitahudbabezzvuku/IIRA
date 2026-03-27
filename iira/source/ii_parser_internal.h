#pragma once

/**
 * @brief Private parser types and helpers shared across all parser modules.
 *
 * This is private header meant to be shared across parser submodules. This enables us to keep the
 * implementation "private" while not needing crazy getter/setters for every field in ParserContext struct.
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_diagnostics.h"
#include "ii_parser.h"
#include "ii_trace.h"

struct ParserContext {
    Ast* ast;
    Source* src;
    DiagnosticContext* diag;
    TraceContext* trace;

    LexerToken* tokens;
    size_t count;
    size_t current;

    uint32_t anon_counter;

    bool had_error;
    SourceSpan last_span;
};

/*
 * Token iteration helpers.
 */

static inline LexerToken* _peek_current(const ParserContext* context)
{
    return &context->tokens[context->current];
}

static inline LexerToken* _peek_ahead(const ParserContext* context, size_t ahead)
{
    if (context->current + ahead >= context->count) {
        return &context->tokens[context->count - 1];
    }
    return &context->tokens[context->current + ahead];
}

static inline void _advance(ParserContext* context)
{
    if (context->current < context->count - 1) {
        context->current++;
    }
}

static inline SourceSpan _span(const ParserContext* context)
{
    return context->tokens[context->current].span;
}

static inline bool _check(const ParserContext* context, enum LexerTokenType type)
{
    return _peek_current(context)->type == type;
}

static inline bool _check_sym(const ParserContext* context, enum LexerSymbolType sym)
{
    return _peek_current(context)->type == LEXER_TOK_SYMBOL &&
           _peek_current(context)->variant.symbol->type == sym;
}

static inline bool _check_prim(const ParserContext* context)
{
    return _check_sym(context, LEXER_SYM_PRIMITIVE);
}

static inline bool _match(ParserContext* context, enum LexerTokenType type)
{
    if (_check(context, type)) {
        _advance(context);
        return true;
    }
    return false;
}

static inline bool _check_ident(const ParserContext* context)
{
    return _check(context, LEXER_TOK_SYMBOL) &&
           _peek_current(context)->variant.symbol->type == LEXER_SYM_IDENTIFIER;
}

static inline bool _peek_check(const ParserContext* context, size_t ahead, enum LexerTokenType type)
{
    return _peek_ahead(context, ahead)->type == type;
}

static inline const char* _sym_text(const ParserContext* context)
{
    return _peek_current(context)->variant.symbol->text;
}

/*
 * Additional token helpers for parser.
 */

static inline SourceSpan _prev_span(const ParserContext* context)
{
    return context->tokens[context->current - 1].span;
}

static inline bool _match_sym(ParserContext* context, enum LexerSymbolType sym)
{
    if (_check_sym(context, sym)) {
        _advance(context);
        return true;
    }
    return false;
}

static inline bool _is_expression_start_ahead(const ParserContext* context, size_t ahead)
{
    LexerToken* tok = _peek_ahead(context, ahead);

    if (tok->type == LEXER_TOK_SYMBOL) {
        return true;
    }

    switch (tok->type) {
    case LEXER_TOK_MINUS:
    case LEXER_TOK_PLUS:
    case LEXER_TOK_NOT:
    case LEXER_TOK_BIT_NOT:
    case LEXER_TOK_DOT:
    case LEXER_TOK_DOLLAR:
    case LEXER_TOK_LPAREN:
    case LEXER_TOK_LBRACE:
    case LEXER_TOK_LBRACKET:
        return true;
    default:
        return false;
    }
}

/*
 * Error reporting macro.
 */

#define _diag_error(context, ...)                                                                            \
    ({                                                                                                       \
        SourceSpan _diag_span = _span(context);                                                              \
        (context)->last_span = _diag_span;                                                                   \
        ii_diag_report((context)->diag, UF_LOG_ERROR, _diag_span, __VA_ARGS__);                              \
        (context)->had_error = true;                                                                         \
    })

/*
 * Panic-mode error recovery - sync sets.
 */

static const enum LexerSymbolType _sync_set_stmt_sym[] = {
    LEXER_SYM_KEY_VAR, LEXER_SYM_KEY_IF,     LEXER_SYM_KEY_WHILE, LEXER_SYM_KEY_FOR,
    LEXER_SYM_KEY_DO,  LEXER_SYM_KEY_RETURN, LEXER_SYM_KEY_BREAK, LEXER_SYM_KEY_CONTINUE,
};

static const enum LexerTokenType _sync_set_stmt_tok[] = {
    LEXER_TOK_SEMICOLON,
    LEXER_TOK_LBRACE,
    LEXER_TOK_RBRACE,
    LEXER_TOK_EOF,
};

static const enum LexerTokenType _sync_set_decl_tok[] = {
    LEXER_TOK_SYMBOL,
    LEXER_TOK_EOF,
};

/*
 * Panic-mode error recovery - helpers.
 */

static inline bool _in_sync_set_stmt(const ParserContext* context)
{
    for (size_t i = 0; i < sizeof(_sync_set_stmt_sym) / sizeof(_sync_set_stmt_sym[0]); i++) {
        if (_check_sym(context, _sync_set_stmt_sym[i])) {
            return true;
        }
    }

    for (size_t i = 0; i < sizeof(_sync_set_stmt_tok) / sizeof(_sync_set_stmt_tok[0]); i++) {
        if (_check(context, _sync_set_stmt_tok[i])) {
            return true;
        }
    }

    return false;
}

static inline bool _in_sync_set_decl(const ParserContext* context)
{
    for (size_t i = 0; i < sizeof(_sync_set_decl_tok) / sizeof(_sync_set_decl_tok[0]); i++) {
        if (_check(context, _sync_set_decl_tok[i])) {
            return true;
        }
    }

    return false;
}

static inline void _sync_to_stmt(ParserContext* context)
{
    TRACE_SCOPE(context->trace);

    while (!_in_sync_set_stmt(context) && context->current < context->count - 1) {
        _advance(context);
    }
}

static inline void _sync_to_decl(ParserContext* context)
{
    TRACE_SCOPE(context->trace);

    while (!_in_sync_set_decl(context) && context->current < context->count - 1) {
        _advance(context);
    }
}

/*
 * Token name lookup - for error messages.
 */

static inline const char* _sym_type_name(enum LexerSymbolType type)
{
    switch (type) {
    case LEXER_SYM_IDENTIFIER:
        return "<identifier>";
    case LEXER_SYM_NUMBER:
        return "<number>";
    case LEXER_SYM_STRING:
        return "<string>";
    case LEXER_SYM_CHAR:
        return "<char>";
    case LEXER_SYM_KEY_AS:
        return "as";
    case LEXER_SYM_KEY_BREAK:
        return "break";
    case LEXER_SYM_KEY_CASE:
        return "case";
    case LEXER_SYM_KEY_CONTINUE:
        return "continue";
    case LEXER_SYM_KEY_DEFAULT:
        return "default";
    case LEXER_SYM_KEY_DO:
        return "do";
    case LEXER_SYM_KEY_ELSE:
        return "else";
    case LEXER_SYM_KEY_FALSE:
        return "false";
    case LEXER_SYM_KEY_FOR:
        return "for";
    case LEXER_SYM_KEY_IF:
        return "if";
    case LEXER_SYM_KEY_NULL:
        return "null";
    case LEXER_SYM_KEY_RETURN:
        return "return";
    case LEXER_SYM_KEY_SELF:
        return "self";
    case LEXER_SYM_KEY_SWITCH:
        return "switch";
    case LEXER_SYM_KEY_TRUE:
        return "true";
    case LEXER_SYM_KEY_VAR:
        return "var";
    case LEXER_SYM_KEY_WHILE:
        return "while";
    case LEXER_SYM_PRIMITIVE:
        return "<primitive type>";
    default:
        return "<unknown>";
    }
}

const char* _token_name(enum LexerTokenType type);

/*
 * Expect helpers.
 */

static inline bool _expect(ParserContext* context, enum LexerTokenType expected)
{
    if (_check(context, expected)) {
        _advance(context);
        return true;
    }

    const char* got_name = _token_name(_peek_current(context)->type);
    const char* expected_name = _token_name(expected);
    _diag_error(context, "Expected '%s' but got '%s'", expected_name, got_name);
    _sync_to_stmt(context);
    return false;
}

static inline bool _expect_ident(ParserContext* context)
{
    if (_check_ident(context)) {
        _advance(context);
        return true;
    }

    LexerToken* tok = _peek_current(context);
    if (tok->type == LEXER_TOK_SYMBOL) {
        _diag_error(context, "Expected identifier but got '%s'", _sym_type_name(tok->variant.symbol->type));
    } else {
        _diag_error(context, "Expected identifier but got '%s'", _token_name(tok->type));
    }
    _sync_to_stmt(context);
    return false;
}

/*
 * String interning helpers.
 */

const char* _make_anon_name(ParserContext* context);

/*
 * Disambiguation helpers.
 */

static inline bool _can_parse_as_type(const ParserContext* context)
{
    if (_check_prim(context)) {
        return true;
    }

    if (_check_ident(context)) {
        return true;
    }

    if (_check(context, LEXER_TOK_LBRACE)) {
        return true;
    }

    return false;
}

static inline bool _is_expression_start(const ParserContext* context)
{
    LexerToken* tok = _peek_current(context);

    if (tok->type == LEXER_TOK_SYMBOL) {
        return true;
    }

    switch (tok->type) {
    case LEXER_TOK_MINUS:
    case LEXER_TOK_PLUS:
    case LEXER_TOK_NOT:
    case LEXER_TOK_BIT_NOT:
    case LEXER_TOK_DOT:
    case LEXER_TOK_DOLLAR:
    case LEXER_TOK_LPAREN:
    case LEXER_TOK_LBRACE:
    case LEXER_TOK_LBRACKET:
        return true;
    default:
        return false;
    }
}
