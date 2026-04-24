/**
 * @brief QBE expression generation.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast_iter.h"
#include "iic_codegen_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Dispatch to appropriate expression handler based on expr->base.kind. */
GenValue _emit_expr(CodegenContext* ctx, AstExpr* expr)
{
    if (expr == nullptr) {
        GenValue dummy = {
            .qbe_temp = "0", .lvalue_addr = nullptr, .is_lvalue = false, .type_suffix = nullptr};
        return dummy;
    }

    switch (ii_ast_expr_get_kind(expr)) {
    case AST_KIND_LITERAL:
        return _emit_literal(ctx, (AstLiteral*)expr);
    case AST_KIND_IDENT:
        return _emit_ident(ctx, (AstIdent*)expr);
    case AST_KIND_BINARY:
        return _emit_binary(ctx, (AstBinary*)expr);
    case AST_KIND_UNARY:
        return _emit_unary(ctx, (AstUnary*)expr);
    case AST_KIND_CALL:
        return _emit_call(ctx, (AstCall*)expr);
    case AST_KIND_MEMBER:
        return _emit_member(ctx, (AstMember*)expr);
    case AST_KIND_INDEX:
        return _emit_index(ctx, (AstIndex*)expr);
    case AST_KIND_INIT:
        return _emit_init(ctx, (AstInit*)expr);
    case AST_KIND_CAST:
        return _emit_cast(ctx, (AstCast*)expr);
    case AST_KIND_FFI:
        return _emit_ffi(ctx, (AstFfi*)expr);
    default: {
        GenValue dummy = {
            .qbe_temp = "0", .lvalue_addr = nullptr, .is_lvalue = false, .type_suffix = nullptr};
        return dummy;
    }
    }
}

/* Emit literal expression (int, float, string, bool, char, null). */
GenValue _emit_literal(CodegenContext* ctx, AstLiteral* lit)
{
    if (lit == nullptr) {
        GenValue dummy = {.qbe_temp = "0",
                          .lvalue_addr = nullptr,
                          .is_lvalue = false,
                          .type_suffix = nullptr,
                          .name = nullptr};
        return dummy;
    }

    GenValue result = {
        .qbe_temp = "0", .lvalue_addr = nullptr, .is_lvalue = false, .type_suffix = "w", .name = nullptr};

    switch (lit->variant) {
    case LITERAL_INT: {
        const char* tmp = _alloc_temp(ctx);
        fprintf(ctx->output, "    %s =w copy %ld\n", tmp, lit->literal.int_value);
        result.qbe_temp = tmp;
        result.type_suffix = "w";
        break;
    }
    case LITERAL_FLOAT: {
        const char* tmp = _alloc_temp(ctx);
        fprintf(ctx->output, "    %s =s copy s_%g\n", tmp, lit->literal.float_value);
        result.qbe_temp = tmp;
        result.type_suffix = "s";
        break;
    }
    case LITERAL_STRING: {
        const char* label = _get_string_label(ctx, lit->literal.string_value);
        const char* tmp = _alloc_temp(ctx);
        fprintf(ctx->output, "    %s =l copy %s\n", tmp, label);
        result.qbe_temp = tmp;
        result.type_suffix = "l";
        break;
    }
    case LITERAL_BOOL: {
        const char* tmp = _alloc_temp(ctx);
        fprintf(ctx->output, "    %s =w copy %d\n", tmp, lit->literal.bool_value ? 1 : 0);
        result.qbe_temp = tmp;
        result.type_suffix = "w";
        break;
    }
    case LITERAL_CHAR: {
        const char* tmp = _alloc_temp(ctx);
        fprintf(ctx->output, "    %s =w copy %d\n", tmp, (int)lit->literal.char_value);
        result.qbe_temp = tmp;
        result.type_suffix = "w";
        break;
    }
    case LITERAL_NULL: {
        const char* tmp = _alloc_temp(ctx);
        fprintf(ctx->output, "    %s =l copy 0\n", tmp);
        result.qbe_temp = tmp;
        result.type_suffix = "l";
        break;
    }
    default: {
        const char* tmp = _alloc_temp(ctx);
        fprintf(ctx->output, "    %s =w copy 0\n", tmp);
        result.qbe_temp = tmp;
        result.type_suffix = "w";
        break;
    }
    }

    return result;
}

