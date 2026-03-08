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
 * Here is a little cheat-sheet for the parser structures:
 * - Program -> Declaration*
 * - Declaration -> FuncDecl || BlueprintDecl
 * - FuncDecl -> IDENTIFIER '(' ParamList? ')' ':' Type ('=' Block)?
 * - BlueprintDecl -> IDENTIFIER ':' '{' Field* Method* '}'
 * - Field -> IDENTIFIER ':' Type ('=' Expression)?
 * - Method -> IDENTIFIER '(' ParamList? ')' ':' Type ('=' Block)?
 * - Parameter -> IDENTIFIER ':' Type
 * - Block -> '{' Statement* '}'
 * - Statement -> ReturnStmt || DeclStmt || ExprStmt || IfStmt || ForStmt || WhileStmt
 *
 * Expressions use precedence climbing:
 * - Each non-terminal (program, declaration, statement, expression) has a dedicated parsing function.
 * - Functions call each other recursively to build the tree.
 * - The call stack mirrors the nested structure of the code.
 * (source: Wikipedia)
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

#include <ctype.h>
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

    /* Current blueprint name being parsed (for method self param type resolution) */
    const char* current_blueprint_name;
};

/*
 * I have copied this from ii_lexer.c for error messages. I should probably move it into some shared header.
 * TODO: move the somewhere else.
 */
static const char* _token_type_to_string[] = {
    "LEXER_TOK_EOF",          "LEXER_TOK_ERROR",       "LEXER_TOK_SYMBOL",       "LEXER_TOK_LBRACE",
    "LEXER_TOK_RBRACE",       "LEXER_TOK_LPAREN",      "LEXER_TOK_RPAREN",       "LEXER_TOK_LBRACKET",
    "LEXER_TOK_RBRACKET",     "LEXER_TOK_QUESTION",    "LEXER_TOK_COLON",        "LEXER_TOK_SEMICOLON",
    "LEXER_TOK_COMMA",        "LEXER_TOK_DOT",         "LEXER_TOK_DOLLAR",       "LEXER_TOK_ASSIGN",
    "LEXER_TOK_PLUS",         "LEXER_TOK_MINUS",       "LEXER_TOK_STAR",         "LEXER_TOK_SLASH",
    "LEXER_TOK_PERCENT",      "LEXER_TOK_PLUS_ASSIGN", "LEXER_TOK_MINUS_ASSIGN", "LEXER_TOK_STAR_ASSIGN",
    "LEXER_TOK_SLASH_ASSIGN", "LEXER_TOK_EQ",          "LEXER_TOK_NEQ",          "LEXER_TOK_LT",
    "LEXER_TOK_GT",           "LEXER_TOK_LTE",         "LEXER_TOK_GTE",          "LEXER_TOK_AND",
    "LEXER_TOK_OR",           "LEXER_TOK_NOT",         "LEXER_TOK_BIT_AND",      "LEXER_TOK_BIT_OR",
    "LEXER_TOK_BIT_XOR",      "LEXER_TOK_BIT_NOT",
};

/*
 * These functions provide safe access to the Token Vector/array. They are logically almost the same as the
 * ones in lexer implementation.
 */

static inline bool _is_end(const ParserContext* context)
{
    return context->current_index >= context->token_count ||
           context->tokens[context->current_index].type == LEXER_TOK_EOF;
}

static inline const LexerToken* _peek_current(const ParserContext* context)
{
    if _unlikely_ (_is_end(context)) {
        return &context->tokens[context->token_count - 1]; /* Return EOF */
    }

    return &context->tokens[context->current_index];
}

static inline const LexerToken* _peek_next(const ParserContext* context)
{
    if _unlikely_ (context->current_index + 1 >= context->token_count) {
        return &context->tokens[context->token_count - 1];
    }

    return &context->tokens[context->current_index + 1];
}

static inline const LexerToken* _advance(ParserContext* context)
{
    if _unlikely_ (!_is_end(context)) {
        context->current_index++;
    }

    return &context->tokens[context->current_index - 1];
}

static inline bool _check(const ParserContext* context, enum LexerTokenType type)
{
    if _unlikely_ (_is_end(context)) {
        return false;
    }

    return _peek_current(context)->type == type;
}

static bool _match(ParserContext* context, enum LexerTokenType type)
{
    if (_check(context, type)) {
        _advance(context);
        return true;
    }
    return false;
}

static const LexerToken* _expect(ParserContext* context, enum LexerTokenType type, const char* expected)
{
    if (_check(context, type)) {
        return _advance(context);
    }

    const LexerToken* current = _peek_current(context);
    ii_diag_report(context->diag_context, UF_LOG_ERROR, current->span, "Expected '%s' but found '%s'",
                   expected,
                   current->type == LEXER_TOK_SYMBOL ? current->variant.symbol->text
                                                     : _token_type_to_string[current->type]);

    return current;
}

/**
 * This is the synchronization function. We call it when something goes wrong. It skips tokens until we find a
 * synchronization point. This allows us to report multiple errors per compilation unit.
 *
 * Synchronization points are:
 * - Semicolon
 * - Opening brace
 * - Closing brace
 * - keyword
 * - EOF
 **/
static void _synchronize(ParserContext* context)
{
    while (!_is_end(context)) {
        const LexerToken* token = _peek_current(context);
        enum LexerTokenType type = token->type;

        if (type == LEXER_TOK_SEMICOLON || type == LEXER_TOK_LBRACE || type == LEXER_TOK_RBRACE ||
            type == LEXER_TOK_EOF) {
            break;
        }

        if (type == LEXER_TOK_SYMBOL && token->variant.symbol) {
            enum LexerSymbolType sym_type = token->variant.symbol->type;
            if (sym_type == LEXER_SYM_KEY_VAR || sym_type == LEXER_SYM_KEY_IF ||
                sym_type == LEXER_SYM_KEY_FOR || sym_type == LEXER_SYM_KEY_WHILE ||
                sym_type == LEXER_SYM_KEY_RETURN) {
                break;
            }
        }

        _advance(context);
    }
}

/**
 * This is used to track an allocated node in the nodes Vector. We use the Vector to iterate over all nodes
 * and execute proper cleanup.
 **/
static void _track_node(ParserContext* context, AstNode* node)
{
    uf_con_vector_push(context->node_vector, &node);
}

/*
 * These parsing functions are declared here so they can call each other in any order. Implementation follows
 * below. Each of them is responsible for one type of grammar rule.
 */

static struct AstFuncDecl* _parse_func_decl(ParserContext*);
static struct AstBlueprintDecl* _parse_blueprint_decl(ParserContext*);
static struct AstBlock* _parse_block(ParserContext*);
static struct AstStmt* _parse_stmt(ParserContext*);
static struct AstExpr* _parse_expression(ParserContext*);
static struct AstType* _parse_type(ParserContext*);
static struct AstField* _parse_field(ParserContext*);
static struct AstParam* _parse_param(ParserContext*, const char* blueprint_name);
static struct AstParam* _parse_param_list(ParserContext*, size_t* out_count, const char* blueprint_name);
static struct AstMethod* _parse_method(ParserContext*);
static struct AstReturnStmt* _parse_return_stmt(ParserContext*);
static struct AstDeclStmt* _parse_decl_stmt(ParserContext*);
static struct AstIfStmt* _parse_if_stmt(ParserContext*);
static struct AstForStmt* _parse_for_stmt(ParserContext*);
static struct AstWhileStmt* _parse_while_stmt(ParserContext*);
static struct AstBreakStmt* _parse_break_stmt(ParserContext*);
static struct AstContinueStmt* _parse_continue_stmt(ParserContext*);

/*
 * We'll start with functions for parsing the top-level (root) of IIRA source file.
 */

/**
 * Parse a function declaration. Functions are only allowed at the top-level (root), so the only function that
 * calls this one is 'ii_parser_context_new'.
 *
 * Grammar: IDENTIFIER '(' ParamList? ')' ':' Type ('=' Block)?
 **/
static struct AstFuncDecl* _parse_func_decl(ParserContext* context)
{
    const LexerToken* first = _peek_current(context);

    /* Expecting a function name. */
    if (first->type != LEXER_TOK_SYMBOL || !first->variant.symbol ||
        first->variant.symbol->type != LEXER_SYM_IDENTIFIER) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected function name");
        return nullptr;
    }

    const char* func_name = first->variant.symbol->text;
    const SourceSpan name_span = first->span;
    _advance(context); /* Consume function name. */

    /* Expect opening paren. */
    if (!_match(context, LEXER_TOK_LPAREN)) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected '(' after function name");
        return nullptr;
    }

    size_t param_count = 0;
    struct AstParam* params = _parse_param_list(context, &param_count, nullptr);

    /* Expect closing paren. */
    if (!_expect(context, LEXER_TOK_RPAREN, ")")) {
        /* TODO: here we should implement proper error handling... you'll see this trend of not properly
         * handling errors a lot :D. */
    }

    /* Expect colon. */
    if (!_match(context, LEXER_TOK_COLON)) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected ':' after parameter list");
        return nullptr;
    }

    struct AstType* return_type = _parse_type(context);
    if (!return_type) {
        return nullptr;
    }

    /* Create function declaration node */
    struct AstFuncDecl* func = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstFuncDecl));
    func->base.type = AST_FUNC_DECL;
    func->base.span = name_span;
    func->name = func_name;
    func->params = params;
    func->param_count = param_count;
    func->return_type = return_type;
    func->body = nullptr;

    /* Parse optional function body */
    if (_match(context, LEXER_TOK_ASSIGN)) {
        func->body = _parse_block(context);
        if (!func->body) {
            ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                           "Expected function body");
        }
    }

    return func;
}

