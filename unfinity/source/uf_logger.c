/**
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

#include "uf_logger.h"

#include <stdarg.h>
#include <stdio.h>

static enum UfLogLevel _set_level = UF_LOG_WARNING;

static const char* _map_level_x_string[] = {
    "debug", "info", "warning", "error", "panic",
};

static const int _map_level_x_color[] = {
    UF_COLOR_GREEN, UF_COLOR_BLUE, UF_COLOR_YELLOW, UF_COLOR_RED, UF_COLOR_RED,
};

#define _get_output_stream(level) ((level >= UF_LOG_ERROR) ? stdout : stderr)

void _uf_log(enum UfLogLevel level, const char* file, int line, const char* format, ...)
{
    if (level < _set_level) {
        return;
    }

    fprintf(_get_output_stream(level), "[\033[%im%s\033[%im] \033[%im%s:%d:\033[%im ",
            _map_level_x_color[level], _map_level_x_string[level], UF_COLOR_RESET, UF_COLOR_BLACK_LIGHT, file,
            line, UF_COLOR_RESET);

    va_list args;
    va_start(args, format);
    vfprintf(_get_output_stream(level), format, args);
    va_end(args);

    fputc('\n', _get_output_stream(level));
}

void uf_log_set_level(const enum UfLogLevel level)
{
    _set_level = level;
}

enum UfLogLevel uf_log_get_level()
{
    return _set_level;
}
