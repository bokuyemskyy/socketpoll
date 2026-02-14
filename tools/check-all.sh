#!/bin/bash

# Run all code quality checks

set -e

echo "Running all code quality checks"

FAILED=0

echo "Running clang-format"
if ./tools/check-format.sh; then
    echo "Formatting check passed"
else
    echo "Formatting check failed"
    FAILED=1
fi
echo ""

echo "Running clang-tidy"
echo "----------------------------------------"
if ./tools/tidy.sh; then
    echo "Clang-tidy check passed"
else
    echo "Clang-tidy check failed"
    FAILED=1
fi
echo ""

if [ $FAILED -eq 0 ]; then
    echo "All checks passed!"
    exit 0
else
    echo "Some checks failed"
    exit 1
fi