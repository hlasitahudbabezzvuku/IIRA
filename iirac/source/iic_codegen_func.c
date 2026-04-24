/**
 * @brief QBE function generation.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast_iter.h"
#include "ii_lexer.h"
#include "iic_codegen_internal.h"

#include <stdio.h>
#include <string.h>

static void _emit_params(CodegenContext* ctx, UfConVector* params)
{
    if (params == nullptr) {
        return;
    }

    bool first = true;
    size_t count = uf_con_vector_length(params);
    for (size_t i = 0; i < count; i++) {
        AstParam* param = *(AstParam**)uf_con_vector_get(params, i);
        if (param == nullptr) {
            continue;
        }
        if (!first) {
            fprintf(ctx->output, ", ");
        }
        first = false;

        const char* param_suffix = _get_type_suffix(param->type);
        if (param_suffix != nullptr && param_suffix[0] == 'b') {
            param_suffix = "sb";
        } else if (param_suffix != nullptr && param_suffix[0] == 'h') {
            param_suffix = "sh";
        }
        fprintf(ctx->output, "%s %%slot%u", param_suffix, param->slot_index);
    }
}

static void _emit_function_signature(CodegenContext* ctx, const char* name, AstType* return_type,
                                     UfConVector* params)
{
    bool is_void = return_type != nullptr && ii_ast_type_get_primitive(return_type) == LEXER_PRIM_VOID;

    if (return_type == nullptr || is_void) {
        fprintf(ctx->output, "export function $%s(", name);
    } else {
        const char* ret_suffix = _get_type_suffix(return_type);
        if (ret_suffix != nullptr && ret_suffix[0] == 'b') {
            ret_suffix = "sb";
        } else if (ret_suffix != nullptr && ret_suffix[0] == 'h') {
            ret_suffix = "sh";
        }
        fprintf(ctx->output, "export function %s $%s(", ret_suffix, name);
    }

    _emit_params(ctx, params);

    fprintf(ctx->output, ") {\n");
}

static void _store_params_to_slots(CodegenContext* ctx, AstFuncDecl* func)
{
    if (func->frame_size == 0) {
        return;
    }

    if (func->params == nullptr) {
        return;
    }

    size_t count = uf_con_vector_length(func->params);
    for (size_t i = 0; i < count; i++) {
        AstParam* param = *(AstParam**)uf_con_vector_get(func->params, i);
        if (param == nullptr) {
            continue;
        }
        uint32_t offset = _get_slot_offset(param->slot_index);
        const char* param_suffix = _get_type_suffix(param->type);

        fprintf(ctx->output, "    # store param %s to slot %u\n", param->name, param->slot_index);
        fprintf(ctx->output, "    %%addr%u =l add %%frame, %u\n", param->slot_index, offset);
        if (param_suffix != nullptr && param_suffix[0] == 'b') {
            fprintf(ctx->output, "    storeb %%slot%u, %%addr%u\n", param->slot_index, param->slot_index);
        } else if (param_suffix != nullptr && param_suffix[0] == 'h') {
            fprintf(ctx->output, "    storeh %%slot%u, %%addr%u\n", param->slot_index, param->slot_index);
        } else {
            fprintf(ctx->output, "    store%s %%slot%u, %%addr%u\n", param_suffix, param->slot_index,
                    param->slot_index);
        }
    }
}

static void _store_method_params_to_slots(CodegenContext* ctx, AstMethodOverload* overload)
{
    if (overload->frame_size == 0) {
        return;
    }

    if (overload->params == nullptr) {
        return;
    }

    size_t count = uf_con_vector_length(overload->params);
    for (size_t i = 0; i < count; i++) {
        AstParam* param = *(AstParam**)uf_con_vector_get(overload->params, i);
        if (param == nullptr) {
            continue;
        }
        uint32_t offset = _get_slot_offset(param->slot_index);
        const char* param_suffix = _get_type_suffix(param->type);

        fprintf(ctx->output, "    # store param %s to slot %u\n", param->name, param->slot_index);
        fprintf(ctx->output, "    %%addr%u =l add %%frame, %u\n", param->slot_index, offset);
        if (param_suffix != nullptr && param_suffix[0] == 'b') {
            fprintf(ctx->output, "    storeb %%slot%u, %%addr%u\n", param->slot_index, param->slot_index);
        } else if (param_suffix != nullptr && param_suffix[0] == 'h') {
            fprintf(ctx->output, "    storeh %%slot%u, %%addr%u\n", param->slot_index, param->slot_index);
        } else {
            fprintf(ctx->output, "    store%s %%slot%u, %%addr%u\n", param_suffix, param->slot_index,
                    param->slot_index);
        }
    }
}

void _emit_function(CodegenContext* ctx, AstFuncDecl* func)
{
    ctx->current_func = func;
    ctx->current_frame_size = func->frame_size;
    ctx->next_slot = 0;

    _emit_function_signature(ctx, func->name, func->return_type, func->params);

    if (func->body == nullptr) {
        fprintf(ctx->output, "}\n");
        return;
    }

    fprintf(ctx->output, "\n  @start\n");
    fprintf(ctx->output, "    jmp @entry\n");

    fprintf(ctx->output, "\n  @entry\n");

    if (func->frame_size > 0) {
        fprintf(ctx->output, "    # allocate frame (%u bytes)\n", func->frame_size);
        fprintf(ctx->output, "    %%frame =l alloc8 %u\n", func->frame_size);
    }

    _store_params_to_slots(ctx, func);

    if (func->body != nullptr) {
        BlockInfo info = {
            .label = nullptr,
            .true_label = nullptr,
            .false_label = nullptr,
            .break_label = nullptr,
            .continue_label = nullptr,
        };
        _emit_block(ctx, func->body, &info);
    }

    if (func->return_type == nullptr) {
        fprintf(ctx->output, "    ret\n");
    }

    fprintf(ctx->output, "}\n");
}

void _emit_method(CodegenContext* ctx, AstBlueprintDecl* bp, AstMethod* method)
{
    (void)bp;

    if (method->overloads == nullptr) {
        return;
    }

    ast_foreach_overloads(method, overload)
    {
        if (overload->body == nullptr) {
            continue;
        }

        ctx->current_func = nullptr;
        ctx->current_frame_size = overload->frame_size;
        ctx->next_slot = 0;

        _emit_function_signature(ctx, method->name, overload->return_type, overload->params);

        fprintf(ctx->output, "\n  @start\n");
        fprintf(ctx->output, "    jmp @entry\n");

        fprintf(ctx->output, "\n  @entry\n");

        if (overload->frame_size > 0) {
            fprintf(ctx->output, "    # allocate frame (%u bytes)\n", overload->frame_size);
            fprintf(ctx->output, "    %%frame =l alloc8 %u\n", overload->frame_size);
        }

        _store_method_params_to_slots(ctx, overload);

        if (overload->body != nullptr) {
            BlockInfo info = {
                .label = nullptr,
                .true_label = nullptr,
                .false_label = nullptr,
                .break_label = nullptr,
                .continue_label = nullptr,
            };
            _emit_block(ctx, overload->body, &info);
        }

        if (overload->return_type == nullptr) {
            fprintf(ctx->output, "    ret\n");
        }

        fprintf(ctx->output, "}\n");
    }
    ast_foreach_end;
}
