#include "lexer.h"

#include "uf_containers.h"
#include "uf_logger.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static const struct Flag FLAGS[] = {
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
};

static constexpr size_t FLAGS_COUNT = sizeof(FLAGS) / sizeof(FLAGS[0]);

static void print_usage()
{
    puts("Usage: iirac [OPTIONS] <file1> [file2 ...]");
    puts("Options:");

    for (size_t i = 0; i < FLAGS_COUNT; i++) {
        const struct Flag* opt = &FLAGS[i];

        char short_str_buffer[8] = "    ";
        if (opt->short_name != '\0') {
            snprintf(short_str_buffer, sizeof(short_str_buffer), "-%c, ", opt->short_name);
        }

        const char* type_hint = (opt->type == FLAG_CONSUME) ? " <val>" : "";
        char combined_name_buffer[64];
        snprintf(combined_name_buffer, sizeof(combined_name_buffer), "%s%s", opt->long_name, type_hint);

        printf("  %s--%-20s %s\n", short_str_buffer, combined_name_buffer, opt->description);
    }
    putchar('\n');
}

static void print_version()
{
    printf("%s version %s %s\n", PROJECT_NAME, PROJECT_VERSION, __DATE__);
}

static bool parse_arguments(int argc, char* argv[], UfConVector* input_files)
{
    for (int i = 1; i < argc; i++) {
        const char* argument = argv[i];

        /* Is it a flag? */
        if (argument[0] == '-') {
            bool is_long = (argument[1] == '-');
            const char* flag_name = is_long ? &argument[2] : &argument[1];
            bool matched = false;

            for (size_t j = 0; j < FLAGS_COUNT; j++) {
                const struct Flag* flag = &FLAGS[j];

                /* Match long or short name */
                if ((is_long && strcmp(flag_name, flag->long_name) == 0) ||
                    (!is_long && flag_name[0] == flag->short_name && flag_name[1] == '\0')) {

                    matched = true;

                    switch (flag->type) {
                    case FLAG_SET:
                        *(bool*)flag->config_var_ptr = true;
                        break;

                    case FLAG_CONSUME:
                        if (i + 1 < argc) {
                            *(const char**)flag->config_var_ptr = argv[++i];
                        } else {
                            uf_log_err("Option '--%s' requires an argument", flag->long_name);
                            return false;
                        }
                        break;

                    case FLAG_HELP:
                        print_usage();
                        exit(EXIT_SUCCESS);
                        break;

                    case FLAG_VERSION:
                        print_version();
                        exit(EXIT_SUCCESS);
                        break;
                    }
                    break;
                }
            }

            if (!matched) {
                uf_log_panic("Unknown flag '%s'", argument);
                return false;
            }
        } else {
            uf_con_vector_push(input_files, &argument);
        }
    }
    return true;
}

int main(int argc, char* argv[])
{
    _autovector_ UfConVector* input_files = uf_con_vector_new(sizeof(const char*));

    if (!parse_arguments(argc, argv, input_files)) {
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
    return EXIT_SUCCESS;
}
