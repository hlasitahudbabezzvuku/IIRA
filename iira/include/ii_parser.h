/**
 * @brief Parser core module.
 *
 * Parser is much more complex module then lexer was. That's why it's split into multiple smaller modules
 * (submodules). This one is the main one. It manages the parsing context, token stream, error recovery, and
 * coordinates all parser submodules. This is basically like root of the parser module.
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#pragma once

#include "ii_ast.h"
#include "ii_diagnostics.h"
#include "ii_lexer.h"
#include "ii_trace.h"
#include "uf_common.h"

typedef struct ParserContext ParserContext;

ParserContext* ii_parser_context_new(Ast* ast, const LexerToken* token_vector, size_t token_count,
                                     DiagnosticContext* diag, TraceContext*) _nodiscard_;
void ii_parser_context_free(ParserContext*);
void ii_parser_context_freep(ParserContext**);

#define _autoparser_ _cleanup_(ii_parser_context_freep)
