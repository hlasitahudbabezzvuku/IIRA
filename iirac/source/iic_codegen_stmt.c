/**
 * @brief QBE statement generation.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast_iter.h"
#include "iic_codegen_internal.h"

#include <stdio.h>
#include <string.h>

static bool _block_returns(AstBlock* block);

static bool _stmt_returns(AstStmt* stmt)
{
    if (stmt == nullptr) {
        return false;
    }
    switch (stmt->kind) {
    case AST_KIND_RETURN:
        return true;
    case AST_KIND_BREAK:
    case AST_KIND_CONTINUE:
        return true;
    case AST_KIND_IF: {
        AstIf* if_stmt = &stmt->if_stmt;
        bool then_returns = _block_returns(if_stmt->then_block);
        bool else_returns = if_stmt->else_stmt != nullptr ? _stmt_returns(if_stmt->else_stmt) : false;
        return then_returns && else_returns;
    }
    case AST_KIND_BLOCK: {
        AstBlock* block = &stmt->block;
        if (block->stmts == nullptr) {
            return false;
        }
        size_t count = uf_con_vector_length(block->stmts);
        if (count == 0) {
            return false;
        }
        AstStmt* last = *(AstStmt**)uf_con_vector_get(block->stmts, count - 1);
        return _stmt_returns(last);
    }
    default:
        return false;
    }
}

static bool _block_returns(AstBlock* block)
{
    if (block == nullptr || block->stmts == nullptr) {
        return false;
    }
    size_t count = uf_con_vector_length(block->stmts);
    if (count == 0) {
        return false;
    }
    AstStmt* last = *(AstStmt**)uf_con_vector_get(block->stmts, count - 1);
    return _stmt_returns(last);
}

/* Emit a block statement (sequence of statements). */
void _emit_block(CodegenContext* ctx, AstBlock* block, BlockInfo* info)
{
    if (block == nullptr || block->stmts == nullptr) {
        return;
    }

    ast_foreach_stmts(block, stmt)
    {
        _emit_stmt(ctx, stmt, info);
    }
    ast_foreach_end;
}

/* Dispatch to appropriate statement handler based on stmt->kind. */
void _emit_stmt(CodegenContext* ctx, AstStmt* stmt, BlockInfo* info)
{
    if (stmt == nullptr) {
        return;
    }

    switch (stmt->kind) {
    case AST_KIND_BLOCK:
        _emit_block(ctx, &stmt->block, info);
        break;
    case AST_KIND_RETURN:
        _emit_return(ctx, &stmt->ret);
        break;
    case AST_KIND_DECL:
        _emit_decl(ctx, &stmt->decl);
        break;
    case AST_KIND_IF:
        _emit_if(ctx, &stmt->if_stmt, info);
        break;
    case AST_KIND_WHILE:
        _emit_while(ctx, &stmt->while_stmt, info);
        break;
    case AST_KIND_FOR:
        _emit_for(ctx, &stmt->for_stmt, info);
        break;
    case AST_KIND_DO_WHILE:
        _emit_do_while(ctx, &stmt->do_while, info);
        break;
    case AST_KIND_BREAK:
        _emit_break(ctx, &stmt->break_stmt, info);
        break;
    case AST_KIND_CONTINUE:
        _emit_continue(ctx, &stmt->continue_stmt, info);
        break;
    case AST_KIND_EXPR_STMT:
        _emit_expr(ctx, stmt->expr_stmt.expr);
        break;
    default:
        break;
    }
}

/* Emit return statement. */
void _emit_return(CodegenContext* ctx, AstReturn* ret)
{
    if (ret->value != nullptr) {
        GenValue val = _emit_expr(ctx, ret->value);
        fprintf(ctx->output, "    ret %s\n", val.qbe_temp);
    } else {
        fprintf(ctx->output, "    ret\n");
    }
}

