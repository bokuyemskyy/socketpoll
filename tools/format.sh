#!/bin/bash

# Find all C++ source files and format them

find . -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.cc" -o -name "*.cxx" \) \
    -not -path "*/build/*" \
    -not -path "*/.git/*" \
    -exec clang-format -i {} +

echo "Code formatting complete"