/**
 * @brief Parser core module.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_parser.h"
#include "ii_parser_decl.h"
#include "ii_parser_expr.h"
#include "ii_parser_stmt.h"
#include "ii_parser_type.h"

#include <stdlib.h>

/*
 * Context lifecycle.
 */

ParserContext* ii_parser_context_new(const char* file_path, bool trace_enabled)
{
    (void)file_path;
    (void)trace_enabled;
    return NULL;
}

void ii_parser_context_free(ParserContext* context)
{
    (void)context;
}

/*
 * Entry point.
 */

Ast* ii_parser_parse(ParserContext* context)
{
    (void)context;
    return NULL;
}