/* Emit variable declaration statement. */
void _emit_decl(CodegenContext* ctx, AstDecl* decl)
{
    uint32_t offset = _get_slot_offset(decl->slot_index);
    const char* type_suffix = _get_type_suffix(decl->type);

    const char* store_suffix = type_suffix;
    if (type_suffix != nullptr && type_suffix[0] == 'l' && decl->type != nullptr &&
        decl->type->tag == AST_TYPE_KIND_BLUEPRINT) {
        AstBlueprintDecl* bp = ii_ast_type_get_blueprint_resolved(decl->type);
        if (bp != nullptr && bp->flat_fields != nullptr && uf_con_vector_length(bp->flat_fields) == 1) {
            AstField* field = *(AstField**)uf_con_vector_get(bp->flat_fields, 0);
            store_suffix = _get_type_suffix(field->type);
        }
    }

    if (decl->type != nullptr && decl->type->tag == AST_TYPE_KIND_ARRAY) {
        uint32_t array_size = 0;
        if (decl->type->tast != nullptr) {
            array_size = decl->type->tast->size;
        }

        if (array_size > 0) {
            fprintf(ctx->output, "    # allocate local: %s (array, %u bytes)\n", decl->name, array_size);
            const char* tmp = _alloc_temp(ctx);
            fprintf(ctx->output, "    %s =l alloc8 %u\n", tmp, array_size);

            if (decl->init != nullptr) {
                GenValue val = _emit_expr(ctx, decl->init);
                fprintf(ctx->output, "    # copy initial value\n");
                fprintf(ctx->output, "    blit %s, %s, %u\n", val.qbe_temp, tmp, array_size);
            }

            fprintf(ctx->output, "    %%slot%u =l add %%frame, %u\n", decl->slot_index, offset);
            fprintf(ctx->output, "    storel %s, %%slot%u\n", tmp, decl->slot_index);
            return;
        }
    }

    if (decl->type != nullptr && decl->type->tag == AST_TYPE_KIND_BLUEPRINT) {
        AstBlueprintDecl* bp = ii_ast_type_get_blueprint_resolved(decl->type);
        bool single_field =
            bp != nullptr && bp->flat_fields != nullptr && uf_con_vector_length(bp->flat_fields) == 1;

        if (!single_field) {
            uint32_t struct_size = 0;
            if (decl->type->tast != nullptr) {
                struct_size = decl->type->tast->size;
            }

            if (struct_size > 0) {
                fprintf(ctx->output, "    # allocate local: %s (struct, %u bytes)\n", decl->name,
                        struct_size);
                const char* tmp = _alloc_temp(ctx);
                fprintf(ctx->output, "    %s =l alloc8 %u\n", tmp, struct_size);

                if (decl->init != nullptr) {
                    GenValue val = _emit_expr(ctx, decl->init);
                    fprintf(ctx->output, "    # copy initial value\n");
                    fprintf(ctx->output, "    blit %s, %s, %u\n", val.qbe_temp, tmp, struct_size);
                }

                fprintf(ctx->output, "    %%slot%u =l add %%frame, %u\n", decl->slot_index, offset);
                fprintf(ctx->output, "    storel %s, %%slot%u\n", tmp, decl->slot_index);
                return;
            }
        }
    }

    fprintf(ctx->output, "    # allocate local: %s\n", decl->name);
    fprintf(ctx->output, "    %%slot%u =l add %%frame, %u\n", decl->slot_index, offset);

    if (decl->init != nullptr) {
        GenValue val = _emit_expr(ctx, decl->init);
        fprintf(ctx->output, "    # store initial value to slot %u\n", decl->slot_index);

        if (type_suffix != nullptr && type_suffix[0] == 'b' && val.type_suffix != nullptr &&
            val.type_suffix[0] == 'w') {
            const char* tmp = _alloc_temp(ctx);
            fprintf(ctx->output, "    %s =w extsb %s\n", tmp, val.qbe_temp);
            fprintf(ctx->output, "    storeb %s, %%slot%u\n", tmp, decl->slot_index);
        } else if (type_suffix != nullptr && type_suffix[0] == 'b' && val.type_suffix != nullptr &&
                   val.type_suffix[0] == 'l') {
            const char* tmp = _alloc_temp(ctx);
            fprintf(ctx->output, "    %s =w extsb %s\n", tmp, val.qbe_temp);
            fprintf(ctx->output, "    storeb %s, %%slot%u\n", tmp, decl->slot_index);
        } else if (type_suffix != nullptr && type_suffix[0] == 'h' && val.type_suffix != nullptr &&
                   val.type_suffix[0] == 'w') {
            const char* tmp = _alloc_temp(ctx);
            fprintf(ctx->output, "    %s =w extsh %s\n", tmp, val.qbe_temp);
            fprintf(ctx->output, "    storeh %s, %%slot%u\n", tmp, decl->slot_index);
        } else if (type_suffix != nullptr && type_suffix[0] == 'h' && val.type_suffix != nullptr &&
                   val.type_suffix[0] == 'l') {
            const char* tmp = _alloc_temp(ctx);
            fprintf(ctx->output, "    %s =w extsh %s\n", tmp, val.qbe_temp);
            fprintf(ctx->output, "    storeh %s, %%slot%u\n", tmp, decl->slot_index);
        } else if (type_suffix != nullptr && type_suffix[0] == 'l' && val.type_suffix != nullptr &&
                   val.type_suffix[0] == 'w') {
            const char* tmp = _alloc_temp(ctx);
            fprintf(ctx->output, "    %s =l extsw %s\n", tmp, val.qbe_temp);
            fprintf(ctx->output, "    storel %s, %%slot%u\n", tmp, decl->slot_index);
        } else if (type_suffix != nullptr && type_suffix[0] == 'd' && val.type_suffix != nullptr &&
                   val.type_suffix[0] == 's') {
            const char* tmp = _alloc_temp(ctx);
            fprintf(ctx->output, "    %s =d exts %s\n", tmp, val.qbe_temp);
            fprintf(ctx->output, "    stored %s, %%slot%u\n", tmp, decl->slot_index);
        } else if (type_suffix != nullptr && type_suffix[0] == 's' && val.type_suffix != nullptr &&
                   val.type_suffix[0] == 'd') {
            const char* tmp = _alloc_temp(ctx);
            fprintf(ctx->output, "    %s =s truncd %s\n", tmp, val.qbe_temp);
            fprintf(ctx->output, "    stores %s, %%slot%u\n", tmp, decl->slot_index);
        } else if (type_suffix != nullptr && type_suffix[0] == 's' && val.type_suffix != nullptr &&
                   (val.type_suffix[0] == 'w' || val.type_suffix[0] == 'l')) {
            const char* tmp = _alloc_temp(ctx);
            if (val.type_suffix[0] == 'w') {
                fprintf(ctx->output, "    %s =s swtof %s\n", tmp, val.qbe_temp);
            } else {
                fprintf(ctx->output, "    %s =s sltof %s\n", tmp, val.qbe_temp);
            }
            fprintf(ctx->output, "    stores %s, %%slot%u\n", tmp, decl->slot_index);
        } else if (type_suffix != nullptr && type_suffix[0] == 'd' && val.type_suffix != nullptr &&
                   (val.type_suffix[0] == 'w' || val.type_suffix[0] == 'l')) {
            const char* tmp = _alloc_temp(ctx);
            if (val.type_suffix[0] == 'w') {
                fprintf(ctx->output, "    %s =d swtof %s\n", tmp, val.qbe_temp);
            } else {
                fprintf(ctx->output, "    %s =d sltof %s\n", tmp, val.qbe_temp);
            }
            fprintf(ctx->output, "    stored %s, %%slot%u\n", tmp, decl->slot_index);
        } else {
            fprintf(ctx->output, "    store%s %s, %%slot%u\n", store_suffix, val.qbe_temp, decl->slot_index);
        }
    } else {
        if (type_suffix != nullptr && (type_suffix[0] == 'b' || type_suffix[0] == 'h')) {
            const char* tmp = _alloc_temp(ctx);
            fprintf(ctx->output, "    # zero-initialize\n");
            fprintf(ctx->output, "    %s =w copy 0\n", tmp);
            fprintf(ctx->output, "    store%s %s, %%slot%u\n", type_suffix, tmp, decl->slot_index);
        } else {
            fprintf(ctx->output, "    # zero-initialize\n");
            fprintf(ctx->output, "    store%s 0, %%slot%u\n", type_suffix, decl->slot_index);
        }
    }
}

