/**
 * @brief Type resolution for semantic analysis.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast_iter.h"
#include "ii_semantic_internal.h"

#include <string.h>

static const struct PrimitiveTypeInfo _prim_info[] = {
    [LEXER_PRIM_VOID] = {.size = 0, .alignment = 1, .c_repr = nullptr},
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

AstType* _make_self_type(SemanticContext* context, AstBlueprintDecl* bp)
{
    AstType* self_type = ii_ast_type_blueprint(context->ast, bp->name);
    if (self_type->tast == nullptr) {
        self_type->tast = ii_tast_type_new(context->ast);
    }
    self_type->tast->state = TAST_RESOLUTION_RESOLVED;
    self_type->variant.blueprint.resolved = bp;
    self_type->tast->size = 8;
    self_type->tast->alignment = 8;
    self_type->tast->c_repr = "l";
    return self_type;
}

void _resolve_all_types(SemanticContext* context)
{
    TRACE_SCOPE(context->trace);

    ast_foreach_blueprints(context->ast, bp)
    {
        ast_foreach_fields(bp, field)
        {
            if (field->type != nullptr) {
                _resolve_type(context, field->type);
            }
        }
        ast_foreach_end;

        ast_foreach_methods(bp, method)
        {
            ast_foreach_overloads(method, overload)
            {
                ast_foreach_params(overload, param)
                {
                    if (param->type != nullptr) {
                        _resolve_type(context, param->type);
                    }
                }
                ast_foreach_end;
                if (overload->return_type != nullptr) {
                    _resolve_type(context, overload->return_type);
                }
            }
            ast_foreach_end;
        }
        ast_foreach_end;
    }
    ast_foreach_end;

    ast_foreach_funcs(context->ast, func)
    {
        ast_foreach_params(func, param)
        {
            if (param->type != nullptr) {
                _resolve_type(context, param->type);
            }
        }
        ast_foreach_end;
        if (func->return_type != nullptr) {
            _resolve_type(context, func->return_type);
        }
    }
    ast_foreach_end;
}

TastType* _resolve_type(SemanticContext* context, AstType* type)
{
    TRACE_SCOPE(context->trace);

    if (type == nullptr) {
        return nullptr;
    }

    if (type->tast != nullptr && ii_tast_type_is_resolved(type->tast)) {
        return type->tast;
    }

    if (type->tast == nullptr) {
        type->tast = ii_tast_type_new(context->ast);
    }

    type->tast->state = TAST_RESOLUTION_RESOLVING;

    switch (type->tag) {
    case AST_TYPE_KIND_PRIMITIVE: {
        const struct PrimitiveTypeInfo* info = _get_prim_info(ii_ast_type_get_primitive(type));
        type->tast->size = info->size;
        type->tast->alignment = info->alignment;
        type->tast->c_repr = info->c_repr;
        break;
    }

    case AST_TYPE_KIND_POINTER: {
        if (ii_ast_type_get_pointed(type) != nullptr) {
            TastType* pointed = _resolve_type(context, ii_ast_type_get_pointed(type));
            (void)pointed;
        }
        type->tast->size = 8;
        type->tast->alignment = 8;
        type->tast->c_repr = "l";
        break;
    }

    case AST_TYPE_KIND_ARRAY: {
        if (ii_ast_type_get_array_element(type) != nullptr) {
            TastType* element = _resolve_type(context, ii_ast_type_get_array_element(type));
            if (ii_ast_type_get_array_size(type) != nullptr) {
                if (ii_ast_expr_get_kind(ii_ast_type_get_array_size(type)) == AST_KIND_LITERAL) {
                    AstLiteral* lit = (AstLiteral*)ii_ast_type_get_array_size(type);
                    if (lit->variant == LITERAL_INT) {
                        type->tast->size = element->size * (uint32_t)lit->literal.int_value;
                    }
                }
            } else {
                type->tast->size = element->size;
            }
            type->tast->alignment = element->alignment;
            type->tast->c_repr = "l";
        } else {
            type->tast->size = 8;
            type->tast->alignment = 8;
            type->tast->c_repr = "l";
        }
        break;
    }

    case AST_TYPE_KIND_BLUEPRINT: {
        const char* name = type->variant.blueprint.name;
        Symbol* sym = _scope_lookup_in_chain(context->global_scope, name);
        if (sym != nullptr && sym->kind == SYMBOL_KIND_BLUEPRINT) {
            AstBlueprintDecl* bp = (AstBlueprintDecl*)sym->decl;
            type->variant.blueprint.resolved = bp;

            uint32_t bp_size = 0;
            uint32_t bp_align = 1;

            if (bp->flat_fields != nullptr) {
                size_t field_count = uf_con_vector_length(bp->flat_fields);
                for (size_t i = 0; i < field_count; i++) {
                    AstField* f = *(AstField**)uf_con_vector_get(bp->flat_fields, i);
                    if (f != nullptr && f->type != nullptr) {
                        if (f->type->tast == nullptr || f->type->tast->state != TAST_RESOLUTION_RESOLVING) {
                            _resolve_type(context, f->type);
                        }
                    }

                    uint32_t f_size = 0;
                    uint32_t f_align = 1;

                    if (f != nullptr && f->type != nullptr) {
                        if (f->type->tag == AST_TYPE_KIND_POINTER) {
                            f_size = 8;
                            f_align = 8;
                        } else if (f->type->tast != nullptr &&
                                   f->type->tast->state == TAST_RESOLUTION_RESOLVED) {
                            f_size = f->type->tast->size;
                            f_align = f->type->tast->alignment;
                        } else {
                            f_size = 8;
                            f_align = 8;
                        }
                    }

                    if (f_size > 0) {
                        bp_size = (bp_size + f_align - 1) & ~(f_align - 1);
                        bp_size += f_size;
                        if (f_align > bp_align)
                            bp_align = f_align;
                    }
                }
                bp_size = (bp_size + bp_align - 1) & ~(bp_align - 1);
            }

            if (bp->flat_fields != nullptr && uf_con_vector_length(bp->flat_fields) > 0) {
                uf_assert_msg(bp_size > 0, "Blueprint '%s' has fields but zero size", name);
            }

            type->tast->size = bp_size > 0 ? bp_size : 8;
            type->tast->alignment = bp_align;
            type->tast->c_repr = "l";
        } else {
            Symbol* var_sym = _scope_lookup_in_chain(context->current_scope, name);
            if (var_sym != nullptr &&
                (var_sym->kind == SYMBOL_KIND_VAR || var_sym->kind == SYMBOL_KIND_PARAM)) {
                AstType* var_type = var_sym->type;
                if (var_type != nullptr && var_type->tag == AST_TYPE_KIND_BLUEPRINT) {
                    _resolve_type(context, var_type);
                    AstBlueprintDecl* original_bp = ii_ast_type_get_blueprint_resolved(var_type);
                    if (original_bp != nullptr) {
                        static uint32_t proto_counter = 0;
                        char proto_name[64];
                        snprintf(proto_name, sizeof(proto_name), "__proto_%u", proto_counter++);
                        const char* interned_name =
                            ii_src_intern(context->src, proto_name, strlen(proto_name));

                        AstBlueprintDecl* new_bp = ii_ast_blueprint_decl(context->ast, interned_name);

                        if (original_bp->flat_fields != nullptr) {
                            size_t field_count = uf_con_vector_length(original_bp->flat_fields);
                            for (size_t i = 0; i < field_count; i++) {
                                AstField** field_ptr =
                                    (AstField**)uf_con_vector_get(original_bp->flat_fields, i);
                                if (field_ptr != nullptr && *field_ptr != nullptr) {
                                    AstField* copy =
                                        ii_ast_field(context->ast, (*field_ptr)->name, (*field_ptr)->type,
                                                     (*field_ptr)->default_value);
                                    if (copy->type != nullptr) {
                                        _resolve_type(context, copy->type);
                                    }
                                    uf_con_vector_push(new_bp->flat_fields, &copy);
                                }
                            }
                        }

                        if (original_bp->flat_methods != nullptr) {
                            size_t method_count = uf_con_vector_length(original_bp->flat_methods);
                            for (size_t i = 0; i < method_count; i++) {
                                AstMethod** method_ptr =
                                    (AstMethod**)uf_con_vector_get(original_bp->flat_methods, i);
                                if (method_ptr != nullptr && *method_ptr != nullptr) {
                                    uf_con_vector_push(new_bp->flat_methods, method_ptr);
                                }
                            }
                        }

                        ii_ast_program_add_blueprint(context->ast, new_bp);
                        _scope_insert(context, context->global_scope, interned_name, SYMBOL_KIND_BLUEPRINT,
                                      new_bp, nullptr, type->base.span);

                        type->variant.blueprint.resolved = new_bp;

                        uint32_t bp_size = 0;
                        uint32_t bp_align = 1;

                        if (new_bp->flat_fields != nullptr) {
                            size_t field_count = uf_con_vector_length(new_bp->flat_fields);
                            for (size_t i = 0; i < field_count; i++) {
                                AstField* f = *(AstField**)uf_con_vector_get(new_bp->flat_fields, i);
                                if (f != nullptr && f->type != nullptr && f->type->tast != nullptr) {
                                    uint32_t f_align = f->type->tast->alignment;
                                    uint32_t f_size = f->type->tast->size;

                                    bp_size = (bp_size + f_align - 1) & ~(f_align - 1);
                                    bp_size += f_size;

                                    if (f_align > bp_align)
                                        bp_align = f_align;
                                }
                            }
                            bp_size = (bp_size + bp_align - 1) & ~(bp_align - 1);
                        }

                        type->tast->size = bp_size > 0 ? bp_size : 8;
                        type->tast->alignment = bp_align;
                        type->tast->c_repr = "l";
                        break;
                    }
                }
            }
            _diag_error(context, type->base.span, "Unknown type '%s'", name);
            type->variant.blueprint.resolved = nullptr;
            type->tast->size = 0;
        }
        break;
    }

    case AST_TYPE_KIND_ANON: {
        uint32_t anon_size = 0;
        uint32_t anon_align = 1;

        if (type->variant.anon.fields != nullptr && type->variant.anon.field_count > 0) {
            for (size_t i = 0; i < type->variant.anon.field_count; i++) {
                AstField* field = &type->variant.anon.fields[i];
                if (field->type != nullptr) {
                    _resolve_type(context, field->type);
                }
                if (field->type != nullptr && field->type->tast != nullptr) {
                    uint32_t f_align = field->type->tast->alignment;
                    uint32_t f_size = field->type->tast->size;

                    anon_size = (anon_size + f_align - 1) & ~(f_align - 1);
                    field->offset = anon_size;
                    anon_size += f_size;

                    if (f_align > anon_align)
                        anon_align = f_align;
                }
            }
            anon_size = (anon_size + anon_align - 1) & ~(anon_align - 1);

            uf_assert_msg(anon_size > 0, "Anonymous type has fields but zero size");
        }

        type->tast->size = anon_size > 0 ? anon_size : 8;
        type->tast->alignment = anon_align;
        type->tast->c_repr = "l";
        break;
    }

    default:
        type->tast->size = 0;
        type->tast->alignment = 1;
        type->tast->c_repr = "w";
        break;
    }

    type->tast->state = TAST_RESOLUTION_RESOLVED;

    return type->tast;
}

bool _types_match(SemanticContext* context, AstType* a, AstType* b)
{
    if (a == nullptr && b == nullptr) {
        return true;
    }

    if (a == nullptr || b == nullptr) {
        return false;
    }

    if (a->tag != b->tag) {
        return false;
    }

    switch (a->tag) {
    case AST_TYPE_KIND_PRIMITIVE: {
        enum LexerPrimitiveType pa = ii_ast_type_get_primitive(a);
        enum LexerPrimitiveType pb = ii_ast_type_get_primitive(b);
        if (pa == pb) {
            return true;
        }
        bool a_is_wider = (pa == LEXER_PRIM_LONG || pa == LEXER_PRIM_DOUBLE);
        bool b_is_wider = (pb == LEXER_PRIM_LONG || pb == LEXER_PRIM_DOUBLE);
        if (pa == LEXER_PRIM_INT && (b_is_wider || pb == LEXER_PRIM_SHORT)) {
            return true;
        }
        if (pb == LEXER_PRIM_INT && (a_is_wider || pa == LEXER_PRIM_SHORT)) {
            return true;
        }
        if (pa == LEXER_PRIM_FLOAT && pb == LEXER_PRIM_DOUBLE) {
            return true;
        }
        if (pb == LEXER_PRIM_FLOAT && pa == LEXER_PRIM_DOUBLE) {
            return true;
        }
        return false;
    }

    case AST_TYPE_KIND_POINTER: {
        if (_is_void_ptr(a) || _is_void_ptr(b)) {
            return true;
        }
        return _types_match(context, ii_ast_type_get_pointed(a), ii_ast_type_get_pointed(b));
    }

    case AST_TYPE_KIND_ARRAY:
        if (!_types_match(context, ii_ast_type_get_array_element(a), ii_ast_type_get_array_element(b))) {
            return false;
        }
        return a->variant.array.fixed_size == b->variant.array.fixed_size;

    case AST_TYPE_KIND_BLUEPRINT:
        return ii_ast_type_get_blueprint_resolved(a) == ii_ast_type_get_blueprint_resolved(b);

    case AST_TYPE_KIND_ANON:
        if (a->variant.anon.field_count != b->variant.anon.field_count) {
            return false;
        }
        return true;

    default:
        return false;
    }
}

/*
 * Numeric coercion check: int/long/short -> float/double.
 *
 * Allows: var f: float = 0; var d: double = 42;
 */

