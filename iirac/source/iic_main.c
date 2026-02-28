/**
 * @brief This is the main entry point of the whole compiler.
 *
 * TODO: A little introduction into the compiler architecture.
 *
 * @author: Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "ii_lexer.h"
#include "ii_source_manager.h"
#include "iic_arguments.h"
#include "uf_containers.h"
#include "uf_logger.h"

#include <stdio.h>

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

    for (size_t i = 0; i < input_files_count; i++) {
        const char* file_path = *(const char**)uf_con_vector_get(input_files, i);
        uf_log_info("Starting compilation: %s", file_path);

        uf_log_debug("Creating SourceManager for file '%s'", file_path);
        _autosrc_ SourceManager* source_manager = ii_src_manager_new(file_path);
        if (!source_manager) {
            uf_log_err("Could not open file '%s'", file_path);
            continue;
        }

        uf_log_debug("Creating LexerContext for file '%s'", file_path);
        _autolexer_ LexerContext* lexer_context = ii_lexer_context_new(source_manager);
        if (!lexer_context) {
            uf_log_err("Lexer could not process file '%s'", file_path);
            continue;
        }

        if (config.show_lexer_output) {
            ii_lexer_print_debug(lexer_context);
        }

        uf_log_info("Compilation finished: %s", file_path);
    }

    return EXIT_SUCCESS;
}
