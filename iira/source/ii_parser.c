/**
 * @brief Parser core module.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_parser.h"

#include "ii_parser_decl.h"
#include "ii_parser_expr.h"
#include "ii_parser_stmt.h"
#include "ii_parser_type.h"

#include <stdlib.h>

struct ParserContext {
    Source* source;
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
 * Token stream helpers.
 */

static inline LexerToken* _cur(const ParserContext* context)
{
    return &context->tokens[context->current];
}

static inline LexerToken* _peek(const ParserContext* context, size_t ahead)
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
    return _cur(context)->type == type;
}

static inline bool _check_sym(const ParserContext* context, enum LexerSymbolType sym)
{
    return _cur(context)->type == LEXER_TOK_SYMBOL && _cur(context)->variant.symbol->type == sym;
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
        if (_match(context, types[i]))
            return true;
    }
    return false;
}

/*
 * Context lifecycle.
 */

ParserContext* ii_parser_context_new(const char* file_path, bool trace_enabled)
{
    (void)file_path;
    (void)trace_enabled;
    return NULL;
}

void ii_parser_context_free(ParserContext* context)
{
    (void)context;
}

/*
 * Entry point.
 */

Ast* ii_parser_parse(ParserContext* context)
{
    (void)context;
    return NULL;
}
