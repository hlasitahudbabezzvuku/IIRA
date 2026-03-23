/**
 * @brief Top-level declaration parser.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_parser_decl.h"

#include "ii_parser_expr.h"
#include "ii_parser_internal.h"
#include "ii_parser_stmt.h"
#include "ii_parser_type.h"

/*
 * Alias list parsing (inheritance field aliasing).
 *
 * Syntax: Parent { field as alias, other as alias }
 *
 * Each entry has form: IDENT ('as' IDENT)?
 * Commas separate entries, list ends at '}'.
 *
 * Error recovery: Skip to next comma or closing brace.
 */
static void _parse_alias_list(ParserContext* context, AstInherit* inherit)
{
    TRACE_SCOPE(context->trace);
    while (!_check(context, LEXER_TOK_RBRACE)) {
        /* EOF inside alias list - fatal, break out */
        if (_check(context, LEXER_TOK_EOF)) {
            _diag_error(context, "Unexpected end of file in alias list");
            break;
        }

        /* Expected identifier (alias name or original name) */
        if (!_check_ident(context)) {
            _diag_error(context, "Expected alias name");
            /* Skip to comma or closing brace */
            while (!_check(context, LEXER_TOK_COMMA) && !_check(context, LEXER_TOK_RBRACE) &&
                   !_check(context, LEXER_TOK_EOF)) {
                _advance(context);
            }
            if (_check(context, LEXER_TOK_RBRACE)) {
                break;
            }
            continue;
        }

        SourceSpan orig_span = _span(context);
        const char* orig_name = _sym_text(context);
        _advance(context);

        /* Optional 'as' keyword */
        if (!_match_sym(context, LEXER_SYM_KEY_AS)) {
            /* Skip to comma or closing brace */
            while (!_check(context, LEXER_TOK_COMMA) && !_check(context, LEXER_TOK_RBRACE) &&
                   !_check(context, LEXER_TOK_EOF)) {
                _advance(context);
            }
            if (_check(context, LEXER_TOK_RBRACE)) {
                break;
            }
            continue;
        }

        /* 'as' present - expect alias name */
        if (!_check_ident(context)) {
            _diag_error(context, "Expected alias name after 'as'");
            /* Skip to comma or closing brace */
            while (!_check(context, LEXER_TOK_COMMA) && !_check(context, LEXER_TOK_RBRACE) &&
                   !_check(context, LEXER_TOK_EOF)) {
                _advance(context);
            }
            if (_check(context, LEXER_TOK_RBRACE)) {
                break;
            }
            continue;
        }

        SourceSpan alias_span = _span(context);
        const char* alias_name = _sym_text(context);
        _advance(context);

        (void)orig_span;
        (void)alias_span;
        ii_ast_inherit_add_alias(inherit, orig_name, alias_name);

        /* Comma separates entries */
        if (_match(context, LEXER_TOK_COMMA)) {
            /* Trailing comma before '}' is an error */
            if (_check(context, LEXER_TOK_RBRACE)) {
                _diag_error(context, "Unexpected ',' in alias list");
                break;
            }
            continue;
        }

        /* End of list */
        if (_check(context, LEXER_TOK_RBRACE)) {
            break;
        }
    }
}

/*
 * Inheritance list parsing (multi-inheritance with aliases).
 *
 * Syntax: Parent1 { aliases }, Parent2 { aliases }, ...
 *
 * Each parent can have optional alias block.
 * Comma separates parent entries.
 * List ends at '{' (blueprint body) or EOF.
 *
 * Error recovery: Missing parent name triggers sync to '{' or EOF.
 */
