#pragma once

/**
 * @brief QBE Graph Builder and IL Emitter modules.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast.h"
#include "ii_diagnostics.h"
#include "ii_trace.h"

#include <stdio.h>

typedef struct CodegenContext CodegenContext;

CodegenContext* iic_gen_context_new(Ast* ast, FILE* output, DiagnosticContext* diag, TraceContext*);
void iic_gen_context_free(DiagnosticContext*);
void iic_gen_context_freep(DiagnosticContext**);

#define _autogen_ _cleanup_(iic_gen_context_freep)
