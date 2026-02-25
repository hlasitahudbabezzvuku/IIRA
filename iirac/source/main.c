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
    return EXIT_SUCCESS;
}
