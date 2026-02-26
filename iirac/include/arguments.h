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

typedef struct Flag Flag;
struct Flag {
    enum FlagType type;
    const char* long_name;
    char short_name;
    const char* description;
    size_t config_field_offset;
};

typedef struct Config Config;
struct Config {
    bool no_warn;
    bool verbose;
    bool debug;
    bool show_lexer_output;
    bool show_parser_output;
};

static const Flag default_flags[] = {
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
        .config_field_offset = offsetof(Config, no_warn),
    },
    {
        .type = FLAG_SET,
        .long_name = "verbose",
        .short_name = 0,
        .description = "Enable more descriptive output (overrides the --no-warn flag)",
        .config_field_offset = offsetof(Config, verbose),
    },
    {
        .type = FLAG_SET,
        .long_name = "debug",
        .short_name = 0,
        .description = "Enable debug output (overrides the --verbrose flag)",
        .config_field_offset = offsetof(Config, debug),
    },
    {
        .type = FLAG_SET,
        .long_name = "show-lexer-output",
        .short_name = 0,
        .description = "Print the generated tokens and spatial data to standard output",
        .config_field_offset = offsetof(Config, show_lexer_output),
    },
    {
        .type = FLAG_SET,
        .long_name = "show-parser-output",
        .short_name = 0,
        .description = "Print the parsed AST to standard output",
        .config_field_offset = offsetof(Config, show_parser_output),
    },
};

static const size_t flags_count = sizeof(default_flags) / sizeof(default_flags[0]);

void arg_print_usage();
void arg_print_version();
bool arg_process(const Config*, const int32_t argc, const char* argv[], UfConVector* input_files);
