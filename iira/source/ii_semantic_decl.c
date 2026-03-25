/**
 * @brief Declaration and inheritance analysis for semantic analysis.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast_iter.h"
#include "ii_semantic.h"
#include "ii_semantic_internal.h"

#include <stdint.h>

static void _validate_blueprint(SemanticContext* context, AstBlueprintDecl* bp);
static void _check_duplicate_params(SemanticContext* context, UfConVector* params);
static bool _check_inheritance_cycle(SemanticContext* context, AstBlueprintDecl* bp);
static bool _check_cycle_recursive(SemanticContext* context, AstBlueprintDecl* bp, UfConVector* visited);
static void _flatten_blueprint(SemanticContext* context, AstBlueprintDecl* bp);

/*
 * Declaration analysis.
 */

void _analyze_declarations(SemanticContext* context)
{
    TRACE_SCOPE(context->trace);

    ast_foreach_blueprints(context->ast, bp)
    {
        Symbol* sym = _scope_insert(context, context->global_scope, bp->name, SYMBOL_KIND_BLUEPRINT, bp,
                                    nullptr, bp->base.span);
        if (sym != nullptr) {
            _validate_blueprint(context, bp);
        }
    }
    ast_foreach_end;

    ast_foreach_funcs(context->ast, func)
    {
        Symbol* sym = _scope_insert(context, context->global_scope, func->name, SYMBOL_KIND_FUNC, func,
                                    func->return_type, func->base.span);
        if (sym != nullptr) {
            _check_duplicate_params(context, func->params);
        }
    }
    ast_foreach_end;
}

void _validate_blueprint(SemanticContext* context, AstBlueprintDecl* bp)
{
    TRACE_SCOPE(context->trace);

    AstField* prev_field = nullptr;
    ast_foreach_fields(bp, field)
    {
        if (prev_field != nullptr && prev_field->name == field->name) {
            _diag_error(context, field->base.span, "Field '%s' is already defined in blueprint '%s'",
                        field->name, bp->name);
        }
        prev_field = field;
    }
    ast_foreach_end;

    AstMethod* prev_method = nullptr;
    ast_foreach_methods(bp, method)
    {
        if (prev_method != nullptr && prev_method->name == method->name) {
            _diag_error(context, method->base.span, "Method '%s' is already defined in blueprint '%s'",
                        method->name, bp->name);
        }
        prev_method = method;
    }
    ast_foreach_end;

    ast_foreach_methods(bp, method)
    {
        ast_foreach_overloads(method, overload)
        {
            _check_duplicate_params(context, overload->params);
        }
        ast_foreach_end;
    }
    ast_foreach_end;
}

static void _check_duplicate_params(SemanticContext* context, UfConVector* params)
{
    if (params == nullptr) {
        return;
    }

    size_t count = uf_con_vector_length(params);
    for (size_t i = 1; i < count; i++) {
        AstParam* prev = *(AstParam**)uf_con_vector_get(params, i - 1);
        AstParam* curr = *(AstParam**)uf_con_vector_get(params, i);
        if (prev != nullptr && curr != nullptr && prev->name == curr->name) {
            _diag_error(context, curr->base.span, "Parameter '%s' is already defined", curr->name);
        }
    }
}