bool _can_coerce_numeric(AstType* target, AstType* source)
{
    if (target == nullptr || source == nullptr) {
        return false;
    }

    if (!_is_primitive_type(target) || !_is_primitive_type(source)) {
        return false;
    }

    enum LexerPrimitiveType target_prim = ii_ast_type_get_primitive(target);
    if (target_prim == LEXER_PRIM_FLOAT || target_prim == LEXER_PRIM_DOUBLE) {
        enum LexerPrimitiveType source_prim = ii_ast_type_get_primitive(source);
        return source_prim == LEXER_PRIM_INT || source_prim == LEXER_PRIM_LONG ||
               source_prim == LEXER_PRIM_SHORT;
    }

    return false;
}

/*
 * Expression-aware coercion check.
 *
 * Handles:
 *   1. Numeric: int -> float/double
 *   2. Integer 0 -> pointer (null pointer)
 *   3. Null literal -> any pointer
 *   4. Integer 0 -> single-field blueprint (unwrap)
 */

bool _can_coerce_expr(SemanticContext* context, AstType* target, AstType* source, AstExpr* expr)
{
    if (target == nullptr || source == nullptr) {
        return false;
    }

    if (_can_coerce_numeric(target, source)) {
        return true;
    }

    if (_is_pointer_type(target) && expr != nullptr) {
        if (_is_zero_literal(expr) || _is_null_literal(expr)) {
            return true;
        }
    }

    if (_is_blueprint_type(target)) {
        AstBlueprintDecl* bp = ii_ast_type_get_blueprint_resolved(target);
        if (bp == nullptr || bp->flat_fields == nullptr) {
            return false;
        }

        size_t field_count = uf_con_vector_length(bp->flat_fields);
        if (field_count == 1) {
            AstField* field = *(AstField**)uf_con_vector_get(bp->flat_fields, 0);
            if (field != nullptr && _types_match(context, field->type, source)) {
                return true;
            }
        }
    }

    return false;
}