/**
 * This parsers a blueprint declaration. Again, it's only top-level (root). It's one of the more complex ones.
 *
 * Grammar: IDENTIFIER ':' '{' Member* '}'
 *
 * Members can be:
 * - Fields: IDENTIFIER ':' Type ('=' Expr)?
 * - Methods: IDENTIFIER '(' ParamList? ')' ':' Type ('=' Block)?
 **/
static struct AstBlueprintDecl* _parse_blueprint_decl(ParserContext* context)
{
    const LexerToken* first = _peek_current(context);

    /* Expecting a blueprint name. */
    if (first->type != LEXER_TOK_SYMBOL || !first->variant.symbol ||
        first->variant.symbol->type != LEXER_SYM_IDENTIFIER) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected blueprint name");
        return nullptr;
    }

    const char* blueprint_name = first->variant.symbol->text;
    const SourceSpan blueprint_span = first->span;
    _advance(context); /* Consume blueprint name. */

    /* Expect colon. */
    if (!_match(context, LEXER_TOK_COLON)) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected ':' after blueprint name");
        return nullptr;
    }

    /* Parse inheritance: IDENTIFIER (',' IDENTIFIER ('as' IDENTIFIER)?)* */
    _autovector_ UfConVector* parents_vec = uf_con_vector_new(sizeof(struct {
        const char* name;
        const char* alias;
    }));

    while (_check(context, LEXER_TOK_SYMBOL) && _peek_current(context)->variant.symbol &&
           _peek_current(context)->variant.symbol->type == LEXER_SYM_IDENTIFIER) {
        const char* parent_name = _peek_current(context)->variant.symbol->text;
        _advance(context);

        const char* alias = nullptr;
        if (_check(context, LEXER_TOK_SYMBOL) && _peek_current(context)->variant.symbol &&
            _peek_current(context)->variant.symbol->type == LEXER_SYM_KEY_AS) {
            _advance(context);
            if (!_check(context, LEXER_TOK_SYMBOL) || !_peek_current(context)->variant.symbol ||
                _peek_current(context)->variant.symbol->type != LEXER_SYM_IDENTIFIER) {
                ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                               "Expected alias name after 'as'");
                return nullptr;
            }
            alias = _peek_current(context)->variant.symbol->text;
            _advance(context);
        }

        struct {
            const char* name;
            const char* alias;
        } parent_entry = {parent_name, alias};
        uf_con_vector_push(parents_vec, &parent_entry);

        if (!_match(context, LEXER_TOK_COMMA)) {
            break;
        }
    }

    size_t parent_count = uf_con_vector_length(parents_vec);

    /* Expect opening brace. */
    if (!_match(context, LEXER_TOK_LBRACE)) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected '{' in blueprint declaration");
        return nullptr;
    }

    struct AstBlueprintDecl* blueprint =
        uf_mem_region_zalloc(context->node_arena, sizeof(struct AstBlueprintDecl));
    blueprint->base.type = AST_BLUEPRINT_DECL;
    blueprint->base.span = blueprint_span;
    blueprint->name = blueprint_name;

    if (parent_count > 0) {
        blueprint->parents =
            uf_mem_region_zalloc(context->node_arena, sizeof(*blueprint->parents) * parent_count);
        blueprint->parent_count = parent_count;
        for (size_t i = 0; i < parent_count; i++) {
            struct {
                const char* name;
                const char* alias;
            }* p = uf_con_vector_get(parents_vec, i);
            blueprint->parents[i].name = p->name;
            blueprint->parents[i].alias = p->alias;
        }
    } else {
        blueprint->parents = nullptr;
        blueprint->parent_count = 0;
    }

    /* Parse members. */
    UfConVector* fields_vector_tmp = uf_con_vector_new(sizeof(struct AstField*));
    UfConVector* methods_vector_tmp = uf_con_vector_new(sizeof(struct AstMethod*));

    while (!_check(context, LEXER_TOK_RBRACE) && !_is_end(context)) {
        /* Skip semicolons between members. */
        if (_match(context, LEXER_TOK_SEMICOLON)) {
            /* TODO: report another unnecessary semicolon. */
            continue;
        }

        const LexerToken* member_token = _peek_current(context);

        /* Check if this looks like a method (identifier followed by paren). */
        if (member_token->type == LEXER_TOK_SYMBOL && member_token->variant.symbol &&
            member_token->variant.symbol->type == LEXER_SYM_IDENTIFIER) {

            if (_peek_next(context)->type == LEXER_TOK_LPAREN) {
                /* It's a method. Set blueprint name for self param type resolution. */
                const char* prev_blueprint = context->current_blueprint_name;
                context->current_blueprint_name = blueprint_name;

                struct AstMethod* method = _parse_method(context);

                context->current_blueprint_name = prev_blueprint;

                if (method) {
                    uf_con_vector_push(methods_vector_tmp, &method);
                }
                continue;
            }
        }

        /* Otherwise, treat it as field. */
        struct AstField* field = _parse_field(context);
        if (field) {
            uf_con_vector_push(fields_vector_tmp, &field);
        } else {
            _synchronize(context);
        }
    }

    size_t field_count = uf_con_vector_length(fields_vector_tmp);

    /* Copy fields to arena. */
    if (field_count > 0) {
        blueprint->fields = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstField) * field_count);
        blueprint->field_count = field_count;

        for (size_t i = 0; i < field_count; i++) {
            struct AstField** f = uf_con_vector_get(fields_vector_tmp, i);
            memcpy(&blueprint->fields[i], *f, sizeof(struct AstField));
        }
    } else {
        blueprint->fields = nullptr;
        blueprint->field_count = 0;
    }

    /* Copy methods to the arena. */
    size_t method_count = uf_con_vector_length(methods_vector_tmp);
    if (method_count > 0) {
        blueprint->methods =
            uf_mem_region_zalloc(context->node_arena, sizeof(struct AstMethod) * method_count);
        blueprint->method_count = method_count;
        for (size_t i = 0; i < method_count; i++) {
            struct AstMethod** m = uf_con_vector_get(methods_vector_tmp, i);
            memcpy(&blueprint->methods[i], *m, sizeof(struct AstMethod));
        }
    } else {
        blueprint->methods = nullptr;
        blueprint->method_count = 0;
    }

    uf_con_vector_free(fields_vector_tmp);
    uf_con_vector_free(methods_vector_tmp);

    /* Expect closing brace. */
    if (!_expect(context, LEXER_TOK_RBRACE, "}")) {
        /* TODO: try to recover properly. */
    }

    return blueprint;
}

/*
 * Now functions for parsing statements.
 */

/**
 * Parse a generic statements. We use lookahead to determine which kind of statement to parse. This is the
 * main dispatch point for statement parsing.
 *
 * Grammar: Statement ->
 *   || ReturnStmt
 *   || DeclStmt
 *   || ExprStmt
 *   || IfStmt
 *   || ForStmt
 *   || WhileStmt
 *   || Block
 *   || BreakStmt
 *   || ContinueStmt
 **/
static struct AstStmt* _parse_stmt(ParserContext* context)
{
    const LexerToken* first = _peek_current(context);

    /* Block statement. */
    if (first->type == LEXER_TOK_LBRACE) {
        return (struct AstStmt*)_parse_block(context);
    }

    /* Return statement. */
    if (first->type == LEXER_TOK_SYMBOL && first->variant.symbol &&
        first->variant.symbol->type == LEXER_SYM_KEY_RETURN) {
        return (struct AstStmt*)_parse_return_stmt(context);
    }

    /* Variable declaration. */
    if (first->type == LEXER_TOK_SYMBOL && first->variant.symbol &&
        (first->variant.symbol->type == LEXER_SYM_KEY_VAR ||
         (first->variant.symbol->type == LEXER_SYM_IDENTIFIER &&
          _peek_next(context)->type == LEXER_TOK_COLON))) {
        return (struct AstStmt*)_parse_decl_stmt(context);
    }

    /* If statement. */
    if (first->type == LEXER_TOK_SYMBOL && first->variant.symbol &&
        first->variant.symbol->type == LEXER_SYM_KEY_IF) {
        return (struct AstStmt*)_parse_if_stmt(context);
    }

    /* For statement. */
    if (first->type == LEXER_TOK_SYMBOL && first->variant.symbol &&
        first->variant.symbol->type == LEXER_SYM_KEY_FOR) {
        return (struct AstStmt*)_parse_for_stmt(context);
    }

    /* While statement (includes do-while). */
    if (first->type == LEXER_TOK_SYMBOL && first->variant.symbol &&
        (first->variant.symbol->type == LEXER_SYM_KEY_WHILE ||
         first->variant.symbol->type == LEXER_SYM_KEY_DO)) {
        return (struct AstStmt*)_parse_while_stmt(context);
    }

