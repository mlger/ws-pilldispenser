/*
 * Host wrapper for unity.h: resolved FIRST in the include path (before the
 * vendored Unity), pulls in the real upstream Unity via #include_next, and
 * then layers the IDF-style TEST_CASE shim (unity_host.h) on top — so the
 * same component test sources compile on the host and as ESP-IDF components.
 */
#ifndef PD_HOST_UNITY_WRAPPER_H
#define PD_HOST_UNITY_WRAPPER_H

#include_next "unity.h"

#include "unity_host.h"

#endif /* PD_HOST_UNITY_WRAPPER_H */
