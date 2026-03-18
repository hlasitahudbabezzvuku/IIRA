/**
 * @brief Expression parser.
 *
 * This parser submodule handles literals, binary/unary operators, member access, function calls, indexing,
 * aggregate initialization, FFI calls, and casts using Pratt top-down operator precedence. This submodule is
 * by far the most complex one.
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#pragma once

#include "ii_parser.h"

AstExpr* ii_parser_parse_expression(ParserContext*) _nodiscard_;
