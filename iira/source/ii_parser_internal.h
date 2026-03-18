/**
 * @brief Private parser types and helpers shared across all parser modules.
 *
 * This is private header meant to be shared across parser submodules. This enables us to keep the
 * implementation "private" while not needing crazy getter/setters for every field in ParserContext struct.
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#pragma once

#include "ii_parser.h"

#include "ii_trace.h"

struct ParserContext {
    Source* src;
    DiagnosticContext* diag;
    LexerContext* lexer;
    TraceContext* trace;
    Ast* ast;

    LexerToken* tokens;
    size_t count;
    size_t current;

    UfMemRegion* arena;
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
    return &context->tokens[context->current + ahead];
}

static inline void _advance(ParserContext* context)
{
    context->current++;
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

static inline bool _match_any(ParserContext* context, const enum LexerTokenType* types, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (_match(context, types[i])) {
            return true;
        }
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
