/**
 * @brief Statement parser.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_parser_stmt.h"

#include "ii_parser_expr.h"
#include "ii_parser_internal.h"
#include "ii_parser_type.h"

/*
 * Return statement parsing.
 *
 * Syntax: return expr? ';'
 *
 * Value is optional. Three cases for value:
 * 1. ';' immediately -> no value (void return)
 * 2. '{' immediately -> block expression (return the block)
 * 3. Otherwise -> parse expression
 *
 * Trailing brace detection allows: return { x; }
 */
static AstStmt* _parse_return(ParserContext* context)
{
    TRACE_SCOPE(context->trace);
    SourceSpan keyword_span = _span(context);
    _advance(context);

    AstExpr* value = nullptr;
    if (_check(context, LEXER_TOK_SEMICOLON)) {
        /* No value - void return */
        value = nullptr;
    } else if (_check(context, LEXER_TOK_LBRACE)) {
        /* Block expression as return value */
        value = ii_parser_parse_expression(context);
    } else if (!_check(context, LEXER_TOK_RBRACE) && !_check(context, LEXER_TOK_EOF)) {
        /* Parse expression value */
        value = ii_parser_parse_expression(context);
    }

    _expect(context, LEXER_TOK_SEMICOLON);
    return (AstStmt*)ii_ast_return_sp(context->ast, keyword_span, value);
}

/*
 * Variable declaration statement parsing.
 *
 * Syntax: var name: Type (= expr)? ';'
 *
 * Creates declaration statement with optional initializer.
 *
 * Error recovery: Missing name triggers error node and sync to statement boundary.
 */
static AstStmt* _parse_decl(ParserContext* context)
{
    TRACE_SCOPE(context->trace);
    SourceSpan keyword_span = _span(context);
    _advance(context);

    /* Expect variable name */
    if (!_check_ident(context)) {
        _diag_error(context, "Expected variable name");
        _sync_to_stmt(context);
        return (AstStmt*)ii_ast_expr_stmt(context->ast, (AstExpr*)ii_ast_error(context->ast, keyword_span));
    }

    const char* name = _sym_text(context);
    _advance(context);

    /* Expect colon after name */
    if (!_expect(context, LEXER_TOK_COLON)) {
        _sync_to_stmt(context);
        return (AstStmt*)ii_ast_expr_stmt(context->ast, (AstExpr*)ii_ast_error(context->ast, keyword_span));
    }

    AstType* type = ii_parser_parse_type_annotation(context);

    /* Optional initializer */
    AstExpr* init = nullptr;
    if (_match(context, LEXER_TOK_ASSIGN)) {
        init = ii_parser_parse_expression(context);
        if (init != nullptr && init->base.kind == AST_KIND_INIT) {
            ((AstInit*)init)->target_type = type;
        }
    }

    _expect(context, LEXER_TOK_SEMICOLON);
    return (AstStmt*)ii_ast_decl_sp(context->ast, keyword_span, name, type, init, true);
}

/*
 * If statement parsing.
 *
 * Syntax: if (expr) stmt (else stmt)?
 *
 * Else is optional. If present, parses the else branch.
 *
 * Error recovery: Parentheses handled by _expect helpers.
 */
static AstStmt* _parse_if(ParserContext* context)
{
    TRACE_SCOPE(context->trace);
    SourceSpan keyword_span = _span(context);
    _advance(context);

    _expect(context, LEXER_TOK_LPAREN);
    AstExpr* cond = ii_parser_parse_expression(context);
    _expect(context, LEXER_TOK_RPAREN);

    /* Then branch */
    AstStmt* then_stmt = ii_parser_parse_statement(context);

    /* Optional else */
    AstStmt* else_stmt = nullptr;
    if (_check_sym(context, LEXER_SYM_KEY_ELSE)) {
        _advance(context);
        else_stmt = ii_parser_parse_statement(context);
    }

    return (AstStmt*)ii_ast_if_sp(context->ast, keyword_span, cond, (AstBlock*)then_stmt, else_stmt);
}

/*
 * While loop parsing.
 *
 * Syntax: while (expr) stmt
 *
 * Condition in parentheses, single body statement.
 *
 * Error recovery: Parentheses handled by _expect helpers.
 */
