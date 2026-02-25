#include "lexer.h"

#include "uf_common.h"
#include "uf_containers.h"
#include "uf_memory.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define LEXER_ARENA_BLOCK_SIZE (64 * 1024) /* 64 kilobytes blocks for strings */
#define LEXER_MAX_IDENTIFIER 256           /* Max size for stack-based fast interning */

#define IS_INDENT_START(ch) (isalpha(ch) || ch == '_')
#define IS_INDENT_PART(ch) (isalnum(ch) || ch == '_')

struct LexerContext {
    int file_descriptor;
    size_t file_size;
    const char* file_buffer;

    UfMemRegion* arena;
    UfConVector* tokens;
    UfConVector* lines;
    UfConMap* string_pool;

    uint32_t current_index;
    uint32_t start_index;
};

struct Record {
    enum LexerTokenType type;
    const char* text;
};

static void _push_token(LexerContext* context, enum LexerTokenType type, const char* text)
{
    uint32_t length = context->current_index - context->start_index;
    struct LexerToken token = {type, context->start_index, length, text};
    uf_con_vector_push(context->tokens, &token);
}

static void _register_keyword(LexerContext* context, const char* keyword, enum LexerTokenType type)
{
    struct Record* record = uf_mem_region_alloc(context->arena, sizeof(struct Record));
    record->type = type;
    record->text = keyword;

    uf_con_map_put(context->string_pool, keyword, record);
}

static const struct Record* _register_string(LexerContext* context, enum LexerTokenType fallback_type)
{
    uint32_t length = context->current_index - context->start_index;
    const char* raw = &context->file_buffer[context->start_index];

    char stack_buf[LEXER_MAX_IDENTIFIER];
    bool fits_in_stack = length < LEXER_MAX_IDENTIFIER;

    char* search_str = nullptr;
    if _likely_ (fits_in_stack) {
        memcpy(stack_buf, raw, length);
        stack_buf[length] = '\0';
        search_str = stack_buf;
    } else {
        search_str = uf_mem_region_alloc(context->arena, length + 1);
        memcpy(search_str, raw, length);
        search_str[length] = '\0';
    }

    const struct Record* existing = uf_con_map_get(context->string_pool, search_str);
    if (existing != nullptr) {
        return existing;
    }

    char* final_str = fits_in_stack ? uf_mem_region_alloc(context->arena, length + 1) : search_str;
    if (fits_in_stack) {
        memcpy(final_str, search_str, length + 1);
    }

    struct Record* record = uf_mem_region_alloc(context->arena, sizeof(struct Record));
    record->type = fallback_type;
    record->text = final_str;

    uf_con_map_put(context->string_pool, final_str, record);
    return record;
}

/*
 * Internal helper functions to make writing the lexing functions much easier.
 */

static inline bool _is_end(const LexerContext* context)
{
    return context->current_index >= context->file_size;
}

static inline char _peek(const LexerContext* context)
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
        uint32_t next_line_offset = context->current_index;
        uf_con_vector_push(context->lines, &next_line_offset);
    }

    return ch;
}

static inline bool _match(LexerContext* context, char expected)
{
    if (_is_end(context) || context->file_buffer[context->current_index] != expected) {
        return false;
    }

    context->current_index++;
    return true;
}

/*
 * Those are the helper functions that are dispatched by the lexer's main loop. Each should handle one type to
 * token. The above functions are used inside those functions to wrap around the `LexerContext` struct's
 * fields, and prevent us from shooting ourselves to foot (e.g., forgetting to count new lines inside
 * multiline comment :D, what an amazing thing to debug).
 */

static void _scan_number(LexerContext* context)
{
    char first = context->file_buffer[context->start_index];
    if (first == '0') {
        char next = _peek(context);
        if (next == 'x' || next == 'X') {
            _advance(context); /* Consume the character "X" */
            while (isxdigit(_peek(context))) {
                _advance(context);
            }
            goto scan_suffix;
        } else if (next == 'b' || next == 'B') {
            _advance(context); /* Consume the character "B" */
            while (_peek(context) == '0' || _peek(context) == '1') {
                _advance(context);
            }
            goto scan_suffix;
        } else if (next == 'o' || next == 'O') {
            _advance(context); /* Consume the character "O" */
            while (_peek(context) >= '0' && _peek(context) <= '7') {
                _advance(context);
            }
            goto scan_suffix;
        }
    }

    while (isdigit(_peek(context))) {
        _advance(context);
    }

    if (_peek(context) == '.' && isdigit(_peek_next(context))) {
        _advance(context); /* Consume the character "." */
        while (isdigit(_peek(context))) {
            _advance(context);
        }
    }

scan_suffix: /* Yes, It's a `goto`. But, as you can see, it actually helps to simplify the logic without the
                need to separate it into multiple smaller functions. It isn't always bad :D. */

    char p = _peek(context);
    if (p == 'f' || p == 'F' || p == 'd' || p == 'D') {
        _advance(context);
    }

    const struct Record* record = _register_string(context, LEXER_TOK_NUMBER);
    _push_token(context, record->type, record->text);
}

