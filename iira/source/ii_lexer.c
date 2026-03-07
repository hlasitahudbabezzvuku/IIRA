/**
 * Lexer is probably the most straight forward module in iira, so it's probably the best place to start
 * learning how iirac works. Lexer's job is quite simple: read the file character by character and slice it to
 * individual tokens and symbols, removing whitespace and comments in the process.
 *
 * Since IIRA has quite simple syntax, we really need just 2 actions when lexing:
 * 1. Advance -> move to (consume) the next character.
 * 2. Peek next -> peek what's the next character, without consuming it.
 *
 * When we combine those two actions with conditions and state, we can parse any valid IIRA source file.
 * Yes... It's just giant state machine in the end :D.
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_lexer.h"
#include "ii_diagnostics.h"
#include "ii_source.h"
#include "uf_common.h"
#include "uf_containers.h"
#include "uf_logger.h"
#include "uf_memory.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define ARENA_BLOCK_SIZE (4 * 1024) /* 4 kilobytes blocks for strings */
#define MAX_IDENTIFIER 256          /* Max size for stack-based fast interning */

struct LexerContext {
    Source* source;
    const char* file_buffer;
    size_t file_size;

    UfConVector* tokens;
    UfConMap* symbol_dictionary;
    UfMemRegion* symbol_arena;

    uint32_t current_index;
    uint32_t start_index;
};

/*
 * Those helper functions wraps around the LexerContext struct. They separate the boring, repeating segments
 * from the active lexical analysis logic.
 */

static inline void _push_token(LexerContext* context, enum LexerTokenType type)
{
    LexerToken token = {
        .type = type,
        .span =
            {
                .offset = context->start_index,
                .length = context->current_index - context->start_index,
            },
        .variant.symbol = nullptr,
    };
    uf_con_vector_push(context->tokens, &token);
}

static inline void _push_token_symbol(LexerContext* context, const LexerSymbol* symbol)
{
    LexerToken token = {
        .type = LEXER_TOK_SYMBOL,
        .span =
            {
                .offset = context->start_index,
                .length = context->current_index - context->start_index,
            },
        .variant.symbol = symbol,
    };
    uf_con_vector_push(context->tokens, &token);
}

static inline void _push_token_error(LexerContext* context, DiagnosticContext* diag_context,
                                     const char* message)
{
    LexerToken token = {
        .type = LEXER_TOK_ERROR,
        .span =
            {
                .offset = context->start_index,
                .length = context->current_index - context->start_index,
            },
        .variant.error_message = message,
    };
    uf_con_vector_push(context->tokens, &token);
    ii_diag_report(diag_context, UF_LOG_ERROR, token.span, message);
}

static void _register_keyword(LexerContext* context, const char* keyword, enum LexerSymbolType type)
{
    LexerSymbol* symbol = uf_mem_region_malloc(context->symbol_arena, sizeof(LexerSymbol));
    symbol->type = type;
    symbol->text = keyword;

    uf_con_map_put(context->symbol_dictionary, keyword, symbol);
}

static const LexerSymbol* _intern_string(LexerContext* context, enum LexerSymbolType fallback_type)
{
    uint32_t length = context->current_index - context->start_index;
    const char* raw = &context->file_buffer[context->start_index];

    char stack_buffer[MAX_IDENTIFIER];
    bool fits_in_stack = length < MAX_IDENTIFIER;

    char* search_str = nullptr;
    if _likely_ (fits_in_stack) {
        memcpy(stack_buffer, raw, length);
        stack_buffer[length] = '\0';
        search_str = stack_buffer;
    } else {
        search_str = uf_mem_region_malloc(context->symbol_arena, length + 1);
        memcpy(search_str, raw, length);
        search_str[length] = '\0';
    }

    const LexerSymbol* existing = uf_con_map_get(context->symbol_dictionary, search_str);
    if (existing != nullptr) {
        return existing;
    }

    char* final_str = fits_in_stack ? uf_mem_region_malloc(context->symbol_arena, length + 1) : search_str;
    if (fits_in_stack) {
        memcpy(final_str, search_str, length + 1);
    }

    LexerSymbol* symbol = uf_mem_region_malloc(context->symbol_arena, sizeof(LexerSymbol));
    symbol->type = fallback_type;
    symbol->text = final_str;

    uf_con_map_put(context->symbol_dictionary, final_str, symbol);
    return symbol;
}