/* Emit if statement with else clause. */
void _emit_if(CodegenContext* ctx, AstIf* if_stmt, BlockInfo* info)
{
    GenValue cond = _emit_expr(ctx, if_stmt->condition);
    const char* then_label = _alloc_label(ctx);
    const char* else_label = _alloc_label(ctx);
    const char* end_label = _alloc_label(ctx);

    bool then_returns = _block_returns(if_stmt->then_block);
    bool else_returns = if_stmt->else_stmt == nullptr ? false : _stmt_returns(if_stmt->else_stmt);

    fprintf(ctx->output, "    # if branch\n");
    if (then_returns && else_returns) {
        fprintf(ctx->output, "    jnz %s, @%s, @%s\n", cond.qbe_temp, then_label, else_label);
        fprintf(ctx->output, "\n  # then branch\n");
        fprintf(ctx->output, "  @%s\n", then_label);
        BlockInfo then_info = *info;
        then_info.true_label = then_label;
        then_info.false_label = else_label;
        _emit_block(ctx, if_stmt->then_block, &then_info);
        fprintf(ctx->output, "\n  # else branch\n");
        fprintf(ctx->output, "  @%s\n", else_label);
        _emit_stmt(ctx, if_stmt->else_stmt, info);
    } else if (then_returns) {
        fprintf(ctx->output, "    jnz %s, @%s, @%s\n", cond.qbe_temp, then_label, else_label);
        fprintf(ctx->output, "  # then branch\n");
        fprintf(ctx->output, "  @%s\n", then_label);
        BlockInfo then_info = *info;
        then_info.true_label = then_label;
        then_info.false_label = else_label;
        _emit_block(ctx, if_stmt->then_block, &then_info);
        fprintf(ctx->output, "  # else branch\n");
        fprintf(ctx->output, "  @%s\n", else_label);
        _emit_stmt(ctx, if_stmt->else_stmt, info);
        if (!else_returns) {
            fprintf(ctx->output, "    jmp @%s\n", end_label);
            fprintf(ctx->output, "\n  # if end\n");
            fprintf(ctx->output, "  @%s\n", end_label);
        }
    } else if (else_returns) {
        fprintf(ctx->output, "    jnz %s, @%s, @%s\n", cond.qbe_temp, then_label, else_label);
        fprintf(ctx->output, "\n  # then branch\n");
        fprintf(ctx->output, "  @%s\n", then_label);
        BlockInfo then_info = *info;
        then_info.true_label = then_label;
        then_info.false_label = else_label;
        _emit_block(ctx, if_stmt->then_block, &then_info);
        if (!_block_returns(if_stmt->then_block)) {
            fprintf(ctx->output, "    jmp @%s\n", end_label);
        }
        fprintf(ctx->output, "\n  # else branch\n");
        fprintf(ctx->output, "  @%s\n", else_label);
        _emit_stmt(ctx, if_stmt->else_stmt, info);
    } else {
        fprintf(ctx->output, "    jnz %s, @%s, @%s\n", cond.qbe_temp, then_label, else_label);
        fprintf(ctx->output, "\n  # then branch\n");
        fprintf(ctx->output, "  @%s\n", then_label);
        BlockInfo then_info = *info;
        then_info.true_label = then_label;
        then_info.false_label = else_label;
        _emit_block(ctx, if_stmt->then_block, &then_info);
        if (!_block_returns(if_stmt->then_block)) {
            fprintf(ctx->output, "    jmp @%s\n", end_label);
        }
        fprintf(ctx->output, "\n  # else branch\n");
        fprintf(ctx->output, "  @%s\n", else_label);
        _emit_stmt(ctx, if_stmt->else_stmt, info);
        if (!else_returns) {
            fprintf(ctx->output, "    jmp @%s\n", end_label);
        }
        fprintf(ctx->output, "\n  # if end\n");
        fprintf(ctx->output, "  @%s\n", end_label);
    }
}