static void _scan_string(LexerContext* context)
{
    while (_peek(context) != '"' && !_is_end(context)) {
        _advance(context);
    }

    if (_is_end(context)) {
        _push_token(context, LEXER_TOK_ERROR, "Unterminated string literal");
        return;
    }

    _advance(context); /* Consume the closing quote */

    context->start_index++;   /* Skip opening quote */
    context->current_index--; /* Skip closing quote */
    const struct Record* rec = _register_string(context, LEXER_TOK_STRING);

    context->start_index--;
    context->current_index++;
    _push_token(context, LEXER_TOK_STRING, rec->text);
}

/* This is the main Lexer pipeline. It uses dispatcher functions for more readable logic flow. */
LexerContext* lexer_context_new(const char* filepath)
{
    int file_descriptor = open(filepath, O_RDONLY);
    if (file_descriptor < 0) {
        return nullptr;
    }

    struct stat file_stat;
    if (fstat(file_descriptor, &file_stat) < 0 || !S_ISREG(file_stat.st_mode) || file_stat.st_size == 0) {
        close(file_descriptor);
        return nullptr;
    }

    size_t file_size = (size_t)file_stat.st_size;
    const char* map = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE, file_descriptor, 0);
    if (map == MAP_FAILED) {
        close(file_descriptor);
        return nullptr;
    }

    LexerContext* context = uf_mem_zalloc(sizeof(LexerContext));
    context->file_descriptor = file_descriptor;
    context->file_size = file_size;
    context->file_buffer = map;

    context->arena = uf_mem_region_new(LEXER_ARENA_BLOCK_SIZE);
    context->tokens = uf_con_vector_new(sizeof(struct LexerToken));
    context->lines = uf_con_vector_new(sizeof(uint32_t));
    context->string_pool = uf_con_map_new();

    /* Here we register our keywords. */
    _register_keyword(context, "as", LEXER_TOK_KEY_AS);
    _register_keyword(context, "break", LEXER_TOK_KEY_BREAK);
    _register_keyword(context, "case", LEXER_TOK_KEY_CASE);
    _register_keyword(context, "continue", LEXER_TOK_KEY_CONTINUE);
    _register_keyword(context, "default", LEXER_TOK_KEY_DEFAULT);
    _register_keyword(context, "do", LEXER_TOK_KEY_DO);
    _register_keyword(context, "else", LEXER_TOK_KEY_ELSE);
    _register_keyword(context, "for", LEXER_TOK_KEY_FOR);
    _register_keyword(context, "if", LEXER_TOK_KEY_IF);
    _register_keyword(context, "return", LEXER_TOK_KEY_RETURN);
    _register_keyword(context, "switch", LEXER_TOK_KEY_SWITCH);
    _register_keyword(context, "var", LEXER_TOK_KEY_VAR);
    _register_keyword(context, "while", LEXER_TOK_KEY_WHILE);

    /* Bootstrapping the line counter (first line at byte 0) */
    uf_con_vector_push(context->lines, &(uint32_t){0});

    /* This is where we dispatch our helper functions for lexing (scanners). */
    while (!_is_end(context)) {
        context->start_index = context->current_index;
        char ch = _advance(context);

        if (isspace(ch)) {
            continue;
        }

        if (isalpha(ch) || ch == '_') {
            while (isalnum(_peek(context)) || _peek(context) == '_') {
                _advance(context);
            }
            const struct Record* record = _register_string(context, LEXER_TOK_IDENTIFIER);
            _push_token(context, record->type, record->text);
            continue;
        }

        if (isdigit(ch)) {
            _scan_number(context);
            continue;
        }

        /* To handle the `.5f` shorthand we retroactively step back so `_scan_number` sees the dot. */
        if (ch == '.' && isdigit(_peek(context))) {
            context->current_index--;
            _scan_number(context);
            continue;
        }

        switch (ch) {
        /* Single character punctuation */
        case '{':
            _push_token(context, LEXER_TOK_LBRACE, nullptr);
            break;
        case '}':
            _push_token(context, LEXER_TOK_RBRACE, nullptr);
            break;
        case '(':
            _push_token(context, LEXER_TOK_LPAREN, nullptr);
            break;
        case ')':
            _push_token(context, LEXER_TOK_RPAREN, nullptr);
            break;
        case '[':
            _push_token(context, LEXER_TOK_LBRACKET, nullptr);
            break;
        case ']':
            _push_token(context, LEXER_TOK_RBRACKET, nullptr);
            break;
        case '?':
            _push_token(context, LEXER_TOK_QUESTION, nullptr);
            break;
        case ':':
            _push_token(context, LEXER_TOK_COLON, nullptr);
            break;
        case ';':
            _push_token(context, LEXER_TOK_SEMICOLON, nullptr);
            break;
        case ',':
            _push_token(context, LEXER_TOK_COMMA, nullptr);
            break;
        case '.':
            _push_token(context, LEXER_TOK_DOT, nullptr);
            break;
        case '$':
            _push_token(context, LEXER_TOK_DOLLAR, nullptr);
            break;
        case '~':
            _push_token(context, LEXER_TOK_BIT_NOT, nullptr);
            break;
        case '^':
            _push_token(context, LEXER_TOK_BIT_XOR, nullptr);
            break;
        case '%':
            _push_token(context, LEXER_TOK_PERCENT, nullptr);
            break;

        /* Two-character operators. We are using the match() function to make you job easier. */
        case '=':
            _push_token(context, _match(context, '=') ? LEXER_TOK_EQ : LEXER_TOK_ASSIGN, nullptr);
            break;
        case '!':
            _push_token(context, _match(context, '=') ? LEXER_TOK_NEQ : LEXER_TOK_NOT, nullptr);
            break;
        case '<':
            _push_token(context, _match(context, '=') ? LEXER_TOK_LTE : LEXER_TOK_LT, nullptr);
            break;
        case '>':
            _push_token(context, _match(context, '=') ? LEXER_TOK_GTE : LEXER_TOK_GT, nullptr);
            break;
        case '+':
            _push_token(context, _match(context, '=') ? LEXER_TOK_PLUS_ASSIGN : LEXER_TOK_PLUS, nullptr);
            break;
        case '-':
            _push_token(context, _match(context, '=') ? LEXER_TOK_MINUS_ASSIGN : LEXER_TOK_MINUS, nullptr);
            break;
        case '*':
            _push_token(context, _match(context, '=') ? LEXER_TOK_STAR_ASSIGN : LEXER_TOK_STAR, nullptr);
            break;

        case '&':
            _push_token(context, _match(context, '&') ? LEXER_TOK_AND : LEXER_TOK_BIT_AND, nullptr);
            break;
        case '|':
            _push_token(context, _match(context, '|') ? LEXER_TOK_OR : LEXER_TOK_BIT_OR, nullptr);
            break;

        case '/':
            if (_match(context, '/')) {
                /* Single-line comments. */
                while (_peek(context) != '\n' && !_is_end(context)) {
                    _advance(context);
                }
            } else if (_match(context, '*')) {
                /* Multiline C-style comments. */
                bool terminated = false;
                while (!_is_end(context)) {
                    if (_peek(context) == '*' && _peek_next(context) == '/') {
                        _advance(context); /* Consume the star character */
                        _advance(context); /* Consume the slash character */
                        terminated = true;
                        break;
                    }
                    _advance(context);
                }

                if _unlikely_ (!terminated) {
                    _push_token(context, LEXER_TOK_ERROR, "Unterminated multi-line comment");
                }
            } else {
                _push_token(context, _match(context, '=') ? LEXER_TOK_SLASH_ASSIGN : LEXER_TOK_SLASH,
                            nullptr);
            }
            break;

        case '"':
            _scan_string(context);
            break;

        default:
            _push_token(context, LEXER_TOK_ERROR, "Unexpected character");
            break;
        }
    }

    /* EOF token signals that we reached the very end of the file. Good job! */
    context->start_index = context->current_index;
    _push_token(context, LEXER_TOK_EOF, nullptr);

    return context;
}

