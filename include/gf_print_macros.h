#ifndef GITFLUSS_PRINT_MACROS
#define GITFLUSS_PRINT_MACROS

#include <stdio.h>
#include <stdlib.h>

// (Paulo):
// __LINE__ is a number, so we need to transform it into a string in the
// pre-processor with some funky stuff.
#define __STRINGIFY(x) #x
#define __TO_STRING(x) __STRINGIFY(x)
#define __LINE_STR __TO_STRING(__LINE__)
#define __LOCATION__ "[" __FILE__ ":" __LINE_STR "]"

// to make compatible with c99, without GNU extensions.
// from Richard Hansen via StackOverflow:
// https://stackoverflow.com/a/11172679
#define FIRST(...) FIRST_HELPER(__VA_ARGS__, throwaway)
#define FIRST_HELPER(first, ...) first

#define REST(...) REST_HELPER(NUM(__VA_ARGS__), __VA_ARGS__)
#define REST_HELPER(qty, ...) REST_HELPER2(qty, __VA_ARGS__)
#define REST_HELPER2(qty, ...) REST_HELPER_##qty(__VA_ARGS__)
#define REST_HELPER_ONE(first)
#define REST_HELPER_TWOORMORE(first, ...) , __VA_ARGS__
#define NUM(...)                                                       \
    SELECT_21ST(__VA_ARGS__,                                           \
                TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE,            \
                TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE,            \
                TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE,            \
                TWOORMORE, TWOORMORE, TWOORMORE, TWOORMORE,            \
                TWOORMORE, TWOORMORE, TWOORMORE,                       \
                ONE, throwaway)

#define SELECT_21ST(a1,  a2,  a3,  a4,  a5,  a6,  a7,  a8,  a9,  a10,               \
                    a11, a12, a13, a14, a15, a16, a17, a18, a19, a20, a21, ...) a21 \

#define GF_ERROR(...) \
        fprintf(stderr, "\n\033[31;1m" __LOCATION__ "\033[31;1;7m\nERROR: " \
                FIRST(__VA_ARGS__) "\033[0m\n" REST(__VA_ARGS__))

#define GF_FAIL(...) \
        fprintf(stderr, "\n\033[31;1m" __LOCATION__ "\033[31;1;7m\nFAIL: " \
                FIRST(__VA_ARGS__) "\033[0m\n" REST(__VA_ARGS__))

#define GF_SUCCESS(...) \
        fprintf(stdout, "\033[32;1;1mSUCCESS: " \
                FIRST(__VA_ARGS__) "\033[0m\n" REST(__VA_ARGS__))

#define GF_WARN(...) \
        fprintf(stdout, "\033[33;1m" __LOCATION__ "\n\033[33;1;7mWARNING: " \
                FIRST(__VA_ARGS__) "\033[0m\n" REST(__VA_ARGS__))

#ifdef GF_DEBUG
    #define GF_ASSERT(condition, ...)                               \
    do                                                              \
    {                                                               \
        if(condition)                                               \
        {                                                           \
            break;                                                  \
        }                                                           \
        fprintf(stderr, "\n\033[31;1m" __LOCATION__                 \
                "\033[31;1;7m\nASSERTION FAILED: "                  \
                FIRST(__VA_ARGS__) "\033[0m\n" REST(__VA_ARGS__));  \
        exit(1);                                                    \
    }                                                               \
    while(0);

    #define GF_DEBUG(...) \
            printf(FIRST(__VA_ARGS__) "\n" REST(__VA_ARGS__))
#else
    #define GF_ASSERT(...)
    #define GF_DEBUG(...)
#endif

#ifdef GF_TRACE
    #define GF_TRACE(...) \
            printf(FIRST(__VA_ARGS__) "\n" REST(__VA_ARGS__))
#else
    #define GF_TRACE(...)
#endif

#endif