    /* Break statement. */
    if (first->type == LEXER_TOK_SYMBOL && first->variant.symbol &&
        first->variant.symbol->type == LEXER_SYM_KEY_BREAK) {
        return (struct AstStmt*)_parse_break_stmt(context);
    }

    /* Continue statement. */
    if (first->type == LEXER_TOK_SYMBOL && first->variant.symbol &&
        first->variant.symbol->type == LEXER_SYM_KEY_CONTINUE) {
        return (struct AstStmt*)_parse_continue_stmt(context);
    }

    /* Expression statement. */
    /* This handles function calls, assignments, etc. */
    struct AstExpr* expression = _parse_expression(context);
    if (expression) {
        struct AstExprStmt* expr_stmt = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstExprStmt));
        expr_stmt->base.type = AST_EXPR_STMT;
        expr_stmt->base.span = expression->base.span;
        expr_stmt->expr = expression;
        return (struct AstStmt*)expr_stmt;
    }

    /* Unknown statement */
    ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span, "Expected statement");
    return nullptr;
}

/**
 * A block introduces a new scope and contains zero or more statements. The parser recursively calls
 * '_parse_statement' for each statement in the block.
 *
 * Grammar: Block -> '{' Statement* '}'
 **/
static struct AstBlock* _parse_block(ParserContext* context)
{
    const LexerToken* first = _peek_current(context);

    /* Expect opening brace. */
    if (!_match(context, LEXER_TOK_LBRACE)) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span, "Expected '{'");
        return nullptr;
    }

    /* Create block node. */
    struct AstBlock* block = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstBlock));
    block->base.type = AST_BLOCK;
    block->base.span = first->span;
    block->local_scope = nullptr;

    struct UfConVector* statements_vector_tmp = uf_con_vector_new(sizeof(struct AstStmt*));

    /* Parse statements. */
    while (!_check(context, LEXER_TOK_RBRACE) && !_is_end(context)) {
        /* Skip semicolons between statements. */
        if (_match(context, LEXER_TOK_SEMICOLON)) {
            /* TODO: is that an improperly handled warning again? :D */
            continue;
        }

        struct AstStmt* statement = _parse_stmt(context);
        if (statement) {
            uf_con_vector_push(statements_vector_tmp, &statement);
        } else {
            /* TODO: try to recover properly. */
            _synchronize(context);
        }
    }

    /* Copy statements to arena. */
    size_t statement_count = uf_con_vector_length(statements_vector_tmp);
    if (statement_count > 0) {
        block->statements =
            uf_mem_region_zalloc(context->node_arena, sizeof(struct AstStmt*) * statement_count);
        block->stmt_count = statement_count;
        for (size_t i = 0; i < statement_count; i++) {
            struct AstStmt** s = uf_con_vector_get(statements_vector_tmp, i);
            block->statements[i] = *s;
        }
    } else {
        block->statements = nullptr;
        block->stmt_count = 0;
    }
    uf_con_vector_free(statements_vector_tmp);

    /* Expect closing brace. */
    if (!_expect(context, LEXER_TOK_RBRACE, "}")) {
        /* TODO: again... try to recover properly. */
    }

    return block;
}

/**
 * Parse a return statement. The optional expression is the value to return. If omitted, this is a void
 * return.
 *
 * Grammar: 'return' Expression? ';'
 **/
static struct AstReturnStmt* _parse_return_stmt(ParserContext* context)
{
    const LexerToken* return_token = _peek_current(context);

    /* Consume 'return' keyword. */
    if (return_token->type == LEXER_TOK_SYMBOL && return_token->variant.symbol &&
        return_token->variant.symbol->type == LEXER_SYM_KEY_RETURN) {
        _advance(context);
    }

    /* Create return statement node. */
    struct AstReturnStmt* ret = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstReturnStmt));
    ret->base.type = AST_RETURN_STMT;
    ret->base.span = return_token->span;
    ret->value = nullptr;
    ret->exit_block = nullptr;

    /* Check for return value. */
    if (!_check(context, LEXER_TOK_SEMICOLON) && !_is_end(context)) {
        ret->value = _parse_expression(context);
    }

    return ret;
}

/**
 * Parse a variable declaration statement.
 *
 * Grammar: ('var')? IDENTIFIER ':' Type ('=' Expression)?
 **/
static struct AstDeclStmt* _parse_decl_stmt(ParserContext* context)
{
    const LexerToken* first = _peek_current(context);
    bool is_var = false;

    /* Check for 'var' keyword. */
    if (first->type == LEXER_TOK_SYMBOL && first->variant.symbol &&
        first->variant.symbol->type == LEXER_SYM_KEY_VAR) {
        _advance(context); /* Consume 'var'. */
        is_var = true;
        first = _peek_current(context);
    }

    /* Expect variable name. */
    if (first->type != LEXER_TOK_SYMBOL || !first->variant.symbol ||
        first->variant.symbol->type != LEXER_SYM_IDENTIFIER) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected variable name");
        return nullptr;
    }

    const char* var_name = first->variant.symbol->text;
    const SourceSpan name_span = first->span;
    _advance(context); /* Consume variable name. */

    /* Expect colon. */
    if (!_match(context, LEXER_TOK_COLON)) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected ':' in variable declaration");
        return nullptr;
    }

    /* Parse variable type. */
    struct AstType* var_type = _parse_type(context);
    if (!var_type) {
        return nullptr;
    }

    /* Create declaration statement node. */
    struct AstDeclStmt* declaration = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstDeclStmt));
    declaration->base.type = AST_DECL_STMT;
    declaration->base.span = name_span;
    declaration->name = var_name;
    declaration->type = var_type;
    declaration->init = nullptr;
    declaration->is_var = is_var;
    declaration->resolved_type = nullptr;
    declaration->codegen_slot = nullptr;

    /* Parse optional initializer. */
    if (_match(context, LEXER_TOK_ASSIGN)) {
        declaration->init = _parse_expression(context);
        if (declaration->init != nullptr && declaration->init->base.type == AST_INIT_EXPR) {
            ((struct AstInitExpr*)declaration->init)->target_type = declaration->type;
        }
    }

    return declaration;
}

/**
 * Parse an if statement. Both the then-branch and else-branch are single statements.
 * Use Block if multiple statements are needed.
 *
 * Grammar: 'if' '(' Expression ')' Statement ('else' Statement)?
 **/
static struct AstIfStmt* _parse_if_stmt(ParserContext* context)
{
    const LexerToken* if_token = _peek_current(context);

    /* Consume 'if' keyword. */
    _advance(context);

    /* Expect '(' */
    if (!_match(context, LEXER_TOK_LPAREN)) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected '(' after 'if'");
        return nullptr;
    }

    /* Parse condition expression. */
    struct AstExpr* condition = _parse_expression(context);
    if (!condition) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected condition expression in 'if' statement");
        return nullptr;
    }

    /* Expect ')' */
    if (!_expect(context, LEXER_TOK_RPAREN, ")")) {
        return nullptr;
    }

    /* Parse then-branch statement. */
    struct AstBlock* then_block = (struct AstBlock*)_parse_stmt(context);
    if (!then_block) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected statement in 'if' then-branch");
        return nullptr;
    }

    /* Check for optional else-branch. */
    struct AstBlock* else_block = nullptr;
    if (_match(context, LEXER_TOK_LPAREN)) {
        /* 'else' keyword - but we need to handle it differently */
    }

    /* Check for 'else' keyword. */
    if (_peek_current(context)->type == LEXER_TOK_SYMBOL && _peek_current(context)->variant.symbol &&
        _peek_current(context)->variant.symbol->type == LEXER_SYM_KEY_ELSE) {
        _advance(context); /* Consume 'else' */
        else_block = (struct AstBlock*)_parse_stmt(context);
        if (!else_block) {
            ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                           "Expected statement in 'else' branch");
            return nullptr;
        }
    }

    /* Create if statement node. */
    struct AstIfStmt* if_stmt = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstIfStmt));
    if_stmt->base.type = AST_IF_STMT;
    if_stmt->base.span = if_token->span;
    if_stmt->condition = condition;
    if_stmt->then_block = then_block;
    if_stmt->else_block = else_block;

    _track_node(context, &if_stmt->base);

    return if_stmt;
}

/**
 * Parse a for loop. All three parts are optional. This: 'for (;;) { }' is valid.
 *
 * Grammar: 'for' '(' Init? ';' Condition? ';' Iteration? ')' Statement
 **/
static struct AstForStmt* _parse_for_stmt(ParserContext* context)
{
    const LexerToken* for_token = _peek_current(context);

    /* Consume 'for' keyword. */
    _advance(context);

