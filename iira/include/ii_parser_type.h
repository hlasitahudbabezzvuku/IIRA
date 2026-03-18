/**
 * @brief Type annotation parser.
 *
 * This parser submodule handles all type syntax: primitives, pointers, arrays, blueprint references,
 * and anonymous types.
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#pragma once

#include "ii_parser.h"

AstType* ii_parser_parse_type(ParserContext*) _nodiscard_;
AstType* ii_parser_parse_type_annotation(ParserContext*) _nodiscard_;
