/**
 * @brief Declaration and inheritance analysis for semantic analysis.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast.h"
#include "ii_ast_iter.h"
#include "ii_semantic.h"
#include "ii_semantic_internal.h"

#include <stdint.h>

static void _validate_blueprint(SemanticContext* context, AstBlueprintDecl* bp);
static void _check_duplicate_params(SemanticContext* context, UfConVector* params);
static void _compute_field_offsets(SemanticContext* context, AstBlueprintDecl* bp);
static void _flatten_blueprint(SemanticContext* context, AstBlueprintDecl* bp);
void _compute_all_field_offsets(SemanticContext* context);

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

/*
 * Inheritance resolution.
 */

void _resolve_inheritance(SemanticContext* context)
{
    TRACE_SCOPE(context->trace);

    ast_foreach_blueprints(context->ast, bp)
    {
        _resolve_blueprint_inheritance(context, bp);
    }
    ast_foreach_end;
}

void _resolve_blueprint_inheritance(SemanticContext* context, AstBlueprintDecl* bp)
{
    TRACE_SCOPE(context->trace);

    if (bp->inheritance_state == TAST_RESOLUTION_RESOLVED) {
        return;
    }

    if (bp->inheritance_state == TAST_RESOLUTION_RESOLVING) {
        _diag_error(context, bp->base.span, "Circular inheritance detected involving '%s'", bp->name);
        return;
    }

    bp->inheritance_state = TAST_RESOLUTION_RESOLVING;

    if (bp->parents != nullptr) {
        ast_foreach_parents(bp, inherit)
        {
            Symbol* sym = _scope_lookup_in_chain(context->global_scope, inherit->parent_name);
            if (sym == nullptr) {
                _diag_error(context, inherit->base.span, "Unknown parent blueprint '%s'",
                            inherit->parent_name);
                inherit->resolved = nullptr;
                continue;
            }

            if (sym->kind != SYMBOL_KIND_BLUEPRINT) {
                _diag_error(context, inherit->base.span, "Parent '%s' is not a blueprint",
                            inherit->parent_name);
                inherit->resolved = nullptr;
                continue;
            }

            inherit->resolved = (AstBlueprintDecl*)sym->decl;
            _resolve_blueprint_inheritance(context, inherit->resolved);
        }
        ast_foreach_end;
    }

    bp->inheritance_state = TAST_RESOLUTION_RESOLVED;

    ast_foreach_parents(bp, inherit)
    {
        if (inherit->field_aliases != nullptr) {
            size_t alias_count = uf_con_vector_length(inherit->field_aliases);
            for (size_t j = 0; j < alias_count; j++) {
                struct {
                    const char* original;
                    const char* alias;
                }* alias_ptr = (void*)uf_con_vector_get(inherit->field_aliases, j);
                if (inherit->resolved != nullptr) {
                    bool found = false;
                    if (inherit->resolved->flat_fields != nullptr) {
                        ast_foreach_flat_fields(inherit->resolved, field)
                        {
                            if (field->name == alias_ptr->original) {
                                found = true;
                                break;
                            }
                        }
                        ast_foreach_end;
                    }
                    if (!found && inherit->resolved->flat_methods != nullptr) {
                        ast_foreach_flat_methods(inherit->resolved, method)
                        {
                            if (method->name == alias_ptr->original) {
                                found = true;
                                break;
                            }
                        }
                        ast_foreach_end;
                    }
                    if (!found) {
                        _diag_error(context, inherit->base.span, "Unknown field '%s' in alias specification",
                                    alias_ptr->original);
                    }
                }
            }
        }
    }
    ast_foreach_end;

    _flatten_blueprint(context, bp);
}

static void _compute_field_offsets(SemanticContext* context, AstBlueprintDecl* bp)
{
    if (bp->flat_fields == nullptr) {
        return;
    }

    uint32_t offset = 0;
    uint32_t max_alignment = 1;

    size_t field_count = uf_con_vector_length(bp->flat_fields);
    for (size_t i = 0; i < field_count; i++) {
        AstField* field = *(AstField**)uf_con_vector_get(bp->flat_fields, i);
        if (field == nullptr) {
            continue;
        }

        if (field->type != nullptr && field->type->tast != nullptr) {
            uint32_t field_size = field->type->tast->size;
            uint32_t field_align = field->type->tast->alignment;

            /* Align offset to field alignment */
            offset = (offset + field_align - 1) & ~(field_align - 1);
            field->offset = offset;
            offset += field_size;

            if (field_align > max_alignment) {
                max_alignment = field_align;
            }
        } else {
            /* Default to 8-byte if type not resolved */
            offset = (offset + 7) & ~7;
            field->offset = offset;
            offset += 8;
            max_alignment = 8;
        }
    }

    /* Align total size to max alignment */
    offset = (offset + max_alignment - 1) & ~(max_alignment - 1);

    /* Store the blueprint size in its type for reference during codegen */
    AstType* bp_type = ii_ast_type_blueprint(context->ast, bp->name);
    if (bp_type->tast == nullptr) {
        bp_type->tast = ii_tast_type_new(context->ast);
    }
    bp_type->tast->size = offset;
    bp_type->tast->alignment = max_alignment;
    bp_type->variant.blueprint.resolved = bp;
    bp_type->tast->c_repr = ":Blueprint";
}

