#pragma once

/**
 * @brief A common header file shared between almost all other files.
 *
 * This file is crucial for building anything inside UnFinity. It provides all sorts of crucial functions,
 * macros, and global variables that are required to maintain consistency between all modules. If you are
 * thinking about creating a new module for UnFinity you'll have to use this file.
 *
 * @author Frantisek Lednicky (HlasitaHudbaBezZvuku)
 **/

/**
 * These macros should help by making the GNU attributes easier to write.
 **/

#define _nodiscard_ __attribute__((warn_unused_result))
#define _malloc_ __attribute__((malloc))
#define _alloc_size_(...) __attribute__((alloc_size(__VA_ARGS__)))
#define _noreturn_ __attribute__((noreturn))
#define _cleanup_(x) __attribute__((cleanup(x)))
#define _aligned_(x) __attribute__((aligned(x)))

/**
 * These macros are meant for optimizations in places where one resolute of a condition is much likelier
 * (e.g., error return value checking, assertion, and null checks). Don't use them where you are uncertain of
 * the condition resolute.
 **/

#define _likely_(x) (__builtin_expect(!!(x), 1))
#define _unlikely_(x) (__builtin_expect(!!(x), 0))

#define uf_container_of(ptr, type, member)                                                                   \
    ({                                                                                                       \
        const typeof(((type*)0)->member)* __mptr = (ptr);                                                    \
        (type*)((char*)__mptr - offsetof(type, member));                                                     \
    })