    /* Expect '(' */
    if (!_match(context, LEXER_TOK_LPAREN)) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected '(' after 'for'");
        return nullptr;
    }

    /* Parse initialization (optional). */
    struct AstStmt* init = nullptr;
    if (!_check(context, LEXER_TOK_SEMICOLON) && !_is_end(context)) {
        /* Could be variable declaration or expression */
        const LexerToken* first = _peek_current(context);
        if (first->type == LEXER_TOK_SYMBOL && first->variant.symbol &&
            (first->variant.symbol->type == LEXER_SYM_KEY_VAR ||
             (first->variant.symbol->type == LEXER_SYM_IDENTIFIER &&
              _peek_next(context)->type == LEXER_TOK_COLON))) {
            init = _parse_stmt(context);
        } else {
            /* Expression statement - consume up to semicolon */
            init = _parse_stmt(context);
        }
    }

    /* Expect ';' */
    if (!_expect(context, LEXER_TOK_SEMICOLON, ";")) {
        return nullptr;
    }

    /* Parse condition (optional). */
    struct AstExpr* condition = nullptr;
    if (!_check(context, LEXER_TOK_SEMICOLON) && !_is_end(context)) {
        condition = _parse_expression(context);
    }

    /* Expect ';' */
    if (!_expect(context, LEXER_TOK_SEMICOLON, ";")) {
        return nullptr;
    }

    /* Parse iteration (optional). */
    struct AstExpr* iter = nullptr;
    if (!_check(context, LEXER_TOK_RPAREN) && !_is_end(context)) {
        iter = _parse_expression(context);
    }

    /* Expect ')' */
    if (!_expect(context, LEXER_TOK_RPAREN, ")")) {
        return nullptr;
    }

    /* Parse body statement. */
    struct AstBlock* body = (struct AstBlock*)_parse_stmt(context);
    if (!body) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected statement in 'for' loop body");
        return nullptr;
    }

    /* Create for statement node. */
    struct AstForStmt* for_stmt = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstForStmt));
    for_stmt->base.type = AST_FOR_STMT;
    for_stmt->base.span = for_token->span;
    for_stmt->init = init;
    for_stmt->condition = condition;
    for_stmt->iter = iter;
    for_stmt->body = body;
    for_stmt->break_target = nullptr;
    for_stmt->continue_target = nullptr;

    _track_node(context, &for_stmt->base);

    return for_stmt;
}

/**
 * Parse a while loop.
 *
 * Grammar: 'while' '(' Expression ')' Statement
 *        | 'do' Statement 'while' '(' Expression ')' ';'
 **/
static struct AstWhileStmt* _parse_while_stmt(ParserContext* context)
{
    const LexerToken* first = _peek_current(context);

    /* Check if this is a do-while loop. */
    bool is_do_while = false;
    if (first->type == LEXER_TOK_SYMBOL && first->variant.symbol &&
        first->variant.symbol->type == LEXER_SYM_KEY_DO) {
        is_do_while = true;
    }

    if (is_do_while) {
        /* Parse do-while: 'do' Statement 'while' '(' Expression ')' ';' */
        _advance(context); /* Consume 'do' */

        /* Parse body statement. */
        struct AstBlock* body = (struct AstBlock*)_parse_stmt(context);
        if (!body) {
            ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                           "Expected statement in 'do-while' body");
            return nullptr;
        }

        /* Expect 'while' keyword. */
        if (!_match(context, LEXER_TOK_LPAREN)) {
            /* Check for 'while' keyword first */
            if (_peek_current(context)->type == LEXER_TOK_SYMBOL && _peek_current(context)->variant.symbol &&
                _peek_current(context)->variant.symbol->type == LEXER_SYM_KEY_WHILE) {
                _advance(context);
            } else {
                ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                               "Expected 'while' in do-while loop");
                return nullptr;
            }
        }

        /* Expect '(' */
        if (!_match(context, LEXER_TOK_LPAREN)) {
            ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                           "Expected '(' after 'while'");
            return nullptr;
        }

        /* Parse condition. */
        struct AstExpr* condition = _parse_expression(context);
        if (!condition) {
            ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                           "Expected condition in do-while");
            return nullptr;
        }

        /* Expect ')' */
        if (!_expect(context, LEXER_TOK_RPAREN, ")")) {
            return nullptr;
        }

        /* Expect ';' */
        if (!_expect(context, LEXER_TOK_SEMICOLON, ";")) {
            return nullptr;
        }

        /* Create while statement node. */
        struct AstWhileStmt* while_stmt =
            uf_mem_region_zalloc(context->node_arena, sizeof(struct AstWhileStmt));
        while_stmt->base.type = AST_WHILE_STMT;
        while_stmt->base.span = first->span;
        while_stmt->condition = condition;
        while_stmt->body = body;
        while_stmt->is_do_while = true;
        while_stmt->break_target = nullptr;
        while_stmt->continue_target = nullptr;

        _track_node(context, &while_stmt->base);

        return while_stmt;
    }

    /* Parse regular while loop: 'while' '(' Expression ')' Statement */
    _advance(context); /* Consume 'while' */

    /* Expect '(' */
    if (!_match(context, LEXER_TOK_LPAREN)) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected '(' after 'while'");
        return nullptr;
    }

    /* Parse condition. */
    struct AstExpr* condition = _parse_expression(context);
    if (!condition) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected condition in 'while' statement");
        return nullptr;
    }

    /* Expect ')' */
    if (!_expect(context, LEXER_TOK_RPAREN, ")")) {
        return nullptr;
    }

    /* Parse body statement. */
    struct AstBlock* body = (struct AstBlock*)_parse_stmt(context);
    if (!body) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected statement in 'while' body");
        return nullptr;
    }

    /* Create while statement node. */
    struct AstWhileStmt* while_stmt = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstWhileStmt));
    while_stmt->base.type = AST_WHILE_STMT;
    while_stmt->base.span = first->span;
    while_stmt->condition = condition;
    while_stmt->body = body;
    while_stmt->is_do_while = false;
    while_stmt->break_target = nullptr;
    while_stmt->continue_target = nullptr;

    _track_node(context, &while_stmt->base);

    return while_stmt;
}

/**
 * Parse a break statement.
 *
 * Grammar: 'break' ';'
 **/
static struct AstBreakStmt* _parse_break_stmt(ParserContext* context)
{
    const LexerToken* break_token = _peek_current(context);

    /* Consume 'break' keyword. */
    _advance(context);

    /* Expect ';'. */
    if (!_expect(context, LEXER_TOK_SEMICOLON, ";")) {
        return nullptr;
    }

    /* Create break statement node. */
    struct AstBreakStmt* break_stmt = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstBreakStmt));
    break_stmt->base.type = AST_BREAK_STMT;
    break_stmt->base.span = break_token->span;

    _track_node(context, &break_stmt->base);

    return break_stmt;
}

/**
 * Parse a continue statement.
 *
 * Grammar: 'continue' ';'
 **/
static struct AstContinueStmt* _parse_continue_stmt(ParserContext* context)
{
    const LexerToken* continue_token = _peek_current(context);

    /* Consume 'continue' keyword. */
    _advance(context);

    /* Expect ';'. */
    if (!_expect(context, LEXER_TOK_SEMICOLON, ";")) {
        return nullptr;
    }

    /* Create continue statement node. */
    struct AstContinueStmt* continue_stmt =
        uf_mem_region_zalloc(context->node_arena, sizeof(struct AstContinueStmt));
    continue_stmt->base.type = AST_CONTINUE_STMT;
    continue_stmt->base.span = continue_token->span;
    continue_stmt->target_loop = nullptr;

    _track_node(context, &continue_stmt->base);

    return continue_stmt;
}

static struct AstType* _wrap_type_in_array(ParserContext* context, struct AstType* element_type)
{
    struct AstExpr* size_expr = nullptr;
    if (!_check(context, LEXER_TOK_RBRACKET)) {
        size_expr = _parse_expression(context);
    }

    if (!_expect(context, LEXER_TOK_RBRACKET, "]")) {
    }

    struct AstType* array_type = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstType));
    array_type->base.type = AST_TYPE_ARRAY;
    array_type->base.span = element_type->base.span;
    array_type->kind = AST_TYPE_KIND_ARRAY;
    array_type->variant.array.element_type = element_type;
    array_type->variant.array.size = size_expr;
    array_type->is_resolved = false;
    array_type->size_in_bytes = 0;
    array_type->alignment = 0;

    return array_type;
}

static struct AstType* _wrap_type_in_pointer(ParserContext* context, struct AstType* pointed_type)
{
    struct AstType* pointer_type = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstType));
    pointer_type->base.type = AST_TYPE_POINTER;
    pointer_type->base.span = pointed_type->base.span;
    pointer_type->kind = AST_TYPE_KIND_POINTER;
    pointer_type->variant.pointer.pointed_type = pointed_type;
    pointer_type->is_resolved = false;
    pointer_type->size_in_bytes = 0;
    pointer_type->alignment = 0;
    return pointer_type;
}

/**
 * Parse a type annotation. This is the most complex function in this parser... I hate this function.
 *
 * Grammar: Type ->
 *   || PrimitiveType
 *   || IDENTIFIER (aka named blueprint)
 *   || Type '[' ']' (aka array)
 *   || Type '*' (aka pointer)
 *   || '{' Field* '}' (aka anonymous blueprint)
 **/
static struct AstType* _parse_type(ParserContext* context)
{
    const LexerToken* first = _peek_current(context);

