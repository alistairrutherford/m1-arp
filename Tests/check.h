#pragma once

#include <iostream>

inline int gFailures = 0;

inline void check (bool cond, const char* what)
{
    std::cout << (cond ? "  PASS  " : "  FAIL  ") << what << "\n";
    if (! cond)
        ++gFailures;
}
