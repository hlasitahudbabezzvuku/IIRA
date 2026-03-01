#pragma once

/**
 * @brief A simple Diagnostic Engine.
 *
 * Every module that can emit errors (almost all of them) should work with Diagnostic Engine to report all
 * errors. Diagnostic Engine does handle the formatting for us, so we don't have to care about that.
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_source.h"
#include "uf_common.h"
#include "uf_logger.h"

typedef struct DiagnosticContext DiagnosticContext;

/* TODO: Add types of diagnostic (e.g. raw, blame, blame_in_func) */

DiagnosticContext* ii_diag_context_new(Source*) _nodiscard_;
void ii_diag_context_free(DiagnosticContext*);
void ii_diag_context_freep(DiagnosticContext**);
#define _autodiag_ _cleanup_(ii_diag_context_freep)

void ii_diag_report(DiagnosticContext*, enum UfLogLevel level, struct SourceSpan span, const char* format,
                    ...);
void ii_diag_output(DiagnosticContext*);
