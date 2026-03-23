#pragma once

/**
 * @brief Type-safe inline iterators for AST traversal.
 *
 * This module provides clean, type-safe iteration over AST vectors.
 * Each iterator returns the properly typed pointer, eliminating
 * the boilerplate of casting and manual loop management.
 *
 * Usage:
 *   ast_foreach_funcs(ast, func) {
 *       ii_analyze_func(ctx, func);
 *   }
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct AstForeach AstForeach;
struct AstForeach {
    const void* vector;
    size_t index;
    size_t length;
    void* current;
};

static inline void _ast_foreach_init(AstForeach* iter, const UfConVector* vec)
{
    iter->vector = vec;
    iter->index = 0;
    iter->length = vec ? uf_con_vector_length(vec) : 0;
    iter->current = nullptr;
}

static inline void _ast_foreach_next(AstForeach* iter)
{
    if (iter->index < iter->length) {
        iter->index++;
    }
}

static inline bool _ast_foreach_done(const AstForeach* iter)
{
    return iter->index >= iter->length;
}

#define _AST_FOREACH_BEGIN(type, iter_name, vec_ptr)                                                         \
    do {                                                                                                     \
        AstForeach iter_name##_iter__;                                                                       \
        _ast_foreach_init(&iter_name##_iter__, (const UfConVector*)(vec_ptr));                               \
        for (; !_ast_foreach_done(&iter_name##_iter__); _ast_foreach_next(&iter_name##_iter__)) {            \
            type* iter_name = *(type**)uf_con_vector_get((UfConVector*)iter_name##_iter__.vector,            \
                                                         iter_name##_iter__.index);                          \
            if (iter_name != nullptr)

#define _AST_FOREACH_END                                                                                     \
    }                                                                                                        \
    }                                                                                                        \
    while (0)

#define _AST_FOREACH_BEGIN_SIMPLE(type, iter_name, vec_ptr)                                                  \
    do {                                                                                                     \
        AstForeach iter_name##_iter__;                                                                       \
        _ast_foreach_init(&iter_name##_iter__, (const UfConVector*)(vec_ptr));                               \
        for (; !_ast_foreach_done(&iter_name##_iter__); _ast_foreach_next(&iter_name##_iter__)) {            \
            type* iter_name = *(type**)uf_con_vector_get((UfConVector*)iter_name##_iter__.vector,            \
                                                         iter_name##_iter__.index)

#define _AST_FOREACH_END_SIMPLE                                                                              \
    }                                                                                                        \
    }                                                                                                        \
    while (0)

#define ast_foreach_funcs(ast_or_prog, var)                                                                  \
    _AST_FOREACH_BEGIN(AstFuncDecl, var, (((ast_or_prog)->program) ? (ast_or_prog)->program->funcs : nullptr))

#define ast_foreach_blueprints(ast_or_prog, var)                                                             \
    _AST_FOREACH_BEGIN(AstBlueprintDecl, var,                                                                \
                       (((ast_or_prog)->program) ? (ast_or_prog)->program->blueprints : nullptr))

#define ast_foreach_params(func, var) _AST_FOREACH_BEGIN(AstParam, var, ((func) ? (func)->params : nullptr))

#define ast_foreach_stmts(block, var) _AST_FOREACH_BEGIN(AstStmt, var, ((block) ? (block)->stmts : nullptr))

#define ast_foreach_parents(bp, var) _AST_FOREACH_BEGIN(AstInherit, var, ((bp) ? (bp)->parents : nullptr))

#define ast_foreach_fields(bp, var) _AST_FOREACH_BEGIN(AstField, var, ((bp) ? (bp)->fields : nullptr))

#define ast_foreach_methods(bp, var) _AST_FOREACH_BEGIN(AstMethod, var, ((bp) ? (bp)->methods : nullptr))

#define ast_foreach_overloads(method, var)                                                                   \
    _AST_FOREACH_BEGIN(AstMethodOverload, var, ((method) ? (method)->overloads : nullptr))

#define ast_foreach_call_args(call, var) _AST_FOREACH_BEGIN(AstExpr, var, ((call) ? (call)->args : nullptr))

#define ast_foreach_ffi_args(ffi, var) _AST_FOREACH_BEGIN(AstExpr, var, ((ffi) ? (ffi)->args : nullptr))

#define ast_foreach_init_values(init, var)                                                                   \
    _AST_FOREACH_BEGIN(AstExpr, var, ((init) ? (init)->values : nullptr))

#define ast_foreach_end _AST_FOREACH_END

#define ast_foreach_end_simple _AST_FOREACH_END_SIMPLE

#define ast_foreach_decl(ast, var)                                                                           \
    do {                                                                                                     \
        AstForeach _decl_iter__;                                                                             \
        const UfConVector* _funcs_vec__ = ((ast)->program ? (ast)->program->funcs : nullptr);                \
        const UfConVector* _bps_vec__ = ((ast)->program ? (ast)->program->blueprints : nullptr);             \
        size_t _funcs_len__ = _funcs_vec__ ? uf_con_vector_length(_funcs_vec__) : 0;                         \
        size_t _bps_len__ = _bps_vec__ ? uf_con_vector_length(_bps_vec__) : 0;                               \
        _ast_foreach_init(&_decl_iter__, nullptr);                                                           \
        for (size_t _decl_idx__ = 0; _decl_idx__ < _funcs_len__ + _bps_len__; _decl_idx__++) {               \
            Ast* var = nullptr;                                                                              \
            if (_decl_idx__ < _funcs_len__) {                                                                \
                var = (Ast*)*(AstFuncDecl**)uf_con_vector_get(_funcs_vec__, _decl_idx__);                    \
            } else {                                                                                         \
                var = (Ast*)*(AstBlueprintDecl**)uf_con_vector_get(_bps_vec__, _decl_idx__ - _funcs_len__);  \
            }                                                                                                \
            if (var != nullptr)

#define ast_foreach_init_named(init, var)                                                                    \
    do {                                                                                                     \
        AstForeach _named_iter__;                                                                            \
        _ast_foreach_init(&_named_iter__, (const UfConVector*)((init) ? (init)->named : nullptr));           \
        for (; !_ast_foreach_done(&_named_iter__); _ast_foreach_next(&_named_iter__)) {                      \
            struct {                                                                                         \
                const char* name;                                                                            \
                AstExpr* value;                                                                              \
            }* var = *(typeof(var)**)uf_con_vector_get((const UfConVector*)_named_iter__.vector,             \
                                                       _named_iter__.index)

#define ast_foreach_init_indexed(init, var)                                                                  \
    do {                                                                                                     \
        AstForeach _indexed_iter__;                                                                          \
        _ast_foreach_init(&_indexed_iter__, (const UfConVector*)((init) ? (init)->indexed : nullptr));       \
        for (; !_ast_foreach_done(&_indexed_iter__); _ast_foreach_next(&_indexed_iter__)) {                  \
            struct {                                                                                         \
                AstExpr* index;                                                                              \
                AstExpr* value;                                                                              \
            }* var = *(typeof(var)**)uf_con_vector_get((const UfConVector*)_indexed_iter__.vector,           \
                                                       _indexed_iter__.index)

#define ast_foreach_idx(ast_or_prog, type, var) _AST_FOREACH_BEGIN_SIMPLE(type, var, ast_or_prog)

#define ast_foreach_idx_end ast_foreach_end_simple

/*
 * Reverse iterators - iterate from end to start.
 * Use for cleanup/deallocation (free in reverse order).
 */