/* Emit identifier reference (variable, parameter, field access). */
GenValue _emit_ident(CodegenContext* ctx, AstIdent* ident)
{
    if (ident == nullptr) {
        GenValue dummy = {.qbe_temp = "0",
                          .lvalue_addr = nullptr,
                          .is_lvalue = false,
                          .type_suffix = nullptr,
                          .name = nullptr};
        return dummy;
    }

    GenValue result = {
        .qbe_temp = "0", .lvalue_addr = nullptr, .is_lvalue = false, .type_suffix = "w", .name = ident->name};

    switch (ident->resolved_kind) {
    case AST_IDENT_PARAM: {
        AstParam* param = ident->resolved.param_decl;
        if (param == nullptr) {
            break;
        }
        fprintf(ctx->output, "    # load param %s\n", param->name);
        result.name = param->name;
        uint32_t offset = _get_slot_offset(param->slot_index);
        const char* type_suffix = _get_type_suffix(param->type);
        const char* tmp = _alloc_temp(ctx);

        fprintf(ctx->output, "    %s =l add %%frame, %u\n", tmp, offset);

        bool is_array_or_pointer_or_struct =
            param->type != nullptr &&
            (param->type->tag == AST_TYPE_KIND_ARRAY || param->type->tag == AST_TYPE_KIND_POINTER ||
             param->type->tag == AST_TYPE_KIND_BLUEPRINT || param->type->tag == AST_TYPE_KIND_ANON);

        if (is_array_or_pointer_or_struct) {
            const char* ptr_tmp = _alloc_temp(ctx);
            fprintf(ctx->output, "    %s =l loadl %s\n", ptr_tmp, tmp);
            result.qbe_temp = ptr_tmp;
            result.lvalue_addr = tmp;
        } else {
            const char* tmp2 = _alloc_temp(ctx);
            if (type_suffix != nullptr && type_suffix[0] == 'b') {
                fprintf(ctx->output, "    %s =w loadsb %s\n", tmp2, tmp);
            } else if (type_suffix != nullptr && type_suffix[0] == 'h') {
                fprintf(ctx->output, "    %s =w loadsh %s\n", tmp2, tmp);
            } else {
                fprintf(ctx->output, "    %s =%s load%s %s\n", tmp2, type_suffix, type_suffix, tmp);
            }
            result.qbe_temp = tmp2;
            result.lvalue_addr = tmp;
        }

        result.is_lvalue = true;
        result.type_suffix = type_suffix;
        break;
    }
    case AST_IDENT_VAR: {
        AstDecl* decl = ident->resolved.var_decl;
        if (decl == nullptr) {
            break;
        }
        fprintf(ctx->output, "    # load local %s\n", decl->name);
        result.name = decl->name;
        uint32_t offset = _get_slot_offset(decl->slot_index);
        const char* type_suffix = _get_type_suffix(decl->type);
        const char* tmp = _alloc_temp(ctx);

        fprintf(ctx->output, "    %s =l add %%frame, %u\n", tmp, offset);

        bool is_pointer = decl->type != nullptr && (decl->type->tag == AST_TYPE_KIND_POINTER);
        bool is_array = decl->type != nullptr && (decl->type->tag == AST_TYPE_KIND_ARRAY);
        bool is_struct = decl->type != nullptr && (decl->type->tag == AST_TYPE_KIND_BLUEPRINT ||
                                                   decl->type->tag == AST_TYPE_KIND_ANON);

        if (is_pointer || is_array) {
            const char* ptr_tmp = _alloc_temp(ctx);
            fprintf(ctx->output, "    %s =l loadl %s\n", ptr_tmp, tmp);
            result.qbe_temp = ptr_tmp;
            result.lvalue_addr = tmp;
        } else if (is_struct) {
            result.qbe_temp = tmp;
            result.lvalue_addr = tmp;
        } else {
            const char* tmp2 = _alloc_temp(ctx);
            if (type_suffix != nullptr && type_suffix[0] == 'b') {
                fprintf(ctx->output, "    %s =w loadsb %s\n", tmp2, tmp);
            } else if (type_suffix != nullptr && type_suffix[0] == 'h') {
                fprintf(ctx->output, "    %s =w loadsh %s\n", tmp2, tmp);
            } else {
                fprintf(ctx->output, "    %s =%s load%s %s\n", tmp2, type_suffix, type_suffix, tmp);
            }
            result.qbe_temp = tmp2;
            result.lvalue_addr = tmp;
        }

        result.is_lvalue = true;
        result.type_suffix = type_suffix;
        break;
    }
    case AST_IDENT_FIELD: {
        AstField* field = ident->resolved.field_decl;
        if (field == nullptr) {
            break;
        }
        fprintf(ctx->output, "    # load field %s (offset %u)\n", field->name, field->offset);
        result.name = field->name;
        uint32_t offset = field->offset;
        const char* type_suffix = _get_type_suffix(field->type);
        const char* tmp = _alloc_temp(ctx);

        fprintf(ctx->output, "    %s =l add %%frame, %u\n", tmp, offset);
        const char* tmp2 = _alloc_temp(ctx);
        if (type_suffix != nullptr && type_suffix[0] == 'b') {
            fprintf(ctx->output, "    %s =w loadsb %s\n", tmp2, tmp);
        } else if (type_suffix != nullptr && type_suffix[0] == 'h') {
            fprintf(ctx->output, "    %s =w loadsh %s\n", tmp2, tmp);
        } else {
            fprintf(ctx->output, "    %s =%s load%s %s\n", tmp2, type_suffix, type_suffix, tmp);
        }

        result.qbe_temp = tmp2;
        result.lvalue_addr = tmp;
        result.is_lvalue = true;
        result.type_suffix = type_suffix;
        break;
    }
    case AST_IDENT_FUNC: {
        AstFuncDecl* func = ident->resolved.func_decl;
        if (func == nullptr) {
            break;
        }
        const char* tmp = _alloc_temp(ctx);
        fprintf(ctx->output, "    # load function %s\n", func->name);
        result.name = func->name;
        fprintf(ctx->output, "    %s =l copy $%s\n", tmp, func->name);
        result.qbe_temp = tmp;
        break;
    }
    case AST_IDENT_SELF: {
        AstMethodOverload* overload = ident->resolved.method_overload;
        if (overload == nullptr) {
            break;
        }
        fprintf(ctx->output, "    # load self\n");
        result.name = "self";
        uint32_t offset = _get_slot_offset(overload->self_slot);
        const char* tmp = _alloc_temp(ctx);
        fprintf(ctx->output, "    %s =l add %%slot%u, %u\n", tmp, overload->self_slot, offset);
        const char* tmp2 = _alloc_temp(ctx);
        fprintf(ctx->output, "    %s =l loadl %s\n", tmp2, tmp);
        result.qbe_temp = tmp2;
        result.lvalue_addr = tmp;
        result.is_lvalue = true;
        break;
    }
    default:
        break;
    }

    return result;
}