static void _parse_inheritance_list(ParserContext* context, AstBlueprintDecl* bp)
{
    TRACE_SCOPE(context->trace);
    while (!_check(context, LEXER_TOK_EOF)) {
        /* '}' seen - inheritance list ends, caller handles body */
        if (_check(context, LEXER_TOK_LBRACE)) {
            break;
        }

        /* Expected parent name */
        if (!_check_ident(context)) {
            _diag_error(context, "Expected parent name or blueprint body");
            _sync_to_decl(context);
            if (_check(context, LEXER_TOK_LBRACE)) {
                break;
            }
            if (_check(context, LEXER_TOK_EOF)) {
                return;
            }
            continue;
        }

        SourceSpan parent_span = _span(context);
        const char* parent_name = _sym_text(context);
        _advance(context);

        AstInherit* inherit = ii_ast_inherit_sp(context->ast, parent_span, parent_name);

        /* Optional alias block: Parent { ... } */
        if (_match(context, LEXER_TOK_LBRACE)) {
            _parse_alias_list(context, inherit);
            _expect(context, LEXER_TOK_RBRACE);
        }

        ii_ast_blueprint_add_inherit(bp, inherit);

        /* Comma - more parents to follow */
        if (_match(context, LEXER_TOK_COMMA)) {
            /* Empty inheritance (no parent) before body: Parent { ... }, { } */
            if (_check(context, LEXER_TOK_LBRACE)) {
                break;
            }
            continue;
        } else if (_check(context, LEXER_TOK_LBRACE)) {
            /* No comma, but body starts */
            break;
        } else if (_check(context, LEXER_TOK_EOF)) {
            /* EOF after parent name without body */
            _diag_error(context, "Unexpected end of file: expected blueprint body");
            return;
        } else {
            /* Unexpected token */
            _diag_error(context, "Expected ',' or blueprint body");
            _sync_to_decl(context);
            if (_check(context, LEXER_TOK_LBRACE)) {
                break;
            }
            if (_check(context, LEXER_TOK_EOF)) {
                return;
            }
            continue;
        }
    }
}

/*
 * Field parsing inside blueprint body.
 *
 * Syntax: field_name: Type (= expr)?
 *
 * Field has type annotation, optional default value.
 * Terminates with ';' which caller expects.
 *
 * Error recovery: Missing colon triggers sync to ';' or statement boundary.
 */
static void _parse_field(ParserContext* context, AstBlueprintDecl* bp)
{
    TRACE_SCOPE(context->trace);
    SourceSpan field_span = _span(context);
    const char* field_name = _sym_text(context);
    _advance(context);

    /* Expect colon after field name */
    if (!_expect(context, LEXER_TOK_COLON)) {
        _sync_to_stmt(context);
        return;
    }

    AstType* field_type = ii_parser_parse_type(context);

    /* Optional default value */
    AstExpr* default_val = nullptr;
    if (_match(context, LEXER_TOK_ASSIGN)) {
        default_val = ii_parser_parse_expression(context);
    }

    AstField* field = ii_ast_field_sp(context->ast, field_span, field_name, field_type, default_val);
    ii_ast_blueprint_add_field(bp, field);
}

/*
 * Method parameter parsing with optional 'self'.
 *
 * Handles two forms:
 * 1. Normal: param_name: Type
 * 2. Self: self: SelfType (only if first param is 'self')
 *
 * self parameter is detected by name and sets is_static=false on overload.
 *
 * Error recovery: Missing parameter name triggers sync to ')' or statement boundary.
 */
static AstMethodOverload* _parse_method_overload(ParserContext* context)
{
    TRACE_SCOPE(context->trace);
    AstMethodOverload* overload = ii_ast_method_overload_sp(context->ast, _span(context), true, nullptr);

    /* Check for self parameter: keyword 'self' followed by ')' or ',' */
    if (_check(context, LEXER_TOK_SYMBOL) &&
        _peek_current(context)->variant.symbol->type == LEXER_SYM_KEY_SELF &&
        (_peek_check(context, 1, LEXER_TOK_RPAREN) || _peek_check(context, 1, LEXER_TOK_COMMA))) {
        overload->is_static = false;
        _advance(context); /* past 'self' */

        /* self doesn't have a type annotation - type is inferred from blueprint */
        const char* self_name = ii_src_intern_cstr(context->src, "self");
        AstParam* self_param = ii_ast_param_sp(context->ast, _prev_span(context), self_name, nullptr);
        ii_ast_method_overload_add_param(overload, self_param);

        /* Comma means more parameters */
        if (_match(context, LEXER_TOK_COMMA)) {
            if (_check(context, LEXER_TOK_RPAREN)) {
                _diag_error(context, "Unexpected ',' before ')'");
            }
        } else {
            /* No more params, self only */
            _expect(context, LEXER_TOK_RPAREN);
            return overload;
        }
    }

    /* Parse remaining parameters */
    while (!_check(context, LEXER_TOK_RPAREN)) {
        /* Skip misplaced closing paren */
        if (!_check_ident(context)) {
            if (_check(context, LEXER_TOK_RPAREN)) {
                break;
            }
            _diag_error(context, "Expected parameter name");
            _sync_to_stmt(context);
            if (_check(context, LEXER_TOK_RPAREN)) {
                break;
            }
            continue;
        }

        SourceSpan param_span = _span(context);
        const char* pname = _sym_text(context);
        _advance(context);

        /* Expect colon after parameter name */
        if (!_expect(context, LEXER_TOK_COLON)) {
            _sync_to_stmt(context);
            if (_check(context, LEXER_TOK_RPAREN)) {
                break;
            }
            continue;
        }

        AstType* ptype = ii_parser_parse_type(context);

        AstParam* param = ii_ast_param_sp(context->ast, param_span, pname, ptype);
        ii_ast_method_overload_add_param(overload, param);

        /* Comma separates parameters */
        if (_match(context, LEXER_TOK_COMMA)) {
            if (_check(context, LEXER_TOK_RPAREN)) {
                _diag_error(context, "Unexpected ',' before ')'");
                break;
            }
            continue;
        }

        if (_check(context, LEXER_TOK_RPAREN)) {
            break;
        }
    }

    _expect(context, LEXER_TOK_RPAREN);
    return overload;
}

