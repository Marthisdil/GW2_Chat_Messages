#pragma once

// Minimal test helper shared by the standalone tests (tests/TestMain.cpp runs them all).

#include <cstdio>

inline int g_TestFailures = 0;
inline int g_TestChecks   = 0;

#define CHECK(cond)                                                                              \
    do {                                                                                         \
        g_TestChecks++;                                                                          \
        if (!(cond)) { g_TestFailures++; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)
