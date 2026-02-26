/**
 * @brief This is the main entry point of the whole compiler.
 *
 * TODO: A little introduction into the compiler architecture.
 *
 * @author: Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "arguments.h"
#include "lexer.h"
#include "uf_containers.h"
#include "uf_logger.h"

int main(int argc, char* argv[])
{
    _autovector_ UfConVector* input_files = uf_con_vector_new(sizeof(const char*));

    if (!arg_process(argc, argv, input_files)) {
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
        arg_print_usage();
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < input_files_count; i++) {
        const char* filepath = *(const char**)uf_con_vector_get(input_files, i);
        uf_log_info("Starting: %s", filepath);

        uf_log_debug("Lexing...");

        _autolexer_ LexerContext* context = lexer_context_new(filepath);
        if (!context) {
            uf_log_err("Could not open or process file '%s'", filepath);
            continue;
        }

        if (config.show_lexer_output) {
            lexer_print_debug(context);
        }

        uf_log_info("Finished: %s", filepath);
    }

    return EXIT_SUCCESS;
}