static const char* _get_binary_op_name(enum LexerTokenType op)
{
    switch (op) {
    case LEXER_TOK_PLUS:
        return "add";
    case LEXER_TOK_MINUS:
        return "sub";
    case LEXER_TOK_STAR:
        return "mul";
    case LEXER_TOK_SLASH:
        return "div";
    case LEXER_TOK_PERCENT:
        return "rem";
    case LEXER_TOK_BIT_AND:
        return "and";
    case LEXER_TOK_BIT_OR:
        return "or";
    case LEXER_TOK_BIT_XOR:
        return "xor";
    default:
        return "add";
    }
}

static enum LexerTokenType _compound_to_simple_op(enum LexerTokenType op)
{
    switch (op) {
    case LEXER_TOK_PLUS_ASSIGN:
        return LEXER_TOK_PLUS;
    case LEXER_TOK_MINUS_ASSIGN:
        return LEXER_TOK_MINUS;
    case LEXER_TOK_STAR_ASSIGN:
        return LEXER_TOK_STAR;
    case LEXER_TOK_SLASH_ASSIGN:
        return LEXER_TOK_SLASH;
    default:
        return LEXER_TOK_PLUS;
    }
}

static const char* _get_comparison_op_name(enum LexerTokenType op, const char* type_suffix)
{
    bool is_long = (type_suffix != nullptr && type_suffix[0] == 'l');
    bool is_single = (type_suffix != nullptr && type_suffix[0] == 's');
    bool is_double = (type_suffix != nullptr && type_suffix[0] == 'd');

    if (is_single || is_double) {
        switch (op) {
        case LEXER_TOK_LT:
            return is_single ? "clts" : "cltd";
        case LEXER_TOK_GT:
            return is_single ? "cgts" : "cgtd";
        case LEXER_TOK_LTE:
            return is_single ? "cles" : "cled";
        case LEXER_TOK_GTE:
            return is_single ? "cges" : "cged";
        default:
            return is_single ? "ceqs" : "ceqd";
        }
    }

    switch (op) {
    case LEXER_TOK_LT:
        return is_long ? "csltl" : "csltw";
    case LEXER_TOK_GT:
        return is_long ? "csgtl" : "csgtw";
    case LEXER_TOK_LTE:
        return is_long ? "cslel" : "cslew";
    case LEXER_TOK_GTE:
        return is_long ? "csgel" : "csgew";
    default:
        return is_long ? "ceql" : "ceqw";
    }
}