/* Emit while loop. */
void _emit_while(CodegenContext* ctx, AstWhile* while_stmt, BlockInfo* info)
{
    const char* cond_label = _alloc_label(ctx);
    const char* body_label = _alloc_label(ctx);
    const char* end_label = _alloc_label(ctx);

    const char* saved_break = info->break_label;
    const char* saved_continue = info->continue_label;

    info->break_label = end_label;
    info->continue_label = cond_label;

    fprintf(ctx->output, "    # while loop\n");
    fprintf(ctx->output, "    jmp @%s\n", cond_label);

    fprintf(ctx->output, "\n  # condition\n");
    fprintf(ctx->output, "  @%s\n", cond_label);
    GenValue cond = _emit_expr(ctx, while_stmt->condition);
    fprintf(ctx->output, "jnz %s, @%s, @%s\n", cond.qbe_temp, body_label, end_label);

    fprintf(ctx->output, "\n  # loop body\n");
    fprintf(ctx->output, "  @%s\n", body_label);
    _emit_block(ctx, while_stmt->body, info);
    if (!_block_returns(while_stmt->body)) {
        fprintf(ctx->output, "jmp @%s\n", cond_label);
    }

    fprintf(ctx->output, "\n  # loop end\n");
    fprintf(ctx->output, "  @%s\n", end_label);

    info->break_label = saved_break;
    info->continue_label = saved_continue;
}

