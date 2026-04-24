/**
 * @brief QBE function generation.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast_iter.h"
#include "ii_lexer.h"
#include "iic_codegen_internal.h"

static void _emit_params(CodegenContext* ctx, UfConVector* params)
{
}

static void _emit_function_signature(CodegenContext* ctx, const char* name, AstType* return_type,
                                     UfConVector* params)
{
}

static void _store_params_to_slots(CodegenContext* ctx, AstFuncDecl* func)
{
}

static void _store_method_params_to_slots(CodegenContext* ctx, AstMethodOverload* overload)
{
}

void _emit_function(CodegenContext* ctx, AstFuncDecl* func)
{
}

void _emit_method(CodegenContext* ctx, AstBlueprintDecl* bp, AstMethod* method)
{
}
