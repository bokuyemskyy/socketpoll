#!/bin/bash

# Check if all files are formatted correctly

FILES=$(find . -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.cc" -o -name "*.cxx" \) \
    -not -path "*/build/*" \
    -not -path "*/.git/*")

UNFORMATTED=""
for file in $FILES; do
    if ! diff -q <(cat "$file") <(clang-format "$file") > /dev/null; then
        UNFORMATTED="$UNFORMATTED\n  $file"
    fi
done

if [ -n "$UNFORMATTED" ]; then
    echo "The following files are not formatted correctly:"
    echo -e "$UNFORMATTED"
    echo ""
    echo "Run ./tools/format.sh to fix formatting"
    exit 1
else
    echo "All files are properly formatted"
fi