/* Emit for loop. */
void _emit_for(CodegenContext* ctx, AstFor* for_stmt, BlockInfo* info)
{
    const char* init_label = _alloc_label(ctx);
    const char* cond_label = _alloc_label(ctx);
    const char* body_label = _alloc_label(ctx);
    const char* iter_label = _alloc_label(ctx);
    const char* end_label = _alloc_label(ctx);

    const char* saved_break = info->break_label;
    const char* saved_continue = info->continue_label;

    info->break_label = end_label;
    info->continue_label = iter_label;

    bool has_init = for_stmt->init != nullptr;
    bool has_cond = for_stmt->condition != nullptr;
    bool has_iter = for_stmt->iter != nullptr;
    bool body_returns = for_stmt->body && _block_returns(for_stmt->body);

    fprintf(ctx->output, "    # for loop\n");
    if (has_init) {
        fprintf(ctx->output, "    # init\n");
        fprintf(ctx->output, "    jmp @%s\n", init_label);
        fprintf(ctx->output, "  @%s\n", init_label);
        _emit_stmt(ctx, for_stmt->init, info);
    }

    if (has_cond) {
        if (!has_init) {
            fprintf(ctx->output, "    jmp @%s\n", cond_label);
        }
        fprintf(ctx->output, "\n  # condition\n");
        fprintf(ctx->output, "  @%s\n", cond_label);
        GenValue cond = _emit_expr(ctx, for_stmt->condition);
        fprintf(ctx->output, "    jnz %s, @%s, @%s\n", cond.qbe_temp, body_label, end_label);
    } else if (has_init) {
        fprintf(ctx->output, "    jmp @%s\n", body_label);
    }

    fprintf(ctx->output, "\n  # body\n");
    fprintf(ctx->output, "  @%s\n", body_label);
    _emit_block(ctx, for_stmt->body, info);

    if (!body_returns) {
        if (has_iter) {
            fprintf(ctx->output, "    # iter\n");
            fprintf(ctx->output, "    jmp @%s\n", iter_label);
            fprintf(ctx->output, "  @%s\n", iter_label);
            _emit_expr(ctx, for_stmt->iter);
            fprintf(ctx->output, "    jmp @%s\n", has_cond ? cond_label : body_label);
        } else {
            fprintf(ctx->output, "    jmp @%s\n", has_cond ? cond_label : body_label);
        }
    }

    if (has_cond) {
        fprintf(ctx->output, "\n  # loop end\n");
        fprintf(ctx->output, "  @%s\n", end_label);
    }

    info->break_label = saved_break;
    info->continue_label = saved_continue;
}

/* Emit do-while loop. */
void _emit_do_while(CodegenContext* ctx, AstDoWhile* do_while, BlockInfo* info)
{
    const char* body_label = _alloc_label(ctx);
    const char* cond_label = _alloc_label(ctx);
    const char* end_label = _alloc_label(ctx);

    const char* saved_break = info->break_label;
    const char* saved_continue = info->continue_label;

    info->break_label = end_label;
    info->continue_label = cond_label;

    fprintf(ctx->output, "\n  # do-while loop\n");
    fprintf(ctx->output, "  # body\n");
    fprintf(ctx->output, "  @%s\n", body_label);
    _emit_block(ctx, do_while->body, info);

    if (!_block_returns(do_while->body)) {
        fprintf(ctx->output, "\n  # condition\n");
        fprintf(ctx->output, "  @%s\n", cond_label);
        GenValue cond = _emit_expr(ctx, do_while->condition);
        fprintf(ctx->output, "    jnz %s, @%s, @%s\n", cond.qbe_temp, body_label, end_label);
        fprintf(ctx->output, "\n  # loop end\n");
        fprintf(ctx->output, "  @%s\n", end_label);
    }

    info->break_label = saved_break;
    info->continue_label = saved_continue;
}

/* Emit break statement. */
void _emit_break(CodegenContext* ctx, AstBreak* brk, BlockInfo* info)
{
    (void)brk;
    if (info == nullptr || info->break_label == nullptr) {
        return;
    }
    fprintf(ctx->output, "    # break\n");
    fprintf(ctx->output, "    jmp @%s\n", info->break_label);
}

/* Emit continue statement. */
void _emit_continue(CodegenContext* ctx, AstContinue* cont, BlockInfo* info)
{
    (void)cont;
    if (info == nullptr || info->continue_label == nullptr) {
        return;
    }
    fprintf(ctx->output, "    # continue\n");
    fprintf(ctx->output, "    jmp @%s\n", info->continue_label);
}