/* Emit binary expression (arithmetic, comparison, logical). */
GenValue _emit_binary(CodegenContext* ctx, AstBinary* bin)
{
    if (bin == nullptr) {
        GenValue dummy = {.qbe_temp = "0",
                          .lvalue_addr = nullptr,
                          .is_lvalue = false,
                          .type_suffix = nullptr,
                          .name = nullptr};
        return dummy;
    }

    GenValue left = _emit_expr(ctx, bin->left);
    GenValue right = _emit_expr(ctx, bin->right);

    const char* type_suffix = "w";
    if (bin->expr.tast != nullptr && bin->expr.tast->resolved_type != nullptr) {
        type_suffix = _get_type_suffix(bin->expr.tast->resolved_type);
    }

    const char* left_name = left.name ? left.name : left.qbe_temp;
    const char* right_name = right.name ? right.name : right.qbe_temp;
    (void)left_name;
    (void)right_name;

    const char* tmp = _alloc_temp(ctx);

    switch (bin->op) {
    case LEXER_TOK_PLUS:
    case LEXER_TOK_MINUS:
    case LEXER_TOK_STAR:
    case LEXER_TOK_SLASH:
    case LEXER_TOK_PERCENT:
    case LEXER_TOK_BIT_AND:
    case LEXER_TOK_BIT_OR:
    case LEXER_TOK_BIT_XOR: {
        const char* op = _get_binary_op_name(bin->op);
        fprintf(ctx->output, "    # compute: %s %s %s\n", left_name, op, right_name);
        fprintf(ctx->output, "    %s =%s %s %s, %s\n", tmp, type_suffix, op, left.qbe_temp, right.qbe_temp);
        break;
    }
    case LEXER_TOK_LT:
    case LEXER_TOK_GT:
    case LEXER_TOK_LTE:
    case LEXER_TOK_GTE: {
        fprintf(ctx->output, "    # compare: %s < %s\n", left_name, right_name);
        const char* left_suffix = left.type_suffix != nullptr ? left.type_suffix : "w";
        const char* right_suffix = right.type_suffix != nullptr ? right.type_suffix : "w";

        const char* conv_left = left.qbe_temp;
        const char* conv_right = right.qbe_temp;
        const char* comp_type = "w";

        if ((left_suffix[0] == 's' || left_suffix[0] == 'd') &&
            (right_suffix[0] == 's' || right_suffix[0] == 'd')) {
            comp_type = left_suffix;
        } else if (left_suffix[0] == 's' || left_suffix[0] == 'd') {
            const char* tmp_l = _alloc_temp(ctx);
            if (right_suffix[0] == 'w') {
                fprintf(ctx->output, "    %s =%s swtof %s\n", tmp_l, left_suffix, right.qbe_temp);
            } else {
                fprintf(ctx->output, "    %s =%s sltof %s\n", tmp_l, left_suffix, right.qbe_temp);
            }
            conv_right = tmp_l;
            comp_type = left_suffix;
        } else if (right_suffix[0] == 's' || right_suffix[0] == 'd') {
            const char* tmp_r = _alloc_temp(ctx);
            if (left_suffix[0] == 'w') {
                fprintf(ctx->output, "    %s =%s swtof %s\n", tmp_r, right_suffix, left.qbe_temp);
            } else {
                fprintf(ctx->output, "    %s =%s sltof %s\n", tmp_r, right_suffix, left.qbe_temp);
            }
            conv_left = tmp_r;
            comp_type = right_suffix;
        }

        const char* op = _get_comparison_op_name(bin->op, comp_type);
        if (comp_type[0] == 's' || comp_type[0] == 'd') {
            const char* conv_tmp = _alloc_temp(ctx);
            fprintf(ctx->output, "    %s =w %s %s, %s\n", conv_tmp, op, conv_left, conv_right);
            fprintf(ctx->output, "    %s =w copy %s\n", tmp, conv_tmp);
        } else {
            fprintf(ctx->output, "    %s =%s %s %s, %s\n", tmp, comp_type, op, conv_left, conv_right);
        }
        break;
    }
    case LEXER_TOK_EQ: {
        fprintf(ctx->output, "    # compare: %s == %s\n", left_name, right_name);
        const char* left_suffix = left.type_suffix != nullptr ? left.type_suffix : "w";
        const char* right_suffix = right.type_suffix != nullptr ? right.type_suffix : "w";

        const char* conv_left = left.qbe_temp;
        const char* conv_right = right.qbe_temp;
        const char* eq_type = "w";

        if ((left_suffix[0] == 's' || left_suffix[0] == 'd') &&
            (right_suffix[0] == 's' || right_suffix[0] == 'd')) {
            eq_type = left_suffix;
        } else if (left_suffix[0] == 's' || left_suffix[0] == 'd') {
            const char* tmp_l = _alloc_temp(ctx);
            if (right_suffix[0] == 'w') {
                fprintf(ctx->output, "    %s =%s swtof %s\n", tmp_l, left_suffix, right.qbe_temp);
            } else {
                fprintf(ctx->output, "    %s =%s sltof %s\n", tmp_l, left_suffix, right.qbe_temp);
            }
            conv_right = tmp_l;
            eq_type = left_suffix;
        } else if (right_suffix[0] == 's' || right_suffix[0] == 'd') {
            const char* tmp_r = _alloc_temp(ctx);
            if (left_suffix[0] == 'w') {
                fprintf(ctx->output, "    %s =%s swtof %s\n", tmp_r, right_suffix, left.qbe_temp);
            } else {
                fprintf(ctx->output, "    %s =%s sltof %s\n", tmp_r, right_suffix, left.qbe_temp);
            }
            conv_left = tmp_r;
            eq_type = right_suffix;
        }

        fprintf(ctx->output, "    %s =w ceq%s %s, %s\n", tmp, eq_type, conv_left, conv_right);
        break;
    }
    case LEXER_TOK_NEQ: {
        fprintf(ctx->output, "    # compare: %s != %s\n", left_name, right_name);
        const char* left_suffix = left.type_suffix != nullptr ? left.type_suffix : "w";
        const char* right_suffix = right.type_suffix != nullptr ? right.type_suffix : "w";

        const char* conv_left = left.qbe_temp;
        const char* conv_right = right.qbe_temp;
        const char* neq_type = "w";

        if ((left_suffix[0] == 's' || left_suffix[0] == 'd') &&
            (right_suffix[0] == 's' || right_suffix[0] == 'd')) {
            neq_type = left_suffix;
        } else if (left_suffix[0] == 's' || left_suffix[0] == 'd') {
            const char* tmp_l = _alloc_temp(ctx);
            if (right_suffix[0] == 'w') {
                fprintf(ctx->output, "    %s =%s swtof %s\n", tmp_l, left_suffix, right.qbe_temp);
            } else {
                fprintf(ctx->output, "    %s =%s sltof %s\n", tmp_l, left_suffix, right.qbe_temp);
            }
            conv_right = tmp_l;
            neq_type = left_suffix;
        } else if (right_suffix[0] == 's' || right_suffix[0] == 'd') {
            const char* tmp_r = _alloc_temp(ctx);
            if (left_suffix[0] == 'w') {
                fprintf(ctx->output, "    %s =%s swtof %s\n", tmp_r, right_suffix, left.qbe_temp);
            } else {
                fprintf(ctx->output, "    %s =%s sltof %s\n", tmp_r, right_suffix, left.qbe_temp);
            }
            conv_left = tmp_r;
            neq_type = right_suffix;
        }

        fprintf(ctx->output, "    %s =w cne%s %s, %s\n", tmp, neq_type, conv_left, conv_right);
        break;
    }
    case LEXER_TOK_AND:
    case LEXER_TOK_OR:
        fprintf(ctx->output, "    # logical: %s %s %s\n", left_name, bin->op == LEXER_TOK_AND ? "and" : "or",
                right_name);
        fprintf(ctx->output, "    %s =w %s %s, %s\n", tmp, bin->op == LEXER_TOK_AND ? "and" : "or",
                left.qbe_temp, right.qbe_temp);
        break;
    case LEXER_TOK_ASSIGN: {
        const char* suffix = left.type_suffix ? left.type_suffix : "w";
        if (left.lvalue_addr == nullptr) {
            fprintf(ctx->output, "    # assign (invalid lvalue)\n");
            fprintf(ctx->output, "    %s =%s copy 0\n", tmp, suffix);
        } else {
            fprintf(ctx->output, "    # assign: %s = %s\n", left_name, right_name);
            fprintf(ctx->output, "    store%s %s, %s\n", suffix, right.qbe_temp, left.lvalue_addr);
            fprintf(ctx->output, "    %s =%s copy %s\n", tmp, suffix, right.qbe_temp);
        }
        break;
    }
    case LEXER_TOK_PLUS_ASSIGN:
    case LEXER_TOK_MINUS_ASSIGN:
    case LEXER_TOK_STAR_ASSIGN:
    case LEXER_TOK_SLASH_ASSIGN: {
        const char* suffix = left.type_suffix ? left.type_suffix : "w";
        if (left.lvalue_addr == nullptr) {
            fprintf(ctx->output, "    # assign (invalid lvalue)\n");
            fprintf(ctx->output, "    %s =%s copy 0\n", tmp, suffix);
        } else {
            const char* op_name = _get_binary_op_name(_compound_to_simple_op(bin->op));
            fprintf(ctx->output, "    # compound assign: %s %s= %s\n", left_name, op_name, right_name);
            const char* current = _alloc_temp(ctx);
            fprintf(ctx->output, "    %s =%s load%s %s\n", current, suffix, suffix, left.lvalue_addr);
            const char* result_val = _alloc_temp(ctx);
            const char* op = _get_binary_op_name(_compound_to_simple_op(bin->op));
            fprintf(ctx->output, "    %s =%s %s %s, %s\n", result_val, suffix, op, current, right.qbe_temp);
            fprintf(ctx->output, "    store%s %s, %s\n", suffix, result_val, left.lvalue_addr);
            fprintf(ctx->output, "    %s =%s copy %s\n", tmp, suffix, result_val);
        }
        break;
    }
    default:
        fprintf(ctx->output, "    %s =%s copy 0\n", tmp, type_suffix);
        break;
    }

    GenValue result = {
        .qbe_temp = tmp, .lvalue_addr = nullptr, .is_lvalue = false, .type_suffix = type_suffix};
    return result;
}

