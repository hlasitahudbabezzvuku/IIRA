/**
 * @brief Top-level declaration parser.
 *
 * This parser submodule handles blueprint definitions (with inheritance, field/method aliasing, and
 * overriding) and function definitions/declarations at the Top-level (aka file root).
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#pragma once

#include "ii_parser.h"

AstNode* ii_parser_parse_declaration(ParserContext*) _nodiscard_;
