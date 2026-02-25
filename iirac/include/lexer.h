#pragma once

/**
 * @brief Simple Lexer for the IIRA language.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "uf_common.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum LexerTokenType {
    LEXER_TOK_EOF = 0,
    LEXER_TOK_ERROR,

    /* Identifiers & Literals */
    LEXER_TOK_IDENTIFIER,
    LEXER_TOK_NUMBER,
    LEXER_TOK_STRING,

    /* Keywords */
    LEXER_TOK_KEY_AS,
    LEXER_TOK_KEY_BREAK,
    LEXER_TOK_KEY_CASE,
    LEXER_TOK_KEY_CONTINUE,
    LEXER_TOK_KEY_DEFAULT,
    LEXER_TOK_KEY_DO,
    LEXER_TOK_KEY_ELSE,
    LEXER_TOK_KEY_FOR,
    LEXER_TOK_KEY_IF,
    LEXER_TOK_KEY_RETURN,
    LEXER_TOK_KEY_SWITCH,
    LEXER_TOK_KEY_VAR,
    LEXER_TOK_KEY_WHILE,

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

typedef struct LexerToken LexerToken;
struct LexerToken {
    enum LexerTokenType type;
    uint32_t byte_offset;
    uint32_t length;
    const char* text; /* Pointer to region-backed deduplicated string that gives us O(1) comparison. */
};

typedef struct LexerContext LexerContext;

LexerContext* lexer_context_new(const char* filepath) _nodiscard_;
void lexer_context_free(LexerContext* ctx);
void lexer_context_freep(LexerContext** ctx_ptr);
#define _autolexer_ _cleanup_(lexer_context_freep)

void lexer_process(LexerContext*);
const struct LexerToken* lexer_get_tokens(const LexerContext*, size_t* out_count) _nodiscard_;
void lexer_get_line_col(const LexerContext*, uint32_t byte_offset, uint32_t* out_line, uint32_t* out_col);