static AstStmt* _parse_while(ParserContext* context)
{
    TRACE_SCOPE(context->trace);
    SourceSpan keyword_span = _span(context);
    _advance(context);

    _expect(context, LEXER_TOK_LPAREN);
    AstExpr* cond = ii_parser_parse_expression(context);
    _expect(context, LEXER_TOK_RPAREN);

    AstStmt* body_stmt = ii_parser_parse_statement(context);

    return (AstStmt*)ii_ast_while_sp(context->ast, keyword_span, cond, (AstBlock*)body_stmt);
}

/*
 * Do-while loop parsing.
 *
 * Syntax: do stmt while (expr);
 *
 * Body executes first, then condition checked.
 * Note: Unlike C, semicolon is required after ')'.
 *
 * Error recovery: Missing 'while' after body - report error, use nullptr condition.
 */
static AstStmt* _parse_do_while(ParserContext* context)
{
    TRACE_SCOPE(context->trace);
    SourceSpan keyword_span = _span(context);
    _advance(context);

    /* Parse body first (do-while checks condition after) */
    AstStmt* body_stmt = ii_parser_parse_statement(context);

    /* Expect 'while' keyword after body */
    if (!_check_sym(context, LEXER_SYM_KEY_WHILE)) {
        _diag_error(context, "Expected 'while' after do block");
        _sync_to_stmt(context);
        return (AstStmt*)ii_ast_do_while_sp(context->ast, keyword_span, (AstBlock*)body_stmt, nullptr);
    }

    _advance(context);
    _expect(context, LEXER_TOK_LPAREN);
    AstExpr* cond = ii_parser_parse_expression(context);
    _expect(context, LEXER_TOK_RPAREN);
    _expect(context, LEXER_TOK_SEMICOLON);

    return (AstStmt*)ii_ast_do_while_sp(context->ast, keyword_span, (AstBlock*)body_stmt, cond);
}

/*
 * For loop parsing.
 *
 * Syntax: for (init; cond; iter) stmt
 *
 * Each clause is optional:
 * - init: var decl, expression, or empty
 * - cond: expression or empty
 * - iter: expression or empty
 *
 * Error recovery: Handled by _expect for separators.
 */
static AstStmt* _parse_for(ParserContext* context)
{
    TRACE_SCOPE(context->trace);
    SourceSpan keyword_span = _span(context);
    _advance(context);

    _expect(context, LEXER_TOK_LPAREN);

    /* Init clause: can be var decl, expr, or empty */
    AstStmt* init = nullptr;
    if (!_check(context, LEXER_TOK_SEMICOLON)) {
        if (_check_sym(context, LEXER_SYM_KEY_VAR)) {
            /* Variable declaration */
            SourceSpan span = _span(context);
            _advance(context);

            if (_check_ident(context)) {
                const char* name = _sym_text(context);
                _advance(context);

                if (_expect(context, LEXER_TOK_COLON)) {
                    AstType* type = ii_parser_parse_type(context);

                    /* Optional initializer in declaration */
                    AstExpr* init_val = nullptr;
                    if (_match(context, LEXER_TOK_ASSIGN)) {
                        init_val = ii_parser_parse_expression(context);
                    }

                    init = (AstStmt*)ii_ast_decl_sp(context->ast, span, name, type, init_val, true);
                }
            }
        } else {
            /* Expression statement */
            AstExpr* expr = ii_parser_parse_expression(context);
            init = (AstStmt*)ii_ast_expr_stmt_sp(context->ast, _span(context), expr);
        }
    }
    _expect(context, LEXER_TOK_SEMICOLON);

    /* Condition clause: expression or empty */
    AstExpr* cond = nullptr;
    if (!_check(context, LEXER_TOK_SEMICOLON)) {
        cond = ii_parser_parse_expression(context);
    }
    _expect(context, LEXER_TOK_SEMICOLON);

    /* Iter clause: expression or empty */
    AstExpr* iter = nullptr;
    if (!_check(context, LEXER_TOK_RPAREN)) {
        iter = ii_parser_parse_expression(context);
    }
    _expect(context, LEXER_TOK_RPAREN);

    AstStmt* body_stmt = ii_parser_parse_statement(context);

    return (AstStmt*)ii_ast_for_sp(context->ast, keyword_span, init, cond, iter, (AstBlock*)body_stmt);
}