/*
 * Internal helper functions to make writing the lexing functions much easier. They include out two basic
 * actions (advance and peek next), with few other helpers for convenience.
 *
 * They exclusively us help with iterating over the source file buffer, managing the index, look ahead, and
 * EOF for us. That prevents us from shooting ourselves in the foot (e.g., forgetting that we have to count
 * the newlines inside a multiline comment :D, what an amazing thing to debug).
 */

static inline bool _is_end(const LexerContext* context)
{
    return context->current_index >= context->file_size;
}

static inline char _peek_current(const LexerContext* context)
{
    if _unlikely_ (_is_end(context)) {
        return '\0';
    }

    return context->file_buffer[context->current_index];
}

static inline char _peek_next(const LexerContext* context)
{
    if _unlikely_ (context->current_index + 1 >= context->file_size) {
        return '\0';
    }

    return context->file_buffer[context->current_index + 1];
}

static inline char _advance(LexerContext* context)
{
    char ch = context->file_buffer[context->current_index++];
    if _unlikely_ (ch == '\n') {
        ii_src_add_newline(context->source, context->current_index);
    }

    return ch;
}

static inline bool _match(LexerContext* context, char expected)
{
    if (_is_end(context) || context->file_buffer[context->current_index] != expected) {
        return false;
    }

    _advance(context);
    return true;
}

/*
 * Those are the functions, meant to be dispatched by the lexer's main loop. They are really just series of
 * our two basic actions (e.g., advance, and peek next). Each function should handle one type of symbol.
 */

static void _scan_number(LexerContext* context)
{
    char first = context->file_buffer[context->start_index];

    if (first == '0') {
        switch (_peek_current(context)) {
        case 'x':
        case 'X':
            _advance(context); /* Consume the character "X" */
            while (isxdigit(_peek_current(context))) {
                _advance(context);
            }
            goto scan_suffix;
        case 'b':
        case 'B':
            _advance(context); /* Consume the character "B" */
            while (_peek_current(context) == '0' || _peek_current(context) == '1') {
                _advance(context);
            }
            goto scan_suffix;
        case 'o':
        case 'O':
            _advance(context); /* Consume the character "O" */
            while (_peek_current(context) >= '0' && _peek_current(context) <= '7') {
                _advance(context);
            }
            goto scan_suffix;
        }
    }

    while (isdigit(_peek_current(context))) {
        _advance(context);
    }

    if (_peek_current(context) == '.' && isdigit(_peek_next(context))) {
        _advance(context); /* Consume the character "." */
        while (isdigit(_peek_current(context))) {
            _advance(context);
        }
    }

scan_suffix: /* Yes, It's a `goto`. But, as you can see, it actually helps to simplify the logic without the
                need to separate this function into two smaller functions. It isn't always bad :D. */

    char ch = _peek_current(context);
    if (ch == 'f' || ch == 'F' || ch == 'd' || ch == 'D') {
        _advance(context);
    }

    const LexerSymbol* symbol = _intern_string(context, LEXER_SYM_NUMBER);
    _push_token_symbol(context, symbol);
}

static void _scan_string(LexerContext* context, DiagnosticContext* diag_context)
{
    while (_peek_current(context) != '"' && !_is_end(context)) {
        _advance(context);
    }

    if (_is_end(context)) {
        _push_token_error(context, diag_context, "Unterminated string literal");
        return;
    }

    _advance(context); /* Consume the closing quote */

    context->start_index++;   /* Skip opening quote */
    context->current_index--; /* Skip closing quote */
    const LexerSymbol* symbol = _intern_string(context, LEXER_SYM_STRING);
    context->start_index--;
    context->current_index++;

    _push_token_symbol(context, symbol);
}

