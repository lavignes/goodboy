#ifndef GB_FATAL_H
#define GB_FATAL_H

#include <stdarg.h>

#define NORETURN                 _Noreturn
#define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)

#define INLINE                   inline
#ifdef __has_attribute
#if __has_attribute(always_inline)
#undef INLINE
#define INLINE inline __attribute__((always_inline))
#endif
#endif

#define FORMAT(n)
#ifdef __has_attribute
#if __has_attribute(format)
#undef FORMAT
#define FORMAT(n) __attribute__((format(printf, (n), (n + 1))))
#endif
#endif

#ifdef __builtin_unreachable
#define UNREACHABLE() __builtin_unreachable()
#else
#define UNREACHABLE()                                                          \
    do {                                                                       \
        fatal("%s:%d: unreachable\n", __FILE__, __LINE__);                     \
    } while (0)
#endif

#define TODO(fmt, ...)                                                         \
    do {                                                                       \
        fatal("%s:%d: not implemented" fmt, __FILE__, __LINE__,                \
              ##__VA_ARGS__);                                                  \
    } while (0)

NORETURN void fatalV(char const* fmt, va_list args);
FORMAT(1) NORETURN void fatal(char const* fmt, ...);

void debugV(char const* fmt, va_list args);
FORMAT(1) void debug(char const* fmt, ...);

#endif // GB_FATAL_H
