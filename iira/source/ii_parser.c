/**
 * Parser is currently the most sophisticated module in iira. Its job is to transform the token Vector from
 * the lexer into an Abstract Syntax Tree (AST). It uses recursive descent for most of the work with
 * precedence climbing for expressions and method chaining.
 *
 * Since IIRA has quite simple grammar, we can use the same 2 actions like we used for lexing:
 * 1. Advance -> move to (consume) the next Token.
 * 2. Peek -> peek what's the next Token, without consuming it.
 *
 * On parse error, we synchronize to statement/declaration boundary and continue parsing to find more errors.
 * We use placeholder nodes to keep tree structure intact for the semantic analyzer.
 *
 * TODO: if, for, while, and do-while statements are not implemented.
 * TODO: expressions are parsed but not fully built. The expression parser is stub that creates placeholders.
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_parser.h"
#include "ii_ast.h"
#include "ii_diagnostics.h"
#include "ii_lexer.h"
#include "ii_source.h"
#include "uf_containers.h"
#include "uf_logger.h"
#include "uf_memory.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARENA_BLOCK_SIZE (4 * 1024)

struct ParserContext {
    Source* source;
    DiagnosticContext* diag_context;
    const LexerToken* tokens;
    size_t token_count;

    /* Those are owned by the context, not the AST. */
    UfMemRegion* node_arena;
    UfConVector* node_vector;
    Ast* ast;

    size_t current_index;
};

/*
 * I have copied this from ii_lexer.c for error messages. I should probably move it into some shared header.
 * TODO: move the somewhere else.
 */
static const char* _token_type_to_string[] = {
    "LEXER_TOK_EOF",          "LEXER_TOK_ERROR",       "LEXER_TOK_SYMBOL",       "LEXER_TOK_LBRACE",
    "LEXER_TOK_RBRACE",       "LEXER_TOK_LPAREN",      "LEXER_TOK_RPAREN",       "LEXER_TOK_LBRACKET",
    "LEXER_TOK_RBRACKET",     "LEXER_TOK_QUESTION",    "LEXER_TOK_COLON",        "LEXER_TOK_SEMICOLON",
    "LEXER_TOK_COMMA",        "LEXER_TOK_DOT",         "LEXER_TOK_DOLLAR",       "LEXER_TOK_ASSIGN",
    "LEXER_TOK_PLUS",         "LEXER_TOK_MINUS",       "LEXER_TOK_STAR",         "LEXER_TOK_SLASH",
    "LEXER_TOK_PERCENT",      "LEXER_TOK_PLUS_ASSIGN", "LEXER_TOK_MINUS_ASSIGN", "LEXER_TOK_STAR_ASSIGN",
    "LEXER_TOK_SLASH_ASSIGN", "LEXER_TOK_EQ",          "LEXER_TOK_NEQ",          "LEXER_TOK_LT",
    "LEXER_TOK_GT",           "LEXER_TOK_LTE",         "LEXER_TOK_GTE",          "LEXER_TOK_AND",
    "LEXER_TOK_OR",           "LEXER_TOK_NOT",         "LEXER_TOK_BIT_AND",      "LEXER_TOK_BIT_OR",
    "LEXER_TOK_BIT_XOR",      "LEXER_TOK_BIT_NOT",
};

/*
 * These functions provide safe access to the Token Vector/array. They are logically almost the same as the
 * ones in lexer implementation.
 */

static inline bool _is_end(const ParserContext* context)
{
    return context->current_index >= context->token_count ||
           context->tokens[context->current_index].type == LEXER_TOK_EOF;
}

static inline const LexerToken* _peek_current(const ParserContext* context)
{
    if _unlikely_ (_is_end(context)) {
        return &context->tokens[context->token_count - 1]; /* Return EOF */
    }

    return &context->tokens[context->current_index];
}

static inline const LexerToken* _peek_next(const ParserContext* context)
{
    if _unlikely_ (context->current_index + 1 >= context->token_count) {
        return &context->tokens[context->token_count - 1];
    }

    return &context->tokens[context->current_index + 1];
}

static inline const LexerToken* _advance(ParserContext* context)
{
    if _unlikely_ (!_is_end(context)) {
        context->current_index++;
    }

    return &context->tokens[context->current_index - 1];
}

static inline bool _check(const ParserContext* context, enum LexerTokenType type)
{
    if _unlikely_ (_is_end(context)) {
        return false;
    }

    return _peek_current(context)->type == type;
}

static bool _match(ParserContext* context, enum LexerTokenType type)
{
    if (_check(context, type)) {
        _advance(context);
        return true;
    }
    return false;
}

static const LexerToken* _expect(ParserContext* context, enum LexerTokenType type, const char* expected)
{
    if (_check(context, type)) {
        return _advance(context);
    }

    const LexerToken* current = _peek_current(context);
    ii_diag_report(context->diag_context, UF_LOG_ERROR, current->span, "Expected '%s' but found '%s'",
                   expected,
                   current->type == LEXER_TOK_SYMBOL ? current->variant.symbol->text
                                                     : _token_type_to_string[current->type]);

    return current;
}

/**
 * This is the synchronization function. We call it when something goes wrong. It skips tokens until we find a
 * synchronization point. This allows us to report multiple errors per compilation unit.
 *
 * Synchronization points are:
 * - Semicolon
 * - Opening brace
 * - Closing brace
 * - keyword
 * - EOF
 **/
static void _synchronize(ParserContext* context)
{
    while (!_is_end(context)) {
        const LexerToken* token = _peek_current(context);
        enum LexerTokenType type = token->type;

        if (type == LEXER_TOK_SEMICOLON || type == LEXER_TOK_LBRACE || type == LEXER_TOK_RBRACE ||
            type == LEXER_TOK_EOF) {
            break;
        }

        if (type == LEXER_TOK_SYMBOL && token->variant.symbol) {
            enum LexerSymbolType sym_type = token->variant.symbol->type;
            if (sym_type == LEXER_SYM_KEY_VAR || sym_type == LEXER_SYM_KEY_IF ||
                sym_type == LEXER_SYM_KEY_FOR || sym_type == LEXER_SYM_KEY_WHILE ||
                sym_type == LEXER_SYM_KEY_RETURN) {
                break;
            }
        }

        _advance(context);
    }
}