/* This is the main Lexer loop. It uses dispatcher functions for more readable logic flow. */
LexerContext* ii_lexer_context_new(Source* source, DiagnosticContext* diag_context)
{
    LexerContext* context = uf_mem_zalloc(sizeof(LexerContext));
    context->source = source;
    context->file_buffer = ii_src_get_buffer(source);
    context->file_size = ii_src_get_size(source);

    context->tokens = uf_con_vector_new(sizeof(LexerToken));
    context->symbol_dictionary = uf_con_map_new();
    context->symbol_arena = uf_mem_region_new(ARENA_BLOCK_SIZE);

    /* Here we register our keywords. */
    _register_keyword(context, "as", LEXER_SYM_KEY_AS);
    _register_keyword(context, "break", LEXER_SYM_KEY_BREAK);
    _register_keyword(context, "case", LEXER_SYM_KEY_CASE);
    _register_keyword(context, "continue", LEXER_SYM_KEY_CONTINUE);
    _register_keyword(context, "default", LEXER_SYM_KEY_DEFAULT);
    _register_keyword(context, "do", LEXER_SYM_KEY_DO);
    _register_keyword(context, "else", LEXER_SYM_KEY_ELSE);
    _register_keyword(context, "false", LEXER_SYM_NUMBER);
    _register_keyword(context, "for", LEXER_SYM_KEY_FOR);
    _register_keyword(context, "if", LEXER_SYM_KEY_IF);
    _register_keyword(context, "return", LEXER_SYM_KEY_RETURN);
    _register_keyword(context, "self", LEXER_SYM_KEY_SELF);
    _register_keyword(context, "switch", LEXER_SYM_KEY_SWITCH);
    _register_keyword(context, "true", LEXER_SYM_NUMBER);
    _register_keyword(context, "var", LEXER_SYM_KEY_VAR);
    _register_keyword(context, "while", LEXER_SYM_KEY_WHILE);

    /* This is where we dispatch our scanner functions. */
    while (!_is_end(context)) {
        context->start_index = context->current_index;
        char ch = _advance(context);

        if (isspace(ch)) {
            continue;
        }

        if (isalpha(ch) || ch == '_') {
            while (isalnum(_peek_current(context)) || _peek_current(context) == '_') {
                _advance(context);
            }
            const LexerSymbol* symbol = _intern_string(context, LEXER_SYM_IDENTIFIER);
            _push_token_symbol(context, symbol);
            continue;
        }

        if (isdigit(ch)) {
            _scan_number(context);
            continue;
        }

        /* To handle the `.5f` shorthand we retroactively step back so `_scan_number` sees the dot. */
        if (ch == '.' && isdigit(_peek_current(context))) {
            context->current_index--;
            _scan_number(context);
            continue;
        }

        switch (ch) {
        /* Single character punctuation. */
        case '{':
            _push_token(context, LEXER_TOK_LBRACE);
            break;
        case '}':
            _push_token(context, LEXER_TOK_RBRACE);
            break;
        case '(':
            _push_token(context, LEXER_TOK_LPAREN);
            break;
        case ')':
            _push_token(context, LEXER_TOK_RPAREN);
            break;
        case '[':
            _push_token(context, LEXER_TOK_LBRACKET);
            break;
        case ']':
            _push_token(context, LEXER_TOK_RBRACKET);
            break;
        case '?':
            _push_token(context, LEXER_TOK_QUESTION);
            break;
        case ':':
            _push_token(context, LEXER_TOK_COLON);
            break;
        case ';':
            _push_token(context, LEXER_TOK_SEMICOLON);
            break;
        case ',':
            _push_token(context, LEXER_TOK_COMMA);
            break;
        case '.':
            _push_token(context, LEXER_TOK_DOT);
            break;
        case '$':
            _push_token(context, LEXER_TOK_DOLLAR);
            break;
        case '~':
            _push_token(context, LEXER_TOK_BIT_NOT);
            break;
        case '^':
            _push_token(context, LEXER_TOK_BIT_XOR);
            break;
        case '%':
            _push_token(context, LEXER_TOK_PERCENT);
            break;

        /* Two-character operators. We are using the match() function to make the it easier. */
        case '=':
            _push_token(context, _match(context, '=') ? LEXER_TOK_EQ : LEXER_TOK_ASSIGN);
            break;
        case '!':
            _push_token(context, _match(context, '=') ? LEXER_TOK_NEQ : LEXER_TOK_NOT);
            break;
        case '<':
            _push_token(context, _match(context, '=') ? LEXER_TOK_LTE : LEXER_TOK_LT);
            break;
        case '>':
            _push_token(context, _match(context, '=') ? LEXER_TOK_GTE : LEXER_TOK_GT);
            break;
        case '+':
            _push_token(context, _match(context, '=') ? LEXER_TOK_PLUS_ASSIGN : LEXER_TOK_PLUS);
            break;
        case '-':
            _push_token(context, _match(context, '=') ? LEXER_TOK_MINUS_ASSIGN : LEXER_TOK_MINUS);
            break;
        case '*':
            _push_token(context, _match(context, '=') ? LEXER_TOK_STAR_ASSIGN : LEXER_TOK_STAR);
            break;

        case '&':
            _push_token(context, _match(context, '&') ? LEXER_TOK_AND : LEXER_TOK_BIT_AND);
            break;
        case '|':
            _push_token(context, _match(context, '|') ? LEXER_TOK_OR : LEXER_TOK_BIT_OR);
            break;

        case '/':
            if (_match(context, '/')) {
                /* Single-line comments. */
                while (_peek_current(context) != '\n' && !_is_end(context)) {
                    _advance(context);
                }
            } else if (_match(context, '*')) {
                /* Multiline C-style comments. */
                bool terminated = false;
                while (!_is_end(context)) {
                    if (_peek_current(context) == '*' && _peek_next(context) == '/') {
                        _advance(context); /* Consume the star character */
                        _advance(context); /* Consume the slash character */
                        terminated = true;
                        break;
                    }
                    _advance(context);
                }

                if _unlikely_ (!terminated) {
                    _push_token_error(context, diag_context, "Unterminated multi-line comment");
                }
            } else {
                _push_token(context, _match(context, '=') ? LEXER_TOK_SLASH_ASSIGN : LEXER_TOK_SLASH);
            }
            break;

        case '"':
            _scan_string(context, diag_context);
            break;

        default:
            _push_token_error(context, diag_context, "Unexpected character");
            break;
        }
    }

    /* EOF token signals that we reached the very end of the file. Good job! */
    context->start_index = context->current_index;
    _push_token(context, LEXER_TOK_EOF);

    return context;
}

