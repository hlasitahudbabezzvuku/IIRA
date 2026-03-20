/**
 * @brief This is the main entry point of the whole compiler.
 *
 * TODO: A little introduction into the compiler architecture.
 *
 * @author: Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_ast.h"
#include "ii_diagnostics.h"
#include "ii_lexer.h"
#include "ii_parser.h"
#include "ii_source.h"
#include "ii_trace.h"
#include "iic_arguments.h"
#include "uf_containers.h"
#include "uf_logger.h"

#include <stdio.h>
#include <stdlib.h>

int main(const int argc, const char* argv[])
{
    CompilerConfig config = {};
    _autovector_ UfConVector* input_files = uf_con_vector_new(sizeof(const char*));

    if (!iic_arg_process(&config, argc, argv, input_files)) {
        return EXIT_FAILURE;
    }

    if (config.no_warn) {
        uf_log_set_level(UF_LOG_ERROR);
    }

    if (config.verbose) {
        uf_log_set_level(UF_LOG_INFO);
    }

    if (config.debug) {
        uf_log_set_level(UF_LOG_DEBUG);
    }

    size_t input_files_count = uf_con_vector_length(input_files);
    if (!input_files_count) {
        uf_log_err("No input files provided");
        iic_arg_print_usage();
        return EXIT_FAILURE;
    }

    bool ret = EXIT_SUCCESS;

    for (size_t i = 0; i < input_files_count; i++) {
        const char* file_path = *(const char**)uf_con_vector_get(input_files, i);
        uf_log_info("Starting compilation: %s", file_path);

        uf_log_debug("Creating Source for file '%s'", file_path);
        _autosrc_ Source* source = ii_src_new(file_path);
        if (!source) {
            uf_log_err("Could not open file '%s'", file_path);
            ret = EXIT_FAILURE;
            continue;
        }

        uf_log_debug("Creating DiagnosticContext for file '%s'", file_path);
        _autodiag_ DiagnosticContext* diagnostic_context = ii_diag_context_new(source);

        uf_log_debug("Creating LexerContext for file '%s'", file_path);
        _autolexer_ LexerContext* lexer_context = ii_lexer_context_new(source, diagnostic_context);
        if (!lexer_context) {
            uf_log_err("Lexer could not process file '%s'", file_path);
            ret = EXIT_FAILURE;
            continue;
        }

        if (config.show_lexer_output) {
            ii_lexer_print_debug(lexer_context);
        }

        if (config.stop_after_lexer) {
            uf_log_info("Stopping after lexical analysis");
            continue;
        }

        uf_log_debug("Creating Ast for file '%s'", file_path);
        _autoast_ Ast* ast = ii_ast_new(source);

        uf_log_debug("Creating TraceContext for file '%s'", file_path);
        _autotrace_ TraceContext* trace_context = ii_trace_context_new(stdout);

        if (config.trace) {
            ii_trace_set_enabled(trace_context, true);
        }

        uf_log_debug("Creating ParserContext for file '%s'", file_path);
        _autoparser_ ParserContext* parser_context =
            ii_parser_context_new(ast, ii_lexer_get_tokens(lexer_context),
                                  ii_lexer_get_token_count(lexer_context), diagnostic_context, trace_context);

        if (config.show_parser_output) {
            ii_ast_print_debug(ast);
        }

        if (config.stop_after_parser) {
            uf_log_info("Stopping after parsing");
            continue;
        }

        ii_diag_output(diagnostic_context);

        if (ii_diag_get_count(diagnostic_context, UF_LOG_ERROR) > 0) {
            uf_log_info("Compilation failed due to errors: %s", file_path);
            ret = EXIT_FAILURE;
            continue;
        }

        uf_log_info("Compilation finished: %s", file_path);
    }

    return ret;
}
