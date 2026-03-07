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

