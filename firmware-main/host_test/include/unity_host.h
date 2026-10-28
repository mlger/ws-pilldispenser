/*
 * unity_host.h — TEST_CASE shim so the same component Unity test files compile
 * both as ESP-IDF components (IDF's unity provides TEST_CASE) and on the host
 * (this shim registers tests via ELF constructors).
 */
#ifndef UNITY_HOST_H
#define UNITY_HOST_H

#include "unity_internals.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*pd_host_test_fn)(void);

typedef struct {
    const char *name;
    const char *suite;
    pd_host_test_fn fn;
} pd_host_test_entry_t;

void pd_host_register_test(const char *name, const char *suite, pd_host_test_fn fn);
const pd_host_test_entry_t *pd_host_tests(int *out_count);

/* Concatenation helpers for unique symbol names. */
#define PD_HOST_CAT_(a, b) a##b
#define PD_HOST_CAT(a, b) PD_HOST_CAT_(a, b)
#define PD_HOST_UNIQ(base) PD_HOST_CAT(base, __LINE__)

#define UNITY_TEST_CASE_EXPLICIT_(fn_name, name, suite)                                              \
    static void fn_name(void);                                                                       \
    static void PD_HOST_UNIQ(pd_host_reg_)(void);                                                    \
    __attribute__((constructor)) static void PD_HOST_UNIQ(pd_host_reg_)(void)                        \
    {                                                                                                \
        pd_host_register_test(name, suite, fn_name);                                                 \
    }                                                                                                \
    static void fn_name(void)

#define TEST_CASE(name, suite) UNITY_TEST_CASE_EXPLICIT_(PD_HOST_UNIQ(pd_host_test_), name, suite)

#ifdef __cplusplus
}
#endif

#endif /* UNITY_HOST_H */
