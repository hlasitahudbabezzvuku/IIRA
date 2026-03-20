#pragma once

/**
 * @brief Semantic analyzer core module.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast.h"
#include "ii_diagnostics.h"
#include "ii_trace.h"

typedef struct SemanticContext SemanticContext;

SemanticContext* ii_sem_context_new(Ast* ast, DiagnosticContext* diag, TraceContext*) _nodiscard_;
void ii_sem_context_free(SemanticContext*);
void ii_sem_context_freep(SemanticContext**);

#define _autosem_ _cleanup_(ii_sem_context_freep)
