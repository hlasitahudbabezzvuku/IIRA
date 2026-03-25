/**
 * @brief Type resolution for semantic analysis.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast_iter.h"
#include "ii_semantic_internal.h"

#include <string.h>

static const struct PrimitiveTypeInfo _prim_info[] = {
    [LEXER_PRIM_VOID] = {.size = 0, .alignment = 1, .c_repr = "b"},
    [LEXER_PRIM_BOOL] = {.size = 1, .alignment = 1, .c_repr = "b"},
    [LEXER_PRIM_CHAR] = {.size = 1, .alignment = 1, .c_repr = "b"},
    [LEXER_PRIM_SHORT] = {.size = 2, .alignment = 2, .c_repr = "h"},
    [LEXER_PRIM_INT] = {.size = 4, .alignment = 4, .c_repr = "w"},
    [LEXER_PRIM_LONG] = {.size = 8, .alignment = 8, .c_repr = "l"},
    [LEXER_PRIM_FLOAT] = {.size = 4, .alignment = 4, .c_repr = "s"},
    [LEXER_PRIM_DOUBLE] = {.size = 8, .alignment = 8, .c_repr = "d"},
};

const struct PrimitiveTypeInfo* _get_prim_info(enum LexerPrimitiveType prim)
{
    return &_prim_info[prim];
}

const char* _type_to_string(enum LexerPrimitiveType prim)
{
    switch (prim) {
    case LEXER_PRIM_VOID:
        return "void";
    case LEXER_PRIM_BOOL:
        return "bool";
    case LEXER_PRIM_CHAR:
        return "char";
    case LEXER_PRIM_INT:
        return "int";
    case LEXER_PRIM_LONG:
        return "long";
    case LEXER_PRIM_SHORT:
        return "short";
    case LEXER_PRIM_FLOAT:
        return "float";
    case LEXER_PRIM_DOUBLE:
        return "double";
    default:
        return "unknown";
    }
}