    /* Handle identifier type (named blueprint like "Point"). */
    if (first->type == LEXER_TOK_SYMBOL && first->variant.symbol &&
        first->variant.symbol->type == LEXER_SYM_IDENTIFIER) {

        const char* name = first->variant.symbol->text;
        _advance(context); /* Consume the identifier. */

        struct AstType* type_ref = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstType));
        type_ref->base.type = AST_TYPE_BLUEPRINT;
        type_ref->base.span = first->span;
        type_ref->kind = AST_TYPE_KIND_BLUEPRINT;
        type_ref->variant.blueprint.name = name;
        type_ref->variant.blueprint.resolved = nullptr;
        type_ref->is_resolved = false;
        type_ref->size_in_bytes = 0;
        type_ref->alignment = 0;

        /* Check for array suffix. */
        if (_match(context, LEXER_TOK_LBRACKET)) {
            struct AstType* array_type = _wrap_type_in_array(context, type_ref);

            /* Check for pointer suffix. */
            if (_match(context, LEXER_TOK_STAR)) {
                return _wrap_type_in_pointer(context, array_type);
            }

            return array_type;
        }

        /* Check for pointer suffix. */
        if (_match(context, LEXER_TOK_STAR)) {
            return _wrap_type_in_pointer(context, type_ref);
        }

        return type_ref;
    }

    /* Handle primitive types by symbol type. */
    if (first->type == LEXER_TOK_SYMBOL && first->variant.symbol &&
        first->variant.symbol->type == LEXER_SYM_PRIMITIVE) {

        const char* name = first->variant.symbol->text;
        enum LexerPrimitiveType prim_type = first->variant.symbol->prim_type;
        _advance(context);

        struct AstType* type = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstType));
        type->base.type = AST_TYPE_PRIMITIVE;
        type->base.span = first->span;
        type->kind = AST_TYPE_KIND_PRIMITIVE;
        type->variant.primitive.name = name;
        type->variant.primitive.prim_type = prim_type;
        type->is_resolved = false;
        type->size_in_bytes = 0;
        type->alignment = 0;

        /* Check for array suffix. */
        if (_match(context, LEXER_TOK_LBRACKET)) {
            struct AstType* array_type = _wrap_type_in_array(context, type);

            /* Check for pointer suffix. */
            if (_match(context, LEXER_TOK_STAR)) {
                return _wrap_type_in_pointer(context, array_type);
            }

            return array_type;
        }

        return type;
    }

    /* Handle anonymous blueprints: { x: int; y: float; }. */
    if (_match(context, LEXER_TOK_LBRACE)) {
        struct AstType* anon_blueprint = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstType));
        anon_blueprint->base.type = AST_TYPE_ANON;
        anon_blueprint->base.span = first->span;
        anon_blueprint->kind = AST_TYPE_KIND_ANON;
        anon_blueprint->is_resolved = false;
        anon_blueprint->size_in_bytes = 0;
        anon_blueprint->alignment = 0;

        UfConVector* fields_vector_tmp = uf_con_vector_new(sizeof(struct AstField*));

        /* Parse fields inside the anonymous blueprint. */
        while (!_check(context, LEXER_TOK_RBRACE) && !_is_end(context)) {
            struct AstField* field = _parse_field(context);
            if (field) {
                uf_con_vector_push(fields_vector_tmp, &field);
            }
            _match(context, LEXER_TOK_SEMICOLON);
        }

        /* Copy fields into the arena. */
        size_t field_count = uf_con_vector_length(fields_vector_tmp);
        if (field_count > 0) {
            anon_blueprint->variant.anon.fields =
                uf_mem_region_zalloc(context->node_arena, sizeof(struct AstField*) * field_count);
            anon_blueprint->variant.anon.field_count = field_count;
            for (size_t i = 0; i < field_count; i++) {
                anon_blueprint->variant.anon.fields[i] =
                    *(struct AstField**)uf_con_vector_get(fields_vector_tmp, i);
            }
        } else {
            anon_blueprint->variant.anon.fields = nullptr;
            anon_blueprint->variant.anon.field_count = 0;
        }
        uf_con_vector_free(fields_vector_tmp);

        _expect(context, LEXER_TOK_RBRACE, "}");
        return anon_blueprint;
    }

    /* Unknown type. */
    ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span, "Expected type");
    return nullptr;
}

/*
 * Now the parameters and fields.
 */

/**
 * Parse a function/method parameter.
 *
 * Grammar: IDENTIFIER ':' Type || 'self'
 **/
static struct AstParam* _parse_param(ParserContext* ctx, const char* blueprint_name)
{
    const LexerToken* name_token = _peek_current(ctx);

    /* Check for 'self' parameter. */
    if (name_token->type == LEXER_TOK_SYMBOL && name_token->variant.symbol &&
        name_token->variant.symbol->type == LEXER_SYM_KEY_SELF) {
        _advance(ctx); /* Consume 'self'. */

        /* Create parameter node with implicit self type. */
        struct AstParam* param = uf_mem_region_zalloc(ctx->node_arena, sizeof(struct AstParam));
        param->base.type = AST_PARAM;
        param->base.span = name_token->span;
        param->name = name_token->variant.symbol->text;

        /* Create type for self parameter: blueprint name type */
        if (blueprint_name != nullptr) {
            param->type = uf_mem_region_zalloc(ctx->node_arena, sizeof(struct AstType));
            param->type->base.type = AST_TYPE_BLUEPRINT;
            param->type->base.span = name_token->span;
            param->type->kind = AST_TYPE_KIND_BLUEPRINT;
            param->type->variant.blueprint.name = blueprint_name;
            param->type->variant.blueprint.resolved = nullptr;
        } else {
            param->type = nullptr;
        }

        return param;
    }

    /* Expect identifier. */
    if (name_token->type != LEXER_TOK_SYMBOL || !name_token->variant.symbol ||
        name_token->variant.symbol->type != LEXER_SYM_IDENTIFIER) {
        ii_diag_report(ctx->diag_context, UF_LOG_ERROR, _peek_current(ctx)->span, "Expected parameter name");
        return nullptr;
    }

    const char* param_name = name_token->variant.symbol->text;
    _advance(ctx); /* Consume parameter name. */

    /* Expect colon. */
    if (!_match(ctx, LEXER_TOK_COLON)) {
        ii_diag_report(ctx->diag_context, UF_LOG_ERROR, _peek_current(ctx)->span,
                       "Expected ':' in parameter");
        return nullptr;
    }

    /* Parse parameter type. */
    struct AstType* param_type = _parse_type(ctx);
    if (!param_type) {
        return nullptr;
    }

    /* Create parameter node. */
    struct AstParam* param = uf_mem_region_zalloc(ctx->node_arena, sizeof(struct AstParam));
    param->base.type = AST_PARAM;
    param->base.span = name_token->span;
    param->name = param_name;
    param->type = param_type;

    return param;
}

/**
 * Parse a list of parameters. This function can be called when parsing both a function or a method.
 * Parameters are comma-separated and enclosed in parentheses. For simplicity, this function expects the
 * opening paren to already be consumed.
 *
 * Grammar: Parameter (',' Parameter)*
 **/
static struct AstParam* _parse_param_list(ParserContext* context, size_t* out_count,
                                          const char* blueprint_name)
{
    /* Handle empty parameter list. */
    if (_check(context, LEXER_TOK_RPAREN)) {
        if (out_count)
            *out_count = 0;
        return nullptr;
    }

    UfConVector* params_vector_tmp = uf_con_vector_new(sizeof(struct AstParam*));

    while (true) {
        struct AstParam* param = _parse_param(context, blueprint_name);
        if (param) {
            uf_con_vector_push(params_vector_tmp, &param);
        }

        /* Check for more parameters. */
        if (!_match(context, LEXER_TOK_COMMA)) {
            break;
        }

        /* Allow trailing comma. */
        if (_check(context, LEXER_TOK_RPAREN)) {
            break;
        }
    }

    /* Copy parameters to arena. */
    size_t param_count = uf_con_vector_length(params_vector_tmp);
    struct AstParam* params = nullptr;

    if (param_count > 0) {
        params = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstParam) * param_count);
        for (size_t i = 0; i < param_count; i++) {
            struct AstParam** p = uf_con_vector_get(params_vector_tmp, i);
            memcpy(&params[i], *p, sizeof(struct AstParam));
        }
    }

    uf_con_vector_free(params_vector_tmp);

    if (out_count)
        *out_count = param_count;
    return params;
}

/**
 * Parse a blueprint field. It can be called either when parsing named blueprint, or an anonymous (inline)
 * one.
 *
 * Grammar: IDENTIFIER ':' Type ('=' Expression)?
 **/
static struct AstField* _parse_field(ParserContext* context)
{
    const LexerToken* first = _peek_current(context);

    /* Expect identifier. */
    if (first->type != LEXER_TOK_SYMBOL || !first->variant.symbol ||
        first->variant.symbol->type != LEXER_SYM_IDENTIFIER) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected field name");
        return nullptr;
    }

    const char* field_name = first->variant.symbol->text;
    _advance(context); /* Consume field name. */

    /* Expect colon. */
    if (!_match(context, LEXER_TOK_COLON)) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected ':' after field name");
        return nullptr;
    }

    /* Parse field type. */
    struct AstType* field_type = _parse_type(context);
    if (!field_type) {
        return nullptr;
    }

    /* Create field node. */
    struct AstField* field = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstField));
    field->base.type = AST_FIELD;
    field->base.span = first->span;
    field->name = field_name;
    field->type = field_type;
    field->default_value = nullptr;

    /* Parse optional default value. */
    if (_match(context, LEXER_TOK_ASSIGN)) {
        field->default_value = _parse_expression(context);
    }

    return field;
}

