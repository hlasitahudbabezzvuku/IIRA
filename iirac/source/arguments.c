/**
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "arguments.h"
#include "uf_logger.h"

#include <stdio.h>
#include <string.h>

void arg_print_usage()
{
    puts("Usage: iirac [OPTIONS] <file1> [file2 ...]");
    puts("Options:");

    for (size_t i = 0; i < flags_count; i++) {
        const Flag* opt = &default_flags[i];

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

void arg_print_version()
{
    printf("%s version %s - %s\n", PROJECT_NAME, PROJECT_VERSION, __DATE__);
}

bool arg_process(const Config* config, const int argc, const char* argv[], UfConVector* input_files)
{
    for (int i = 1; i < argc; i++) {
        const char* argument = argv[i];

        /* Is it a flag? */
        if (argument[0] == '-') {
            bool is_long = (argument[1] == '-');
            const char* flag_name = is_long ? &argument[2] : &argument[1];
            bool matched = false;

            for (size_t j = 0; j < flags_count; j++) {
                const Flag* flag = &default_flags[j];

                /* Match long or short name */
                if ((is_long && strcmp(flag_name, flag->long_name) == 0) ||
                    (!is_long && flag_name[0] == flag->short_name && flag_name[1] == '\0')) {

                    matched = true;
                    void* config_field = (char*)config + flag->config_field_offset;

                    switch (flag->type) {
                    case FLAG_SET:
                        *(bool*)config_field = true;
                        break;

                    case FLAG_CONSUME:
                        if (i + 1 < argc) {
                            *(const char**)config_field = argv[++i];
                        } else {
                            uf_log_err("Option '--%s' requires an argument", flag->long_name);
                            return false;
                        }
                        break;

                    case FLAG_HELP:
                        arg_print_usage();
                        exit(EXIT_SUCCESS);
                        break;

                    case FLAG_VERSION:
                        arg_print_version();
                        exit(EXIT_SUCCESS);
                        break;
                    }
                    break;
                }
            }

            if (!matched) {
                uf_log_err("Unknown flag '%s'", argument);
                return false;
            }
        } else {
            uf_con_vector_push(input_files, &argument);
        }
    }
    return true;
}