/* Emit unary expression (-, !, ~, +). */
GenValue _emit_unary(CodegenContext* ctx, AstUnary* un)
{
    if (un == nullptr) {
        GenValue dummy = {
            .qbe_temp = "0", .lvalue_addr = nullptr, .is_lvalue = false, .type_suffix = nullptr};
        return dummy;
    }

    GenValue operand = _emit_expr(ctx, un->operand);

    const char* type_suffix = "w";
    if (un->expr.tast != nullptr && un->expr.tast->resolved_type != nullptr) {
        type_suffix = _get_type_suffix(un->expr.tast->resolved_type);
    }

    const char* tmp = _alloc_temp(ctx);
    GenValue result = {.qbe_temp = tmp, .lvalue_addr = nullptr, .is_lvalue = false};

    switch (un->op) {
    case LEXER_TOK_MINUS:
        fprintf(ctx->output, "    # negate\n");
        fprintf(ctx->output, "    %s =%s neg %s\n", tmp, type_suffix, operand.qbe_temp);
        break;
    case LEXER_TOK_NOT: {
        fprintf(ctx->output, "    # logical not\n");
        if (type_suffix != nullptr && type_suffix[0] == 'b') {
            fprintf(ctx->output, "    %s =w ceqw %s, 0\n", tmp, operand.qbe_temp);
        } else {
            fprintf(ctx->output, "    %s =w ceqw %s, 0\n", tmp, operand.qbe_temp);
        }
        break;
    }
    case LEXER_TOK_BIT_NOT:
        fprintf(ctx->output, "    # bitwise not\n");
        fprintf(ctx->output, "    %s =w xor %s, -1\n", tmp, operand.qbe_temp);
        break;
    case LEXER_TOK_PLUS:
        result.qbe_temp = operand.qbe_temp;
        break;
    case LEXER_TOK_STAR:
        fprintf(ctx->output, "    # dereference pointer\n");
        fprintf(ctx->output, "    %s =%s load%s %s\n", tmp, type_suffix, type_suffix, operand.qbe_temp);
        result.lvalue_addr = operand.qbe_temp;
        result.is_lvalue = true;
        break;
    case LEXER_TOK_BIT_AND:
        fprintf(ctx->output, "    # address of\n");
        fprintf(ctx->output, "    %s =l copy %s\n", tmp, operand.lvalue_addr);
        result.lvalue_addr = operand.lvalue_addr;
        result.is_lvalue = false;
        result.type_suffix = "l";
        break;
    default:
        fprintf(ctx->output, "    %s =%s copy 0\n", tmp, type_suffix);
        break;
    }

    return result;
}