/*
 * Method parsing (name(params): ReturnType = body).
 *
 * Methods are stored in blueprint's method table.
 * Creates new method if name doesn't exist, adds overload otherwise.
 *
 * Error recovery: Missing paren triggers sync to ';' or declaration boundary.
 */
static void _parse_method(ParserContext* context, AstBlueprintDecl* bp)
{
    TRACE_SCOPE(context->trace);
    const char* method_name = _sym_text(context);
    _advance(context);

    /* Get or create method for this name */
    AstMethod* method = ii_ast_method_get_or_add(bp, method_name);
    if (method == nullptr) {
        method = ii_ast_method(context->ast, method_name);
        ii_ast_blueprint_add_method(bp, method);
    }

    /* Expect opening paren for parameters */
    if (!_expect(context, LEXER_TOK_LPAREN)) {
        _sync_to_stmt(context);
        return;
    }

    /* Parse parameter list */
    AstMethodOverload* overload = _parse_method_overload(context);
    ii_ast_method_add_overload(method, overload);

    /* Expect colon before return type */
    if (!_expect(context, LEXER_TOK_COLON)) {
        _sync_to_stmt(context);
        return;
    }

    overload->return_type = ii_parser_parse_type_annotation(context);

    /* Optional body: '=' block or ';' for declaration only */
    if (_match(context, LEXER_TOK_ASSIGN)) {
        overload->body = ii_parser_parse_block(context);
    } else {
        if (!_expect(context, LEXER_TOK_SEMICOLON)) {
            _sync_to_stmt(context);
        }
    }
}

/*
 * Blueprint body parsing (fields and methods).
 *
 * Loop until closing '}'.
 * Each member is either:
 * - Field: name: Type; (detected by ':' after name)
 * - Method: name(params): Type; (detected by '(' after name)
 *
 * Error recovery: Unknown token triggers sync to '}'.
 */
static void _parse_blueprint_body(ParserContext* context, AstBlueprintDecl* bp)
{
    TRACE_SCOPE(context->trace);
    while (!_check(context, LEXER_TOK_RBRACE)) {
        /* EOF inside blueprint body */
        if (_check(context, LEXER_TOK_EOF)) {
            _diag_error(context, "Unexpected end of file in blueprint body");
            _sync_to_decl(context);
            return;
        }

        /* Each member starts with identifier */
        if (!_check_ident(context)) {
            _diag_error(context, "Expected field or method declaration");
            _sync_to_stmt(context);
            continue;
        }

        /* Two-peek lookahead: ':' means field, '(' means method */
        if (_peek_check(context, 1, LEXER_TOK_COLON)) {
            _parse_field(context, bp);
            /* Field terminates with ';' */
            if (!_expect(context, LEXER_TOK_SEMICOLON)) {
                _sync_to_stmt(context);
                continue;
            }
        } else if (_peek_check(context, 1, LEXER_TOK_LPAREN)) {
            /* Method */
            _parse_method(context, bp);
        } else {
            /* Identifier not followed by ':' or '(' */
            _diag_error(context, "Expected field or method declaration");
            _advance(context);
            _sync_to_stmt(context);
            continue;
        }
    }
}

/*
 * Blueprint declaration parsing.
 *
 * Syntax: Name: (Parent { aliases }, ...)? { body }
 *
 * Entry already consumed the name (caller checked).
 * Expects colon, optional inheritance list, then body.
 *
 * Error recovery: Missing parts trigger sync to '{' or declaration boundary.
 */