/*
 * Equality comparison coercion.
 *
 * Allows: same types, pointer vs void*, pointer vs 0/null, any vs 0/null
 */

bool _can_coerce_for_equality(SemanticContext* context, AstType* left, AstType* right, AstExpr* left_expr,
                              AstExpr* right_expr)
{
    if (left == nullptr || right == nullptr) {
        return false;
    }

    if (_types_match(context, left, right)) {
        return true;
    }

    bool left_is_void_ptr = _is_void_ptr(left);
    bool right_is_void_ptr = _is_void_ptr(right);
    if ((_is_pointer_type(left) && right_is_void_ptr) || (_is_pointer_type(right) && left_is_void_ptr)) {
        return true;
    }

    bool left_is_zero = _is_zero_literal(left_expr);
    bool right_is_zero = _is_zero_literal(right_expr);
    bool left_is_null = _is_null_literal(left_expr);
    bool right_is_null = _is_null_literal(right_expr);

    if ((_is_pointer_type(left) && (right_is_zero || right_is_null)) ||
        (_is_pointer_type(right) && (left_is_zero || left_is_null))) {
        return true;
    }

    return left_is_zero || right_is_zero || left_is_null || right_is_null;
}

const char* _type_name(SemanticContext* context, AstType* type)
{
    if (type == nullptr) {
        return "unknown";
    }

    switch (type->tag) {
    case AST_TYPE_KIND_PRIMITIVE:
        return _type_to_string(ii_ast_type_get_primitive(type));

    case AST_TYPE_KIND_POINTER: {
        static char buffer[256];
        const char* inner = _type_name(context, ii_ast_type_get_pointed(type));
        snprintf(buffer, sizeof(buffer), "%s*", inner);
        return buffer;
    }

    case AST_TYPE_KIND_ARRAY: {
        static char buffer[256];
        const char* inner = _type_name(context, ii_ast_type_get_array_element(type));
        if (ii_ast_type_get_array_size(type) != nullptr) {
            snprintf(buffer, sizeof(buffer), "%s[]", inner);
        } else if (type->variant.array.fixed_size > 0) {
            snprintf(buffer, sizeof(buffer), "%s[%u]", inner, type->variant.array.fixed_size);
        } else {
            snprintf(buffer, sizeof(buffer), "%s[]", inner);
        }
        return buffer;
    }

    case AST_TYPE_KIND_BLUEPRINT:
        return type->variant.blueprint.name;

    case AST_TYPE_KIND_ANON:
        return type->variant.anon.auto_name ? type->variant.anon.auto_name : "{ ... }";

    default:
        return "unknown";
    }
}