/* Emit function/method call expression. */
GenValue _emit_call(CodegenContext* ctx, AstCall* call)
{
    if (call == nullptr) {
        GenValue dummy = {
            .qbe_temp = "0", .lvalue_addr = nullptr, .is_lvalue = false, .type_suffix = nullptr};
        return dummy;
    }

    const char* func_name = nullptr;
    bool is_method = false;

    if (ii_ast_expr_get_kind(call->callee) == AST_KIND_MEMBER) {
        AstMember* member = (AstMember*)call->callee;
        func_name = member->member_name;
        is_method = true;
    } else if (ii_ast_expr_get_kind(call->callee) == AST_KIND_IDENT) {
        AstIdent* ident = (AstIdent*)call->callee;
        if (ident->resolved_kind == AST_IDENT_FUNC && ident->resolved.func_decl != nullptr) {
            func_name = ident->resolved.func_decl->name;
        }
    }

    if (func_name == nullptr) {
        GenValue dummy = {
            .qbe_temp = "0", .lvalue_addr = nullptr, .is_lvalue = false, .type_suffix = nullptr};
        return dummy;
    }

    const char* ret_suffix = nullptr;
    if (call->expr.tast != nullptr && call->expr.tast->resolved_type != nullptr) {
        ret_suffix = _get_type_suffix(call->expr.tast->resolved_type);
    }

    const char* ret_tmp = nullptr;
    if (ret_suffix != nullptr) {
        ret_tmp = _alloc_temp(ctx);
    }

    size_t max_args = 32;
    const char** arg_temps = uf_mem_region_malloc(ctx->arena, max_args * sizeof(const char*));
    const char** arg_suffixes = uf_mem_region_malloc(ctx->arena, max_args * sizeof(const char*));
    size_t arg_count = 0;

    if (is_method) {
        AstMember* member = (AstMember*)call->callee;
        GenValue obj = _emit_expr(ctx, member->object);
        arg_temps[arg_count] = obj.qbe_temp;
        arg_suffixes[arg_count] = "l";
        arg_count++;
    }

    if (call->args != nullptr) {
        ast_foreach_call_args(call, arg)
        {
            GenValue arg_val = _emit_expr(ctx, arg);
            arg_temps[arg_count] = arg_val.qbe_temp;
            const char* arg_suffix = "w";
            if (arg->tast != nullptr && arg->tast->resolved_type != nullptr) {
                arg_suffix = _get_type_suffix(arg->tast->resolved_type);
            }
            arg_suffixes[arg_count] = arg_suffix;
            arg_count++;
        }
        ast_foreach_end;
    }

    if (ret_tmp != nullptr) {
        fprintf(ctx->output, "    # call %s\n", func_name);
        fprintf(ctx->output, "    %s =%s call $%s(", ret_tmp, ret_suffix, func_name);
    } else {
        fprintf(ctx->output, "    # call %s\n", func_name);
        fprintf(ctx->output, "    call $%s(", func_name);
    }

    for (size_t i = 0; i < arg_count; i++) {
        if (i > 0) {
            fprintf(ctx->output, ", ");
        }
        fprintf(ctx->output, "%s %s", arg_suffixes[i], arg_temps[i]);
    }

    fprintf(ctx->output, ")\n");

    GenValue result = {.qbe_temp = ret_tmp ? ret_tmp : "0", .lvalue_addr = nullptr, .is_lvalue = false};
    return result;
}

/* Emit member access (object.member) or method call. */
GenValue _emit_member(CodegenContext* ctx, AstMember* member)
{
    if (member == nullptr) {
        GenValue dummy = {
            .qbe_temp = "0", .lvalue_addr = nullptr, .is_lvalue = false, .type_suffix = nullptr};
        return dummy;
    }

    if (member->is_method_call) {
        return _emit_call(ctx, (AstCall*)member);
    }

    GenValue obj = _emit_expr(ctx, member->object);

    if (member->resolved_field == nullptr) {
        GenValue dummy = {
            .qbe_temp = "0", .lvalue_addr = nullptr, .is_lvalue = false, .type_suffix = nullptr};
        return dummy;
    }

    AstField* field = member->resolved_field;
    uint32_t offset = field->offset;
    const char* type_suffix = _get_type_suffix(field->type);

    fprintf(ctx->output, "    # load field '%s' (offset %u)\n", member->member_name, offset);
    const char* tmp = _alloc_temp(ctx);
    const char* base = obj.qbe_temp ? obj.qbe_temp : "0";
    fprintf(ctx->output, "    %s =l add %s, %u\n", tmp, base, offset);

    const char* tmp2 = _alloc_temp(ctx);
    if (type_suffix != nullptr && type_suffix[0] == 'b') {
        fprintf(ctx->output, "    %s =w loadsb %s\n", tmp2, tmp);
    } else if (type_suffix != nullptr && type_suffix[0] == 'h') {
        fprintf(ctx->output, "    %s =w loadsh %s\n", tmp2, tmp);
    } else {
        fprintf(ctx->output, "    %s =%s load%s %s\n", tmp2, type_suffix, type_suffix, tmp);
    }

    GenValue result = {.qbe_temp = tmp2, .lvalue_addr = tmp, .is_lvalue = true, .type_suffix = type_suffix};
    return result;
}

/* Emit array subscript expression (array[index]). */
GenValue _emit_index(CodegenContext* ctx, AstIndex* idx)
{
    if (idx == nullptr) {
        GenValue dummy = {
            .qbe_temp = "0", .lvalue_addr = nullptr, .is_lvalue = false, .type_suffix = nullptr};
        return dummy;
    }

    GenValue arr = _emit_expr(ctx, idx->array);
    GenValue idx_val = _emit_expr(ctx, idx->index);

    const char* elem_type_suffix = "w";
    uint32_t elem_size = 4;

    if (idx->expr.tast != nullptr && idx->expr.tast->resolved_type != nullptr) {
        elem_type_suffix = _get_type_suffix(idx->expr.tast->resolved_type);
    }
    if (idx->expr.tast != nullptr && idx->expr.tast->resolved_type != nullptr &&
        idx->expr.tast->resolved_type->tast != nullptr) {
        elem_size = idx->expr.tast->resolved_type->tast->size;
    }

    const char* tmp = _alloc_temp(ctx);
    if (idx->index != nullptr && idx->index->tast != nullptr && idx->index->tast->resolved_type != nullptr) {
        const char* index_suffix = _get_type_suffix(idx->index->tast->resolved_type);
        if (index_suffix != nullptr && index_suffix[0] == 'w') {
            fprintf(ctx->output, "    %s =l extsw %s\n", tmp, idx_val.qbe_temp);
        } else {
            fprintf(ctx->output, "    %s =l copy %s\n", tmp, idx_val.qbe_temp);
        }
    } else {
        fprintf(ctx->output, "    %s =l copy %s\n", tmp, idx_val.qbe_temp);
    }

    const char* mul_tmp = _alloc_temp(ctx);
    fprintf(ctx->output, "    # compute element offset (index * %u bytes)\n", elem_size);
    fprintf(ctx->output, "    %s =l mul %s, %u\n", mul_tmp, tmp, elem_size);

    const char* tmp2 = _alloc_temp(ctx);
    const char* arr_base = arr.qbe_temp ? arr.qbe_temp : "0";
    fprintf(ctx->output, "    %s =l add %s, %s\n", tmp2, arr_base, mul_tmp);

    fprintf(ctx->output, "    # load element from array\n");
    const char* tmp3 = _alloc_temp(ctx);
    if (elem_type_suffix != nullptr && elem_type_suffix[0] == 'b') {
        fprintf(ctx->output, "    %s =w loadsb %s\n", tmp3, tmp2);
    } else if (elem_type_suffix != nullptr && elem_type_suffix[0] == 'h') {
        fprintf(ctx->output, "    %s =w loadsh %s\n", tmp3, tmp2);
    } else {
        fprintf(ctx->output, "    %s =%s load%s %s\n", tmp3, elem_type_suffix, elem_type_suffix, tmp2);
    }

    GenValue result = {
        .qbe_temp = tmp3, .lvalue_addr = tmp2, .is_lvalue = true, .type_suffix = elem_type_suffix};
    return result;
}