static inline void _ast_foreach_rev_init(AstForeach* iter, const UfConVector* vec)
{
    iter->vector = vec;
    iter->length = vec ? uf_con_vector_length(vec) : 0;
    iter->index = iter->length;
    iter->current = nullptr;
}

static inline void _ast_foreach_rev_next(AstForeach* iter)
{
    if (iter->index > 0) {
        iter->index--;
    }
}

static inline bool _ast_foreach_rev_done(const AstForeach* iter)
{
    return iter->index == 0;
}

#define _AST_FOREACH_REV_BEGIN(type, iter_name, vec_ptr)                                                     \
    do {                                                                                                     \
        AstForeach iter_name##_riter__;                                                                      \
        _ast_foreach_rev_init(&iter_name##_riter__, (const UfConVector*)(vec_ptr));                          \
        for (; !_ast_foreach_rev_done(&iter_name##_riter__); _ast_foreach_rev_next(&iter_name##_riter__)) {  \
            type* iter_name = *(type**)uf_con_vector_get((UfConVector*)iter_name##_riter__.vector,           \
                                                         iter_name##_riter__.index - 1);                     \
            if (iter_name != nullptr)

#define _AST_FOREACH_REV_END                                                                                 \
    }                                                                                                        \
    }                                                                                                        \
    while (0)

#define ast_foreach_funcs_rev(ast_or_prog, var)                                                              \
    _AST_FOREACH_REV_BEGIN(AstFuncDecl, var,                                                                 \
                           (((ast_or_prog)->program) ? (ast_or_prog)->program->funcs : nullptr))

#define ast_foreach_blueprints_rev(ast_or_prog, var)                                                         \
    _AST_FOREACH_REV_BEGIN(AstBlueprintDecl, var,                                                            \
                           (((ast_or_prog)->program) ? (ast_or_prog)->program->blueprints : nullptr))

#define ast_foreach_end_rev _AST_FOREACH_REV_END