void ii_lexer_context_free(LexerContext* context)
{
    if (context == nullptr) {
        return;
    }

    uf_con_vector_free(context->tokens);
    uf_con_map_free(context->symbol_dictionary);
    uf_mem_region_free(context->symbol_arena);

    uf_mem_free(context);
}

void ii_lexer_context_freep(LexerContext** context_ptr)
{
    if (context_ptr && *context_ptr) {
        ii_lexer_context_free(*context_ptr);
        *context_ptr = nullptr;
    }
}

const LexerToken* ii_lexer_get_tokens(const LexerContext* context)
{
    return (const LexerToken*)uf_con_vector_get(context->tokens, 0);
}

size_t ii_lexer_get_token_count(const LexerContext* context)
{
    return uf_con_vector_length(context->tokens);
}

/*
 * Helper function for printing the tokens into the standard output. It's used to print the lexer output when
 * appropriate flag is used for compilation of particular unit.
 */

const char* _token_type_to_string[] = {
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

void ii_lexer_print_debug(const LexerContext* context)
{
    const LexerToken* tokens = ii_lexer_get_tokens(context);

    for (size_t i = 0; i < uf_con_vector_length(context->tokens); i++) {
        const LexerToken* token = &tokens[i];
        SourceLocation locaction = ii_src_resolve_location(context->source, token->span.offset);

        printf("\e[1;%im[%3u:%-3u]\e[%im  %s", UF_COLOR_BLACK_LIGHT, locaction.line, locaction.column,
               UF_COLOR_RESET, _token_type_to_string[token->type]);

        if (token->type == LEXER_TOK_SYMBOL) {
            switch (token->variant.symbol->type) {
            case LEXER_SYM_IDENTIFIER:
                printf(" -> \e[1;%im%s", UF_COLOR_WHITE_LIGHT, token->variant.symbol->text);
                break;
            case LEXER_SYM_NUMBER:
                printf(" -> \e[1;%im%s", UF_COLOR_YELLOW_LIGHT, token->variant.symbol->text);
                break;
            case LEXER_SYM_STRING:
                printf(" -> \e[1;%im\"%s\"", UF_COLOR_GREEN_LIGHT, token->variant.symbol->text);
                break;
            default:
                printf(" -> \e[1;%im%s", UF_COLOR_BLUE_LIGHT, token->variant.symbol->text);
            }
        } else if (token->type == LEXER_TOK_ERROR) {
            printf("\e[1;%im%s", UF_COLOR_RED_LIGHT, token->variant.error_message);
        }

        printf("\e[%im\n", UF_COLOR_RESET);
    }
}