/**
 * Parse a blueprint method. This function is invoked only from '_parse_blueprint_decl'.
 *
 * Grammar: IDENTIFIER '(' ParamList? ')' ':' Type ('=' Block)?
 **/
static struct AstMethod* _parse_method(ParserContext* context)
{
    const LexerToken* first = _peek_current(context);

    /* Expecting a method name. */
    if (first->type != LEXER_TOK_SYMBOL || !first->variant.symbol ||
        first->variant.symbol->type != LEXER_SYM_IDENTIFIER) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected method name");
        return nullptr;
    }

    const char* method_name = first->variant.symbol->text;
    const SourceSpan name_span = first->span;
    _advance(context); /* Consume method name. */

    /* Expect opening paren. */
    if (!_match(context, LEXER_TOK_LPAREN)) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected '(' after method name");
        return nullptr;
    }

    /* Get blueprint name for resolving self parameter type */
    const char* blueprint_name = context->current_blueprint_name;

    size_t param_count = 0;
    struct AstParam* params = _parse_param_list(context, &param_count, blueprint_name);

    /* Expect closing paren. */
    if (!_expect(context, LEXER_TOK_RPAREN, ")")) {
        /* TODO: who might guessed... try to recover properly. */
    }

    /* Expect colon. */
    if (!_match(context, LEXER_TOK_COLON)) {
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Expected ':' after parameter list");
        return nullptr;
    }

    /* Parse return type. */
    struct AstType* return_type = _parse_type(context);
    if (!return_type) {
        return nullptr;
    }

    /* Create method node. */
    struct AstMethod* method = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstMethod));
    method->base.type = AST_METHOD;
    method->base.span = name_span;
    method->name = method_name;
    method->params = params;
    method->param_count = param_count;
    method->return_type = return_type;
    method->body = nullptr;

    /* Check if any parameter is 'self' - if so, this is an instance method */
    method->is_static = true;
    for (size_t i = 0; i < param_count; i++) {
        if (params[i].name != nullptr && strcmp(params[i].name, "self") == 0) {
            method->is_static = false;
            break;
        }
    }

    /* Parse optional method body. */
    if (_match(context, LEXER_TOK_ASSIGN)) {
        method->body = _parse_block(context);
        if (!method->body) {
            ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                           "Expected method body");
        }
    }

    return method;
}

/*
 * Now the expressions...
 * ...
 * ...
 * why am I doing this to myself?
 */

/*
 * Operator precedence table for expression parsing. Higher numbers = higher precedence (bind tighter).
 */
static inline int32_t _get_precedence(enum LexerTokenType op)
{
    switch (op) {
    case LEXER_TOK_OR:
        return 1;
    case LEXER_TOK_AND:
        return 2;
    case LEXER_TOK_BIT_OR:
        return 3;
    case LEXER_TOK_BIT_XOR:
        return 4;
    case LEXER_TOK_BIT_AND:
        return 5;
    case LEXER_TOK_EQ:
    case LEXER_TOK_NEQ:
        return 6;
    case LEXER_TOK_LT:
    case LEXER_TOK_GT:
    case LEXER_TOK_LTE:
    case LEXER_TOK_GTE:
        return 7;
    case LEXER_TOK_PLUS:
    case LEXER_TOK_MINUS:
        return 8;
    case LEXER_TOK_STAR:
    case LEXER_TOK_SLASH:
    case LEXER_TOK_PERCENT:
        return 9;
    default:
        return 0;
    }
}

static inline bool _is_binary_op(enum LexerTokenType op)
{
    return _get_precedence(op) > 0;
}

static inline bool _is_unary_op(enum LexerTokenType op)
{
    switch (op) {
    case LEXER_TOK_MINUS:
    case LEXER_TOK_NOT:
    case LEXER_TOK_BIT_NOT:
        return true;
    default:
        return false;
    }
}

/**
 * Parse primary expression like literals, identifiers, parenthesized expressions.
 *
 * Grammar: Literal || Identifier || '(' Expression ')' || '{' Expression* '}'
 **/
static struct AstExpr* _parse_primary_expr(ParserContext* context)
{
    const LexerToken* first = _peek_current(context);

    /* Handle parenthesized expressions */
    if (_match(context, LEXER_TOK_LPAREN)) {
        struct AstExpr* inner = _parse_expression(context);
        if (!inner) {
            ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                           "Expected expression inside parentheses");
            return nullptr;
        }
        if (!_expect(context, LEXER_TOK_RPAREN, ")")) {
            return nullptr;
        }
        return inner;
    }

    /* Handle array/object initialization. */
    if (_match(context, LEXER_TOK_LBRACE)) {
        _autovector_ UfConVector* values = uf_con_vector_new(sizeof(struct AstExpr*));
        _autovector_ UfConVector* named = uf_con_vector_new(sizeof(struct {
            const char* name;
            struct AstExpr* expr;
        }));
        bool saw_named = false;

        /* Check for empty init or list of expressions. */
        if (!_check(context, LEXER_TOK_RBRACE) && !_is_end(context)) {
            while (true) {
                /* Check for named: IDENTIFIER followed by = */
                if (_check(context, LEXER_TOK_SYMBOL) && _peek_next(context)->type == LEXER_TOK_ASSIGN) {
                    /* Cannot mix positional and named initialization */
                    if (!saw_named && uf_con_vector_length(values) > 0) {
                        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                                       "Cannot mix positional and named initialization");
                        return nullptr;
                    }

                    saw_named = true;
                    const char* name = _peek_current(context)->variant.symbol->text;
                    _advance(context); /* consume name */
                    _advance(context); /* consume = */

                    struct AstExpr* val = _parse_expression(context);
                    if (!val) {
                        return nullptr;
                    }

                    struct {
                        const char* name;
                        struct AstExpr* expr;
                    } named_entry = {name, val};
                    uf_con_vector_push(named, &named_entry);
                } else {
                    /* Positional: Expression */
                    /* Cannot mix positional and named initialization */
                    if (saw_named) {
                        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                                       "Cannot mix positional and named initialization");
                        return nullptr;
                    }

                    struct AstExpr* val = _parse_expression(context);
                    if (!val) {
                        return nullptr;
                    }
                    uf_con_vector_push(values, &val);
                }

                if (!_match(context, LEXER_TOK_COMMA)) {
                    break;
                }

                /* Trailing comma is allowed. */
                if (_check(context, LEXER_TOK_RBRACE)) {
                    break;
                }
            }
        }

        if (!_expect(context, LEXER_TOK_RBRACE, "}")) {
            return nullptr;
        }

        /* Create init expression node. */
        struct AstInitExpr* init = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstInitExpr));
        init->base.type = AST_INIT_EXPR;
        init->base.span = first->span;
        init->inferred_type = nullptr;
        init->target_type = nullptr;

        /* Store positional values */
        size_t count = uf_con_vector_length(values);
        if (count > 0) {
            init->values = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstExpr*) * count);
            init->value_count = count;
            for (size_t i = 0; i < count; i++) {
                struct AstExpr** v = uf_con_vector_get(values, i);
                init->values[i] = *v;
            }
        } else {
            init->values = nullptr;
            init->value_count = 0;
        }

        /* Store named values */
        size_t named_count = uf_con_vector_length(named);
        if (named_count > 0) {
            init->named_values =
                uf_mem_region_zalloc(context->node_arena, sizeof(*init->named_values) * named_count);
            init->named_value_count = named_count;
            for (size_t i = 0; i < named_count; i++) {
                struct {
                    const char* name;
                    struct AstExpr* expr;
                }* n = uf_con_vector_get(named, i);
                init->named_values[i].name = n->name;
                init->named_values[i].value = n->expr;
            }
        } else {
            init->named_values = nullptr;
            init->named_value_count = 0;
        }

        _track_node(context, &init->base);

        return (struct AstExpr*)init;
    }

    /* Handle literals */
    if (first->type == LEXER_TOK_SYMBOL && first->variant.symbol) {
        const char* text = first->variant.symbol->text;

        /* Check for integer literal (starts with digit). */
        if (isdigit(text[0])) {
            _advance(context);

            /* Check if it's a float. */
            bool is_float = false;
            for (const char* p = text; *p; p++) {
                if (*p == '.' || *p == 'f' || *p == 'F' || *p == 'd' || *p == 'D') {
                    is_float = true;
                    break;
                }
            }

            struct AstLiteral* literal = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstLiteral));
            literal->base.type = AST_LITERAL;
            literal->base.span = first->span;
            literal->inferred_type = nullptr;

            if (is_float) {
                literal->kind = LITERAL_FLOAT;
                literal->variant.float_value = strtod(text, nullptr);
            } else {
                literal->kind = LITERAL_INT;
                literal->variant.int_value = strtol(text, nullptr, 0);
            }

            _track_node(context, &literal->base);
            return (struct AstExpr*)literal;
        }

        /* Check for boolean literals. */
        if (strcmp(text, "true") == 0 || strcmp(text, "false") == 0) {
            _advance(context);

            struct AstLiteral* literal = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstLiteral));
            literal->base.type = AST_LITERAL;
            literal->base.span = first->span;
            literal->inferred_type = nullptr;
            literal->kind = LITERAL_BOOL;
            literal->variant.bool_value = (strcmp(text, "true") == 0);

            _track_node(context, &literal->base);
            return (struct AstExpr*)literal;
        }

        /* Check for string literals. */
        if (first->variant.symbol->type == LEXER_SYM_STRING) {
            _advance(context);

            struct AstLiteral* literal = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstLiteral));
            literal->base.type = AST_LITERAL;
            literal->base.span = first->span;
            literal->inferred_type = nullptr;
            literal->kind = LITERAL_STRING;
            literal->variant.string_value = first->variant.symbol->text;

            _track_node(context, &literal->base);
            return (struct AstExpr*)literal;
        }

        /* Check for character literals. */
        if (first->variant.symbol->type == LEXER_SYM_CHAR) {
            _advance(context);

            struct AstLiteral* literal = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstLiteral));
            literal->base.type = AST_LITERAL;
            literal->base.span = first->span;
            literal->inferred_type = nullptr;
            literal->kind = LITERAL_CHAR;
            literal->variant.char_value = first->variant.symbol->text[0];

            _track_node(context, &literal->base);
            return (struct AstExpr*)literal;
        }
    }

    /* Handle identifiers. */
    if (first->type == LEXER_TOK_SYMBOL && first->variant.symbol &&
        first->variant.symbol->type == LEXER_SYM_IDENTIFIER) {
        const char* name = first->variant.symbol->text;
        _advance(context);

        struct AstIdent* ident = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstIdent));
        ident->base.type = AST_IDENT;
        ident->base.span = first->span;
        ident->inferred_type = nullptr;
        ident->name = name;
        memset(&ident->resolved, 0, sizeof(ident->resolved));

        _track_node(context, &ident->base);
        return (struct AstExpr*)ident;
    }

    /* Handle 'self' keyword as identifier in expressions. */
    if (first->type == LEXER_TOK_SYMBOL && first->variant.symbol &&
        first->variant.symbol->type == LEXER_SYM_KEY_SELF) {
        const char* name = first->variant.symbol->text;
        _advance(context);

        struct AstIdent* ident = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstIdent));
        ident->base.type = AST_IDENT;
        ident->base.span = first->span;
        ident->inferred_type = nullptr;
        ident->name = name;
        memset(&ident->resolved, 0, sizeof(ident->resolved));

        _track_node(context, &ident->base);
        return (struct AstExpr*)ident;
    }

    /* Not a valid expression start */
    ii_diag_report(context->diag_context, UF_LOG_ERROR, first->span, "Expected expression");
    return nullptr;
}

