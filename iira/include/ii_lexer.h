#pragma once

/**
 * @brief Simple Lexer for the IIRA language.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_diagnostics.h"
#include "ii_source.h"
#include "uf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum LexerSymbolType {
    LEXER_SYM_IDENTIFIER = 0,
    LEXER_SYM_NUMBER,
    LEXER_SYM_STRING,

    /* Keywords */
    LEXER_SYM_KEY_AS,
    LEXER_SYM_KEY_BREAK,
    LEXER_SYM_KEY_CASE,
    LEXER_SYM_KEY_CONTINUE,
    LEXER_SYM_KEY_DEFAULT,
    LEXER_SYM_KEY_DO,
    LEXER_SYM_KEY_ELSE,
    LEXER_SYM_KEY_FOR,
    LEXER_SYM_KEY_IF,
    LEXER_SYM_KEY_RETURN,
    LEXER_SYM_KEY_SWITCH,
    LEXER_SYM_KEY_VAR,
    LEXER_SYM_KEY_WHILE,
};

enum LexerTokenType {
    LEXER_TOK_EOF = 0,
    LEXER_TOK_ERROR,
    LEXER_TOK_SYMBOL,

    /* Punctuation */
    LEXER_TOK_LBRACE,    // {
    LEXER_TOK_RBRACE,    // }
    LEXER_TOK_LPAREN,    // (
    LEXER_TOK_RPAREN,    // )
    LEXER_TOK_LBRACKET,  // [
    LEXER_TOK_RBRACKET,  // ]
    LEXER_TOK_QUESTION,  // ?
    LEXER_TOK_COLON,     // :
    LEXER_TOK_SEMICOLON, // ;
    LEXER_TOK_COMMA,     // ,
    LEXER_TOK_DOT,       // .
    LEXER_TOK_DOLLAR,    // $

    /* Operators */
    LEXER_TOK_ASSIGN,       // =
    LEXER_TOK_PLUS,         // +
    LEXER_TOK_MINUS,        // -
    LEXER_TOK_STAR,         // *
    LEXER_TOK_SLASH,        // /
    LEXER_TOK_PERCENT,      // %
    LEXER_TOK_PLUS_ASSIGN,  // +=
    LEXER_TOK_MINUS_ASSIGN, // -=
    LEXER_TOK_STAR_ASSIGN,  // *=
    LEXER_TOK_SLASH_ASSIGN, // /=
    LEXER_TOK_EQ,           // ==
    LEXER_TOK_NEQ,          // !=
    LEXER_TOK_LT,           // <
    LEXER_TOK_GT,           // >
    LEXER_TOK_LTE,          // <=
    LEXER_TOK_GTE,          // >=
    LEXER_TOK_AND,          // &&
    LEXER_TOK_OR,           // ||
    LEXER_TOK_NOT,          // !
    LEXER_TOK_BIT_AND,      // &
    LEXER_TOK_BIT_OR,       // |
    LEXER_TOK_BIT_XOR,      // ^
    LEXER_TOK_BIT_NOT,      // ~
};

typedef struct LexerSymbol LexerSymbol;
struct LexerSymbol {
    enum LexerSymbolType type;
    const char* text;
};

typedef struct LexerToken LexerToken;
struct LexerToken {
    enum LexerTokenType type;
    SourceSpan span;
    union {
        const struct LexerSymbol* symbol; /* Valid if type is LEXER_TOK_SYMBOL */
        const char* error_message;        /* Valid if type is LEXER_TOK_ERROR */
    };
};

typedef struct LexerContext LexerContext;

LexerContext* ii_lexer_context_new(Source*, DiagnosticContext*) _nodiscard_;
void ii_lexer_context_free(LexerContext*);
void ii_lexer_context_freep(LexerContext**);
#define _autolexer_ _cleanup_(ii_lexer_context_freep)

const struct LexerToken* ii_lexer_get_tokens(const LexerContext*) _nodiscard_;

void ii_lexer_print_debug(const LexerContext*); /* Can be enabled by a flag */
