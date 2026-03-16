/**
 * @brief Abstract Syntax Tree and Typed AST implementation.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast.h"
#include "uf_memory.h"

#include <stdio.h>
#include <string.h>

struct Ast {
    Source* source;
    UfMemRegion* arena;

    AstProgram* program;
    uint32_t anon_counter;

    /* Error tracking. */
    uint32_t error_count;
};

static void* _alloc_ast(Ast* ast, size_t size)
{
    return uf_mem_region_malloc(ast->arena, size);
}

/*
 * AST creation, destruction, and utility functions.
 */

Ast* ii_ast_new(Source* source)
{
    Ast* ast = uf_mem_zalloc(sizeof(Ast));
    ast->source = source;
    ast->arena = uf_mem_region_new(4096);
    ast->program = _alloc_ast(ast, sizeof(AstProgram));
    memset(ast->program, 0, sizeof(AstProgram));
    ast->error_count = 0;
    ast->anon_counter = 0;
    return ast;
}

static void ii_ast_free_vectors(Ast* ast)
{
    if (!ast || !ast->program) {
        return;
    }

    /* Free program-level vectors. */
    uf_con_vector_free(ast->program->funcs);
    uf_con_vector_free(ast->program->blueprints);

    /* Free function vectors. */
    for (size_t i = 0; i < uf_con_vector_length(ast->program->funcs); i++) {
        AstFuncDecl* func = *(AstFuncDecl**)uf_con_vector_get(ast->program->funcs, i);

        if (!func) {
            continue;
        }

        uf_con_vector_free(func->params);
        if (func->body) {
            uf_con_vector_free(func->body->stmts);
        }
    }

    /* Free blueprint vectors. */
    for (size_t i = 0; i < uf_con_vector_length(ast->program->blueprints); i++) {
        AstBlueprintDecl* bp = *(AstBlueprintDecl**)uf_con_vector_get(ast->program->blueprints, i);
        if (!bp) {
            continue;
        }

        uf_con_vector_free(bp->parents);
        uf_con_vector_free(bp->fields);
        uf_con_vector_free(bp->methods);
        uf_con_vector_free(bp->flat_fields);
        uf_con_vector_free(bp->flat_methods);

        /* Free parent inheritance vectors. */
        for (size_t j = 0; j < uf_con_vector_length(bp->parents); j++) {
            AstInherit* inh = *(AstInherit**)uf_con_vector_get(bp->parents, j);
            if (inh) {
                uf_con_vector_free(inh->field_aliases);
            }
        }

        /* Free field vectors. */
        for (size_t j = 0; j < uf_con_vector_length(bp->fields); j++) {
            AstField* field = *(AstField**)uf_con_vector_get(bp->fields, j);
            /* TODO: free field vectors. */
            (void)field;
        }

        /* Free method vectors and their contents. */
        for (size_t j = 0; j < uf_con_vector_length(bp->methods); j++) {
            AstMethod* method = *(AstMethod**)uf_con_vector_get(bp->methods, j);
            if (!method) {
                continue;
            }

            uf_con_vector_free(method->overloads);

            /* Free method overload vectors. */
            for (size_t k = 0; k < uf_con_vector_length(method->overloads); k++) {
                AstMethodOverload* ov = *(AstMethodOverload**)uf_con_vector_get(method->overloads, k);
                if (!ov) {
                    continue;
                }

                uf_con_vector_free(ov->params);
                if (ov->body) {
                    uf_con_vector_free(ov->body->stmts);
                }
            }
        }
    }
}

void ii_ast_free(Ast* ast)
{
    if (!ast) {
        return;
    }

    ii_ast_free_vectors(ast);
    uf_mem_region_free(ast->arena);
    uf_mem_free(ast);
}

void ii_ast_freep(Ast** ast)
{
    if (!ast || !*ast) {
        return;
    }

    ii_ast_free(*ast);
    *ast = NULL;
}

const char* ii_ast_intern(Ast* ast, const char* str, size_t len)
{
    return ii_src_intern(ast->source, str, len);
}

const char* ii_ast_intern_cstr(Ast* ast, const char* cstr)
{
    return ii_src_intern_cstr(ast->source, cstr);
}

void ii_ast_print(const Ast* ast)
{
    /* TODO: implement. */
    (void)ast;
    printf("AST print not yet implemented\n");
}
