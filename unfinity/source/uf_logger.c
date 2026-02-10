/*
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 * */

#include "uf_logger.h"

#include <stdarg.h>
#include <stdio.h>

static const char* map_level_x_string[] = {
    "debug", "info", "warning", "error", "panic",
};

static const int map_level_x_color[] = {
    UF_COLOR_GREEN, UF_COLOR_BLUE, UF_COLOR_YELLOW, UF_COLOR_RED, UF_COLOR_RED,
};

#define get_output_stream(level) ((level >= UF_LOG_ERROR) ? stdout : stderr)

void _uf_log(enum UfLogLevel level, const char* file, int line, const char* format, ...)
{
    fprintf(get_output_stream(level), "[\033[%im%s\033[%im] \033[%im%s:%d:\033[%im ",
            map_level_x_color[level], map_level_x_string[level], UF_COLOR_RESET, UF_COLOR_BLACK_LIGHT, file,
            line, UF_COLOR_RESET);

    va_list args;
    va_start(args, format);
    vfprintf(get_output_stream(level), format, args);
    va_end(args);

    fputc('\n', get_output_stream(level));
}
