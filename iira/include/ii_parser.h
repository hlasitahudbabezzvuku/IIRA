#pragma once

/**
 * @brief Parser for the IIRA language.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast.h"
#include "ii_diagnostics.h"
#include "ii_lexer.h"
#include "ii_source.h"
#include "uf_common.h"

#include <stddef.h>
#include <stdint.h>

typedef struct ParserContext ParserContext;

ParserContext* ii_parser_context_new(Source*, const LexerToken* tokens, size_t token_count,
                                     DiagnosticContext*) _nodiscard_;
void ii_parser_context_free(ParserContext*);
void ii_parser_context_freep(ParserContext**);
#define _autoparser_ _cleanup_(ii_parser_context_freep)

Ast* ii_parser_get_ast(ParserContext*) _nodiscard_;

void ii_parser_print_debug(ParserContext*); /* Can be enabled by a flag */