/* Emit initialization expression ({ x = 1, y = 2 } or { 1, 2 }). */
GenValue _emit_init(CodegenContext* ctx, AstInit* init)
{
    if (init == nullptr) {
        GenValue dummy = {
            .qbe_temp = "0", .lvalue_addr = nullptr, .is_lvalue = false, .type_suffix = nullptr};
        return dummy;
    }

    AstType* init_type = init->target_type;
    if (init_type == nullptr && init->expr.tast != nullptr) {
        init_type = init->expr.tast->resolved_type;
    }

    uint32_t result_size = 8;
    uint32_t elem_size = 4;
    if (init_type != nullptr && init_type->tast != nullptr) {
        result_size = init_type->tast->size;
    }
    if (init_type != nullptr && init_type->tag == AST_TYPE_KIND_ARRAY) {
        AstType* elem = ii_ast_type_get_array_element(init_type);
        if (elem != nullptr && elem->tast != nullptr) {
            elem_size = elem->tast->size;
        }
    }

    const char* tmp = _alloc_temp(ctx);
    fprintf(ctx->output, "    # initialize aggregate (%u bytes)\n", result_size);
    fprintf(ctx->output, "    %s =l alloc8 %u\n", tmp, result_size);

    if (init->values != nullptr && uf_con_vector_length(init->values) > 0) {
        size_t i = 0;
        ast_foreach_init_values(init, val)
        {
            GenValue elem_val = _emit_expr(ctx, val);
            const char* type_suffix = "w";

            if (val->tast != nullptr && val->tast->resolved_type != nullptr) {
                type_suffix = _get_type_suffix(val->tast->resolved_type);
            } else if (init_type != nullptr && init_type->tag == AST_TYPE_KIND_ARRAY) {
                AstType* elem = ii_ast_type_get_array_element(init_type);
                if (elem != nullptr) {
                    type_suffix = _get_type_suffix(elem);
                }
            }

            const char* offset_tmp = _alloc_temp(ctx);
            fprintf(ctx->output, "    %s =l mul %zu, %u\n", offset_tmp, i, elem_size);

            const char* ptr_tmp = _alloc_temp(ctx);
            fprintf(ctx->output, "    %s =l add %s, %s\n", ptr_tmp, tmp, offset_tmp);

            fprintf(ctx->output, "    store%s %s, %s\n", type_suffix, elem_val.qbe_temp, ptr_tmp);
            i++;
        }
        ast_foreach_end;
    }

    GenValue result = {.qbe_temp = tmp, .lvalue_addr = tmp, .is_lvalue = true};
    return result;
}

/* Emit type cast expression. */
GenValue _emit_cast(CodegenContext* ctx, AstCast* cast)
{
    if (cast == nullptr) {
        GenValue dummy = {
            .qbe_temp = "0", .lvalue_addr = nullptr, .is_lvalue = false, .type_suffix = nullptr};
        return dummy;
    }

    GenValue src = _emit_expr(ctx, cast->expr_);

    const char* src_suffix = "w";
    const char* tgt_suffix = "w";

    if (cast->expr_->tast != nullptr && cast->expr_->tast->resolved_type != nullptr) {
        src_suffix = _get_type_suffix(cast->expr_->tast->resolved_type);
    }
    tgt_suffix = _get_type_suffix(cast->target_type);

    const char* tmp = _alloc_temp(ctx);

    fprintf(ctx->output, "    # cast %s to %s\n", src_suffix, tgt_suffix);
    if (src_suffix[0] == 'l' && tgt_suffix[0] == 's') {
        fprintf(ctx->output, "    %s =%s sltof %s\n", tmp, tgt_suffix, src.qbe_temp);
    } else if (src_suffix[0] == 'l' && tgt_suffix[0] == 'd') {
        fprintf(ctx->output, "    %s =%s sltof %s\n", tmp, tgt_suffix, src.qbe_temp);
    } else if (src_suffix[0] == 'w' && tgt_suffix[0] == 's') {
        fprintf(ctx->output, "    %s =%s swtof %s\n", tmp, tgt_suffix, src.qbe_temp);
    } else if (src_suffix[0] == 'w' && tgt_suffix[0] == 'd') {
        fprintf(ctx->output, "    %s =%s swtof %s\n", tmp, tgt_suffix, src.qbe_temp);
    } else if (src_suffix[0] == 's' && (tgt_suffix[0] == 'w' || tgt_suffix[0] == 'l')) {
        fprintf(ctx->output, "    %s =%s stosi %s\n", tmp, tgt_suffix, src.qbe_temp);
    } else if (src_suffix[0] == 'd' && (tgt_suffix[0] == 'w' || tgt_suffix[0] == 'l')) {
        fprintf(ctx->output, "    %s =%s dtosi %s\n", tmp, tgt_suffix, src.qbe_temp);
    } else if (src_suffix[0] == 'w' && tgt_suffix[0] == 'l') {
        fprintf(ctx->output, "    %s =%s extsw %s\n", tmp, tgt_suffix, src.qbe_temp);
    } else {
        fprintf(ctx->output, "    %s =%s copy %s\n", tmp, tgt_suffix, src.qbe_temp);
    }

    GenValue result = {.qbe_temp = tmp, .lvalue_addr = nullptr, .is_lvalue = false};
    return result;
}