/*
 * Expression statement parsing.
 *
 * Syntax: expr;
 *
 * Parses expression followed by semicolon.
 * Used for function calls, assignments, etc.
 */
static AstStmt* _parse_expr_stmt(ParserContext* context)
{
    TRACE_SCOPE(context->trace);
    SourceSpan expr_span = _span(context);
    AstExpr* expr = ii_parser_parse_expression(context);
    _expect(context, LEXER_TOK_SEMICOLON);
    return (AstStmt*)ii_ast_expr_stmt_sp(context->ast, expr_span, expr);
}

/*
 * Block parsing.
 *
 * Syntax: { stmt* }
 *
 * Expects opening brace. Parses statements until closing brace.
 * Accumulates statements in block node.
 *
 * Error recovery: EOF inside block triggers sync to statement boundary.
 */
AstBlock* ii_parser_parse_block(ParserContext* context)
{
    TRACE_SCOPE(context->trace);

    SourceSpan brace_span = _span(context);
    /* Expect opening brace */
    if (!_expect(context, LEXER_TOK_LBRACE)) {
        return ii_ast_block_sp(context->ast, brace_span);
    }

    AstBlock* block = ii_ast_block_sp(context->ast, brace_span);

    while (!_check(context, LEXER_TOK_RBRACE)) {
        /* EOF inside block */
        if (_check(context, LEXER_TOK_EOF)) {
            _diag_error(context, "Unexpected end of file inside block");
            _sync_to_stmt(context);
            break;
        }

        /* Parse statement, add to block */
        AstStmt* stmt = ii_parser_parse_statement(context);
        if (stmt != nullptr) {
            ii_ast_block_add_stmt(block, stmt);
        }
    }

    _expect(context, LEXER_TOK_RBRACE);
    return block;
}

/*
 * Statement dispatch.
 *
 * Examines current token and dispatches to appropriate parser:
 * - '{' -> block
 * - Keywords (var, if, while, for, return, break, continue) -> their parsers
 * - ';' -> empty statement
 * - EOF -> nullptr
 * - Otherwise -> expression statement
 *
 * Dispatch uses symbol type for keywords since they're all LEXER_TOK_SYMBOL.
 */
AstStmt* ii_parser_parse_statement(ParserContext* context)
{
    TRACE_SCOPE(context->trace);

    LexerToken* tok = _peek_current(context);

    /* Block is standalone statement */
    if (tok->type == LEXER_TOK_LBRACE) {
        return (AstStmt*)ii_parser_parse_block(context);
    }

    /* Keyword statements */
    if (tok->type == LEXER_TOK_SYMBOL) {
        switch (tok->variant.symbol->type) {
        case LEXER_SYM_KEY_VAR:
            return _parse_decl(context);
        case LEXER_SYM_KEY_IF:
            return _parse_if(context);
        case LEXER_SYM_KEY_WHILE:
            return _parse_while(context);
        case LEXER_SYM_KEY_DO:
            return _parse_do_while(context);
        case LEXER_SYM_KEY_FOR:
            return _parse_for(context);
        case LEXER_SYM_KEY_RETURN:
            return _parse_return(context);
        case LEXER_SYM_KEY_BREAK: {
            SourceSpan span = _span(context);
            _advance(context);
            _expect(context, LEXER_TOK_SEMICOLON);
            return (AstStmt*)ii_ast_break_sp(context->ast, span);
        }
        case LEXER_SYM_KEY_CONTINUE: {
            SourceSpan span = _span(context);
            _advance(context);
            _expect(context, LEXER_TOK_SEMICOLON);
            return (AstStmt*)ii_ast_continue_sp(context->ast, span);
        }
        default:
            break;
        }
    }

    /* Empty statement: just ';' */
    if (tok->type == LEXER_TOK_SEMICOLON) {
        SourceSpan span = _span(context);
        _advance(context);
        return (AstStmt*)ii_ast_block_sp(context->ast, span);
    }

    /* EOF outside block - return nullptr to signal end */
    if (tok->type == LEXER_TOK_EOF) {
        return nullptr;
    }

    /* Anything else is an expression statement */
    return _parse_expr_stmt(context);
}