void lexer_context_free(LexerContext* context)
{
    if (context == nullptr) {
        return;
    }

    if (context->file_buffer != nullptr && context->file_size > 0) {
        munmap((void*)context->file_buffer, context->file_size);
    }

    if (context->file_descriptor >= 0) {
        close(context->file_descriptor);
    }

    uf_con_vector_free(context->tokens);
    uf_con_vector_free(context->lines);
    uf_con_map_free(context->string_pool);
    uf_mem_region_free(context->arena);

    uf_mem_free(context);
}

void lexer_context_freep(LexerContext** context_ptr)
{
    if (context_ptr && *context_ptr) {
        lexer_context_free(*context_ptr);
        *context_ptr = nullptr;
    }
}

const LexerToken* lexer_get_tokens(const LexerContext* context, size_t* out_count)
{
    if (out_count) {
        *out_count = uf_con_vector_length(context->tokens);
    }

    return (const LexerToken*)uf_con_vector_get(context->tokens, 0);
}

void lexer_get_line_col(const LexerContext* context, uint32_t byte_offset, uint32_t* out_line,
                        uint32_t* out_col)
{
    size_t lines = uf_con_vector_length(context->lines);
    if (lines == 0) {
        if (out_line)
            *out_line = 1;
        if (out_col)
            *out_col = 1;
        return;
    }

    /*
     * We are using binary search to find the correct line we are on. It's really the fastest way to search
     * through a sorted set. You can read more about it here: https://en.wikipedia.org/wiki/Binary_search.
     */
    size_t low = 0;
    size_t high = lines - 1;
    size_t match_index = 0;

    while (low <= high) {
        size_t mid = low + (high - low) / 2;
        uint32_t line_index = *(const uint32_t*)uf_con_vector_get(context->lines, mid);

        if (line_index <= byte_offset) {
            match_index = mid;
            low = mid + 1;
        } else {
            if (mid == 0)
                break;
            high = mid - 1;
        }
    }

    if (out_line) {
        *out_line = (uint32_t)(match_index + 1);
    }

    if (out_col) {
        uint32_t line_start = *(const uint32_t*)uf_con_vector_get(context->lines, match_index);
        *out_col = (byte_offset - line_start) + 1;
    }
}
