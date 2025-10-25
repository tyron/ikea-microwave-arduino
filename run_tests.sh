#!/bin/bash

# Test runner script for IKEA Microwave Arduino
# This script compiles and runs unit tests

set -e

echo "==================================="
echo "IKEA Microwave Arduino - Test Suite"
echo "==================================="
echo ""

# Check if Unity framework exists
if [ ! -d "/tmp/Unity" ]; then
    echo "Downloading Unity testing framework..."
    cd /tmp
    git clone --depth 1 https://github.com/ThrowTheSwitch/Unity.git
    cd - > /dev/null
    echo "Unity framework downloaded."
    echo ""
fi

# Get the project directory
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Compiling tests..."
g++ -std=c++11 -DUNIT_TEST \
    -I"$PROJECT_DIR/include" \
    -I/tmp/Unity/src \
    "$PROJECT_DIR/test/test_timer_logic/test_timer_logic.cpp" \
    /tmp/Unity/src/unity.c \
    -o "$PROJECT_DIR/test_runner"

echo "Compilation successful!"
echo ""
echo "Running tests..."
echo "-----------------------------------"

# Run the tests
"$PROJECT_DIR/test_runner"

# Clean up
rm -f "$PROJECT_DIR/test_runner"

echo ""
echo "==================================="
echo "Test suite completed!"
echo "==================================="
