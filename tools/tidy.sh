#!/bin/bash

# Run clang-tidy on all source files

if [ ! -f "build/compile_commands.json" ]; then
    echo "compile_commands.json not found"
    exit 1
fi

FILES=$(find . -type f \( -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" \) \
    -not -path "*/build/*" \
    -not -path "*/tests/*" \
    -not -path "*/.git/*")

FAILED=0
for file in $FILES; do
    if ! clang-tidy -p build "$file"; then
        FAILED=1
    fi
done

if [ $FAILED -eq 1 ]; then
    echo "Clang-tidy found issues"
    exit 1
fi

echo "Clang-tidy analysis complete - no issues found"