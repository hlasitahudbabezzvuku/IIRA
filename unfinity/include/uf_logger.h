#pragma once

/*
 * @brief A simple logging module designed for printing to terminal.
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 * */

#include "uf_common.h"

#include <stdlib.h> // IWYU pragma: keep

enum UfLogLevel {
    UF_LOG_DEBUG,
    UF_LOG_INFO,
    UF_LOG_WARNING,
    UF_LOG_ERROR,
    UF_LOG_PANIC,
};

enum UfColor {
    UF_COLOR_RESET = 0,
    UF_COLOR_BLACK = 30,
    UF_COLOR_RED = 31,
    UF_COLOR_GREEN = 32,
    UF_COLOR_YELLOW = 33,
    UF_COLOR_BLUE = 34,
    UF_COLOR_MAGENTA = 35,
    UF_COLOR_CYAN = 36,
    UF_COLOR_WHITE = 37,
    UF_COLOR_BLACK_LIGHT = 90,
    UF_COLOR_RED_LIGHT = 91,
    UF_COLOR_GREEN_LIGHT = 92,
    UF_COLOR_YELLOW_LIGHT = 93,
    UF_COLOR_BLUE_LIGHT = 94,
    UF_COLOR_MAGENTA_LIGHT = 95,
    UF_COLOR_CYAN_LIGHT = 96,
    UF_COLOR_WHITE_LIGHT = 97,
};

/*
 * @brief Internal function for printing. Use wrapping macros instead.
 *
 * @param level The level of severity the function should output.
 * @param file The filename from which the log is called.
 * @param line The line on which the log is called.
 * @param format The format string for printf.
 * @param ... Any additional parameters corresponding to the format string.
 * */
void _uf_log(enum UfLogLevel level, const char* file, int line, const char* format, ...)
    __attribute__((format(printf, 4, 5)));

#define uf_log_debug(format, ...) _uf_log(UF_LOG_DEBUG, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define uf_log_info(format, ...) _uf_log(UF_LOG_INFO, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define uf_log_warn(format, ...) _uf_log(UF_LOG_WARNING, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define uf_log_err(format, ...) _uf_log(UF_LOG_ERROR, __FILE__, __LINE__, format, ##__VA_ARGS__)

/*
 * @brief A panic function designed to terminate the program in unrecoverable scenarios.
 *
 * If you find your program in a state that you don't know how to recover from, then it's probably time to
 * call this function, but remember... DON'T PANIC!... yeah, I'm just a little child ;)
 * */
#define uf_log_panic(format, ...)                                                                            \
    ({                                                                                                       \
        _uf_log(UF_LOG_PANIC, __FILE__, __LINE__, format, ##__VA_ARGS__);                                    \
        abort();                                                                                             \
    })

/*
 * @brief A simple assert macro that panics when the assertion fails.
 * @param expr The expression we are testing for.
 * */
#define uf_assert(expr)                                                                                      \
    ({                                                                                                       \
        if _unlikely_ (!(expr)) {                                                                            \
            uf_log_panic("Assertion failed: %s", #expr);                                                     \
        }                                                                                                    \
    })

/*
 * @brief A simple assert macro that panics when the assertion fails with formatted message.
 * @param expr The expression we are testing for.
 * @param format The format string for printf.
 * @param ... Any additional parameters corresponding to the format string.
 * */
#define uf_assert_msg(expr, format, ...)                                                                     \
    ({                                                                                                       \
        if _unlikely_ (!(expr)) {                                                                            \
            uf_log_panic("Assertion failed: (%s) " format, #expr, ##__VA_ARGS__);                            \
        }                                                                                                    \
    })