static AstBlueprintDecl* _parse_blueprint(ParserContext* context)
{
    TRACE_SCOPE(context->trace);
    SourceSpan name_span = _span(context);
    const char* name = _sym_text(context);
    _advance(context);

    /* Expect colon after name */
    if (!_expect(context, LEXER_TOK_COLON)) {
        _sync_to_decl(context);
        return nullptr;
    }

    AstBlueprintDecl* bp = ii_ast_blueprint_decl_sp(context->ast, name_span, name);

    /* Parse optional inheritance list */
    _parse_inheritance_list(context, bp);

    /* Expect body */
    if (!_expect(context, LEXER_TOK_LBRACE)) {
        _sync_to_decl(context);
        return bp;
    }

    _parse_blueprint_body(context, bp);

    if (!_expect(context, LEXER_TOK_RBRACE)) {
        _sync_to_decl(context);
    }

    ii_ast_program_add_blueprint(context->ast, bp);
    return bp;
}

/*
 * Function parameter parsing.
 *
 * Syntax: param_name: Type
 *
 * Error recovery: Missing parameter name triggers sync to ')' or statement boundary.
 */
static void _parse_func_param(ParserContext* context, AstFuncDecl* func)
{
    TRACE_SCOPE(context->trace);
    if (!_check_ident(context)) {
        _diag_error(context, "Expected parameter name");
        _sync_to_stmt(context);
        return;
    }

    SourceSpan param_span = _span(context);
    const char* pname = _sym_text(context);
    _advance(context);

    /* Expect colon after parameter name */
    if (!_expect(context, LEXER_TOK_COLON)) {
        _sync_to_stmt(context);
        return;
    }

    AstType* ptype = ii_parser_parse_type_annotation(context);

    AstParam* param = ii_ast_param_sp(context->ast, param_span, pname, ptype);
    ii_ast_func_add_param(func, param);
}

/*
 * Function declaration parsing.
 *
 * Syntax: name(params): ReturnType (= body)?
 *
 * Parameters parsed in loop until ')'.
 * Body is optional: '=' block or ';' for declaration only.
 *
 * Error recovery: EOF in parameter list triggers sync to ')' or declaration boundary.
 */
static AstFuncDecl* _parse_func(ParserContext* context)
{
    TRACE_SCOPE(context->trace);
    SourceSpan name_span = _span(context);
    const char* name = _sym_text(context);
    _advance(context);

    AstFuncDecl* func = ii_ast_func_decl_sp(context->ast, name_span, name, nullptr);

    /* Expect opening paren */
    if (!_expect(context, LEXER_TOK_LPAREN)) {
        _sync_to_decl(context);
        return func;
    }

    /* Parse parameters */
    while (!_check(context, LEXER_TOK_RPAREN)) {
        /* EOF in parameter list */
        if (_check(context, LEXER_TOK_EOF)) {
            break;
        }
        _parse_func_param(context, func);
        /* Comma separates parameters */
        if (!_match(context, LEXER_TOK_COMMA)) {
            break;
        }
        /* Trailing comma before ')' */
        if (_check(context, LEXER_TOK_RPAREN)) {
            _diag_error(context, "Unexpected ',' before ')'");
            break;
        }
    }

    /* Expect closing paren */
    if (!_expect(context, LEXER_TOK_RPAREN)) {
        _sync_to_decl(context);
    }

    /* Expect colon before return type */
    if (!_expect(context, LEXER_TOK_COLON)) {
        _sync_to_decl(context);
        return func;
    }

    func->return_type = ii_parser_parse_type_annotation(context);

    /* Optional body */
    if (_match(context, LEXER_TOK_ASSIGN)) {
        func->body = ii_parser_parse_block(context);
    } else {
        if (!_expect(context, LEXER_TOK_SEMICOLON)) {
            _sync_to_decl(context);
        }
    }

    ii_ast_program_add_func(context->ast, func);
    return func;
}

/*
 * Top-level declaration entry point.
 *
 * Distinguishes blueprints from functions using two-token lookahead:
 * - name:  -> blueprint
 * - name( -> function
 *
 * Error recovery: Non-identifier or unexpected token triggers sync to declaration boundary.
 */
AstNode* ii_parser_parse_declaration(ParserContext* context)
{
    TRACE_SCOPE(context->trace);

    /* Top-level declarations must start with identifier */
    if (!_check_ident(context)) {
        _diag_error(context, "Expected function or blueprint declaration");
        _advance(context);
        _sync_to_decl(context);
        return nullptr;
    }

    /* Two-peek lookahead: ':' means blueprint, '(' means function */
    if (_peek_check(context, 1, LEXER_TOK_COLON)) {
        return (AstNode*)_parse_blueprint(context);
    } else if (_peek_check(context, 1, LEXER_TOK_LPAREN)) {
        return (AstNode*)_parse_func(context);
    } else {
        /* Identifier not followed by ':' or '(' */
        _diag_error(context, "Expected ':' or '(' after identifier");
        _advance(context);
        _sync_to_decl(context);
        return nullptr;
    }
}
