#pragma once

/**
 * @brief Private semantic analyzer types and helpers shared across all submodules.
 *
 * This is private header meant to be shared across parser submodules. This enables us to keep the
 * implementation "private" while not needing crazy getter/setters for every field in SemanticContext struct.
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast.h"
#include "ii_diagnostics.h"
#include "ii_trace.h"

struct SemanticContext {
    Source* src;
    DiagnosticContext* diag;
    TraceContext* trace;
    Ast* ast;

    bool had_error;
};