void _compute_all_field_offsets(SemanticContext* context)
{
    TRACE_SCOPE(context->trace);

    ast_foreach_blueprints(context->ast, bp)
    {
        _compute_field_offsets(context, bp);
    }
    ast_foreach_end;
}

void _flatten_blueprint(SemanticContext* context, AstBlueprintDecl* bp)
{
    TRACE_SCOPE(context->trace);

    if (bp->flat_fields == nullptr) {
        bp->flat_fields = uf_con_vector_new(sizeof(AstField*));
    }
    if (bp->flat_methods == nullptr) {
        bp->flat_methods = uf_con_vector_new(sizeof(AstMethod*));
    }

    ast_foreach_parents(bp, inherit)
    {
        if (inherit->resolved == nullptr) {
            continue;
        }

        AstBlueprintDecl* parent = inherit->resolved;
        if (parent->flat_fields == nullptr) {
            _flatten_blueprint(context, parent);
        }

        ast_foreach_flat_fields(parent, parent_field)
        {
            const char* alias_name = nullptr;
            if (inherit->field_aliases != nullptr) {
                size_t alias_count = uf_con_vector_length(inherit->field_aliases);
                for (size_t k = 0; k < alias_count; k++) {
                    struct {
                        const char* original;
                        const char* alias;
                    }* alias_ptr = (void*)uf_con_vector_get(inherit->field_aliases, k);
                    if (alias_ptr->original == parent_field->name) {
                        alias_name = alias_ptr->alias;
                        break;
                    }
                }
            }

            bool has_original_conflict = false;
            ast_foreach_flat_fields(bp, existing)
            {
                if (existing->name == parent_field->name) {
                    has_original_conflict = true;
                    break;
                }
            }
            ast_foreach_end;

            if (!has_original_conflict) {
                uf_con_vector_push(bp->flat_fields, &parent_field);
            }

            if (alias_name != nullptr) {
                bool has_alias_conflict = false;
                ast_foreach_flat_fields(bp, existing)
                {
                    if (existing->name == alias_name) {
                        has_alias_conflict = true;
                        break;
                    }
                }
                ast_foreach_end;

                if (!has_alias_conflict) {
                    AstField* field_copy = uf_mem_region_zalloc(context->symbol_arena, sizeof(AstField));
                    field_copy->name = alias_name;
                    field_copy->type = parent_field->type;
                    field_copy->default_value = parent_field->default_value;
                    uf_con_vector_push(bp->flat_fields, &field_copy);
                }
            }
        }
        ast_foreach_end;
    }
    ast_foreach_end;

    ast_foreach_fields(bp, local_field)
    {
        bool conflict = false;
        ast_foreach_flat_fields(bp, existing)
        {
            if (existing->name == local_field->name) {
                conflict = true;
                break;
            }
        }
        ast_foreach_end;

        if (conflict) {
            _diag_error(context, local_field->base.span, "Cannot override field '%s' from parent",
                        local_field->name);
        } else {
            uf_con_vector_push(bp->flat_fields, &local_field);
        }
    }
    ast_foreach_end;

    ast_foreach_parents(bp, inherit)
    {
        if (inherit->resolved == nullptr) {
            continue;
        }

        AstBlueprintDecl* parent = inherit->resolved;
        if (parent->flat_methods == nullptr) {
            _flatten_blueprint(context, parent);
        }

        ast_foreach_flat_methods(parent, parent_method)
        {
            const char* alias_name = nullptr;
            if (inherit->field_aliases != nullptr) {
                size_t alias_count = uf_con_vector_length(inherit->field_aliases);
                for (size_t k = 0; k < alias_count; k++) {
                    struct {
                        const char* original;
                        const char* alias;
                    }* alias_ptr = (void*)uf_con_vector_get(inherit->field_aliases, k);
                    if (alias_ptr->original == parent_method->name) {
                        alias_name = alias_ptr->alias;
                        break;
                    }
                }
            }

            bool has_override = false;
            ast_foreach_methods(bp, local_method)
            {
                if (local_method->name == parent_method->name) {
                    has_override = true;
                    break;
                }
            }
            ast_foreach_end;

            if (!has_override) {
                uf_con_vector_push(bp->flat_methods, &parent_method);
            }

            if (alias_name != nullptr) {
                bool has_alias_override = false;
                ast_foreach_methods(bp, local_method)
                {
                    if (local_method->name == alias_name) {
                        has_alias_override = true;
                        break;
                    }
                }
                ast_foreach_end;
                if (!has_alias_override) {
                    AstMethod* alias_method = uf_mem_region_zalloc(context->symbol_arena, sizeof(AstMethod));
                    alias_method->base.kind = AST_KIND_METHOD;
                    alias_method->name = alias_name;
                    alias_method->overloads = parent_method->overloads;
                    uf_con_vector_push(bp->flat_methods, &alias_method);
                }
            }
        }
        ast_foreach_end;
    }
    ast_foreach_end;

    ast_foreach_methods(bp, local_method)
    {
        uf_con_vector_push(bp->flat_methods, &local_method);
    }
    ast_foreach_end;

    /* Compute field offsets for codegen */
    _compute_field_offsets(context, bp);
}
