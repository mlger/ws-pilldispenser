#include "unity_host.h"

#include <stdio.h>
#include <string.h>

#define PD_HOST_MAX_TESTS 256

static pd_host_test_entry_t s_tests[PD_HOST_MAX_TESTS];
static int s_test_count;

void pd_host_register_test(const char *name, const char *suite, pd_host_test_fn fn) {
    if (s_test_count >= PD_HOST_MAX_TESTS) {
        fprintf(stderr, "host test registry full\n");
        return;
    }
    s_tests[s_test_count].name = name;
    s_tests[s_test_count].suite = suite;
    s_tests[s_test_count].fn = fn;
    s_test_count++;
}

const pd_host_test_entry_t *pd_host_tests(int *out_count) {
    if (out_count != NULL) {
        *out_count = s_test_count;
    }
    return s_tests;
}

/* Weak defaults, overridable per test file (mirrors the IDF unity runner). */
__attribute__((weak)) void setUp(void) {}

__attribute__((weak)) void tearDown(void) {}

int main(void) {
    UnityBegin(__FILE__);

    int count = 0;
    const pd_host_test_entry_t *tests = pd_host_tests(&count);
    for (int i = 0; i < count; i++) {
        char line[256];
        snprintf(line, sizeof(line), "%s %s", tests[i].suite, tests[i].name);
        UnityDefaultTestRun(tests[i].fn, line, (unsigned)i);
    }

    const int failures = (int)UnityEnd();
    printf("HOST_TESTS_DONE failed=%d\n", failures);
    return failures;
}
