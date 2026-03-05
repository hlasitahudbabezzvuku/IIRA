#pragma once

/**
 * @brief TODO
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "uf_containers.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

enum FlagType {
    FLAG_SET,     /* Flag that sets a bool to true */
    FLAG_CONSUME, /* Flag that consumes the next argument */
    FLAG_HELP,    /* Special flag to print help */
    FLAG_VERSION, /* Special flag to print version */
};

typedef struct CompilerFlag CompilerFlag;
struct CompilerFlag {
    enum FlagType type;
    const char* long_name;
    char short_name;
    const char* description;
    size_t config_field_offset;
};

typedef struct CompilerConfig CompilerConfig;
struct CompilerConfig {
    bool no_warn;
    bool verbose;
    bool debug;
    const char* output_file;
    const char* max_errors;
    bool show_lexer_output;
    bool show_parser_output;
    bool show_analyzer_output;
};

static const CompilerFlag default_flags[] = {
    {
        .type = FLAG_HELP,
        .long_name = "help",
        .short_name = 'h',
        .description = "Print this help message and exit",
        .config_field_offset = 0,
    },
    {
        .type = FLAG_VERSION,
        .long_name = "version",
        .short_name = 'v',
        .description = "Print version information and exit",
        .config_field_offset = 0,
    },
    {
        .type = FLAG_SET,
        .long_name = "no-warn",
        .short_name = 's',
        .description = "Suppress all compilation warnings",
        .config_field_offset = offsetof(CompilerConfig, no_warn),
    },
    {
        .type = FLAG_SET,
        .long_name = "verbose",
        .short_name = 0,
        .description = "Enable more descriptive output (overrides the --no-warn flag)",
        .config_field_offset = offsetof(CompilerConfig, verbose),
    },
    {
        .type = FLAG_SET,
        .long_name = "debug",
        .short_name = 0,
        .description = "Enable debug output (overrides the --verbrose flag)",
        .config_field_offset = offsetof(CompilerConfig, debug),
    },
    {
        .type = FLAG_CONSUME,
        .long_name = "output",
        .short_name = 'o',
        .description = "Specify the output name (without suffix)",
        .config_field_offset = offsetof(CompilerConfig, output_file),
    },
    {
        .type = FLAG_CONSUME,
        .long_name = "max-errors",
        .short_name = 0,
        .description = "Specify the maximum number of errors before exiting",
        .config_field_offset = offsetof(CompilerConfig, max_errors),
    },
    {
        .type = FLAG_SET,
        .long_name = "show-lexer-output",
        .short_name = 0,
        .description = "Print the generated tokens and spatial data to standard output",
        .config_field_offset = offsetof(CompilerConfig, show_lexer_output),
    },
    {
        .type = FLAG_SET,
        .long_name = "show-parser-output",
        .short_name = 0,
        .description = "Print the parsed AST to standard output",
        .config_field_offset = offsetof(CompilerConfig, show_parser_output),
    },
    {
        .type = FLAG_SET,
        .long_name = "show-analyzer-output",
        .short_name = 0,
        .description = "Print the analyzed TAST to standard output",
        .config_field_offset = offsetof(CompilerConfig, show_analyzer_output),
    },
};

static const size_t flags_count = sizeof(default_flags) / sizeof(default_flags[0]);

void iic_arg_print_usage();
void iic_arg_print_version();
bool iic_arg_process(const CompilerConfig*, const int32_t argc, const char* argv[], UfConVector* input_files);