/* Emit FFI call ($function(args)). */
GenValue _emit_ffi(CodegenContext* ctx, AstFfi* ffi)
{
    if (ffi == nullptr) {
        GenValue dummy = {
            .qbe_temp = "0", .lvalue_addr = nullptr, .is_lvalue = false, .type_suffix = nullptr};
        return dummy;
    }

    const char* func_name = ffi->function_name;
    if (func_name != nullptr && func_name[0] == '$') {
        func_name++;
    }

    const char* ret_suffix = "w";
    if (ffi->expr.tast != nullptr && ffi->expr.tast->resolved_type != nullptr) {
        ret_suffix = _get_type_suffix(ffi->expr.tast->resolved_type);
    }

    size_t max_args = 32;
    const char** arg_temps = uf_mem_region_malloc(ctx->arena, max_args * sizeof(const char*));
    const char** arg_suffixes = uf_mem_region_malloc(ctx->arena, max_args * sizeof(const char*));
    size_t arg_count = 0;

    if (ffi->args != nullptr) {
        ast_foreach_ffi_args(ffi, arg)
        {
            GenValue arg_val = _emit_expr(ctx, arg);
            const char* arg_suffix = "w";
            if (arg->tast != nullptr && arg->tast->resolved_type != nullptr) {
                arg_suffix = _get_type_suffix(arg->tast->resolved_type);
            }
            arg_temps[arg_count] = arg_val.qbe_temp;
            arg_suffixes[arg_count] = arg_suffix;
            arg_count++;
        }
        ast_foreach_end;
    }

    const char* tmp = _alloc_temp(ctx);
    fprintf(ctx->output, "    # FFI call: $%s\n", func_name);
    fprintf(ctx->output, "    %s =%s call $%s(", tmp, ret_suffix, func_name);

    for (size_t i = 0; i < arg_count; i++) {
        if (i > 0) {
            fprintf(ctx->output, ", ");
        }
        fprintf(ctx->output, "%s %s", arg_suffixes[i], arg_temps[i]);
    }

    fprintf(ctx->output, ")\n");

    GenValue result = {.qbe_temp = tmp, .lvalue_addr = nullptr, .is_lvalue = false};
    return result;
}

/*
 * Helper function implementations.
 */

/* Get QBE type suffix from AST type. */
const char* _get_type_suffix(AstType* type)
{
    if (type == nullptr) {
        return nullptr;
    }

    if (type->tast != nullptr && type->tast->c_repr != nullptr) {
        return type->tast->c_repr;
    }

    switch (type->tag) {
    case AST_TYPE_KIND_PRIMITIVE: {
        enum LexerPrimitiveType prim = ii_ast_type_get_primitive(type);
        switch (prim) {
        case LEXER_PRIM_INT:
        case LEXER_PRIM_BOOL:
        case LEXER_PRIM_CHAR:
            return "w";
        case LEXER_PRIM_LONG:
            return "l";
        case LEXER_PRIM_SHORT:
            return "h";
        case LEXER_PRIM_FLOAT:
            return "s";
        case LEXER_PRIM_DOUBLE:
            return "d";
        case LEXER_PRIM_VOID:
            return nullptr;
        default:
            return "w";
        }
    }
    case AST_TYPE_KIND_POINTER:
    case AST_TYPE_KIND_BLUEPRINT:
    case AST_TYPE_KIND_ANON:
        return "l";
    case AST_TYPE_KIND_ARRAY:
        return "l";
    default:
        return "w";
    }
}

/* Allocate a new QBE temporary and return its name. */
const char* _alloc_temp(CodegenContext* ctx)
{
    char* buf = uf_mem_region_malloc(ctx->arena, 32);
    snprintf(buf, 32, "%%tmp_%u", ctx->temp_counter);
    ctx->temp_counter++;
    return buf;
}

/* Allocate a new QBE label and return its name. */
const char* _alloc_label(CodegenContext* ctx)
{
    char* buf = uf_mem_region_malloc(ctx->arena, 32);
    snprintf(buf, 32, "block_%u", ctx->label_counter);
    ctx->label_counter++;
    return buf;
}

/* Get or create QBE label for string literal. */
const char* _get_string_label(CodegenContext* ctx, const char* str)
{
    if (ctx->string_literals == nullptr) {
        ctx->string_literals = uf_mem_region_malloc(ctx->arena, 32 * sizeof(const char*));
    }

    char* buf = uf_mem_region_malloc(ctx->arena, 32);
    snprintf(buf, 32, "$str%zu", ctx->string_count);

    ctx->string_literals[ctx->string_count] = str;
    ctx->string_count++;

    return buf;
}

/* Calculate stack slot offset in bytes. */
uint32_t _get_slot_offset(uint32_t slot_index)
{
    return slot_index * 8;
}
