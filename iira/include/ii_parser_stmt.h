/**
 * @brief Statement parser.
 *
 * This parser submodule handles all statement forms: blocks, return, variable declarations,
 * if/while/do-while/for loops, break, continue, and expression statements.
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#pragma once

#include "ii_parser.h"

AstBlock* ii_parser_parse_block(ParserContext*) _nodiscard_;
AstStmt* ii_parser_parse_statement(ParserContext*) _nodiscard_;
