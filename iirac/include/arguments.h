#pragma once

/**
 * @brief TODO
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "uf_containers.h"
#include <stdint.h>
#include <stdlib.h>

enum FlagType {
    FLAG_SET,     /* Flag that sets a bool to true */
    FLAG_CONSUME, /* Flag that consumes the next argument */
    FLAG_HELP,    /* Special flag to print help */
    FLAG_VERSION, /* Special flag to print version */
};

struct Flag {
    enum FlagType type;
    const char* long_name;
    char short_name;
    void* config_var_ptr;
    const char* description;
};

struct Config {
    bool no_warn;
    bool verbose;
    bool debug;
    bool show_lexer_output;
    bool show_parser_output;
};

static struct Config config = {};

static const struct Flag default_flags[] = {
    {
        .type = FLAG_HELP,
        .long_name = "help",
        .short_name = 'h',
        .config_var_ptr = nullptr,
        .description = "Print this help message and exit",
    },
    {
        .type = FLAG_VERSION,
        .long_name = "version",
        .short_name = 'v',
        .config_var_ptr = nullptr,
        .description = "Print version information and exit",
    },
    {
        .type = FLAG_SET,
        .long_name = "no-warn",
        .short_name = 's',
        .config_var_ptr = &config.no_warn,
        .description = "Suppress all compilation warnings",
    },
    {
        .type = FLAG_SET,
        .long_name = "verbose",
        .short_name = 0,
        .config_var_ptr = &config.verbose,
        .description = "Enable more descriptive output (overrides the --no-warn flag)",
    },
    {
        .type = FLAG_SET,
        .long_name = "debug",
        .short_name = 0,
        .config_var_ptr = &config.debug,
        .description = "Enable debug output (overrides the --verbrose flag)",
    },
    {
        .type = FLAG_SET,
        .long_name = "show-lexer-output",
        .short_name = 0,
        .config_var_ptr = &config.show_lexer_output,
        .description = "Print the generated tokens and spatial data to standard output",
    },
    {
        .type = FLAG_SET,
        .long_name = "show-parser-output",
        .short_name = 0,
        .config_var_ptr = &config.show_parser_output,
        .description = "Print the parsed AST to standard output",
    },
};

static const size_t flags_count = sizeof(default_flags) / sizeof(default_flags[0]);

void arg_print_usage();
void arg_print_version();
bool arg_process(int32_t argc, char* argv[], UfConVector* input_files);
