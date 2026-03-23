/**
 * @brief Parser core module.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_parser_internal.h"

#include "ii_parser.h"
#include "ii_parser_decl.h"
#include "ii_trace.h"

#include <inttypes.h>
#include <stdio.h>

const char* _token_name(enum LexerTokenType type)
{
    switch (type) {
    case LEXER_TOK_EOF:
        return "<end of file>";

    case LEXER_TOK_ERROR:
        return "<error>";

    case LEXER_TOK_SYMBOL:
        return "<symbol>";

    /* Punctuation */
    case LEXER_TOK_LBRACE:
        return "{";
    case LEXER_TOK_RBRACE:
        return "}";
    case LEXER_TOK_LPAREN:
        return "(";
    case LEXER_TOK_RPAREN:
        return ")";
    case LEXER_TOK_LBRACKET:
        return "[";
    case LEXER_TOK_RBRACKET:
        return "]";
    case LEXER_TOK_QUESTION:
        return "?";
    case LEXER_TOK_COLON:
        return ":";
    case LEXER_TOK_SEMICOLON:
        return ";";
    case LEXER_TOK_COMMA:
        return ",";
    case LEXER_TOK_DOT:
        return ".";
    case LEXER_TOK_DOLLAR:
        return "$";

    /* Operators */
    case LEXER_TOK_ASSIGN:
        return "=";
    case LEXER_TOK_PLUS:
        return "+";
    case LEXER_TOK_MINUS:
        return "-";
    case LEXER_TOK_STAR:
        return "*";
    case LEXER_TOK_SLASH:
        return "/";
    case LEXER_TOK_PERCENT:
        return "%";
    case LEXER_TOK_PLUS_ASSIGN:
        return "+=";
    case LEXER_TOK_MINUS_ASSIGN:
        return "-=";
    case LEXER_TOK_STAR_ASSIGN:
        return "*=";
    case LEXER_TOK_SLASH_ASSIGN:
        return "/=";
    case LEXER_TOK_EQ:
        return "==";
    case LEXER_TOK_NEQ:
        return "!=";
    case LEXER_TOK_LT:
        return "<";
    case LEXER_TOK_GT:
        return ">";
    case LEXER_TOK_LTE:
        return "<=";
    case LEXER_TOK_GTE:
        return ">=";
    case LEXER_TOK_AND:
        return "&&";
    case LEXER_TOK_OR:
        return "||";
    case LEXER_TOK_NOT:
        return "!";
    case LEXER_TOK_BIT_AND:
        return "&";
    case LEXER_TOK_BIT_OR:
        return "|";
    case LEXER_TOK_BIT_XOR:
        return "^";
    case LEXER_TOK_BIT_NOT:
        return "~";

    default:
        return "<unknown>";
    }
}

const char* _make_anon_name(ParserContext* context)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "__anon_%" PRIu32, context->anon_counter);
    context->anon_counter++;
    return ii_src_intern_cstr(context->src, buf);
}

ParserContext* ii_parser_context_new(Ast* ast, const LexerToken* token_vector, size_t token_count,
                                     DiagnosticContext* diag, TraceContext* trace_context)
{
    ParserContext* context = uf_mem_zalloc(sizeof(ParserContext));

    context->ast = ast;
    context->src = ii_ast_get_source(ast);
    context->diag = diag;
    context->tokens = (LexerToken*)token_vector;
    context->count = token_count;
    context->current = 0;
    context->anon_counter = 0;
    context->had_error = false;
    context->last_span = (SourceSpan){0, 0};
    context->trace = trace_context;

    TRACE_SCOPE(context->trace);

    while (true) {
        if (context->current >= context->count - 1) {
            break;
        }
        if (_check(context, LEXER_TOK_EOF)) {
            break;
        }

        AstNode* declaration = ii_parser_parse_declaration(context);

        if (declaration == nullptr) {
            _sync_to_decl(context);
            continue;
        }
    }

    return context;
}

void ii_parser_context_free(ParserContext* context)
{
    if (!context) {
        return;
    }

    uf_mem_free(context);
}

void ii_parser_context_freep(ParserContext** context_ptr)
{
    if (!context_ptr || !*context_ptr) {
        return;
    }

    ii_parser_context_free(*context_ptr);
    *context_ptr = nullptr;
}
