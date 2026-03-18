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
#include "ii_source.h"
#include "ii_trace.h"
#include "uf_common.h"

#include <stddef.h>

typedef struct ParserContext ParserContext;

ParserContext* ii_parser_context_new(const char* file_path, bool trace_enabled) _nodiscard_;
void ii_parser_context_free(ParserContext*);
Ast* ii_parser_parse(ParserContext*) _nodiscard_;

#define _autoparser_ _cleanup_(ii_parser_context_freep)