/**
 * Parse function call arguments.
 *
 * Grammar: '(' (Expression (',' Expression)*)? ')'
 **/
static struct AstCallExpr* _parse_call_args(ParserContext* context, struct AstExpr* callee)
{
    const LexerToken* first = _peek_current(context);

    _autovector_ UfConVector* args = uf_con_vector_new(sizeof(struct AstExpr*));

    if (!_check(context, LEXER_TOK_RPAREN)) {
        while (true) {
            struct AstExpr* arg = _parse_expression(context);
            if (!arg) {
                return nullptr;
            }
            uf_con_vector_push(args, &arg);

            if (!_match(context, LEXER_TOK_COMMA)) {
                break;
            }
        }
    }

    if (!_expect(context, LEXER_TOK_RPAREN, ")")) {
        return nullptr;
    }

    struct AstCallExpr* call = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstCallExpr));
    call->base.type = AST_CALL_EXPR;
    call->base.span = first->span;
    call->inferred_type = nullptr;
    call->callee = callee;

    size_t arg_count = uf_con_vector_length(args);
    if (arg_count > 0) {
        call->args = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstExpr*) * arg_count);
        call->arg_count = arg_count;
        for (size_t i = 0; i < arg_count; i++) {
            struct AstExpr** a = uf_con_vector_get(args, i);
            call->args[i] = *a;
        }
    } else {
        call->args = nullptr;
        call->arg_count = 0;
    }

    return call;
}

/**
 * Parse postfix expressions: member access, function calls, array indexing.
 *
 * Grammar: PrimaryExpr ( ('.' IDENTIFIER) | ('[' Expression ']') | ('(' Arguments ')') )*
 *
 * This handles chained expressions like: obj.method().field[0]
 **/
static struct AstExpr* _parse_postfix_expr(ParserContext* context)
{
    struct AstExpr* expr = _parse_primary_expr(context);
    if (!expr) {
        return nullptr;
    }

    while (true) {
        if (_match(context, LEXER_TOK_DOT)) {
            const LexerToken* member_token = _peek_current(context);
            if (member_token->type != LEXER_TOK_SYMBOL || !member_token->variant.symbol ||
                member_token->variant.symbol->type != LEXER_SYM_IDENTIFIER) {
                ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                               "Expected member name after '.'");
                return nullptr;
            }

            const char* member_name = member_token->variant.symbol->text;
            _advance(context);

            bool is_method = _check(context, LEXER_TOK_LPAREN);

            struct AstMemberExpr* member =
                uf_mem_region_zalloc(context->node_arena, sizeof(struct AstMemberExpr));
            member->base.type = AST_MEMBER_EXPR;
            member->base.span = expr->base.span;
            member->inferred_type = nullptr;
            member->object = expr;
            member->member = member_name;
            member->is_method = is_method;

            _track_node(context, &member->base);
            expr = (struct AstExpr*)member;

            if (is_method) {
                _advance(context);
                struct AstCallExpr* call = _parse_call_args(context, expr);
                if (!call) {
                    return nullptr;
                }
                _track_node(context, &call->base);
                expr = (struct AstExpr*)call;
            }
        } else if (_match(context, LEXER_TOK_LBRACKET)) {
            struct AstExpr* index = _parse_expression(context);
            if (!index) {
                return nullptr;
            }

            if (!_expect(context, LEXER_TOK_RBRACKET, "]")) {
                return nullptr;
            }

            struct AstIndexExpr* subscript =
                uf_mem_region_zalloc(context->node_arena, sizeof(struct AstIndexExpr));
            subscript->base.type = AST_INDEX_EXPR;
            subscript->base.span = expr->base.span;
            subscript->inferred_type = nullptr;
            subscript->array = expr;
            subscript->index = index;

            _track_node(context, &subscript->base);
            expr = (struct AstExpr*)subscript;
        } else if (_match(context, LEXER_TOK_LPAREN)) {
            struct AstCallExpr* call = _parse_call_args(context, expr);
            if (!call) {
                return nullptr;
            }
            _track_node(context, &call->base);
            expr = (struct AstExpr*)call;
        } else {
            break;
        }
    }

    return expr;
}

/**
 * Parse unary expression: prefix operators or primary.
 *
 * Grammar: ('-' || '!' || '~' || '$') UnaryExpr || PrimaryExpr
 **/
static struct AstExpr* _parse_unary_expr(ParserContext* context)
{
    const LexerToken* first = _peek_current(context);

    /* Check for FFI prefix. */
    if (first->type == LEXER_TOK_DOLLAR) {
        _advance(context);

        const LexerToken* name_token = _peek_current(context);
        if (name_token->type != LEXER_TOK_SYMBOL || !name_token->variant.symbol ||
            name_token->variant.symbol->type != LEXER_SYM_IDENTIFIER) {
            ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                           "Expected function name after '$'");
            return nullptr;
        }

        const char* func_name = name_token->variant.symbol->text;
        _advance(context);

        if (!_match(context, LEXER_TOK_LPAREN)) {
            ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                           "Expected '(' after FFI function name");
            return nullptr;
        }

        _autovector_ UfConVector* args = uf_con_vector_new(sizeof(struct AstExpr*));

        if (!_check(context, LEXER_TOK_RPAREN)) {
            while (true) {
                struct AstExpr* arg = _parse_expression(context);
                if (!arg) {
                    return nullptr;
                }
                uf_con_vector_push(args, &arg);

                if (!_match(context, LEXER_TOK_COMMA)) {
                    break;
                }
            }
        }

        if (!_expect(context, LEXER_TOK_RPAREN, ")")) {
            return nullptr;
        }

        struct AstFfiExpr* ffi = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstFfiExpr));
        ffi->base.type = AST_FFI_EXPR;
        ffi->base.span = first->span;
        ffi->inferred_type = nullptr;
        ffi->function_name = func_name;

        size_t arg_count = uf_con_vector_length(args);
        if (arg_count > 0) {
            ffi->args = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstExpr*) * arg_count);
            ffi->arg_count = arg_count;
            for (size_t i = 0; i < arg_count; i++) {
                struct AstExpr** a = uf_con_vector_get(args, i);
                ffi->args[i] = *a;
            }
        } else {
            ffi->args = nullptr;
            ffi->arg_count = 0;
        }

        _track_node(context, &ffi->base);
        return (struct AstExpr*)ffi;
    }

    /* Check for unary prefix operators. */
    if (first->type == LEXER_TOK_MINUS || first->type == LEXER_TOK_NOT || first->type == LEXER_TOK_BIT_NOT) {
        enum LexerTokenType op = first->type;
        _advance(context);

        struct AstExpr* operand = _parse_unary_expr(context);
        if (!operand) {
            return nullptr;
        }

        struct AstUnaryExpr* unary = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstUnaryExpr));
        unary->base.type = AST_UNARY_EXPR;
        unary->base.span = first->span;
        unary->inferred_type = nullptr;
        unary->op = op;
        unary->operand = operand;

        _track_node(context, &unary->base);
        return (struct AstExpr*)unary;
    }

    /* No unary operator, parse postfix. */
    return _parse_postfix_expr(context);
}

