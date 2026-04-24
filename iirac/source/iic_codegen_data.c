/**
 * @brief QBE data section generation (string literals).
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "iic_codegen_internal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

void _emit_data_section(CodegenContext* ctx)
{
    if (ctx->string_literals == nullptr || ctx->string_count == 0) {
        return;
    }

    for (size_t i = 0; i < ctx->string_count; i++) {
        const char* str = ctx->string_literals[i];
        if (str == nullptr) {
            continue;
        }
        fprintf(ctx->output, "data $str%zu = { b \"", i);
        fprintf(ctx->output, "%.*s", (int32_t)strlen(str) - 2, str + 1);
        fprintf(ctx->output, "\", b 0 }\n");
    }
}
