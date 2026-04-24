/**
 * @brief QBE type definition generation.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast_iter.h"
#include "iic_codegen_internal.h"

#include <stdio.h>
#include <string.h>

void _emit_type_def(CodegenContext* ctx, AstBlueprintDecl* bp)
{
    if (bp->flat_fields == nullptr) {
        return;
    }

    fprintf(ctx->output, "type :%s = { ", bp->name);

    bool first = true;
    ast_foreach_flat_fields(bp, field)
    {
        if (!first) {
            fprintf(ctx->output, ", ");
        }
        first = false;

        const char* suffix = _get_type_suffix(field->type);
        fprintf(ctx->output, "%s", suffix);
    }
    ast_foreach_end;

    fprintf(ctx->output, " }\n");
}

void _emit_anon_type_def(CodegenContext* ctx, AstType* type)
{
    if (type->tag != AST_TYPE_KIND_ANON) {
        return;
    }

    fprintf(ctx->output, "# anonymous blueprint: %s\n", type->variant.anon.auto_name);
    fprintf(ctx->output, "type :%s = { ", type->variant.anon.auto_name);

    bool first = true;
    for (size_t i = 0; i < type->variant.anon.field_count; i++) {
        AstField* field = &type->variant.anon.fields[i];
        if (!first) {
            fprintf(ctx->output, ", ");
        }
        first = false;

        const char* suffix = _get_type_suffix(field->type);
        fprintf(ctx->output, "%s", suffix);
    }

    fprintf(ctx->output, " }\n");
}