/**
 * Parse binary expression with precedence climbing.
 *
 * Grammar: UnaryExpr (BinaryOp UnaryExpr)*
 **/
static struct AstExpr* _parse_binary_expr(ParserContext* context, int32_t min_prec)
{
    /* Parse the left-hand side. */
    struct AstExpr* left = _parse_unary_expr(context);
    if (!left) {
        return nullptr;
    }

    /* Keep parsing binary operators while they have sufficient precedence. */
    while (true) {
        const LexerToken* op_token = _peek_current(context);
        enum LexerTokenType op = op_token->type;

        /* Check if this is a binary operator with sufficient precedence. */
        int32_t prec = _get_precedence(op);
        if (prec == 0 || prec < min_prec) {
            break;
        }

        /* For left-associative operators, use 'prec + 1' for right side. */
        int32_t next_min_prec = prec + 1;

        /* Consume the operator */
        _advance(context);

        /* Parse the right-hand side */
        struct AstExpr* right = _parse_binary_expr(context, next_min_prec);
        if (!right) {
            return nullptr;
        }

        /* Create binary expression node */
        struct AstBinaryExpr* binary =
            uf_mem_region_zalloc(context->node_arena, sizeof(struct AstBinaryExpr));
        binary->base.type = AST_BINARY_EXPR;
        binary->base.span = left->base.span;
        binary->inferred_type = nullptr;
        binary->op = op;
        binary->left = left;
        binary->right = right;

        _track_node(context, &binary->base);

        left = (struct AstExpr*)binary;
    }

    return left;
}

/**
 * Parse assignment expression: binary expr with assignment operators.
 *
 * Grammar: BinaryExpr ('=' | '+=' | '-=' | '*=' | '/=' AssignmentExpr)?
 **/
static struct AstExpr* _parse_assignment_expr(ParserContext* context)
{
    /* First parse as binary expression (to handle comparisons etc.). */
    struct AstExpr* left = _parse_binary_expr(context, 1);
    if (!left) {
        return nullptr;
    }

    const LexerToken* op_token = _peek_current(context);
    enum LexerTokenType op = op_token->type;

    /* Check for assignment operators. */
    switch (op) {
    case LEXER_TOK_ASSIGN:
    case LEXER_TOK_PLUS_ASSIGN:
    case LEXER_TOK_MINUS_ASSIGN:
    case LEXER_TOK_STAR_ASSIGN:
    case LEXER_TOK_SLASH_ASSIGN:
        break;
    default:
        return left;
    }

    /* Consume the assignment operator. */
    _advance(context);

    /* Parse the right-hand side (assignment is right-associative). */
    struct AstExpr* right = _parse_assignment_expr(context);
    if (!right) {
        return nullptr;
    }

    /* Create binary expression node with assignment operator. */
    struct AstBinaryExpr* binary = uf_mem_region_zalloc(context->node_arena, sizeof(struct AstBinaryExpr));
    binary->base.type = AST_BINARY_EXPR;
    binary->base.span = left->base.span;
    binary->inferred_type = nullptr;
    binary->op = op;
    binary->left = left;
    binary->right = right;

    _track_node(context, &binary->base);

    return (struct AstExpr*)binary;
}

/**
 * This is the main entry point for expression parsing.
 **/
static struct AstExpr* _parse_expression(ParserContext* context)
{
    return _parse_assignment_expr(context);
}

/* This is the main entry point for parser. */
ParserContext* ii_parser_context_new(Source* source, const LexerToken* tokens, size_t token_count,
                                     DiagnosticContext* diag)
{
    ParserContext* context = uf_mem_zalloc(sizeof(ParserContext));
    context->diag_context = diag;
    context->tokens = tokens;
    context->token_count = token_count;

    context->source = source;
    context->node_arena = uf_mem_region_new(ARENA_BLOCK_SIZE);
    context->node_vector = uf_con_vector_new(sizeof(AstNode*));

    /* Here we create the AST. */
    Ast* ast = uf_mem_region_zalloc(context->node_arena, sizeof(Ast));
    context->ast = ast;

    ast->node_arena = context->node_arena;
    ast->node_vector = context->node_vector;
    ast->base.type = AST_ROOT;
    ast->base.span = (SourceSpan){0, 0};
    _track_node(context, &ast->base);

    ast->decl_count = 0;
    ast->declarations = uf_mem_region_zalloc(context->node_arena, sizeof(typeof(*ast->declarations)));

    /*
     * This is the main parser loop.
     *
     * Grammar: Program -> Declaration*
     *
     * IIRA program is for now just a sequence of top-level declarations, which can be:
     * - Function declarations
     * - Blueprint declarations
     */
    while (!_is_end(context)) {
        /* Skip any semicolons between declarations. */
        while (_match(context, LEXER_TOK_SEMICOLON)) {
            /* TODO: we should probably throw at least a warning. */
        }

        if (_is_end(context)) {
            break;
        }

        /* Look ahead to determine what kind of declaration this is. */
        const LexerToken* first = _peek_current(context);

        /* Check for identifier - this could be function or blueprint. */
        if (first->type == LEXER_TOK_SYMBOL && first->variant.symbol &&
            first->variant.symbol->type == LEXER_SYM_IDENTIFIER) {

            const LexerToken* second = _peek_next(context);

            /* Blueprint expects: IDENTIFIER ':' '{' - check if next token is colon followed by brace */
            if (second->type == LEXER_TOK_COLON) {
                /* This should be a blueprint - try to parse it */
                struct AstBlueprintDecl* blueprint = _parse_blueprint_decl(context);

                if (blueprint) {
                    /* Expand declarations array */
                    size_t new_count = ast->decl_count + 1;
                    typeof(ast->declarations) new_declaration =
                        uf_mem_region_zalloc(context->node_arena, sizeof(ast->declarations[0]) * new_count);

                    /* Zero the new declaration to ensure union is properly initialized */
                    memset(&new_declaration[ast->decl_count], 0, sizeof(ast->declarations[0]));

                    /* Copy old declarations */
                    if (ast->decl_count > 0 && ast->declarations) {
                        memcpy(new_declaration, ast->declarations,
                               sizeof(ast->declarations[0]) * ast->decl_count);
                    }

                    /* Add new declaration */
                    new_declaration[ast->decl_count].blueprint_decl.kind = AST_BLUEPRINT_DECL;
                    new_declaration[ast->decl_count].blueprint_decl.blueprint = blueprint;
                    ast->declarations = new_declaration;
                    ast->decl_count = new_count;
                }
                continue;
            }

            /* Function: IDENTIFIER '(' */
            if (second->type == LEXER_TOK_LPAREN) {
                struct AstFuncDecl* func = _parse_func_decl(context);
                if (func) {
                    /* Expand declarations array */
                    size_t new_count = ast->decl_count + 1;
                    typeof(ast->declarations) new_decl =
                        uf_mem_region_zalloc(context->node_arena, sizeof(ast->declarations[0]) * new_count);

                    /* Zero the new declaration to ensure union is properly initialized */
                    memset(&new_decl[ast->decl_count], 0, sizeof(ast->declarations[0]));

                    /* Copy old declarations */
                    if (ast->decl_count > 0 && ast->declarations) {
                        memcpy(new_decl, ast->declarations, sizeof(ast->declarations[0]) * ast->decl_count);
                    }

                    /* Add new declaration */
                    new_decl[ast->decl_count].func_decl.kind = AST_FUNC_DECL;
                    new_decl[ast->decl_count].func_decl.func = func;
                    ast->declarations = new_decl;
                    ast->decl_count = new_count;
                }
                continue;
            }
        }

        /* Unknown declaration - consume one token and continue */
        ii_diag_report(context->diag_context, UF_LOG_ERROR, _peek_current(context)->span,
                       "Unexpected token at top level");
        _advance(context);
    }

    return context;
}

void ii_parser_context_free(ParserContext* context)
{
    if (context == nullptr) {
        return;
    }

    uf_mem_region_free(context->node_arena);
    uf_con_vector_free(context->node_vector);

    uf_mem_free(context);
}

void ii_parser_context_freep(ParserContext** context_ptr)
{
    if (context_ptr && *context_ptr) {
        ii_parser_context_free(*context_ptr);
        *context_ptr = nullptr;
    }
}

Ast* ii_parser_get_ast(ParserContext* context)
{
    return context->ast;
}

/*
 * Helper function for printing the AST into the standard output. It's used to print the lexer output when
 * appropriate flag is used for compilation of particular unit.
 */

void ii_parser_print_debug(ParserContext* context)
{
    /* TODO: I really don't want to do this right now. */
    (void)context;
    printf("Parser debug printing isn't implemented right now :(\n");
}
