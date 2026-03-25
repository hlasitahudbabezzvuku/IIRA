/**
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast.h"
#include "ii_diagnostics.h"
#include "ii_trace.h"

#include <stdio.h>

struct CodegenContext {
    Ast* ast;
    FILE* output;
    DiagnosticContext* diag;
    TraceContext* trace;
};

